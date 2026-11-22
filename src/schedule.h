// Portable, pure: decide what a wake is for and when to wake next (PLAN "Device flow" step 2-3).
// Local time comes from the C library: the caller (or test) sets TZ and calls tzset() first.
#pragma once
#include <stdint.h>

enum class WakeReason { Timer, Reset, Unknown };   // Reset = power-on or reset button; Unknown = anything else
enum class Action {
  Live,        // draw the live screen
  Overnight,   // draw the overnight summary (see Plan::titles)
  ChargeMe,    // battery low: draw the charge-me screen if Plan::drawChargeMe, no WiFi needed
  SleepOnly,   // nothing to draw, just sleep (timer wake inside quiet hours)
  NeedTime     // no valid clock yet: connect WiFi, sync NTP, then call planWake again
};
enum class TitlesMode { TodayTomorrow, YesterdayToday };

#define SCHED_DRIFT_PAD_S     45      // wake up to this much early (RTC drift) counts as the intended slot

struct ScheduleInput {
  bool timeValid;        // system clock is trustworthy (after NTP, or kept running through deep sleep)
  int64_t nowEpoch;      // UTC epoch seconds
  WakeReason reason;
  float battVolts;       // NAN or < 2 V = reading failed, ignored
  bool chargeMeMode;     // RTC flag: currently in charge-me mode
  bool chargeMeShown;    // RTC flag: charge-me screen is on the panel
};

struct Plan {
  Action action;
  TitlesMode titles;       // meaningful for Overnight
  bool drawChargeMe;       // meaningful for ChargeMe
  bool chargeMeMode;       // new value for the RTC flag
  bool chargeMeShown;      // new value for the RTC flag
  int64_t wakeEpoch;       // when to wake next (UTC epoch); 0 when no time was available
  int32_t sleepSeconds;    // wakeEpoch - nowEpoch (or the fixed charge-me interval)
};

Plan planWake(const ScheduleInput& in);

// Seconds to sleep so the board wakes at wakeEpoch; never below minSeconds (guards a late finish).
int32_t sleepSecondsUntil(int64_t wakeEpoch, int64_t nowEpoch, int32_t minSeconds = 5);

// Local calendar helpers (use the process TZ).
int localDayKey(int64_t epoch);                       // yyyymmdd
int64_t localMidnight(int64_t epoch, int dayOffset);  // local 00:00 of (day of epoch + dayOffset)
