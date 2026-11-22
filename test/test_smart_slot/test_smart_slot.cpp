#include <unity.h>
#include <string.h>
#include <math.h>
#include "smart_slot.h"
#include "../../tools/native/fixture_loader.h"

static const int64_t NOW = FIXTURE_NOW;            // Sat Nov 21 2026 8:07 PM CST
static const uint32_t NEXT_HOUR = 1795316400u;     // 9 PM, first hourly forecast entry
static WeatherData d;

void setUp() { fixtureSetTimezone(); weatherDataInit(d); d.have = HAVE_FORECAST | HAVE_OBS; d.valid = true; }
void tearDown() {}

// d.hourly[0..n): one entry per hour starting at 9 PM, probability `p` for all of them.
static void fillHourly(int n, int p) {
  d.hourlyCount = (uint8_t)n;
  for (int i = 0; i < n; i++) {
    d.hourly[i] = HourlyFc{NEXT_HOUR + (uint32_t)i * 3600u, 60.0f, 5.0f, 0.0f, (uint8_t)p, ICON_CLOUDY};
  }
}

void test_default_is_no_rain() {
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::NoRain, (int)s.kind);
  TEST_ASSERT_EQUAL((int)SlotIcon::None, (int)s.icon);
  TEST_ASSERT_EQUAL_STRING("NEXT RAIN", s.label);
  TEST_ASSERT_EQUAL_STRING("None", s.big);
  TEST_ASSERT_EQUAL_STRING("No rain in", s.sub1);
  TEST_ASSERT_EQUAL_STRING("3 days", s.sub2);
}

void test_lightning_wins_over_everything() {
  d.cur.strikes3h = 28; d.cur.lastStrikeMi = 3.2f;
  d.cur.rainRateInHr = 0.4f; d.cur.uv = 9;
  fillHourly(72, 90);
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::Lightning, (int)s.kind);
  TEST_ASSERT_EQUAL((int)SlotIcon::Bolt, (int)s.icon);
  TEST_ASSERT_EQUAL_STRING("LIGHTNING", s.label);
  TEST_ASSERT_EQUAL_STRING("28", s.big);
  TEST_ASSERT_EQUAL_STRING("strikes \xC2\xB7 3 h", s.sub1);
  TEST_ASSERT_EQUAL_STRING("nearest 3 mi", s.sub2);
}
void test_lightning_boundary_one_strike() {
  d.cur.strikes3h = 1; d.cur.lastStrikeMi = NAN;
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::Lightning, (int)s.kind);
  TEST_ASSERT_EQUAL_STRING("1", s.big);
  TEST_ASSERT_EQUAL_STRING("", s.sub2);            // unknown distance: no "nearest" line
  d.cur.strikes3h = 0;
  TEST_ASSERT_NOT_EQUAL((int)SlotKind::Lightning, (int)pickSmartSlot(d, NOW).kind);
}
void test_lightning_fallback_from_history_without_obs() {
  d.have = HAVE_FORECAST | HAVE_HISTORY;
  d.hist[21].strikes = 2; d.hist[22].strikes = 0; d.hist[23].strikes = 3; d.hist[20].strikes = 50;  // 20 is older than 3 h
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::Lightning, (int)s.kind);
  TEST_ASSERT_EQUAL_STRING("5", s.big);
}

void test_raining_with_rate() {
  d.cur.rainRateInHr = 0.41f; d.cur.uv = 9;
  fillHourly(72, 90);
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::Raining, (int)s.kind);
  TEST_ASSERT_EQUAL_STRING("RAINING", s.label);
  TEST_ASSERT_EQUAL_STRING("0.41", s.big);
  TEST_ASSERT_EQUAL_STRING("in / hr", s.sub1);
  TEST_ASSERT_EQUAL_STRING("heavy", s.sub2);
}
void test_raining_descriptors_and_zero_nan() {
  d.cur.rainRateInHr = 0.05f; TEST_ASSERT_EQUAL_STRING("light", pickSmartSlot(d, NOW).sub2);
  d.cur.rainRateInHr = 0.2f;  TEST_ASSERT_EQUAL_STRING("moderate", pickSmartSlot(d, NOW).sub2);
  d.cur.rainRateInHr = 0.0f;  TEST_ASSERT_NOT_EQUAL((int)SlotKind::Raining, (int)pickSmartSlot(d, NOW).kind);
  d.cur.rainRateInHr = NAN;   TEST_ASSERT_NOT_EQUAL((int)SlotKind::Raining, (int)pickSmartSlot(d, NOW).kind);
  d.cur.rainRateInHr = 0.001f; TEST_ASSERT_EQUAL((int)SlotKind::Raining, (int)pickSmartSlot(d, NOW).kind);
}

void test_uv_boundaries() {
  d.cur.uv = 5.9f; TEST_ASSERT_NOT_EQUAL((int)SlotKind::UV, (int)pickSmartSlot(d, NOW).kind);
  d.cur.uv = 6.0f;
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::UV, (int)s.kind);
  TEST_ASSERT_EQUAL((int)SlotIcon::Sun, (int)s.icon);
  TEST_ASSERT_EQUAL_STRING("UV", s.label);
  TEST_ASSERT_EQUAL_STRING("6", s.big);
  TEST_ASSERT_EQUAL_STRING("high", s.sub1);
  d.cur.uv = 7.9f;  TEST_ASSERT_EQUAL_STRING("high", pickSmartSlot(d, NOW).sub1);
  d.cur.uv = 8.0f;  TEST_ASSERT_EQUAL_STRING("very high", pickSmartSlot(d, NOW).sub1);
  d.cur.uv = 10.9f; TEST_ASSERT_EQUAL_STRING("very high", pickSmartSlot(d, NOW).sub1);
  d.cur.uv = 11.0f; TEST_ASSERT_EQUAL_STRING("extreme", pickSmartSlot(d, NOW).sub1);
  TEST_ASSERT_EQUAL_STRING("11", pickSmartSlot(d, NOW).big);
  d.cur.uv = NAN;   TEST_ASSERT_NOT_EQUAL((int)SlotKind::UV, (int)pickSmartSlot(d, NOW).kind);
}
void test_uv_loses_to_rain_and_lightning() {
  d.cur.uv = 10; d.cur.rainRateInHr = 0.1f;
  TEST_ASSERT_EQUAL((int)SlotKind::Raining, (int)pickSmartSlot(d, NOW).kind);
  d.cur.strikes3h = 1;
  TEST_ASSERT_EQUAL((int)SlotKind::Lightning, (int)pickSmartSlot(d, NOW).kind);
}

void test_next_rain_threshold_and_day_name() {
  fillHourly(72, 0);
  d.hourly[3].precipProbPct = 39;     // just below the threshold
  TEST_ASSERT_EQUAL((int)SlotKind::NoRain, (int)pickSmartSlot(d, NOW).kind);
  d.hourly[3].precipProbPct = 40;     // exactly the threshold counts. 9 PM + 3 h = midnight = Sunday
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::NextRain, (int)s.kind);
  TEST_ASSERT_EQUAL((int)SlotIcon::Rain, (int)s.icon);
  TEST_ASSERT_EQUAL_STRING("NEXT RAIN", s.label);
  TEST_ASSERT_EQUAL_STRING("Sun", s.big);
  TEST_ASSERT_EQUAL_STRING("12 AM", s.sub1);
  TEST_ASSERT_EQUAL_STRING("40% chance", s.sub2);
}
void test_next_rain_same_day_says_today() {
  fillHourly(72, 0);
  d.hourly[1].precipProbPct = 65;     // 10 PM tonight
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL_STRING("Today", s.big);
  TEST_ASSERT_EQUAL_STRING("10 PM", s.sub1);
  TEST_ASSERT_EQUAL_STRING("65% chance", s.sub2);
}
void test_next_rain_first_match_wins() {
  fillHourly(72, 0);
  d.hourly[10].precipProbPct = 50;    // 7 AM Sunday
  d.hourly[2].precipProbPct = 45;     // 11 PM Saturday: earlier, wins
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL_STRING("11 PM", s.sub1);
}
void test_next_rain_72h_window() {
  fillHourly(72, 0);
  d.hourly[71].time = (uint32_t)(NOW + 72 * 3600);        // exactly 72 h ahead: included
  d.hourly[71].precipProbPct = 80;
  SmartSlot s = pickSmartSlot(d, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::NextRain, (int)s.kind);
  TEST_ASSERT_EQUAL_STRING("80% chance", s.sub2);
  d.hourly[71].time = (uint32_t)(NOW + 72 * 3600 + 1);    // one second further: excluded
  TEST_ASSERT_EQUAL((int)SlotKind::NoRain, (int)pickSmartSlot(d, NOW).kind);
}
void test_next_rain_ignores_past_hours_of_stale_data() {
  fillHourly(10, 0);
  d.hourly[0].precipProbPct = 90;                         // the 9 PM hour
  TEST_ASSERT_EQUAL((int)SlotKind::NextRain, (int)pickSmartSlot(d, NOW).kind);
  // once "now" is 10:00:00 PM that hour is over
  TEST_ASSERT_EQUAL((int)SlotKind::NoRain, (int)pickSmartSlot(d, NEXT_HOUR + 3600).kind);
  // still counts at 9:59:59 PM
  TEST_ASSERT_EQUAL((int)SlotKind::NextRain, (int)pickSmartSlot(d, NEXT_HOUR + 3599).kind);
}
void test_no_forecast_is_no_rain() {
  d.hourlyCount = 0;
  TEST_ASSERT_EQUAL((int)SlotKind::NoRain, (int)pickSmartSlot(d, NOW).kind);
}

void test_real_and_variant_fixtures() {
  WeatherData f;
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, f));
  TEST_ASSERT_EQUAL((int)SlotKind::NextRain, (int)pickSmartSlot(f, NOW).kind);   // 50 % at 9 PM
  TEST_ASSERT_TRUE(loadFixtureSet("storm", f));
  SmartSlot s = pickSmartSlot(f, NOW);
  TEST_ASSERT_EQUAL((int)SlotKind::Lightning, (int)s.kind);
  TEST_ASSERT_EQUAL_STRING("28", s.big);
  TEST_ASSERT_EQUAL_STRING("nearest 3 mi", s.sub2);
  TEST_ASSERT_TRUE(loadFixtureSet("raining", f));
  TEST_ASSERT_EQUAL((int)SlotKind::Raining, (int)pickSmartSlot(f, NOW).kind);
  TEST_ASSERT_TRUE(loadFixtureSet("hot", f));
  TEST_ASSERT_EQUAL((int)SlotKind::UV, (int)pickSmartSlot(f, NOW).kind);
  TEST_ASSERT_TRUE(loadFixtureSet("cold", f));
  TEST_ASSERT_EQUAL((int)SlotKind::NextRain, (int)pickSmartSlot(f, NOW).kind);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_is_no_rain);
  RUN_TEST(test_lightning_wins_over_everything);
  RUN_TEST(test_lightning_boundary_one_strike);
  RUN_TEST(test_lightning_fallback_from_history_without_obs);
  RUN_TEST(test_raining_with_rate);
  RUN_TEST(test_raining_descriptors_and_zero_nan);
  RUN_TEST(test_uv_boundaries);
  RUN_TEST(test_uv_loses_to_rain_and_lightning);
  RUN_TEST(test_next_rain_threshold_and_day_name);
  RUN_TEST(test_next_rain_same_day_says_today);
  RUN_TEST(test_next_rain_first_match_wins);
  RUN_TEST(test_next_rain_72h_window);
  RUN_TEST(test_next_rain_ignores_past_hours_of_stale_data);
  RUN_TEST(test_no_forecast_is_no_rain);
  RUN_TEST(test_real_and_variant_fixtures);
  return UNITY_END();
}
