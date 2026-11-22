// E-ink grey palette shared by icons, charts and screens (4-bit greys, 0 = black, 15 = white).
//
// MEASURED PANEL RESPONSE (photo of the real LilyGo T5 4.7" panel): the grey ramp is strongly
// non-linear and much darker than on screen.
//   15 paper, 14 light grey (reads as light, fine for large fills),
//   13 medium-dark grey (about what a mockup "mid grey" looks like), 12..6 all look nearly black,
//   8 and below are practically indistinguishable from black.
// Rules: only 14 (light) and 13 (medium) may be used for fills; never rely on a grey between 2 and 12 to be
// told apart from black. Text is black (COL_K) or at most COL_D. 1 px hairlines may use COL_L.
#pragma once
#include <stdint.h>

enum : uint8_t {
  COL_K     = 1,    // near black: ink, strokes, text, big numbers
  COL_D     = 4,    // dark grey: at most this dark-grey for small text (prefer COL_K)
  COL_M     = 13,   // medium: storm/sleet cloud fill (the darkest fill that still reads as grey)
  COL_L     = 11,   // hairlines only (1 px gridlines, rules); too dark for fills
  COL_XL    = 14,   // light grey fill: measured area, cloud fill, "light" things
  COL_FC    = 14,   // light grey (kept for callers that want a tint; the forecast half itself is paper)
  COL_PAPER = 15    // background
};
