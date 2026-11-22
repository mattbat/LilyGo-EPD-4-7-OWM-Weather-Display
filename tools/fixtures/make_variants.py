#!/usr/bin/env python3
"""Derive the hand-edited fixture variants from the real captures in test/fixtures/.

Run from the repository root:  python tools/fixtures/make_variants.py
Output: test/fixtures/{storm,raining,cold,hot,nohistory}/*.json (only the files that differ;
the loaders fall back to the base directory for the rest). The edits only change values, never
the structure, so the parsers see exactly the real API formats.

Fixture time: "now" = 1795313256 (Sat Nov 21 2026, 8:07 PM CST). Bucket 23 of the history is the
clock hour 8 PM, bucket 22 is 7 PM, and so on.
"""
import copy, json, os

ROOT = os.path.join("test", "fixtures")
NOW = 1795313256
NOW_H = NOW // 3600 * 3600          # 8 PM
HIST_START = NOW_H - 23 * 3600


def load(name):
    with open(os.path.join(ROOT, name), encoding="utf-8") as f:
        return json.load(f)


def save(sub, name, obj, compact=False):
    d = os.path.join(ROOT, sub)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "w", encoding="utf-8", newline="\n") as f:
        if compact:
            json.dump(obj, f, separators=(",", ":"))
        else:
            json.dump(obj, f, indent=1)
        f.write("\n")


def bucket_of(t):
    return (t - HIST_START) // 3600


TEMP_KEYS = ("air_temperature", "feels_like", "dew_point", "wet_bulb_temperature",
             "wet_bulb_globe_temperature", "air_temp_high", "air_temp_low")


def shift_forecast(fc, dF):
    def walk(o):
        if isinstance(o, dict):
            for k, v in o.items():
                if k in TEMP_KEYS and isinstance(v, (int, float)):
                    o[k] = round(v + dF, 1)
                else:
                    walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)
    walk(fc["current_conditions"])
    walk(fc["forecast"])


def set_current(fc, **kw):
    fc["current_conditions"].update(kw)


def hist_edit(hist, fn):
    for row in hist["obs"]:
        fn(row, bucket_of(row[0]))


def rows_by_bucket(hist):
    rows = {}
    for r in hist["obs"]:
        rows.setdefault(bucket_of(r[0]), []).append(r)
    return rows


def spread(hist, per_bucket_mm):
    """Distribute rain (mm) of each bucket evenly over the rows that exist in that bucket."""
    rows = rows_by_bucket(hist)
    for b, mm in per_bucket_mm.items():
        rs = rows.get(b, [])
        for r in rs:
            r[12] = round(mm / len(rs), 4)


def strikes(hist, per_bucket):
    rows = rows_by_bucket(hist)
    for b, n in per_bucket.items():
        rs = rows.get(b, [])
        if rs:
            rs[min(10, len(rs) - 1)][15] = n


def shift_hist_temp(hist, dC):
    for r in hist["obs"]:
        if isinstance(r[7], (int, float)):
            r[7] = round(r[7] + dC, 2)


def bump_gust(row, b):
    if b >= 21:
        row[3] = round(row[3] + 6, 1)


def zero_rain(row, b):
    row[12] = 0.0


base_fc = load("better_forecast.json")
base_obs = load("station_obs.json")
base_hist = load("device_history.json")

# ---------------------------------------------------------------- storm
fc, obs, hist = copy.deepcopy(base_fc), copy.deepcopy(base_obs), copy.deepcopy(base_hist)
set_current(fc, conditions="Thunderstorm", icon="thunderstorm", wind_avg=18.0, wind_gust=31.0,
            relative_humidity=91, dew_point=60.0, pressure_trend="rising", sea_level_pressure=29.84,
            precip_accum_local_day=0.65, precip_probability=90, wind_direction=225,
            wind_direction_cardinal="SW", lightning_strike_count_last_3hr=28,
            lightning_strike_count_last_1hr=5, lightning_strike_last_distance=3,
            lightning_strike_last_epoch=NOW - 240)
for i, h in enumerate(fc["forecast"]["hourly"][:8]):
    h["precip_probability"] = [70, 45, 20, 10, 10, 10, 5, 5][i]
    if i < 2:
        h["icon"] = "thunderstorm"
        h["conditions"] = "Thunderstorms Likely"
o = obs["obs"][0]
o.update(precip=0.28, precip_accum_last_1hr=11.2, precip_accum_local_day=16.5, wind_avg=8.0,
         wind_gust=13.9, relative_humidity=91, pressure_trend="rising",
         lightning_strike_count=2, lightning_strike_count_last_1hr=5, lightning_strike_count_last_3hr=28,
         lightning_strike_last_distance=5, lightning_strike_last_epoch=NOW - 240)
spread(hist, {19: 0.0, 20: 0.4, 21: 3.8, 22: 8.1, 23: 4.6})
strikes(hist, {19: 3, 20: 2, 21: 9, 22: 14, 23: 5})
hist_edit(hist, bump_gust)
save("storm", "better_forecast.json", fc)
save("storm", "station_obs.json", obs)
save("storm", "device_history.json", hist, compact=True)

# ---------------------------------------------------------------- raining
fc, obs, hist = copy.deepcopy(base_fc), copy.deepcopy(base_obs), copy.deepcopy(base_hist)
set_current(fc, conditions="Rain", icon="rainy", relative_humidity=94, precip_accum_local_day=0.3,
            precip_probability=100, wind_avg=6.0, wind_gust=11.0, pressure_trend="falling")
o = obs["obs"][0]
o.update(precip=0.1, precip_accum_last_1hr=3.4, precip_accum_local_day=7.6, pressure_trend="falling",
         relative_humidity=94)
spread(hist, {20: 0.3, 21: 1.2, 22: 3.0, 23: 2.6})
save("raining", "better_forecast.json", fc)
save("raining", "station_obs.json", obs)
save("raining", "device_history.json", hist, compact=True)

# ---------------------------------------------------------------- cold (negative temps)
fc, obs, hist = copy.deepcopy(base_fc), copy.deepcopy(base_obs), copy.deepcopy(base_hist)
shift_forecast(fc, -75.0)        # 63 F -> -12 F
set_current(fc, conditions="Light Snow and Blowing Snow", icon="snow", relative_humidity=71,
            wind_avg=14.0, wind_gust=26.0, pressure_trend="rising", precip_accum_local_day=0.0)
for h in fc["forecast"]["hourly"]:
    if h["icon"] in ("rainy", "possibly-rainy-night", "possibly-rainy-day"):
        h["icon"] = "snow"
for d in fc["forecast"]["daily"]:
    if d["icon"] in ("rainy", "thunderstorm"):
        d["icon"] = "snow"
        d["conditions"] = "Snow Likely"
o = obs["obs"][0]
o.update(air_temperature=-24.4, feels_like=-32.8, dew_point=-27.8, precip=0.0, precip_accum_last_1hr=0.0,
         precip_accum_local_day=0.0, wind_avg=6.3, wind_gust=11.6)
shift_hist_temp(hist, -41.4)
hist_edit(hist, zero_rain)
save("cold", "better_forecast.json", fc)
save("cold", "station_obs.json", obs)
save("cold", "device_history.json", hist, compact=True)

# ---------------------------------------------------------------- hot (100+ F, UV slot, long text)
fc, obs, hist = copy.deepcopy(base_fc), copy.deepcopy(base_obs), copy.deepcopy(base_hist)
shift_forecast(fc, 41.0)         # 63 F -> 104 F
set_current(fc, conditions="Extremely Hot and Humid", icon="clear-day", relative_humidity=38, uv=9,
            precip_probability=0, wind_avg=9.0, wind_gust=17.0, pressure_trend="falling",
            precip_accum_local_day=0.0)
for h in fc["forecast"]["hourly"]:
    h["precip_probability"] = 0
    h["icon"] = "clear-day"
for d in fc["forecast"]["daily"]:
    d["precip_probability"] = 0
    d["icon"] = "clear-day"
    d["conditions"] = "Hot and Sunny"
o = obs["obs"][0]
o.update(air_temperature=40.0, feels_like=44.4, dew_point=23.0, uv=9, precip=0.0, precip_accum_last_1hr=0.0,
         precip_accum_local_day=0.0)
shift_hist_temp(hist, 23.0)
hist_edit(hist, zero_rain)
save("hot", "better_forecast.json", fc)
save("hot", "station_obs.json", obs)
save("hot", "device_history.json", hist, compact=True)

# ---------------------------------------------------------------- missing history
save("nohistory", "device_history.json",
     {"status": {"status_code": 0, "status_message": "SUCCESS"}, "device_id": 249992,
      "type": "obs_st", "bucket_step_minutes": 1, "obs": []}, compact=True)
print("variants written under", ROOT)
