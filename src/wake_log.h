// Persistent wake log (ESP32 only). Each wake writes one record to NVS flash, updated as it
// progresses, so a wake that dies mid-way (e.g. a brownout during the panel refresh) is still
// visible afterwards. The whole log is printed to serial at every boot: plug the board into USB,
// reset it, and read what happened while it ran on battery.
#pragma once
#include <stdint.h>

enum WakeStage : uint8_t {
  STAGE_BOOT = 1,      // started, battery read
  STAGE_NET = 2,       // WiFi / fetch in progress
  STAGE_DISPLAY = 3,   // panel refresh in progress
  STAGE_DONE = 4       // reached deep sleep normally
};

struct WakeRecord {
  uint32_t epoch;      // UTC time when known (0 = clock not set)
  uint16_t battMv;     // battery reading at boot
  uint16_t awakeDs;    // awake time in tenths of a second (set at STAGE_DONE)
  uint8_t resetReason; // esp_reset_reason()
  uint8_t wakeCause;   // esp_sleep_get_wakeup_cause()
  uint8_t stage;       // last WakeStage reached
  uint8_t pad;
};

void wakeLogBegin(uint16_t battMv);          // prints the stored log, then starts a new record
void wakeLogStage(WakeStage s);              // update the current record's stage (and epoch if known)
void wakeLogDone(uint32_t awakeMs);          // mark the current record done
bool wakeLogLastWasBrownout();               // this boot was a brownout reset
