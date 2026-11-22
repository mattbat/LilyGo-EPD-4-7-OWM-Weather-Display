#ifdef ARDUINO
#include "battery.h"
#include <Arduino.h>
#include "epd_driver.h"
#include "esp_adc_cal.h"

// The divider is powered from the display rail, so the rail must be on while sampling.
float readBatteryVolts() {
  epd_poweron();
  delay(50);   // let the rail settle
  esp_adc_cal_characteristics_t adc;
  esp_adc_cal_value_t src = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc);
  uint32_t vref = (src == ESP_ADC_CAL_VAL_EFUSE_VREF) ? adc.vref : 1100;
  analogRead(36);   // discard the first sample
  uint32_t sum = 0;
  const int N = 16;
  for (int i = 0; i < N; i++) { sum += analogRead(36); delay(2); }
  epd_poweroff_all();
  float raw = (float)sum / N;
  return raw / 4096.0f * 6.566f * (vref / 1000.0f);   // same scaling as the old OWM sketch
}
#endif
