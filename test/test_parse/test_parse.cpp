#include <unity.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <sstream>
#include "parse.h"
#include "units.h"

void setUp() {}
void tearDown() {}

static std::string load(const char* name) {
  std::string path = std::string("test/fixtures/") + name;
  std::ifstream f(path, std::ios::binary);
  if (!f) { TEST_FAIL_MESSAGE(("cannot open " + path).c_str()); }
  std::stringstream ss; ss << f.rdbuf();
  return ss.str();
}

static const int64_t NOW = 1795313256;          // current_conditions.time of the fixtures
static const int64_t DAY0 = 1795240800;         // local midnight, Sat Nov 21 2026 (CST)
static WeatherData wd;
static HistoryWindow fixtureWindow() { HistoryWindow w{NOW, DAY0, DAY0 + 86400, 0.0f}; return w; }

// ------------------------------------------------------------ WeatherData
void test_struct_size_fits_rtc() {
  char msg[64]; snprintf(msg, sizeof msg, "sizeof(WeatherData) = %u", (unsigned)sizeof(WeatherData));
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE(sizeof(WeatherData) < 6000);
}
void test_init_state() {
  weatherDataInit(wd);
  TEST_ASSERT_FALSE(wd.valid);
  TEST_ASSERT_EQUAL_UINT8(0, wd.have);
  TEST_ASSERT_TRUE(isnan(wd.cur.tempF));
  TEST_ASSERT_TRUE(isnan(wd.hist[3].tempF));
  TEST_ASSERT_FALSE(wd.hist[3].valid);
  TEST_ASSERT_FALSE(wd.rainMonthYearKnown);
  TEST_ASSERT_FALSE(wd.today.valid);
}

// ------------------------------------------------------------ better_forecast
void test_forecast_current() {
  weatherDataInit(wd);
  std::string j = load("better_forecast.json");
  TEST_ASSERT_EQUAL(PARSE_OK, parseBetterForecast(j.c_str(), j.size(), wd));
  TEST_ASSERT_TRUE(wd.valid);
  TEST_ASSERT_TRUE(wd.have & HAVE_FORECAST);
  const CurrentConditions& c = wd.cur;
  TEST_ASSERT_EQUAL_UINT32(1795313256u, c.time);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 63.0f, c.tempF);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 63.0f, c.feelsLikeF);
  TEST_ASSERT_EQUAL_STRING("Rain Likely", c.conditions);
  TEST_ASSERT_EQUAL(ICON_RAINY, c.icon);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, c.windMph);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, c.gustMph);
  TEST_ASSERT_EQUAL_INT16(157, c.windDirDeg);
  TEST_ASSERT_EQUAL_UINT8(83, c.humidityPct);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 57.0f, c.dewPointF);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 29.8f, c.pressureInHg);
  TEST_ASSERT_EQUAL_INT8(TREND_STEADY, c.pressureTrend);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, c.uv);
  TEST_ASSERT_EQUAL_UINT8(50, c.precipProbPct);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.05f, c.rainTodayIn);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.0f, c.lastStrikeMi);
}
void test_forecast_hourly_daily() {
  weatherDataInit(wd);
  std::string j = load("better_forecast.json");
  TEST_ASSERT_EQUAL(PARSE_OK, parseBetterForecast(j.c_str(), j.size(), wd));
  TEST_ASSERT_EQUAL_UINT8(HOURLY_FC_MAX, wd.hourlyCount);   // 219 in the response, capped
  TEST_ASSERT_EQUAL_UINT32(1795316400u, wd.hourly[0].time);
  TEST_ASSERT_EQUAL_UINT32(1795320000u, wd.hourly[1].time);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 63.0f, wd.hourly[0].tempF);
  TEST_ASSERT_EQUAL_UINT8(50, wd.hourly[0].precipProbPct);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 17.0f, wd.hourly[0].gustMph);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.01f, wd.hourly[0].precipIn);
  TEST_ASSERT_EQUAL(ICON_RAINY, wd.hourly[0].icon);
  TEST_ASSERT_EQUAL_UINT8(6, wd.dailyCount);
  TEST_ASSERT_EQUAL_UINT32(1795240800u, wd.daily[0].dayStart);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 68.0f, wd.daily[0].highF);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 54.0f, wd.daily[0].lowF);
  TEST_ASSERT_EQUAL(ICON_THUNDERSTORM, wd.daily[0].icon);
  TEST_ASSERT_EQUAL_STRING("Thunderstorms Likely", wd.daily[0].conditions);
  TEST_ASSERT_EQUAL_UINT8(50, wd.daily[0].precipProbPct);
  TEST_ASSERT_EQUAL_UINT32(1795266700u, wd.daily[0].sunrise);
  TEST_ASSERT_EQUAL_UINT32(1795303126u, wd.daily[0].sunset);
  TEST_ASSERT_EQUAL(ICON_PARTLY_DAY, wd.daily[2].icon);
}
void test_icon_keys() {
  TEST_ASSERT_EQUAL(ICON_CLEAR_NIGHT, iconFromKey("clear-night"));
  TEST_ASSERT_EQUAL(ICON_POSSIBLY_THUNDER_NIGHT, iconFromKey("possibly-thunderstorm-night"));
  TEST_ASSERT_EQUAL(ICON_UNKNOWN, iconFromKey("bogus"));
  TEST_ASSERT_EQUAL(ICON_UNKNOWN, iconFromKey(nullptr));
  TEST_ASSERT_EQUAL_STRING("foggy", iconKeyName(ICON_FOGGY));
}
void test_forecast_streamed_from_istream() {
  weatherDataInit(wd);
  std::ifstream f("test/fixtures/better_forecast.json", std::ios::binary);
  TEST_ASSERT_TRUE(f.good());
  IStreamReader r(f);
  TEST_ASSERT_EQUAL(PARSE_OK, parseBetterForecast(r, wd));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 63.0f, wd.cur.tempF);
  TEST_ASSERT_EQUAL_UINT8(6, wd.dailyCount);
}
void test_forecast_missing_fields() {
  weatherDataInit(wd);
  const char* j = "{\"status\":{\"status_code\":0},\"current_conditions\":{\"air_temperature\":null,\"conditions\":null,"
                  "\"wind_gust\":3.5},\"forecast\":{\"hourly\":[{\"time\":100,\"air_temperature\":null},{\"junk\":1}],"
                  "\"daily\":[]}}";
  TEST_ASSERT_EQUAL(PARSE_OK, parseBetterForecast(j, strlen(j), wd));
  TEST_ASSERT_TRUE(isnan(wd.cur.tempF));
  TEST_ASSERT_TRUE(isnan(wd.cur.feelsLikeF));
  TEST_ASSERT_TRUE(isnan(wd.cur.dewPointF));
  TEST_ASSERT_EQUAL_STRING("", wd.cur.conditions);
  TEST_ASSERT_EQUAL(ICON_UNKNOWN, wd.cur.icon);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.5f, wd.cur.gustMph);
  TEST_ASSERT_EQUAL_UINT8(1, wd.hourlyCount);
  TEST_ASSERT_TRUE(isnan(wd.hourly[0].tempF));
  TEST_ASSERT_EQUAL_UINT8(0, wd.dailyCount);
}
void test_forecast_wrong_units_rejected() {
  weatherDataInit(wd);
  const char* j = "{\"current_conditions\":{\"air_temperature\":17},\"units\":{\"units_temp\":\"c\"}}";
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseBetterForecast(j, strlen(j), wd));
}

// ------------------------------------------------------------ station obs
void test_station_obs_fixture() {
  weatherDataInit(wd);
  std::string j = load("station_obs.json");
  TEST_ASSERT_EQUAL(PARSE_OK, parseStationObs(j.c_str(), j.size(), wd));
  TEST_ASSERT_TRUE(wd.have & HAVE_OBS);
  const CurrentConditions& c = wd.cur;
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 1.288441f / 25.4f, c.rainTodayIn);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.0f, c.rainYesterdayIn);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.0f, c.rainRateInHr);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.352665f / 25.4f, c.rainLastHourIn);
  TEST_ASSERT_EQUAL_UINT16(0, c.strikes3h);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.43f, c.lastStrikeMi);   // 20 km
  TEST_ASSERT_EQUAL_UINT32(1793924361u, c.lastStrikeEpoch);
  // no forecast yet: falls back to the metric obs values
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 62.6f, c.tempF);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 29.84f, c.pressureInHg);
  TEST_ASSERT_EQUAL_UINT8(83, c.humidityPct);
}
void test_obs_and_forecast_order_independent() {
  WeatherData a, b;
  std::string f = load("better_forecast.json"), o = load("station_obs.json");
  weatherDataInit(a); weatherDataInit(b);
  TEST_ASSERT_EQUAL(PARSE_OK, parseBetterForecast(f.c_str(), f.size(), a));
  TEST_ASSERT_EQUAL(PARSE_OK, parseStationObs(o.c_str(), o.size(), a));
  TEST_ASSERT_EQUAL(PARSE_OK, parseStationObs(o.c_str(), o.size(), b));
  TEST_ASSERT_EQUAL(PARSE_OK, parseBetterForecast(f.c_str(), f.size(), b));
  TEST_ASSERT_EQUAL(0, memcmp(&a.cur.lastStrikeEpoch, &b.cur.lastStrikeEpoch, sizeof(uint32_t)));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.cur.rainTodayIn, b.cur.rainTodayIn);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.cur.rainLastHourIn, b.cur.rainLastHourIn);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, a.cur.tempF, b.cur.tempF);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 63.0f, b.cur.tempF);          // forecast wins for shared fields
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.43f, b.cur.lastStrikeMi);  // obs wins for lightning
  TEST_ASSERT_EQUAL_UINT8(HAVE_FORECAST | HAVE_OBS, b.have);
}
void test_obs_rain_rate_conversion() {
  weatherDataInit(wd);
  const char* j = "{\"status\":{\"status_code\":0},\"obs\":[{\"precip\":0.5,\"precip_accum_local_yesterday_final\":2.54,"
                  "\"lightning_strike_count_last_3hr\":4,\"lightning_strike_last_distance\":8}]}";
  TEST_ASSERT_EQUAL(PARSE_OK, parseStationObs(j, strlen(j), wd));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f * 60.0f / 25.4f, wd.cur.rainRateInHr);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, wd.cur.rainYesterdayIn);
  TEST_ASSERT_EQUAL_UINT16(4, wd.cur.strikes3h);
  TEST_ASSERT_TRUE(isnan(wd.cur.rainTodayIn));
}

// ------------------------------------------------------------ stats / stations
void test_stats_fixture() {
  weatherDataInit(wd);
  std::string j = load("stats.json");
  TEST_ASSERT_EQUAL(PARSE_OK, parseStats(j.c_str(), j.size(), 2026, 11, wd));
  TEST_ASSERT_TRUE(wd.rainMonthYearKnown);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 66.3078f / 25.4f, wd.rainMonthIn);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 800.497f / 25.4f, wd.rainYearIn);
  TEST_ASSERT_TRUE(wd.have & HAVE_STATS);
}
void test_stats_wrong_month_is_unknown() {
  weatherDataInit(wd);
  std::string j = load("stats.json");
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseStats(j.c_str(), j.size(), 2026, 12, wd));
  TEST_ASSERT_FALSE(wd.rainMonthYearKnown);
  TEST_ASSERT_TRUE(isnan(wd.rainMonthIn));
  TEST_ASSERT_EQUAL(PARSE_OK, parseStats(j.c_str(), j.size(), 0, 0, wd));   // no date check
}
void test_stations_fixture() {
  uint32_t dev = 0;
  std::string j = load("stations.json");
  TEST_ASSERT_EQUAL(PARSE_OK, parseStations(j.c_str(), j.size(), 0, dev));
  TEST_ASSERT_EQUAL_UINT32(249992u, dev);
  dev = 0;
  TEST_ASSERT_EQUAL(PARSE_OK, parseStations(j.c_str(), j.size(), 97113, dev));
  TEST_ASSERT_EQUAL_UINT32(249992u, dev);
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseStations(j.c_str(), j.size(), 5, dev));
}

// ------------------------------------------------------------ history: synthetic
// Build one obs_st row (22 columns like the real API).
static std::string row(int64_t t, float wind, float gust, float pres, float temp, int rh, float rain, int strikes, float batt = 2.6f) {
  char b[256];
  snprintf(b, sizeof b, "[%lld,0.0,%g,%g,150,3,%g,%g,%d,1,0.0,0,%g,0,0,%d,%g,1,0.0,0.0,0.0,0]",
           (long long)t, wind, gust, pres, temp, rh, rain, strikes, batt);
  return b;
}
static std::string doc(const std::string& rows) { return "{\"status\":{\"status_code\":0},\"type\":\"obs_st\",\"obs\":[" + rows + "]}"; }
static const int64_t BASE = 1000000LL * 3600;    // top of an hour
static const int64_t SNOW = BASE + 1800;         // "now" is 30 min into the hour
static HistoryWindow noDay() { HistoryWindow w{SNOW, 0, 0, 0.0f}; return w; }

void test_bucket_boundaries() {
  weatherDataInit(wd);
  std::string r = row(BASE, 1, 2, 1000, 10, 50, 1.0f, 1) + "," +                // first second of bucket 23
                  row(BASE - 1, 1, 2, 1000, 20, 50, 2.0f, 2) + "," +             // last second of bucket 22
                  row(BASE - 23 * 3600, 1, 2, 1000, 30, 50, 4.0f, 0) + "," +     // first second of bucket 0
                  row(BASE - 23 * 3600 - 1, 1, 2, 1000, 99, 50, 8.0f, 8) + "," + // before the window: dropped
                  row(BASE + 3600, 1, 2, 1000, 99, 50, 16.0f, 16);               // future: dropped
  std::string j = doc(r);
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), noDay(), wd));
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(BASE - 23 * 3600), wd.histStartEpoch);
  TEST_ASSERT_TRUE(wd.hist[23].valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(10), wd.hist[23].tempF);
  TEST_ASSERT_TRUE(wd.hist[22].valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(20), wd.hist[22].tempF);
  TEST_ASSERT_TRUE(wd.hist[0].valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(30), wd.hist[0].tempF);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f / 25.4f, wd.hist[23].rainIn);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 4.0f / 25.4f, wd.hist[0].rainIn);
  TEST_ASSERT_EQUAL_UINT16(1, wd.hist[23].strikes);
  TEST_ASSERT_EQUAL_UINT16(2, wd.hist[22].strikes);
  int valid = 0; for (auto& b : wd.hist) valid += b.valid;
  TEST_ASSERT_EQUAL(3, valid);
  TEST_ASSERT_FALSE(wd.today.valid);   // no day window requested
  TEST_ASSERT_TRUE(wd.have & HAVE_HISTORY);
}
void test_bucket_averages_sums_gaps() {
  weatherDataInit(wd);
  int64_t b21 = BASE - 2 * 3600;   // bucket 21
  std::string r = row(b21 + 60, 2, 5, 1000, 10, 40, 0.5f, 1) + "," +
                  row(b21 + 120, 4, 9, 1010, 20, 60, 0.25f, 2) + "," +
                  row(b21 + 180, 6, 3, 1020, 30, 80, 0.0f, 0);
  std::string j = doc(r);
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), noDay(), wd));
  const HistBucket& b = wd.hist[21];
  TEST_ASSERT_TRUE(b.valid);
  TEST_ASSERT_EQUAL_UINT8(3, b.samples);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(20), b.tempF);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, msToMph(4), b.windMph);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, msToMph(9), b.gustMph);         // max, not avg
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, b.humidityPct);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, mbToInHg(1010), b.pressureInHg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f / 25.4f, b.rainIn);
  TEST_ASSERT_EQUAL_UINT16(3, b.strikes);
  // neighbours are gaps
  TEST_ASSERT_FALSE(wd.hist[20].valid);
  TEST_ASSERT_TRUE(isnan(wd.hist[20].tempF));
  TEST_ASSERT_TRUE(isnan(wd.hist[20].gustMph));
  TEST_ASSERT_EQUAL_UINT16(0, wd.hist[20].strikes);
  TEST_ASSERT_FALSE(wd.hist[22].valid);
}
void test_pressure_offset() {
  weatherDataInit(wd);
  std::string j = doc(row(BASE + 5, 1, 1, 969.1f, 10, 50, 0, 0));
  HistoryWindow w = noDay(); w.pressureOffsetMb = 40.0f;
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), w, wd));
  TEST_ASSERT_FLOAT_WITHIN(0.002f, mbToInHg(1009.1f), wd.hist[23].pressureInHg);
}
void test_day_summary() {
  weatherDataInit(wd);
  int64_t d0 = BASE - 10 * 3600;   // local midnight for this synthetic day
  HistoryWindow w{SNOW, d0, d0 + 86400, 0.0f};
  // out of order on purpose; tie on the high (30 C at two times) must report the earlier one
  std::string r = row(d0 + 7200, 3, 8.0f, 1000, 30, 50, 1.0f, 2) + "," +
                  row(d0 + 600, 1, 2.0f, 1000, 12, 50, 0.5f, 0) + "," +
                  row(d0 + 9000, 3, 8.0f, 1000, 30, 55, 0.0f, 1) + "," +      // tie high and tie gust
                  row(d0 - 60, 9, 20.0f, 1000, 50, 50, 9.0f, 9) + "," +       // yesterday: excluded
                  row(d0 + 20000, 2, 4.0f, 1000, 18, 71, 0.25f, 0);           // newest
  std::string j = doc(r);
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), w, wd));
  const DaySummary& s = wd.today;
  TEST_ASSERT_TRUE(s.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(30), s.highF);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(d0 + 7200), s.highTime);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(12), s.lowF);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(d0 + 600), s.lowTime);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.75f / 25.4f, s.rainIn);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, msToMph(8.0f), s.peakGustMph);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(d0 + 7200), s.peakGustTime);
  TEST_ASSERT_EQUAL_UINT16(3, s.strikes);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(d0 + 20000), s.lastTime);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(18), s.lastTempF);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, msToMph(2), s.lastWindMph);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 71.0f, s.lastHumidityPct);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.6f, wd.cur.stationBatteryV);
}
void test_day_summary_empty_window() {
  weatherDataInit(wd);
  HistoryWindow w{SNOW, BASE - 100 * 3600, BASE - 76 * 3600, 0.0f};
  std::string j = doc(row(BASE, 1, 1, 1000, 10, 50, 0, 0));
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), w, wd));
  TEST_ASSERT_FALSE(wd.today.valid);
  TEST_ASSERT_TRUE(isnan(wd.today.highF));
}
void test_history_nulls_and_short_rows() {
  weatherDataInit(wd);
  // temp null, gust null, a short row (skipped), a row with null epoch (skipped)
  std::string r = "[" + std::to_string(BASE + 10) + ",0,1.0,null,150,3,990,null,50,1,0,0,0.5,0,0,0,2.5,1,0,0,0,0],"
                  "[" + std::to_string(BASE + 20) + ",0,1.0],"
                  "[null,0,1.0,2.0,150,3,990,10,50,1,0,0,0.5,0,0,0,2.5,1,0,0,0,0]";
  std::string j = doc(r);
  HistoryWindow w{SNOW, BASE, BASE + 3600, 0.0f};
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), w, wd));
  TEST_ASSERT_TRUE(wd.hist[23].valid);
  TEST_ASSERT_EQUAL_UINT8(1, wd.hist[23].samples);
  TEST_ASSERT_TRUE(isnan(wd.hist[23].tempF));
  TEST_ASSERT_TRUE(isnan(wd.hist[23].gustMph));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, msToMph(1.0f), wd.hist[23].windMph);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f / 25.4f, wd.hist[23].rainIn);
  TEST_ASSERT_TRUE(wd.today.valid);
  TEST_ASSERT_TRUE(isnan(wd.today.highF));
  TEST_ASSERT_EQUAL_UINT32(0, wd.today.highTime);
  TEST_ASSERT_TRUE(isnan(wd.today.peakGustMph));
}
void test_history_empty_and_null_obs() {
  weatherDataInit(wd);
  std::string j = doc("");
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), noDay(), wd));
  for (auto& b : wd.hist) TEST_ASSERT_FALSE(b.valid);
  const char* n = "{\"status\":{\"status_code\":0},\"obs\":null}";
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseDeviceHistory(n, strlen(n), noDay(), wd));
  const char* m = "{\"status\":{\"status_code\":0}}";
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parseDeviceHistory(m, strlen(m), noDay(), wd));
}
void test_history_pretty_printed() {
  weatherDataInit(wd);
  std::string j = "{\n \"obs\" :\n [\n  " + row(BASE + 1, 1, 2, 1000, 10, 50, 0, 0) + " ,\n  " + row(BASE + 2, 1, 2, 1000, 20, 50, 0, 0) + "\n ]\n}";
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), noDay(), wd));
  TEST_ASSERT_EQUAL_UINT8(2, wd.hist[23].samples);
}

// ------------------------------------------------------------ history: real fixture
static void checkFixtureHistory(const WeatherData& d) {
  TEST_ASSERT_EQUAL_UINT32(1795312800u - 23 * 3600, d.histStartEpoch);
  for (int i = 0; i < HIST_BUCKETS; i++) TEST_ASSERT_TRUE_MESSAGE(d.hist[i].valid, "bucket should be valid");
  TEST_ASSERT_EQUAL_UINT8(60, d.hist[0].samples);
  TEST_ASSERT_EQUAL_UINT8(8, d.hist[23].samples);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, cToF(16.73f), d.hist[0].tempF);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, msToMph(4.6f), d.hist[0].gustMph);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, mbToInHg(973.798f), d.hist[0].pressureInHg);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.510768f / 25.4f, d.hist[22].rainIn);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, cToF(17.0625f), d.hist[23].tempF);
  const DaySummary& s = d.today;
  TEST_ASSERT_TRUE(s.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(20.2f), s.highF);
  TEST_ASSERT_EQUAL_UINT32(1795285560u, s.highTime);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(12.2f), s.lowF);
  TEST_ASSERT_EQUAL_UINT32(1795267800u, s.lowTime);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 1.288441f / 25.4f, s.rainIn);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, msToMph(7.7f), s.peakGustMph);
  TEST_ASSERT_EQUAL_UINT32(1795283820u, s.peakGustTime);
  TEST_ASSERT_EQUAL_UINT16(0, s.strikes);
  TEST_ASSERT_EQUAL_UINT32(1795313220u, s.lastTime);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cToF(17.0f), s.lastTempF);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 83.0f, s.lastHumidityPct);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.63f, d.cur.stationBatteryV);
}
void test_history_fixture_from_memory() {
  weatherDataInit(wd);
  std::string j = load("device_history.json");
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(j.c_str(), j.size(), fixtureWindow(), wd));
  checkFixtureHistory(wd);
}
void test_history_fixture_from_istream() {
  weatherDataInit(wd);
  std::ifstream f("test/fixtures/device_history.json", std::ios::binary);
  TEST_ASSERT_TRUE(f.good());
  IStreamReader r(f);
  TEST_ASSERT_EQUAL(PARSE_OK, parseDeviceHistory(r, fixtureWindow(), wd));
  checkFixtureHistory(wd);
}

// ------------------------------------------------------------ errors
void test_garbage_and_truncated() {
  const char* bad[] = {"", "not json at all", "{\"current_conditions\":{\"air_temperature\":6", "[1,2,3", "{{{{"};
  for (const char* b : bad) {
    weatherDataInit(wd);
    uint32_t dev = 0;
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_ERR_JSON, parseBetterForecast(b, strlen(b), wd), b);
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_ERR_JSON, parseStationObs(b, strlen(b), wd), b);
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_ERR_JSON, parseStats(b, strlen(b), 0, 0, wd), b);
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_ERR_JSON, parseStations(b, strlen(b), 0, dev), b);
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_ERR_JSON, parseDeviceHistory(b, strlen(b), noDay(), wd), b);
    TEST_ASSERT_FALSE(wd.valid);
  }
}
void test_truncated_fixtures() {
  const char* names[] = {"better_forecast.json", "station_obs.json", "stats.json", "stations.json", "device_history.json"};
  for (const char* n : names) {
    weatherDataInit(wd);
    std::string j = load(n);
    j.resize(j.size() / 2);
    uint32_t dev = 0;
    ParseStatus s = PARSE_OK;
    switch (n[0]) {
      case 'b': s = parseBetterForecast(j.c_str(), j.size(), wd); break;
      case 'd': s = parseDeviceHistory(j.c_str(), j.size(), fixtureWindow(), wd); break;
      case 's': if (n[2] == 'a' && n[3] == 't' && n[4] == 'i') s = parseStations(j.c_str(), j.size(), 0, dev);
                else if (n[5] == 'o') s = parseStationObs(j.c_str(), j.size(), wd);
                else s = parseStats(j.c_str(), j.size(), 0, 0, wd);
                break;
    }
    TEST_ASSERT_EQUAL_MESSAGE(PARSE_ERR_JSON, s, n);
  }
}
void test_api_error_and_structure() {
  weatherDataInit(wd);
  const char* e = "{\"status\":{\"status_code\":1,\"status_message\":\"UNAUTHORIZED\"}}";
  uint32_t dev = 0;
  TEST_ASSERT_EQUAL(PARSE_ERR_API, parseBetterForecast(e, strlen(e), wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_API, parseStationObs(e, strlen(e), wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_API, parseStats(e, strlen(e), 0, 0, wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_API, parseStations(e, strlen(e), 0, dev));
  const char* s = "{}";
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseBetterForecast(s, strlen(s), wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseStationObs(s, strlen(s), wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseStats(s, strlen(s), 0, 0, wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_STRUCTURE, parseStations(s, strlen(s), 0, dev));
  TEST_ASSERT_FALSE(wd.valid);
}
void test_history_truncated_mid_row_and_junk() {
  weatherDataInit(wd);
  std::string r = row(BASE + 1, 1, 2, 1000, 10, 50, 0, 0);
  std::string a = "{\"obs\":[" + r + ",[" + std::to_string(BASE + 2) + ",0,1";           // cut inside a row
  std::string b = "{\"obs\":[" + r + "," + r;                                               // cut after a row
  std::string c = "{\"obs\":[" + r + " x " + r + "]}";                                      // junk between rows
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parseDeviceHistory(a.c_str(), a.size(), noDay(), wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parseDeviceHistory(b.c_str(), b.size(), noDay(), wd));
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parseDeviceHistory(c.c_str(), c.size(), noDay(), wd));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_struct_size_fits_rtc);
  RUN_TEST(test_init_state);
  RUN_TEST(test_forecast_current);
  RUN_TEST(test_forecast_hourly_daily);
  RUN_TEST(test_icon_keys);
  RUN_TEST(test_forecast_streamed_from_istream);
  RUN_TEST(test_forecast_missing_fields);
  RUN_TEST(test_forecast_wrong_units_rejected);
  RUN_TEST(test_station_obs_fixture);
  RUN_TEST(test_obs_and_forecast_order_independent);
  RUN_TEST(test_obs_rain_rate_conversion);
  RUN_TEST(test_stats_fixture);
  RUN_TEST(test_stats_wrong_month_is_unknown);
  RUN_TEST(test_stations_fixture);
  RUN_TEST(test_bucket_boundaries);
  RUN_TEST(test_bucket_averages_sums_gaps);
  RUN_TEST(test_pressure_offset);
  RUN_TEST(test_day_summary);
  RUN_TEST(test_day_summary_empty_window);
  RUN_TEST(test_history_nulls_and_short_rows);
  RUN_TEST(test_history_empty_and_null_obs);
  RUN_TEST(test_history_pretty_printed);
  RUN_TEST(test_history_fixture_from_memory);
  RUN_TEST(test_history_fixture_from_istream);
  RUN_TEST(test_garbage_and_truncated);
  RUN_TEST(test_truncated_fixtures);
  RUN_TEST(test_api_error_and_structure);
  RUN_TEST(test_history_truncated_mid_row_and_junk);
  return UNITY_END();
}