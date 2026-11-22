// Native-only helper (tools and unit tests): load a fixture set into WeatherData.
// A set is a variant directory under test/fixtures (storm, raining, cold, hot, nohistory); any file the
// variant lacks falls back to the real capture in test/fixtures/. Header-only, never built for the ESP32.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <sstream>
#include "parse.h"
#include "timefmt.h"
#include "config.h"

#define FIXTURE_NOW        1795313256LL   // Sat Nov 21 2026, 8:07:36 PM CST: current_conditions.time of the captures
#define FIXTURE_DAY_START  1795240800LL   // local midnight of that day

inline void fixtureSetTimezone() {
#if defined(_WIN32)
  _putenv_s("TZ", TZ_POSIX);
  _tzset();
#else
  setenv("TZ", TZ_POSIX, 1);
  tzset();
#endif
}

inline bool fixtureRead(const char* variant, const char* file, std::string& out) {
  const char* root = "test/fixtures/";
  for (int pass = 0; pass < 2; pass++) {
    std::string path = root;
    if (pass == 0) { if (!variant || !*variant) continue; path += variant; path += "/"; }
    path += file;
    std::ifstream f(path, std::ios::binary);
    if (!f) continue;
    std::stringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
  }
  return false;
}

// Loads forecast + station obs + history. `history` = false skips the history parse entirely
// (HAVE_HISTORY stays unset, like a failed device-history request). Returns false if a required file is missing.
inline bool loadFixtureSet(const char* variant, WeatherData& d, bool history = true) {
  std::string j;
  weatherDataInit(d);
  if (!fixtureRead(variant, "better_forecast.json", j) || parseBetterForecast(j.c_str(), j.size(), d) != PARSE_OK) return false;
  if (!fixtureRead(variant, "station_obs.json", j) || parseStationObs(j.c_str(), j.size(), d) != PARSE_OK) return false;
  if (history) {
    HistoryWindow w{FIXTURE_NOW, FIXTURE_DAY_START, FIXTURE_DAY_START + 86400, 0.0f};
    if (!fixtureRead(variant, "device_history.json", j) || parseDeviceHistory(j.c_str(), j.size(), w, d) != PARSE_OK) return false;
  }
  d.fetchedEpoch = (uint32_t)FIXTURE_NOW;
  return true;
}
