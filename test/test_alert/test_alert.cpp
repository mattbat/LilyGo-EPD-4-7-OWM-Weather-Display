#include <unity.h>
#include <string.h>
#include <math.h>
#include "alert.h"
#include "../../tools/native/fixture_loader.h"

static WeatherData d;

void setUp() { fixtureSetTimezone(); weatherDataInit(d); d.have = HAVE_FORECAST | HAVE_OBS; d.valid = true; }
void tearDown() {}

void test_default_is_none() {
  Alert a = pickAlert(d);
  TEST_ASSERT_EQUAL((int)AlertKind::None, (int)a.kind);
  TEST_ASSERT_EQUAL_STRING("", a.headline);
  TEST_ASSERT_EQUAL_STRING("", a.sub);
}
void test_calm_values_are_none() {
  d.cur.strikes3h = 0; d.cur.rainRateInHr = 0; d.cur.uv = 5.9f;
  TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(d).kind);
}

void test_lightning_text_and_priority() {
  d.cur.strikes3h = 28; d.cur.lastStrikeMi = 3.2f;
  d.cur.rainRateInHr = 0.4f; d.cur.uv = 9;
  Alert a = pickAlert(d);
  TEST_ASSERT_EQUAL((int)AlertKind::Lightning, (int)a.kind);
  TEST_ASSERT_EQUAL_STRING("28 lightning strikes", a.headline);
  TEST_ASSERT_EQUAL_STRING("in the last 3 hours \xC2\xB7 nearest 3 mi", a.sub);
}
void test_lightning_boundary_one_strike() {
  d.cur.strikes3h = 1; d.cur.lastStrikeMi = NAN;
  Alert a = pickAlert(d);
  TEST_ASSERT_EQUAL((int)AlertKind::Lightning, (int)a.kind);
  TEST_ASSERT_EQUAL_STRING("1 lightning strike", a.headline);   // singular
  TEST_ASSERT_EQUAL_STRING("in the last 3 hours", a.sub);       // unknown distance: no "nearest"
  d.cur.strikes3h = 0;
  TEST_ASSERT_NOT_EQUAL((int)AlertKind::Lightning, (int)pickAlert(d).kind);
}
void test_lightning_fallback_from_history_without_obs() {
  d.have = HAVE_FORECAST | HAVE_HISTORY;
  d.hist[21].strikes = 2; d.hist[22].strikes = 0; d.hist[23].strikes = 3; d.hist[20].strikes = 50;  // 20 is older than 3 h
  Alert a = pickAlert(d);
  TEST_ASSERT_EQUAL((int)AlertKind::Lightning, (int)a.kind);
  TEST_ASSERT_EQUAL_STRING("5 lightning strikes", a.headline);
}

void test_raining_text() {
  d.cur.rainRateInHr = 0.24f; d.cur.rainTodayIn = 0.65f; d.cur.uv = 9;
  Alert a = pickAlert(d);
  TEST_ASSERT_EQUAL((int)AlertKind::Raining, (int)a.kind);
  TEST_ASSERT_EQUAL_STRING("Raining 0.24 in/hr", a.headline);
  TEST_ASSERT_EQUAL_STRING("0.65 in so far today", a.sub);
}
void test_raining_threshold_and_missing_total() {
  d.cur.rainRateInHr = 0.0f;   TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(d).kind);
  d.cur.rainRateInHr = NAN;    TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(d).kind);
  d.cur.rainRateInHr = 0.001f; TEST_ASSERT_EQUAL((int)AlertKind::Raining, (int)pickAlert(d).kind);
  d.cur.rainRateInHr = 0.1f; d.cur.rainTodayIn = NAN;
  Alert a = pickAlert(d);
  TEST_ASSERT_EQUAL_STRING("", a.sub);                           // no "nan in so far today"
  d.today.valid = true; d.today.rainIn = 0.33f;                  // falls back to the day summary
  TEST_ASSERT_EQUAL_STRING("0.33 in so far today", pickAlert(d).sub);
}

void test_uv_boundaries_and_text() {
  d.cur.uv = 5.9f; TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(d).kind);
  d.cur.uv = 6.0f;
  Alert a = pickAlert(d);
  TEST_ASSERT_EQUAL((int)AlertKind::UV, (int)a.kind);
  TEST_ASSERT_EQUAL_STRING("UV 6 \xC2\xB7 high", a.headline);
  TEST_ASSERT_EQUAL_STRING("limit time in the sun", a.sub);
  d.cur.uv = 7.4f;  TEST_ASSERT_EQUAL_STRING("UV 7 · high", pickAlert(d).headline);
  d.cur.uv = 7.9f;  TEST_ASSERT_EQUAL_STRING("UV 8 · very high", pickAlert(d).headline);   // wording follows the shown number
  d.cur.uv = 8.0f;  TEST_ASSERT_EQUAL_STRING("UV 8 \xC2\xB7 very high", pickAlert(d).headline);
  d.cur.uv = 10.4f; TEST_ASSERT_EQUAL_STRING("UV 10 · very high", pickAlert(d).headline);
  d.cur.uv = 11.0f; TEST_ASSERT_EQUAL_STRING("UV 11 \xC2\xB7 extreme", pickAlert(d).headline);
  d.cur.uv = NAN;   TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(d).kind);
}
void test_priority_order() {
  d.cur.uv = 10; TEST_ASSERT_EQUAL((int)AlertKind::UV, (int)pickAlert(d).kind);
  d.cur.rainRateInHr = 0.1f; TEST_ASSERT_EQUAL((int)AlertKind::Raining, (int)pickAlert(d).kind);
  d.cur.strikes3h = 1; TEST_ASSERT_EQUAL((int)AlertKind::Lightning, (int)pickAlert(d).kind);
}
void test_forecast_rain_is_not_an_alert() {
  for (int i = 0; i < 24; i++) d.hourly[i] = HourlyFc{1795316400u + (uint32_t)i * 3600u, 60.0f, 5.0f, 0.0f, 95, ICON_RAINY};
  d.hourlyCount = 24;
  TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(d).kind);   // the old "next rain" slot state is gone
}

void test_real_and_variant_fixtures() {
  WeatherData f;
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, f));
  TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(f).kind);
  TEST_ASSERT_TRUE(loadFixtureSet("storm", f));
  Alert a = pickAlert(f);
  TEST_ASSERT_EQUAL((int)AlertKind::Lightning, (int)a.kind);
  TEST_ASSERT_EQUAL_STRING("28 lightning strikes", a.headline);
  TEST_ASSERT_EQUAL_STRING("in the last 3 hours \xC2\xB7 nearest 3 mi", a.sub);
  TEST_ASSERT_TRUE(loadFixtureSet("raining", f));
  TEST_ASSERT_EQUAL((int)AlertKind::Raining, (int)pickAlert(f).kind);
  TEST_ASSERT_TRUE(loadFixtureSet("hot", f));
  TEST_ASSERT_EQUAL((int)AlertKind::UV, (int)pickAlert(f).kind);
  TEST_ASSERT_TRUE(loadFixtureSet("cold", f));
  TEST_ASSERT_EQUAL((int)AlertKind::None, (int)pickAlert(f).kind);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_is_none);
  RUN_TEST(test_calm_values_are_none);
  RUN_TEST(test_lightning_text_and_priority);
  RUN_TEST(test_lightning_boundary_one_strike);
  RUN_TEST(test_lightning_fallback_from_history_without_obs);
  RUN_TEST(test_raining_text);
  RUN_TEST(test_raining_threshold_and_missing_total);
  RUN_TEST(test_uv_boundaries_and_text);
  RUN_TEST(test_priority_order);
  RUN_TEST(test_forecast_rain_is_not_an_alert);
  RUN_TEST(test_real_and_variant_fixtures);
  return UNITY_END();
}
