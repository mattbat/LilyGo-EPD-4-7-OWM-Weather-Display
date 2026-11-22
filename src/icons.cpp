#include "icons.h"
#include <math.h>

static const float kPi = 3.14159265358979f;

int iconStroke(float scale) {
  int s = (int)lroundf(3.0f * scale);
  return s < 2 ? 2 : s;
}

// ---------------------------------------------------------------------------------------------
// Cloud outline: the mockup's SVG path flattened once into a unit-scale polygon.
//   M -40 20  A20 -> (-40,-20) | A28 -> (12,-30) | A18 -> (40,-8) | A14 -> (40,20) | Z
// (all arcs sweep=1, small arc)
// ---------------------------------------------------------------------------------------------
namespace {
const int kCloudMax = 96;
Pt   g_cloud[kCloudMax];
int  g_cloudN = 0;

void appendArc(Pt p0, Pt p1, float r) {
  float dx = p1.x - p0.x, dy = p1.y - p0.y;
  float d = sqrtf(dx * dx + dy * dy) * 0.5f;
  if (r < d) r = d;
  float h = sqrtf(r * r - d * d);
  Pt mid = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
  Pt u = {-dy / (2 * d), dx / (2 * d)};
  float best = kPi, bestA0 = atan2f(p0.y - mid.y, p0.x - mid.x);
  Pt bc = mid;
  for (int s = -1; s <= 1; s += 2) {
    Pt c = {mid.x + s * h * u.x, mid.y + s * h * u.y};
    float a0 = atan2f(p0.y - c.y, p0.x - c.x), a1 = atan2f(p1.y - c.y, p1.x - c.x);
    float delta = a1 - a0;
    while (delta < 0) delta += 2 * kPi;
    while (delta >= 2 * kPi) delta -= 2 * kPi;
    if (delta <= kPi + 1e-3f) { best = delta; bestA0 = a0; bc = c; break; }
  }
  int segs = (int)ceilf(best / (kPi / 18.0f));
  if (segs < 2) segs = 2;
  for (int i = 1; i <= segs && g_cloudN < kCloudMax; i++) {
    float a = bestA0 + best * i / segs;
    g_cloud[g_cloudN++] = {bc.x + r * cosf(a), bc.y + r * sinf(a)};
  }
}

void buildCloud() {
  if (g_cloudN) return;
  g_cloud[g_cloudN++] = {-40, 20};
  appendArc({-40, 20}, {-40, -20}, 20);
  appendArc({-40, -20}, {12, -30}, 28);
  appendArc({12, -30}, {40, -8}, 18);
  appendArc({40, -8}, {40, 20}, 14);
}
}  // namespace

void drawCloud(Gfx& g, float cx, float cy, float scale, uint8_t fill, int stroke, uint8_t ink) {
  buildCloud();
  Pt p[kCloudMax + 1];
  for (int i = 0; i < g_cloudN; i++) p[i] = {cx + g_cloud[i].x * scale, cy + g_cloud[i].y * scale};
  g.fillPolygon(p, g_cloudN, fill);
  p[g_cloudN] = p[0];
  g.polyline(p, g_cloudN + 1, stroke, ink, CAP_ROUND);
}

void drawSun(Gfx& g, int cx, int cy, int r, int stroke, uint8_t fill, uint8_t ink) {
  g.fillCircle(cx, cy, r + stroke / 2, ink);
  int ri = r - (stroke + 1) / 2;
  if (ri > 0) g.fillCircle(cx, cy, ri, fill);
  for (int i = 0; i < 8; i++) {
    float a = i * kPi / 4;
    float r1 = r * 1.4f, r2 = r * 1.85f;
    g.line((int)lroundf(cx + r1 * cosf(a)), (int)lroundf(cy + r1 * sinf(a)),
           (int)lroundf(cx + r2 * cosf(a)), (int)lroundf(cy + r2 * sinf(a)), stroke, ink, CAP_ROUND);
  }
}

void drawBolt(Gfx& g, float cx, float cy, float scale, uint8_t grey) {
  static const Pt b[7] = {{1, -10}, {-6, 1}, {-1, 1}, {-3, 10}, {6, -2}, {1, -2}, {3, -10}};
  Pt p[7];
  for (int i = 0; i < 7; i++) p[i] = {cx + b[i].x * scale, cy + b[i].y * scale};
  g.fillPolygon(p, 7, grey);
}

void drawMoon(Gfx& g, int cx, int cy, int r, uint8_t fg, uint8_t bg) {
  g.fillCircle(cx, cy, r, fg);
  g.fillCircle(cx + (int)lroundf(r * 0.45f), cy - (int)lroundf(r * 0.3f), (int)lroundf(r * 0.85f), bg);
}

void drawBattery(Gfx& g, int x, int y, int percent, uint8_t grey) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  g.roundRect(x, y, 34, 16, 2, grey, 2);
  g.fillRect(x + 34, y + 5, 3, 6, grey);
  int w = (28 * percent + 50) / 100;
  if (w > 0) g.fillRect(x + 3, y + 3, w, 10, grey);
}

void drawDrops(Gfx& g, int cx, int cy, float scale, uint8_t grey, int n) {
  int sw = iconStroke(scale);
  for (int i = 0; i < n; i++) {
    float off = (i - (n - 1) * 0.5f) * 22 * scale;
    int x = (int)lroundf(cx + off);
    g.line(x, cy, (int)lroundf(x - 8 * scale), (int)lroundf(cy + 18 * scale), sw, grey, CAP_ROUND);
  }
}

void drawSnowflake(Gfx& g, int cx, int cy, int r, int stroke, uint8_t grey) {
  for (int i = 0; i < 3; i++) {
    float a = i * kPi / 3 + kPi / 2;
    int dx = (int)lroundf(r * cosf(a)), dy = (int)lroundf(r * sinf(a));
    g.line(cx - dx, cy - dy, cx + dx, cy + dy, stroke, grey, CAP_ROUND);
  }
}

// ---------------------------------------------------------------------------------------------
// Composite icons
// ---------------------------------------------------------------------------------------------
namespace {
inline int R(float v) { return (int)lroundf(v); }

enum Precip { P_NONE, P_RAIN, P_SNOW, P_SLEET, P_THUNDER };

void sunOrMoon(Gfx& g, bool night, float cx, float cy, float r, int sw, uint8_t bg) {
  if (night) drawMoon(g, R(cx), R(cy), R(r), COL_K, bg);
  else drawSun(g, R(cx), R(cy), R(r), sw, COL_PAPER);
}

// Two small precipitation marks centred on (cx, cy).
void smallPrecip(Gfx& g, Precip p, float cx, float cy, float sc, int sw) {
  for (int i = 0; i < 2; i++) {
    float x = cx + (i - 0.5f) * 24 * sc;
    switch (p) {
      case P_RAIN:
        g.line(R(x), R(cy - 8 * sc), R(x - 6 * sc), R(cy + 8 * sc), sw, COL_K);
        break;
      case P_SNOW: drawSnowflake(g, R(x), R(cy), R(6 * sc), sw, COL_K); break;
      case P_SLEET:
        if (i == 0) g.line(R(x), R(cy - 8 * sc), R(x - 6 * sc), R(cy + 8 * sc), sw, COL_K);
        else drawSnowflake(g, R(x), R(cy), R(6 * sc), sw, COL_K);
        break;
      case P_THUNDER:
        if (i == 0) drawBolt(g, x, cy, 1.1f * sc, COL_K);
        else g.line(R(x), R(cy - 8 * sc), R(x - 6 * sc), R(cy + 8 * sc), sw, COL_K);
        break;
      default: break;
    }
  }
}

// Sun/moon peeking behind a cloud. With precipitation the group is smaller and shifted up.
void partly(Gfx& g, bool night, Precip p, int cx, int cy, float sc, uint8_t bg) {
  int sw = iconStroke(sc);
  if (p == P_NONE) {
    sunOrMoon(g, night, cx - 18 * sc, cy - 18 * sc, 18 * sc, sw, bg);
    drawCloud(g, cx + 6 * sc, cy + 8 * sc, sc * 0.85f, COL_PAPER, sw);
  } else {
    sunOrMoon(g, night, cx - 22 * sc, cy - 28 * sc, 15 * sc, sw, bg);
    drawCloud(g, cx + 8 * sc, cy - 6 * sc, sc * 0.8f, COL_PAPER, sw);
    smallPrecip(g, p, cx + 8 * sc, cy + 34 * sc, sc, sw);
  }
}

void rainy(Gfx& g, int cx, int cy, float sc) {
  int sw = iconStroke(sc);
  drawCloud(g, cx, cy - 8 * sc, sc, COL_M, sw);
  drawDrops(g, cx, R(cy + 22 * sc), sc, COL_K, 3);
}

void snowy(Gfx& g, int cx, int cy, float sc) {
  int sw = iconStroke(sc);
  drawCloud(g, cx, cy - 10 * sc, sc, COL_XL, sw);
  int r = R(7 * sc);
  drawSnowflake(g, R(cx - 22 * sc), R(cy + 30 * sc), r, sw, COL_K);
  drawSnowflake(g, cx, R(cy + 40 * sc), r, sw, COL_K);
  drawSnowflake(g, R(cx + 22 * sc), R(cy + 30 * sc), r, sw, COL_K);
}

void sleety(Gfx& g, int cx, int cy, float sc) {
  int sw = iconStroke(sc);
  drawCloud(g, cx, cy - 10 * sc, sc, COL_M, sw);
  g.line(R(cx - 22 * sc), R(cy + 24 * sc), R(cx - 30 * sc), R(cy + 42 * sc), sw, COL_K);
  drawSnowflake(g, cx, R(cy + 36 * sc), R(7 * sc), sw, COL_K);
  g.line(R(cx + 22 * sc), R(cy + 24 * sc), R(cx + 14 * sc), R(cy + 42 * sc), sw, COL_K);
}

void stormy(Gfx& g, int cx, int cy, float sc) {
  drawCloud(g, cx, cy - 8 * sc, sc, COL_M, iconStroke(sc));
  drawBolt(g, cx + 2 * sc, cy + 30 * sc, 2.2f * sc, COL_K);
}

void foggy(Gfx& g, int cx, int cy, float sc) {
  int sw = iconStroke(sc);
  drawCloud(g, cx, cy - 20 * sc, sc * 0.85f, COL_XL, sw);
  static const float half[3] = {46, 34, 42};
  static const float dx[3] = {0, 6, -4};
  for (int i = 0; i < 3; i++) {
    int y = R(cy + (14 + i * 14) * sc);
    g.line(R(cx + (dx[i] - half[i]) * sc), y, R(cx + (dx[i] + half[i]) * sc), y, sw, COL_K);
  }
}

// One wind streak: straight run ending in a curl (up = curl rises, down = curl drops).
void windLine(Gfx& g, float x0, float x1, float y, float curlR, bool up, int sw) {
  Pt p[24];
  int n = 0;
  p[n++] = {x0, y};
  float cxm = x1, cym = up ? y - curlR : y + curlR;
  for (int i = 0; i <= 14; i++) {
    float t = i / 14.0f;
    float a = up ? (kPi / 2 - t * (kPi * 1.45f)) : (-kPi / 2 + t * (kPi * 1.45f));
    p[n++] = {cxm + curlR * cosf(a), cym + curlR * sinf(a)};
  }
  g.polyline(p, n, sw, COL_K, CAP_ROUND);
}

void windy(Gfx& g, int cx, int cy, float sc) {
  int sw = iconStroke(sc);
  windLine(g, cx - 46 * sc, cx + 20 * sc, cy - 16 * sc, 9 * sc, true, sw);
  g.line(R(cx - 52 * sc), R(cy + 4 * sc), R(cx + 40 * sc), R(cy + 4 * sc), sw, COL_K);
  windLine(g, cx - 30 * sc, cx + 14 * sc, cy + 24 * sc, 8 * sc, false, sw);
}
}  // namespace

void drawIcon(Gfx& g, IconKey key, int cx, int cy, float scale, uint8_t bg) {
  if (scale < 0.1f) scale = 0.1f;
  int sw = iconStroke(scale);
  switch (key) {
    case ICON_CLEAR_DAY: drawSun(g, cx, cy, R(22 * scale), sw, COL_PAPER); break;
    case ICON_CLEAR_NIGHT: drawMoon(g, R(cx - 4 * scale), R(cy + 2 * scale), R(30 * scale), COL_K, bg); break;
    case ICON_PARTLY_DAY: partly(g, false, P_NONE, cx, cy, scale, bg); break;
    case ICON_PARTLY_NIGHT: partly(g, true, P_NONE, cx, cy, scale, bg); break;
    case ICON_CLOUDY: drawCloud(g, cx, cy, scale, COL_XL, sw); break;
    case ICON_RAINY: rainy(g, cx, cy, scale); break;
    case ICON_POSSIBLY_RAINY_DAY: partly(g, false, P_RAIN, cx, cy, scale, bg); break;
    case ICON_POSSIBLY_RAINY_NIGHT: partly(g, true, P_RAIN, cx, cy, scale, bg); break;
    case ICON_THUNDERSTORM: stormy(g, cx, cy, scale); break;
    case ICON_POSSIBLY_THUNDER_DAY: partly(g, false, P_THUNDER, cx, cy, scale, bg); break;
    case ICON_POSSIBLY_THUNDER_NIGHT: partly(g, true, P_THUNDER, cx, cy, scale, bg); break;
    case ICON_SNOW: snowy(g, cx, cy, scale); break;
    case ICON_POSSIBLY_SNOW_DAY: partly(g, false, P_SNOW, cx, cy, scale, bg); break;
    case ICON_POSSIBLY_SNOW_NIGHT: partly(g, true, P_SNOW, cx, cy, scale, bg); break;
    case ICON_SLEET: sleety(g, cx, cy, scale); break;
    case ICON_POSSIBLY_SLEET_DAY: partly(g, false, P_SLEET, cx, cy, scale, bg); break;
    case ICON_POSSIBLY_SLEET_NIGHT: partly(g, true, P_SLEET, cx, cy, scale, bg); break;
    case ICON_FOGGY: foggy(g, cx, cy, scale); break;
    case ICON_WINDY: windy(g, cx, cy, scale); break;
    case ICON_UNKNOWN:
    default: drawCloud(g, cx, cy, scale, COL_XL, sw); break;
  }
}
