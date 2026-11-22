// ESP32 only: e-paper driver wrapper.
#pragma once
#include <stdint.h>
// Initialises the panel driver and allocates the 960x540 4-bit framebuffer in PSRAM (nullptr on failure).
uint8_t* displayInit();
// Power on, clear, push the framebuffer, power off (the panel keeps the image unpowered).
void displayShow(const uint8_t* framebuffer);
