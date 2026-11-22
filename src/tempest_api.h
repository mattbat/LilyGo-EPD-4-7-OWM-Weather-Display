// ESP32 only: HTTPS calls to the Tempest REST API (task 6).
#pragma once
#include "weather_data.h"
bool fetchAll(WeatherData& out);
