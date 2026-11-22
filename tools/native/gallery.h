// Icon + chart gallery (task 4): out/gallery.png = four 960x540 pages stacked vertically.
// Header-only helper for main_native.cpp (not compiled by the firmware or by unit tests).
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gfx.h"
#include "fonts/fonts.h"
#include "icons.h"
#include "charts.h"

struct NamedIcon { IconKey key; const char* name; };
static const NamedIcon kIcons[] = {
  {ICON_CLEAR_DAY, "clear-day"}, {ICON_CLEAR_NIGHT, "clear-night"}, {ICON_PARTLY_DAY, "partly-cloudy-day"},
  {ICON_PARTLY_NIGHT, "partly-cloudy-night"}, {ICON_CLOUDY, "cloudy"}, {ICON_RAINY, "rainy"},
  {ICON_POSSIBLY_RAINY_DAY, "possibly-rainy-day"}, {ICON_POSSIBLY_RAINY_NIGHT, "possibly-rainy-night"},
  {ICON_THUNDERSTORM, "thunderstorm"}, {ICON_POSSIBLY_THUNDER_DAY, "possibly-thunder-day"},
  {ICON_POSSIBLY_THUNDER_NIGHT, "possibly-thunder-night"}, {ICON_SNOW, "snow"},
  {ICON_POSSIBLY_SNOW_DAY, "possibly-snow-day"}, {ICON_POSSIBLY_SNOW_NIGHT, "possibly-snow-night"},
  {ICON_SLEET, "sleet"}, {ICON_POSSIBLY_SLEET_DAY, "possibly-sleet-day"},
  {ICON_POSSIBLY_SLEET_NIGHT, "possibly-sleet-night"}, {ICON_FOGGY, "foggy"}, {ICON_WINDY, "windy"},
  {ICON_UNKNOWN, "unknown"}};
static const int kIconCount = (int)(sizeof kIcons / sizeof kIcons[0]);

static void galleryIcons(Gfx& g, float scale, int cols, int cellW, int cellH, int top, const EFont& label) {
  for (int i = 0; i < kIconCount; i++) {
    int col = i % cols, row = i / cols;
    int x = col * cellW + cellW / 2, y = top + row * cellH + (int)(cellH * 0.45f);
    drawIcon(g, kIcons[i].key, x, y, scale);
    g.drawText(x, top + (row + 1) * cellH - 8, kIcons[i].name, label, COL_D, ALIGN_CENTER);
  }
}

static void galleryPageIcons1(Gfx& g) {
  g.clear(15);
  g.drawText(12, 22, "Icons at scale 1.0", AtkB20, COL_K);
  galleryIcons(g, 1.0f, 5, 192, 126, 30, AtkR20);
}

static void galleryPageIcons2(Gfx& g) {
  g.clear(15);
  g.drawText(12, 22, "Icons at scale 0.55", AtkB20, COL_K);
  galleryIcons(g, 0.55f, 6, 160, 86, 30, AtkR20);
}

static void galleryPageIcons3(Gfx& g) {
  g.clear(15);
  // on the forecast background (moon cut-out must use the same bg)
  g.fillRect(0, 10, PANEL_WIDTH, 70, COL_FC);
  g.drawText(12, 26, "on forecast bg (14), scale 0.45", AtkR20, COL_D);
  for (int i = 0; i < 8; i++) drawIcon(g, kIcons[i].key, 80 + i * 110, 56, 0.45f, COL_FC);
  // standalone helpers
  g.drawText(12, 110, "Helpers: bolt, moon, battery 0/25/50/80/100 (+ inverted), drops, snowflakes", AtkB20, COL_K);
  drawBolt(g, 40, 160, 2.0f, COL_K);
  drawBolt(g, 90, 160, 3.0f, COL_M);
  drawMoon(g, 160, 160, 26, COL_K, COL_PAPER);
  drawMoon(g, 230, 160, 14, COL_D, COL_PAPER);
  static const int pcts[] = {0, 25, 50, 80, 100};
  for (int i = 0; i < 5; i++) drawBattery(g, 290 + i * 54, 152, pcts[i]);
  g.fillRect(580, 130, 120, 60, COL_K);
  drawBattery(g, 590, 152, 80, COL_PAPER);
  drawDrops(g, 740, 142, 1.0f, COL_K, 3);
  drawSnowflake(g, 810, 160, 10, 3, COL_K);
  drawSnowflake(g, 850, 160, 5, 2, COL_K);
  // one icon across scales 0.3 .. 2.0
  g.drawText(12, 230, "Thunderstorm at scale 0.3 / 0.55 / 1.0 / 1.5 / 2.0", AtkB20, COL_K);
  static const float scales[] = {0.3f, 0.55f, 1.0f, 1.5f, 2.0f};
  int x = 50;
  for (int i = 0; i < 5; i++) {
    int w = (int)(120 * scales[i]);
    drawIcon(g, ICON_THUNDERSTORM, x + w / 2, 360, scales[i]);
    x += w + 40;
  }
}

static void galleryPageCharts(Gfx& g) {
  g.clear(15);
  const uint32_t nowEpoch = 1795291200u;   // Sat 2026-11-21 20:00 UTC = 2 PM at UTC-6
  const int tz = -6 * 3600;
  const int nowHour = 14;
  const int cx = 60, cw = 870, half = cw / 2, nowX = cx + half;

  // ---- 48 h temperature chart, calm (no lightning) ---------------------------------------------
  const int cy = 34, ch = 112;
  float past[25], fut[25], rain[24], pop[24];
  for (int i = 0; i <= 24; i++) {
    int hd = (nowHour - 24 + i + 48) % 24;
    past[i] = 58 + 8 * sinf(2 * 3.14159f * (hd - 9) / 24) + 0.6f * sinf(i * 1.7f);
  }
  past[24] = 68.4f; past[5] = NAN; past[6] = NAN;   // two missing samples -> line breaks
  for (int k = 0; k <= 24; k++) {
    int hd = (nowHour + k) % 24;
    fut[k] = 57.5f + 8 * sinf(2 * 3.14159f * (hd - 8) / 24) - 4 * (k / 24.0f);
    if (k < 24) pop[k] = k < 9 ? 10.f : k < 13 ? 25.f : k < 17 ? 55.f : 65.f;
  }
  fut[0] = 68.4f;
  for (int i = 0; i < 24; i++) rain[i] = (i == 3) ? 0.04f : (i == 4) ? 0.12f : (i == 5) ? 0.02f : 0.0f;
  AxisRange ax = autoRange2(past, 25, fut, 25);
  g.drawText(cx + 4, 24, "MEASURED \xC2\xB7 last 24 h", AtkB20, COL_D, ALIGN_LEFT, 1);
  g.drawText(cx + cw - 4, 24, "FORECAST \xC2\xB7 next 24 h", AtkB20, COL_D, ALIGN_RIGHT, 1);
  g.fillRect(nowX, cy - 4, half, 196, COL_FC);
  ChartRect pr = {cx, cy, half, ch}, fr = {nowX, cy, half, ch};
  drawGrid(g, {cx, cy, cw, ch}, ax, AtkR20);
  LineStyle ls; ls.thickness = 4; ls.fillArea = true;
  drawLineSeries(g, past, 25, pr, ax, ls);
  LineStyle fs; fs.thickness = 4; fs.dashed = true;
  drawLineSeries(g, fut, 25, fr, ax, fs);
  drawNowLine(g, nowX, cy - 6, cy + 196);
  drawEndDot(g, past, 25, pr, ax, 7);
  drawHiLoMarker(g, past, 25, pr, ax, true, AtkB20);
  drawHiLoMarker(g, past, 25, pr, ax, false, AtkB20);
  drawHiLoMarker(g, fut, 25, fr, ax, true, AtkB20);
  drawHiLoMarker(g, fut, 25, fr, ax, false, AtkB20);
  // rain band: measured bars (auto-scaled with a 0.05 in floor) + forecast probability bars
  const int ry = cy + ch + 10, rh = 40;
  BarStyle bs;
  drawBars(g, rain, 24, {cx, ry, half, rh}, 0, bs);
  drawProbBars(g, pop, 24, {nowX, ry, half, rh});
  g.drawText(cx - 8, ry + rh - 2, "rain", AtkR20, COL_D, ALIGN_RIGHT);
  uint16_t none[24] = {0};
  const int ly = ry + rh + 6;
  g.drawText(cx - 8, ly + 16, "strikes", AtkR20, COL_D, ALIGN_RIGHT);
  drawLightningRow(g, none, 24, {cx, ly, half, 34}, AtkB20, AtkR20);
  HourAxis hx = {nowEpoch - 24 * 3600u, 48, 6, 24, tz};
  drawHourAxis(g, cx, cw, cy + 196 + 22, hx, AtkR20, AtkB20);

  // ---- storm: heavy rain auto-scale and lightning row with strikes -----------------------------
  const int y2 = 292;
  uint16_t strikes[24] = {0};
  strikes[19] = 3; strikes[20] = 14; strikes[21] = 9; strikes[22] = 2; strikes[10] = 120;
  g.drawText(12, y2 - 8, "Bars auto-scale (left: heavy rain + lightning row, right: floor keeps small rain small)", AtkR20, COL_D);
  float heavy[24] = {0};
  heavy[18] = 0.15f; heavy[19] = 0.65f; heavy[20] = 0.32f; heavy[21] = 0.18f;
  drawBars(g, heavy, 24, {cx, y2, half, 40}, 0, bs);
  g.drawText(cx - 8, y2 + 38, "rain", AtkR20, COL_D, ALIGN_RIGHT);
  g.drawText(cx - 8, y2 + 62, "strikes", AtkR20, COL_D, ALIGN_RIGHT);
  drawLightningRow(g, strikes, 24, {cx, y2 + 46, half, 34}, AtkB20, AtkR20);
  float tiny[24] = {0};
  tiny[7] = 0.01f; tiny[8] = 0.03f; tiny[9] = 0.05f; tiny[10] = 0.20f;
  drawBars(g, tiny, 24, {nowX + 30, y2, half - 30, 40}, 0.2f, bs);
  g.drawText(nowX + 30, y2 + 62, "0.01 in still gets a 3 px stub", AtkR20, COL_D);

  // ---- sparklines in tiles ---------------------------------------------------------------------
  const int y3 = 395;
  float wind[24], hum[24], sol[24];
  for (int i = 0; i < 24; i++) {
    wind[i] = 5 + 3 * sinf(i / 3.5f);
    hum[i] = 78 - 20 * sinf(2 * 3.14159f * (i - 8) / 24);
    sol[i] = fmaxf(0, 600 * sinf(3.14159f * (i - 7) / 10));
  }
  struct Tile { const char* name; const float* d; int n; int fill; } tiles[] = {
    {"TEMPERATURE", past, 25, -1}, {"WIND", wind, 24, -1}, {"HUMIDITY", hum, 24, -1}, {"SUN (area)", sol, 24, COL_L}};
  for (int t = 0; t < 4; t++) {
    int x = 20 + t * 232;
    g.roundRect(x, y3, 218, 130, 8, COL_K, 2);
    g.drawText(x + 14, y3 + 26, tiles[t].name, AtkB20, COL_D, ALIGN_LEFT, 1);
    drawSparkline(g, tiles[t].d, tiles[t].n, {x + 14, y3 + 42, 190, 70}, 3, tiles[t].fill);
  }
}

static bool galleryWrite(Gfx* pages, int n, const char* path) {
  const int W = PANEL_WIDTH, H = PANEL_HEIGHT;
  uint8_t* img = (uint8_t*)malloc((size_t)W * H * n);
  if (!img) return false;
  for (int p = 0; p < n; p++)
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) img[(size_t)(p * H + y) * W + x] = (uint8_t)(pages[p].get(x, y) * 17);
  bool ok = stbi_write_png(path, W, H * n, 1, img, W) != 0;
  free(img);
  return ok;
}
