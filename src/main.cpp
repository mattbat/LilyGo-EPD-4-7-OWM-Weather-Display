// ESP32 entry point. One wake = one pass through setup(), then deep sleep (loop() never runs).
// Flow (docs/PLAN.md "Device flow"): wake -> battery -> charge-me? -> schedule -> WiFi+NTP -> fetch
// -> WiFi off -> render -> e-paper refresh -> sleep until the next slot.
#ifdef ARDUINO
#include <Arduino.h>
#include <math.h>
#include <time.h>
#include "config.h"
#include "battery.h"
#include "display_hw.h"
#include "gfx.h"
#include "helpers.h"
#include "schedule.h"
#include "screens.h"
#include "tempest_api.h"
#include "weather_data.h"

// ---- RTC memory (8 KB slow RAM, survives deep sleep, cleared on power loss) ----
struct RtcState {
  uint32_t magic;
  bool chargeMeMode, chargeMeShown;
  bool cacheValid;            // g_cache holds the last good fetch
  StatsCache stats;           // rain month/year, refreshed once per local day
};
static const uint32_t RTC_MAGIC = 0x54454D50;   // "TEMP"
RTC_DATA_ATTR static RtcState g_rtc;
RTC_DATA_ATTR static WeatherData g_cache;       // last good data (fetchedEpoch = when)
static_assert(sizeof(RtcState) + sizeof(WeatherData) <= 7600, "RTC memory budget (8 KB) exceeded");

static WeatherData g_work;   // fetch target (regular RAM, so a failed fetch never damages g_cache)
static const int32_t FALLBACK_SLEEP_S = 15 * 60;   // when there is no clock at all

static void goToSleep(int32_t seconds, uint32_t t0) {
  netOff();
  Serial.printf("awake %lu ms, sleeping %ld s\n", (unsigned long)(millis() - t0), (long)seconds);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  esp_deep_sleep_start();
}

static WakeReason wakeReason() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER: return WakeReason::Timer;
    case ESP_SLEEP_WAKEUP_UNDEFINED: return WakeReason::Reset;   // power-on or reset button
    default: return WakeReason::Unknown;
  }
}

static ScreenStatus makeStatus(int64_t now, float volts, bool offline, int64_t lastGood) {
  ScreenStatus st;
  st.nowEpoch = now;
  st.batteryVolts = volts;
  st.batteryPercent = (isnan(volts) || volts < 2.0f) ? -1 : batteryPercent(volts);
  st.offline = offline;
  st.lastGoodEpoch = lastGood;
  return st;
}
static void showError(Gfx& g, const char* reason, const ScreenStatus& st) {
  drawError(g, reason, st);
  displayShow(g.fb);
}

void setup() {
  const uint32_t t0 = millis();
  Serial.begin(115200);
  setenv("TZ", TZ_POSIX, 1);
  tzset();

  if (g_rtc.magic != RTC_MAGIC) {
    memset(&g_rtc, 0, sizeof g_rtc);
    weatherDataInit(g_cache);
    g_rtc.magic = RTC_MAGIC;
  }
  const WakeReason reason = wakeReason();

  uint8_t* fb = displayInit();
  if (!fb) { Serial.println("framebuffer alloc failed"); goToSleep(FALLBACK_SLEEP_S, t0); }
  Gfx g; g.begin(fb);

  const float volts = readBatteryVolts();
  Serial.printf("wake reason %d, battery %.2f V (%d%%)\n", (int)reason, volts, batteryPercent(volts));

  // ---- first plan: handles charge-me without WiFi; also covers stray timer wakes if the RTC clock survived
  ScheduleInput in;
  in.timeValid = systemTimeValid();
  in.nowEpoch = time(nullptr);
  in.reason = reason;
  in.battVolts = volts;
  in.chargeMeMode = g_rtc.chargeMeMode;
  in.chargeMeShown = g_rtc.chargeMeShown;
  Plan plan = planWake(in);
  g_rtc.chargeMeMode = plan.chargeMeMode;
  g_rtc.chargeMeShown = plan.chargeMeShown;

  if (plan.action == Action::ChargeMe) {
    if (plan.drawChargeMe) {
      drawChargeMe(g, makeStatus(in.timeValid ? in.nowEpoch : 0, volts, false,
                                 g_rtc.cacheValid ? (int64_t)g_cache.fetchedEpoch : 0));
      displayShow(g.fb);
    }
    goToSleep(plan.sleepSeconds, t0);
  }
  if (plan.action == Action::SleepOnly) goToSleep(sleepSecondsUntil(plan.wakeEpoch, time(nullptr)), t0);

  // ---- WiFi + NTP, then plan again with a trustworthy clock
  bool wifi = netConnect(WIFI_TIMEOUT_MS);
  Serial.printf("wifi %s (%lu ms)\n", wifi ? "ok" : "FAILED", (unsigned long)(millis() - t0));
  if (wifi) netSyncTime(8000);
  Serial.printf("ntp done (%lu ms)\n", (unsigned long)(millis() - t0));
  in.timeValid = systemTimeValid();
  in.nowEpoch = time(nullptr);
  in.chargeMeMode = g_rtc.chargeMeMode;
  in.chargeMeShown = g_rtc.chargeMeShown;
  plan = planWake(in);
  const int64_t now = in.nowEpoch;

  if (plan.action == Action::NeedTime) {
    // No clock and no way to get one.
    if (!g_rtc.cacheValid) showError(g, "WiFi failed (no clock)", makeStatus(0, volts, false, 0));
    goToSleep(FALLBACK_SLEEP_S, t0);
  }
  if (plan.action == Action::SleepOnly) goToSleep(sleepSecondsUntil(plan.wakeEpoch, now), t0);

  // ---- fetch
  FetchResult fr;
  fr.status = FETCH_ERR_WIFI; fr.httpCode = 0; fr.parse = PARSE_OK; snprintf(fr.what, sizeof fr.what, "wifi");
  bool fresh = false;
  if (wifi) {
    FetchContext ctx{now, plan.titles, &g_rtc.stats};
    fr = fetchAll(g_work, ctx);
    fresh = fr.status == FETCH_OK;
  }
  netOff();   // WiFi must be off before the panel refresh and sleep
  Serial.printf("fetch done (%lu ms)\n", (unsigned long)(millis() - t0));
  if (fresh) {
    g_cache = g_work;
    g_rtc.cacheValid = true;
  } else {
    char why[96]; describeFetchResult(fr, why, sizeof why);
    Serial.printf("fetch failed: %s\n", why);
  }

  // ---- render
  WeatherData& data = g_cache;
  const bool haveData = g_rtc.cacheValid && data.valid;
  const ScreenStatus st = makeStatus(now, volts, !fresh, haveData ? (int64_t)data.fetchedEpoch : 0);
  if (!haveData) {
    char why[96]; describeFetchResult(fr, why, sizeof why);
    drawError(g, why, st);
  } else if (plan.action == Action::Live) {
    drawLive(g, data, st);
  } else {   // Overnight
    drawOvernight(g, data, st, plan.titles == TitlesMode::YesterdayToday ? TITLES_YESTERDAY_TODAY : TITLES_TODAY_TOMORROW);
  }
  Serial.printf("render done (%lu ms)\n", (unsigned long)(millis() - t0));
  displayShow(g.fb);
  Serial.printf("display done (%lu ms)\n", (unsigned long)(millis() - t0));
  g_rtc.chargeMeShown = false;   // the panel now shows something else

  goToSleep(sleepSecondsUntil(plan.wakeEpoch, time(nullptr)), t0);
}

void loop() {}
#endif
