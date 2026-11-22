#include <unity.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "schedule.h"
#include "config.h"

void setUp() {}
void tearDown() {}

static void setTz() {
#if defined(_WIN32)
  _putenv("TZ=CST6CDT,M3.2.0,M11.1.0");
  _tzset();
#else
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
  tzset();
#endif
}

// Local (US Central) wall clock -> epoch.
static int64_t L(int y, int mo, int d, int h, int mi, int s = 0) {
  struct tm t = {};
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
  t.tm_isdst = -1;
  return (int64_t)mktime(&t);
}

static ScheduleInput base(int64_t now, WakeReason r = WakeReason::Timer) {
  ScheduleInput in;
  in.timeValid = true; in.nowEpoch = now; in.reason = r;
  in.battVolts = 4.0f; in.chargeMeMode = false; in.chargeMeShown = false;
  return in;
}

static void expectWake(int64_t now, WakeReason r, Action a, int64_t wake) {
  Plan p = planWake(base(now, r));
  TEST_ASSERT_EQUAL_INT((int)a, (int)p.action);
  TEST_ASSERT_EQUAL_INT64(wake, p.wakeEpoch);
  TEST_ASSERT_EQUAL_INT32((int32_t)(wake - now), p.sleepSeconds);
}

// ---- slot boundaries on a normal day (Sat 2026-11-21)
void test_every_half_hour_slot() {
  for (int h = 5; h <= 21; h++)
    for (int m = 0; m < 60; m += 30) {
      int nh = (m == 30) ? h + 1 : h, nm = (m == 30) ? 0 : 30;
      expectWake(L(2026, 11, 21, h, m, 3), WakeReason::Timer, Action::Live, L(2026, 11, 21, nh, nm));
    }
}
void test_2130_goes_to_2200() {
  expectWake(L(2026, 11, 21, 21, 30, 4), WakeReason::Timer, Action::Live, L(2026, 11, 21, 22, 0));
}
void test_2200_overnight_then_0500_next_day() {
  Plan p = planWake(base(L(2026, 11, 21, 22, 0, 5)));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)p.action);
  TEST_ASSERT_EQUAL_INT((int)TitlesMode::TodayTomorrow, (int)p.titles);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 5, 0), p.wakeEpoch);
  TEST_ASSERT_EQUAL_INT32(7 * 3600 - 5, p.sleepSeconds);
}
void test_0500_is_live() {
  expectWake(L(2026, 11, 22, 5, 0, 2), WakeReason::Timer, Action::Live, L(2026, 11, 22, 5, 30));
}
void test_stray_timer_wake_in_quiet_hours_sleeps_to_0500() {
  expectWake(L(2026, 11, 22, 1, 30, 0), WakeReason::Timer, Action::SleepOnly, L(2026, 11, 22, 5, 0));
  expectWake(L(2026, 11, 21, 23, 0, 0), WakeReason::Timer, Action::SleepOnly, L(2026, 11, 22, 5, 0));
  expectWake(L(2026, 11, 22, 4, 30, 0), WakeReason::Timer, Action::SleepOnly, L(2026, 11, 22, 5, 0));
}
void test_reset_at_0100_overnight_yesterday_today() {
  Plan p = planWake(base(L(2026, 11, 22, 1, 0, 10), WakeReason::Reset));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)p.action);
  TEST_ASSERT_EQUAL_INT((int)TitlesMode::YesterdayToday, (int)p.titles);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 5, 0), p.wakeEpoch);
}
void test_reset_before_midnight_in_quiet_hours_today_tomorrow() {
  Plan p = planWake(base(L(2026, 11, 21, 23, 10, 0), WakeReason::Reset));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)p.action);
  TEST_ASSERT_EQUAL_INT((int)TitlesMode::TodayTomorrow, (int)p.titles);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 5, 0), p.wakeEpoch);
  p = planWake(base(L(2026, 11, 21, 22, 12, 0), WakeReason::Reset));
  TEST_ASSERT_EQUAL_INT((int)TitlesMode::TodayTomorrow, (int)p.titles);
}
void test_reset_in_live_hours_is_live_and_aligns() {
  expectWake(L(2026, 11, 21, 14, 12, 0), WakeReason::Reset, Action::Live, L(2026, 11, 21, 14, 30));
  expectWake(L(2026, 11, 21, 14, 41, 0), WakeReason::Reset, Action::Live, L(2026, 11, 21, 15, 0));
  expectWake(L(2026, 11, 21, 21, 40, 0), WakeReason::Unknown, Action::Live, L(2026, 11, 21, 22, 0));
}

// ---- RTC drift padding
void test_early_wake_0459_40_is_the_0500_slot() {
  int64_t now = L(2026, 11, 22, 4, 59, 40);
  Plan p = planWake(base(now));
  TEST_ASSERT_EQUAL_INT((int)Action::Live, (int)p.action);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 5, 30), p.wakeEpoch);   // not 05:00 again, 20 s later
  TEST_ASSERT_EQUAL_INT32(30 * 60 + 20, p.sleepSeconds);
}
void test_early_wake_xx29_50_is_the_slot() {
  int64_t now = L(2026, 11, 21, 10, 29, 50);
  expectWake(now, WakeReason::Timer, Action::Live, L(2026, 11, 21, 11, 0));
  now = L(2026, 11, 21, 10, 59, 50);
  expectWake(now, WakeReason::Timer, Action::Live, L(2026, 11, 21, 11, 30));
}
void test_early_wake_2159_50_is_overnight() {
  Plan p = planWake(base(L(2026, 11, 21, 21, 59, 50)));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)p.action);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 5, 0), p.wakeEpoch);
}
void test_wake_too_early_is_not_rounded_up() {
  // 04:58 is outside the pad: still quiet hours, sleep to 05:00
  expectWake(L(2026, 11, 22, 4, 58, 0), WakeReason::Timer, Action::SleepOnly, L(2026, 11, 22, 5, 0));
}
void test_late_wake_still_the_slot() {
  expectWake(L(2026, 11, 21, 10, 0, 40), WakeReason::Timer, Action::Live, L(2026, 11, 21, 10, 30));
}
void test_midnight_rollover_with_pad() {
  Plan p = planWake(base(L(2026, 11, 21, 23, 59, 50)));
  TEST_ASSERT_EQUAL_INT((int)Action::SleepOnly, (int)p.action);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 5, 0), p.wakeEpoch);
}

// ---- charge-me
void test_charge_me_enter() {
  ScheduleInput in = base(L(2026, 11, 21, 12, 0));
  in.battVolts = 3.29f;
  Plan p = planWake(in);
  TEST_ASSERT_EQUAL_INT((int)Action::ChargeMe, (int)p.action);
  TEST_ASSERT_TRUE(p.drawChargeMe);
  TEST_ASSERT_TRUE(p.chargeMeMode);
  TEST_ASSERT_TRUE(p.chargeMeShown);
  TEST_ASSERT_EQUAL_INT32(2 * 3600, p.sleepSeconds);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 21, 14, 0), p.wakeEpoch);
}
void test_charge_me_exactly_3_3_is_not_low() {
  ScheduleInput in = base(L(2026, 11, 21, 12, 0));
  in.battVolts = 3.3f;
  TEST_ASSERT_EQUAL_INT((int)Action::Live, (int)planWake(in).action);
}
void test_charge_me_stays_with_hysteresis_and_no_redraw() {
  ScheduleInput in = base(L(2026, 11, 21, 14, 0));
  in.chargeMeMode = true; in.chargeMeShown = true;
  const float volts[] = {3.31f, 3.5f, 3.69f};
  for (float v : volts) {
    in.battVolts = v;
    Plan p = planWake(in);
    TEST_ASSERT_EQUAL_INT((int)Action::ChargeMe, (int)p.action);
    TEST_ASSERT_FALSE(p.drawChargeMe);
    TEST_ASSERT_TRUE(p.chargeMeMode);
    TEST_ASSERT_EQUAL_INT32(2 * 3600, p.sleepSeconds);
  }
}
void test_charge_me_redraws_if_not_shown() {
  ScheduleInput in = base(L(2026, 11, 21, 14, 0));
  in.chargeMeMode = true; in.chargeMeShown = false; in.battVolts = 3.5f;
  TEST_ASSERT_TRUE(planWake(in).drawChargeMe);
}
void test_charge_me_exit_at_3_7() {
  ScheduleInput in = base(L(2026, 11, 21, 14, 2), WakeReason::Timer);
  in.chargeMeMode = true; in.chargeMeShown = true; in.battVolts = 3.7f;
  Plan p = planWake(in);
  TEST_ASSERT_EQUAL_INT((int)Action::Live, (int)p.action);
  TEST_ASSERT_FALSE(p.chargeMeMode);
  TEST_ASSERT_FALSE(p.chargeMeShown);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 21, 14, 30), p.wakeEpoch);
}
void test_charge_me_exit_in_quiet_hours_redraws_overnight() {
  ScheduleInput in = base(L(2026, 11, 22, 2, 0), WakeReason::Timer);
  in.chargeMeMode = true; in.chargeMeShown = true; in.battVolts = 3.9f;
  Plan p = planWake(in);
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)p.action);   // do not leave "Charge me" on screen
  TEST_ASSERT_EQUAL_INT((int)TitlesMode::YesterdayToday, (int)p.titles);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 5, 0), p.wakeEpoch);
}
void test_charge_me_needs_no_clock() {
  ScheduleInput in = base(0);
  in.timeValid = false; in.battVolts = 3.0f;
  Plan p = planWake(in);
  TEST_ASSERT_EQUAL_INT((int)Action::ChargeMe, (int)p.action);
  TEST_ASSERT_EQUAL_INT32(7200, p.sleepSeconds);
  TEST_ASSERT_EQUAL_INT64(0, p.wakeEpoch);
}
void test_bad_battery_reading_ignored() {
  ScheduleInput in = base(L(2026, 11, 21, 12, 0));
  in.battVolts = NAN;
  TEST_ASSERT_EQUAL_INT((int)Action::Live, (int)planWake(in).action);
  in.battVolts = 0.0f;
  TEST_ASSERT_EQUAL_INT((int)Action::Live, (int)planWake(in).action);
  in.chargeMeMode = true; in.battVolts = NAN;   // unknown reading keeps the current mode
  TEST_ASSERT_EQUAL_INT((int)Action::ChargeMe, (int)planWake(in).action);
}

// ---- no time yet
void test_no_valid_time() {
  ScheduleInput in = base(0);
  in.timeValid = false;
  Plan p = planWake(in);
  TEST_ASSERT_EQUAL_INT((int)Action::NeedTime, (int)p.action);
  in.reason = WakeReason::Reset;
  TEST_ASSERT_EQUAL_INT((int)Action::NeedTime, (int)planWake(in).action);
}

// ---- DST
void test_dst_spring_forward_day_2026_03_08() {
  // Sat night -> Sun: 22:00 CST to 05:00 CDT is 6 h of wall-clock sleep
  int64_t now = L(2026, 3, 7, 22, 0, 0);
  Plan p = planWake(base(now));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)p.action);
  TEST_ASSERT_EQUAL_INT64(L(2026, 3, 8, 5, 0), p.wakeEpoch);
  TEST_ASSERT_EQUAL_INT32(6 * 3600, p.sleepSeconds);
  // 05:00 CDT is one 05:00 only, and the next wake is 05:30
  expectWake(L(2026, 3, 8, 5, 0, 1), WakeReason::Timer, Action::Live, L(2026, 3, 8, 5, 30));
  // reset at 01:30 CST (before the jump): 2.5 h to 05:00 CDT
  Plan r = planWake(base(L(2026, 3, 8, 1, 30), WakeReason::Reset));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)r.action);
  TEST_ASSERT_EQUAL_INT32(2 * 3600 + 1800, r.sleepSeconds);
  // reset at 03:10 CDT (right after the jump) is still quiet hours
  Plan r2 = planWake(base(L(2026, 3, 8, 3, 10), WakeReason::Reset));
  TEST_ASSERT_EQUAL_INT((int)TitlesMode::YesterdayToday, (int)r2.titles);
  TEST_ASSERT_EQUAL_INT32(1 * 3600 + 50 * 60, r2.sleepSeconds);
  // every live slot that day is 30 min apart
  for (int h = 5; h < 22; h++) {
    Plan q = planWake(base(L(2026, 3, 8, h, 0)));
    TEST_ASSERT_EQUAL_INT32(1800, q.sleepSeconds);
  }
  // evening after DST: 22:00 CDT -> Mon 05:00 CDT = 7 h
  TEST_ASSERT_EQUAL_INT32(7 * 3600, planWake(base(L(2026, 3, 8, 22, 0))).sleepSeconds);
}
void test_dst_fall_back_day_2026_11_01() {
  // Sat 22:00 CDT -> Sun 05:00 CST is 8 h
  Plan p = planWake(base(L(2026, 10, 31, 22, 0, 0)));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)p.action);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 1, 5, 0), p.wakeEpoch);
  TEST_ASSERT_EQUAL_INT32(8 * 3600, p.sleepSeconds);
  expectWake(L(2026, 11, 1, 5, 0, 1), WakeReason::Timer, Action::Live, L(2026, 11, 1, 5, 30));
  // reset at the repeated 01:30 (second occurrence, CST): 3.5 h to 05:00 CST
  const int64_t mid = L(2026, 11, 1, 0, 0);          // 00:00 CDT, unambiguous
  int64_t second = mid + 2 * 3600 + 1800;           // 01:30 CST (clocks repeated 01:00-02:00)
  Plan r = planWake(base(second, WakeReason::Reset));
  TEST_ASSERT_EQUAL_INT((int)Action::Overnight, (int)r.action);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 1, 5, 0), r.wakeEpoch);
  TEST_ASSERT_EQUAL_INT32(3 * 3600 + 1800, r.sleepSeconds);
  // first 01:30 (CDT): 4.5 h
  Plan r0 = planWake(base(mid + 3600 + 1800, WakeReason::Reset));   // 01:30 CDT
  TEST_ASSERT_EQUAL_INT32(4 * 3600 + 1800, r0.sleepSeconds);
  TEST_ASSERT_EQUAL_INT32(7 * 3600, planWake(base(L(2026, 11, 1, 22, 0))).sleepSeconds);
}
void test_dst_no_double_0500_across_transitions() {
  // from 22:00 the evening before: one wake at 05:00 local, then 05:30
  const int days[2][3] = {{2026, 3, 7}, {2026, 10, 31}};
  for (int i = 0; i < 2; i++) {
    Plan p = planWake(base(L(days[i][0], days[i][1], days[i][2], 22, 0)));
    time_t w = (time_t)p.wakeEpoch;
    struct tm lt = *localtime(&w);
    TEST_ASSERT_EQUAL_INT(5, lt.tm_hour);
    TEST_ASSERT_EQUAL_INT(0, lt.tm_min);
    Plan q = planWake(base(p.wakeEpoch - 15));   // 15 s early
    TEST_ASSERT_EQUAL_INT((int)Action::Live, (int)q.action);
    TEST_ASSERT_EQUAL_INT64(p.wakeEpoch + 1800, q.wakeEpoch);
  }
}

// ---- helpers
void test_sleep_seconds_clamp_and_midnight_helpers() {
  TEST_ASSERT_EQUAL_INT32(5, sleepSecondsUntil(100, 200));
  TEST_ASSERT_EQUAL_INT32(60, sleepSecondsUntil(260, 200));
  TEST_ASSERT_EQUAL_INT(20261121, localDayKey(L(2026, 11, 21, 23, 59)));
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 21, 0, 0), localMidnight(L(2026, 11, 21, 13, 7), 0));
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 0, 0), localMidnight(L(2026, 11, 21, 13, 7), 1));
  // DST day lengths
  TEST_ASSERT_EQUAL_INT64(23 * 3600, localMidnight(L(2026, 3, 8, 12, 0), 1) - localMidnight(L(2026, 3, 8, 12, 0), 0));
  TEST_ASSERT_EQUAL_INT64(25 * 3600, localMidnight(L(2026, 11, 1, 12, 0), 1) - localMidnight(L(2026, 11, 1, 12, 0), 0));
}

int main() {
  setTz();
  UNITY_BEGIN();
  RUN_TEST(test_every_half_hour_slot);
  RUN_TEST(test_2130_goes_to_2200);
  RUN_TEST(test_2200_overnight_then_0500_next_day);
  RUN_TEST(test_0500_is_live);
  RUN_TEST(test_stray_timer_wake_in_quiet_hours_sleeps_to_0500);
  RUN_TEST(test_reset_at_0100_overnight_yesterday_today);
  RUN_TEST(test_reset_before_midnight_in_quiet_hours_today_tomorrow);
  RUN_TEST(test_reset_in_live_hours_is_live_and_aligns);
  RUN_TEST(test_early_wake_0459_40_is_the_0500_slot);
  RUN_TEST(test_early_wake_xx29_50_is_the_slot);
  RUN_TEST(test_early_wake_2159_50_is_overnight);
  RUN_TEST(test_wake_too_early_is_not_rounded_up);
  RUN_TEST(test_late_wake_still_the_slot);
  RUN_TEST(test_midnight_rollover_with_pad);
  RUN_TEST(test_charge_me_enter);
  RUN_TEST(test_charge_me_exactly_3_3_is_not_low);
  RUN_TEST(test_charge_me_stays_with_hysteresis_and_no_redraw);
  RUN_TEST(test_charge_me_redraws_if_not_shown);
  RUN_TEST(test_charge_me_exit_at_3_7);
  RUN_TEST(test_charge_me_exit_in_quiet_hours_redraws_overnight);
  RUN_TEST(test_charge_me_needs_no_clock);
  RUN_TEST(test_bad_battery_reading_ignored);
  RUN_TEST(test_no_valid_time);
  RUN_TEST(test_dst_spring_forward_day_2026_03_08);
  RUN_TEST(test_dst_fall_back_day_2026_11_01);
  RUN_TEST(test_dst_no_double_0500_across_transitions);
  RUN_TEST(test_sleep_seconds_clamp_and_midnight_helpers);
  return UNITY_END();
}
