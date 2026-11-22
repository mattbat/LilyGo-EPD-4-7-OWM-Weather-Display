#ifdef ARDUINO
#include "battery.h"
#include <Arduino.h>
#include <math.h>
#include "epd_driver.h"
#include "esp_adc_cal.h"

// Kept from the old OWM sketch DrawBattery():
//   esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
//   (if eFuse Vref is available, vref = adc_chars.vref; default 1100)
//   float voltage = analogRead(36) / 4096.0 * 6.566 * (vref / 1000.0);
//   percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3)
//              + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
//   if (voltage >= 4.20) percentage = 100;
//   if (voltage <= 3.20) percentage = 0;
// The divider is powered from the display rail: epd_poweron() before analogRead(36).

float readBatteryVolts() {
  // TODO(task 6): epd_poweron(); analogRead(36) with esp_adc_cal; epd_poweroff_all();
  return 0.0f;
}
int batteryPercent(float voltage) {
  if (voltage >= 4.20f) return 100;
  if (voltage <= 3.20f) return 0;
  double p = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) +
             255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)p;
}
#endif
