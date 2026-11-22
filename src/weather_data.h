// Portable plain structs shared by parser, screens and tests.
// Fixed-size arrays only, no heap, trivially copyable: safe to cache in RTC memory.
// All values are in display units (F, mph, inHg, inches, miles) unless noted.
// Missing floating point values are NAN; check with isnan().
#pragma once
#include <stdint.h>
#include <math.h>

#define HIST_BUCKETS      24   // measured history, one per clock hour
#define HOURLY_FC_MAX     72   // hourly forecast entries kept (smart slot looks 72 h ahead)
#define DAILY_FC_MAX      6
#define COND_LEN          24   // condition text incl. NUL ("Thunderstorms Likely" = 20)

// Tempest icon keys (better_forecast "icon" field).
enum IconKey : uint8_t {
  ICON_UNKNOWN = 0,
  ICON_CLEAR_DAY, ICON_CLEAR_NIGHT,
  ICON_PARTLY_DAY, ICON_PARTLY_NIGHT, ICON_CLOUDY,
  ICON_RAINY, ICON_POSSIBLY_RAINY_DAY, ICON_POSSIBLY_RAINY_NIGHT,
  ICON_THUNDERSTORM, ICON_POSSIBLY_THUNDER_DAY, ICON_POSSIBLY_THUNDER_NIGHT,
  ICON_SNOW, ICON_POSSIBLY_SNOW_DAY, ICON_POSSIBLY_SNOW_NIGHT,
  ICON_SLEET, ICON_POSSIBLY_SLEET_DAY, ICON_POSSIBLY_SLEET_NIGHT,
  ICON_FOGGY, ICON_WINDY
};

enum PressureTrend : int8_t { TREND_FALLING = -1, TREND_STEADY = 0, TREND_RISING = 1 };

// Bits for WeatherData::have (which parsers have contributed).
enum : uint8_t {
  HAVE_FORECAST = 1,   // current conditions + hourly + daily from better_forecast
  HAVE_OBS      = 2,   // station observation (rain rate, lightning, ...)
  HAVE_HISTORY  = 4,   // 24 h history buckets + day summary
  HAVE_STATS    = 8    // rain month/year
};

struct CurrentConditions {
  uint32_t time;            // epoch of the reading
  float tempF, feelsLikeF;
  char  conditions[COND_LEN];
  IconKey icon;
  float windMph, gustMph;
  int16_t windDirDeg;
  uint8_t humidityPct;
  float dewPointF;
  float pressureInHg;       // sea level
  int8_t pressureTrend;     // PressureTrend
  float rainTodayIn, rainYesterdayIn;
  float rainRateInHr;       // from the last-minute accumulation x 60; 0 if dry, NAN unknown
  float rainLastHourIn;
  float uv;
  int16_t solarWm2;
  uint16_t strikes3h, strikes1h;
  float lastStrikeMi;       // NAN if unknown
  uint32_t lastStrikeEpoch; // 0 if unknown
  float stationBatteryV;    // NAN if unknown
  uint8_t precipProbPct;    // current-hour chance of rain
};

// One clock hour of measured data (bucket 23 = hour containing "now", partial).
struct HistBucket {
  bool  valid;              // at least one row landed in this hour
  uint8_t samples;          // rows folded in (capped at 255)
  float tempF;              // average, NAN if no data
  float windMph;            // average
  float gustMph;            // max
  float humidityPct;        // average
  float pressureInHg;       // average (station pressure + HistoryWindow::pressureOffsetMb)
  float rainIn;             // sum
  uint16_t strikes;         // sum
};

struct HourlyFc {
  uint32_t time;            // epoch, top of the hour
  float tempF;
  float gustMph;
  float precipIn;
  uint8_t precipProbPct;
  IconKey icon;
};

struct DailyFc {
  uint32_t dayStart;        // local midnight epoch
  float highF, lowF;
  IconKey icon;
  char conditions[COND_LEN];
  uint8_t precipProbPct;
  uint32_t sunrise, sunset; // epoch
};

// Summary of one local day from the 1-minute history.
struct DaySummary {
  bool valid;
  float highF; uint32_t highTime;
  float lowF;  uint32_t lowTime;
  float rainIn;
  float peakGustMph; uint32_t peakGustTime;
  uint16_t strikes;
  uint32_t lastTime;        // newest reading inside the window
  float lastTempF;
  float lastWindMph;
  float lastHumidityPct;
};

struct WeatherData {
  bool valid;               // set once something useful was parsed
  uint8_t have;             // HAVE_* bits
  uint32_t fetchedEpoch;    // set by the caller (time of fetch)

  CurrentConditions cur;

  uint32_t histStartEpoch;  // start of bucket 0 (clock hour)
  HistBucket hist[HIST_BUCKETS];
  DaySummary today;

  uint8_t hourlyCount;
  HourlyFc hourly[HOURLY_FC_MAX];
  uint8_t dailyCount;
  DailyFc daily[DAILY_FC_MAX];

  bool  rainMonthYearKnown;
  float rainMonthIn, rainYearIn;
};

// Reset to an empty, invalid state (all floats NAN where unknown).
void weatherDataInit(WeatherData& d);
