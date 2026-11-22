// Native-only: the list of screens rendered to out/*.png and checked by test/test_screens.
// Header-only; shared by tools/native/main_native.cpp and the unit tests so both see the same scenes.
#pragma once
#include <string.h>
#include "screens.h"
#include "fixture_loader.h"

struct SceneDef { const char* name; };   // PNG is out/<name>.png
static const SceneDef kScenes[] = {
  {"live_real"}, {"live_storm"}, {"live_raining"}, {"live_cold"}, {"live_hot"}, {"live_offline"},
  {"live_nohistory"}, {"live_rain6h"}, {"overnight_today"}, {"overnight_yesterday"}, {"overnight_cold"}, {"overnight_hot"},
  {"charge_me"}, {"error"}};
static const int kSceneCount = (int)(sizeof kScenes / sizeof kScenes[0]);

// Status as main.cpp would fill it at the fixtures' fetch time.
inline ScreenStatus sceneStatus(int64_t now) {
  ScreenStatus st;
  st.nowEpoch = now;
  st.lastGoodEpoch = FIXTURE_NOW;
  st.batteryPercent = 80;
  st.batteryVolts = 3.95f;
  st.offline = false;
  return st;
}

// What the 1 AM reset would see: the daily forecast list starts at the new day.
inline void sceneAdvanceDay(WeatherData& d) {
  if (d.dailyCount > 0) {
    for (int i = 0; i + 1 < d.dailyCount; i++) d.daily[i] = d.daily[i + 1];
    d.dailyCount--;
  }
}

inline bool renderScene(const char* name, Gfx& g) {
  fixtureSetTimezone();
  static WeatherData d;
  ScreenStatus st = sceneStatus(FIXTURE_NOW);
  const int64_t NIGHT = FIXTURE_DAY_START + 22 * 3600;        // 10:00 PM, Sat Nov 21
  const int64_t AFTER_MIDNIGHT = FIXTURE_DAY_START + 86400 + 3600;   // 1:00 AM, Sun Nov 22
  if (!strcmp(name, "live_real"))     { if (!loadFixtureSet(nullptr, d)) return false; drawLive(g, d, st); }
  else if (!strcmp(name, "live_storm"))   { if (!loadFixtureSet("storm", d)) return false; drawLive(g, d, st); }
  else if (!strcmp(name, "live_raining")) { if (!loadFixtureSet("raining", d)) return false; drawLive(g, d, st); }
  else if (!strcmp(name, "live_cold"))    { if (!loadFixtureSet("cold", d)) return false; drawLive(g, d, st); }
  else if (!strcmp(name, "live_hot"))     { if (!loadFixtureSet("hot", d)) return false; drawLive(g, d, st); }
  else if (!strcmp(name, "live_offline")) {
    if (!loadFixtureSet(nullptr, d)) return false;
    st.nowEpoch = FIXTURE_NOW + 3 * 1800; st.offline = true; drawLive(g, d, st);
  }
  else if (!strcmp(name, "live_nohistory")) { if (!loadFixtureSet("nohistory", d)) return false; drawLive(g, d, st); }
  else if (!strcmp(name, "live_rain6h")) {       // dry now, but it rained during the last six hours (rain note + bars)
    if (!loadFixtureSet(nullptr, d)) return false;
    static const float mm[] = {0.02f, 0.06f, 0.08f, 0.03f, 0.02f};
    for (int i = 0; i < 5; i++) { d.hist[19 + i].valid = true; d.hist[19 + i].rainIn = mm[i]; }
    drawLive(g, d, st);
  }
  else if (!strcmp(name, "overnight_today")) {
    if (!loadFixtureSet(nullptr, d)) return false;
    st.nowEpoch = NIGHT; drawOvernight(g, d, st, TITLES_TODAY_TOMORROW);
  }
  else if (!strcmp(name, "overnight_yesterday")) {
    if (!loadFixtureSet(nullptr, d)) return false;
    sceneAdvanceDay(d);
    st.nowEpoch = AFTER_MIDNIGHT; st.lastGoodEpoch = AFTER_MIDNIGHT;
    drawOvernight(g, d, st, TITLES_YESTERDAY_TODAY);
  }
  else if (!strcmp(name, "overnight_cold")) { if (!loadFixtureSet("cold", d)) return false; st.nowEpoch = NIGHT; drawOvernight(g, d, st, TITLES_TODAY_TOMORROW); }
  else if (!strcmp(name, "overnight_hot"))  { if (!loadFixtureSet("hot", d)) return false; st.nowEpoch = NIGHT; drawOvernight(g, d, st, TITLES_TODAY_TOMORROW); }
  else if (!strcmp(name, "charge_me")) {
    st.batteryVolts = 3.28f; st.batteryPercent = 0; st.lastGoodEpoch = FIXTURE_DAY_START + 15 * 3600 + 1800;
    drawChargeMe(g, st);
  }
  else if (!strcmp(name, "error")) { drawError(g, "Tempest API error (HTTP 401)", st); }
  else return false;
  return true;
}
