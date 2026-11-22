#include "charts.h"
#include "icons.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------------------------------------
// Pure maths
// ---------------------------------------------------------------------------------------------
bool dataMinMax(const float* v, int n, float* lo, float* hi) {
  bool any = false;
  float a = 0, b = 0;
  for (int i = 0; v && i < n; i++) {
    if (!isfinite(v[i])) continue;
    if (!any) { a = b = v[i]; any = true; }
    else { if (v[i] < a) a = v[i]; if (v[i] > b) b = v[i]; }
  }
  if (any) { *lo = a; *hi = b; }
  return any;
}

AxisRange rangeFromMinMax(float lo, float hi, float step, float pad) {
  if (step <= 0) step = 1;
  if (!isfinite(lo) || !isfinite(hi)) return {0, 2 * step, false};
  if (hi < lo) { float t = lo; lo = hi; hi = t; }
  float mn = floorf((lo - pad) / step) * step;
  float mx = ceilf((hi + pad) / step) * step;
  if (mx - mn < step) mx = mn + step;
  return {mn, mx, true};
}

AxisRange autoRange(const float* v, int n, float step, float pad) {
  float lo, hi;
  if (!dataMinMax(v, n, &lo, &hi)) return rangeFromMinMax(NAN, NAN, step, pad);
  return rangeFromMinMax(lo, hi, step, pad);
}

AxisRange autoRange2(const float* a, int na, const float* b, int nb, float step, float pad) {
  float lo = 0, hi = 0, l2, h2;
  bool any = dataMinMax(a, na, &lo, &hi);
  if (dataMinMax(b, nb, &l2, &h2)) {
    if (!any) { lo = l2; hi = h2; any = true; }
    else { if (l2 < lo) lo = l2; if (h2 > hi) hi = h2; }
  }
  if (!any) return rangeFromMinMax(NAN, NAN, step, pad);
  return rangeFromMinMax(lo, hi, step, pad);
}

float gridStepFor(const AxisRange& r) { return (r.max - r.min) > 20 ? 10.0f : 5.0f; }

int gridValues(const AxisRange& r, float step, float* out, int maxOut) {
  if (step <= 0 || !(r.max > r.min)) return 0;
  int n = 0;
  float v = ceilf(r.min / step) * step;
  if (v <= r.min) v += step;
  for (; v < r.max && n < maxOut; v += step) out[n++] = v;
  return n;
}

float mapX(int i, int n, int x, int w) {
  if (n <= 1) return (float)x;
  return x + (float)i / (float)(n - 1) * w;
}

float mapY(float v, float vmin, float vmax, int y, int h) {
  float t = (vmax > vmin) ? (v - vmin) / (vmax - vmin) : 0.5f;
  float py = y + h - t * h;
  if (py < y) py = (float)y;
  if (py > y + h) py = (float)(y + h);
  return py;
}

int hiLoIndex(const float* v, int n, bool wantMax, int endMargin) {
  int idx = -1;
  for (int i = 0; v && i < n; i++) {
    if (!isfinite(v[i])) continue;
    if (idx < 0 || (wantMax ? v[i] > v[idx] : v[i] < v[idx])) idx = i;
  }
  if (idx < 0) return -1;
  if (idx < endMargin || idx >= n - endMargin) return -1;
  return idx;
}

float barScaleMax(const float* v, int n, float floorMax) {
  float lo, hi;
  if (!dataMinMax(v, n, &lo, &hi)) return floorMax;
  return hi > floorMax ? hi : floorMax;
}

int barHeight(float v, float scaleMax, int h, int minHeight) {
  if (!isfinite(v) || v <= 0 || !(scaleMax > 0) || h <= 0) return 0;
  int px = (int)lroundf(v / scaleMax * h);
  if (px < minHeight) px = minHeight;
  if (px > h) px = h;
  return px;
}

void barSlot(int i, int n, int x, int w, int gap, int* bx, int* bw) {
  if (n < 1) n = 1;
  int x0 = (int)lroundf(x + (float)i * w / n + gap * 0.5f);
  int x1 = (int)lroundf(x + (float)(i + 1) * w / n - gap * 0.5f);
  if (x1 <= x0) x1 = x0 + 1;
  *bx = x0;
  *bw = x1 - x0;
}

AxisRange sparkRange(const float* v, int n) {
  float lo, hi;
  if (!dataMinMax(v, n, &lo, &hi)) return {0, 1, false};
  float pad = (hi - lo) * 0.1f;
  if (pad <= 0) pad = 1;
  return {lo - pad, hi + pad, true};
}

void formatHourLabel(char* buf, size_t n, int hour24) {
  int h = ((hour24 % 24) + 24) % 24;
  snprintf(buf, n, "%d %s", h % 12 == 0 ? 12 : h % 12, h < 12 ? "AM" : "PM");
}

void formatDegrees(char* buf, size_t n, float v) {
  int r = (int)lroundf(v);
  snprintf(buf, n, "%d\xC2\xB0", r);   // lroundf(-0.4) is 0, so "-0" cannot occur
}

int hourOfDay(uint32_t epoch, int tzOffsetSec) {
  int64_t local = (int64_t)epoch + tzOffsetSec;
  int64_t h = (local / 3600) % 24;
  if (h < 0) h += 24;
  return (int)h;
}

int localHourOfEpoch(uint32_t epoch, int tzOffsetSec) {
  if (tzOffsetSec != CHART_TZ_LOCAL) return hourOfDay(epoch, tzOffsetSec);
  time_t t = (time_t)epoch;
  struct tm* tmv = localtime(&t);
  return tmv ? tmv->tm_hour : hourOfDay(epoch, 0);
}

int hourAxisLabels(const HourAxis& ax, HourLabel* out, int maxOut) {
  int step = ax.step < 1 ? 1 : ax.step;
  int n = 0;
  for (int i = 0; i <= ax.slots && n < maxOut; i++) {
    if (i % step != 0 && i != ax.nowSlot) continue;
    HourLabel& l = out[n++];
    l.slot = i;
    l.isNow = (i == ax.nowSlot);
    if (l.isNow) { strcpy(l.text, "NOW"); continue; }
    formatHourLabel(l.text, sizeof l.text, localHourOfEpoch(ax.startEpoch + (uint32_t)i * 3600u, ax.tzOffsetSec));
  }
  return n;
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------
namespace {
// Next run of finite samples at or after `from`; false when none.
bool nextRun(const float* v, int n, int from, int* start, int* len) {
  int i = from;
  while (i < n && !isfinite(v[i])) i++;
  if (i >= n) return false;
  int j = i;
  while (j < n && isfinite(v[j])) j++;
  *start = i;
  *len = j - i;
  return true;
}
}  // namespace

bool seriesPoint(const float* v, int n, int i, const ChartRect& r, const AxisRange& ax, float* px, float* py) {
  if (!v || i < 0 || i >= n || !isfinite(v[i])) return false;
  *px = mapX(i, n, r.x, r.w);
  *py = mapY(v[i], ax.min, ax.max, r.y, r.h);
  return true;
}

void drawLineSeries(Gfx& g, const float* v, int n, const ChartRect& r, const AxisRange& ax, const LineStyle& st) {
  if (!v || n <= 0) return;
  int total = n;
  if (total > CHART_MAX_POINTS) total = CHART_MAX_POINTS;
  float base = (float)(r.y + r.h);
  int from = 0, start, len;
  while (from < total && nextRun(v, total, from, &start, &len)) {
    from = start + len;
    Pt pts[CHART_MAX_POINTS + 2];
    for (int k = 0; k < len; k++) {
      float px, py;
      seriesPoint(v, n, start + k, r, ax, &px, &py);
      pts[k] = {px, py};
    }
    if (len == 1) {
      int rad = st.thickness / 2;
      g.fillCircle((int)lroundf(pts[0].x), (int)lroundf(pts[0].y), rad < 1 ? 1 : rad, st.grey);
      continue;
    }
    if (st.fillArea) {
      Pt poly[CHART_MAX_POINTS + 2];
      for (int k = 0; k < len; k++) poly[k] = pts[k];
      poly[len] = {pts[len - 1].x, base};
      poly[len + 1] = {pts[0].x, base};
      g.fillPolygon(poly, len + 2, st.fillGrey);
    }
    if (st.dashed) g.dashedPolyline(pts, len, st.thickness, st.grey, st.dash, st.gap);
    else g.polyline(pts, len, st.thickness, st.grey, CAP_ROUND);
  }
}

bool drawEndDot(Gfx& g, const float* v, int n, const ChartRect& r, const AxisRange& ax, int radius, uint8_t grey) {
  for (int i = n - 1; i >= 0; i--) {
    float px, py;
    if (seriesPoint(v, n, i, r, ax, &px, &py)) {
      g.fillCircle((int)lroundf(px), (int)lroundf(py), radius, grey);
      return true;
    }
  }
  return false;
}

bool drawHiLoMarker(Gfx& g, const float* v, int n, const ChartRect& r, const AxisRange& ax, bool wantMax,
                    const EFont& font, int endMargin, int dotRadius, uint8_t grey) {
  int idx = hiLoIndex(v, n, wantMax, endMargin);
  if (idx < 0) return false;
  float fx, fy;
  if (!seriesPoint(v, n, idx, r, ax, &fx, &fy)) return false;
  int px = (int)lroundf(fx), py = (int)lroundf(fy);
  g.fillCircle(px, py, dotRadius, grey);

  char text[16];
  formatDegrees(text, sizeof text, v[idx]);
  TextSize ts = measureText(font, text);
  int cap = measureText(font, "0").inkAscent;
  int baseline = wantMax ? py - dotRadius - 8 : py + dotRadius + 6 + cap;
  if (baseline - ts.inkAscent < r.y) baseline = r.y + ts.inkAscent;
  if (baseline + ts.inkDescent > r.y + r.h) baseline = r.y + r.h - ts.inkDescent;
  int left = px - ts.width / 2;
  if (left + ts.width > r.x + r.w) left = r.x + r.w - ts.width;
  if (left < r.x) left = r.x;
  g.drawText(left, baseline, text, font, grey, ALIGN_LEFT);
  return true;
}

void drawGrid(Gfx& g, const ChartRect& r, const AxisRange& ax, const EFont& font, float step,
              uint8_t lineGrey, uint8_t textGrey) {
  if (step <= 0) step = gridStepFor(ax);
  float vals[32];
  int cnt = gridValues(ax, step, vals, 32);
  int half = measureText(font, "0").inkAscent / 2;
  for (int i = 0; i < cnt; i++) {
    int y = (int)lroundf(mapY(vals[i], ax.min, ax.max, r.y, r.h));
    g.hline(r.x, y, r.w, lineGrey);
    char t[16];
    formatDegrees(t, sizeof t, vals[i]);
    g.drawText(r.x - 8, y + half, t, font, textGrey, ALIGN_RIGHT);
  }
}

void drawNowLine(Gfx& g, int x, int y0, int y1, int thickness, uint8_t grey) {
  g.line(x, y0, x, y1, thickness, grey, CAP_BUTT);
}

void drawBars(Gfx& g, const float* v, int n, const ChartRect& r, float scaleMax, const BarStyle& st) {
  if (!v || n <= 0) return;
  if (!(scaleMax > 0)) scaleMax = barScaleMax(v, n, st.floorMax);
  if (st.baseline) g.hline(r.x, r.y + r.h - 1, r.w, st.baselineGrey);
  for (int i = 0; i < n; i++) {
    int h = barHeight(v[i], scaleMax, r.h, st.minHeight);
    if (h <= 0) continue;
    int bx, bw;
    barSlot(i, n, r.x, r.w, st.gap, &bx, &bw);
    g.fillRect(bx, r.y + r.h - h, bw, h, st.grey);
  }
}

void drawProbBars(Gfx& g, const float* pct, int n, const ChartRect& r, uint8_t grey, int gap) {
  if (!pct || n <= 0) return;
  for (int i = 0; i < n; i++) {
    int h = barHeight(pct[i], 100.0f, r.h, 2);
    if (h <= 0) continue;
    int bx, bw;
    barSlot(i, n, r.x, r.w, gap, &bx, &bw);
    g.fillRect(bx, r.y + r.h - h, bw, h, grey);
  }
}

void drawSparkline(Gfx& g, const float* v, int n, const ChartRect& r, int thickness,
                   int fillGrey, bool baseline, int dotRadius) {
  AxisRange ax = sparkRange(v, n);
  if (!ax.valid) return;
  if (baseline) g.hline(r.x, r.y + r.h, r.w, COL_L);
  LineStyle st;
  st.thickness = thickness;
  st.fillArea = fillGrey >= 0;
  st.fillGrey = (uint8_t)(fillGrey < 0 ? 0 : fillGrey);
  drawLineSeries(g, v, n, r, ax, st);
  if (dotRadius > 0) drawEndDot(g, v, n, r, ax, dotRadius, COL_K);
}

bool drawLightningRow(Gfx& g, const uint16_t* counts, int n, const ChartRect& r,
                      const EFont& countFont, const EFont& msgFont, const char* emptyMsg) {
  bool any = false;
  for (int i = 0; counts && i < n; i++) if (counts[i] > 0) { any = true; break; }
  if (n <= 0) return false;
  float slot = (float)r.w / n;
  if (!any) {
    if (emptyMsg && *emptyMsg) g.drawText((int)lroundf(r.x + 2 * slot), r.y + 16, emptyMsg, msgFont, COL_M);
    return false;
  }
  for (int i = 0; i < n; i++) {
    if (counts[i] == 0) continue;
    float cx = r.x + i * slot + slot * 0.5f;
    drawBolt(g, cx, (float)(r.y + 8), 0.8f, COL_K);
    char t[8];
    snprintf(t, sizeof t, "%u", (unsigned)counts[i]);
    g.drawText((int)lroundf(cx), r.y + 30, t, countFont, COL_K, ALIGN_CENTER);
  }
  return true;
}

void drawHourAxis(Gfx& g, int x, int w, int baselineY, const HourAxis& ax, const EFont& regular, const EFont& bold) {
  HourLabel labels[64];
  int cnt = hourAxisLabels(ax, labels, 64);
  for (int i = 0; i < cnt; i++) {
    const HourLabel& l = labels[i];
    int lx = x + (int)lroundf((float)l.slot * w / (ax.slots < 1 ? 1 : ax.slots));
    Align al = l.slot == 0 ? ALIGN_LEFT : (l.slot == ax.slots ? ALIGN_RIGHT : ALIGN_CENTER);
    g.drawText(lx, baselineY, l.text, l.isNow ? bold : regular, l.isNow ? COL_K : COL_D, al);
  }
}
