#ifdef ARDUINO
#include "display_hw.h"
#include <Arduino.h>
#include "config.h"
#include "epd_driver.h"

uint8_t* displayInit() {
  epd_init();
  uint8_t* fb = (uint8_t*)ps_calloc(PANEL_WIDTH * PANEL_HEIGHT / 2, 1);
  if (fb) memset(fb, 0xFF, PANEL_WIDTH * PANEL_HEIGHT / 2);   // white
  return fb;
}

void displayShow(const uint8_t* fb) {
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), (uint8_t*)fb);
  epd_poweroff_all();
}
#endif
