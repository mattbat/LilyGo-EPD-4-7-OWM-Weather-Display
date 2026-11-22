// Portable screens: Live, Overnight summary, Charge me, Error.
// Pixel positions, font sizes, weights, greys and strokes are copied from docs/mockups.html
// (optC2 = live, optNight = overnight, optCharge = charge me). SVG letter-spacing of 1.5 is drawn as 2.
#include "screens.h"
#include "screens_debug.h"
#include "smart_slot.h"
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
#define COL_SLOT_SUB 12   // #cfcfca on the black slot

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

int64_t localMidnight(int64_t epoch) {
  struct tm t;
  if (!localTm(epoch, &t)) return epoch - epoch % 86400;
  t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
  return (int64_t)mktime(&t);
}

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

void drawHeaderLabel(Gfx& g, int x, int y, uint8_t grey) { txt(g, x, y, LOCATION_LABEL, AtkB18, grey, ALIGN_LEFT, 2); }

// "UV 3  ·  ..." style joiner: appends `item` to `line` with the separator when both are non-empty.
void addItem(char* line, size_t n, const char* item) {
  if (!item || !*item) return;
  if (line[0]) strncat(line, SEP, n - strlen(line) - 1);
  strncat(line, item, n - strlen(line) - 1);
}

void stationBatteryText(char* b, size_t n, float v) {
  b[0] = 0;
  if (!isfinite(v)) return;
  if (v >= STATION_BATT_OK_V) snprintf(b, n, "Station battery OK");
  else snprintf(b, n, "Station battery LOW %.2f V", v);
}

// Rain month / year ("Rain Nov 2.41 in  ·  2026 31.6 in"), empty when unknown.
void rainTotalsText(char* b, size_t n, const WeatherData& d, int64_t now) {
  b[0] = 0;
  if (!d.rainMonthYearKnown) return;
  struct tm t;
  if (!localTm(now, &t)) return;
  char m[32] = "", y[32] = "";
  if (isfinite(d.rainMonthIn)) snprintf(m, sizeof m, "Rain %s %.2f in", monName(t.tm_mon), d.rainMonthIn);
  if (isfinite(d.rainYearIn)) snprintf(y, sizeof y, "%d %.1f in", t.tm_year + 1900, d.rainYearIn);
  addItem(b, n, m);
  addItem(b, n, y);
}

// One centred info line, shrinking the font until it fits 912 px.
void drawInfoLine(Gfx& g, int baseline, const char* line) {
  static const EFont* const fonts[] = {&AtkR17, &AtkR16, &AtkR15, &AtkR14};
  txtFit(g, 480, baseline, line, fonts, 4, 912, COL_K, ALIGN_CENTER);
}

// ---------------------------------------------------------------------------------------------
// Smart slot
// ---------------------------------------------------------------------------------------------
void drawSlot(Gfx& g, const SmartSlot& s) {
  const int sx = 798, sy = 60, sw = 138, sh = 172;
  g.fillRoundRect(sx, sy, sw, sh, 8, COL_K);
  txt(g, sx + sw / 2, sy + 26, s.label, AtkB14, COL_PAPER, ALIGN_CENTER, 2);

  // Big text: next to the icon if it fits, else centred without the icon.
  static const EFont* const withIcon[] = {&AtkB50, &AtkB42};
  static const EFont* const alone[] = {&AtkB50, &AtkB42, &AtkB32, &AtkB22};
  const int iconW = 76;
  bool useIcon = s.icon != SlotIcon::None;
  const EFont* bigFont = nullptr;
  if (useIcon) {
    for (const EFont* f : withIcon) if (tw(*f, s.big) <= iconW) { bigFont = f; break; }
    if (!bigFont) useIcon = false;
  }
  if (useIcon) {
    switch (s.icon) {
      case SlotIcon::Bolt: drawBolt(g, sx + 30, sy + 76, 2.4f, COL_PAPER); break;
      case SlotIcon::Rain:
        for (int i = 0; i < 3; i++) g.line(sx + 22 + i * 12, sy + 62, sx + 16 + i * 12, sy + 84, 3, COL_PAPER);
        break;
      case SlotIcon::Sun: drawSun(g, sx + 32, sy + 74, 10, 3, COL_K, COL_PAPER); break;
      default: break;
    }
    txt(g, sx + 54, sy + 94, s.big, *bigFont, COL_PAPER);
  } else {
    txtFit(g, sx + sw / 2, sy + 94, s.big, alone, 4, sw - 16, COL_PAPER, ALIGN_CENTER);
  }
  static const EFont* const sub1F[] = {&AtkB17, &AtkB15, &AtkB14};
  static const EFont* const sub2F[] = {&AtkR16, &AtkR15, &AtkR14};
  txtFit(g, sx + sw / 2, sy + 132, s.sub1, sub1F, 3, sw - 12, COL_PAPER, ALIGN_CENTER);
  txtFit(g, sx + sw / 2, sy + 156, s.sub2, sub2F, 3, sw - 12, COL_SLOT_SUB, ALIGN_CENTER);
}

// ---------------------------------------------------------------------------------------------
// Live screen pieces
// ---------------------------------------------------------------------------------------------
void drawLiveHeader(Gfx& g, const ScreenStatus& st, int64_t now, int64_t dataEpoch) {
  drawHeaderLabel(g, 28, 34, COL_K);
  bool batt = st.batteryPercent >= 0;
  int right = batt ? 876 : 936;
  char clock[16], date[24], line[80];
  if (st.offline) {
    fmtClock(clock, sizeof clock, dataEpoch);
    snprintf(line, sizeof line, "OFFLINE " MID_DOT " last data %s", clock);
    int w = tw(AtkB17, line);
    int x1 = right, x0 = right - w - 24;
    g.fillRoundRect(x0, 9, x1 - x0, 32, 6, COL_K);
    txt(g, x0 + 12, 31, line, AtkB17, COL_PAPER);
  } else {
    fmtDate(date, sizeof date, now);
    fmtClock(clock, sizeof clock, dataEpoch);
    snprintf(line, sizeof line, "%s " MID_DOT " Updated %s", date, clock);
    txt(g, right, 34, line, AtkR22, COL_D, ALIGN_RIGHT);
  }
  if (batt) drawBattery(g, 894, 19, st.batteryPercent);
  rule(g, 48);
}

void drawStat(Gfx& g, int x, int y, const char* label, const char* value, const char* unit, const char* sub) {
  txt(g, x, y + 15, label, AtkB14, COL_D, ALIGN_LEFT, 2);
  int w = txt(g, x, y + 56, value, AtkB42, COL_K);
  if (!isDash(value) && unit && *unit) txt(g, x + w + 4, y + 56, unit, AtkR18, COL_K);
  static const EFont* const subF[] = {&AtkR16, &AtkR15, &AtkR14};
  txtFit(g, x, y + 79, sub, subF, 3, 148, COL_D);
}

void drawCurrent(Gfx& g, const WeatherData& d, const SmartSlot& slot) {
  const CurrentConditions& c = d.cur;
  drawIcon(g, c.icon, 82, 122, 1.0f);

  // big temperature
  char tb[16];
  if (isfinite(c.tempF)) snprintf(tb, sizeof tb, "%d\xC2\xB0", (int)lroundf(c.tempF)); else snprintf(tb, sizeof tb, "--");
  static const EFont* const tf[] = {&AtkB136, &AtkB84, &AtkB50};
  const EFont& f = fitFont(tb, tf, 3, 332);
  int capMid = 184 - measureText(AtkB136, "0").inkAscent / 2;
  int base = capMid + measureText(f, "0").inkAscent / 2;
  txt(g, 146, base, tb, f, COL_K);

  // condition + feels like
  char feels[32] = "", cond[COND_LEN + 8];
  if (isfinite(c.feelsLikeF)) { char t[16]; fTemp(t, sizeof t, c.feelsLikeF); snprintf(feels, sizeof feels, SEP "feels %s", t); }
  trimCopy(cond, sizeof cond, c.conditions);
  if (!feels[0] && !cond[0]) snprintf(cond, sizeof cond, "--");
  int avail = 440 - tw(AtkR22, feels);
  char line[96];
  if (!cond[0]) snprintf(line, sizeof line, "%s", feels + 5);                 // skip the leading separator
  else { char c2[48]; ellipsize(c2, sizeof c2, cond, AtkR22, avail); snprintf(line, sizeof line, "%s%s", c2, feels); }
  txt(g, 32, 226, line, AtkR22, COL_D);

  // 2x2 stat grid
  char v[24], sub[48], t1[16], t2[24];
  // WIND
  fInt(v, sizeof v, c.windMph);
  const char* dir = (isfinite((float)c.windDirDeg) && c.windDirDeg >= 0) ? degToCompass((float)c.windDirDeg) : "--";
  sub[0] = 0;
  if (!isDash(dir)) snprintf(sub, sizeof sub, "%s", dir);
  if (isfinite(c.gustMph)) {
    fInt(t1, sizeof t1, c.gustMph);
    char g2[32]; snprintf(g2, sizeof g2, "gusts %s", t1);
    if (sub[0]) strncat(sub, " " MID_DOT " ", sizeof sub - strlen(sub) - 1);
    strncat(sub, g2, sizeof sub - strlen(sub) - 1);
  }
  drawStat(g, 486, 60, "WIND", v, "mph", sub);
  // RAIN TODAY
  float rainToday = isfinite(c.rainTodayIn) ? c.rainTodayIn : (d.today.valid ? d.today.rainIn : NAN);
  fFix(v, sizeof v, rainToday, 2);
  sub[0] = 0;
  if (isfinite(c.rainRateInHr) && c.rainRateInHr > 0) { fFix(t1, sizeof t1, c.rainRateInHr, 2); snprintf(sub, sizeof sub, "%s in/hr now", t1); }
  else if (isfinite(c.rainYesterdayIn)) { fFix(t1, sizeof t1, c.rainYesterdayIn, 2); snprintf(sub, sizeof sub, "yesterday %s", t1); }
  drawStat(g, 640, 60, "RAIN TODAY", v, "in", sub);
  // HUMIDITY
  fInt(v, sizeof v, c.humidityPct == 0 && !(d.have & (HAVE_FORECAST | HAVE_OBS)) ? NAN : (float)c.humidityPct);
  sub[0] = 0;
  if (isfinite(c.dewPointF)) { fTemp(t2, sizeof t2, c.dewPointF); snprintf(sub, sizeof sub, "dew point %s", t2); }
  drawStat(g, 486, 148, "HUMIDITY", v, "%", sub);
  // PRESSURE
  fFix(v, sizeof v, c.pressureInHg, 2);
  const char* trend = c.pressureTrend < 0 ? "falling \xE2\x86\x93" : c.pressureTrend > 0 ? "rising \xE2\x86\x91" : "steady";
  drawStat(g, 640, 148, "PRESSURE", v, "in", isfinite(c.pressureInHg) ? trend : "");

  drawSlot(g, slot);
}

// Chart geometry (mockup): plot x 60..930, y 274..386, "now" in the middle.
struct LiveChart {
  float past[25], fut[25];
  float rain[24];
  float pop[24];
  uint16_t strikes[24];
  uint32_t startEpoch;   // epoch of slot 0 (clock hour, 23 h before the current hour)
  bool anyPast, anyFut, anyHist;
};

void buildLiveChart(const WeatherData& d, LiveChart& c, int64_t now) {
  bool hist = (d.have & HAVE_HISTORY) != 0;
  c.anyHist = false;
  for (int j = 0; hist && j < HIST_BUCKETS; j++) if (d.hist[j].valid) c.anyHist = true;
  int64_t ref = d.cur.time ? (int64_t)d.cur.time : now;
  c.startEpoch = (hist && d.histStartEpoch) ? d.histStartEpoch : (uint32_t)(ref / 3600 * 3600 - 23 * 3600);
  for (int i = 0; i < 25; i++) { c.past[i] = NAN; c.fut[i] = NAN; }
  for (int i = 0; i < 24; i++) { c.rain[i] = 0; c.pop[i] = NAN; c.strikes[i] = 0; }
  if (hist) {
    for (int j = 0; j < HIST_BUCKETS; j++) {
      const HistBucket& b = d.hist[j];
      if (!b.valid) continue;
      c.past[j] = b.tempF;
      if (isfinite(b.rainIn)) c.rain[j] = b.rainIn;
      c.strikes[j] = b.strikes;
    }
  }
  c.past[24] = isfinite(d.cur.tempF) ? d.cur.tempF : NAN;
  for (int i = 0; i < d.hourlyCount && i < HOURLY_FC_MAX; i++) {
    const HourlyFc& h = d.hourly[i];
    int slot = (int)lround(((double)h.time - (double)c.startEpoch) / 3600.0);
    if (slot < 24 || slot > 48) continue;
    c.fut[slot - 24] = h.tempF;
    if (slot < 48) c.pop[slot - 24] = (float)h.precipProbPct;
  }
  if (isfinite(c.past[24])) c.fut[0] = c.past[24];   // join the two halves at "now"
  int fin = 0;
  for (int i = 0; i < 25; i++) if (isfinite(c.past[i])) fin++;
  c.anyPast = fin > 1;
  fin = 0;
  for (int i = 0; i < 25; i++) if (isfinite(c.fut[i])) fin++;
  c.anyFut = fin > 1;
}

void popNoteText(char* out, size_t n, const LiveChart& c) {
  out[0] = 0;
  int best = -1; float bv = 0;
  for (int i = 0; i < 24; i++) if (isfinite(c.pop[i]) && c.pop[i] > bv) { bv = c.pop[i]; best = i; }
  bool any = false;
  for (int i = 0; i < 24; i++) if (isfinite(c.pop[i])) any = true;
  if (!any) return;
  if (best < 0 || bv < 20) { snprintf(out, n, "no rain expected"); return; }
  int64_t t = (int64_t)c.startEpoch + (int64_t)(24 + best) * 3600;
  struct tm lt, nt;
  char h[16];
  formatHourLabel(h, sizeof h, localHourOfEpoch((uint32_t)t));
  int64_t nowish = (int64_t)c.startEpoch + 24 * 3600;
  if (localTm(t, &lt) && localTm(nowish, &nt) && (lt.tm_yday != nt.tm_yday || lt.tm_year != nt.tm_year))
    snprintf(out, n, "%d%% chance %s %s", (int)lroundf(bv), dowName(lt.tm_wday), h);
  else
    snprintf(out, n, "%d%% chance %s", (int)lroundf(bv), h);
}

void drawLiveChartBlock(Gfx& g, const WeatherData& d, int64_t now) {
  LiveChart c;
  buildLiveChart(d, c, now);
  const int cx = 60, cy = 274, cw = 870, ch = 112, half = cw / 2, nowX = cx + half;
  const float bw = half / 24.0f;

  txt(g, cx + 4, 264, "MEASURED " MID_DOT " last 24 h", AtkB14, COL_D, ALIGN_LEFT, 2);
  txt(g, cx + cw - 4, 264, "FORECAST " MID_DOT " next 24 h", AtkB14, COL_D, ALIGN_RIGHT, 2);
  g.fillRect(nowX, cy - 4, half, 196, COL_FC);

  AxisRange ax = autoRange2(c.past, 25, c.fut, 25, 5, 3);
  ChartRect full{cx, cy, cw, ch}, lr{cx, cy, half, ch}, rr{nowX, cy, half, ch};
  if (ax.valid) {
    drawGrid(g, full, ax, AtkR15, 0, COL_XL, COL_D);
    LineStyle past; past.thickness = 4; past.fillArea = true; past.fillGrey = COL_XL;
    drawLineSeries(g, c.past, 25, lr, ax, past);
    LineStyle fut; fut.thickness = 3; fut.dashed = true; fut.dash = 10; fut.gap = 7;
    drawLineSeries(g, c.fut, 25, rr, ax, fut);
  }
  drawNowLine(g, nowX, cy - 6, cy + 196, 2, COL_K);
  if (ax.valid) {
    drawEndDot(g, c.past, 25, lr, ax, 7, COL_K);
    drawHiLoMarker(g, c.past, 25, lr, ax, true, AtkB19);
    drawHiLoMarker(g, c.past, 25, lr, ax, false, AtkB19);
    drawHiLoMarker(g, c.fut, 25, rr, ax, false, AtkB19);
    drawHiLoMarker(g, c.fut, 25, rr, ax, true, AtkB19);
  }
  if (!c.anyPast) txt(g, cx + half / 2, cy + ch / 2 + 6, c.anyHist ? "no data" : "no history", AtkR20, COL_M, ALIGN_CENTER);
  if (!c.anyFut) txt(g, nowX + half / 2, cy + ch / 2 + 6, "no forecast", AtkR20, COL_M, ALIGN_CENTER);

  // rain band
  const int ry = cy + ch + 10, rh = 40;
  g.hline(cx, ry + rh, cw, COL_L);
  BarStyle bs; bs.baseline = false; bs.gap = 4; bs.minHeight = 3; bs.floorMax = 0.05f; bs.grey = COL_K;
  drawBars(g, c.rain, 24, ChartRect{cx, ry, half, rh}, 0, bs);
  drawProbBars(g, c.pop, 24, ChartRect{nowX, ry, half, rh}, COL_M, 4);
  txt(g, cx - 8, ry + rh - 2, "rain", AtkR14, COL_D, ALIGN_RIGHT);
  float total = 0;
  for (int i = 0; i < 24; i++) total += c.rain[i];
  char note[48];
  if (c.anyHist && total >= 0.005f) {
    snprintf(note, sizeof note, "%.2f in last 24 h", total);
    txt(g, (int)(cx + 2 * bw), ry + 14, note, AtkR15, COL_D);
  }
  popNoteText(note, sizeof note, c);
  if (note[0]) txt(g, cx + cw - 4, ry + 14, note, AtkR15, COL_D, ALIGN_RIGHT);

  // lightning row
  const int ly = ry + rh + 16;
  txt(g, cx - 8, ly + 5, "strikes", AtkR14, COL_D, ALIGN_RIGHT);
  drawLightningRow(g, c.strikes, 24, ChartRect{cx, ly - 10, half, 34}, AtkB13, AtkR14,
                   c.anyHist ? "no lightning in 24 h" : "lightning history unavailable");

  // time axis
  HourAxis hx{c.startEpoch, 48, 6, 24, CHART_TZ_LOCAL};
  drawHourAxis(g, cx, cw, 494, hx, AtkR15, AtkB15);
}

void drawLiveInfo(Gfx& g, const WeatherData& d, int64_t now) {
  rule(g, 504);
  char line[200] = "", item[64], a[16];
  if (d.dailyCount > 0 && d.daily[0].sunrise) { fmtClockShort(a, sizeof a, d.daily[0].sunrise); snprintf(item, sizeof item, "Sunrise %s", a); addItem(line, sizeof line, item); }
  if (d.dailyCount > 0 && d.daily[0].sunset) { fmtClockShort(a, sizeof a, d.daily[0].sunset); snprintf(item, sizeof item, "Sunset %s", a); addItem(line, sizeof line, item); }
  if (isfinite(d.cur.uv)) { snprintf(item, sizeof item, "UV %d", (int)lroundf(d.cur.uv)); addItem(line, sizeof line, item); }
  rainTotalsText(item, sizeof item, d, now);
  addItem(line, sizeof line, item);
  stationBatteryText(item, sizeof item, d.cur.stationBatteryV);
  addItem(line, sizeof line, item);
  if (!line[0]) snprintf(line, sizeof line, "--");
  drawInfoLine(g, 529, line);
}

// ---------------------------------------------------------------------------------------------
// Overnight pieces
// ---------------------------------------------------------------------------------------------
struct NightRow { const char* label; char time[16]; char value[24]; };

void drawNightRows(Gfx& g, const DaySummary& s) {
  bool v = s.valid;
  NightRow rows[6] = {};
  char t[16];
  rows[0].label = "High";      rows[1].label = "Low";    rows[2].label = "Rain";
  rows[3].label = "Peak gust"; rows[4].label = "Lightning";
  char at[24] = "At --";
  if (v && s.lastTime) {
    struct tm lt;
    if (localTm(s.lastTime, &lt)) {
      int h = lt.tm_hour + (lt.tm_min >= 30 ? 1 : 0);
      h %= 24;
      formatHourLabel(t, sizeof t, h);
      snprintf(at, sizeof at, "At %s", t);
    }
  }
  rows[5].label = at;

  if (v && isfinite(s.highF)) { fmtClock(rows[0].time, sizeof rows[0].time, s.highTime); fTemp(rows[0].value, sizeof rows[0].value, s.highF); }
  else snprintf(rows[0].value, sizeof rows[0].value, "--");
  if (v && isfinite(s.lowF)) { fmtClock(rows[1].time, sizeof rows[1].time, s.lowTime); fTemp(rows[1].value, sizeof rows[1].value, s.lowF); }
  else snprintf(rows[1].value, sizeof rows[1].value, "--");
  if (v && isfinite(s.rainIn)) snprintf(rows[2].value, sizeof rows[2].value, "%.2f in", s.rainIn); else snprintf(rows[2].value, sizeof rows[2].value, "--");
  if (v && isfinite(s.peakGustMph)) { fmtClock(rows[3].time, sizeof rows[3].time, s.peakGustTime); snprintf(rows[3].value, sizeof rows[3].value, "%d mph", (int)lroundf(s.peakGustMph)); }
  else snprintf(rows[3].value, sizeof rows[3].value, "--");
  if (!v) snprintf(rows[4].value, sizeof rows[4].value, "--");
  else if (s.strikes == 0) snprintf(rows[4].value, sizeof rows[4].value, "none");
  else snprintf(rows[4].value, sizeof rows[4].value, "%u strikes", (unsigned)s.strikes);
  if (v && isfinite(s.lastTempF)) { snprintf(rows[5].time, sizeof rows[5].time, "last reading"); fTemp(rows[5].value, sizeof rows[5].value, s.lastTempF); }
  else snprintf(rows[5].value, sizeof rows[5].value, "--");

  for (int i = 0; i < 6; i++) {
    int y = 140 + i * 46;
    txt(g, 28, y, rows[i].label, AtkB22, COL_K);
    if (rows[i].time[0] && !isDash(rows[i].time)) txt(g, 150, y, rows[i].time, AtkR17, COL_D);
    static const EFont* const vf[] = {&AtkB32, &AtkB22};
    txtFit(g, 430, y + 2, rows[i].value, vf, 2, 270, COL_K, ALIGN_RIGHT);
    if (i < 5) g.hline(28, y + 16, 402, COL_XL);
  }
}

void drawNightSpark(Gfx& g, const WeatherData& d, const ScreenStatus& st, int64_t now, int64_t dateRef, OvernightTitles titles) {
  txt(g, 28, 420, titles == TITLES_TODAY_TOMORROW ? "Today's temperature" : "Yesterday's temperature", AtkR15, COL_D);
  int64_t mid = localMidnight(dateRef);
  int endHour = 23;
  if (titles == TITLES_TODAY_TOMORROW) {
    struct tm nt;
    if (localTm(now, &nt)) endHour = nt.tm_hour + (nt.tm_min >= 30 ? 1 : 0);
    if (endHour < 2) endHour = 2;
    if (endHour > 23) endHour = 23;
  }
  float v[24];
  for (int i = 0; i < 24; i++) v[i] = NAN;
  if (d.have & HAVE_HISTORY) {
    for (int j = 0; j < HIST_BUCKETS; j++) {
      if (!d.hist[j].valid) continue;
      int64_t hs = (int64_t)d.histStartEpoch + (int64_t)j * 3600;
      int h = (int)((hs - mid) / 3600);
      if (hs < mid || h < 0 || h > endHour) continue;
      v[h] = d.hist[j].tempF;
    }
  }
  if (d.today.valid && d.today.lastTime && isfinite(d.today.lastTempF)) {
    int h = (int)(((int64_t)d.today.lastTime - mid) / 3600);
    if (h >= 0 && h <= endHour && !isfinite(v[h])) v[h] = d.today.lastTempF;
  }
  int fin = 0;
  for (int i = 0; i <= endHour; i++) if (isfinite(v[i])) fin++;
  if (fin >= 2) drawSparkline(g, v, endHour + 1, ChartRect{28, 430, 402, 50}, 3, COL_XL, true, 4);
  else txt(g, 229, 462, "not enough data", AtkR17, COL_M, ALIGN_CENTER);
  for (int h = 0; h < endHour - 2; h += 6) {
    char lb[16];
    formatHourLabel(lb, sizeof lb, h);
    int x = 28 + (int)lroundf((float)h / endHour * 402);
    txt(g, x, 498, lb, AtkR14, COL_D, h == 0 ? ALIGN_LEFT : ALIGN_CENTER);
  }
  char lb[16];
  formatHourLabel(lb, sizeof lb, endHour);
  txt(g, 430, 498, lb, AtkR14, COL_D, ALIGN_RIGHT);
  (void)st;
}

// Picks the daily entry (and its date) for the right column and fills the hourly 5 AM - 10 PM series.
void drawNightRight(Gfx& g, const WeatherData& d, int64_t now, OvernightTitles titles, int64_t* sunriseOut) {
  int idx = titles == TITLES_TODAY_TOMORROW ? 1 : 0;
  bool have = d.dailyCount > idx;
  const DailyFc* df = have ? &d.daily[idx] : nullptr;
  int64_t dayRef = have && df->dayStart ? (int64_t)df->dayStart + 12 * 3600 : now + (int64_t)idx * 86400;
  struct tm dt;
  localTm(dayRef, &dt);
  char title[48], dow[8], mon[8];
  snprintf(dow, sizeof dow, "%s", dowName(dt.tm_wday)); upperAscii(dow);
  snprintf(mon, sizeof mon, "%s", monName(dt.tm_mon)); upperAscii(mon);
  snprintf(title, sizeof title, "%s " MID_DOT " %s %s %d", titles == TITLES_TODAY_TOMORROW ? "TOMORROW" : "TODAY", dow, mon, dt.tm_mday);
  txt(g, 492, 92, title, AtkB15, COL_D, ALIGN_LEFT, 2);
  if (sunriseOut) *sunriseOut = have ? df->sunrise : 0;

  // hourly series for that date
  float temp[18], pop[18], gustMax = NAN;
  for (int i = 0; i < 18; i++) { temp[i] = NAN; pop[i] = NAN; }
  for (int i = 0; i < d.hourlyCount && i < HOURLY_FC_MAX; i++) {
    const HourlyFc& h = d.hourly[i];
    struct tm ht;
    if (!localTm(h.time, &ht) || ht.tm_yday != dt.tm_yday || ht.tm_year != dt.tm_year) continue;
    if (isfinite(h.gustMph) && (!isfinite(gustMax) || h.gustMph > gustMax)) gustMax = h.gustMph;
    if (ht.tm_hour < 5 || ht.tm_hour > 22) continue;
    temp[ht.tm_hour - 5] = h.tempF;
    pop[ht.tm_hour - 5] = (float)h.precipProbPct;
  }

  // forecast icon + hi / lo
  if (have) drawIcon(g, df->icon, 548, 160, 0.95f);
  char hi[16], lo[24];
  fTemp(hi, sizeof hi, have ? df->highF : NAN);
  char lt[16]; fTemp(lt, sizeof lt, have ? df->lowF : NAN);
  snprintf(lo, sizeof lo, "/ %s", lt);
  struct HiLo { const EFont* h; const EFont* l; };
  static const HiLo opts[] = {{&AtkB84, &AtkR44}, {&AtkB76, &AtkR44}, {&AtkB50, &AtkR22}};
  const HiLo* pick = &opts[2];
  for (const HiLo& o : opts) if (616 + tw(*o.h, hi) + 14 + tw(*o.l, lo) <= 936) { pick = &o; break; }
  int hw = txt(g, 616, 186, hi, *pick->h, COL_K);
  txt(g, 616 + hw + 14, 186, lo, *pick->l, COL_D);

  // conditions, chance, wind note
  static const EFont* const cf[] = {&AtkB22, &AtkB20, &AtkB18};
  char dcond[COND_LEN + 1] = "";
  if (have) trimCopy(dcond, sizeof dcond, df->conditions);
  txtFit(g, 492, 236, dcond[0] ? dcond : (have ? "--" : "Forecast unavailable"), cf, 3, 444, COL_K);
  char sub[96] = "", part[48];
  if (have) { snprintf(part, sizeof part, "%d%% chance", (int)df->precipProbPct); addItem(sub, sizeof sub, part); }
  if (isfinite(gustMax) && gustMax >= 15) {
    snprintf(part, sizeof part, "%sgusts to %d mph", gustMax >= 30 ? "windy, " : gustMax >= 20 ? "breezy, " : "", (int)lroundf(gustMax));
    addItem(sub, sizeof sub, part);
  }
  static const EFont* const sf[] = {&AtkR18, &AtkR17, &AtkR16};
  txtFit(g, 492, 264, sub, sf, 3, 444, COL_D);

  // hourly chart
  const int cx = 530, cy = 292, cw = 400, ch = 100;
  AxisRange ax = autoRange(temp, 18, 5, 3);
  ChartRect cr{cx, cy, cw, ch};
  if (ax.valid) {
    drawGrid(g, cr, ax, AtkR14, 0, COL_XL, COL_D);
    LineStyle ls; ls.thickness = 3; ls.dashed = true; ls.dash = 10; ls.gap = 7;
    drawLineSeries(g, temp, 18, cr, ax, ls);
    drawHiLoMarker(g, temp, 18, cr, ax, true, AtkB18, 0);
  } else {
    txt(g, cx + cw / 2, cy + ch / 2 + 6, "no forecast", AtkR20, COL_M, ALIGN_CENTER);
  }
  const int ry = cy + ch + 10, rh = 36;
  g.hline(cx, ry + rh, cw, COL_L);
  drawProbBars(g, pop, 18, ChartRect{cx, ry, cw, rh}, COL_M, 4);
  txt(g, cx - 6, ry + rh - 2, "rain %", AtkR13, COL_D, ALIGN_RIGHT);
  static const int axisHours[] = {5, 9, 13, 17, 21};
  for (int i = 0; i < 5; i++) {
    char lb[16];
    formatHourLabel(lb, sizeof lb, axisHours[i]);
    int x = cx + (int)lroundf((float)(axisHours[i] - 5) / 17 * cw);
    txt(g, x, 470, lb, AtkR14, COL_D, i == 0 ? ALIGN_LEFT : ALIGN_CENTER);
  }
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Debug hooks
// ---------------------------------------------------------------------------------------------
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
  SmartSlot slot = pickSmartSlot(d, now);
  drawLiveHeader(g, st, now, dataEpoch);
  drawCurrent(g, d, slot);
  rule(g, 242);
  drawLiveChartBlock(g, d, now);
  drawLiveInfo(g, d, now);
}

void drawOvernight(Gfx& g, const WeatherData& d, const ScreenStatus& st, OvernightTitles titles) {
  g.clear(COL_PAPER);
  int64_t now = pickNow(d, st);
  int64_t dataEpoch = pickDataEpoch(d, st, now);

  // black header bar
  g.fillRect(0, 0, PANEL_WIDTH, 56, COL_K);
  drawHeaderLabel(g, 24, 36, COL_PAPER);
  drawMoon(g, 526, 28, 12, COL_PAPER, COL_K);
  char next[64];
  snprintf(next, sizeof next, "Paused overnight " MID_DOT " next update %d:%02d AM", QUIET_END_HOUR, QUIET_END_MIN);
  txt(g, 936, 36, next, AtkB20, COL_PAPER, ALIGN_RIGHT);

  // left column title
  int64_t dateRef = d.today.valid && d.today.lastTime ? (int64_t)d.today.lastTime
                                                      : (titles == TITLES_TODAY_TOMORROW ? now : now - 86400);
  struct tm lt;
  localTm(dateRef, &lt);
  char dow[8], mon[8], title[48];
  snprintf(dow, sizeof dow, "%s", dowName(lt.tm_wday)); upperAscii(dow);
  snprintf(mon, sizeof mon, "%s", monName(lt.tm_mon)); upperAscii(mon);
  snprintf(title, sizeof title, "%s " MID_DOT " %s %s %d", titles == TITLES_TODAY_TOMORROW ? "TODAY" : "YESTERDAY", dow, mon, lt.tm_mday);
  txt(g, 28, 92, title, AtkB15, COL_D, ALIGN_LEFT, 2);
  drawNightRows(g, d.today);
  drawNightSpark(g, d, st, now, dateRef, titles);

  g.fillRect(461, 76, 2, 424, COL_L);

  int64_t sunrise = 0;
  drawNightRight(g, d, now, titles, &sunrise);

  // bottom line
  rule(g, 506);
  char line[200] = "", item[64], a[16];
  fmtClock(a, sizeof a, dataEpoch);
  snprintf(item, sizeof item, "Last data %s", a);
  addItem(line, sizeof line, item);
  if (sunrise) { fmtClockShort(a, sizeof a, sunrise); snprintf(item, sizeof item, "Sunrise %s", a); addItem(line, sizeof line, item); }
  rainTotalsText(item, sizeof item, d, now);
  addItem(line, sizeof line, item);
  stationBatteryText(item, sizeof item, d.cur.stationBatteryV);
  addItem(line, sizeof line, item);
  drawInfoLine(g, 530, line);
}

void drawChargeMe(Gfx& g, const ScreenStatus& st) {
  g.clear(COL_PAPER);
  drawHeaderLabel(g, 28, 36, COL_D);
  g.roundRect(340, 120, 260, 130, 16, COL_K, 8);
  g.fillRoundRect(604, 160, 20, 50, 4, COL_K);
  g.fillRoundRect(356, 136, 22, 98, 3, COL_K);
  txt(g, 480, 350, "Charge me", AtkB76, COL_K, ALIGN_CENTER);
  char line[96];
  if (st.batteryVolts > 0.5f) snprintf(line, sizeof line, "Battery %.2f V " MID_DOT " weather updates stopped to protect the cell", st.batteryVolts);
  else snprintf(line, sizeof line, "Battery low " MID_DOT " weather updates stopped to protect the cell");
  static const EFont* const f22[] = {&AtkR22, &AtkR20, &AtkR18};
  txtFit(g, 480, 396, line, f22, 3, 900, COL_D, ALIGN_CENTER);
  if (st.lastGoodEpoch > 0) {
    char date[24], clock[16];
    fmtDate(date, sizeof date, st.lastGoodEpoch);
    fmtClock(clock, sizeof clock, st.lastGoodEpoch);
    snprintf(line, sizeof line, "Last data: %s " MID_DOT " %s", date, clock);
  } else {
    snprintf(line, sizeof line, "No weather data yet");
  }
  txt(g, 480, 446, line, AtkR20, COL_D, ALIGN_CENTER);
  txt(g, 480, 476, "Updates restart on their own once the battery is charged.", AtkR20, COL_D, ALIGN_CENTER);
}

void drawError(Gfx& g, const char* reason, const ScreenStatus& st) {
  g.clear(COL_PAPER);
  drawHeaderLabel(g, 28, 36, COL_D);
  drawIcon(g, ICON_CLOUDY, 480, 150, 1.2f);
  g.fillCircle(552, 196, 26, COL_K);
  txt(g, 552, 208, "!", AtkB32, COL_PAPER, ALIGN_CENTER);
  txt(g, 480, 320, "No weather data", AtkB50, COL_K, ALIGN_CENTER);
  static const EFont* const rf[] = {&AtkR22, &AtkR20, &AtkR18};
  txtFit(g, 480, 372, reason && *reason ? reason : "Unknown error", rf, 3, 880, COL_D, ALIGN_CENTER);
  if (st.nowEpoch > 0) {
    char date[24], clock[16], line[64];
    fmtDate(date, sizeof date, st.nowEpoch);
    fmtClock(clock, sizeof clock, st.nowEpoch);
    snprintf(line, sizeof line, "%s " MID_DOT " %s", date, clock);
    txt(g, 480, 420, line, AtkR20, COL_D, ALIGN_CENTER);
  }
  txt(g, 480, 466, "Will try again at the next update.", AtkR20, COL_D, ALIGN_CENTER);
  if (st.batteryPercent >= 0) drawBattery(g, 894, 19, st.batteryPercent);
}
