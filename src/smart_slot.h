// Portable: pick the smart slot content (the black box on the live screen) from WeatherData.
// Priority, first match wins: lightning in the last 3 h > raining now > UV >= 6 > next rain
// (first hourly forecast >= 40 % within 72 h) > "No rain in 3 days".
#pragma once
#include <stdint.h>
#include "weather_data.h"

enum class SlotKind { Lightning, Raining, UV, NextRain, NoRain };
enum class SlotIcon { None, Bolt, Rain, Sun };

struct SmartSlot {
  SlotKind kind = SlotKind::NoRain;
  SlotIcon icon = SlotIcon::None;
  char label[16] = "";   // small caps title ("LIGHTNING")
  char big[12] = "";     // the big text ("28", "Sun", "0.41")
  char sub1[24] = "";    // bold line
  char sub2[24] = "";    // grey line (may be empty)
};

#define SLOT_NEXT_RAIN_PCT    40          // minimum precip probability that counts as rain
#define SLOT_NEXT_RAIN_HOURS  72          // look-ahead
#define SLOT_UV_MIN           6.0f

// `now` is the current UTC epoch; local day names use the C library's localtime() (TZ must be set).
SmartSlot pickSmartSlot(const WeatherData& d, int64_t now);
