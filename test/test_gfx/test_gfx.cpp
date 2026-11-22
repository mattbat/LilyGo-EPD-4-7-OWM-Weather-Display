#include <unity.h>
#include <string.h>
#include "gfx.h"

static uint8_t fb[PANEL_WIDTH * PANEL_HEIGHT / 2];
static Gfx g;

void setUp() { g.begin(fb); g.clear(15); }
void tearDown() {}

static int countGrey(int x0, int y0, int x1, int y1, uint8_t v) {  // inclusive box
  int n = 0;
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (g.get(x, y) == v) n++;
  return n;
}
static int countBlack() { return countGrey(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 0); }
static bool fbAllWhite() {
  for (int i = 0; i < PANEL_WIDTH * PANEL_HEIGHT / 2; i++) if (fb[i] != 0xFF) return false;
  return true;
}

void test_pixel_nibble_packing() {
  g.pixel(0, 0, 3);
  g.pixel(1, 0, 9);
  TEST_ASSERT_EQUAL_HEX8(0x93, fb[0]);  // even x = low nibble, odd x = high nibble (epd_draw_pixel)
  g.pixel(2, 0, 0);
  TEST_ASSERT_EQUAL_HEX8(0xF0, fb[1]);
  g.pixel(PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 5);
  TEST_ASSERT_EQUAL_HEX8(0x5F, fb[PANEL_WIDTH * PANEL_HEIGHT / 2 - 1]);
  TEST_ASSERT_EQUAL_UINT8(3, g.get(0, 0));
  TEST_ASSERT_EQUAL_UINT8(9, g.get(1, 0));
  g.pixel(0, 1, 7);  // row stride is W/2 bytes
  TEST_ASSERT_EQUAL_HEX8(0xF7, fb[PANEL_WIDTH / 2]);
}

void test_clear_and_mask() {
  g.clear(4);
  TEST_ASSERT_EQUAL_HEX8(0x44, fb[0]);
  TEST_ASSERT_EQUAL_HEX8(0x44, fb[PANEL_WIDTH * PANEL_HEIGHT / 2 - 1]);
  g.pixel(5, 5, 0x1F);  // high bits ignored
  TEST_ASSERT_EQUAL_UINT8(15, g.get(5, 5));
  TEST_ASSERT_EQUAL_UINT8(4, g.get(4, 5));
}

void test_pixel_clipping_all_edges() {
  g.pixel(-1, 0, 0); g.pixel(0, -1, 0); g.pixel(PANEL_WIDTH, 0, 0); g.pixel(0, PANEL_HEIGHT, 0);
  g.pixel(-100000, 5, 0); g.pixel(5, 100000, 0);
  TEST_ASSERT_TRUE(fbAllWhite());
  TEST_ASSERT_EQUAL_UINT8(15, g.get(-1, 0));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(PANEL_WIDTH, PANEL_HEIGHT));
  g.pixel(0, 0, 0); g.pixel(PANEL_WIDTH - 1, 0, 0); g.pixel(0, PANEL_HEIGHT - 1, 0); g.pixel(PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 0);
  TEST_ASSERT_EQUAL_INT(4, countBlack());
}

void test_hline_length_all_alignments() {
  for (int x0 = 0; x0 < 4; x0++)
    for (int len = 1; len <= 7; len++) {
      g.clear(15);
      g.hline(100 + x0, 50, len, 0);
      TEST_ASSERT_EQUAL_INT(len, countBlack());
      TEST_ASSERT_EQUAL_INT(len, countGrey(100 + x0, 50, 100 + x0 + len - 1, 50, 0));
    }
  g.clear(15);
  g.hline(10, 10, 0, 0);
  g.hline(10, 10, -5, 0);
  TEST_ASSERT_EQUAL_INT(0, countBlack());
}

void test_hline_vline_clipping() {
  g.hline(-5, 0, 10, 0);
  TEST_ASSERT_EQUAL_INT(5, countBlack());
  g.clear(15);
  g.hline(PANEL_WIDTH - 3, 7, 10, 0);
  TEST_ASSERT_EQUAL_INT(3, countBlack());
  g.clear(15);
  g.hline(0, -1, 50, 0); g.hline(0, PANEL_HEIGHT, 50, 0);
  TEST_ASSERT_TRUE(fbAllWhite());
  g.vline(3, -4, 10, 0);
  TEST_ASSERT_EQUAL_INT(6, countBlack());
  g.clear(15);
  g.vline(3, PANEL_HEIGHT - 2, 10, 0);
  TEST_ASSERT_EQUAL_INT(2, countBlack());
  g.clear(15);
  g.vline(20, 30, 25, 0);
  TEST_ASSERT_EQUAL_INT(25, countBlack());
  TEST_ASSERT_EQUAL_UINT8(0, g.get(20, 54));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(20, 55));
}

void test_thin_line() {
  g.line(10, 10, 50, 10, 1, 0);
  TEST_ASSERT_EQUAL_INT(41, countBlack());  // both endpoints inclusive
  g.clear(15);
  g.line(10, 10, 40, 40, 1, 0);
  TEST_ASSERT_EQUAL_INT(31, countBlack());
  TEST_ASSERT_EQUAL_UINT8(0, g.get(25, 25));
  g.clear(15);
  g.line(10, 10, 10, 10, 1, 0);
  TEST_ASSERT_EQUAL_INT(1, countBlack());
}

void test_thick_line_coverage() {
  for (int t = 2; t <= 8; t++) {
    const Cap caps[] = {CAP_BUTT, CAP_SQUARE, CAP_ROUND};
    for (Cap c : caps) {
      g.clear(15);
      g.line(100, 100, 160, 100, t, 0, c);
      TEST_ASSERT_EQUAL_INT(t, countGrey(130, 0, 130, PANEL_HEIGHT - 1, 0));  // thickness at mid
      g.clear(15);
      g.line(100, 100, 100, 160, t, 0, c);
      TEST_ASSERT_EQUAL_INT(t, countGrey(0, 130, PANEL_WIDTH - 1, 130, 0));
    }
    g.clear(15);  // butt: exactly (len+1) x t pixels, end pixels included
    g.line(100, 100, 160, 100, t, 0, CAP_BUTT);
    TEST_ASSERT_EQUAL_INT(61 * t, countBlack());
  }
}

void test_thick_diagonal_area_and_reverse() {
  g.line(100, 100, 200, 200, 6, 0, CAP_BUTT);
  TEST_ASSERT_INT_WITHIN(60, 6 * 142, countBlack());  // band area about t * length
  static uint8_t copy[PANEL_WIDTH * PANEL_HEIGHT / 2];
  memcpy(copy, fb, sizeof copy);
  g.clear(15);
  g.line(200, 200, 100, 100, 6, 0, CAP_BUTT);
  TEST_ASSERT_EQUAL_INT(0, memcmp(copy, fb, sizeof copy));  // direction independent
}

void test_round_cap_extends_and_is_round() {
  g.line(100, 100, 100, 100, 8, 0, CAP_ROUND);  // a dot
  TEST_ASSERT_INT_WITHIN(8, 50, countBlack());   // ~ pi * 4^2
  TEST_ASSERT_EQUAL_UINT8(15, g.get(96, 96));    // square corner not filled
  g.clear(15);
  g.line(100, 100, 100, 100, 8, 0, CAP_SQUARE);
  TEST_ASSERT_EQUAL_INT(64, countBlack());
}

void test_line_clipping_extreme() {
  g.line(-5000, -5000, 6000, 6000, 3, 0);
  g.line(-50, 270, 2000, 270, 8, 0);
  g.line(PANEL_WIDTH - 2, 0, PANEL_WIDTH + 100, 30, 5, 0);
  TEST_ASSERT_TRUE(countBlack() > 0);
}

void test_rect_edges() {
  g.rect(10, 10, 20, 10, 0, 2);
  TEST_ASSERT_EQUAL_INT(20 * 10 - 16 * 6, countBlack());
  TEST_ASSERT_EQUAL_UINT8(0, g.get(10, 10));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(29, 19));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(30, 19));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(29, 20));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(15, 15));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(9, 10));
  g.clear(15);
  g.rect(5, 5, 40, 30, 0, 1);
  TEST_ASSERT_EQUAL_INT(2 * 40 + 2 * 28, countBlack());
  g.clear(15);
  g.rect(5, 5, 6, 6, 0, 4);  // thicker than half: fully filled
  TEST_ASSERT_EQUAL_INT(36, countBlack());
}

void test_fillrect_exact_and_clipping() {
  g.fillRect(11, 7, 13, 9, 0);
  TEST_ASSERT_EQUAL_INT(13 * 9, countBlack());
  g.clear(15);
  g.fillRect(-5, -5, 10, 10, 0);
  TEST_ASSERT_EQUAL_INT(25, countBlack());
  g.clear(15);
  g.fillRect(PANEL_WIDTH - 5, PANEL_HEIGHT - 5, 10, 10, 0);
  TEST_ASSERT_EQUAL_INT(25, countBlack());
  g.clear(15);
  g.fillRect(-10, -10, PANEL_WIDTH + 20, PANEL_HEIGHT + 20, 7);
  TEST_ASSERT_EQUAL_INT(PANEL_WIDTH * PANEL_HEIGHT, countGrey(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 7));
  g.clear(15);
  g.fillRect(10, 10, 0, 5, 0); g.fillRect(10, 10, 5, -1, 0);
  TEST_ASSERT_EQUAL_INT(0, countBlack());
}

void test_round_rect() {
  g.fillRoundRect(100, 100, 60, 40, 10, 0);
  int a = countBlack();
  TEST_ASSERT_TRUE(a < 60 * 40);
  TEST_ASSERT_TRUE(a > 60 * 40 - 4 * 25);  // corners remove less than r*r each
  TEST_ASSERT_EQUAL_UINT8(15, g.get(100, 100));  // rounded corner
  TEST_ASSERT_EQUAL_UINT8(15, g.get(159, 139));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(130, 100));   // top edge middle
  TEST_ASSERT_EQUAL_UINT8(0, g.get(100, 120));   // left edge middle
  TEST_ASSERT_EQUAL_UINT8(0, g.get(159, 120));
  for (int y = 0; y < 40; y++) for (int x = 0; x < 30; x++) {  // mirror symmetry
    TEST_ASSERT_EQUAL_UINT8(g.get(100 + x, 100 + y), g.get(159 - x, 100 + y));
    TEST_ASSERT_EQUAL_UINT8(g.get(100 + x, 100 + y), g.get(100 + x, 139 - y));
  }
  g.clear(15);
  g.roundRect(100, 100, 60, 40, 10, 0, 2);
  TEST_ASSERT_EQUAL_UINT8(15, g.get(130, 120));  // hollow
  TEST_ASSERT_EQUAL_UINT8(0, g.get(130, 101));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(101, 120));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(100, 100));
  g.clear(15);
  g.fillRoundRect(-20, -20, 60, 60, 12, 0);  // clipped, must not crash
  g.fillRoundRect(PANEL_WIDTH - 20, PANEL_HEIGHT - 20, 60, 60, 12, 0);
  TEST_ASSERT_TRUE(countBlack() > 0);
}

void test_circles() {
  g.fillCircle(200, 200, 10, 0);
  TEST_ASSERT_INT_WITHIN(15, 346, countBlack());  // pi (r+0.5)^2
  TEST_ASSERT_EQUAL_UINT8(0, g.get(200, 200));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(210, 200));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(200, 190));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(211, 200));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(208, 208));
  g.clear(15);
  g.fillCircle(50, 50, 0, 0);
  TEST_ASSERT_EQUAL_INT(1, countBlack());
  g.clear(15);
  g.circle(200, 200, 20, 0, 1);
  TEST_ASSERT_EQUAL_UINT8(15, g.get(200, 200));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(220, 200));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(200, 180));
  TEST_ASSERT_INT_WITHIN(25, 126, countBlack());  // 2 pi r
  g.clear(15);
  g.circle(0, 0, 30, 0, 4);  // clipped at the corner
  g.fillCircle(PANEL_WIDTH, PANEL_HEIGHT, 30, 0);
  TEST_ASSERT_TRUE(countBlack() > 0);
}

void test_fill_polygon_concave() {
  // U shape 30x30 with a 10x20 notch cut from the top middle
  Pt u[] = {{50, 50}, {60, 50}, {60, 70}, {70, 70}, {70, 50}, {80, 50}, {80, 80}, {50, 80}};
  g.fillPolygon(u, 8, 0);
  TEST_ASSERT_EQUAL_INT(30 * 30 - 10 * 20, countBlack());
  TEST_ASSERT_EQUAL_UINT8(0, g.get(55, 60));    // left arm
  TEST_ASSERT_EQUAL_UINT8(0, g.get(75, 60));    // right arm
  TEST_ASSERT_EQUAL_UINT8(15, g.get(65, 60));   // notch stays empty
  TEST_ASSERT_EQUAL_UINT8(0, g.get(65, 75));    // base
  TEST_ASSERT_EQUAL_UINT8(15, g.get(49, 60));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(80, 60));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(65, 80));
}

void test_fill_polygon_orientation_clip_degenerate() {
  Pt cw[] = {{10, 10}, {30, 10}, {30, 30}, {10, 30}};
  Pt ccw[] = {{10, 10}, {10, 30}, {30, 30}, {30, 10}};
  g.fillPolygon(cw, 4, 0);
  TEST_ASSERT_EQUAL_INT(400, countBlack());
  g.clear(15);
  g.fillPolygon(ccw, 4, 0);
  TEST_ASSERT_EQUAL_INT(400, countBlack());
  g.clear(15);
  Pt big[] = {{-100, -100}, {2000, -100}, {2000, 2000}, {-100, 2000}};
  g.fillPolygon(big, 4, 0);
  TEST_ASSERT_EQUAL_INT(PANEL_WIDTH * PANEL_HEIGHT, countBlack());
  g.clear(15);
  Pt two[] = {{1, 1}, {5, 5}};
  g.fillPolygon(two, 2, 0);
  g.fillPolygon(nullptr, 5, 0);
  TEST_ASSERT_EQUAL_INT(0, countBlack());
}

void test_fill_triangle() {
  g.fillTriangle(100, 100, 140, 100, 100, 140, 0);  // right triangle, legs 40
  TEST_ASSERT_INT_WITHIN(30, 800, countBlack());
  TEST_ASSERT_EQUAL_UINT8(0, g.get(105, 105));
  TEST_ASSERT_EQUAL_UINT8(15, g.get(135, 135));
}

// Runs of black along a row: returns run count and fills lengths.
static int runsOnRow(int y, int x0, int x1, int* black, int* white) {
  int nb = 0, nw = 0, x = x0;
  while (x <= x1) {
    uint8_t v = g.get(x, y);
    int s = x;
    while (x <= x1 && g.get(x, y) == v) x++;
    if (v == 0) { if (nb < 32) black[nb++] = x - s; } else { if (nw < 32) white[nw++] = x - s; }
  }
  return nb;
}

void test_dashed_line_has_gaps() {
  g.dashedLine(10, 100, 200, 100, 2, 0, 10, 7);
  int black[32], white[32];
  int nb = runsOnRow(100, 0, 260, black, white);
  TEST_ASSERT_TRUE_MESSAGE(nb >= 10, "expected many dashes");
  TEST_ASSERT_EQUAL_INT(10, black[0]);
  TEST_ASSERT_EQUAL_INT(10, black[1]);
  TEST_ASSERT_EQUAL_INT(7, white[1]);  // [0] is the margin before the line
  TEST_ASSERT_EQUAL_INT(7, white[2]);
  TEST_ASSERT_TRUE(countBlack() < 190 * 2);          // gaps removed ink
  TEST_ASSERT_EQUAL_INT(2, countGrey(15, 0, 15, PANEL_HEIGHT - 1, 0));  // line is 2 px thick
}

void test_dashed_polyline_follows_corners() {
  Pt p[] = {{20, 20}, {120, 20}, {120, 120}};
  g.dashedPolyline(p, 3, 3, 0, 10, 7);
  TEST_ASSERT_EQUAL_UINT8(0, g.get(25, 20));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(120, 30));
  int black = countBlack();
  g.clear(15);
  g.polyline(p, 3, 3, 0);
  TEST_ASSERT_TRUE(black < countBlack() * 8 / 10);  // dashes use well under the solid ink
  TEST_ASSERT_TRUE(black > countBlack() / 3);
}

void test_polyline_joins() {
  Pt p[] = {{50, 50}, {100, 50}, {100, 100}};
  g.polyline(p, 3, 4, 0);
  TEST_ASSERT_EQUAL_UINT8(0, g.get(100, 50));   // corner
  TEST_ASSERT_EQUAL_UINT8(0, g.get(101, 51));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(75, 50));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(100, 75));
  g.clear(15);
  g.polyline(p, 3, 4, 0, CAP_BUTT);
  TEST_ASSERT_EQUAL_UINT8(0, g.get(101, 51));
  g.clear(15);
  g.polyline(nullptr, 3, 4, 0);
  g.polyline(p, 0, 4, 0);
  TEST_ASSERT_EQUAL_INT(0, countBlack());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pixel_nibble_packing);
  RUN_TEST(test_clear_and_mask);
  RUN_TEST(test_pixel_clipping_all_edges);
  RUN_TEST(test_hline_length_all_alignments);
  RUN_TEST(test_hline_vline_clipping);
  RUN_TEST(test_thin_line);
  RUN_TEST(test_thick_line_coverage);
  RUN_TEST(test_thick_diagonal_area_and_reverse);
  RUN_TEST(test_round_cap_extends_and_is_round);
  RUN_TEST(test_line_clipping_extreme);
  RUN_TEST(test_rect_edges);
  RUN_TEST(test_fillrect_exact_and_clipping);
  RUN_TEST(test_round_rect);
  RUN_TEST(test_circles);
  RUN_TEST(test_fill_polygon_concave);
  RUN_TEST(test_fill_polygon_orientation_clip_degenerate);
  RUN_TEST(test_fill_triangle);
  RUN_TEST(test_dashed_line_has_gaps);
  RUN_TEST(test_dashed_polyline_follows_corners);
  RUN_TEST(test_polyline_joins);
  return UNITY_END();
}
