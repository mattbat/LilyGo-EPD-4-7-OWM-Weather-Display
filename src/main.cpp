// ESP32 entry point (task 6 fills this in). Only ESP32 files may include secrets.h.
#ifdef ARDUINO
#include <Arduino.h>
#include "config.h"
#include "secrets.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Tempest display scaffold");
  esp_sleep_enable_timer_wakeup((uint64_t)UPDATE_INTERVAL_MIN * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}
void loop() {}
#endif
