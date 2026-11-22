// ESP32 only: battery voltage (the percent curve lives in helpers.h so it can be tested on the PC).
#pragma once
#include "helpers.h"
// Powers the display rail, averages ADC samples on GPIO 36, powers the rail down. Call after displayInit().
float readBatteryVolts();
