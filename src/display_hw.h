// ESP32 only: e-paper driver wrapper (task 6).
#pragma once
#include <stdint.h>
void displayInit();
void displayShow(const uint8_t* framebuffer);
