// Copy this file to secrets.h (same folder) and fill in your values.
// secrets.h is gitignored so your passwords and token never get committed.
#pragma once

// ---- WiFi ----
#define WIFI_SSID          "your-wifi-name"
#define WIFI_PASSWORD      "your-wifi-password"

// ---- Tempest ----
// Personal access token: tempestwx.com -> Settings -> Data Authorizations -> Create Token
#define TEMPEST_TOKEN      "00000000-0000-0000-0000-000000000000"
// Station ID: the number in the URL of your station page on tempestwx.com
#define TEMPEST_STATION_ID 0
// Device ID of the outdoor sensor. Leave 0 and it will be looked up from the station.
#define TEMPEST_DEVICE_ID  0
