// Portable: decide what a wake is for (task 6).
#pragma once
#include <stdint.h>
enum class WakeReason { Timer, Reset };
enum class Action { Live, Overnight, OvernightYesterdayToday, SleepOnly };
struct Plan {
  Action action;
  int32_t sleepSeconds;  // until next :00/:30, or 5:00 AM
};
// hour/min/sec are local time.
Plan planWake(int hour, int min, int sec, WakeReason reason);
