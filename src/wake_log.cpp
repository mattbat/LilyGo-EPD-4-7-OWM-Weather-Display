#ifdef ARDUINO
#include "wake_log.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <time.h>

static const int LOG_N = 48;   // one day of 30-minute wakes
static WakeRecord g_cur;
static int g_slot = -1;

static const char* resetName(int r) {
  switch (r) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "CRASH";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default: return "other";
  }
}
static const char* stageName(int s) {
  switch (s) {
    case STAGE_BOOT: return "died at boot";
    case STAGE_NET: return "died during WiFi/fetch";
    case STAGE_DISPLAY: return "died during panel refresh";
    case STAGE_DONE: return "ok";
    default: return "?";
  }
}

static void save() {
  if (g_slot < 0) return;
  Preferences p;
  if (!p.begin("wakelog", false)) return;
  char key[6]; snprintf(key, sizeof key, "r%02d", g_slot);
  p.putBytes(key, &g_cur, sizeof g_cur);
  p.end();
}

static uint32_t nowEpoch() {
  time_t t = time(nullptr);
  return t > 1700000000 ? (uint32_t)t : 0;
}

void wakeLogBegin(uint16_t battMv) {
  Preferences p;
  if (!p.begin("wakelog", false)) return;
  int head = p.getInt("head", 0);   // next slot to write
  Serial.println("---- wake log (oldest first) ----");
  for (int i = 0; i < LOG_N; i++) {
    int s = (head + i) % LOG_N;
    char key[6]; snprintf(key, sizeof key, "r%02d", s);
    WakeRecord r;
    if (!p.isKey(key) || p.getBytes(key, &r, sizeof r) != sizeof r || r.stage == 0) continue;
    char when[24] = "no clock";
    if (r.epoch) { time_t t = r.epoch; struct tm lt; localtime_r(&t, &lt); strftime(when, sizeof when, "%m-%d %I:%M %p", &lt); }
    Serial.printf("%s  batt %.2f V  reset %-10s  wake %d  %-26s  awake %.1f s\n", when, r.battMv / 1000.0f,
                  resetName(r.resetReason), r.wakeCause, stageName(r.stage), r.awakeDs / 10.0f);
  }
  Serial.println("---------------------------------");
  g_slot = head;
  p.putInt("head", (head + 1) % LOG_N);
  p.end();
  memset(&g_cur, 0, sizeof g_cur);
  g_cur.epoch = nowEpoch();
  g_cur.battMv = battMv;
  g_cur.resetReason = (uint8_t)esp_reset_reason();
  g_cur.wakeCause = (uint8_t)esp_sleep_get_wakeup_cause();
  g_cur.stage = STAGE_BOOT;
  save();
}

void wakeLogStage(WakeStage s) {
  g_cur.stage = s;
  uint32_t e = nowEpoch();
  if (e) g_cur.epoch = e;
  save();
}

void wakeLogDone(uint32_t awakeMs) {
  g_cur.awakeDs = (uint16_t)min<uint32_t>(awakeMs / 100, 65535);
  wakeLogStage(STAGE_DONE);
}

bool wakeLogLastWasBrownout() { return esp_reset_reason() == ESP_RST_BROWNOUT; }
#endif
