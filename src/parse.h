// Portable JSON -> WeatherData (task 3).
#pragma once
#include "weather_data.h"
#include <stddef.h>
bool parseBetterForecast(const char* json, size_t len, WeatherData& out);
bool parseStationObs(const char* json, size_t len, WeatherData& out);
bool parseDeviceHistory(const char* json, size_t len, WeatherData& out);
