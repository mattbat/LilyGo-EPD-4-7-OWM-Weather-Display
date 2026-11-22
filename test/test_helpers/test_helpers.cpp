#include <unity.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <fstream>
#include <sstream>
#include "helpers.h"
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
static int64_t L(int y, int mo, int d, int h, int mi, int s = 0) {
  struct tm t = {};
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
  t.tm_isdst = -1;
  return (int64_t)mktime(&t);
}
static std::string load(const char* name) {
  std::ifstream f(std::string("test/fixtures/") + name, std::ios::binary);
  std::stringstream ss; ss << f.rdbuf();
  return ss.str();
}

// ---- battery
void test_battery_percent_clamp_and_range() {
  TEST_ASSERT_EQUAL_INT(0, batteryPercent(0.0f));
  TEST_ASSERT_EQUAL_INT(0, batteryPercent(3.2f));
  TEST_ASSERT_EQUAL_INT(0, batteryPercent(2.5f));
  TEST_ASSERT_EQUAL_INT(100, batteryPercent(4.2f));
  TEST_ASSERT_EQUAL_INT(100, batteryPercent(5.0f));
  TEST_ASSERT_EQUAL_INT(0, batteryPercent(NAN));
  for (float v = 3.0f; v <= 4.5f; v += 0.01f) {
    int p = batteryPercent(v);
    TEST_ASSERT_TRUE(p >= 0 && p <= 100);
  }
  int mid = batteryPercent(3.8f);
  TEST_ASSERT_TRUE(mid > 30 && mid < 80);
  TEST_ASSERT_TRUE(batteryPercent(4.1f) > batteryPercent(3.7f));
}
void test_station_battery_text() {
  char b[24];
  stationBatteryText(2.45f, b, sizeof b); TEST_ASSERT_EQUAL_STRING("OK", b);
  stationBatteryText(2.6f, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("OK", b);
  stationBatteryText(2.44f, b, sizeof b); TEST_ASSERT_EQUAL_STRING("LOW 2.44 V", b);
  stationBatteryText(2.1f, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("LOW 2.10 V", b);
  stationBatteryText(NAN, b, sizeof b);   TEST_ASSERT_EQUAL_STRING("--", b);
}

// ---- URLs
void test_url_builders() {
  char u[400];
  tempestUrlForecast(u, sizeof u, "TOK", 12345);
  TEST_ASSERT_EQUAL_STRING("https://swd.weatherflow.com/swd/rest/better_forecast?station_id=12345&units_temp=f&units_wind=mph"
                           "&units_pressure=inhg&units_precip=in&units_distance=mi&token=TOK", u);
  tempestUrlStationObs(u, sizeof u, "TOK", 12345);
  TEST_ASSERT_EQUAL_STRING("https://swd.weatherflow.com/swd/rest/observations/station/12345?token=TOK", u);
  tempestUrlStations(u, sizeof u, "TOK");
  TEST_ASSERT_EQUAL_STRING("https://swd.weatherflow.com/swd/rest/stations?token=TOK", u);
  tempestUrlDeviceHistory(u, sizeof u, "TOK", 249992, 1795200000LL, 1795313256LL);
  TEST_ASSERT_EQUAL_STRING("https://swd.weatherflow.com/swd/rest/observations/device/249992?time_start=1795200000"
                           "&time_end=1795313256&token=TOK", u);
}
void test_url_truncation_reported() {
  char u[20];
  int n = tempestUrlStations(u, sizeof u, "TOK");
  TEST_ASSERT_TRUE(n >= (int)sizeof u);
}
void test_redaction() {
  char u[300], r[300];
  tempestUrlStationObs(u, sizeof u, "aaaa-bbbb-cccc", 1);
  redactUrl(u, r, sizeof r);
  TEST_ASSERT_EQUAL_STRING("https://swd.weatherflow.com/swd/rest/observations/station/1?token=REDACTED", r);
  TEST_ASSERT_NULL(strstr(r, "aaaa"));
  // token in the middle of the query
  redactUrl("https://x/y?token=SECRET&a=1", r, sizeof r);
  TEST_ASSERT_EQUAL_STRING("https://x/y?token=REDACTED&a=1", r);
  // token last in the forecast URL
  tempestUrlForecast(u, sizeof u, "SECRET", 9);
  redactUrl(u, r, sizeof r);
  TEST_ASSERT_NULL(strstr(r, "SECRET"));
  TEST_ASSERT_NOT_NULL(strstr(r, "units_distance=mi&token=REDACTED"));
  // no token: unchanged
  redactUrl("https://x/y?a=1", r, sizeof r);
  TEST_ASSERT_EQUAL_STRING("https://x/y?a=1", r);
  // tiny buffer never overflows
  char t[8]; redactUrl(u, t, sizeof t);
  TEST_ASSERT_EQUAL_INT(7, (int)strlen(t));
}

// ---- history window
void test_history_plan_today() {
  int64_t now = L(2026, 11, 21, 14, 7);
  HistoryPlan h = planHistory(now, TitlesMode::TodayTomorrow, 40.0f);
  TEST_ASSERT_EQUAL_INT64(now, h.timeEnd);
  TEST_ASSERT_EQUAL_INT64(now, h.window.now);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 21, 0, 0), h.window.dayStart);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 0, 0), h.window.dayEnd);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, h.window.pressureOffsetMb);
  // covers every one of the 24 hourly buckets (bucket 0 starts 23 h before the top of the current hour)
  int64_t bucket0 = L(2026, 11, 21, 14, 0) - 23 * 3600;
  TEST_ASSERT_TRUE(h.timeStart <= bucket0);
  TEST_ASSERT_TRUE(h.timeEnd - h.timeStart <= 30 * 3600);
}
void test_history_plan_yesterday_today_reaches_yesterday_midnight() {
  int64_t now = L(2026, 11, 22, 4, 59);   // worst case: just before 05:00
  HistoryPlan h = planHistory(now, TitlesMode::YesterdayToday, NAN);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 21, 0, 0), h.window.dayStart);
  TEST_ASSERT_EQUAL_INT64(L(2026, 11, 22, 0, 0), h.window.dayEnd);
  TEST_ASSERT_TRUE(h.timeStart <= h.window.dayStart);
  TEST_ASSERT_TRUE(h.timeEnd - h.timeStart <= 30 * 3600);
  TEST_ASSERT_TRUE(h.timeEnd - h.timeStart >= 28 * 3600);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, h.window.pressureOffsetMb);   // NAN offset -> 0
}
void test_history_plan_dst_day_windows() {
  HistoryPlan h = planHistory(L(2026, 3, 8, 12, 0), TitlesMode::TodayTomorrow, 0);
  TEST_ASSERT_EQUAL_INT64(23 * 3600, h.window.dayEnd - h.window.dayStart);
  h = planHistory(L(2026, 11, 1, 12, 0), TitlesMode::TodayTomorrow, 0);
  TEST_ASSERT_EQUAL_INT64(25 * 3600, h.window.dayEnd - h.window.dayStart);
}
void test_history_plan_feeds_the_parser_on_fixture() {
  const int64_t NOW = 1795313256;   // matches the fixtures
  HistoryPlan h = planHistory(NOW, TitlesMode::TodayTomorrow, 0);
  std::string j = load("device_history.json");
  TEST_ASSERT_TRUE(j.size() > 1000);
  static WeatherData wd; weatherDataInit(wd);
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), h.window, wd));
  TEST_ASSERT_TRUE(wd.have & HAVE_HISTORY);
  TEST_ASSERT_TRUE(wd.today.valid);
}

// ---- pressure offset from the station obs
void test_pressure_offset_from_station_obs() {
  static WeatherData wd; weatherDataInit(wd);
  TEST_ASSERT_TRUE(isnan(wd.pressureOffsetMb));
  std::string j = load("station_obs.json");
  TEST_ASSERT_EQUAL(PARSE_OK, parseStationObs(j.c_str(), j.size(), wd));
  TEST_ASSERT_FALSE(isnan(wd.pressureOffsetMb));
  TEST_ASSERT_TRUE(wd.pressureOffsetMb > 20.0f && wd.pressureOffsetMb < 60.0f);
}

// ---- fetch result text and retry policy
void test_fetch_result_text_and_retry() {
  FetchResult r; memset(&r, 0, sizeof r);
  char b[96];
  r.status = FETCH_ERR_WIFI;
  describeFetchResult(r, b, sizeof b); TEST_ASSERT_EQUAL_STRING("WiFi failed", b);
  TEST_ASSERT_TRUE(fetchShouldRetry(r));
  r.status = FETCH_ERR_HTTP; r.httpCode = 401; strcpy(r.what, "forecast");
  describeFetchResult(r, b, sizeof b); TEST_ASSERT_EQUAL_STRING("Tempest API error: HTTP 401 (forecast)", b);
  TEST_ASSERT_FALSE(fetchShouldRetry(r));
  r.httpCode = 503; TEST_ASSERT_TRUE(fetchShouldRetry(r));
  r.httpCode = -1;
  describeFetchResult(r, b, sizeof b); TEST_ASSERT_NOT_NULL(strstr(b, "unreachable"));
  TEST_ASSERT_TRUE(fetchShouldRetry(r));
  r.status = FETCH_ERR_PARSE; r.parse = PARSE_ERR_JSON;
  TEST_ASSERT_TRUE(fetchShouldRetry(r));
  r.parse = PARSE_ERR_API; TEST_ASSERT_FALSE(fetchShouldRetry(r));
  describeFetchResult(r, b, sizeof b); TEST_ASSERT_NOT_NULL(strstr(b, "API status"));
  r.status = FETCH_ERR_CONFIG; TEST_ASSERT_FALSE(fetchShouldRetry(r));
  r.status = FETCH_OK; TEST_ASSERT_FALSE(fetchShouldRetry(r));
}

int main() {
  setTz();
  UNITY_BEGIN();
  RUN_TEST(test_battery_percent_clamp_and_range);
  RUN_TEST(test_station_battery_text);
  RUN_TEST(test_url_builders);
  RUN_TEST(test_url_truncation_reported);
  RUN_TEST(test_redaction);
  RUN_TEST(test_history_plan_today);
  RUN_TEST(test_history_plan_yesterday_today_reaches_yesterday_midnight);
  RUN_TEST(test_history_plan_dst_day_windows);
  RUN_TEST(test_history_plan_feeds_the_parser_on_fixture);
  RUN_TEST(test_pressure_offset_from_station_obs);
  RUN_TEST(test_fetch_result_text_and_retry);
  return UNITY_END();
}
