// Portable screens (task 5).
#pragma once
#include "gfx.h"
#include "weather_data.h"
void drawLive(Gfx& g, const WeatherData& d, bool offline, int64_t lastGoodEpoch);
void drawOvernight(Gfx& g, const WeatherData& d, bool yesterdayToday);
void drawChargeMe(Gfx& g, float volts, int64_t lastDataEpoch);
void drawError(Gfx& g, const char* reason, int64_t nowEpoch);
