#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline float fclampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void Gfx::begin(uint8_t* buffer) { fb = buffer; }

void Gfx::clear(uint8_t grey) {
  grey &= 0x0F;
  if (fb) memset(fb, (grey << 4) | grey, PANEL_WIDTH * PANEL_HEIGHT / 2);
}

void Gfx::pixel(int x, int y, uint8_t grey) {
  if (!fb || x < 0 || y < 0 || x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return;
  grey &= 0x0F;
  uint8_t* p = &fb[(y * PANEL_WIDTH + x) / 2];
  if (x & 1) *p = (uint8_t)((*p & 0x0F) | (grey << 4));
  else       *p = (uint8_t)((*p & 0xF0) | grey);
}

uint8_t Gfx::get(int x, int y) const {
  if (!fb || x < 0 || y < 0 || x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return 15;
  uint8_t v = fb[(y * PANEL_WIDTH + x) / 2];
  return (x & 1) ? (v >> 4) : (v & 0x0F);
}

// ---------------------------------------------------------------- spans, lines

void Gfx::span(int y, int x0, int x1, uint8_t grey) {
  if (!fb || y < 0 || y >= PANEL_HEIGHT) return;
  x0 = imax(x0, 0);
  x1 = imin(x1, PANEL_WIDTH);
  if (x0 >= x1) return;
  grey &= 0x0F;
  if (x0 & 1) { pixel(x0, y, grey); x0++; }
  if ((x1 & 1) && x1 > x0) { pixel(x1 - 1, y, grey); x1--; }
  if (x0 < x1) memset(&fb[(y * PANEL_WIDTH + x0) / 2], (grey << 4) | grey, (x1 - x0) / 2);
}

void Gfx::hline(int x, int y, int len, uint8_t grey) { if (len > 0) span(y, x, x + len, grey); }

void Gfx::vline(int x, int y, int len, uint8_t grey) {
  for (int i = 0; i < len; i++) pixel(x, y + i, grey);
}

void Gfx::thinLine(int x0, int y0, int x1, int y1, uint8_t grey) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(x0, y0, grey);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Fill every pixel whose centre lies within a stroke of width t around segment a-b
// (continuous coordinates). BUTT/SQUARE ranges are half-open so adjacent pieces tile exactly.
void Gfx::capsule(float ax, float ay, float bx, float by, float t, uint8_t grey, Cap cap) {
  const float r = t * 0.5f;
  float dx = bx - ax, dy = by - ay;
  float len = sqrtf(dx * dx + dy * dy);
  float pad = r * 1.5f + 1.0f;
  int x0 = imax(0, (int)floorf(fminf(ax, bx) - pad)), x1 = imin(PANEL_WIDTH - 1, (int)ceilf(fmaxf(ax, bx) + pad));
  int y0 = imax(0, (int)floorf(fminf(ay, by) - pad)), y1 = imin(PANEL_HEIGHT - 1, (int)ceilf(fmaxf(ay, by) + pad));
  float ux = 0, uy = 0;
  if (len > 1e-6f) { ux = dx / len; uy = dy / len; }
  else { ux = 1; len = 0; }  // degenerate: dot (round) or axis-aligned square
  const float ext = (cap == CAP_SQUARE) ? r : 0.0f;
  const float r2 = r * r;
  for (int y = y0; y <= y1; y++) {
    float py = y + 0.5f - ay;
    for (int x = x0; x <= x1; x++) {
      float px = x + 0.5f - ax;
      bool in;
      if (cap == CAP_ROUND) {
        float s = fclampf(px * ux + py * uy, 0.0f, len);
        float qx = px - ux * s, qy = py - uy * s;
        in = qx * qx + qy * qy < r2;
      } else {
        float along = px * ux + py * uy;
        float perp = fabsf(px * uy - py * ux);
        in = perp < r && along >= -ext && along < len + ext;
      }
      if (in) pixel(x, y, grey);
    }
  }
}

void Gfx::disc(float cx, float cy, float r, uint8_t grey) {
  int x0 = imax(0, (int)floorf(cx - r - 1)), x1 = imin(PANEL_WIDTH - 1, (int)ceilf(cx + r + 1));
  int y0 = imax(0, (int)floorf(cy - r - 1)), y1 = imin(PANEL_HEIGHT - 1, (int)ceilf(cy + r + 1));
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      float px = x + 0.5f - cx, py = y + 0.5f - cy;
      if (px * px + py * py < r * r) pixel(x, y, grey);
    }
}

// Even thicknesses are centred on a pixel corner, odd ones on a pixel centre.
static inline float strokeOffset(int t) { return (t & 1) ? 0.5f : 1.0f; }

void Gfx::line(int x0, int y0, int x1, int y1, int t, uint8_t grey, Cap cap) {
  if (t <= 1) { thinLine(x0, y0, x1, y1, grey); return; }
  Pt p[2] = {{(float)x0, (float)y0}, {(float)x1, (float)y1}};
  polyline(p, 2, t, grey, cap);
}

void Gfx::polyline(const Pt* pts, int n, int t, uint8_t grey, Cap cap) {
  if (!pts || n <= 0) return;
  if (t <= 1) {
    if (n == 1) thinLine((int)lroundf(pts[0].x), (int)lroundf(pts[0].y), (int)lroundf(pts[0].x), (int)lroundf(pts[0].y), grey);
    for (int i = 0; i + 1 < n; i++)
      thinLine((int)lroundf(pts[i].x), (int)lroundf(pts[i].y), (int)lroundf(pts[i + 1].x), (int)lroundf(pts[i + 1].y), grey);
    return;
  }
  const float o = strokeOffset(t);
  const float r = t * 0.5f;
  if (n == 1) { disc(pts[0].x + o, pts[0].y + o, r, grey); return; }
  for (int i = 0; i + 1 < n; i++) {
    float ax = pts[i].x + o, ay = pts[i].y + o, bx = pts[i + 1].x + o, by = pts[i + 1].y + o;
    if (cap == CAP_ROUND) {
      capsule(ax, ay, bx, by, (float)t, grey, CAP_ROUND);
      continue;
    }
    // Butt-style segments (half-open), end pieces extended half a pixel (BUTT: so the end pixels
    // are included) or by r (SQUARE); interior vertices get a round join.
    float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    float ux = 1, uy = 0;  // degenerate segment: dot along x
    if (len > 1e-6f) { ux = dx / len; uy = dy / len; }
    float e = (cap == CAP_SQUARE) ? r : 0.5f;
    if (i == 0) { ax -= ux * e; ay -= uy * e; }
    if (i == n - 2) { bx += ux * e; by += uy * e; }
    capsule(ax, ay, bx, by, (float)t, grey, CAP_BUTT);
    if (i + 2 < n) disc(pts[i + 1].x + o, pts[i + 1].y + o, r, grey);
  }
}

void Gfx::dashedPolyline(const Pt* pts, int n, int t, uint8_t grey, float dash, float gap, float phase) {
  if (!pts || n < 2 || dash <= 0) return;
  if (gap <= 0) { polyline(pts, n, t, grey, CAP_BUTT); return; }
  if (t < 1) t = 1;
  const float o = strokeOffset(t);
  const float period = dash + gap;
  phase = fmodf(phase, period);
  if (phase < 0) phase += period;
  bool inDash = phase < dash;
  float remain = inDash ? dash - phase : period - phase;
  for (int i = 0; i + 1 < n; i++) {
    float ax = pts[i].x + o, ay = pts[i].y + o;
    float dx = pts[i + 1].x + o - ax, dy = pts[i + 1].y + o - ay;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) continue;
    float ux = dx / len, uy = dy / len;
    float pos = 0;
    while (pos < len - 1e-6f) {
      float step = fminf(remain, len - pos);
      if (inDash) {
        if (t == 1) {
          // thin dashes: sample pixel centres along the path
          for (float s = pos; s < pos + step; s += 0.5f)
            pixel((int)floorf(ax + ux * s), (int)floorf(ay + uy * s), grey);
        } else {
          capsule(ax + ux * pos, ay + uy * pos, ax + ux * (pos + step), ay + uy * (pos + step), (float)t, grey, CAP_BUTT);
        }
      }
      pos += step;
      remain -= step;
      if (remain <= 1e-6f) { inDash = !inDash; remain = inDash ? dash : gap; }
    }
    if (inDash && i + 2 < n && t >= 3) disc(ax + dx, ay + dy, t * 0.5f, grey);
  }
}

void Gfx::dashedLine(int x0, int y0, int x1, int y1, int t, uint8_t grey, float dash, float gap) {
  Pt p[2] = {{(float)x0, (float)y0}, {(float)x1, (float)y1}};
  dashedPolyline(p, 2, t, grey, dash, gap);
}

// ---------------------------------------------------------------- rectangles

void Gfx::fillRect(int x, int y, int w, int h, uint8_t grey) {
  if (w <= 0 || h <= 0) return;
  int y0 = imax(y, 0), y1 = imin(y + h, PANEL_HEIGHT);
  for (int yy = y0; yy < y1; yy++) span(yy, x, x + w, grey);
}

void Gfx::rect(int x, int y, int w, int h, uint8_t grey, int t) {
  if (w <= 0 || h <= 0 || t <= 0) return;
  if (2 * t >= w || 2 * t >= h) { fillRect(x, y, w, h, grey); return; }
  fillRect(x, y, w, t, grey);
  fillRect(x, y + h - t, w, t, grey);
  fillRect(x, y + t, t, h - 2 * t, grey);
  fillRect(x + w - t, y + t, t, h - 2 * t, grey);
}

static bool rrSpan(int row, int x, int y, int w, int h, int r, int& x0, int& x1) {
  if (w <= 0 || h <= 0) return false;
  float yc = row + 0.5f;
  if (yc < y || yc >= y + h) return false;
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  float dy = 0;
  if (r > 0) {
    if (yc < y + r) dy = (y + r) - yc;
    else if (yc > y + h - r) dy = yc - (y + h - r);
  }
  float inset = 0;
  if (dy > 0) inset = r - sqrtf(fmaxf(0.0f, (float)r * r - dy * dy));
  x0 = (int)ceilf(x + inset - 0.5f);
  x1 = (int)floorf(x + w - inset + 0.5f);
  return x1 > x0;
}

void Gfx::fillRoundRect(int x, int y, int w, int h, int r, uint8_t grey) {
  int y0 = imax(y, 0), y1 = imin(y + h, PANEL_HEIGHT);
  for (int row = y0; row < y1; row++) {
    int a, b;
    if (rrSpan(row, x, y, w, h, r, a, b)) span(row, a, b, grey);
  }
}

void Gfx::roundRect(int x, int y, int w, int h, int r, uint8_t grey, int t) {
  if (t <= 0) return;
  int y0 = imax(y, 0), y1 = imin(y + h, PANEL_HEIGHT);
  int ir = imax(0, imin(r, imin(w, h) / 2) - t);
  for (int row = y0; row < y1; row++) {
    int a, b, c, d;
    if (!rrSpan(row, x, y, w, h, r, a, b)) continue;
    if (rrSpan(row, x + t, y + t, w - 2 * t, h - 2 * t, ir, c, d)) {
      span(row, a, c, grey);
      span(row, d, b, grey);
    } else span(row, a, b, grey);
  }
}

// ---------------------------------------------------------------- circles

void Gfx::fillCircle(int cx, int cy, int r, uint8_t grey) {
  if (r < 0) return;
  for (int dy = -r; dy <= r; dy++) {
    int s = r * r + r - dy * dy;
    if (s < 0) continue;
    int dx = (int)sqrtf((float)s);
    while ((dx + 1) * (dx + 1) <= s) dx++;
    while (dx * dx > s) dx--;
    span(cy + dy, cx - dx, cx + dx + 1, grey);
  }
}

void Gfx::circle(int cx, int cy, int r, uint8_t grey, int t) {
  if (r < 0 || t <= 0) return;
  float ro = r + t * 0.5f, ri = r - t * 0.5f;
  if (ri < 0) ri = 0;
  int R = (int)ceilf(ro);
  for (int dy = -R; dy <= R; dy++) {
    for (int dx = -R; dx <= R; dx++) {
      float d2 = (float)(dx * dx + dy * dy);
      if (d2 < ro * ro && (ri <= 0 || d2 >= ri * ri)) pixel(cx + dx, cy + dy, grey);
    }
  }
}

// ---------------------------------------------------------------- polygons

void Gfx::fillPolygon(const Pt* pts, int n, uint8_t grey) {
  if (!pts || n < 3) return;
  float ymin = pts[0].y, ymax = pts[0].y;
  for (int i = 1; i < n; i++) { ymin = fminf(ymin, pts[i].y); ymax = fmaxf(ymax, pts[i].y); }
  int y0 = imax(0, (int)floorf(ymin)), y1 = imin(PANEL_HEIGHT - 1, (int)ceilf(ymax));
  enum { MAXX = 64 };
  float xs[MAXX];
  for (int y = y0; y <= y1; y++) {
    float yc = y + 0.5f;
    int k = 0;
    for (int i = 0; i < n; i++) {
      const Pt& a = pts[i];
      const Pt& b = pts[(i + 1) % n];
      if ((a.y <= yc) == (b.y <= yc)) continue;
      if (k < MAXX) xs[k++] = a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y);
    }
    for (int i = 1; i < k; i++) {  // insertion sort
      float v = xs[i]; int j = i - 1;
      while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; }
      xs[j + 1] = v;
    }
    for (int i = 0; i + 1 < k; i += 2)
      span(y, (int)ceilf(xs[i] - 0.5f), (int)ceilf(xs[i + 1] - 0.5f), grey);
  }
}

void Gfx::fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint8_t grey) {
  Pt p[3] = {{x0, y0}, {x1, y1}, {x2, y2}};
  fillPolygon(p, 3, grey);
}

// ---------------------------------------------------------------- text

uint32_t utf8Next(const char*& p) {
  const uint8_t* s = (const uint8_t*)p;
  if (!*s) return 0;
  uint32_t c = *s;
  int extra;
  uint32_t minv;
  if (c < 0x80) { p++; return c; }
  else if ((c & 0xE0) == 0xC0) { c &= 0x1F; extra = 1; minv = 0x80; }
  else if ((c & 0xF0) == 0xE0) { c &= 0x0F; extra = 2; minv = 0x800; }
  else if ((c & 0xF8) == 0xF0) { c &= 0x07; extra = 3; minv = 0x10000; }
  else { p++; return 0xFFFD; }
  s++;
  for (int i = 0; i < extra; i++) {
    if ((s[i] & 0xC0) != 0x80) { p = (const char*)(s + i); return 0xFFFD; }  // truncated
    c = (c << 6) | (s[i] & 0x3F);
  }
  p = (const char*)(s + extra);
  if (c < minv || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0xFFFD;
  return c;
}

const EGlyph* findGlyph(const EFont& font, uint32_t cp) {
  for (uint32_t i = 0; i < font.interval_count; i++) {
    const EInterval& iv = font.intervals[i];
    if (cp >= iv.first && cp <= iv.last) return &font.glyph[iv.offset + (cp - iv.first)];
    if (cp < iv.first) break;
  }
  return nullptr;
}

// Glyph to use for a code point, falling back to question mark (63) then space (32); may be nullptr.
static const EGlyph* glyphFor(const EFont& font, uint32_t cp) {
  const EGlyph* g = findGlyph(font, cp);
  if (!g) g = findGlyph(font, 63);
  if (!g) g = findGlyph(font, 32);
  return g;
}

TextSize measureText(const EFont& font, const char* s, int ls) {
  TextSize m = {0, font.ascender, -font.descender, 0, 0};
  if (!s) return m;
  int n = 0;
  while (*s) {
    uint32_t cp = utf8Next(s);
    const EGlyph* g = glyphFor(font, cp);
    m.width += g ? g->advance_x : font.advance_y / 4;
    if (g && g->height > 0) {
      m.inkAscent = imax(m.inkAscent, g->top);
      m.inkDescent = imax(m.inkDescent, (int)g->height - g->top);
    }
    n++;
  }
  if (n > 1) m.width += ls * (n - 1);
  return m;
}

int Gfx::drawText(int x, int y, const char* s, const EFont& font, uint8_t grey, Align align, int ls) {
  if (!s || !fb) return 0;
  TextSize m = measureText(font, s, ls);
  if (align == ALIGN_CENTER) x -= m.width / 2;
  else if (align == ALIGN_RIGHT) x -= m.width;
  grey &= 0x0F;
  int pen = x;
  while (*s) {
    uint32_t cp = utf8Next(s);
    const EGlyph* g = glyphFor(font, cp);
    if (!g) { pen += font.advance_y / 4 + ls; continue; }
    int stride = (g->width + 1) / 2;
    const uint8_t* bm = font.bitmap + g->data_offset;
    int gx = pen + g->left, gy = y - g->top;
    for (int r = 0; r < g->height; r++) {
      int py = gy + r;
      if (py < 0 || py >= PANEL_HEIGHT) continue;
      for (int c = 0; c < g->width; c++) {
        int px = gx + c;
        if (px < 0 || px >= PANEL_WIDTH) continue;
        uint8_t b = bm[r * stride + (c >> 1)];
        int a = (c & 1) ? (b >> 4) : (b & 0x0F);
        if (a == 0) continue;
        if (a == 15) { pixel(px, py, grey); continue; }
        int bg = get(px, py);
        int d = ((int)grey - bg) * a;
        int out = bg + (d >= 0 ? (d + 7) / 15 : -((-d + 7) / 15));
        pixel(px, py, (uint8_t)out);
      }
    }
    pen += g->advance_x + ls;
  }
  return m.width;
}
