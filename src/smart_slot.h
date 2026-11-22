// Portable: pick smart slot content from WeatherData (task 5).
#pragma once
#include "weather_data.h"
enum class SlotKind { Lightning, Raining, UV, NextRain };
struct SmartSlot {
  SlotKind kind = SlotKind::NextRain;
  // TODO(task 5): label, value, sub text
};
SmartSlot pickSmartSlot(const WeatherData& d);
