#include "helpers.h"
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int batteryPercent(float v) {
  if (isnan(v) || v <= 3.20f) return 0;
  if (v >= 4.20f) return 100;
  double p = 2836.9625 * pow(v, 4) - 43987.4889 * pow(v, 3) + 255233.8134 * pow(v, 2) - 656689.7123 * v + 632041.7303;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)p;
}

void stationBatteryText(float v, char* buf, size_t n) {
  if (isnan(v)) snprintf(buf, n, "--");
  else if (v >= STATION_BATT_OK_V) snprintf(buf, n, "OK");
  else snprintf(buf, n, "LOW %.2f V", (double)v);
}

int tempestUrlForecast(char* b, size_t n, const char* t, uint32_t st) {
  return snprintf(b, n,
                  TEMPEST_BASE_URL "/better_forecast?station_id=%lu&units_temp=f&units_wind=mph&units_pressure=inhg"
                                   "&units_precip=in&units_distance=mi&token=%s",
                  (unsigned long)st, t);
}
int tempestUrlStationObs(char* b, size_t n, const char* t, uint32_t st) {
  return snprintf(b, n, TEMPEST_BASE_URL "/observations/station/%lu?token=%s", (unsigned long)st, t);
}
int tempestUrlStations(char* b, size_t n, const char* t) {
  return snprintf(b, n, TEMPEST_BASE_URL "/stations?token=%s", t);
}
int tempestUrlDeviceHistory(char* b, size_t n, const char* t, uint32_t dev, int64_t s, int64_t e) {
  return snprintf(b, n, TEMPEST_BASE_URL "/observations/device/%lu?time_start=%lld&time_end=%lld&token=%s",
                  (unsigned long)dev, (long long)s, (long long)e, t);
}
int tempestUrlStats(char* b, size_t n, const char* t, uint32_t st) {
  return snprintf(b, n, TEMPEST_BASE_URL "/stats/station/%lu?token=%s", (unsigned long)st, t);
}

void redactUrl(const char* url, char* out, size_t n) {
  if (!n) return;
  const char* p = strstr(url, "token=");
  if (!p) { snprintf(out, n, "%s", url); return; }
  p += 6;
  const char* end = strchr(p, '&');
  snprintf(out, n, "%.*sREDACTED%s", (int)(p - url), url, end ? end : "");
}

HistoryPlan planHistory(int64_t now, TitlesMode titles, float offsetMb) {
  HistoryPlan h;
  const bool yest = titles == TitlesMode::YesterdayToday;
  int64_t today0 = localMidnight(now, 0);
  h.timeEnd = now;
  h.timeStart = now - 24 * 3600 - 300;           // covers all 24 hourly buckets
  if (yest) {
    int64_t y0 = localMidnight(now, -1);
    if (y0 < h.timeStart) h.timeStart = y0;
    h.window.dayStart = y0;
    h.window.dayEnd = today0;
  } else {
    h.window.dayStart = today0;
    h.window.dayEnd = localMidnight(now, 1);
  }
  h.window.now = now;
  h.window.pressureOffsetMb = isnan(offsetMb) ? 0.0f : offsetMb;
  return h;
}

bool statsNeedFetch(const StatsCache& c, int64_t now) { return !c.valid || c.dayKey != localDayKey(now); }

void statsStore(StatsCache& c, const WeatherData& d, int64_t now) {
  if (!d.rainMonthYearKnown) return;
  c.valid = true;
  c.dayKey = localDayKey(now);
  c.monthIn = d.rainMonthIn;
  c.yearIn = d.rainYearIn;
}

bool statsApply(const StatsCache& c, WeatherData& d) {
  if (!c.valid) return false;
  d.rainMonthIn = c.monthIn;
  d.rainYearIn = c.yearIn;
  d.rainMonthYearKnown = true;
  d.have |= HAVE_STATS;
  return true;
}

void describeFetchResult(const FetchResult& r, char* buf, size_t n) {
  switch (r.status) {
    case FETCH_OK: snprintf(buf, n, "OK"); break;
    case FETCH_ERR_CONFIG: snprintf(buf, n, "Settings missing (station id or token)"); break;
    case FETCH_ERR_WIFI: snprintf(buf, n, "WiFi failed"); break;
    case FETCH_ERR_HTTP:
      if (r.httpCode > 0) snprintf(buf, n, "Tempest API error: HTTP %d (%s)", r.httpCode, r.what);
      else snprintf(buf, n, "Tempest unreachable: error %d (%s)", r.httpCode, r.what);
      break;
    case FETCH_ERR_PARSE:
      snprintf(buf, n, "Tempest data error: %s (%s)",
               r.parse == PARSE_ERR_API ? "API status" : r.parse == PARSE_ERR_STRUCTURE ? "unexpected reply" : "bad JSON",
               r.what);
      break;
  }
}

bool fetchShouldRetry(const FetchResult& r) {
  switch (r.status) {
    case FETCH_OK: case FETCH_ERR_CONFIG: return false;
    case FETCH_ERR_WIFI: return true;
    case FETCH_ERR_HTTP: return r.httpCode <= 0 || r.httpCode >= 500 || r.httpCode == 408 || r.httpCode == 429;
    case FETCH_ERR_PARSE: return r.parse == PARSE_ERR_JSON;   // truncated body; API/structure errors repeat
  }
  return false;
}
