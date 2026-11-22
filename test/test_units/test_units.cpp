#include <unity.h>
#include <math.h>
#include <string.h>
#include "units.h"

void setUp() {}
void tearDown() {}

void test_c_to_f() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, cToF(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 212.0f, cToF(100.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.0f, cToF(-40.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 98.6f, cToF(37.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 68.36f, cToF(20.2f));
}
void test_ms_to_mph() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, msToMph(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.2369f, msToMph(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.369f, msToMph(10.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 17.22f, msToMph(7.7f));
}
void test_mm_to_in() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, mmToIn(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, mmToIn(25.4f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0394f, mmToIn(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 39.37f, mmToIn(1000.0f));
}
void test_mb_to_inhg() {
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 29.92f, mbToInHg(1013.25f));
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 28.6175f, mbToInHg(969.1f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, mbToInHg(0.0f));
}
void test_km_to_mi() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, kmToMi(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6214f, kmToMi(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.43f, kmToMi(20.0f));
}
void test_compass_cardinals() {
  TEST_ASSERT_EQUAL_STRING("N", degToCompass(0));
  TEST_ASSERT_EQUAL_STRING("E", degToCompass(90));
  TEST_ASSERT_EQUAL_STRING("S", degToCompass(180));
  TEST_ASSERT_EQUAL_STRING("W", degToCompass(270));
  TEST_ASSERT_EQUAL_STRING("SSE", degToCompass(157));
  TEST_ASSERT_EQUAL_STRING("NNE", degToCompass(22.5f));
  TEST_ASSERT_EQUAL_STRING("NW", degToCompass(315));
}
void test_compass_boundaries() {
  TEST_ASSERT_EQUAL_STRING("N", degToCompass(11.24f));
  TEST_ASSERT_EQUAL_STRING("NNE", degToCompass(11.26f));
  TEST_ASSERT_EQUAL_STRING("N", degToCompass(348.76f));
  TEST_ASSERT_EQUAL_STRING("NNW", degToCompass(348.74f));
  TEST_ASSERT_EQUAL_STRING("N", degToCompass(359.9f));
}
void test_compass_wrap_and_invalid() {
  TEST_ASSERT_EQUAL_STRING("N", degToCompass(360));
  TEST_ASSERT_EQUAL_STRING("E", degToCompass(450));
  TEST_ASSERT_EQUAL_STRING("NNW", degToCompass(-20));
  TEST_ASSERT_EQUAL_STRING("W", degToCompass(-90));
  TEST_ASSERT_EQUAL_STRING("--", degToCompass(NAN));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_c_to_f);
  RUN_TEST(test_ms_to_mph);
  RUN_TEST(test_mm_to_in);
  RUN_TEST(test_mb_to_inhg);
  RUN_TEST(test_km_to_mi);
  RUN_TEST(test_compass_cardinals);
  RUN_TEST(test_compass_boundaries);
  RUN_TEST(test_compass_wrap_and_invalid);
  return UNITY_END();
}