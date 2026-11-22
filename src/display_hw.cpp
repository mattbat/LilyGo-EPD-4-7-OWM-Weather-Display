#ifdef ARDUINO
#include "display_hw.h"
#include <Arduino.h>
#include "epd_driver.h"
void displayInit() { epd_init(); }
void displayShow(const uint8_t* fb) {
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), (uint8_t*)fb);
  epd_poweroff_all();
}
#endif
