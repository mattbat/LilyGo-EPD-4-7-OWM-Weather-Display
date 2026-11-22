#include <unity.h>
#include <string.h>
#include <math.h>
#include "charts.h"
#include "fonts/fonts.h"

static uint8_t fb[PANEL_WIDTH * PANEL_HEIGHT / 2];
static Gfx g;

void setUp() { g.begin(fb); g.clear(15); }
void tearDown() {}

static int countBox(int x0, int y0, int x1, int y1, uint8_t maxGrey) {  // pixels <= maxGrey, inclusive box
  int n = 0;
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (g.get(x, y) <= maxGrey) n++;
  return n;
}
static int countNot(int x0, int y0, int x1, int y1, uint8_t bg) {
  int n = 0;
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (g.get(x, y) != bg) n++;
  return n;
}

// ---- autoRange -------------------------------------------------------------------------------
void test_autorange_normal() {
  float v[] = {57.2f, 61.0f, 68.4f, 52.1f};
  AxisRange r = autoRange(v, 4);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_FLOAT(45, r.min);   // floor((52.1-3)/5)*5
  TEST_ASSERT_EQUAL_FLOAT(75, r.max);   // ceil((68.4+3)/5)*5
}
void test_autorange_exact_multiples_get_padding() {
  float v[] = {50, 70};
  AxisRange r = autoRange(v, 2);
  TEST_ASSERT_EQUAL_FLOAT(45, r.min);
  TEST_ASSERT_EQUAL_FLOAT(75, r.max);
}
void test_autorange_flat() {
  float v[] = {60, 60, 60};
  AxisRange r = autoRange(v, 3);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.min < 60 && r.max > 60);
  TEST_ASSERT_TRUE(r.max - r.min >= 5);
  AxisRange z = autoRange(v, 3, 5, 0);   // no padding: still a non-empty range
  TEST_ASSERT_TRUE(z.max - z.min >= 5);
}
void test_autorange_negative() {
  float v[] = {-8.2f, -1.0f, 3.0f};
  AxisRange r = autoRange(v, 3);
  TEST_ASSERT_EQUAL_FLOAT(-15, r.min);   // floor(-11.2/5)*5
  TEST_ASSERT_EQUAL_FLOAT(10, r.max);    // ceil(6/5)*5
}
void test_autorange_all_nan_and_empty() {
  float v[] = {NAN, NAN};
  AxisRange r = autoRange(v, 2);
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_TRUE(r.max > r.min);
  TEST_ASSERT_FALSE(autoRange(nullptr, 0).valid);
}
void test_autorange_single_value_and_nan_mixed() {
  float v[] = {NAN, 72.0f, NAN};
  AxisRange r = autoRange(v, 3);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_FLOAT(65, r.min);
  TEST_ASSERT_EQUAL_FLOAT(75, r.max);
}
void test_autorange2_combines() {
  float a[] = {50, 55};
  float b[] = {NAN, 80};
  AxisRange r = autoRange2(a, 2, b, 2);
  TEST_ASSERT_EQUAL_FLOAT(45, r.min);
  TEST_ASSERT_EQUAL_FLOAT(85, r.max);
  TEST_ASSERT_FALSE(autoRange2(nullptr, 0, nullptr, 0).valid);
  AxisRange only = autoRange2(nullptr, 0, b, 2);
  TEST_ASSERT_TRUE(only.valid);
}
void test_grid_values() {
  AxisRange r = {45, 75, true};
  TEST_ASSERT_EQUAL_FLOAT(10, gridStepFor(r));
  float out[8];
  int n = gridValues(r, 10, out, 8);
  TEST_ASSERT_EQUAL_INT(3, n);
  TEST_ASSERT_EQUAL_FLOAT(50, out[0]);
  TEST_ASSERT_EQUAL_FLOAT(70, out[2]);
  AxisRange small = {50, 65, true};
  TEST_ASSERT_EQUAL_FLOAT(5, gridStepFor(small));
  n = gridValues(small, 5, out, 8);
  TEST_ASSERT_EQUAL_INT(2, n);         // 55, 60 (endpoints excluded)
  TEST_ASSERT_EQUAL_FLOAT(55, out[0]);
}

// ---- mapping ---------------------------------------------------------------------------------
void test_mapping_endpoints() {
  TEST_ASSERT_EQUAL_FLOAT(100, mapX(0, 25, 100, 400));
  TEST_ASSERT_EQUAL_FLOAT(500, mapX(24, 25, 100, 400));
  TEST_ASSERT_EQUAL_FLOAT(300, mapX(12, 25, 100, 400));
  TEST_ASSERT_EQUAL_FLOAT(100, mapX(0, 1, 100, 400));
  TEST_ASSERT_EQUAL_FLOAT(150, mapY(45, 45, 75, 50, 100));   // min -> bottom
  TEST_ASSERT_EQUAL_FLOAT(50, mapY(75, 45, 75, 50, 100));    // max -> top
  TEST_ASSERT_EQUAL_FLOAT(100, mapY(60, 45, 75, 50, 100));
  TEST_ASSERT_EQUAL_FLOAT(50, mapY(500, 45, 75, 50, 100));   // clamped
  TEST_ASSERT_EQUAL_FLOAT(150, mapY(-500, 45, 75, 50, 100));
  TEST_ASSERT_EQUAL_FLOAT(100, mapY(7, 5, 5, 50, 100));      // degenerate range -> middle
}

// ---- hi / lo ---------------------------------------------------------------------------------
void test_hilo_index_and_end_skip() {
  float v[] = {5, 6, 9, 7, 3, 4, 8, 6, 5};   // max 9 @2, min 3 @4
  TEST_ASSERT_EQUAL_INT(2, hiLoIndex(v, 9, true));
  TEST_ASSERT_EQUAL_INT(4, hiLoIndex(v, 9, false));
  float e[] = {9, 6, 5, 3, 4, 5, 6, 7, 8};   // max at the first sample, min inside
  TEST_ASSERT_EQUAL_INT(-1, hiLoIndex(e, 9, true));
  TEST_ASSERT_EQUAL_INT(3, hiLoIndex(e, 9, false));
  float near[] = {1, 9, 5, 4, 3, 4, 5, 2, 6};  // max @1, within the default margin of 2
  TEST_ASSERT_EQUAL_INT(-1, hiLoIndex(near, 9, true));
  TEST_ASSERT_EQUAL_INT(1, hiLoIndex(near, 9, true, 1));
  TEST_ASSERT_EQUAL_INT(1, hiLoIndex(near, 9, true, 0));
  float last[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  TEST_ASSERT_EQUAL_INT(-1, hiLoIndex(last, 9, true));
  TEST_ASSERT_EQUAL_INT(-1, hiLoIndex(last, 9, true, 1));
  float nn[] = {NAN, NAN};
  TEST_ASSERT_EQUAL_INT(-1, hiLoIndex(nn, 2, true, 0));
  float tie[] = {1, 5, 3, 5, 2, 1, 1};
  TEST_ASSERT_EQUAL_INT(1, hiLoIndex(tie, 7, true, 0));   // first occurrence
  float gap[] = {NAN, 3, 8, NAN, 4, 2, 5, 6};
  TEST_ASSERT_EQUAL_INT(2, hiLoIndex(gap, 8, true));
}

// ---- bars ------------------------------------------------------------------------------------
void test_bar_scale_floor_and_height() {
  float tiny[] = {0, 0.01f, 0.02f};
  TEST_ASSERT_EQUAL_FLOAT(0.05f, barScaleMax(tiny, 3));          // floor wins
  float big[] = {0.1f, 0.4f, NAN};
  TEST_ASSERT_EQUAL_FLOAT(0.4f, barScaleMax(big, 3));
  TEST_ASSERT_EQUAL_FLOAT(0.05f, barScaleMax(nullptr, 0));
  TEST_ASSERT_EQUAL_INT(40, barHeight(0.4f, 0.4f, 40));
  TEST_ASSERT_EQUAL_INT(20, barHeight(0.2f, 0.4f, 40));
  TEST_ASSERT_EQUAL_INT(3, barHeight(0.001f, 0.4f, 40));         // min visible height
  TEST_ASSERT_EQUAL_INT(0, barHeight(0.0f, 0.4f, 40));
  TEST_ASSERT_EQUAL_INT(0, barHeight(NAN, 0.4f, 40));
  TEST_ASSERT_EQUAL_INT(0, barHeight(-1, 0.4f, 40));
  TEST_ASSERT_EQUAL_INT(40, barHeight(9.0f, 0.4f, 40));          // clamped to the rect
  TEST_ASSERT_EQUAL_INT(2, barHeight(1.0f, 0.4f, 2, 3));         // min height cannot exceed h
  // 0.01 in against the 0.05 floor: 8 px of 40, not a full-height bar
  TEST_ASSERT_EQUAL_INT(8, barHeight(0.01f, barScaleMax(tiny, 3), 40));
}
void test_bar_slots_do_not_overlap_and_fit() {
  int prevEnd = -1;
  for (int i = 0; i < 24; i++) {
    int bx, bw;
    barSlot(i, 24, 60, 435, 4, &bx, &bw);
    TEST_ASSERT_TRUE(bx >= 60 && bx + bw <= 60 + 435);
    TEST_ASSERT_TRUE(bw >= 1);
    TEST_ASSERT_TRUE(bx > prevEnd);   // at least a 1 px gap
    prevEnd = bx + bw;
  }
}
void test_draw_bars_pixels() {
  float v[] = {0.4f, 0, 0.2f, NAN};
  BarStyle st; st.baseline = false; st.gap = 4;
  drawBars(g, v, 4, {100, 100, 80, 40}, 0, st);   // auto scale -> 0.4 is full height
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(110, 100));             // tallest bar reaches the top
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(110, 139));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(130, 139));                // zero bar: empty
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(150, 139));             // half height bar: bottom drawn
  TEST_ASSERT_EQUAL_UINT8(15, g.get(150, 110));                // ...and its top half is not
  TEST_ASSERT_EQUAL_UINT8(15, g.get(170, 139));                // NAN: empty
}
void test_prob_bars() {
  float p[] = {100, 50, 0, NAN, 1};
  drawProbBars(g, p, 5, {0, 0, 100, 40}, 8, 4);
  TEST_ASSERT_EQUAL_UINT8(8, g.get(10, 0));       // 100% reaches the top
  TEST_ASSERT_EQUAL_UINT8(8, g.get(30, 39));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(30, 10));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(50, 39));
  TEST_ASSERT_EQUAL_UINT8(8, g.get(90, 39));      // 1% still visible (min 2 px)
  TEST_ASSERT_EQUAL_UINT8(8, g.get(90, 38));
}

// ---- line series -----------------------------------------------------------------------------
void test_line_series_nan_gap_breaks_line() {
  float v[] = {10, 10, NAN, NAN, 10, 10};
  AxisRange ax = {0, 20, true};
  ChartRect r = {100, 100, 250, 100};   // x step 50: samples at 100,150,200,250,300,350
  LineStyle st; st.thickness = 3;
  drawLineSeries(g, v, 6, r, ax, st);
  int y = 150;
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(125, y));   // inside first run
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(325, y));   // inside last run
  TEST_ASSERT_EQUAL_UINT8(15, g.get(200, y));      // gap
  TEST_ASSERT_EQUAL_UINT8(15, g.get(250, y));
  // the same data without NAN is one continuous line
  g.clear(15);
  float w[] = {10, 10, 10, 10, 10, 10};
  drawLineSeries(g, w, 6, r, ax, st);
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(225, y));
}
void test_line_series_single_sample_is_dot_and_all_nan_is_empty() {
  float v[] = {NAN, 10, NAN};
  AxisRange ax = {0, 20, true};
  drawLineSeries(g, v, 3, {100, 100, 200, 100}, ax, LineStyle());
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(200, 150));
  g.clear(15);
  float n[] = {NAN, NAN};
  drawLineSeries(g, n, 2, {100, 100, 200, 100}, ax, LineStyle());
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15));
}
void test_line_series_area_fill_and_dashed() {
  float v[] = {10, 10, 10};
  AxisRange ax = {0, 20, true};
  ChartRect r = {100, 100, 200, 100};
  LineStyle st; st.fillArea = true; st.fillGrey = 13; st.thickness = 3;
  drawLineSeries(g, v, 3, r, ax, st);
  TEST_ASSERT_EQUAL_UINT8(13, g.get(200, 180));    // filled below the line
  TEST_ASSERT_EQUAL_UINT8(15, g.get(200, 120));    // not above
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(200, 150));
  g.clear(15);
  LineStyle d; d.dashed = true; d.dash = 10; d.gap = 7; d.thickness = 3;
  drawLineSeries(g, v, 3, r, ax, d);
  int ink = 0;
  for (int x = 100; x < 300; x++) if (g.get(x, 150) == COL_K) ink++;
  TEST_ASSERT_TRUE(ink > 60 && ink < 160);          // roughly 10/17 of 200 px
}
void test_series_point_and_end_dot() {
  float v[] = {0, 10, NAN};
  AxisRange ax = {0, 20, true};
  ChartRect r = {0, 0, 100, 100};
  float x, y;
  TEST_ASSERT_TRUE(seriesPoint(v, 3, 1, r, ax, &x, &y));
  TEST_ASSERT_EQUAL_FLOAT(50, x);
  TEST_ASSERT_EQUAL_FLOAT(50, y);
  TEST_ASSERT_FALSE(seriesPoint(v, 3, 2, r, ax, &x, &y));
  TEST_ASSERT_FALSE(seriesPoint(v, 3, 3, r, ax, &x, &y));
  TEST_ASSERT_TRUE(drawEndDot(g, v, 3, {100, 100, 100, 100}, ax, 5));   // last finite = index 1
  TEST_ASSERT_EQUAL_UINT8(COL_K, g.get(150, 150));
  float n[] = {NAN};
  TEST_ASSERT_FALSE(drawEndDot(g, n, 1, r, ax));
}

// ---- markers / grid / sparkline ---------------------------------------------------------------
void test_hilo_marker_draws_inside_rect() {
  float v[25];
  for (int i = 0; i < 25; i++) v[i] = 60 + 10 * sinf(i / 4.0f);   // max near i=6, min near i=19
  AxisRange ax = autoRange(v, 25);
  ChartRect r = {100, 100, 400, 112};
  TEST_ASSERT_TRUE(drawHiLoMarker(g, v, 25, r, ax, true, AtkB19));
  TEST_ASSERT_TRUE(drawHiLoMarker(g, v, 25, r, ax, false, AtkB19));
  // everything drawn is inside the rect
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, PANEL_WIDTH - 1, 99, 15));
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 213, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15));
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, 99, PANEL_HEIGHT - 1, 15));
  TEST_ASSERT_EQUAL_INT(0, countNot(501, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15));
  // skipped at the ends: monotonic data has its max at the last sample
  g.clear(15);
  float m[] = {1, 2, 3, 4, 5, 6, 7};
  TEST_ASSERT_FALSE(drawHiLoMarker(g, m, 7, r, autoRange(m, 7), true, AtkB19));
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15));
}
void test_hilo_label_clamped_at_top() {
  // extreme near the top of the range: the label would stick out above the rect, so it is pushed in
  float v[] = {0, 0, 0, 0, 20, 0, 0, 0, 0};
  AxisRange ax = {0, 20, true};
  ChartRect r = {100, 100, 200, 60};
  TEST_ASSERT_TRUE(drawHiLoMarker(g, v, 9, r, ax, true, AtkB19));
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, PANEL_WIDTH - 1, 99 - 5, 15));   // only the dot may poke out (r=4)
}
void test_grid_draws_lines_and_labels() {
  AxisRange ax = {45, 75, true};
  ChartRect r = {100, 100, 300, 120};
  drawGrid(g, r, ax, AtkR15);
  // 50, 60, 70 -> y = 220 - (v-45)/30*120
  TEST_ASSERT_EQUAL_UINT8(COL_XL, g.get(200, 220 - 20));
  TEST_ASSERT_EQUAL_UINT8(COL_XL, g.get(200, 220 - 60));
  TEST_ASSERT_EQUAL_UINT8(COL_XL, g.get(200, 220 - 100));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(200, 220 - 40));
  TEST_ASSERT_TRUE(countBox(40, 190, 95, 210, 8) > 10);   // axis label to the left of the rect
}
void test_sparkline() {
  AxisRange r = sparkRange(nullptr, 0);
  TEST_ASSERT_FALSE(r.valid);
  float flat[] = {5, 5, 5};
  AxisRange f = sparkRange(flat, 3);
  TEST_ASSERT_TRUE(f.min < 5 && f.max > 5);
  float v[] = {1, 3, 2, 5, 4};
  drawSparkline(g, v, 5, {100, 100, 100, 50});
  TEST_ASSERT_TRUE(countBox(100, 95, 205, 155, 4) > 50);
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, 91, PANEL_HEIGHT - 1, 15));   // nothing left of rect (dot r=4)
  g.clear(15);
  float n[] = {NAN, NAN};
  drawSparkline(g, n, 2, {100, 100, 100, 50});
  drawSparkline(g, flat, 3, {0, 0, 0, 0});   // degenerate rect: no crash
  TEST_ASSERT_EQUAL_INT(0, countNot(150, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15));
}

// ---- lightning row ---------------------------------------------------------------------------
void test_lightning_row() {
  uint16_t none[24] = {0};
  ChartRect r = {100, 100, 240, 34};
  TEST_ASSERT_FALSE(drawLightningRow(g, none, 24, r, AtkB13, AtkR14));
  int grey = 0, dark = 0;
  for (int y = 90; y < 140; y++) for (int x = 90; x < 400; x++) { uint8_t v = g.get(x, y); if (v != 15) grey++; if (v < 5) dark++; }
  TEST_ASSERT_TRUE(grey > 30);     // the message is there
  TEST_ASSERT_EQUAL_INT(0, dark);  // ...and it is grey, not black
  g.clear(15);
  uint16_t s[24] = {0};
  s[5] = 14;
  TEST_ASSERT_TRUE(drawLightningRow(g, s, 24, r, AtkB13, AtkR14));
  int x5 = 100 + 5 * 10 + 5;
  TEST_ASSERT_TRUE(countBox(x5 - 6, 100, x5 + 6, 118, 4) > 10);      // bolt
  TEST_ASSERT_TRUE(countBox(x5 - 10, 120, x5 + 10, 134, 4) > 8);     // count text
  TEST_ASSERT_EQUAL_INT(0, countBox(100, 100, 140, 134, 4));          // other slots empty
}

// ---- labels ----------------------------------------------------------------------------------
void test_hour_labels() {
  char b[16];
  formatHourLabel(b, sizeof b, 0);  TEST_ASSERT_EQUAL_STRING("12 AM", b);
  formatHourLabel(b, sizeof b, 12); TEST_ASSERT_EQUAL_STRING("12 PM", b);
  formatHourLabel(b, sizeof b, 14); TEST_ASSERT_EQUAL_STRING("2 PM", b);
  formatHourLabel(b, sizeof b, 5);  TEST_ASSERT_EQUAL_STRING("5 AM", b);
  formatHourLabel(b, sizeof b, 11); TEST_ASSERT_EQUAL_STRING("11 AM", b);
  formatHourLabel(b, sizeof b, 23); TEST_ASSERT_EQUAL_STRING("11 PM", b);
  formatHourLabel(b, sizeof b, 24); TEST_ASSERT_EQUAL_STRING("12 AM", b);
  formatHourLabel(b, sizeof b, -6); TEST_ASSERT_EQUAL_STRING("6 PM", b);
  formatHourLabel(b, sizeof b, 38); TEST_ASSERT_EQUAL_STRING("2 PM", b);
}
void test_format_degrees() {
  char b[16];
  formatDegrees(b, sizeof b, 71.6f);  TEST_ASSERT_EQUAL_STRING("72\xC2\xB0", b);
  formatDegrees(b, sizeof b, -5.2f);  TEST_ASSERT_EQUAL_STRING("-5\xC2\xB0", b);
  formatDegrees(b, sizeof b, -0.3f);  TEST_ASSERT_EQUAL_STRING("0\xC2\xB0", b);
  formatDegrees(b, sizeof b, 105.0f); TEST_ASSERT_EQUAL_STRING("105\xC2\xB0", b);
}
void test_hour_of_day() {
  TEST_ASSERT_EQUAL_INT(20, hourOfDay(1795291200u, 0));             // 2026-11-21 20:00 UTC
  TEST_ASSERT_EQUAL_INT(14, hourOfDay(1795291200u, -6 * 3600));
  TEST_ASSERT_EQUAL_INT(2, hourOfDay(1795291200u, 6 * 3600));       // wraps past midnight
  TEST_ASSERT_EQUAL_INT(14, localHourOfEpoch(1795291200u, -6 * 3600));
  TEST_ASSERT_EQUAL_INT(14, hourOfDay(1795291200u + 1800, -6 * 3600));   // mid-hour stays in the hour
}
void test_hour_axis_labels() {
  HourAxis ax = {1795291200u - 24 * 3600u, 48, 6, 24, -6 * 3600};   // slot 24 = 2 PM local = now
  HourLabel l[32];
  int n = hourAxisLabels(ax, l, 32);
  TEST_ASSERT_EQUAL_INT(9, n);
  TEST_ASSERT_EQUAL_INT(0, l[0].slot);
  TEST_ASSERT_EQUAL_STRING("2 PM", l[0].text);
  TEST_ASSERT_EQUAL_STRING("8 PM", l[1].text);
  TEST_ASSERT_EQUAL_STRING("2 AM", l[2].text);
  TEST_ASSERT_EQUAL_STRING("8 AM", l[3].text);
  TEST_ASSERT_EQUAL_STRING("NOW", l[4].text);
  TEST_ASSERT_TRUE(l[4].isNow);
  TEST_ASSERT_EQUAL_STRING("8 PM", l[5].text);
  TEST_ASSERT_EQUAL_STRING("2 PM", l[8].text);
  TEST_ASSERT_EQUAL_INT(48, l[8].slot);
  ax.step = 12;                                  // coarser axis: 0, 12, 24, 36, 48
  TEST_ASSERT_EQUAL_INT(5, hourAxisLabels(ax, l, 32));
  ax.nowSlot = -1; ax.step = 6;
  n = hourAxisLabels(ax, l, 32);
  TEST_ASSERT_FALSE(l[4].isNow);
  TEST_ASSERT_EQUAL_STRING("2 PM", l[4].text);
  TEST_ASSERT_EQUAL_INT(3, hourAxisLabels(ax, l, 3));   // respects maxOut
}
void test_hour_axis_draw_stays_inside() {
  HourAxis ax = {1795291200u - 24 * 3600u, 48, 6, 24, -6 * 3600};
  drawHourAxis(g, 60, 870, 300, ax, AtkR15, AtkB15);
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, 59, PANEL_HEIGHT - 1, 15));
  TEST_ASSERT_EQUAL_INT(0, countNot(931, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15));
  TEST_ASSERT_TRUE(countBox(60, 280, 930, 305, 5) > 100);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_autorange_normal);
  RUN_TEST(test_autorange_exact_multiples_get_padding);
  RUN_TEST(test_autorange_flat);
  RUN_TEST(test_autorange_negative);
  RUN_TEST(test_autorange_all_nan_and_empty);
  RUN_TEST(test_autorange_single_value_and_nan_mixed);
  RUN_TEST(test_autorange2_combines);
  RUN_TEST(test_grid_values);
  RUN_TEST(test_mapping_endpoints);
  RUN_TEST(test_hilo_index_and_end_skip);
  RUN_TEST(test_bar_scale_floor_and_height);
  RUN_TEST(test_bar_slots_do_not_overlap_and_fit);
  RUN_TEST(test_draw_bars_pixels);
  RUN_TEST(test_prob_bars);
  RUN_TEST(test_line_series_nan_gap_breaks_line);
  RUN_TEST(test_line_series_single_sample_is_dot_and_all_nan_is_empty);
  RUN_TEST(test_line_series_area_fill_and_dashed);
  RUN_TEST(test_series_point_and_end_dot);
  RUN_TEST(test_hilo_marker_draws_inside_rect);
  RUN_TEST(test_hilo_label_clamped_at_top);
  RUN_TEST(test_grid_draws_lines_and_labels);
  RUN_TEST(test_sparkline);
  RUN_TEST(test_lightning_row);
  RUN_TEST(test_hour_labels);
  RUN_TEST(test_format_degrees);
  RUN_TEST(test_hour_of_day);
  RUN_TEST(test_hour_axis_labels);
  RUN_TEST(test_hour_axis_draw_stays_inside);
  return UNITY_END();
}
