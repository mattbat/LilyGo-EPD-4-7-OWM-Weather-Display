// Native preview: renders the graphics test card (and later every screen) to out/*.png.
// Excluded when building unit tests (they bring their own main).
#ifndef PIO_UNIT_TESTING
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "gfx.h"
#include "fonts/fonts.h"
#include "gallery.h"
#include "scenes.h"

static uint8_t g_fb[PANEL_WIDTH * PANEL_HEIGHT / 2];

static bool writePng(const Gfx& g, const char* path) {
  static uint8_t img[PANEL_WIDTH * PANEL_HEIGHT];
  for (int y = 0; y < PANEL_HEIGHT; y++)
    for (int x = 0; x < PANEL_WIDTH; x++) img[y * PANEL_WIDTH + x] = (uint8_t)(g.get(x, y) * 17);
  return stbi_write_png(path, PANEL_WIDTH, PANEL_HEIGHT, 1, img, PANEL_WIDTH) != 0;
}

struct NamedFont { const EFont* f; const char* name; };

static void drawTestCard(Gfx& g) {
  g.clear(15);
  char buf[96];

  // ---- column A: regular sizes
  static const NamedFont reg[] = {{&AtkR13, "R13"}, {&AtkR14, "R14"}, {&AtkR15, "R15"}, {&AtkR16, "R16"},
                                  {&AtkR17, "R17"}, {&AtkR18, "R18"}, {&AtkR20, "R20"}, {&AtkR22, "R22"}, {&AtkR44, "R44"}};
  int y = 6;
  for (const NamedFont& nf : reg) {
    const EFont& f = *nf.f;
    y += f.ascender;
    snprintf(buf, sizeof buf, "%s Wind 12 mph \xE2\x86\x91\xE2\x86\x93 72\xC2\xB0 \xC2\xB7 Hg", nf.name);
    if (f.ascender > 30) snprintf(buf, sizeof buf, "%s 72\xC2\xB0\xE2\x86\x91", nf.name);
    g.drawText(8, y, buf, f, 0);
    y += -f.descender + 2;
  }
  // ---- column B: bold sizes
  static const NamedFont bold[] = {{&AtkB13, "B13"}, {&AtkB14, "B14"}, {&AtkB15, "B15"}, {&AtkB17, "B17"}, {&AtkB18, "B18"},
                                   {&AtkB19, "B19"}, {&AtkB20, "B20"}, {&AtkB22, "B22"}, {&AtkB32, "B32"}, {&AtkB42, "B42"},
                                   {&AtkB50, "B50"}};
  y = 6;
  for (const NamedFont& nf : bold) {
    const EFont& f = *nf.f;
    y += f.ascender;
    snprintf(buf, sizeof buf, "%s Wind 12 mph \xE2\x86\x91\xE2\x86\x93 72\xC2\xB0 \xC2\xB7 Hg", nf.name);
    if (f.ascender > 30) snprintf(buf, sizeof buf, "%s -12\xC2\xB0\xE2\x86\x93", nf.name);
    g.drawText(330, y, buf, f, 0);
    y += -f.descender + 2;
  }
  // ---- big digits
  g.drawText(8, 535, "72\xC2\xB0", AtkB136, 0);
  g.drawText(330, 535, "-8\xC2\xB0/", AtkB84, 0);
  g.drawText(330, 415, "-12\xC2\xB0", AtkB76, 0);

  // ---- primitives, right column
  const int X = 660;
  for (int t = 1; t <= 8; t++) {
    g.line(X, 20 + t * 14, X + 100, 20 + t * 14, t, 0);
    g.line(X + 120, 20 + t * 14, X + 220, 20 + t * 14, t, 0, CAP_SQUARE);
    g.line(X + 240, 20 + t * 14, X + 290, 12 + t * 14, t, 0, CAP_BUTT);
  }
  // diagonal / vertical thick lines
  for (int t = 1; t <= 8; t++) {
    g.line(X + 10 * t, 150, X + 10 * t + 20, 190, t, 0);
    g.line(X + 100 + 12 * t, 150, X + 100 + 12 * t, 190, t, 0);
  }
  // dashed lines "10 7"
  g.dashedLine(X, 205, X + 290, 205, 2, 0, 10, 7);
  g.dashedLine(X, 214, X + 290, 214, 3, 0, 10, 7);
  g.dashedLine(X, 224, X + 290, 224, 1, 0, 3, 3);
  Pt zig[] = {{X, 250}, {X + 60, 232}, {X + 120, 262}, {X + 180, 236}, {X + 230, 258}, {X + 290, 240}};
  g.dashedPolyline(zig, 6, 3, 0, 10, 7);
  Pt chart[] = {{X, 300}, {X + 40, 280}, {X + 80, 292}, {X + 120, 270}, {X + 170, 285}, {X + 220, 268}, {X + 290, 275}};
  g.polyline(chart, 7, 4, 0);
  // greyscale ramp
  for (int i = 0; i < 16; i++) g.fillRect(X + i * 18, 308, 18, 14, (uint8_t)i);

  // ---- lower right: shapes
  g.rect(X, 335, 60, 40, 0, 1);
  g.rect(X + 70, 335, 60, 40, 0, 3);
  g.roundRect(X + 140, 335, 70, 40, 12, 0, 2);
  g.fillRoundRect(X + 220, 335, 70, 40, 14, 0);
  g.circle(X + 25, 410, 20, 0, 1);
  g.circle(X + 80, 410, 20, 0, 4);
  g.fillCircle(X + 135, 410, 20, 0);
  g.fillCircle(X + 180, 410, 6, 6);
  g.fillTriangle(X + 205, 430, X + 235, 390, X + 265, 430, 0);
  // concave: cloud-like blob and lightning bolt
  Pt bolt[] = {{X + 10, 450}, {X + 50, 450}, {X + 35, 478}, {X + 62, 478}, {X + 18, 530}, {X + 28, 492}, {X + 2, 492}};
  g.fillPolygon(bolt, 7, 0);
  Pt star[] = {{X + 120, 450}, {X + 130, 478}, {X + 160, 478}, {X + 136, 494}, {X + 146, 524},
               {X + 120, 506}, {X + 94, 524}, {X + 104, 494}, {X + 80, 478}, {X + 110, 478}};
  g.fillPolygon(star, 10, 0);
  Pt arrowOut[] = {{X + 200, 470}, {X + 280, 470}, {X + 280, 520}, {X + 200, 520}};
  g.fillPolygon(arrowOut, 4, 10);
  g.rect(X + 200, 470, 80, 50, 0, 2);

  // ---- white on black + spacing + alignment
  g.fillRoundRect(560, 340, 90, 190, 10, 0);  // narrow box at the edge of the right column gap
  g.drawText(605, 372, "SMART", AtkB19, 15, ALIGN_CENTER, 2);
  g.drawText(605, 410, "42", AtkB42, 15, ALIGN_CENTER);
  g.drawText(605, 440, "white", AtkR17, 15, ALIGN_CENTER);
  g.drawText(605, 470, "on black", AtkR14, 15, ALIGN_CENTER);
  g.drawText(605, 500, "grey 8", AtkB15, 8, ALIGN_CENTER);
  // alignment guide
  g.dashedLine(170, 270, 170, 420, 1, 0, 4, 3);
  g.drawText(170, 295, "left", AtkR15, 0, ALIGN_LEFT);
  g.drawText(170, 320, "centre", AtkR15, 0, ALIGN_CENTER);
  g.drawText(170, 345, "right", AtkR15, 0, ALIGN_RIGHT);
  g.drawText(170, 375, "SPACED", AtkB14, 0, ALIGN_LEFT, 3);
  g.drawText(170, 400, "SPACED", AtkB14, 0, ALIGN_RIGHT, 3);
}

int main() {
  Gfx g;
  g.begin(g_fb);
  MKDIR("out");
  drawTestCard(g);
  if (!writePng(g, "out/test_card.png")) { fprintf(stderr, "failed to write out/test_card.png\n"); return 1; }
  printf("wrote out/test_card.png\n");

  static uint8_t fb2[PANEL_WIDTH * PANEL_HEIGHT / 2], fb3[PANEL_WIDTH * PANEL_HEIGHT / 2], fb4[PANEL_WIDTH * PANEL_HEIGHT / 2];
  Gfx pages[4];
  pages[0].begin(g_fb); pages[1].begin(fb2); pages[2].begin(fb3); pages[3].begin(fb4);
  galleryPageIcons1(pages[0]);
  galleryPageIcons2(pages[1]);
  galleryPageIcons3(pages[2]);
  galleryPageCharts(pages[3]);
  if (!galleryWrite(pages, 4, "out/gallery.png")) { fprintf(stderr, "failed to write out/gallery.png\n"); return 1; }
  printf("wrote out/gallery.png\n");

  // every screen, rendered from the fixtures at a fixed "now" (deterministic output)
  Gfx scr;
  scr.begin(g_fb);
  for (int i = 0; i < kSceneCount; i++) {
    char path[96];
    snprintf(path, sizeof path, "out/%s.png", kScenes[i].name);
    if (!renderScene(kScenes[i].name, scr)) { fprintf(stderr, "failed to render %s\n", kScenes[i].name); return 1; }
    if (!writePng(scr, path)) { fprintf(stderr, "failed to write %s\n", path); return 1; }
    printf("wrote %s\n", path);
  }
  return 0;
}
#endif
