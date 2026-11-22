// Portable JSON -> WeatherData. Used unchanged by the ESP32 firmware and the native tools/tests.
//
// Every entry point exists in two forms:
//   parseX(const char* json, size_t len, ...)  parse from a memory buffer
//   parseX(In& in, ...)                         parse from any reader: an Arduino Stream (WiFiClient,
//                                               HTTPClient::getStream()) or, on native, IStreamReader
//                                               (wraps std::istream / file). `In` needs read(), peek()
//                                               and readBytes(char*, size_t), like Arduino Stream.
// Responses are streamed through an ArduinoJson filter so only the needed fields are kept in RAM.
// Device history is never held in memory: rows are read one at a time and folded into buckets.
#pragma once
#include "weather_data.h"
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#ifndef ARDUINO
#include <istream>
#endif

enum ParseStatus {
  PARSE_OK = 0,
  PARSE_ERR_JSON = 1,       // invalid, truncated or empty JSON
  PARSE_ERR_API = 2,        // API status_code != 0
  PARSE_ERR_STRUCTURE = 3   // JSON fine but expected sections are missing
};

// ---- Readers -------------------------------------------------------------
class MemReader {
 public:
  MemReader(const char* p, size_t n) : p_(p), n_(n), i_(0) {}
  int read() { return i_ < n_ ? (uint8_t)p_[i_++] : -1; }
  int peek() { return i_ < n_ ? (uint8_t)p_[i_] : -1; }
  size_t readBytes(char* b, size_t k) {
    size_t r = 0;
    while (r < k && i_ < n_) b[r++] = p_[i_++];
    return r;
  }
 private:
  const char* p_; size_t n_, i_;
};

#ifndef ARDUINO
class IStreamReader {
 public:
  explicit IStreamReader(std::istream& s) : s_(s) {}
  int read() { int c = s_.get(); return c == std::istream::traits_type::eof() ? -1 : c; }
  int peek() { int c = s_.peek(); return c == std::istream::traits_type::eof() ? -1 : c; }
  size_t readBytes(char* b, size_t k) { s_.read(b, (std::streamsize)k); return (size_t)s_.gcount(); }
 private:
  std::istream& s_;
};
#endif

// ---- Shared pieces (parse.cpp) ------------------------------------------
IconKey iconFromKey(const char* key);
const char* iconKeyName(IconKey k);

void makeForecastFilter(JsonDocument& f);
void makeStationObsFilter(JsonDocument& f);
void makeStatsFilter(JsonDocument& f);
void makeStationsFilter(JsonDocument& f);
ParseStatus applyBetterForecast(JsonVariantConst root, WeatherData& out);
ParseStatus applyStationObs(JsonVariantConst root, WeatherData& out);
// year/month = local date required in the newest row (0 = no check).
ParseStatus applyStats(JsonVariantConst root, int year, int month, WeatherData& out);
// stationId 0 = first station. Fills deviceId with the "ST" device.
ParseStatus applyStations(JsonVariantConst root, uint32_t stationId, uint32_t& deviceId);

template <class In> ParseStatus parseJsonFiltered(In& in, void (*mk)(JsonDocument&), JsonDocument& doc) {
  JsonDocument filter;
  mk(filter);
  DeserializationError e = deserializeJson(doc, in, DeserializationOption::Filter(filter));
  return e ? PARSE_ERR_JSON : PARSE_OK;
}

// ---- better_forecast: current conditions + hourly + daily -----------------
template <class In> ParseStatus parseBetterForecast(In& in, WeatherData& out) {
  JsonDocument doc;
  ParseStatus s = parseJsonFiltered(in, makeForecastFilter, doc);
  return s != PARSE_OK ? s : applyBetterForecast(doc.as<JsonVariantConst>(), out);
}
inline ParseStatus parseBetterForecast(const char* j, size_t n, WeatherData& out) {
  MemReader r(j, n); return parseBetterForecast(r, out);
}

// ---- /observations/station/ID: rain rate, lightning, rain today/yesterday ---
template <class In> ParseStatus parseStationObs(In& in, WeatherData& out) {
  JsonDocument doc;
  ParseStatus s = parseJsonFiltered(in, makeStationObsFilter, doc);
  return s != PARSE_OK ? s : applyStationObs(doc.as<JsonVariantConst>(), out);
}
inline ParseStatus parseStationObs(const char* j, size_t n, WeatherData& out) {
  MemReader r(j, n); return parseStationObs(r, out);
}

// ---- /stats/station/ID: rain this month / year -----------------------------
// The response is large (stats_day alone is ~250 KB for a 3 year old station); it is
// skimmed through the filter, only stats_month and stats_year are kept (~10 KB).
template <class In> ParseStatus parseStats(In& in, int year, int month, WeatherData& out) {
  JsonDocument doc;
  ParseStatus s = parseJsonFiltered(in, makeStatsFilter, doc);
  return s != PARSE_OK ? s : applyStats(doc.as<JsonVariantConst>(), year, month, out);
}
inline ParseStatus parseStats(const char* j, size_t n, int year, int month, WeatherData& out) {
  MemReader r(j, n); return parseStats(r, year, month, out);
}

// ---- /stations: find the ST device id ---------------------------------------
template <class In> ParseStatus parseStations(In& in, uint32_t stationId, uint32_t& deviceId) {
  JsonDocument doc;
  ParseStatus s = parseJsonFiltered(in, makeStationsFilter, doc);
  return s != PARSE_OK ? s : applyStations(doc.as<JsonVariantConst>(), stationId, deviceId);
}
inline ParseStatus parseStations(const char* j, size_t n, uint32_t stationId, uint32_t& deviceId) {
  MemReader r(j, n); return parseStations(r, stationId, deviceId);
}

// ---- /observations/device/ID: streamed history -------------------------------
struct HistoryWindow {
  int64_t now;              // epoch "now"; bucket 23 is the clock hour containing it
  int64_t dayStart, dayEnd; // local day window for the day summary [start, end). dayEnd 0 = skip summary
  float pressureOffsetMb;   // added to station pressure (to approximate sea level); 0 = raw
};

// Folds history rows into buckets and the day summary. Rows may arrive in any order.
class HistoryFolder {
 public:
  explicit HistoryFolder(const HistoryWindow& w);
  // row = one obs_st row (array). Returns false if the row was malformed/ignored.
  bool addRow(JsonArrayConst row);
  void finish(WeatherData& out) const;   // converts to display units, sets HAVE_HISTORY
 private:
  struct Acc {
    float t, w, h, p, rain, gust;
    uint16_t n, tn, wn, hn, pn, strikes;
  };
  HistoryWindow w_;
  int64_t base_;
  Acc acc_[HIST_BUCKETS];
  // day window (metric until finish)
  bool dayAny_;
  float hiC_, loC_, rainMm_, gustMs_; uint16_t strikes_;
  int64_t hiT_, loT_, gustT_, lastT_;
  float lastC_, lastWind_, lastRH_;
  float battV_; int64_t battT_;
  uint32_t rows_;
};

inline bool jsonWs(int c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

// Positions the reader just after the [ of the "obs" array.
template <class In> ParseStatus historySeekObs(In& in) {
  int state = 0;
  static const char tok[] = "\"obs\"";
  for (;;) {
    int c = in.read();
    if (c < 0) return PARSE_ERR_JSON;
    if (c == tok[state]) { if (++state == 5) break; }
    else state = (c == '"') ? 1 : 0;
  }
  int c;
  while (jsonWs(c = in.read())) {}
  if (c != ':') return c < 0 ? PARSE_ERR_JSON : PARSE_ERR_STRUCTURE;
  while (jsonWs(c = in.read())) {}
  if (c == '[') return PARSE_OK;
  return c < 0 ? PARSE_ERR_JSON : PARSE_ERR_STRUCTURE;  // e.g. "obs": null
}

template <class In> ParseStatus parseDeviceHistory(In& in, const HistoryWindow& w, WeatherData& out) {
  ParseStatus s = historySeekObs(in);
  if (s != PARSE_OK) return s;
  HistoryFolder folder(w);
  JsonDocument row;
  for (;;) {
    int c;
    while (jsonWs(c = in.peek())) in.read();
    if (c < 0) return PARSE_ERR_JSON;          // truncated
    if (c == ']') break;                       // end of obs
    row.clear();
    if (deserializeJson(row, in)) return PARSE_ERR_JSON;
    folder.addRow(row.as<JsonArrayConst>());
    while (jsonWs(c = in.read())) {}
    if (c == ',') continue;
    if (c == ']') break;
    return PARSE_ERR_JSON;                     // EOF or junk
  }
  folder.finish(out);
  return PARSE_OK;
}
inline ParseStatus parseDeviceHistory(const char* j, size_t n, const HistoryWindow& w, WeatherData& out) {
  MemReader r(j, n); return parseDeviceHistory(r, w, out);
}