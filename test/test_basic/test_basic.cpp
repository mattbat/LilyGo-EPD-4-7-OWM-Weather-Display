#include <unity.h>
#include "units.h"
#include "gfx.h"

void setUp() {}
void tearDown() {}

void test_c_to_f() { TEST_ASSERT_FLOAT_WITHIN(0.01f, 212.0f, cToF(100.0f)); }
void test_pixel_roundtrip() {
  static uint8_t fb[PANEL_WIDTH * PANEL_HEIGHT / 2];
  Gfx g; g.begin(fb); g.clear(15);
  g.pixel(3, 4, 5); g.pixel(4, 4, 9);
  TEST_ASSERT_EQUAL_UINT8(5, g.get(3, 4));
  TEST_ASSERT_EQUAL_UINT8(9, g.get(4, 4));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_c_to_f);
  RUN_TEST(test_pixel_roundtrip);
  return UNITY_END();
}
