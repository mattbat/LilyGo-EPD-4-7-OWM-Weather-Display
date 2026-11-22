#include <unity.h>
#include <math.h>
#include <initializer_list>
#include <string.h>
#include <stdio.h>
#include "icons.h"

static uint8_t fb[PANEL_WIDTH * PANEL_HEIGHT / 2];
static Gfx g;

void setUp() { g.begin(fb); g.clear(15); }
void tearDown() {}

static int countNot(int x0, int y0, int x1, int y1, uint8_t bg) {
  int n = 0;
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (g.get(x, y) != bg) n++;
  return n;
}
static int countDark(int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (g.get(x, y) <= 2) n++;
  return n;
}

static const int CX = 480, CY = 270;

void test_stroke_width() {
  TEST_ASSERT_EQUAL_INT(3, iconStroke(1.0f));
  TEST_ASSERT_EQUAL_INT(2, iconStroke(0.55f));   // minimum 2
  TEST_ASSERT_EQUAL_INT(2, iconStroke(0.3f));
  TEST_ASSERT_EQUAL_INT(6, iconStroke(2.0f));
}

// Every key (including values past the enum) draws ink inside a sane box and nothing outside it.
void test_every_icon_draws_inside_box() {
  for (int k = 0; k <= ICON_WINDY + 3; k++) {
    for (float sc : {0.55f, 1.0f, 1.5f}) {
      g.clear(15);
      drawIcon(g, (IconKey)k, CX, CY, sc);
      int bx0 = CX - (int)(66 * sc) - 4, bx1 = CX + (int)(62 * sc) + 4;
      int by0 = CY - (int)(62 * sc) - 4, by1 = CY + (int)(52 * sc) + 4;
      char msg[64];
      snprintf(msg, sizeof msg, "key %d scale %.2f outside box", k, sc);
      int total = countNot(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15);
      int inside = countNot(bx0, by0, bx1, by1, 15);
      TEST_ASSERT_EQUAL_INT_MESSAGE(total, inside, msg);
      snprintf(msg, sizeof msg, "key %d scale %.2f draws too little", k, sc);
      TEST_ASSERT_TRUE_MESSAGE(countDark(bx0, by0, bx1, by1) > (int)(120 * sc), msg);
    }
  }
}

void test_icon_footprint_is_substantial_at_scale_one() {
  // the icon should fill a good part of its ~120x105 box (not a stray dot)
  for (int k = 0; k <= ICON_WINDY; k++) {
    g.clear(15);
    drawIcon(g, (IconKey)k, CX, CY, 1.0f);
    int minX = 9999, maxX = -1, minY = 9999, maxY = -1;
    for (int y = 0; y < PANEL_HEIGHT; y++)
      for (int x = 0; x < PANEL_WIDTH; x++)
        if (g.get(x, y) != 15) { if (x < minX) minX = x; if (x > maxX) maxX = x; if (y < minY) minY = y; if (y > maxY) maxY = y; }
    TEST_ASSERT_TRUE_MESSAGE(maxX - minX >= 50, "icon too narrow");
    TEST_ASSERT_TRUE_MESSAGE(maxY - minY >= 40, "icon too short");
  }
}

void test_unknown_is_neutral_cloud() {
  drawIcon(g, ICON_UNKNOWN, CX, CY, 1.0f);
  static uint8_t fb2[PANEL_WIDTH * PANEL_HEIGHT / 2];
  Gfx g2; g2.begin(fb2); g2.clear(15);
  drawIcon(g2, ICON_CLOUDY, CX, CY, 1.0f);
  TEST_ASSERT_EQUAL_MEMORY(fb2, fb, sizeof fb);
  g.clear(15); g2.clear(15);
  drawIcon(g, (IconKey)200, CX, CY, 1.0f);
  drawIcon(g2, ICON_UNKNOWN, CX, CY, 1.0f);
  TEST_ASSERT_EQUAL_MEMORY(fb2, fb, sizeof fb);
}

void test_scale_range_no_crash() {
  for (int k = 0; k <= ICON_WINDY; k++)
    for (float sc = 0.3f; sc <= 2.001f; sc += 0.1f) {
      g.clear(15);
      drawIcon(g, (IconKey)k, CX, CY, sc);
      TEST_ASSERT_TRUE(countNot(CX - 140, CY - 140, CX + 140, CY + 120, 15) > 0);
    }
  // extreme / clipped positions must not crash or write out of bounds
  for (int k = 0; k <= ICON_WINDY; k++) {
    drawIcon(g, (IconKey)k, 0, 0, 1.0f);
    drawIcon(g, (IconKey)k, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 2.0f);
    drawIcon(g, (IconKey)k, -500, 900, 1.5f);
    drawIcon(g, (IconKey)k, CX, CY, 0.0f);
    drawIcon(g, (IconKey)k, CX, CY, -3.0f);
    drawIcon(g, (IconKey)k, CX, CY, 25.0f);
  }
}

void test_variants_differ() {
  // day/night and precipitation variants should not be identical drawings
  static uint8_t a[PANEL_WIDTH * PANEL_HEIGHT / 2];
  drawIcon(g, ICON_PARTLY_DAY, CX, CY, 1.0f);
  memcpy(a, fb, sizeof a);
  g.clear(15);
  drawIcon(g, ICON_PARTLY_NIGHT, CX, CY, 1.0f);
  TEST_ASSERT_TRUE(memcmp(a, fb, sizeof a) != 0);
  g.clear(15);
  drawIcon(g, ICON_RAINY, CX, CY, 1.0f);
  memcpy(a, fb, sizeof a);
  g.clear(15);
  drawIcon(g, ICON_THUNDERSTORM, CX, CY, 1.0f);
  TEST_ASSERT_TRUE(memcmp(a, fb, sizeof a) != 0);
}

void test_bolt_moon_battery_drops() {
  drawBolt(g, 100, 100, 1.0f, 0);
  TEST_ASSERT_EQUAL_UINT8(0, g.get(100, 100));   // centre of the zig-zag body is solid
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, 90, PANEL_HEIGHT - 1, 15));
  TEST_ASSERT_EQUAL_INT(0, countNot(110, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15));

  g.clear(15);
  drawMoon(g, 200, 200, 20, 1, 15);
  TEST_ASSERT_EQUAL_UINT8(1, g.get(185, 200));   // crescent body on the lower-left
  TEST_ASSERT_EQUAL_UINT8(15, g.get(212, 192));  // bite taken out of the upper right
  g.clear(7);
  drawMoon(g, 200, 200, 20, 1, 7);
  TEST_ASSERT_EQUAL_UINT8(7, g.get(212, 192));   // bite uses the supplied background

  g.clear(15);
  drawBattery(g, 100, 100, 0);
  TEST_ASSERT_EQUAL_UINT8(15, g.get(110, 108));  // empty inside
  TEST_ASSERT_EQUAL_UINT8(1, g.get(100 + 17, 100));  // outline (default grey COL_K)
  g.clear(15);
  drawBattery(g, 100, 100, 100, 0);
  TEST_ASSERT_EQUAL_UINT8(0, g.get(110, 108));
  TEST_ASSERT_EQUAL_UINT8(0, g.get(100 + 35, 108));  // nub
  g.clear(15);
  drawBattery(g, 100, 100, 50, 0);
  TEST_ASSERT_EQUAL_UINT8(0, g.get(105, 108));       // left half filled
  TEST_ASSERT_EQUAL_UINT8(15, g.get(128, 108));      // right half empty
  g.clear(15);
  drawBattery(g, 100, 100, 250);                     // clamped
  drawBattery(g, 200, 100, -20);
  TEST_ASSERT_TRUE(countNot(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 15) > 0);

  g.clear(15);
  drawDrops(g, 300, 300, 1.0f, 0, 3);
  TEST_ASSERT_EQUAL_INT(0, countNot(0, 0, 260, PANEL_HEIGHT - 1, 15));   // spans 22 px each side of cx
  TEST_ASSERT_TRUE(countNot(250, 295, 350, 325, 15) > 30);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_stroke_width);
  RUN_TEST(test_every_icon_draws_inside_box);
  RUN_TEST(test_icon_footprint_is_substantial_at_scale_one);
  RUN_TEST(test_unknown_is_neutral_cloud);
  RUN_TEST(test_scale_range_no_crash);
  RUN_TEST(test_variants_differ);
  RUN_TEST(test_bolt_moon_battery_drops);
  return UNITY_END();
}
