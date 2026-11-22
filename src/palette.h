// E-ink grey palette shared by icons, charts and screens (4-bit greys, 0 = black, 15 = white).
// Maps the docs/mockups.html colours: K #1b1b1b, D #4a4a4a, M #888, L #bdbdbd, XL #d9d9d5, paper, forecast bg.
#pragma once
#include <stdint.h>

enum : uint8_t {
  COL_K     = 1,    // near black: ink, strokes, big numbers
  COL_D     = 4,    // dark grey: secondary text, axis labels
  COL_M     = 8,    // mid grey: bars, storm cloud, "no data" text
  COL_L     = 11,   // light grey: hairlines
  COL_XL    = 13,   // extra light: gridlines, area fills, cloud fill
  COL_FC    = 14,   // forecast half background
  COL_PAPER = 15    // background
};
