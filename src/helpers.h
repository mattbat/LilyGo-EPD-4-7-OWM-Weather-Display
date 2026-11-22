// Portable helpers used by the ESP32 modules (kept here so they can be unit tested on the PC).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "parse.h"
#include "schedule.h"
#include "weather_data.h"

// ---- Battery ----
int batteryPercent(float volts);   // LiPo curve from the old OWM sketch, clamped 0..100 (NAN -> 0)
// "OK" at or above STATION_BATT_OK_V, otherwise "LOW 2.31 V"; "--" if unknown.
void stationBatteryText(float volts, char* buf, size_t n);

// ---- Tempest URLs (all return the snprintf length; >= n means truncated) ----
#define TEMPEST_BASE_URL "https://swd.weatherflow.com/swd/rest"
int tempestUrlForecast(char* buf, size_t n, const char* token, uint32_t stationId);
int tempestUrlStationObs(char* buf, size_t n, const char* token, uint32_t stationId);
int tempestUrlStations(char* buf, size_t n, const char* token);
int tempestUrlDeviceHistory(char* buf, size_t n, const char* token, uint32_t deviceId, int64_t timeStart,
                            int64_t timeEnd);
int tempestUrlStats(char* buf, size_t n, const char* token, uint32_t stationId);
// Copy of url with the token value replaced by "REDACTED" (safe to log).
void redactUrl(const char* url, char* out, size_t n);

// ---- History window ----
struct HistoryPlan {
  int64_t timeStart, timeEnd;   // request range (epoch)
  HistoryWindow window;         // for parseDeviceHistory
};
// Live and TodayTomorrow: summary covers local today. YesterdayToday: summary covers local
// yesterday and the request reaches back to yesterday's midnight (~30 h at most, 24 h min).
// pressureOffsetMb may be NAN (treated as 0).
HistoryPlan planHistory(int64_t now, TitlesMode titles, float pressureOffsetMb);

// ---- Stats (rain this month / year), fetched at most once per local day ----
struct StatsCache {
  bool valid;
  int32_t dayKey;        // local yyyymmdd of the successful fetch
  float monthIn, yearIn;
};
bool statsNeedFetch(const StatsCache& c, int64_t now);
void statsStore(StatsCache& c, const WeatherData& d, int64_t now);   // after a successful parse
bool statsApply(const StatsCache& c, WeatherData& d);                // fallback to cached values

// ---- Fetch results (shared by tempest_api.cpp and the error screen text) ----
enum FetchStatus {
  FETCH_OK = 0,
  FETCH_ERR_CONFIG,   // station id / token missing
  FETCH_ERR_WIFI,     // could not join WiFi
  FETCH_ERR_HTTP,     // connection failure (httpCode < 0) or non-200 response
  FETCH_ERR_PARSE     // 200 but the body was bad (see parse)
};
struct FetchResult {
  FetchStatus status;
  int httpCode;        // HTTP status, or a negative HTTPClient error
  ParseStatus parse;   // for FETCH_ERR_PARSE
  char what[12];       // which call failed: "forecast", "obs", "history", ...
};
// Short text for the error screen, e.g. "Tempest API error: HTTP 401 (forecast)".
void describeFetchResult(const FetchResult& r, char* buf, size_t n);
// One retry is worth it for network trouble and 5xx; not for 4xx or a well-formed API error.
bool fetchShouldRetry(const FetchResult& r);
