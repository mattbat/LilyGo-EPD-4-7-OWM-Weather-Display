// Portable screens: Live, Overnight summary, Charge me, Error.
// Pixel positions, font sizes, weights and strokes are copied from docs/mockups.html:
//   live      = iteration H3 (gHeader, hTop, hChart, label, optH3) and the alert band (alertBand, optH4)
//   overnight = option N3 (nightHeader, nightFooter, optN3)
// Large text only: nothing is drawn below 20 px.
#include "screens.h"
#include "screens_debug.h"
#include "alert.h"
#include "charts.h"
#include "icons.h"
#include "units.h"
#include "timefmt.h"
#include "palette.h"
#include "fonts/fonts.h"
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MID_DOT "\xC2\xB7"
#define SEP     "  \xC2\xB7  "

// ---------------------------------------------------------------------------------------------
// Text layer: every string goes through here. A "nan"/"inf" token never reaches the framebuffer.
// ---------------------------------------------------------------------------------------------
namespace {
int g_anomalies = 0;
char g_lastAnomaly[64] = "";

bool hasBadToken(const char* s) {
  while (*s) {
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) { s++; continue; }
    char w[10]; int n = 0;
    while (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) {
      if (n < 9) w[n++] = (char)((*s >= 'A' && *s <= 'Z') ? *s + 32 : *s);
      s++;
    }
    w[n < 9 ? n : 9] = 0;
    if (n == 3 && (!strcmp(w, "nan") || !strcmp(w, "inf"))) return true;
    if (n == 8 && !strncmp(w, "infinity", 8)) return true;
  }
  return false;
}

// Replaces anything containing a nan/inf token by "--" (and records it for the tests).
const char* clean(const char* s) {
  if (!s) return "--";
  if (hasBadToken(s)) {
    g_anomalies++;
    snprintf(g_lastAnomaly, sizeof g_lastAnomaly, "%s", s);
    return "--";
  }
  return s;
}

int txt(Gfx& g, int x, int y, const char* s, const EFont& f, uint8_t grey, Align a = ALIGN_LEFT, int ls = 0) {
  return g.drawText(x, y, clean(s), f, grey, a, ls);
}
int tw(const EFont& f, const char* s, int ls = 0) { return measureText(f, clean(s), ls).width; }

// Shortens `s` so that it fits maxW, appending "..." (the fonts have no ellipsis glyph).
void ellipsize(char* out, size_t n, const char* s, const EFont& f, int maxW, int ls = 0) {
  snprintf(out, n, "%s", clean(s));
  if (measureText(f, out, ls).width <= maxW) return;
  size_t len = strlen(out);
  while (len > 0) {
    len--;
    while (len > 0 && ((uint8_t)out[len] & 0xC0) == 0x80) len--;   // start of a UTF-8 sequence
    char tmp[96];
    snprintf(tmp, sizeof tmp, "%.*s...", (int)len, out);
    if (measureText(f, tmp, ls).width <= maxW || len == 0) {
      snprintf(out, n, "%s", tmp);
      return;
    }
  }
}

// First font of `fonts` whose width fits maxW; the last one if none does.
const EFont& fitFont(const char* s, const EFont* const* fonts, int nf, int maxW, int ls = 0) {
  for (int i = 0; i < nf; i++) if (tw(*fonts[i], s, ls) <= maxW) return *fonts[i];
  return *fonts[nf - 1];
}

// Draw with the biggest font that fits; truncate with an ellipsis if even the smallest is too wide.
int txtFit(Gfx& g, int x, int y, const char* s, const EFont* const* fonts, int nf, int maxW, uint8_t grey,
           Align a = ALIGN_LEFT, int ls = 0) {
  const EFont& f = fitFont(s, fonts, nf, maxW, ls);
  char buf[128];
  ellipsize(buf, sizeof buf, s, f, maxW, ls);
  return g.drawText(x, y, buf, f, grey, a, ls);
}

// ---- formatting (NAN -> "--") ----
void fTemp(char* b, size_t n, float v) { if (!isfinite(v)) snprintf(b, n, "--"); else formatDegrees(b, n, v); }
void fInt(char* b, size_t n, float v) { if (!isfinite(v)) snprintf(b, n, "--"); else snprintf(b, n, "%d", (int)lroundf(v)); }
void fFix(char* b, size_t n, float v, int dec) { if (!isfinite(v)) snprintf(b, n, "--"); else snprintf(b, n, "%.*f", dec, v); }
bool isDash(const char* s) { return s[0] == '-' && s[1] == '-'; }

// Copies `src` without trailing blanks (the parser cuts long condition text at a fixed length).
void trimCopy(char* dst, size_t n, const char* src) {
  snprintf(dst, n, "%s", src);
  size_t l = strlen(dst);
  while (l > 0 && dst[l - 1] == ' ') dst[--l] = 0;
}

// ---- small drawing helpers ----
void rule(Gfx& g, int y, int thickness = 2) { g.fillRect(24, y - 1, 912, thickness, COL_K); }

int64_t pickNow(const WeatherData& d, const ScreenStatus& st) {
  if (st.nowEpoch > 0) return st.nowEpoch;
  if (st.lastGoodEpoch > 0) return st.lastGoodEpoch;
  if (d.fetchedEpoch > 0) return d.fetchedEpoch;
  return d.cur.time;
}
int64_t pickDataEpoch(const WeatherData& d, const ScreenStatus& st, int64_t now) {
  if (st.lastGoodEpoch > 0) return st.lastGoodEpoch;
  if (d.fetchedEpoch > 0) return d.fetchedEpoch;
  return now;
}

void drawHeaderLabel(Gfx& g, int x, int y, uint8_t grey, const EFont& f = AtkB20) {
  txt(g, x, y, LOCATION_LABEL, f, grey, ALIGN_LEFT, 2);
}

// Appends `item` to `line` with the separator when both are non-empty.
void addItem(char* line, size_t n, const char* item) {
  if (!item || !*item) return;
  if (line[0]) strncat(line, SEP, n - strlen(line) - 1);
  strncat(line, item, n - strlen(line) - 1);
}

// Right-aligned "value unit" (big bold value, regular unit). The value shrinks through `fonts` to fit maxW.
void drawValueUnit(Gfx& g, int right, int baseline, const char* value, const char* unit, const EFont* const* fonts,
                   int nf, const EFont& unitFont, int gap, int maxW) {
  bool dash = isDash(value);
  int uw = (!dash && unit && *unit) ? tw(unitFont, unit) + gap : 0;
  const EFont& f = fitFont(value, fonts, nf, maxW - uw);
  if (uw) txt(g, right, baseline, unit, unitFont, COL_K, ALIGN_RIGHT);
  txt(g, right - uw, baseline, value, f, COL_K, ALIGN_RIGHT);
}

// ---------------------------------------------------------------------------------------------
// Live screen (H3)
// ---------------------------------------------------------------------------------------------
void drawLiveHeader(Gfx& g, const ScreenStatus& st, int64_t dataEpoch) {
  drawHeaderLabel(g, 24, 34, COL_K);
  bool batt = st.batteryPercent >= 0;
  int right = batt ? 882 : 936;
  char clock[16];
  fmtClock(clock, sizeof clock, dataEpoch);
  if (st.offline) {
    // black rounded chip "OFFLINE · 1:30 PM" (the time of the last good data)
    char line[48];
    snprintf(line, sizeof line, "OFFLINE " MID_DOT " %s", clock);
    int w = tw(AtkB24, line);
    int x0 = right - w - 28;
    g.fillRoundRect(x0, 8, right - x0, 38, 10, COL_K);
    txt(g, x0 + 14, 35, line, AtkB24, COL_PAPER);
  } else {
    txt(g, right, 34, clock, AtkR24, COL_K, ALIGN_RIGHT);
  }
  if (batt) drawBattery(g, 894, 19, st.batteryPercent);
  rule(g, 50);
}

void drawLiveTop(Gfx& g, const WeatherData& d) {
  const CurrentConditions& c = d.cur;
  drawIcon(g, c.icon, 92, 136, 1.15f);

  // big temperature: 170 px, shrinks so that it never reaches the right-hand rows
  char tb[16];
  if (isfinite(c.tempF)) snprintf(tb, sizeof tb, "%d\xC2\xB0", (int)lroundf(c.tempF)); else snprintf(tb, sizeof tb, "--");
  static const EFont* const tf[] = {&AtkB170, &AtkB136, &AtkB76};
  const EFont& f = fitFont(tb, tf, 3, 548 - 176);
  int capMid = 236 - measureText(AtkB170, "0").inkAscent / 2;
  int base = capMid + measureText(f, "0").inkAscent / 2;
  txt(g, 176, base, tb, f, COL_K);

  // condition
  char cond[COND_LEN + 8];
  trimCopy(cond, sizeof cond, c.conditions);
  if (!cond[0]) snprintf(cond, sizeof cond, "--");
  if (clean(cond) != cond) snprintf(cond, sizeof cond, "--");      // a nan/inf word is refused once, here
  static const EFont* const cf[] = {&AtkB34, &AtkB28, &AtkB24};
  txtFit(g, 32, 292, cond, cf, 3, 540, COL_K);

  // three rows: label 28, value 56 bold, unit 28
  char v[24];
  struct Row { const char* label; const char* unit; } rows[3] = {{"Wind", "mph"}, {"Rain today", "in"}, {"Humidity", "%"}};
  fInt(v, sizeof v, c.windMph);
  char vals[3][24];
  snprintf(vals[0], sizeof vals[0], "%s", v);
  float rainToday = isfinite(c.rainTodayIn) ? c.rainTodayIn : (d.today.valid ? d.today.rainIn : NAN);
  fFix(vals[1], sizeof vals[1], rainToday, 2);
  fInt(vals[2], sizeof vals[2], c.humidityPct == 0 && !(d.have & (HAVE_FORECAST | HAVE_OBS)) ? NAN : (float)c.humidityPct);
  static const EFont* const vf[] = {&AtkB56, &AtkB46, &AtkB34};
  for (int i = 0; i < 3; i++) {
    int y = 112 + i * 78;
    txt(g, 580, y, rows[i].label, AtkR28, COL_K);
    int maxW = 936 - (580 + tw(AtkR28, rows[i].label) + 20);
    drawValueUnit(g, 936, y + 6, vals[i], rows[i].unit, vf, 3, AtkR28, 6, maxW);
    if (i < 2) g.hline(580, y + 26, 356, COL_L);
  }
  rule(g, 316);
}

// Chart geometry (mockup hChart): x is TRUE TIME over [now - 6 h, now + 18 h] mapped to x 44..916 (see liveChartX).
const int CH_X = 44, CH_W = 872, CH_Y = 368, CH_H = 76, CH_BASE = 488, CH_RH = 30;
const int CH_BACK_S = 6 * 3600, CH_FWD_S = 18 * 3600;

struct SeriesPt { int x; float v; };   // v = NAN breaks the line

// Draws runs of finite points as a solid or dashed polyline (a lone point becomes a dot).
void drawRuns(Gfx& g, const SeriesPt* p, int n, const AxisRange& ax, int thickness, bool dashed) {
  int i = 0;
  while (i < n) {
    while (i < n && !isfinite(p[i].v)) i++;
    int j = i;
    while (j < n && isfinite(p[j].v)) j++;
    int len = j - i;
    if (len == 1) {
      g.fillCircle(p[i].x, (int)lroundf(mapY(p[i].v, ax.min, ax.max, CH_Y, CH_H)), thickness / 2, COL_K);
    } else if (len > 1) {
      Pt pts[HIST_BUCKETS + HOURLY_FC_MAX + 2];
      int m = len > HIST_BUCKETS + HOURLY_FC_MAX ? HIST_BUCKETS + HOURLY_FC_MAX : len;
      for (int k = 0; k < m; k++) pts[k] = {(float)p[i + k].x, mapY(p[i + k].v, ax.min, ax.max, CH_Y, CH_H)};
      if (dashed) g.dashedPolyline(pts, m, thickness, COL_K, 14, 10);
      else g.polyline(pts, m, thickness, COL_K, CAP_ROUND);
    }
    i = j;
  }
}

void drawLiveChartBlock(Gfx& g, const WeatherData& d, int64_t now) {
  const int64_t nowT = d.cur.time ? (int64_t)d.cur.time : now;
  const int64_t T0 = nowT - CH_BACK_S, T1 = nowT + CH_FWD_S;
  auto X = [&](int64_t t) { return liveChartX(t, nowT); };
  const int nowX = X(nowT), rightX = X(T1);
  const bool hist = (d.have & HAVE_HISTORY) != 0 && d.histStartEpoch;
  bool anyHist = false;
  for (int j = 0; (d.have & HAVE_HISTORY) && j < HIST_BUCKETS; j++) if (d.hist[j].valid) anyHist = true;

  // measured points: completed hours at their midpoint, the running hour at the middle of [hour start, now]
  SeriesPt meas[HIST_BUCKETS + 1];
  int nm = 0;
  if (hist) {
    for (int j = 0; j < HIST_BUCKETS; j++) {
      int64_t s = (int64_t)d.histStartEpoch + (int64_t)j * 3600;
      int64_t mid = (s <= nowT && nowT < s + 3600) ? (s + nowT) / 2 : s + 1800;
      if (mid < T0 || mid >= nowT) continue;
      meas[nm++] = {X(mid), d.hist[j].valid ? d.hist[j].tempF : NAN};
    }
  }
  if (isfinite(d.cur.tempF)) meas[nm++] = {nowX, d.cur.tempF};
  // forecast points at their own epochs, starting from the current reading
  SeriesPt fut[HOURLY_FC_MAX + 1];
  int nf = 0;
  if (isfinite(d.cur.tempF)) fut[nf++] = {nowX, d.cur.tempF};
  for (int i = 0; i < d.hourlyCount && i < HOURLY_FC_MAX; i++) {
    const HourlyFc& h = d.hourly[i];
    if ((int64_t)h.time <= nowT || (int64_t)h.time > T1) continue;
    fut[nf++] = {X(h.time), h.tempF};
  }
  auto countFinite = [](const SeriesPt* p, int n) { int c = 0; for (int i = 0; i < n; i++) if (isfinite(p[i].v)) c++; return c; };
  bool anyMeas = countFinite(meas, nm) > 1, anyFut = countFinite(fut, nf) > 1;

  // temperature scale: data range plus 3 degrees on both sides
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < nm; i++) if (isfinite(meas[i].v)) { if (meas[i].v < lo) lo = meas[i].v; if (meas[i].v > hi) hi = meas[i].v; }
  for (int i = 0; i < nf; i++) if (isfinite(fut[i].v)) { if (fut[i].v < lo) lo = fut[i].v; if (fut[i].v > hi) hi = fut[i].v; }
  AxisRange ax{lo - 3, hi + 3, hi >= lo};

  // clock-hour ticks every 3 h (local hours divisible by 3) at their true x
  struct Tick { int64_t t; int x; int hour; };
  Tick ticks[12];
  int nt = 0;
  for (int64_t t = (T0 + 3599) / 3600 * 3600; t <= T1 && nt < 12; t += 3600) {
    struct tm lt;
    if (!localTm(t, &lt) || lt.tm_min != 0 || lt.tm_hour % 3 != 0) continue;
    ticks[nt++] = {t, X(t), lt.tm_hour};
  }
  for (int i = 0; i < nt; i++) if (abs(ticks[i].x - nowX) > 4) g.vline(ticks[i].x, CH_Y - 8, CH_BASE - (CH_Y - 8), COL_L);

  // rain: solid = measured per hour (spanning its real hour, clipped to the window), outlined = forecast probability
  float rain[HIST_BUCKETS] = {0}, rmax = 0.05f, rainTotal = 0;
  if (hist) {
    for (int j = 0; j < HIST_BUCKETS; j++) {
      int64_t s = (int64_t)d.histStartEpoch + (int64_t)j * 3600;
      if (s + 3600 <= T0 || s >= nowT || !d.hist[j].valid || !isfinite(d.hist[j].rainIn)) continue;
      rain[j] = d.hist[j].rainIn;
      if (rain[j] > rmax) rmax = rain[j];
      rainTotal += rain[j] * (s >= T0 ? 1.0f : (float)(s + 3600 - T0) / 3600.0f);
    }
    for (int j = 0; j < HIST_BUCKETS; j++) {
      if (rain[j] <= 0) continue;
      int64_t s = (int64_t)d.histStartEpoch + (int64_t)j * 3600;
      int x0 = X(s < T0 ? T0 : s), x1 = X(s + 3600 > nowT ? nowT : s + 3600);
      int bx = x0 + 2, bw = x1 - x0 - 4;
      if (bw < 3) { bx = x0; bw = x1 - x0 < 3 ? 3 : x1 - x0; }
      int h = barHeight(rain[j], rmax, CH_RH, 5);
      g.fillRect(bx, CH_BASE - h, bw, h, COL_K);
    }
  }
  for (int i = 0; i < d.hourlyCount && i < HOURLY_FC_MAX; i++) {
    const HourlyFc& h = d.hourly[i];
    int64_t s = h.time, e = s + 3600;
    if (e <= nowT || s >= T1) continue;
    int x0 = X(s < nowT ? nowT : s), x1 = X(e > T1 ? T1 : e);
    int bw = x1 - x0 - 6;
    if (bw < 4) continue;
    int ph = barHeight((float)h.precipProbPct, 100.0f, CH_RH, 5);
    if (ph <= 0) continue;
    g.fillRect(x0 + 3, CH_BASE - ph, bw, ph, COL_PAPER);
    g.rect(x0 + 3, CH_BASE - ph, bw, ph, COL_K, 2);
  }
  g.fillRect(CH_X, CH_BASE - 1, CH_W, 2, COL_K);

  // lines, now line and dot
  if (ax.valid) {
    drawRuns(g, meas, nm, ax, 5, false);
    drawRuns(g, fut, nf, ax, 5, true);
  }
  drawNowLine(g, nowX, CH_Y - 10, CH_BASE, 2, COL_K);
  if (ax.valid && isfinite(d.cur.tempF))
    g.fillCircle(nowX, (int)lroundf(mapY(d.cur.tempF, ax.min, ax.max, CH_Y, CH_H)), 10, COL_K);
  if (!anyMeas) txt(g, CH_X + (nowX - CH_X) / 2, CH_Y + CH_H / 2 + 8, anyHist ? "no data" : "no history", AtkR22, COL_K, ALIGN_CENTER);
  if (!anyFut) txt(g, nowX + (rightX - nowX) / 2, CH_Y + CH_H / 2 + 8, "no forecast", AtkR22, COL_K, ALIGN_CENTER);

  // note, only when it rained in the measured window
  if (anyHist && rainTotal >= 0.005f) {
    char note[48];
    snprintf(note, sizeof note, "Rain, last 6 h: %.2f in", rainTotal);
    txt(g, CH_X, 344, note, AtkR22, COL_K);
  }

  // time labels at the ticks (skipping any that would hit NOW) and forecast temperatures on the line
  txt(g, nowX, 522, "NOW", AtkB22, COL_K, ALIGN_CENTER);
  for (int i = 0; i < nt; i++) {
    int x = ticks[i].x;
    if (abs(x - nowX) >= 64) {
      char lb[16];
      formatHourLabel(lb, sizeof lb, ticks[i].hour);
      int w = tw(AtkR22, lb);
      int lx = x < 24 + w / 2 ? 24 + w / 2 : x > 936 - w / 2 ? 936 - w / 2 : x;
      txt(g, lx, 522, lb, AtkR22, COL_K, ALIGN_CENTER);
    }
    if (ticks[i].t > nowT && x - nowX >= 40 && ax.valid) {
      for (int k = 0; k < d.hourlyCount && k < HOURLY_FC_MAX; k++) {
        const HourlyFc& h = d.hourly[k];
        if (llabs((int64_t)h.time - ticks[i].t) > 1800 || !isfinite(h.tempF)) continue;
        int py = (int)lroundf(mapY(h.tempF, ax.min, ax.max, CH_Y, CH_H));
        g.fillCircle(x, py, 5, COL_K);
        char t[16];
        formatDegrees(t, sizeof t, h.tempF);
        int lx = x > CH_X + CH_W - 30 ? CH_X + CH_W - 30 : x;
        knockoutText(g, lx, py - 14, t, AtkB24, ALIGN_CENTER);
        txt(g, lx, py - 14, t, AtkB24, COL_K, ALIGN_CENTER);
        break;
      }
    }
  }
}

// Black band that replaces the chart area (y 318..540) when something needs attention.
void drawAlertBand(Gfx& g, const Alert& a) {
  const int y = 318, h = 222, mid = y + h / 2;
  g.fillRect(0, y, PANEL_WIDTH, h, COL_K);
  switch (a.kind) {
    case AlertKind::Lightning: drawBolt(g, 74, (float)mid, 4.0f, COL_PAPER); break;
    case AlertKind::Raining:
      for (int i = -1; i <= 1; i++) g.line(74 + i * 32 + 10, mid - 30, 74 + i * 32 - 10, mid + 30, 7, COL_PAPER);
      break;
    case AlertKind::UV: drawSun(g, 74, mid, 20, 5, COL_K, COL_PAPER); break;
    default: break;
  }
  static const EFont* const hf[] = {&AtkB46, &AtkB34, &AtkB28};
  static const EFont* const sf[] = {&AtkR28, &AtkR24, &AtkR22};
  if (a.sub[0]) {
    txtFit(g, 140, mid - 2, a.headline, hf, 3, 936 - 140, COL_PAPER);
    txtFit(g, 140, mid + 40, a.sub, sf, 3, 936 - 140, COL_PAPER);
  } else {
    txtFit(g, 140, mid + 16, a.headline, hf, 3, 936 - 140, COL_PAPER);
  }
}

// ---------------------------------------------------------------------------------------------
// Overnight (N3)
// ---------------------------------------------------------------------------------------------
void drawNightHeader(Gfx& g) {
  g.fillRect(0, 0, PANEL_WIDTH, 64, COL_K);
  drawHeaderLabel(g, 24, 42, COL_PAPER, AtkB22);
  char next[32];
  int h12 = QUIET_END_HOUR % 12; if (h12 == 0) h12 = 12;
  if (QUIET_END_MIN) snprintf(next, sizeof next, "Paused until %d:%02d %s", h12, QUIET_END_MIN, QUIET_END_HOUR < 12 ? "AM" : "PM");
  else snprintf(next, sizeof next, "Paused until %d %s", h12, QUIET_END_HOUR < 12 ? "AM" : "PM");
  int w = txt(g, 936, 42, next, AtkB24, COL_PAPER, ALIGN_RIGHT);
  drawMoon(g, 936 - w - 36, 32, 13, COL_PAPER, COL_K);
}

// TODAY / YESTERDAY row: High, Low, Rain (22 px label above a 50 px bold value).
void drawNightToday(Gfx& g, const DaySummary& s, OvernightTitles titles) {
  txt(g, 28, 102, titles == TITLES_TODAY_TOMORROW ? "TODAY" : "YESTERDAY", AtkB22, COL_K, ALIGN_LEFT, 2);
  bool v = s.valid;
  char hi[16], lo[16], rain[16];
  fTemp(hi, sizeof hi, v ? s.highF : NAN);
  fTemp(lo, sizeof lo, v ? s.lowF : NAN);
  fFix(rain, sizeof rain, v ? s.rainIn : NAN, 2);
  const char* labels[3] = {"High", "Low", "Rain"};
  const char* vals[3] = {hi, lo, rain};
  static const EFont* const vf[] = {&AtkB50, &AtkB34};
  for (int i = 0; i < 3; i++) {
    int x = 28 + i * 140;
    txt(g, x, 132, labels[i], AtkR22, COL_K);
    if (i == 2) {
      int maxW = 440 - x;
      const EFont& f = fitFont(rain, vf, 2, isDash(rain) ? maxW : maxW - tw(AtkR22, "in") - 6);
      int w = txt(g, x, 184, rain, f, COL_K);
      if (!isDash(rain)) txt(g, x + w + 6, 184, "in", AtkR22, COL_K);
    } else {
      txtFit(g, x, 184, vals[i], vf, 2, 136, COL_K);
    }
  }
}

// Tomorrow (or today after midnight): title, icon, hi / lo, condition, chart. Returns the sunrise epoch.
int64_t drawNightRight(Gfx& g, const WeatherData& d, int64_t now, OvernightTitles titles) {
  int idx = titles == TITLES_TODAY_TOMORROW ? 1 : 0;
  bool have = d.dailyCount > idx;
  const DailyFc* df = have ? &d.daily[idx] : nullptr;
  int64_t dayRef = have && df->dayStart ? (int64_t)df->dayStart + 12 * 3600 : now + (int64_t)idx * 86400;
  struct tm dt;
  localTm(dayRef, &dt);
  char title[48], dow[8];
  snprintf(dow, sizeof dow, "%s", dowName(dt.tm_wday)); upperAscii(dow);
  snprintf(title, sizeof title, "%s " MID_DOT " %s", titles == TITLES_TODAY_TOMORROW ? "TOMORROW" : "TODAY", dow);
  txt(g, 480, 102, title, AtkB22, COL_K, ALIGN_LEFT, 2);

  // hourly series 5 AM - 10 PM for that date
  float temp[18], pop[18];
  for (int i = 0; i < 18; i++) { temp[i] = NAN; pop[i] = NAN; }
  for (int i = 0; i < d.hourlyCount && i < HOURLY_FC_MAX; i++) {
    const HourlyFc& h = d.hourly[i];
    struct tm ht;
    if (!localTm(h.time, &ht) || ht.tm_yday != dt.tm_yday || ht.tm_year != dt.tm_year) continue;
    if (ht.tm_hour < 5 || ht.tm_hour > 22) continue;
    temp[ht.tm_hour - 5] = h.tempF;
    pop[ht.tm_hour - 5] = (float)h.precipProbPct;
  }

  // icon, hi "/ lo", condition
  if (have) drawIcon(g, df->icon, 532, 160, 0.75f);
  char hi[16], lo[24], lt[16];
  fTemp(hi, sizeof hi, have ? df->highF : NAN);
  fTemp(lt, sizeof lt, have ? df->lowF : NAN);
  snprintf(lo, sizeof lo, "/ %s", lt);
  struct HiLo { const EFont* h; const EFont* l; };
  static const HiLo opts[] = {{&AtkB76, &AtkR44}, {&AtkB50, &AtkR28}};
  const HiLo* pick = &opts[1];
  for (const HiLo& o : opts) if (596 + tw(*o.h, hi) + 14 + tw(*o.l, lo) <= 936) { pick = &o; break; }
  int hw = txt(g, 596, 186, hi, *pick->h, COL_K);
  txt(g, 596 + hw + 14, 186, lo, *pick->l, COL_K);
  char dcond[COND_LEN + 1] = "";
  if (have) trimCopy(dcond, sizeof dcond, df->conditions);
  if (clean(dcond) != dcond) snprintf(dcond, sizeof dcond, "--");
  static const EFont* const cf[] = {&AtkB28, &AtkB24};
  txtFit(g, 480, 220, dcond[0] ? dcond : (have ? "--" : "Forecast unavailable"), cf, 2, 936 - 480, COL_K);
  rule(g, 234);

  // chart: dashed temperature line, hairline degree gridlines, peak label
  const int cx = 84, cy = 280, cw = 840, ch = 112;
  const float bw = (float)cw / 18;
  AxisRange ax = autoRange(temp, 18, 5, 3);
  ChartRect full{cx, cy, cw, ch};
  ChartRect lr{cx, cy, (int)lroundf(17 * bw), ch};   // true time: hour h (5..22) at cx + (h - 5) * bw; bars span [h, h+1)
  if (ax.valid) {
    drawGrid(g, full, ax, AtkR20, ax.max - ax.min > 30 ? 10.0f : 5.0f, COL_L, COL_K);
    LineStyle ls; ls.thickness = 4; ls.dashed = true; ls.dash = 12; ls.gap = 8;
    drawLineSeries(g, temp, 18, lr, ax, ls);
    int pk = hiLoIndex(temp, 18, true, 0);
    float px, py;
    if (pk >= 0 && seriesPoint(temp, 18, pk, lr, ax, &px, &py)) {
      g.fillCircle((int)lroundf(px), (int)lroundf(py), 6, COL_K);
      char t[16];
      formatDegrees(t, sizeof t, temp[pk]);
      int lx = (int)lroundf(px);
      if (lx < cx + 34) lx = cx + 34;
      if (lx > cx + cw - 34) lx = cx + cw - 34;
      knockoutText(g, lx, (int)lroundf(py) - 18, t, AtkB28, ALIGN_CENTER);
      txt(g, lx, (int)lroundf(py) - 18, t, AtkB28, COL_K, ALIGN_CENTER);
    }
  } else {
    txt(g, cx + cw / 2, cy + ch / 2 + 8, "no forecast", AtkR22, COL_K, ALIGN_CENTER);
  }

  // precipitation probability bars; the percentage sits just above its own bar (only bars of 10 % or more)
  const int ry = 404, rh = 48;
  drawProbBars(g, pop, 18, ChartRect{cx, ry, cw, rh}, COL_K, 6);
  g.hline(cx, ry + rh, cw, COL_K);
  int lastRight = -1000;
  for (int k = 0; k < 18; k++) {
    if (!isfinite(pop[k]) || pop[k] < 10) continue;
    char t[8];
    snprintf(t, sizeof t, "%d%%", (int)lroundf(pop[k]));
    int w = tw(AtkB20, t), x = cx + (int)lroundf(k * bw + bw / 2);
    if (x - w / 2 < lastRight + 4) continue;             // would touch the previous label
    int top = ry + rh - barHeight(pop[k], 100.0f, rh, 5);
    txt(g, x, top - 6, t, AtkB20, COL_K, ALIGN_CENTER);
    lastRight = x + w / 2;
  }
  txt(g, cx - 10, ry + rh, "rain", AtkR20, COL_K, ALIGN_RIGHT);
  for (int k = 0; k < 18; k += 4) {
    char lb[16];
    formatHourLabel(lb, sizeof lb, 5 + k);
    txt(g, cx + (int)lroundf(k * bw), 482, lb, AtkR22, COL_K, ALIGN_CENTER);
  }
  return have ? (int64_t)df->sunrise : 0;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Debug hooks
// ---------------------------------------------------------------------------------------------
int liveChartX(int64_t epoch, int64_t nowT) {
  return CH_X + (int)lround((double)(epoch - (nowT - CH_BACK_S)) * CH_W / (CH_BACK_S + CH_FWD_S));
}
int screenTextAnomalies() { return g_anomalies; }
const char* screenLastAnomaly() { return g_lastAnomaly; }
void screenTextAnomaliesReset() { g_anomalies = 0; g_lastAnomaly[0] = 0; }

// ---------------------------------------------------------------------------------------------
// Public screens
// ---------------------------------------------------------------------------------------------
void drawLive(Gfx& g, const WeatherData& d, const ScreenStatus& st) {
  g.clear(COL_PAPER);
  int64_t now = pickNow(d, st);
  int64_t dataEpoch = pickDataEpoch(d, st, now);
  drawLiveHeader(g, st, dataEpoch);
  drawLiveTop(g, d);
  Alert a = pickAlert(d);
  if (a.kind != AlertKind::None) drawAlertBand(g, a);
  else drawLiveChartBlock(g, d, now);
}

void drawOvernight(Gfx& g, const WeatherData& d, const ScreenStatus& st, OvernightTitles titles) {
  g.clear(COL_PAPER);
  int64_t now = pickNow(d, st);
  int64_t dataEpoch = pickDataEpoch(d, st, now);
  drawNightHeader(g);
  drawNightToday(g, d.today, titles);
  g.fillRect(451, 80, 2, 144, COL_K);
  int64_t sunrise = drawNightRight(g, d, now, titles);

  // footer
  rule(g, 500);
  char line[96] = "", item[48], a[16];
  if (sunrise) { fmtClock(a, sizeof a, sunrise); snprintf(item, sizeof item, "Sunrise %s", a); addItem(line, sizeof line, item); }
  fmtClock(a, sizeof a, dataEpoch);
  snprintf(item, sizeof item, "Last data %s", a);
  addItem(line, sizeof line, item);
  static const EFont* const ff[] = {&AtkR22, &AtkR20};
  txtFit(g, 480, 530, line, ff, 2, 912, COL_K, ALIGN_CENTER);
}

void drawChargeMe(Gfx& g, const ScreenStatus& st) {
  g.clear(COL_PAPER);
  drawHeaderLabel(g, 28, 36, COL_K);
  g.roundRect(340, 120, 260, 130, 16, COL_K, 8);
  g.fillRoundRect(604, 160, 20, 50, 4, COL_K);
  g.fillRoundRect(356, 136, 22, 98, 3, COL_K);
  txt(g, 480, 350, "Charge me", AtkB76, COL_K, ALIGN_CENTER);
  char line[96];
  if (st.batteryVolts > 0.5f) snprintf(line, sizeof line, "Battery %.2f V " MID_DOT " weather updates stopped to protect the cell", st.batteryVolts);
  else snprintf(line, sizeof line, "Battery low " MID_DOT " weather updates stopped to protect the cell");
  static const EFont* const f22[] = {&AtkR22, &AtkR20};
  txtFit(g, 480, 396, line, f22, 2, 900, COL_K, ALIGN_CENTER);
  if (st.lastGoodEpoch > 0) {
    char date[24], clock[16];
    fmtDate(date, sizeof date, st.lastGoodEpoch);
    fmtClock(clock, sizeof clock, st.lastGoodEpoch);
    snprintf(line, sizeof line, "Last data: %s " MID_DOT " %s", date, clock);
  } else {
    snprintf(line, sizeof line, "No weather data yet");
  }
  txt(g, 480, 446, line, AtkR20, COL_K, ALIGN_CENTER);
  txt(g, 480, 476, "Updates restart on their own once the battery is charged.", AtkR20, COL_K, ALIGN_CENTER);
}

void drawError(Gfx& g, const char* reason, const ScreenStatus& st) {
  g.clear(COL_PAPER);
  drawHeaderLabel(g, 28, 36, COL_K);
  drawIcon(g, ICON_CLOUDY, 480, 150, 1.2f);
  g.fillCircle(552, 196, 26, COL_K);
  txt(g, 552, 207, "!", AtkB28, COL_PAPER, ALIGN_CENTER);
  txt(g, 480, 320, "No weather data", AtkB50, COL_K, ALIGN_CENTER);
  static const EFont* const rf[] = {&AtkR22, &AtkR20};
  txtFit(g, 480, 372, reason && *reason ? reason : "Unknown error", rf, 2, 880, COL_K, ALIGN_CENTER);
  if (st.nowEpoch > 0) {
    char date[24], clock[16], line[64];
    fmtDate(date, sizeof date, st.nowEpoch);
    fmtClock(clock, sizeof clock, st.nowEpoch);
    snprintf(line, sizeof line, "%s " MID_DOT " %s", date, clock);
    txt(g, 480, 420, line, AtkR20, COL_K, ALIGN_CENTER);
  }
  txt(g, 480, 466, "Will try again at the next update.", AtkR20, COL_K, ALIGN_CENTER);
  if (st.batteryPercent >= 0) drawBattery(g, 894, 19, st.batteryPercent);
}
