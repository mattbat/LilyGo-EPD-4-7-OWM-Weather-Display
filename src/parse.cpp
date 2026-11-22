#include "parse.h"
#include "units.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ---------------------------------------------------------------- helpers
static bool getF(JsonVariantConst v, float& out) {
  if (!v.is<float>()) return false;
  out = v.as<float>();
  return !isnan(out);
}
static float fOr(JsonVariantConst v, float def) { float x; return getF(v, x) ? x : def; }
static void copyStr(char* dst, size_t n, const char* src) {
  if (!src) { dst[0] = 0; return; }
  size_t i = 0;
  for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
  dst[i] = 0;
}
static int8_t trendFrom(const char* s) {
  if (!s) return TREND_STEADY;
  if (!strcmp(s, "rising")) return TREND_RISING;
  if (!strcmp(s, "falling")) return TREND_FALLING;
  return TREND_STEADY;
}
static ParseStatus checkStatus(JsonVariantConst root) {
  if (!root.is<JsonObjectConst>()) return PARSE_ERR_STRUCTURE;
  JsonVariantConst sc = root["status"]["status_code"];
  if (sc.is<int>() && sc.as<int>() != 0) return PARSE_ERR_API;
  return PARSE_OK;
}
static uint32_t u32(JsonVariantConst v) { return v.is<int64_t>() ? (uint32_t)v.as<int64_t>() : 0; }

struct IconName { const char* name; IconKey key; };
static const IconName kIcons[] = {
  {"clear-day", ICON_CLEAR_DAY}, {"clear-night", ICON_CLEAR_NIGHT},
  {"partly-cloudy-day", ICON_PARTLY_DAY}, {"partly-cloudy-night", ICON_PARTLY_NIGHT},
  {"cloudy", ICON_CLOUDY}, {"rainy", ICON_RAINY},
  {"possibly-rainy-day", ICON_POSSIBLY_RAINY_DAY}, {"possibly-rainy-night", ICON_POSSIBLY_RAINY_NIGHT},
  {"thunderstorm", ICON_THUNDERSTORM},
  {"possibly-thunderstorm-day", ICON_POSSIBLY_THUNDER_DAY}, {"possibly-thunderstorm-night", ICON_POSSIBLY_THUNDER_NIGHT},
  {"snow", ICON_SNOW}, {"possibly-snow-day", ICON_POSSIBLY_SNOW_DAY}, {"possibly-snow-night", ICON_POSSIBLY_SNOW_NIGHT},
  {"sleet", ICON_SLEET}, {"possibly-sleet-day", ICON_POSSIBLY_SLEET_DAY}, {"possibly-sleet-night", ICON_POSSIBLY_SLEET_NIGHT},
  {"foggy", ICON_FOGGY}, {"windy", ICON_WINDY},
};
IconKey iconFromKey(const char* key) {
  if (!key) return ICON_UNKNOWN;
  for (const IconName& i : kIcons) if (!strcmp(i.name, key)) return i.key;
  return ICON_UNKNOWN;
}
const char* iconKeyName(IconKey k) {
  for (const IconName& i : kIcons) if (i.key == k) return i.name;
  return "unknown";
}

void weatherDataInit(WeatherData& d) {
  memset(&d, 0, sizeof(d));
  CurrentConditions& c = d.cur;
  c.tempF = c.feelsLikeF = c.windMph = c.gustMph = c.dewPointF = c.pressureInHg = NAN;
  c.rainTodayIn = c.rainYesterdayIn = c.rainRateInHr = c.rainLastHourIn = NAN;
  c.uv = c.lastStrikeMi = c.stationBatteryV = NAN;
  for (HistBucket& b : d.hist) b.tempF = b.windMph = b.gustMph = b.humidityPct = b.pressureInHg = NAN;
  DaySummary& s = d.today;
  s.highF = s.lowF = s.rainIn = s.peakGustMph = s.lastTempF = s.lastWindMph = s.lastHumidityPct = NAN;
  d.rainMonthIn = d.rainYearIn = NAN;
}

// ---------------------------------------------------------------- filters
void makeForecastFilter(JsonDocument& f) {
  f["status"]["status_code"] = true;
  JsonObject u = f["units"].to<JsonObject>();
  u["units_temp"] = true; u["units_wind"] = true; u["units_pressure"] = true;
  u["units_precip"] = true; u["units_distance"] = true;
  const char* ck[] = {"time", "air_temperature", "feels_like", "conditions", "icon", "wind_avg", "wind_gust",
                      "wind_direction", "relative_humidity", "dew_point", "sea_level_pressure", "pressure_trend",
                      "uv", "solar_radiation", "precip_accum_local_day", "precip_accum_local_yesterday",
                      "lightning_strike_count_last_1hr", "lightning_strike_count_last_3hr",
                      "lightning_strike_last_distance", "lightning_strike_last_epoch", "precip_probability"};
  for (const char* k : ck) f["current_conditions"][k] = true;
  const char* hk[] = {"time", "air_temperature", "wind_gust", "precip", "precip_probability", "icon"};
  for (const char* k : hk) f["forecast"]["hourly"][0][k] = true;
  const char* dk[] = {"day_start_local", "air_temp_high", "air_temp_low", "icon", "conditions",
                      "precip_probability", "sunrise", "sunset"};
  for (const char* k : dk) f["forecast"]["daily"][0][k] = true;
}

void makeStationObsFilter(JsonDocument& f) {
  f["status"]["status_code"] = true;
  const char* k[] = {"timestamp", "air_temperature", "feels_like", "dew_point", "sea_level_pressure",
                     "relative_humidity", "wind_avg", "wind_gust", "wind_direction", "uv", "solar_radiation",
                     "precip", "precip_accum_last_1hr", "precip_accum_local_day", "precip_accum_local_yesterday",
                     "precip_accum_local_yesterday_final", "pressure_trend",
                     "lightning_strike_count_last_1hr", "lightning_strike_count_last_3hr",
                     "lightning_strike_last_distance", "lightning_strike_last_epoch"};
  for (const char* x : k) f["obs"][0][x] = true;
}

void makeStatsFilter(JsonDocument& f) {
  f["status"]["status_code"] = true;
  f["stats_month"] = true;
  f["stats_year"] = true;
}

void makeStationsFilter(JsonDocument& f) {
  f["status"]["status_code"] = true;
  f["stations"][0]["station_id"] = true;
  f["stations"][0]["devices"][0]["device_id"] = true;
  f["stations"][0]["devices"][0]["device_type"] = true;
}

// ---------------------------------------------------------------- better_forecast
ParseStatus applyBetterForecast(JsonVariantConst root, WeatherData& out) {
  ParseStatus st = checkStatus(root);
  if (st != PARSE_OK) return st;
  JsonObjectConst cc = root["current_conditions"];
  if (cc.isNull()) return PARSE_ERR_STRUCTURE;
  // We ask for F / mph / inHg / in / mi. Refuse silently wrong data if the request was not honoured.
  JsonObjectConst u = root["units"];
  if (!u.isNull()) {
    auto is = [&](const char* k, const char* v) { const char* s = u[k]; return !s || !strcmp(s, v); };
    if (!is("units_temp", "f") || !is("units_wind", "mph") || !is("units_pressure", "inhg") ||
        !is("units_precip", "in") || !is("units_distance", "mi"))
      return PARSE_ERR_STRUCTURE;
  }
  float x;
  CurrentConditions& c = out.cur;
  c.time = u32(cc["time"]);
  c.tempF = fOr(cc["air_temperature"], NAN);
  c.feelsLikeF = fOr(cc["feels_like"], c.tempF);
  copyStr(c.conditions, COND_LEN, cc["conditions"] | "");
  c.icon = iconFromKey(cc["icon"] | "");
  c.windMph = fOr(cc["wind_avg"], NAN);
  c.gustMph = fOr(cc["wind_gust"], NAN);
  c.windDirDeg = (int16_t)fOr(cc["wind_direction"], 0);
  c.humidityPct = (uint8_t)fOr(cc["relative_humidity"], 0);
  c.dewPointF = fOr(cc["dew_point"], NAN);
  c.pressureInHg = fOr(cc["sea_level_pressure"], NAN);
  c.pressureTrend = trendFrom(cc["pressure_trend"] | "");
  c.uv = fOr(cc["uv"], NAN);
  c.solarWm2 = (int16_t)fOr(cc["solar_radiation"], 0);
  c.precipProbPct = (uint8_t)fOr(cc["precip_probability"], 0);
  if (!(out.have & HAVE_OBS)) {   // station obs is the better source for these; only fill as a fallback
    c.rainTodayIn = getF(cc["precip_accum_local_day"], x) ? x : NAN;
    c.rainYesterdayIn = getF(cc["precip_accum_local_yesterday"], x) ? x : NAN;
    c.strikes1h = (uint16_t)fOr(cc["lightning_strike_count_last_1hr"], 0);
    c.strikes3h = (uint16_t)fOr(cc["lightning_strike_count_last_3hr"], 0);
    c.lastStrikeMi = getF(cc["lightning_strike_last_distance"], x) ? x : NAN;
    c.lastStrikeEpoch = u32(cc["lightning_strike_last_epoch"]);
  }

  out.hourlyCount = 0;
  for (JsonObjectConst h : root["forecast"]["hourly"].as<JsonArrayConst>()) {
    if (out.hourlyCount >= HOURLY_FC_MAX) break;
    if (!h["time"].is<int64_t>()) continue;
    HourlyFc& e = out.hourly[out.hourlyCount++];
    e.time = u32(h["time"]);
    e.tempF = fOr(h["air_temperature"], NAN);
    e.gustMph = fOr(h["wind_gust"], NAN);
    e.precipIn = fOr(h["precip"], 0);
    e.precipProbPct = (uint8_t)fOr(h["precip_probability"], 0);
    e.icon = iconFromKey(h["icon"] | "");
  }
  out.dailyCount = 0;
  for (JsonObjectConst d : root["forecast"]["daily"].as<JsonArrayConst>()) {
    if (out.dailyCount >= DAILY_FC_MAX) break;
    if (!d["day_start_local"].is<int64_t>()) continue;
    DailyFc& e = out.daily[out.dailyCount++];
    e.dayStart = u32(d["day_start_local"]);
    e.highF = fOr(d["air_temp_high"], NAN);
    e.lowF = fOr(d["air_temp_low"], NAN);
    e.icon = iconFromKey(d["icon"] | "");
    copyStr(e.conditions, COND_LEN, d["conditions"] | "");
    e.precipProbPct = (uint8_t)fOr(d["precip_probability"], 0);
    e.sunrise = u32(d["sunrise"]);
    e.sunset = u32(d["sunset"]);
  }
  out.have |= HAVE_FORECAST;
  out.valid = true;
  return PARSE_OK;
}

// ---------------------------------------------------------------- station obs (metric)
ParseStatus applyStationObs(JsonVariantConst root, WeatherData& out) {
  ParseStatus st = checkStatus(root);
  if (st != PARSE_OK) return st;
  JsonObjectConst o = root["obs"][0];
  if (o.isNull()) return PARSE_ERR_STRUCTURE;
  float x;
  CurrentConditions& c = out.cur;
  // rain: "precip" is mm in the last minute
  c.rainRateInHr = getF(o["precip"], x) ? mmToIn(x * 60.0f) : NAN;
  c.rainLastHourIn = getF(o["precip_accum_last_1hr"], x) ? mmToIn(x) : NAN;
  c.rainTodayIn = getF(o["precip_accum_local_day"], x) ? mmToIn(x) : NAN;
  if (getF(o["precip_accum_local_yesterday"], x) || getF(o["precip_accum_local_yesterday_final"], x))
    c.rainYesterdayIn = mmToIn(x);
  else c.rainYesterdayIn = NAN;
  c.strikes1h = (uint16_t)fOr(o["lightning_strike_count_last_1hr"], 0);
  c.strikes3h = (uint16_t)fOr(o["lightning_strike_count_last_3hr"], 0);
  c.lastStrikeMi = getF(o["lightning_strike_last_distance"], x) ? kmToMi(x) : NAN;   // km in the API
  c.lastStrikeEpoch = u32(o["lightning_strike_last_epoch"]);
  if (!(out.have & HAVE_FORECAST)) {   // fallback so the screen still has something
    c.time = u32(o["timestamp"]);
    c.tempF = getF(o["air_temperature"], x) ? cToF(x) : NAN;
    c.feelsLikeF = getF(o["feels_like"], x) ? cToF(x) : c.tempF;
    c.dewPointF = getF(o["dew_point"], x) ? cToF(x) : NAN;
    c.pressureInHg = getF(o["sea_level_pressure"], x) ? mbToInHg(x) : NAN;
    c.pressureTrend = trendFrom(o["pressure_trend"] | "");
    c.humidityPct = (uint8_t)fOr(o["relative_humidity"], 0);
    c.windMph = getF(o["wind_avg"], x) ? msToMph(x) : NAN;
    c.gustMph = getF(o["wind_gust"], x) ? msToMph(x) : NAN;
    c.windDirDeg = (int16_t)fOr(o["wind_direction"], 0);
    c.uv = fOr(o["uv"], NAN);
    c.solarWm2 = (int16_t)fOr(o["solar_radiation"], 0);
  }
  out.have |= HAVE_OBS;
  out.valid = true;
  return PARSE_OK;
}

// ---------------------------------------------------------------- stats
static const int kStatsRainCol = 28;   // total rain (mm) for the period, see docs in parse.h

static bool statsRow(JsonVariantConst arr, const char* prefix, size_t plen, float& mm) {
  JsonArrayConst a = arr.as<JsonArrayConst>();
  if (a.isNull() || a.size() == 0) return false;
  JsonArrayConst row = a[a.size() - 1];
  if (row.isNull() || row.size() <= (size_t)kStatsRainCol) return false;
  if (plen) {
    const char* d = row[0] | "";
    if (strncmp(d, prefix, plen) != 0) return false;
  }
  return getF(row[kStatsRainCol], mm);
}

ParseStatus applyStats(JsonVariantConst root, int year, int month, WeatherData& out) {
  ParseStatus st = checkStatus(root);
  if (st != PARSE_OK) return st;
  char py[8] = {0}, pm[12] = {0};
  if (year > 0) { snprintf(py, sizeof py, "%04d-", year); snprintf(pm, sizeof pm, "%04d-%02d-", year, month); }
  float m, y;
  if (!statsRow(root["stats_month"], pm, strlen(pm), m) || !statsRow(root["stats_year"], py, strlen(py), y))
    return PARSE_ERR_STRUCTURE;
  out.rainMonthIn = mmToIn(m);
  out.rainYearIn = mmToIn(y);
  out.rainMonthYearKnown = true;
  out.have |= HAVE_STATS;
  return PARSE_OK;
}

// ---------------------------------------------------------------- stations
ParseStatus applyStations(JsonVariantConst root, uint32_t stationId, uint32_t& deviceId) {
  ParseStatus st = checkStatus(root);
  if (st != PARSE_OK) return st;
  for (JsonObjectConst s : root["stations"].as<JsonArrayConst>()) {
    if (stationId && u32(s["station_id"]) != stationId) continue;
    for (JsonObjectConst d : s["devices"].as<JsonArrayConst>()) {
      const char* t = d["device_type"] | "";
      if (!strcmp(t, "ST") && d["device_id"].is<int64_t>()) { deviceId = u32(d["device_id"]); return PARSE_OK; }
    }
  }
  return PARSE_ERR_STRUCTURE;
}

// ---------------------------------------------------------------- history folding
// obs_st columns: 0 epoch, 2 wind avg m/s, 3 gust m/s, 6 pressure mb, 7 temp C, 8 RH %,
// 12 rain mm in interval, 15 lightning count, 16 battery V.
HistoryFolder::HistoryFolder(const HistoryWindow& w) : w_(w) {
  base_ = w.now - (((w.now % 3600) + 3600) % 3600);
  memset(acc_, 0, sizeof(acc_));
  for (Acc& a : acc_) a.gust = -1.0f;
  dayAny_ = false;
  hiC_ = -1e9f; loC_ = 1e9f; rainMm_ = 0; gustMs_ = -1; strikes_ = 0;
  hiT_ = loT_ = gustT_ = 0; lastT_ = -1;
  lastC_ = lastWind_ = lastRH_ = NAN;
  battV_ = NAN; battT_ = -1;
  rows_ = 0;
}

bool HistoryFolder::addRow(JsonArrayConst row) {
  if (row.isNull() || row.size() < 17 || !row[0].is<int64_t>()) return false;
  int64_t t = row[0].as<int64_t>();
  float wind, gust, pres, temp, rh, rain, strikes, batt;
  bool hWind = getF(row[2], wind), hGust = getF(row[3], gust), hPres = getF(row[6], pres);
  bool hTemp = getF(row[7], temp), hRh = getF(row[8], rh), hRain = getF(row[12], rain);
  bool hStr = getF(row[15], strikes), hBatt = getF(row[16], batt);
  rows_++;

  if (hBatt && t > battT_) { battT_ = t; battV_ = batt; }

  if (t < base_ + 3600) {   // not in the future
    int64_t k = t >= base_ ? 0 : (base_ - t + 3599) / 3600;
    int idx = HIST_BUCKETS - 1 - (int)k;
    if (k < HIST_BUCKETS && idx >= 0) {
      Acc& a = acc_[idx];
      if (a.n < 65535) a.n++;
      if (hTemp) { a.t += temp; a.tn++; }
      if (hWind) { a.w += wind; a.wn++; }
      if (hGust && gust > a.gust) a.gust = gust;
      if (hRh) { a.h += rh; a.hn++; }
      if (hPres) { a.p += pres; a.pn++; }
      if (hRain) a.rain += rain;
      if (hStr && strikes > 0) a.strikes += (uint16_t)strikes;
    }
  }

  if (w_.dayEnd > w_.dayStart && t >= w_.dayStart && t < w_.dayEnd) {
    dayAny_ = true;
    if (hTemp) {
      if (temp > hiC_ || (temp == hiC_ && t < hiT_)) { hiC_ = temp; hiT_ = t; }
      if (temp < loC_ || (temp == loC_ && t < loT_)) { loC_ = temp; loT_ = t; }
    }
    if (hRain) rainMm_ += rain;
    if (hGust && (gust > gustMs_ || (gust == gustMs_ && t < gustT_))) { gustMs_ = gust; gustT_ = t; }
    if (hStr && strikes > 0) strikes_ += (uint16_t)strikes;
    if (t > lastT_) {
      lastT_ = t;
      lastC_ = hTemp ? temp : NAN;
      lastWind_ = hWind ? wind : NAN;
      lastRH_ = hRh ? rh : NAN;
    }
  }
  return true;
}

void HistoryFolder::finish(WeatherData& out) const {
  out.histStartEpoch = (uint32_t)(base_ - (HIST_BUCKETS - 1) * 3600);
  for (int i = 0; i < HIST_BUCKETS; i++) {
    const Acc& a = acc_[i];
    HistBucket& b = out.hist[i];
    b.valid = a.n > 0;
    b.samples = a.n > 255 ? 255 : (uint8_t)a.n;
    b.tempF = a.tn ? cToF(a.t / a.tn) : NAN;
    b.windMph = a.wn ? msToMph(a.w / a.wn) : NAN;
    b.gustMph = a.gust >= 0 ? msToMph(a.gust) : NAN;
    b.humidityPct = a.hn ? a.h / a.hn : NAN;
    b.pressureInHg = a.pn ? mbToInHg(a.p / a.pn + w_.pressureOffsetMb) : NAN;
    b.rainIn = mmToIn(a.rain);
    b.strikes = a.strikes;
  }
  DaySummary& s = out.today;
  s.valid = dayAny_;
  if (dayAny_) {
    bool haveT = hiC_ > -1e8f;
    s.highF = haveT ? cToF(hiC_) : NAN;  s.highTime = haveT ? (uint32_t)hiT_ : 0;
    s.lowF = haveT ? cToF(loC_) : NAN;   s.lowTime = haveT ? (uint32_t)loT_ : 0;
    s.rainIn = mmToIn(rainMm_);
    bool haveG = gustMs_ >= 0;
    s.peakGustMph = haveG ? msToMph(gustMs_) : NAN;  s.peakGustTime = haveG ? (uint32_t)gustT_ : 0;
    s.strikes = strikes_;
    s.lastTime = (uint32_t)lastT_;
    s.lastTempF = isnan(lastC_) ? NAN : cToF(lastC_);
    s.lastWindMph = isnan(lastWind_) ? NAN : msToMph(lastWind_);
    s.lastHumidityPct = lastRH_;
  } else {
    s = DaySummary();
    s.highF = s.lowF = s.rainIn = s.peakGustMph = s.lastTempF = s.lastWindMph = s.lastHumidityPct = NAN;
  }
  if (battT_ >= 0) out.cur.stationBatteryV = battV_;
  out.have |= HAVE_HISTORY;
  out.valid = true;
}