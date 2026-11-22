#include "gfx.h"
#include <string.h>

void Gfx::begin(uint8_t* buffer) { fb = buffer; }
void Gfx::clear(uint8_t grey) {
  if (fb) memset(fb, (grey << 4) | grey, PANEL_WIDTH * PANEL_HEIGHT / 2);
}
void Gfx::pixel(int x, int y, uint8_t grey) {
  if (!fb || x < 0 || y < 0 || x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return;
  uint8_t* p = &fb[(y * PANEL_WIDTH + x) / 2];
  if (x & 1) *p = (uint8_t)((*p & 0x0F) | (grey << 4));
  else       *p = (uint8_t)((*p & 0xF0) | (grey & 0x0F));
}
uint8_t Gfx::get(int x, int y) const {
  if (!fb || x < 0 || y < 0 || x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return 15;
  uint8_t v = fb[(y * PANEL_WIDTH + x) / 2];
  return (x & 1) ? (v >> 4) : (v & 0x0F);
}
