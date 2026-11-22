// ESP32 only: WiFi, NTP and HTTPS calls to the Tempest REST API. Bodies are streamed straight
// into the parse.h parsers (no big String buffers).
#pragma once
#include "helpers.h"
#include "weather_data.h"

bool netConnect(uint32_t timeoutMs);   // WiFi station join
bool netSyncTime(uint32_t timeoutMs);  // NTP with TZ_POSIX; true once the clock is set by NTP
void netOff();                         // WiFi fully off (call before display refresh and sleep)
bool systemTimeValid();                // clock looks set (year >= 2025)

struct FetchContext {
  int64_t now;            // UTC epoch
  TitlesMode titles;      // YesterdayToday widens the history request
};
// Fetches forecast, station obs and history into `out`. All three must succeed. Each call retries once.
FetchResult fetchAll(WeatherData& out, FetchContext& ctx);
