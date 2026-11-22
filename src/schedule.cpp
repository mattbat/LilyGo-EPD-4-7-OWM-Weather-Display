#include "schedule.h"
#include "config.h"
#include <math.h>
#include <time.h>

static bool battValid(float v) { return !isnan(v) && v >= 2.0f; }

static void localTm(int64_t epoch, struct tm& out) {
  time_t t = (time_t)epoch;
  out = *localtime(&t);   // single threaded firmware/tests; copy out of the static buffer
}

// Epoch of local (day of ref + dayOffset) at hour:min.
static int64_t localAt(const struct tm& ref, int dayOffset, int hour, int min) {
  struct tm t = ref;
  t.tm_mday += dayOffset;
  t.tm_hour = hour; t.tm_min = min; t.tm_sec = 0;
  t.tm_isdst = -1;
  return (int64_t)mktime(&t);
}

int localDayKey(int64_t epoch) {
  struct tm t; localTm(epoch, t);
  return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}
int64_t localMidnight(int64_t epoch, int dayOffset) {
  struct tm t; localTm(epoch, t);
  return localAt(t, dayOffset, 0, 0);
}

int32_t sleepSecondsUntil(int64_t wakeEpoch, int64_t nowEpoch, int32_t minSeconds) {
  int64_t d = wakeEpoch - nowEpoch;
  if (d < minSeconds) d = minSeconds;
  if (d > 24 * 3600) d = 24 * 3600;
  return (int32_t)d;
}

Plan planWake(const ScheduleInput& in) {
  Plan p;
  p.action = Action::SleepOnly;
  p.titles = TitlesMode::TodayTomorrow;
  p.drawChargeMe = false;
  p.chargeMeMode = in.chargeMeMode;
  p.chargeMeShown = in.chargeMeShown;
  p.wakeEpoch = 0;
  p.sleepSeconds = 0;

  // ---- battery / charge-me hysteresis (needs no clock, no WiFi)
  bool recovered = false;
  if (battValid(in.battVolts)) {
    if (!in.chargeMeMode && in.battVolts < BATT_LOW_V) { p.chargeMeMode = true; p.chargeMeShown = false; }
    else if (in.chargeMeMode && in.battVolts >= BATT_RECOVER_V) { p.chargeMeMode = false; p.chargeMeShown = false; recovered = true; }
  }
  if (p.chargeMeMode) {
    p.action = Action::ChargeMe;
    p.drawChargeMe = !p.chargeMeShown;
    p.chargeMeShown = true;
    p.sleepSeconds = CHARGE_SLEEP_HOURS * 3600;
    p.wakeEpoch = in.timeValid ? in.nowEpoch + p.sleepSeconds : 0;
    return p;
  }

  if (!in.timeValid) { p.action = Action::NeedTime; return p; }

  // ---- slot: round to the nearest intended :00/:30 (early wakes from RTC drift land on the slot)
  struct tm eff;
  localTm(in.nowEpoch + SCHED_DRIFT_PAD_S, eff);
  eff.tm_min = eff.tm_min < 30 ? 0 : 30;
  eff.tm_sec = 0;
  const int m = eff.tm_hour * 60 + eff.tm_min;     // minute of local day of the slot
  const int quietStart = QUIET_START_HOUR * 60 + QUIET_START_MIN;
  const int quietEnd = QUIET_END_HOUR * 60 + QUIET_END_MIN;

  const bool userWake = in.reason != WakeReason::Timer || recovered;
  const bool inQuiet = m >= quietStart || m < quietEnd;

  if (!inQuiet) {
    p.action = Action::Live;
  } else if (m == quietStart && in.reason == WakeReason::Timer) {
    p.action = Action::Overnight;               // the scheduled 22:00 draw
  } else if (userWake) {
    p.action = Action::Overnight;               // reset / unknown wake while paused
    p.titles = (m < quietEnd) ? TitlesMode::YesterdayToday : TitlesMode::TodayTomorrow;
  } else {
    p.action = Action::SleepOnly;               // stray timer wake in quiet hours
  }

  // ---- next wake, computed in local time (DST safe)
  if (m < quietEnd)        p.wakeEpoch = localAt(eff, 0, QUIET_END_HOUR, QUIET_END_MIN);
  else if (m < quietStart) {
    int n = m + UPDATE_INTERVAL_MIN;
    if (n > quietStart) n = quietStart;         // 21:30 -> 22:00
    p.wakeEpoch = localAt(eff, 0, n / 60, n % 60);
  } else                   p.wakeEpoch = localAt(eff, 1, QUIET_END_HOUR, QUIET_END_MIN);
  p.sleepSeconds = sleepSecondsUntil(p.wakeEpoch, in.nowEpoch, 1);
  return p;
}
