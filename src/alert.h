// Portable: pick the alert that replaces the chart area on the live screen (the full-width black band).
// Priority, first match wins: lightning in the last 3 h > raining now > UV >= 6 > none.
#pragma once
#include <stdint.h>
#include "weather_data.h"

enum class AlertKind { None, Lightning, Raining, UV };

struct Alert {
  AlertKind kind = AlertKind::None;
  char headline[40] = "";   // "28 lightning strikes", "Raining 0.24 in/hr", "UV 8 \xC2\xB7 very high"
  char sub[56] = "";        // second line, may be empty
};

#define ALERT_UV_MIN  6.0f

Alert pickAlert(const WeatherData& d);
