// Portable weather icons (task 4). Outline style: 3 px strokes at scale 1 (minimum 2 px).
// Everything is drawn around (cx, cy) with the same proportions as the mockup SVG icons:
// at scale 1.0 an icon fits inside roughly x in [-62, +58], y in [-58, +50] around the centre.
#pragma once
#include "gfx.h"
#include "weather_data.h"
#include "palette.h"

// Stroke width used for a given icon scale: round(3 * scale), never below 2.
int iconStroke(float scale);

// Any Tempest icon key; unknown / out of range values draw a neutral cloud.
// `bg` is the colour behind the icon (only the moon cut-out uses it).
void drawIcon(Gfx& g, IconKey key, int cx, int cy, float scale, uint8_t bg = COL_PAPER);

// Standalone pieces (also used by screens). Sizes are in pixels unless noted.
void drawSun(Gfx& g, int cx, int cy, int r, int stroke, uint8_t fill = COL_PAPER, uint8_t ink = COL_K);
// Cloud outline from the mockup path; `scale` 1.0 is ~114 px wide.
void drawCloud(Gfx& g, float cx, float cy, float scale, uint8_t fill, int stroke, uint8_t ink = COL_K);
// Solid lightning bolt, scale 1.0 = 9 px wide, 20 px tall, centred on (cx, cy).
void drawBolt(Gfx& g, float cx, float cy, float scale, uint8_t grey);
// Crescent: disc of colour fg with a bg disc cut out of the upper right (flat backgrounds only).
void drawMoon(Gfx& g, int cx, int cy, int r, uint8_t fg, uint8_t bg);
// Battery outline 34x16 plus a 3 px nub; fill proportional to percent (0..100, clamped).
void drawBattery(Gfx& g, int x, int y, int percent, uint8_t grey = COL_K);
// `n` slanted rain strokes spaced 22*scale apart, centred on cx, top at cy (bottom at cy + 18*scale).
void drawDrops(Gfx& g, int cx, int cy, float scale, uint8_t grey, int n = 3);
// Small snow asterisk of arm length r centred on (cx, cy).
void drawSnowflake(Gfx& g, int cx, int cy, int r, int stroke, uint8_t grey);
