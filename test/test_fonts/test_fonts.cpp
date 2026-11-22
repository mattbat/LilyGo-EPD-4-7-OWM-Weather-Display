#include <unity.h>
#include <string.h>
#include "gfx.h"
#include "fonts/fonts.h"

static uint8_t fb[PANEL_WIDTH * PANEL_HEIGHT / 2];
static Gfx g;

void setUp() { g.begin(fb); g.clear(15); }
void tearDown() {}

struct Box { int x0, y0, x1, y1, n; };
static Box inkBox(uint8_t bg = 15) {
  Box b = {PANEL_WIDTH, PANEL_HEIGHT, -1, -1, 0};
  for (int y = 0; y < PANEL_HEIGHT; y++)
    for (int x = 0; x < PANEL_WIDTH; x++)
      if (g.get(x, y) != bg) {
        if (x < b.x0) b.x0 = x;
        if (y < b.y0) b.y0 = y;
        if (x > b.x1) b.x1 = x;
        if (y > b.y1) b.y1 = y;
        b.n++;
      }
  return b;
}

void test_utf8_decode() {
  const char* s = "A\xC2\xB0\xC2\xB7\xE2\x86\x91\xE2\x86\x93\xF0\x9F\x98\x80z";
  const char* p = s;
  TEST_ASSERT_EQUAL_HEX32(65, utf8Next(p));
  TEST_ASSERT_EQUAL_HEX32(0xB0, utf8Next(p));
  TEST_ASSERT_EQUAL_HEX32(0xB7, utf8Next(p));
  TEST_ASSERT_EQUAL_HEX32(0x2191, utf8Next(p));
  TEST_ASSERT_EQUAL_HEX32(0x2193, utf8Next(p));
  TEST_ASSERT_EQUAL_HEX32(0x1F600, utf8Next(p));
  TEST_ASSERT_EQUAL_HEX32(122, utf8Next(p));
  TEST_ASSERT_EQUAL_HEX32(0, utf8Next(p));
  TEST_ASSERT_EQUAL_PTR(s + strlen(s), p);
}

void test_utf8_invalid_does_not_hang_or_overrun() {
  const char* bad[] = {"\xFF", "\x80", "\xC2", "\xE2\x86", "\xC0\x80", "\xED\xA0\x80", "\xF8\x88\x80\x80\x80", "a\xE2"};
  for (const char* b : bad) {
    const char* p = b;
    int guard = 0;
    while (*p && guard++ < 10) utf8Next(p);
    TEST_ASSERT_TRUE(guard < 10);
    TEST_ASSERT_TRUE(p <= b + strlen(b));
  }
  const char* p = "\xFF";
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, utf8Next(p));
}

void test_glyph_lookup() {
  const EFont* all[] = {&AtkR20, &AtkR22, &AtkR44, &AtkB20, &AtkB22, &AtkB50, &AtkB76};
  const uint32_t need[] = {65, 122, 48, 57, 32, 126, 0xB0, 0xB7, 0x2191, 0x2193};
  for (const EFont* f : all) {
    TEST_ASSERT_TRUE(f->ascender > 0 && f->descender < 0 && f->advance_y > 0);
    for (uint32_t cp : need) {
      const EGlyph* gl = findGlyph(*f, cp);
      TEST_ASSERT_NOT_NULL(gl);
      TEST_ASSERT_TRUE(gl->advance_x > 0);
    }
    TEST_ASSERT_NULL(findGlyph(*f, 0x65E5));
    TEST_ASSERT_NULL(findGlyph(*f, 10));
  }
  const EFont* big[] = {&AtkB136, &AtkB170};  // digits, degree, minus, slash only
  for (const EFont* f : big) {
    for (uint32_t c = 48; c <= 57; c++) TEST_ASSERT_NOT_NULL(findGlyph(*f, c));
    TEST_ASSERT_NOT_NULL(findGlyph(*f, 0xB0));
    TEST_ASSERT_NOT_NULL(findGlyph(*f, 45));
    TEST_ASSERT_NOT_NULL(findGlyph(*f, 47));
    TEST_ASSERT_NULL(findGlyph(*f, 65));
  }
}

void test_sizes_grow() {
  TEST_ASSERT_TRUE(measureText(AtkB136, "1").width > 0);
  TEST_ASSERT_TRUE(measureText(AtkB170, "7").width > measureText(AtkB136, "7").width);
  TEST_ASSERT_TRUE(measureText(AtkB170, "7").inkAscent > measureText(AtkB136, "7").inkAscent);
  const EFont* order[] = {&AtkR20, &AtkR22, &AtkR24, &AtkR28, &AtkR44};
  for (int i = 1; i < 5; i++)
    TEST_ASSERT_TRUE(measureText(*order[i], "Wind 12").width >= measureText(*order[i - 1], "Wind 12").width);
  const EFont* bold[] = {&AtkB20, &AtkB22, &AtkB24, &AtkB28, &AtkB34, &AtkB46, &AtkB50, &AtkB56, &AtkB76};
  for (int i = 1; i < 9; i++)
    TEST_ASSERT_TRUE(measureText(*bold[i], "Wind 12").width >= measureText(*bold[i - 1], "Wind 12").width);
}

void test_measure_width_monotonic_and_spacing() {
  int w1 = measureText(AtkR22, "a").width;
  int w2 = measureText(AtkR22, "ab").width;
  int w3 = measureText(AtkR22, "abc").width;
  TEST_ASSERT_EQUAL_INT(0, measureText(AtkR22, "").width);
  TEST_ASSERT_TRUE(w1 > 0 && w2 > w1 && w3 > w2);
  TEST_ASSERT_EQUAL_INT(w3 + 2 * 4, measureText(AtkR22, "abc", 4).width);  // between glyphs only
  TEST_ASSERT_EQUAL_INT(w1, measureText(AtkR22, "a", 9).width);
  TEST_ASSERT_TRUE(measureText(AtkB22, "Wind").width > measureText(AtkR22, "Wind").width);
  TextSize m = measureText(AtkB46, "Hg");
  TEST_ASSERT_EQUAL_INT(AtkB46.ascender, m.ascent);
  TEST_ASSERT_EQUAL_INT(-AtkB46.descender, m.descent);
  TEST_ASSERT_TRUE(m.inkAscent > 0 && m.inkAscent <= m.ascent);
  TEST_ASSERT_TRUE(m.inkDescent > 0 && m.inkDescent <= m.descent);
  TEST_ASSERT_EQUAL_INT(0, measureText(AtkB46, "H").inkDescent);
  TEST_ASSERT_EQUAL_INT(0, measureText(AtkB46, nullptr).width);
}

void test_special_glyph_widths_utf8() {
  TEST_ASSERT_TRUE(measureText(AtkR20, "72\xC2\xB0").width > measureText(AtkR20, "72").width);
  TEST_ASSERT_TRUE(measureText(AtkR20, "\xE2\x86\x91").width > 0);
  TEST_ASSERT_TRUE(measureText(AtkR20, "\xC2\xB7").width > 0);
  const char* four = "\xC2\xB0\xC2\xB7\xE2\x86\x91\xE2\x86\x93";  // 4 code points, 10 bytes
  TEST_ASSERT_EQUAL_INT(measureText(AtkR20, four).width + 3 * 5, measureText(AtkR20, four, 5).width);
  g.drawText(50, 100, "\xC2\xB0", AtkR22, 0);
  TEST_ASSERT_TRUE(inkBox().n > 5);
  g.clear(15);
  g.drawText(50, 100, "\xE2\x86\x91", AtkR22, 0);
  TEST_ASSERT_TRUE(inkBox().n > 10);
}

void test_alignment_math() {
  const char* s = "HHHH";
  int w = measureText(AtkB22, s).width;
  g.drawText(200, 100, s, AtkB22, 0, ALIGN_LEFT);
  Box l = inkBox();
  TEST_ASSERT_TRUE(l.n > 0);
  TEST_ASSERT_TRUE(l.x0 >= 200 && l.x0 < 200 + 6);  // starts at the pen plus bearing
  TEST_ASSERT_TRUE(l.x1 < 200 + w);
  g.clear(15);
  int ret = g.drawText(200 + w / 2, 100, s, AtkB22, 0, ALIGN_CENTER);
  Box c = inkBox();
  TEST_ASSERT_EQUAL_INT(w, ret);
  TEST_ASSERT_EQUAL_INT(l.x0, c.x0);  // same pixels as the left-aligned run
  TEST_ASSERT_EQUAL_INT(l.x1, c.x1);
  g.clear(15);
  g.drawText(200 + w, 100, s, AtkB22, 0, ALIGN_RIGHT);
  Box r = inkBox();
  TEST_ASSERT_EQUAL_INT(l.x0, r.x0);
  TEST_ASSERT_EQUAL_INT(l.x1, r.x1);
  TEST_ASSERT_EQUAL_INT(l.n, r.n);
  TEST_ASSERT_TRUE(r.x1 < 200 + w);  // right aligned ink never passes the anchor
}

void test_baseline_placement() {
  g.drawText(100, 200, "H", AtkB46, 0);
  Box b = inkBox();
  TEST_ASSERT_EQUAL_INT(200 - 1, b.y1);  // flat-bottomed letter sits on the baseline
  TEST_ASSERT_EQUAL_INT(200 - measureText(AtkB46, "H").inkAscent, b.y0);
}

void test_letter_spacing_draw() {
  g.drawText(50, 100, "III", AtkB22, 0, ALIGN_LEFT, 0);
  Box a = inkBox();
  g.clear(15);
  g.drawText(50, 100, "III", AtkB22, 0, ALIGN_LEFT, 6);
  Box b = inkBox();
  TEST_ASSERT_EQUAL_INT(a.x0, b.x0);
  TEST_ASSERT_EQUAL_INT(a.x1 + 12, b.x1);
}

void test_missing_glyph_safe() {
  g.drawText(10, 100, "\xE6\x97\xA5\xE6\x9C\xAC", AtkR22, 0);  // CJK: falls back to question mark
  g.drawText(10, 200, "\xE6\x97\xA5 abc", AtkB136, 0);          // limited font: falls back to space
  g.drawText(10, 300, "\xFF\xFE\xC2", AtkR20, 0);               // invalid UTF-8
  g.drawText(10, 300, "", AtkR20, 0);
  g.drawText(10, 300, nullptr, AtkR20, 0);
  TEST_ASSERT_TRUE(measureText(AtkR22, "\xE6\x97\xA5").width == measureText(AtkR22, "?").width);
  TEST_ASSERT_TRUE(measureText(AtkB136, "A").width == measureText(AtkB136, " ").width);
  TEST_ASSERT_TRUE(inkBox().n > 0);
}

void test_text_clipped_at_edges() {
  g.drawText(-30, 30, "Clipped left", AtkB46, 0);
  g.drawText(PANEL_WIDTH - 30, 100, "Clipped right", AtkB46, 0);
  g.drawText(10, -5, "Top", AtkB46, 0);
  g.drawText(10, PANEL_HEIGHT + 10, "Bottom", AtkB46, 0);
  g.drawText(10, PANEL_HEIGHT - 3, "Edge", AtkB46, 0);
  g.drawText(-2000, 100, "Far", AtkB22, 0, ALIGN_RIGHT);
  TEST_ASSERT_TRUE(inkBox().n > 0);
  Gfx none;  // a Gfx without a framebuffer must not crash either
  TEST_ASSERT_EQUAL_INT(0, none.drawText(0, 0, "x", AtkR20, 0));
}

void test_antialiasing_blends() {
  g.drawText(50, 100, "Hamburg", AtkR22, 0);  // black on white: intermediate greys appear
  bool mid = false, black = false;
  for (int y = 70; y < 110; y++) for (int x = 50; x < 150; x++) {
    uint8_t v = g.get(x, y);
    if (v > 0 && v < 15) mid = true;
    if (v == 0) black = true;
  }
  TEST_ASSERT_TRUE(mid);
  TEST_ASSERT_TRUE(black);
  g.clear(15);  // white on black: text reaches white, edges blend
  g.fillRect(40, 60, 200, 60, 0);
  g.drawText(50, 100, "Hamburg", AtkB22, 15);
  mid = false;
  bool white = false;
  for (int y = 60; y < 120; y++) for (int x = 40; x < 240; x++) {
    uint8_t v = g.get(x, y);
    if (v > 0 && v < 15) mid = true;
    if (v == 15) white = true;
  }
  TEST_ASSERT_TRUE(mid);
  TEST_ASSERT_TRUE(white);
  TEST_ASSERT_EQUAL_UINT8(0, g.get(41, 61));
  g.clear(8);  // text on grey stays between background and text colour
  g.drawText(50, 100, "Hamburg", AtkB22, 2);
  for (int y = 70; y < 110; y++) for (int x = 50; x < 150; x++) {
    uint8_t v = g.get(x, y);
    TEST_ASSERT_TRUE(v >= 2 && v <= 8);
  }
}

void test_all_fonts_render() {
  const EFont* all[] = {&AtkR20, &AtkR22, &AtkR24, &AtkR28, &AtkR44,
                        &AtkB20, &AtkB22, &AtkB24, &AtkB28, &AtkB34, &AtkB46, &AtkB50, &AtkB56, &AtkB76,
                        &AtkB136, &AtkB170};
  for (const EFont* f : all) {
    g.clear(15);
    g.drawText(20, 300, "-12\xC2\xB0/8", *f, 0);
    TEST_ASSERT_TRUE(inkBox().n > 20);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_utf8_decode);
  RUN_TEST(test_utf8_invalid_does_not_hang_or_overrun);
  RUN_TEST(test_glyph_lookup);
  RUN_TEST(test_sizes_grow);
  RUN_TEST(test_measure_width_monotonic_and_spacing);
  RUN_TEST(test_special_glyph_widths_utf8);
  RUN_TEST(test_alignment_math);
  RUN_TEST(test_baseline_placement);
  RUN_TEST(test_letter_spacing_draw);
  RUN_TEST(test_missing_glyph_safe);
  RUN_TEST(test_text_clipped_at_edges);
  RUN_TEST(test_antialiasing_blends);
  RUN_TEST(test_all_fonts_render);
  return UNITY_END();
}
