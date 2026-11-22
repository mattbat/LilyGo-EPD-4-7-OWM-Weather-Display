// Portable plain structs shared by parser, screens and tests. Filled in by task 3.
#pragma once
#include <stdint.h>

struct WeatherData {
  bool valid = false;
  int64_t fetchedEpoch = 0;
  // TODO(task 3): current, 24 hourly history buckets, hourly forecast, daily, day summary
};
