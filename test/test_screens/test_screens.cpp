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

void test_no_text_smaller_than_20px() {
  // advance_y (line height) of the smallest permitted fonts (20 px regular and bold); no smaller font exists any more
  const int minAdvance = AtkR20.advance_y < AtkB20.advance_y ? AtkR20.advance_y : AtkB20.advance_y;
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
  {"live_real", 0x8AE22178u, 41359},
  {"live_storm", 0x1DA3A418u, 234003},
  {"live_raining", 0xF2A3DDD1u, 233231},
  {"live_cold", 0x36F1EE4Fu, 40927},
  {"live_hot", 0x2C92E9EEu, 243106},
  {"live_offline", 0xEEDF4B55u, 48460},
  {"live_nohistory", 0x240041F1u, 39103},
  {"live_rain6h", 0x025A3107u, 42672},
  {"overnight_today", 0xFE16F621u, 82755},
  {"overnight_yesterday", 0x50335DA6u, 82638},
  {"overnight_cold", 0x20D5A438u, 80735},
  {"overnight_hot", 0x846A3CF6u, 80562},
  {"charge_me", 0xD4E18531u, 24644},
  {"error", 0xF7BC3272u, 14568},
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
void test_live_header_rules_and_top() {
  renderScene("live_real", g);
  for (int x = 24; x < 936; x += 37) TEST_ASSERT_TRUE(g.get(x, 50) <= 4 || g.get(x, 49) <= 4);     // 2 px rule at y 49-50
  for (int x = 24; x < 936; x += 37) TEST_ASSERT_TRUE(g.get(x, 316) <= 4 || g.get(x, 315) <= 4);   // 2 px rule at y 315-316
  TEST_ASSERT_TRUE(countNot(176, 100, 548, 240, COL_PAPER) > 3000);     // the 170 px temperature
  TEST_ASSERT_TRUE(countNot(580, 90, 936, 124, COL_PAPER) > 300);       // first right-hand row
  TEST_ASSERT_TRUE(countNot(24, 6, 440, 40, COL_PAPER) > 300);          // location label
  TEST_ASSERT_TRUE(countNot(780, 8, 882, 44, COL_PAPER) > 40);          // update time
}
void test_alert_band_only_when_an_alert_applies() {
  const char* alerts[] = {"live_storm", "live_raining", "live_hot"};
  for (const char* n : alerts) {
    setUp(); renderScene(n, g);
    TEST_ASSERT_TRUE_MESSAGE(darkFraction(0, 318, 959, 539) > 0.88f, n);        // full-width black band (white text on top)
    TEST_ASSERT_TRUE_MESSAGE(darkFraction(0, 318, 20, 539) > 0.99f, n);
    TEST_ASSERT_TRUE_MESSAGE(darkFraction(940, 318, 959, 539) > 0.99f, n);
    TEST_ASSERT_TRUE_MESSAGE(countNot(140, 395, 700, 435, 1) > 400, n);          // white headline inside the band
    TEST_ASSERT_TRUE_MESSAGE(g.get(480, 317) == COL_PAPER, n);                    // paper between the rule and the band
    TEST_ASSERT_TRUE_MESSAGE(guardsIntact(), n);
  }
  const char* calm[] = {"live_real", "live_cold", "live_offline", "live_nohistory", "live_rain6h"};
  for (const char* n : calm) {
    setUp(); renderScene(n, g);
    TEST_ASSERT_TRUE_MESSAGE(darkFraction(0, 318, 959, 539) < 0.1f, n);          // chart area, mostly paper
    TEST_ASSERT_TRUE_MESSAGE(darkFraction(0, 318, 20, 539) < 0.1f, n);
  }
  // a calm station with only a UV >= 6 reading gets the UV band
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  d.cur.uv = 6.0f;
  setUp(); drawLive(g, d, status());
  TEST_ASSERT_TRUE(darkFraction(0, 318, 959, 539) > 0.88f);
  d.cur.uv = 5.9f;
  setUp(); drawLive(g, d, status());
  TEST_ASSERT_TRUE(darkFraction(0, 318, 959, 539) < 0.1f);
}
void test_offline_chip_only_when_offline() {
  renderScene("live_real", g);
  TEST_ASSERT_TRUE(darkFraction(630, 10, 870, 42) < 0.25f);
  setUp(); renderScene("live_offline", g);
  TEST_ASSERT_TRUE(darkFraction(640, 12, 870, 42) > 0.5f);      // black rounded chip with white text
  TEST_ASSERT_TRUE(darkFraction(640, 12, 650, 42) > 0.95f);     // solid at the left edge of the chip
  TEST_ASSERT_EQUAL_INT(0, countNot(450, 10, 600, 42, COL_PAPER));   // nothing else in the header between label and chip
}
void test_rain_note_only_when_rain_in_last_6_hours() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  for (int i = 0; i < HIST_BUCKETS; i++) d.hist[i].rainIn = 0;
  d.hist[10].rainIn = 0.5f;                        // rain long ago does not count
  drawLive(g, d, status());
  TEST_ASSERT_EQUAL_INT(0, countNot(44, 325, 300, 348, COL_PAPER));
  d.hist[20].rainIn = 0.12f;
  setUp(); drawLive(g, d, status());
  TEST_ASSERT_TRUE(countNot(44, 325, 300, 348, COL_PAPER) > 80);   // "Rain, last 6 h: 0.12 in"
  setUp(); renderScene("live_rain6h", g);
  TEST_ASSERT_TRUE(countNot(44, 325, 300, 348, COL_PAPER) > 80);
}
void test_rain_bars_measured_solid_forecast_outlined() {
  renderScene("live_rain6h", g);
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  const int64_t nowT = d.cur.time;
  // measured bucket 20 spans its real hour: solid black in the middle of that hour
  int bx = liveChartX((int64_t)d.histStartEpoch + 20 * 3600 + 1800, nowT);
  TEST_ASSERT_TRUE(g.get(bx, 484) <= 4);
  TEST_ASSERT_TRUE(g.get(bx, 470) <= 4);
  // forecast probability bars are outlined: the 9 PM hour, paper inside a 30 px bar
  setUp();
  d.hourly[0].precipProbPct = 100; d.hourly[1].precipProbPct = 100;
  drawLive(g, d, status());
  int fx = liveChartX((int64_t)d.hourly[1].time + 1800, nowT);
  TEST_ASSERT_TRUE(g.get(fx, 475) >= 13);                          // paper inside the outlined bar
  TEST_ASSERT_TRUE(g.get(fx, 459) <= 4 || g.get(fx, 460) <= 4);   // outline at the top of the 30 px bar
}
void test_live_chart_x_axis_is_true_time() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  const int64_t nowT = d.cur.time;                              // 8:07 PM fetch time
  TEST_ASSERT_EQUAL_INT(262, liveChartX(nowT, nowT));           // 44 + 6/24 * 872
  TEST_ASSERT_EQUAL_INT(44, liveChartX(nowT - 6 * 3600, nowT));
  TEST_ASSERT_EQUAL_INT(916, liveChartX(nowT + 18 * 3600, nowT));
  drawLive(g, d, status());
  // the now line is drawn at the fetch time (black column at x 262 / 263)
  TEST_ASSERT_TRUE(darkFraction(262, 380, 263, 484, 4) > 0.9f);
  // the 12 AM hairline sits at the x of its epoch (local midnight after the fixture day), not at a slot edge
  int x = liveChartX(FIXTURE_DAY_START + 86400, nowT);
  TEST_ASSERT_EQUAL_INT(262 + (int)lround((double)(FIXTURE_DAY_START + 86400 - nowT) * 872 / 86400), x);
  int col = countNot(x, 362, x, 440, COL_PAPER), left = countNot(x - 3, 362, x - 3, 440, COL_PAPER);
  TEST_ASSERT_TRUE(col > 40);
  TEST_ASSERT_TRUE(left < col / 2);
  // and its label "12 AM" is centred under it
  TEST_ASSERT_TRUE(countNot(x - 14, 505, x + 14, 526, COL_PAPER) > 40);
}
void test_overnight_top_never_touches() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  const char* conds[] = {"Rain", "Thunderstorms Possible", "Rain and Snow Possible", "Thunderstorms Likely"};
  int title = -1, hilo = -1;
  for (const char* c : conds) {
    snprintf(d.daily[1].conditions, sizeof d.daily[1].conditions, "%s", c);
    setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
    TEST_ASSERT_TRUE(guardsIntact());
    int t = countNot(480, 84, 936, 106, COL_PAPER), h = countNot(596, 128, 936, 190, COL_PAPER);
    if (title < 0) { title = t; hilo = h; }
    TEST_ASSERT_EQUAL_INT_MESSAGE(title, t, c);                      // title row identical: the condition never reaches it
    TEST_ASSERT_EQUAL_INT_MESSAGE(hilo, h, c);                       // nor the high / low
    TEST_ASSERT_TRUE_MESSAGE(countNot(480, 196, 936, 228, COL_PAPER) > 200, c);   // condition on its own line
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countNot(480, 229, 936, 231, COL_PAPER), c);   // clear of the rule at y 233-234
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countNot(937, 190, 959, 230, COL_PAPER), c);
  }
}
void test_overnight_chart_true_time_full_day_and_hilo() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  // hour h sits at x = 84 + 35 h: hairline + label every 6 h (12 AM, 6 AM, 12 PM, 6 PM, 12 AM)
  for (int h = 0; h <= 24; h += 6) {
    int x = 84 + 35 * h;
    TEST_ASSERT_TRUE(countNot(x, 300, x, 440, COL_PAPER) > 60);
    TEST_ASSERT_TRUE(countNot(x - 12, 462, x + 12, 486, COL_PAPER) > 30);
  }
  TEST_ASSERT_TRUE(countNot(84 + 35 * 9, 300, 84 + 35 * 9, 440, COL_PAPER) < 60);   // no hairline at 9 AM
  // labelled high / low are the max / min of the plotted hourly temperatures (hours 0..23 of tomorrow)
  float mx = -1e9f, mn = 1e9f;
  struct tm day;
  TEST_ASSERT_TRUE(localTm((int64_t)d.daily[1].dayStart + 12 * 3600, &day));
  for (int i = 0; i < d.hourlyCount; i++) {
    struct tm ht;
    localTm(d.hourly[i].time, &ht);
    if (ht.tm_yday != day.tm_yday || ht.tm_year != day.tm_year) continue;
    if (d.hourly[i].tempF > mx) mx = d.hourly[i].tempF;
    if (d.hourly[i].tempF < mn) mn = d.hourly[i].tempF;
  }
  TEST_ASSERT_EQUAL_FLOAT(lroundf(mx), screenNightChartHigh());
  TEST_ASSERT_EQUAL_FLOAT(lroundf(mn), screenNightChartLow());
  TEST_ASSERT_EQUAL_FLOAT(59, screenNightChartHigh());              // the header shows 59 / 40 for the same day
  TEST_ASSERT_EQUAL_FLOAT(40, screenNightChartLow());
  TEST_ASSERT_EQUAL_FLOAT(lroundf(d.daily[1].highF), screenNightChartHigh());
  TEST_ASSERT_EQUAL_FLOAT(lroundf(d.daily[1].lowF), screenNightChartLow());
  // both labels are on the chart, inside it
  TEST_ASSERT_TRUE(countNot(84, 236, 924, 290, COL_PAPER) > 200);
}
void test_overnight_today_mode_uses_measured_hours() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  sceneAdvanceDay(d);
  ScreenStatus st = status();
  st.nowEpoch = FIXTURE_DAY_START + 86400 + 3600;           // 1:00 AM Sunday
  st.lastGoodEpoch = st.nowEpoch;
  // measured hour 0 of Sunday: bucket 22 when bucket 0 starts 22 h before midnight
  d.histStartEpoch = (uint32_t)(FIXTURE_DAY_START + 86400 - 22 * 3600);
  d.have |= HAVE_HISTORY;
  for (int i = 0; i < HIST_BUCKETS; i++) d.hist[i].valid = false;
  d.hist[22].valid = true; d.hist[22].tempF = 75.0f; d.hist[22].rainIn = 0.1f;
  drawOvernight(g, d, st, TITLES_YESTERDAY_TODAY);
  TEST_ASSERT_EQUAL_FLOAT(75, screenNightChartHigh());       // the measured hour is part of the plotted series
  TEST_ASSERT_TRUE(g.get(84 + 17, 440) <= 4);                // solid rain bar for the measured hour 0
}
void test_overnight_rain_labels_only_for_10_percent_or_more() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  for (int i = 0; i < d.hourlyCount; i++) d.hourly[i].precipProbPct = 9;
  drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_EQUAL_INT(0, countNot(90, 415, 290, 443, COL_PAPER));        // no labels (between the 12 AM and 6 AM hairlines) (and no stray row of "0%")
  for (int i = 0; i < d.hourlyCount; i++) d.hourly[i].precipProbPct = 100;
  setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_TRUE(countNot(84, 393, 924, 400, COL_PAPER) > 150);          // 100 % bars are 48 px tall: labels right above them
}

void test_overnight_header_bar_is_black() {
  renderScene("overnight_today", g);
  TEST_ASSERT_TRUE(darkFraction(0, 0, 959, 63) > 0.8f);
  TEST_ASSERT_TRUE(darkFraction(0, 0, 10, 63) > 0.99f);
  TEST_ASSERT_TRUE(g.get(480, 66) == COL_PAPER);   // below the bar: paper
  for (int x = 24; x < 936; x += 37) TEST_ASSERT_TRUE(g.get(x, 234) <= 4 || g.get(x, 233) <= 4);   // rule under the summary
  for (int x = 24; x < 936; x += 37) TEST_ASSERT_TRUE(g.get(x, 500) <= 4 || g.get(x, 499) <= 4);   // footer rule
  TEST_ASSERT_TRUE(darkFraction(451, 80, 452, 222) > 0.99f);                                         // vertical rule
  TEST_ASSERT_TRUE(countNot(100, 515, 860, 536, COL_PAPER) > 400);                                   // footer line
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
                     &d.cur.stationBatteryV, &d.today.highF, &d.today.lowF, &d.today.rainIn,
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
  TEST_ASSERT_TRUE(countNot(100, 385, 220, 420, COL_PAPER) > 100);
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
  TEST_ASSERT_EQUAL_INT(0, countNot(574, 280, 600, 310, COL_PAPER));     // nothing leaks past x 572 under the stats
  TEST_ASSERT_TRUE(countNot(30, 262, 572, 300, COL_PAPER) > 400);       // the text is there (shrunk / ellipsized)
  TEST_ASSERT_TRUE(countNot(500, 285, 572, 296, COL_PAPER) > 15);       // ends in "..." near the limit
}
void test_wide_temperatures_do_not_touch_neighbours() {
  const float temps[] = {-100, -12, -9, 0, 5, 99, 100, 123, 199, -40.6f};
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  for (float t : temps) {
    d.cur.tempF = t;
    setUp(); drawLive(g, d, status());
    TEST_ASSERT_TRUE(guardsIntact());
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countNot(550, 60, 579, 240, COL_PAPER), "big temp reaches the right-hand rows");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countNot(0, 60, 13, 240, COL_PAPER), "big temp left of the icon");
    TEST_ASSERT_TRUE(countNot(176, 84, 548, 240, COL_PAPER) > 400);        // something big is there
  }
  // overnight hi / lo
  d.daily[1].highF = -100; d.daily[1].lowF = -120;
  setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_TRUE(guardsIntact());
  TEST_ASSERT_EQUAL_INT(0, countNot(937, 110, 959, 205, COL_PAPER));
  d.daily[1].highF = 119; d.daily[1].lowF = 100;
  setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_EQUAL_INT(0, countNot(937, 110, 959, 205, COL_PAPER));
  d.today.highF = 119; d.today.lowF = -100; d.today.rainIn = 12.34f;
  setUp(); drawOvernight(g, d, status(), TITLES_TODAY_TOMORROW);
  TEST_ASSERT_EQUAL_INT(0, countNot(441, 110, 450, 205, COL_PAPER));      // today row stays left of the vertical rule
}
void test_stat_values_stay_in_their_columns() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  d.cur.windMph = 123; d.cur.gustMph = 188; d.cur.rainTodayIn = 12.34f; d.cur.pressureInHg = 29.99f;
  d.cur.dewPointF = -100; d.cur.windDirDeg = 337;
  drawLive(g, d, status());
  TEST_ASSERT_EQUAL_INT(0, countNot(937, 60, 959, 300, COL_PAPER));      // right margin stays clear
  TEST_ASSERT_EQUAL_INT(0, countNot(550, 60, 579, 300, COL_PAPER));      // and nothing left of the labels
  TEST_ASSERT_TRUE(guardsIntact());
}
void test_missing_values_show_dashes_not_nan() {
  TEST_ASSERT_TRUE(loadFixtureSet(nullptr, d));
  d.cur.windMph = NAN; d.cur.rainTodayIn = NAN; d.today.valid = false; d.cur.tempF = NAN;
  drawLive(g, d, status());
  TEST_ASSERT_EQUAL(0, screenTextAnomalies());
  TEST_ASSERT_TRUE(countNot(176, 120, 548, 240, COL_PAPER) > 300);      // "--" in the big font
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
  RUN_TEST(test_no_text_smaller_than_20px);
  RUN_TEST(test_goldens);
  RUN_TEST(test_live_header_rules_and_top);
  RUN_TEST(test_alert_band_only_when_an_alert_applies);
  RUN_TEST(test_offline_chip_only_when_offline);
  RUN_TEST(test_rain_note_only_when_rain_in_last_6_hours);
  RUN_TEST(test_rain_bars_measured_solid_forecast_outlined);
  RUN_TEST(test_live_chart_x_axis_is_true_time);
  RUN_TEST(test_overnight_top_never_touches);
  RUN_TEST(test_overnight_chart_true_time_full_day_and_hilo);
  RUN_TEST(test_overnight_today_mode_uses_measured_hours);
  RUN_TEST(test_overnight_rain_labels_only_for_10_percent_or_more);
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
  RUN_TEST(test_missing_values_show_dashes_not_nan);
  RUN_TEST(test_live_chart_handles_extreme_ranges);
  RUN_TEST(test_stale_data_offline_and_old_forecast);
  RUN_TEST(test_charge_me_variants);
  RUN_TEST(test_error_long_reason_is_truncated);
  return UNITY_END();
}
