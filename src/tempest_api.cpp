#ifdef ARDUINO
#include "tempest_api.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "esp_sntp.h"
#include "config.h"
#include "secrets.h"

static const uint32_t HTTP_TIMEOUT_MS = 15000;

bool systemTimeValid() { return time(nullptr) > 1735689600; }   // 2025-01-01

bool netConnect(uint32_t timeoutMs) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > timeoutMs) {
      // Diagnose: status 1 = network not found, 4 = connect failed (often wrong password), 6 = disconnected.
      Serial.printf("wifi status %d after %u ms\n", (int)WiFi.status(), (unsigned)(millis() - t0));
      int n = WiFi.scanNetworks();
      bool seen = false;
      for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == WIFI_SSID) {
          seen = true;
          Serial.printf("wifi target seen: rssi %d dBm, channel %d, auth %d\n", WiFi.RSSI(i), WiFi.channel(i), (int)WiFi.encryptionType(i));
        }
      }
      if (!seen) Serial.printf("wifi target NOT seen among %d networks (2.4 GHz only)\n", n);
      return false;
    }
    delay(100);
  }
  Serial.printf("wifi connected in %u ms, rssi %d dBm\n", (unsigned)(millis() - t0), WiFi.RSSI());
  return true;
}

bool netSyncTime(uint32_t timeoutMs) {
  configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
  uint32_t t0 = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    if (millis() - t0 > timeoutMs) return false;
    delay(50);
  }
  return true;
}

void netOff() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

// Waits for data like a blocking stream. WiFiClient::read()/peek() return -1 when nothing has
// arrived yet, which the parsers would take for end of input, so wait while the socket is open.
class NetReader {
 public:
  NetReader(WiFiClient& c, uint32_t timeoutMs) : c_(c), to_(timeoutMs), pos_(0), len_(0) {}
  int read() { return fill() ? (uint8_t)buf_[pos_++] : -1; }
  int peek() { return fill() ? (uint8_t)buf_[pos_] : -1; }
  size_t readBytes(char* b, size_t k) {
    size_t r = 0;
    while (r < k && fill()) {
      size_t n = len_ - pos_;
      if (n > k - r) n = k - r;
      memcpy(b + r, buf_ + pos_, n);
      pos_ += n; r += n;
    }
    return r;
  }
 private:
  bool fill() {
    if (pos_ < len_) return true;
    uint32_t t0 = millis();
    for (;;) {
      int a = c_.available();
      if (a > 0) {
        int n = c_.read((uint8_t*)buf_, a < (int)sizeof buf_ ? a : (int)sizeof buf_);
        if (n > 0) { pos_ = 0; len_ = n; return true; }
      } else if (!c_.connected()) {
        return false;                       // closed and drained: end of body
      }
      if (millis() - t0 > to_) return false;
      delay(1);
    }
  }
  WiFiClient& c_;
  uint32_t to_;
  size_t pos_, len_;
  char buf_[512];
};

static FetchResult makeResult(FetchStatus s, const char* what) {
  FetchResult r; r.status = s; r.httpCode = 0; r.parse = PARSE_OK;
  snprintf(r.what, sizeof r.what, "%s", what);
  return r;
}

// One GET, body handed to consume(NetReader&) -> ParseStatus.
template <class F> static FetchResult getOnce(const char* url, const char* what, F consume) {
  char safe[200];
  redactUrl(url, safe, sizeof safe);
  Serial.printf("GET %s\n", safe);
  const uint32_t tStart = millis();
  FetchResult r = makeResult(FETCH_OK, what);
  WiFiClientSecure client;
  client.setInsecure();   // no certificate check: the data is public weather and the token is revocable;
                          // pin swd.weatherflow.com's root CA later if that matters
  client.setTimeout(HTTP_TIMEOUT_MS / 1000);
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.useHTTP10(true);   // no chunked transfer encoding: the body is plain JSON until close
  if (!http.begin(client, url)) { r.status = FETCH_ERR_HTTP; r.httpCode = -1; return r; }
  int code = http.GET();
  r.httpCode = code;
  if (code != 200) { r.status = FETCH_ERR_HTTP; http.end(); return r; }
  NetReader in(*http.getStreamPtr(), HTTP_TIMEOUT_MS);
  ParseStatus ps = consume(in);
  if (ps != PARSE_OK) { r.status = FETCH_ERR_PARSE; r.parse = ps; }
  http.end();
  Serial.printf("  %s: http %d, parse %d, %lu ms\n", what, code, (int)ps, (unsigned long)(millis() - tStart));
  return r;
}

template <class F> static FetchResult getRetry(const char* url, const char* what, F consume) {
  FetchResult r = getOnce(url, what, consume);
  if (r.status != FETCH_OK && fetchShouldRetry(r)) {
    Serial.printf("retry %s (status %d, http %d)\n", what, (int)r.status, r.httpCode);
    delay(500);
    r = getOnce(url, what, consume);
  }
  return r;
}

static uint32_t resolveDeviceId(FetchResult& err) {
  if (TEMPEST_DEVICE_ID != 0) return TEMPEST_DEVICE_ID;
  Preferences prefs;
  uint32_t id = 0;
  if (prefs.begin("tempest", true)) { id = prefs.getUInt("device", 0); prefs.end(); }
  if (id) return id;
  char url[200];
  tempestUrlStations(url, sizeof url, TEMPEST_TOKEN);
  uint32_t found = 0;
  err = getRetry(url, "stations", [&](NetReader& in) { return parseStations(in, TEMPEST_STATION_ID, found); });
  if (err.status != FETCH_OK) return 0;
  if (prefs.begin("tempest", false)) { prefs.putUInt("device", found); prefs.end(); }
  return found;
}

FetchResult fetchAll(WeatherData& out, FetchContext& ctx) {
  if (TEMPEST_STATION_ID == 0) return makeResult(FETCH_ERR_CONFIG, "station");
  char url[320];
  FetchResult r;
  weatherDataInit(out);

  tempestUrlForecast(url, sizeof url, TEMPEST_TOKEN, TEMPEST_STATION_ID);
  r = getRetry(url, "forecast", [&](NetReader& in) { return parseBetterForecast(in, out); });
  if (r.status != FETCH_OK) return r;

  tempestUrlStationObs(url, sizeof url, TEMPEST_TOKEN, TEMPEST_STATION_ID);
  r = getRetry(url, "obs", [&](NetReader& in) { return parseStationObs(in, out); });
  if (r.status != FETCH_OK) return r;

  uint32_t dev = resolveDeviceId(r);
  if (!dev) return r.status != FETCH_OK ? r : makeResult(FETCH_ERR_CONFIG, "device");

  HistoryPlan hp = planHistory(ctx.now, ctx.titles, out.pressureOffsetMb);
  tempestUrlDeviceHistory(url, sizeof url, TEMPEST_TOKEN, dev, hp.timeStart, hp.timeEnd);
  r = getRetry(url, "history", [&](NetReader& in) { return parseDeviceHistory(in, hp.window, out); });
  if (r.status != FETCH_OK) return r;

  out.fetchedEpoch = (uint32_t)ctx.now;
  return makeResult(FETCH_OK, "all");
}
#endif
