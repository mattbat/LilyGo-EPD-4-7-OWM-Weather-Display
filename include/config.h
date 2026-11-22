// Non-secret settings. WiFi and Tempest credentials live in secrets.h (ESP32-only files).
#pragma once

// ---- Display ----
#define LOCATION_LABEL        "HIGHGARDEN - STARK ST. TEMPEST"

// ---- Time ----
#define TZ_POSIX              "CST6CDT,M3.2.0,M11.1.0"   // US Central
#define NTP_SERVER_1          "pool.ntp.org"
#define NTP_SERVER_2          "time.nist.gov"

// ---- Schedule ----
#define UPDATE_INTERVAL_MIN   30     // aligned to :00 and :30
#define QUIET_START_HOUR      22     // last update at 22:00 (overnight screen)
#define QUIET_START_MIN       0
#define QUIET_END_HOUR        5      // next update at 05:00
#define QUIET_END_MIN         0
#define LAST_LIVE_HOUR        21     // last live update at 21:30
#define LAST_LIVE_MIN         30
#define WIFI_TIMEOUT_MS       15000
#define CHARGE_SLEEP_HOURS    2

// ---- Battery (18650, volts) ----
#define BATT_LOW_V            3.3f   // below: show Charge me screen
#define BATT_RECOVER_V        3.7f   // hysteresis: resume at or above
#define STATION_BATT_OK_V     2.45f  // Tempest station battery OK threshold

// ---- Units (display) ----
#define UNITS_TEMP_F          1      // degrees F
#define UNITS_WIND_MPH        1
#define UNITS_RAIN_IN         1
#define UNITS_PRESSURE_INHG   1

// ---- Panel ----
#define PANEL_WIDTH           960
#define PANEL_HEIGHT          540
