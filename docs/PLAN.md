# Tempest E-ink Display: Rewrite Plan

Status: **all design decisions made.** Waiting only on the user filling in `include/secrets.h`. Ready for implementation by subagents.

Mockups: `docs/mockups.html` (published copy: https://claude.ai/artifact/3SFe7XbkUHYFsB1hyDeAKo). The screens to build are drawn by these functions, and their pixel coordinates are the spec:

| Screen | Function in mockups.html | When it shows |
|---|---|---|
| Live (calm and storm examples) | `optC2(calm)`, `optC2(storm)` | Every update from 5:00 AM through 9:30 PM |
| Overnight summary | `optNight()` | Drawn by the 10:00 PM update, stays up until 5:00 AM |
| Charge me | `optCharge()` | Battery below 3.3 V |

Options A, B and the original C on that page are kept only for history. Do not build them.

## Goal

Replace the OpenWeatherMap sketch with new firmware for the LilyGo T5 4.7" e-paper board that:

1. Pulls data from the user's own Tempest weather station (WeatherFlow REST API).
2. Draws a simpler, high-legibility layout that still includes charts.
3. Can be previewed on a PC as a PNG without hardware, so layout work can be done and reviewed before flashing.

Nothing in the old code needs to be kept. Delete the `.ino`, `forecast_record.h`, `lang*.h`, `Font Files/` and all `opensans*.h` files. The old `DrawBattery()` voltage-to-percent curve can be copied into `battery.cpp`.

## User settings (decided)

| Setting | Value |
|---|---|
| Board | LilyGo T5 4.7" with ESP32-WROVER-E (16 MB flash, 8 MB PSRAM). Not the S3 version. |
| Power | 18650 cell (from a vape, likely unprotected). Sometimes plugged into USB. |
| Units | °F, mph, inches of rain, inHg |
| Update interval | Every 30 minutes, aligned to :00 and :30 |
| Overnight pause | Last update 10:00 PM, next update 5:00 AM |
| Header label | `HIGHGARDEN - STARK ST. TEMPEST` |
| Time zone | US Central, POSIX TZ `CST6CDT,M3.2.0,M11.1.0` |
| Toolchain | PlatformIO |
| Secrets | `include/secrets.h` (gitignored, already created, user fills it in). Template: `include/secrets.example.h`. Defines `WIFI_SSID`, `WIFI_PASSWORD`, `TEMPEST_TOKEN`, `TEMPEST_STATION_ID`, `TEMPEST_DEVICE_ID` (0 = look it up from the station). |

All non-secret values above go in `include/config.h`.

## Screen 1: Live (Option C, extended)

All coordinates are panel pixels (960 x 540). Copy exact positions and sizes from `optC2()`.

| Region | y range | Content |
|---|---|---|
| Header | 0-48 | Location label (left, 18 px bold, letter-spaced). Right: `Sat, Nov 21 · Updated 2:00 PM` = date and the time the data was fetched. Battery icon far right. 2 px rule at y 48. |
| Current | 52-236 | Condition icon. Big temperature (~136 px bold). Condition text and feels-like. 2x2 stat grid (Wind + gusts/dir, Rain today + rate or yesterday, Humidity + dew point, Pressure + trend arrow). **Smart slot**: black rounded box at x 798-936 with inverted text. 2 px rule at y 242. |
| Chart | 254-470 | 48 h chart. Left half: measured temp (solid 4 px line, light-grey area fill). Right half: forecast temp (dashed line) on a slightly darker background. Black "now" line and dot. Labelled high/low points (skip points at the very ends). Temp axis auto-scales to the 48 h min/max rounded to 5°. |
| Rain band | under chart, 40 px | Measured hourly rain as black bars (auto-scaled to the max hour, minimum 0.05 in). Forecast precip probability as grey bars (0-100%). Short notes at top-left and top-right. |
| Lightning row | under rain band | Small bolt with the strike count centered under it, for each hour that had strikes. If none: "no lightning in 24 h" in grey. |
| Time axis | y ~494 | Labels every 6 h, "NOW" bold in the middle. |
| Info line | 504-540 | 2 px rule, then one centered line: sunrise, sunset, UV, rain this month, rain this year, station battery. |

**Smart slot priority** (first match wins):
1. Lightning strikes in the last 3 h > 0: label "LIGHTNING", bolt icon, strike count, "strikes · 3 h", "nearest N mi".
2. Raining now (rain rate > 0): label "RAINING", rate in in/hr.
3. UV ≥ 6: label "UV", value, "high" / "very high" / "extreme".
4. Otherwise: label "NEXT RAIN", day name, hour, and % chance of the first forecast hour with precip probability ≥ 40% within 72 h. If none, show "No rain in 3 days".

**Offline state.** If any fetch fails, draw the live screen from the last good data and replace the header's right-hand text with a black chip: `OFFLINE · last data 1:30 PM`. If there is no good data at all, draw a simple error screen with the reason (WiFi failed / Tempest API error + HTTP code) and the time.

## Screen 2: Overnight summary

Drawn by the 10:00 PM wake, then the board sleeps until 5:00 AM. It must not look live. Copy from `optNight()`.

- **Black header bar** (0-56) with white text: location label, moon icon, `Paused overnight · next update 5:00 AM`.
- **Left column, "TODAY · SAT NOV 21":** rows for High (with time), Low (with time), Rain, Peak gust (with time), Lightning (count or "none"), "At 10 PM" last reading. Below: small area sparkline of today's temperature, midnight to now.
- **Right column, "TOMORROW · SUN NOV 22":** forecast icon, high / low, condition text, rain chance and gust note. Dashed hourly temperature line 5 AM-10 PM with the peak labelled, and rain-chance bars under it.
- **Bottom line:** `Last data 10:00 PM`, sunrise, rain this month and year, station battery.

If the user presses reset during quiet hours (after midnight), fetch fresh data and draw this screen with the left column titled "YESTERDAY · <date>" and the right column titled "TODAY · <date>". Then sleep until 5:00 AM.

## Screen 3: Charge me

Copy from `optCharge()`. Big empty battery outline, "Charge me", battery voltage, last data time, and "Updates restart on their own once the battery is charged."

## Data sources (Tempest REST API)

Base URL `https://swd.weatherflow.com/swd/rest`, auth with `?token=<personal access token>`.

| Call | Used for |
|---|---|
| `GET /better_forecast?station_id=ID&units_temp=f&units_wind=mph&units_pressure=inhg&units_precip=in&units_distance=mi` | `current_conditions` (station's own current readings in the chosen units, including gust, dew point, pressure trend), `forecast.daily` (hi/lo, icon, precip %, sunrise/sunset, conditions text), `forecast.hourly` (temp, precip %, icon, wind gust) |
| `GET /observations/station/ID` | Lightning count last 3 h (`lightning_strike_count_last_3hr`), last strike distance (`lightning_strike_last_distance`), rain yesterday (`precip_accum_local_yesterday`), rain rate (`precip` last minute ×60, or `precip_accum_last_1hr`; check which behaves well on real data) |
| `GET /observations/device/DEVICE_ID?time_start=T-24h&time_end=now` | Last 24 h history for the chart and the overnight summary. `obs` is an array of arrays; `obs_st` field order: 0 epoch, 2 wind avg m/s, 3 gust m/s, 4 dir, 6 pressure mb, 7 temp C, 8 RH, 10 UV, 11 solar W/m², 12 rain mm in interval, 15 lightning count, 16 battery V |
| `GET /stations` | Look up the device ID once if `TEMPEST_DEVICE_ID` is 0 (the device with `device_type` "ST"). Cache it in NVS. |
| `GET /stats/station/ID` (unverified) | Rain this month and this year. If it doesn't work with a personal token, sum daily rain from history each day and keep month/year totals in NVS. |

Notes for the implementer:
- History is metric regardless of unit params: convert in `units.cpp`.
- **Check the history response size first** with `curl`. If it is 1-minute data (~1440 rows), do not load it whole: stream-parse it and fold each row into 24 hourly buckets as it arrives (avg temp/wind/humidity/pressure, max gust with its time, min/max temp with times, sum rain, sum strikes). ArduinoJson 7 can deserialize array elements one at a time from a stream.
- Use `WiFiClientSecure` with `setInsecure()` (or pin the root CA) for HTTPS.
- Station battery: show "OK" at 2.45 V or above, otherwise "LOW x.xx V".
- Save one real response of each call into `test/fixtures/` (token removed) so the native preview and tests use real data.

## Hardware notes

- Panel: 960 x 540, 4-bit greyscale (16 levels), framebuffer = `EPD_WIDTH * EPD_HEIGHT / 2` bytes in PSRAM.
- Display driver: `https://github.com/Xinyuan-LilyGO/LilyGo-EPD47` (`epd_driver.h`). Pin a specific commit in `platformio.ini`.
- Battery ADC is GPIO 36 through a divider. On this board the divider is powered from the display power rail, so call `epd_poweron()` before `analogRead(36)` and `epd_poweroff_all()` after. Use `esp_adc_cal` for Vref, as the old code did.
- E-paper keeps the image with power off, so the board can sleep between updates.

## Device flow (`main.cpp`)

1. Wake (timer or reset). Allocate framebuffer. Read battery voltage.
2. **Battery check.**
   - Below 3.3 V: if the Charge me screen is not already showing (flag in RTC memory), draw it. Then sleep for 2 hours (no WiFi) and check again.
   - In charge-me mode, stay there until the voltage is 3.7 V or higher (hysteresis), then continue normally.
3. **Schedule.** Work out what this wake is for, using local time:
   - 5:00 AM to 9:30 PM: live screen.
   - 10:00 PM: overnight screen.
   - Any other timer wake inside quiet hours (shouldn't happen, but RTC drift): sleep until 5:00 AM.
   - Reset-button wake inside quiet hours: overnight screen (yesterday/today titles), then sleep until 5:00 AM.
   - First boot has no time yet: connect WiFi and sync NTP first, then decide.
4. Connect WiFi (15 s timeout), sync NTP with the TZ string.
5. Fetch the data. Retry each call once.
6. Disconnect WiFi.
7. Render the screen into the framebuffer. Keep the last good data in RTC memory for the offline state.
8. `epd_poweron`, `epd_clear`, draw framebuffer, `epd_poweroff_all`.
9. Sleep until the next :00 or :30 (or 5:00 AM after the overnight screen). Keep the existing small RTC-drift padding (`Delta`), so the wake never lands at xx:59.

## Power

- Rough estimate: ~25-30 mAh/day at every 30 minutes around the clock. Skipping 10 PM-5 AM saves about 30%, so expect roughly 3-4 months from a healthy 2,500 mAh 18650. Measure the real figure in task 7.
- Keep awake time under ~12 s per wake. Log awake time to serial.
- Optional, not yet requested: when voltage is above 4.15 V (charging on USB), update every 10 minutes. Ask the user before building this.

## Architecture

PlatformIO project (replaces the Arduino IDE sketch). Two build environments:

| env | target | purpose |
|---|---|---|
| `t5-47` | ESP32 | real firmware. `platform = espressif32`, `board = esp32dev`, `framework = arduino`, `build_flags = -DBOARD_HAS_PSRAM -mfix-esp32-psram-cache-issue`, 16 MB flash partition table with a large app partition, `monitor_speed = 115200`. Libraries: LilyGo-EPD47 (git, pinned commit), ArduinoJson 7. |
| `native` | Windows PC | renders all screens from fixture JSON to PNGs and runs unit tests. **Needs a host C++ compiler**: install MinGW-w64 (e.g. `winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT`) and make sure `g++` is on PATH. |

The key design rule: **all drawing code is portable C++** that writes into a plain 4-bit framebuffer using our own primitives and our own font renderer. Only `display_hw.cpp` and `battery.cpp` touch board-specific APIs. This is what makes the `native` preview possible.

```
platformio.ini
include/
  config.h            // non-secret settings (see "User settings")
  secrets.h           // gitignored, already exists
  secrets.example.h   // committed template, already exists
src/
  main.cpp            // ESP32 only: wake, battery check, schedule, fetch, render, show, sleep
  schedule.h/.cpp     // portable: decide live / overnight / sleep-until for a given local time + wake reason
  tempest_api.h/.cpp  // ESP32 only: HTTPS calls, streams JSON into WeatherData
  parse.h/.cpp        // portable: JSON -> WeatherData (shared by device and native)
  weather_data.h      // portable: plain structs (current, 24 hourly history buckets, hourly forecast, daily, day summary)
  units.h/.cpp        // portable: C->F, m/s->mph, mm->in, mb->inHg, degrees->compass
  gfx.h/.cpp          // portable: framebuffer, pixel, hline/vline, thick line, rect, fillRect, rounded rect,
                      //           circle, fillCircle, polyline, dashed polyline, polygon fill, text + alignment
  fonts/              // generated GFXfont headers (UNcompressed, see Fonts)
  icons.h/.cpp        // portable: sun, cloud, partly cloudy, rain, snow, storm, fog, moon, bolt, battery
  charts.h/.cpp       // portable: line + area, dashed line, bars, sparkline, axis labels, hi/lo marks
  smart_slot.h/.cpp   // portable: pick the smart slot content from WeatherData
  screens.h/.cpp      // portable: drawLive(), drawOvernight(), drawChargeMe(), drawError()
  display_hw.cpp      // ESP32 only: epd_init, epd_poweron, epd_clear, epd_draw_grayscale_image, poweroff
  battery.cpp         // ESP32 only: ADC read -> volts and percent
tools/native/
  main_native.cpp     // loads test/fixtures, renders every screen to out/*.png (stb_image_write)
test/fixtures/
  station_obs.json, better_forecast.json, device_history.json   // real captured responses
  storm/*.json, raining/*.json                                  // hand-edited copies for other smart slot states
```

## Fonts

- Font: **Atkinson Hyperlegible** (OFL), regular + bold, from Google Fonts. Download the TTFs into `tools/fonts/`.
- Generate with LilyGo's `fontconvert.py` (in the LilyGo-EPD47 repo, `scripts/`) **without compression**, so the portable renderer can read the 4-bit glyph bitmaps directly. Flash is 16 MB, so size is not a concern.
- Sizes used across the three screens: regular 13, 14, 15, 16, 17, 18, 20, 22, 44; bold 13, 14, 15, 17, 18, 19, 20, 22, 32, 42, 50, 76, 84, 136. For 84 and 136 include only digits, degree sign, minus and slash. Merge sizes that look identical on the panel to save build time.
- Include the degree sign (U+00B0), middle dot (U+00B7), and arrows (U+2191, U+2193) in the character ranges.

## Testing and CI (required for every task)

- **Every portable module gets Unity tests** under `test/` run by `pio test -e native`: gfx (pixel/line/rect/text bounds checked against framebuffer contents), fonts (glyph lookup, text width), units, parse (all fixtures), history bucketing and day summary, smart slot (each priority case), schedule (every wake case, DST days, midnight), charts (scaling and auto-range math), screens (render each screen from fixtures without crashing, plus golden-image checksums or pixel spot checks for key regions). A task is not done until its tests exist and pass.
- ESP32-only modules (`tempest_api`, `display_hw`, `battery`, `main`) keep their logic thin: anything testable (URL building, voltage-to-percent, retry decisions) lives in a portable helper with tests.
- **GitHub Actions** (`.github/workflows/ci.yml`, added in task 1): on every push and PR, copy `secrets.example.h` to `secrets.h`, run `pio test -e native`, build `native` and upload the screen preview PNGs as an artifact, build `t5-47` and upload `firmware.bin`.
- **Release** (`.github/workflows/release.yml`): on a `v*` tag, build `t5-47` and attach the binaries to a GitHub Release. These binaries hold placeholder settings; the real device is flashed from a local build with the real `secrets.h`.

## Implementation tasks (for Sonnet subagents)

Run in this order. Tasks 2, 3 and 4 can run in parallel once task 1 is merged. Each task ends with a commit on its own branch.

1. **Scaffold.** Delete old files. Install PlatformIO Core (`pip install platformio`) and MinGW-w64 if missing. Create the PlatformIO project with both envs, `config.h`, keep the existing `secrets.h`/`secrets.example.h`, empty module stubs. Done when `pio run -e t5-47` and `pio run -e native` both build.
2. **Graphics core + fonts.** `gfx.cpp` primitives with clipping, thick lines (2-8 px), dashed lines, rounded rects, polygon fill, text measure/draw with LEFT/CENTER/RIGHT alignment and letter-spacing. Generate fonts. Native program writes a test-card PNG. Done when the test card shows every primitive and font size correctly.
3. **Data layer.** Capture fixtures with curl using the user's secrets, write `weather_data.h`, `units.cpp`, `parse.cpp` (portable), history bucketing and the day summary (high/low/peak gust with times, rain, strikes). Check the stats endpoint. Done when `pio test -e native` parses all fixtures into the expected values.
4. **Icons + charts.** Portable icon set matching the mockup style (outline icons, 3 px strokes, moon, bolt, battery) and chart helpers. Done when a native gallery PNG shows each one.
5. **Screens.** Implement `drawLive()`, `drawOvernight()`, `drawChargeMe()`, `drawError()` and the offline chip, plus `smart_slot.cpp` as a pure function with a unit test for each priority case. Add the storm and raining fixtures. Compare the PNGs against `docs/mockups.html` side by side. Done when the previews match the mockups and handle edge cases: negative temps, 3-digit temps, no rain, no lightning, missing history, long condition text.
6. **Device integration.** `tempest_api.cpp`, `display_hw.cpp`, `battery.cpp`, `schedule.cpp` (with unit tests for every case in "Device flow" step 3, including DST change days), `main.cpp`, RTC-memory last-good cache, charge-me mode. Done when the board shows live station data, draws the overnight screen at 10 PM, and survives a WiFi failure.
7. **Flash + tune.** With the board on USB: `pio run -e t5-47 -t upload`, then `pio device monitor`. Check legibility at real size, adjust font sizes and greys, measure awake time, and update the battery estimate.

## Still needed from the user

- Fill in `include/secrets.h` (WiFi and Tempest token / station ID).
- Connect the board by USB for task 7.
