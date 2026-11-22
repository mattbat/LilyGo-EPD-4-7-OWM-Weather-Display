// Portable bitmap font types. Binary layout matches what LilyGo's fontconvert.py emits
// (GFXglyph/GFXfont/UnicodeInterval), renamed so we do not clash with epd_driver.h.
// Glyph bitmaps are uncompressed 4-bit alpha (15 = fully covered), two pixels per byte,
// first pixel in the LOW nibble, each row padded to a whole byte.
#pragma once
#include <stdint.h>

struct EGlyph {
  uint16_t width, height;    // bitmap size in pixels
  uint16_t advance_x;
  int16_t left;              // bitmap x offset from the pen position
  int16_t top;               // bitmap y offset above the baseline
  uint32_t compressed_size;  // == byte size when uncompressed
  uint32_t data_offset;
};

struct EInterval {
  uint32_t first, last;      // inclusive code point range
  uint32_t offset;           // index of `first` in the glyph table
};

struct EFont {
  const uint8_t* bitmap;
  const EGlyph* glyph;
  const EInterval* intervals;
  uint32_t interval_count;
  uint8_t compressed;        // always 0 here
  int16_t advance_y;         // line height
  int16_t ascender;
  int16_t descender;         // negative
};
