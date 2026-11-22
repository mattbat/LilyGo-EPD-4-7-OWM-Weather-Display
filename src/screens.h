// Portable screens (task 5). This header is the fixed contract between the
// screens (task 5) and main.cpp (task 6). Do not change signatures without
// updating both sides.
#pragma once
#include <stdint.h>
#include "gfx.h"
#include "weather_data.h"

// Device status shown in headers/footers, filled in by main.cpp.
struct ScreenStatus {
  int64_t nowEpoch = 0;        // current time (UTC epoch); local time via localtime() with TZ set
  int     batteryPercent = -1; // device battery 0-100, -1 = unknown (hide icon)
  float   batteryVolts = 0;    // device battery volts
  bool    offline = false;     // true = last fetch failed, d holds the last good data
  int64_t lastGoodEpoch = 0;   // when d was fetched (shown as "Updated" / "last data")
};

enum OvernightTitles { TITLES_TODAY_TOMORROW, TITLES_YESTERDAY_TODAY };

void drawLive(Gfx& g, const WeatherData& d, const ScreenStatus& st);
void drawOvernight(Gfx& g, const WeatherData& d, const ScreenStatus& st, OvernightTitles titles);
void drawChargeMe(Gfx& g, const ScreenStatus& st);
void drawError(Gfx& g, const char* reason, const ScreenStatus& st);
