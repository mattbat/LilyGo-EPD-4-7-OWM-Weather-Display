// Portable framebuffer and drawing primitives (task 2). 4-bit greyscale, two pixels per byte.
#pragma once
#include <stdint.h>
#include "config.h"

enum Align { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

struct Gfx {
  uint8_t* fb = nullptr;   // PANEL_WIDTH * PANEL_HEIGHT / 2 bytes
  void begin(uint8_t* buffer);
  void clear(uint8_t grey = 15);
  void pixel(int x, int y, uint8_t grey);
  uint8_t get(int x, int y) const;
  // TODO(task 2): hline, vline, thick line, rect, fillRect, roundRect, circle, polyline,
  // dashed polyline, polygon fill, text + alignment + letter-spacing
};
