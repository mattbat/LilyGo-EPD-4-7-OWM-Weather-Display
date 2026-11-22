// Portable framebuffer, drawing primitives and text (task 2).
// 4-bit greyscale (0 = black, 15 = white), two pixels per byte, same layout as LilyGo's
// epd_draw_pixel / epd_draw_grayscale_image: pixel (x,y) lives in byte y*W/2 + x/2, even x in
// the LOW nibble, odd x in the HIGH nibble.
//
// Coordinates: integers are pixel indices. Everything is clipped to the panel.
// Thick shapes are centred on the given pixel (even thicknesses extend one extra pixel
// down/right). Float polygon vertices are continuous coordinates (pixel i covers [i, i+1)).
#pragma once
#include <stdint.h>
#include "config.h"
#include "fonts/efont.h"

enum Align { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum Cap { CAP_BUTT, CAP_SQUARE, CAP_ROUND };

struct Pt { float x, y; };

struct TextSize {
  int width;       // advance width incl. letter spacing between glyphs (none trailing)
  int ascent;      // font ascender (stable per font)
  int descent;     // font descender as a positive number
  int inkAscent;   // highest ink pixel above the baseline for this string
  int inkDescent;  // lowest ink pixel below the baseline for this string (>= 0)
};

// Test hook: smallest font line height (EFont::advance_y) passed to drawText since the last reset (0 = none yet).
int gfxMinTextAdvance();
void gfxMinTextAdvanceReset();

// UTF-8 decoding: returns next code point and advances p. Invalid input yields U+FFFD.
uint32_t utf8Next(const char*& p);
// Glyph lookup, nullptr if the font does not contain the code point.
const EGlyph* findGlyph(const EFont& font, uint32_t cp);
TextSize measureText(const EFont& font, const char* utf8, int letterSpacing = 0);

struct Gfx {
  uint8_t* fb = nullptr;   // PANEL_WIDTH * PANEL_HEIGHT / 2 bytes
  void begin(uint8_t* buffer);
  void clear(uint8_t grey = 15);
  void pixel(int x, int y, uint8_t grey);
  uint8_t get(int x, int y) const;

  // Lines
  void hline(int x, int y, int len, uint8_t grey);          // len pixels from x
  void vline(int x, int y, int len, uint8_t grey);
  void line(int x0, int y0, int x1, int y1, int thickness, uint8_t grey, Cap cap = CAP_ROUND);
  void polyline(const Pt* pts, int n, int thickness, uint8_t grey, Cap cap = CAP_ROUND);
  // Dash and gap lengths are measured along the path (pixels), pattern continues around corners.
  void dashedPolyline(const Pt* pts, int n, int thickness, uint8_t grey, float dash, float gap,
                      float phase = 0);
  void dashedLine(int x0, int y0, int x1, int y1, int thickness, uint8_t grey, float dash, float gap);

  // Rectangles (w x h pixels). Outlines of `thickness` are drawn INSIDE the bounds.
  void fillRect(int x, int y, int w, int h, uint8_t grey);
  void rect(int x, int y, int w, int h, uint8_t grey, int thickness = 1);
  void fillRoundRect(int x, int y, int w, int h, int r, uint8_t grey);
  void roundRect(int x, int y, int w, int h, int r, uint8_t grey, int thickness = 1);

  // Circles centred on pixel (cx, cy). fillCircle covers dist <= r; circle is a ring of `thickness`.
  void fillCircle(int cx, int cy, int r, uint8_t grey);
  void circle(int cx, int cy, int r, uint8_t grey, int thickness = 1);

  // Polygons (even-odd scanline fill, concave and self-intersecting shapes ok).
  void fillPolygon(const Pt* pts, int n, uint8_t grey);
  void fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint8_t grey);

  // Text. (x, y) is the pen position on the baseline; for CENTER/RIGHT x is the centre/right edge.
  // Glyph alpha is blended onto what is already in the framebuffer. Returns the text width.
  int drawText(int x, int y, const char* utf8, const EFont& font, uint8_t grey,
               Align align = ALIGN_LEFT, int letterSpacing = 0);

 private:
  void span(int y, int x0, int x1, uint8_t grey);          // [x0, x1) clipped
  void capsule(float ax, float ay, float bx, float by, float t, uint8_t grey, Cap cap);
  void disc(float cx, float cy, float r, uint8_t grey);
  void thinLine(int x0, int y0, int x1, int y1, uint8_t grey);
};
