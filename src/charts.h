// Portable chart helpers (task 4): screen independent, data driven.
// Series are plain float arrays where NAN means "missing". A ChartRect is the plotting area in
// pixels; value ranges come from an AxisRange (see autoRange). Pure maths helpers do not need a
// Gfx so they can be unit tested; the draw* functions only combine them with Gfx primitives.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "gfx.h"
#include "palette.h"

#define CHART_MAX_POINTS 128   // longest series a line/sparkline can draw (longer input is truncated)

struct ChartRect { int x, y, w, h; };
struct AxisRange { float min, max; bool valid; };   // valid=false: no data (range is a harmless 0..2*step)

// ---------------------------------------------------------------------------------------------
// Pure maths
// ---------------------------------------------------------------------------------------------
// Smallest/largest finite value; false (outputs untouched) when there is none.
bool dataMinMax(const float* v, int n, float* lo, float* hi);
// Round [lo - pad, hi + pad] outward to multiples of `step`. Always at least `step` tall.
AxisRange rangeFromMinMax(float lo, float hi, float step = 5, float pad = 3);
AxisRange autoRange(const float* v, int n, float step = 5, float pad = 3);
// Range covering two series (past + forecast), either may be null/empty.
AxisRange autoRange2(const float* a, int na, const float* b, int nb, float step = 5, float pad = 3);
// Gridline spacing for a range: 10 when the span is over 20, else 5.
float gridStepFor(const AxisRange& r);
// Gridline values strictly inside the range, at multiples of `step`. Returns how many were written.
int gridValues(const AxisRange& r, float step, float* out, int maxOut);

// Pixel x of sample i of n spread over [x, x + w] (n == 1 -> left edge).
float mapX(int i, int n, int x, int w);
// Pixel y of value v (vmin at the bottom y + h, vmax at the top y). Result clamped to [y, y + h].
float mapY(float v, float vmin, float vmax, int y, int h);

// Index of the first max/min finite sample; -1 when there is none, or when it lies within
// `endMargin` samples of either end (the "skip at the ends" rule: 0 = never skip).
int hiLoIndex(const float* v, int n, bool wantMax, int endMargin = 2);

// Bars: scale maximum with a floor (so a tiny shower is not drawn full height).
float barScaleMax(const float* v, int n, float floorMax = 0.05f);
// Height in pixels for value v: 0 for NAN / <= 0, otherwise at least minHeight, at most h.
int barHeight(float v, float scaleMax, int h, int minHeight = 3);
// Pixel span of bar i of n inside [x, x + w] leaving `gap` pixels between bars.
void barSlot(int i, int n, int x, int w, int gap, int* bx, int* bw);

// Sparkline range: finite min/max with 10% headroom; flat data gets +-1.
AxisRange sparkRange(const float* v, int n);

// Text helpers. hour24 may be any integer (wrapped into 0..23).
void formatHourLabel(char* buf, size_t n, int hour24);                 // "12 AM", "2 PM"
void formatDegrees(char* buf, size_t n, float v);                       // "72°", "-5°", "0°" (never "-0°")
int  hourOfDay(uint32_t epoch, int tzOffsetSec);                        // local hour from UTC epoch + fixed offset
#define CHART_TZ_LOCAL  0x7fffffff                                      // tzOffsetSec value: use the C library's localtime()
int  localHourOfEpoch(uint32_t epoch, int tzOffsetSec = CHART_TZ_LOCAL);

// Time axis: slot i (0..slots) is startEpoch + i hours. Labels every `step` slots, slot `nowSlot` reads "NOW".
struct HourAxis {
  uint32_t startEpoch;   // epoch of slot 0 (top of an hour)
  int slots;             // e.g. 48
  int step;              // label every N slots, e.g. 6
  int nowSlot;           // slot drawn as "NOW" (-1 for none)
  int tzOffsetSec;       // CHART_TZ_LOCAL or fixed offset
};
struct HourLabel { int slot; bool isNow; char text[8]; };
int hourAxisLabels(const HourAxis& ax, HourLabel* out, int maxOut);

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------
struct LineStyle {
  int thickness = 3;
  uint8_t grey = COL_K;
  bool dashed = false;
  float dash = 10, gap = 7;
  bool fillArea = false;           // fill between the line and the bottom of the rect
  uint8_t fillGrey = COL_XL;
};
// NAN samples break the line (and area) into separate runs; a lone sample becomes a dot.
void drawLineSeries(Gfx& g, const float* v, int n, const ChartRect& r, const AxisRange& ax, const LineStyle& st);
// Pixel position of sample i; false when i is out of range or NAN.
bool seriesPoint(const float* v, int n, int i, const ChartRect& r, const AxisRange& ax, float* px, float* py);
// Filled dot on the last finite sample ("now" dot). Returns false when there is none.
bool drawEndDot(Gfx& g, const float* v, int n, const ChartRect& r, const AxisRange& ax, int radius = 7, uint8_t grey = COL_K);
// Dot plus "72°" label on the series max (above) or min (below). Skipped (false) per hiLoIndex().
// The label is kept inside the rect (and the rect's horizontal extent).
bool drawHiLoMarker(Gfx& g, const float* v, int n, const ChartRect& r, const AxisRange& ax, bool wantMax,
                    const EFont& font, int endMargin = 2, int dotRadius = 4, uint8_t grey = COL_K);
// Horizontal gridlines at multiples of `step` (0 = gridStepFor) with right-aligned axis labels left of the rect.
void drawGrid(Gfx& g, const ChartRect& r, const AxisRange& ax, const EFont& font, float step = 0,
              uint8_t lineGrey = COL_XL, uint8_t textGrey = COL_D);
// Vertical "now" divider.
void drawNowLine(Gfx& g, int x, int y0, int y1, int thickness = 2, uint8_t grey = COL_K);

struct BarStyle {
  uint8_t grey = COL_K;
  int gap = 4;
  int minHeight = 3;
  float floorMax = 0.05f;      // scale maximum never goes below this (inches for rain)
  bool baseline = true;        // hairline along the bottom
  uint8_t baselineGrey = COL_L;
};
// Bars that grow up from the bottom of the rect. scaleMax <= 0 auto-scales from the data (with floorMax).
void drawBars(Gfx& g, const float* v, int n, const ChartRect& r, float scaleMax, const BarStyle& st);
// Probability bars: 0..100 percent of the rect height (NAN skipped). Draws with minHeight 2.
void drawProbBars(Gfx& g, const float* pct, int n, const ChartRect& r, uint8_t grey = COL_M, int gap = 4);
// Small trend line with end dot for a stat tile. Optional area fill (fillGrey < 0 for none), baseline hairline.
void drawSparkline(Gfx& g, const float* v, int n, const ChartRect& r, int thickness = 3,
                   int fillGrey = -1, bool baseline = true, int dotRadius = 4);
// One slot per entry of `counts` across rect.w: bolt + count below each non-zero slot. All zero ->
// emptyMsg in grey (left aligned, two slots in). Row height is about 34 px starting at r.y.
bool drawLightningRow(Gfx& g, const uint16_t* counts, int n, const ChartRect& r,
                      const EFont& countFont, const EFont& msgFont, const char* emptyMsg = "no lightning in 24 h");
// Time axis labels with baseline y: regular grey text, bold black "NOW"; first label left aligned,
// last right aligned, the rest centred, so nothing leaves [x, x + w].
void drawHourAxis(Gfx& g, int x, int w, int baselineY, const HourAxis& ax, const EFont& regular, const EFont& bold);
