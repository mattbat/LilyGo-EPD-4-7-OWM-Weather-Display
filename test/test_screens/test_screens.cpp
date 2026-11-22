// Screen tests: every screen renders from every fixture set without crashing or touching memory outside
// the framebuffer, key regions look right, no "nan" reaches the text layer, and a framebuffer checksum
// per scene is compared with a recorded golden value.
//
// UPDATING THE GOLDEN VALUES (after an intentional design change):
//   1. Run   pio test -e native -f test_screens   once; test_goldens fails and prints the new table.
//      (or set SCREENS_PRINT_GOLDEN=1 to print it without failing:  SCREENS_PRINT_GOLDEN=1 pio test -e native -f test_screens -v)
//   2. Look at the new PNGs (pio run -e native && ./.pio/build/native/program.exe -> out/*.png).
//   3. Paste the printed { "name", checksum, darkPixels } lines over kGolden below.
// The checksum is FNV-1a over the 4-bit framebuffer bytes. If it differs but the count of dark pixels (grey <= 7)
// is within 0.3 %, the test passes with a message: libm differences between platforms can move a few edge pixels.
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "screens.h"
#include "screens_debug.h"
#include "palette.h"
#include "fonts/fonts.h"
#include "../../tools/native/scenes.h"

#define FB_BYTES (PANEL_WIDTH * PANEL_HEIGHT / 2)
#define GUARD 4096
static uint8_t arena[GUARD + FB_BYTES + GUARD];
static Gfx g;
static WeatherData d;

void setUp() {
  fixtureSetTimezone();
  memset(arena, 0xA5, sizeof arena);
  g.begin(arena + GUARD);
  g.clear(15);
  screenTextAnomaliesReset();
  gfxMinTextAdvanceReset();
}
void tearDown() {}

static bool guardsIntact() {
  for (int i = 0; i < GUARD; i++) if (arena[i] != 0xA5 || arena[GUARD + FB_BYTES + i] != 0xA5) return false;
  return true;
}
static uint32_t checksum() {
  uint32_t h = 2166136261u;
  for (int i = 0; i < FB_BYTES; i++) { h ^= arena[GUARD + i]; h *= 16777619u; }
  return h;
}
static int darkPixels() {
  int n = 0;
  for (int y = 0; y < PANEL_HEIGHT; y++) for (int x = 0; x < PANEL_WIDTH; x++) if (g.get(x, y) <= 7) n++;
  return n;
}
// Fraction (0..1) of pixels in the inclusive box that are <= maxGrey.
static float darkFraction(int x0, int y0, int x1, int y1, int maxGrey = 4) {
  int n = 0, tot = 0;
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) { tot++; if (g.get(x, y) <= maxGrey) n++; }
  return (float)n / (float)tot;
}
static int countNot(int x0, int y0, int x1, int y1, uint8_t bg) {
  int n = 0;
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (g.get(x, y) != bg) n++;
  return n;
}
static ScreenStatus status() { return sceneStatus(FIXTURE_NOW); }

// ------------------------------------------------------------------------------------------ scenes
void test_every_scene_renders_inside_the_panel() {
  for (int i = 0; i < kSceneCount; i++) {
    setUp();
    TEST_ASSERT_TRUE_MESSAGE(renderScene(kScenes[i].name, g), kScenes[i].name);
    TEST_ASSERT_TRUE_MESSAGE(guardsIntact(), kScenes[i].name);
    TEST_ASSERT_EQUAL_MESSAGE(0, screenTextAnomalies(), kScenes[i].name);
    TEST_ASSERT_TRUE_MESSAGE(darkPixels() > 1500, kScenes[i].name);      // something was drawn
  }
}
void test_rendering_is_deterministic() {
  for (int i = 0; i < kSceneCount; i++) {
    setUp(); renderScene(kScenes[i].name, g); uint32_t a = checksum();
    setUp(); renderScene(kScenes[i].name, g); uint32_t b = checksum();
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(a, b, kScenes[i].name);
  }
}

void test_no_text_smaller_than_16px() {
  // advance_y (line height) of the smallest permitted font; the 13-15 px fonts all have a smaller one
  const int minAdvance = AtkR16.advance_y;
  for (int i = 0; i < kSceneCount; i++) {
    setUp();
    renderScene(kScenes[i].name, g);
    TEST_ASSERT_TRUE_MESSAGE(gfxMinTextAdvance() >= minAdvance, kScenes[i].name);
  }
  setUp(); weatherDataInit(d); drawLive(g, d, status());
  TEST_ASSERT_TRUE(gfxMinTextAdvance() >= minAdvance);
  setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_TRUE(gfxMinTextAdvance() >= minAdvance);
}

// ------------------------------------------------------------------------------------------ goldens
struct Golden { const char* name; uint32_t sum; int dark; };
static const Golden kGolden[] = {
  {"live_real", 0x60D0C460u, 59937},
  {"live_storm", 0x931B345Eu, 60379},
  {"live_raining", 0x84437278u, 60598},
  {"live_cold", 0xB768E925u, 58900},
  {"live_hot", 0x704D7E43u, 62201},
  {"live_offline", 0x51FFD5E0u, 65358},
  {"live_nohistory", 0x07F9B4E7u, 56811},
  {"overnight_today", 0xCAFD9A0Cu, 75402},
  {"overnight_yesterday", 0xB73480E3u, 75505},
  {"overnight_cold", 0xEFF21CF3u, 74337},
  {"overnight_hot", 0x5316B3CAu, 75841},
  {"charge_me", 0xA8418E19u, 24341},
  {"error", 0xE0D09BBAu, 14245},
};

void test_goldens() {
  bool printOnly = getenv("SCREENS_PRINT_GOLDEN") != nullptr;
  char table[4096]; table[0] = 0;
  int bad = 0;
  for (int i = 0; i < kSceneCount; i++) {
    setUp();
    renderScene(kScenes[i].name, g);
    uint32_t sum = checksum();
    int dark = darkPixels();
    char line[96];
    snprintf(line, sizeof line, "  {\"%s\", 0x%08Xu, %d},\n", kScenes[i].name, (unsigned)sum, dark);
    strncat(table, line, sizeof table - strlen(table) - 1);
    const Golden* gd = nullptr;
    for (const Golden& x : kGolden) if (!strcmp(x.name, kScenes[i].name)) gd = &x;
    if (!gd) { bad++; continue; }
    if (sum == gd->sum) continue;
    int tol = gd->dark * 3 / 1000;
    if (abs(dark - gd->dark) <= tol && gd->dark > 0) {
      char msg[128]; snprintf(msg, sizeof msg, "%s: checksum differs but dark pixels within 0.3%% (%d vs %d)", gd->name, dark, gd->dark);
      TEST_MESSAGE(msg);
    } else bad++;
  }
  if (printOnly) { printf("GOLDEN TABLE:\n%s", table); return; }
  if (bad) {
    printf("GOLDEN TABLE (paste over kGolden):\n%s", table);
    TEST_FAIL_MESSAGE("screen checksums changed: see the table above and the header comment of test_screens.cpp");
  }
}

// ------------------------------------------------------------------------------------------ spot checks
void test_live_header_rule_and_slot_box() {
  renderScene("live_real", g);
  for (int x = 24; x < 936; x += 37) TEST_ASSERT_TRUE(g.get(x, 48) <= 4 || g.get(x, 47) <= 4);   // 2 px rule at y 47-48
  for (int x = 24; x < 936; x += 37) TEST_ASSERT_TRUE(g.get(x, 242) <= 4 || g.get(x, 241) <= 4);
  for (int x = 24; x < 936; x += 37) TEST_ASSERT_TRUE(g.get(x, 504) <= 4 || g.get(x, 503) <= 4);
  // the smart slot is a mostly black box at x 798-936, y 60-232 (text is white on top)
  TEST_ASSERT_TRUE(darkFraction(798, 60, 935, 231) > 0.75f);
  TEST_ASSERT_TRUE(darkFraction(800, 62, 806, 230) > 0.99f);
  // the next stat column stops before it
  TEST_ASSERT_EQUAL_INT(0, countNot(790, 56, 796, 236, COL_PAPER));
}
void test_slot_box_in_every_state() {
  const char* names[] = {"live_storm", "live_raining", "live_hot", "live_real", "live_cold"};
  for (const char* n : names) {
    setUp(); renderScene(n, g);
    TEST_ASSERT_TRUE_MESSAGE(darkFraction(798, 60, 935, 231) > 0.7f, n);
    TEST_ASSERT_TRUE_MESSAGE(guardsIntact(), n);
  }
}
void test_offline_chip_only_when_offline() {
  renderScene("live_real", g);
  TEST_ASSERT_TRUE(darkFraction(640, 10, 870, 40) < 0.25f);
  setUp(); renderScene("live_offline", g);
  TEST_ASSERT_TRUE(darkFraction(660, 10, 860, 40) > 0.55f);
  // the chip replaces "Updated": the right-hand header text is gone from the left of the chip
  TEST_ASSERT_EQUAL_INT(0, countNot(420, 10, 560, 40, COL_PAPER));
}
void test_overnight_header_bar_is_black() {
  renderScene("overnight_today", g);
  TEST_ASSERT_TRUE(darkFraction(0, 0, 959, 55) > 0.8f);
  TEST_ASSERT_TRUE(darkFraction(0, 0, 10, 55) > 0.99f);
  TEST_ASSERT_TRUE(g.get(480, 60) == COL_PAPER);   // below the bar: paper
  setUp(); renderScene("live_real", g);
  TEST_ASSERT_TRUE(darkFraction(0, 0, 959, 55) < 0.2f);
}
void test_charge_me_and_error_screens() {
  renderScene("charge_me", g);
  TEST_ASSERT_TRUE(darkFraction(340, 120, 600, 250) > 0.15f);   // battery outline + charge
  TEST_ASSERT_TRUE(darkFraction(356, 136, 377, 233) > 0.95f);
  TEST_ASSERT_EQUAL_INT(0, countNot(380, 140, 590, 230, COL_PAPER));   // empty inside
  setUp(); renderScene("error", g);
  TEST_ASSERT_TRUE(countNot(280, 280, 680, 330, COL_PAPER) > 500);
  TEST_ASSERT_TRUE(guardsIntact());
}

// ------------------------------------------------------------------------------------------ edge cases
void test_all_nan_and_empty_data_renders_cleanly() {
  weatherDataInit(d);                              // nothing parsed: every float NAN, no have bits
  ScreenStatus st = status();
  drawLive(g, d, st);
  TEST_ASSERT_TRUE(guardsIntact());
  TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  setUp(); drawOvernight(g, d, st, TITLES_TODAY_TOMORROW);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  setUp(); drawOvernight(g, d, st, TITLES_YESTERDAY_TODAY);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  setUp(); st.offline = true; st.batteryPercent = -1; drawLive(g, d, st);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
}
void test_zero_status_and_null_reason() {
  ScreenStatus st;                                  // all defaults: time 0, battery unknown
  weatherDataInit(d);
  drawLive(g, d, st);   TEST_ASSERT_TRUE(guardsIntact());
  setUp(); drawChargeMe(g, st); TEST_ASSERT_TRUE(guardsIntact());
  setUp(); drawError(g, nullptr, st); TEST_ASSERT_TRUE(guardsIntact());
  setUp(); drawError(g, "", st); TEST_ASSERT_TRUE(guardsIntact());
  TEST_ASSERT_EQUAL(0, screenTextAnomalies());
}
void test_every_field_nan_individually() {
  // knock out one measured value at a time on top of the real fixtures
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  float* fields[] = {&d.cur.tempF, &d.cur.feelsLikeF, &d.cur.windMph, &d.cur.gustMph, &d.cur.dewPointF, &d.cur.pressureInHg,
                     &d.cur.rainTodayIn, &d.cur.rainYesterdayIn, &d.cur.rainRateInHr, &d.cur.uv, &d.cur.lastStrikeMi,
                     &d.cur.stationBatteryV, &d.rainMonthIn, &d.rainYearIn, &d.today.highF, &d.today.lowF, &d.today.rainIn,
                     &d.today.peakGustMph, &d.today.lastTempF, &d.daily[1].highF, &d.daily[1].lowF, &d.hourly[3].tempF,
                     &d.hist[5].tempF, &d.hist[23].tempF, &d.hist[7].rainIn};
  ScreenStatus st = status();
  for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
    float keep = *fields[i];
    *fields[i] = NAN;
    setUp(); drawLive(g, d, st);
    TEST_ASSERT_TRUE_MESSAGE(guardsIntact(), "live");
    TEST_ASSERT_EQUAL_MESSAGE(0, screenTextAnomalies(), "live nan");
    setUp(); drawOvernight(g, d, st, TITLES_TODAY_TOMORROW);
    TEST_ASSERT_TRUE_MESSAGE(guardsIntact(), "overnight");
    TEST_ASSERT_EQUAL_MESSAGE(0, screenTextAnomalies(), "overnight nan");
    *fields[i] = keep;
  }
}
void test_missing_history_variants() {
  // history never parsed (failed request) and history parsed but empty
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d, false));
  TEST_ASSERT_FALSE(d.have & HAVE_HISTORY);
  ScreenStatus st = status();
  drawLive(g, d, st);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  uint32_t withoutHistory = checksum();
  setUp(); drawOvernight(g, d, st, TITLES_TODAY_TOMORROW);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  setUp(); TEST_ASSERT_TRUE(loadFixtureSet("nohistory", d));
  drawLive(g, d, st);
  TEST_ASSERT_TRUE(guardsIntact());
  TEST_ASSERT_NOT_EQUAL(0, withoutHistory);
  // the measured half says so instead of drawing a line
  TEST_ASSERT_TRUE(countNot(180, 300, 380, 340, COL_PAPER) > 100);
}
void test_nan_text_is_intercepted_by_the_hook() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  snprintf(d.cur.conditions, sizeof d.cur.conditions, "NaN");
  drawLive(g, d, status());
  TEST_ASSERT_EQUAL(1, screenTextAnomalies());
  TEST_ASSERT_EQUAL_STRING("NaN", screenLastAnomaly());      // refused and drawn as "--" instead
  setUp();
  snprintf(d.cur.conditions, sizeof d.cur.conditions, "Mainly Clear");
  drawLive(g, d, status());
  TEST_ASSERT_EQUAL(0, screenTextAnomalies());               // ordinary words are fine
}
void test_long_condition_text_is_truncated_not_overlapping() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  snprintf(d.cur.conditions, sizeof d.cur.conditions, "WWWWWWWWWWWWWWWWWWWWWWW");   // 23 wide letters
  drawLive(g, d, status());
  TEST_ASSERT_TRUE(guardsIntact());
  TEST_ASSERT_EQUAL_INT(0, countNot(474, 196, 485, 236, COL_PAPER));    // nothing between the text and the stat grid
  // and the feels-like part survived at the right end of the line (some ink near x 380-470)
  TEST_ASSERT_TRUE(countNot(380, 205, 470, 232, COL_PAPER) > 50);
}
void test_wide_temperatures_do_not_touch_neighbours() {
  const float temps[] = {-100, -12, -9, 0, 5, 99, 100, 123, 199, -40.6f};
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  for (float t : temps) {
    d.cur.tempF = t;
    setUp(); drawLive(g, d, status());
    TEST_ASSERT_TRUE(guardsIntact());
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countNot(480, 84, 485, 200, COL_PAPER), "big temp reaches the stat grid");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countNot(0, 60, 13, 200, COL_PAPER), "big temp left of the icon");
    TEST_ASSERT_TRUE(countNot(146, 84, 470, 190, COL_PAPER) > 400);        // something big is there
  }
  // overnight hi / lo
  d.daily[1].highF = -100; d.daily[1].lowF = -120;
  setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_TRUE(guardsIntact());
  TEST_ASSERT_EQUAL_INT(0, countNot(937, 110, 959, 200, COL_PAPER));
  d.daily[1].highF = 119; d.daily[1].lowF = 100;
  setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_EQUAL_INT(0, countNot(937, 110, 959, 200, COL_PAPER));
}
void test_stat_values_stay_in_their_columns() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  d.cur.windMph = 123; d.cur.gustMph = 188; d.cur.rainTodayIn = 12.34f; d.cur.pressureInHg = 29.99f;
  d.cur.dewPointF = -100; d.cur.windDirDeg = 337;
  drawLive(g, d, status());
  TEST_ASSERT_EQUAL_INT(0, countNot(794, 56, 797, 236, COL_PAPER));      // nothing leaks into the gap before the slot
  TEST_ASSERT_TRUE(guardsIntact());
}
void test_live_chart_handles_extreme_ranges() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  for (int i = 0; i < HIST_BUCKETS; i++) d.hist[i].tempF = (i % 2) ? -60.0f : 130.0f;
  drawLive(g, d, status());
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  // flat data
  setUp();
  for (int i = 0; i < HIST_BUCKETS; i++) d.hist[i].tempF = 50;
  for (int i = 0; i < d.hourlyCount; i++) d.hourly[i].tempF = 50;
  d.cur.tempF = 50;
  drawLive(g, d, status());
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
}
void test_stale_data_offline_and_old_forecast() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  ScreenStatus st = status();
  st.offline = true;
  st.nowEpoch = FIXTURE_NOW + 3 * 86400;          // three days later: forecast entirely in the past
  drawLive(g, d, st);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  setUp(); drawOvernight(g, d, st, TITLES_YESTERDAY_TODAY);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
}
void test_charge_me_variants() {
  ScreenStatus st;
  st.batteryVolts = 3.19f; st.lastGoodEpoch = FIXTURE_NOW;
  drawChargeMe(g, st);
  TEST_ASSERT_TRUE(guardsIntact());
  setUp(); st.batteryVolts = NAN; drawChargeMe(g, st);
  TEST_ASSERT_TRUE(guardsIntact()); TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  setUp(); st.lastGoodEpoch = 0; st.batteryVolts = 0; drawChargeMe(g, st);
  TEST_ASSERT_EQUAL(0, screenTextAnomalies());
}
void test_error_long_reason_is_truncated() {
  ScreenStatus st = status();
  char reason[400];
  memset(reason, 'x', sizeof reason - 1);
  for (int i = 5; i < 300; i += 7) reason[i] = ' ';
  reason[sizeof reason - 1] = 0;
  drawError(g, reason, st);
  TEST_ASSERT_TRUE(guardsIntact());
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 345, 39, 380, COL_PAPER));        // 880 px max, centred: 40 px margins
  TEST_ASSERT_EQUAL_INT(0, countNot(921, 345, 959, 380, COL_PAPER));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_scene_renders_inside_the_panel);
  RUN_TEST(test_rendering_is_deterministic);
  RUN_TEST(test_no_text_smaller_than_16px);
  RUN_TEST(test_goldens);
  RUN_TEST(test_live_header_rule_and_slot_box);
  RUN_TEST(test_slot_box_in_every_state);
  RUN_TEST(test_offline_chip_only_when_offline);
  RUN_TEST(test_overnight_header_bar_is_black);
  RUN_TEST(test_charge_me_and_error_screens);
  RUN_TEST(test_all_nan_and_empty_data_renders_cleanly);
  RUN_TEST(test_zero_status_and_null_reason);
  RUN_TEST(test_every_field_nan_individually);
  RUN_TEST(test_missing_history_variants);
  RUN_TEST(test_nan_text_is_intercepted_by_the_hook);
  RUN_TEST(test_long_condition_text_is_truncated_not_overlapping);
  RUN_TEST(test_wide_temperatures_do_not_touch_neighbours);
  RUN_TEST(test_stat_values_stay_in_their_columns);
  RUN_TEST(test_live_chart_handles_extreme_ranges);
  RUN_TEST(test_stale_data_offline_and_old_forecast);
  RUN_TEST(test_charge_me_variants);
  RUN_TEST(test_error_long_reason_is_truncated);
  return UNITY_END();
}
