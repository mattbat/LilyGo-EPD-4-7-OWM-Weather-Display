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

## Screen 1: Live (iteration H3, glanceable)

Big text, nothing smaller than 20 px, no sub-lines. All coordinates are panel pixels (960 x 540); the exact numbers live in `docs/mockups.html` (`gHeader`, `hTop`, `hChart`, `label`, `optH3`, `alertBand`/`optH4`).

- **Header** (rule at y 50): location label (bold 20, letter-spaced) left; right only the update time (`2:00 PM`, 24 px) and the device battery icon.
- **Top** (rule at y 316): condition icon, temperature 170 px bold (shrinks for negative / 3-digit values), condition 34 px bold under it (shrinks, then ends in "..."). Right: three rows Wind (mph), Rain today (in), Humidity (%), 28 px labels, 56 px bold values, 28 px units, hairline dividers.
- **Chart** (y 318-540): last 6 hourly history buckets (measured, solid 5 px line) then the next 18 forecast hours (dashed 5 px `14 10`), black "now" line and 10 px dot at the current temperature, faint vertical hairlines every 3 h, forecast temperature printed on the line every 3 h (bold 24, paper knockout, 5 px dot). Rain bars on a baseline at y 488: solid = measured rain per hour (auto-scaled, 0.05 in floor), outlined 2 px = forecast precipitation probability. Time labels every 3 h at y 522 (22 px, "NOW" bold). The note `Rain, last 6 h: 0.21 in` (22 px, top-left at y 344) appears only when the measured rain in that window is at least 0.005 in.
- **Alert band**: when something needs attention a full-width black band replaces the chart area (y 318-540): large white icon, 46 px bold white headline, 28 px white sub line. First match wins (`alert.h`):
  1. Lightning strikes in the last 3 h: `28 lightning strikes` / `in the last 3 hours · nearest 3 mi`.
  2. Raining now (rate > 0): `Raining 0.24 in/hr` / `0.65 in so far today`.
  3. UV >= 6: `UV 8 · very high` / `limit time in the sun`.
  Otherwise no band. The alert band replaced the old smart slot; the "next rain" state is gone (the forecast bars show it).
- **Offline**: the header time becomes a black rounded chip `OFFLINE · 1:30 PM` (white text, time of the last good data). If there is no good data at all, the error screen is drawn with the reason (WiFi failed / Tempest API error + HTTP code) and the time.
- Missing values draw as `--`, never "nan".

## Screen 2: Overnight summary (option N3)

Drawn by the 10:00 PM wake, then the board sleeps until 5:00 AM. It must not look live. Exact numbers in `docs/mockups.html` (`nightHeader`, `nightFooter`, `optN3`).

- **Black header bar** (0-64): location label (bold 22, letter-spaced), moon, `Paused until 5 AM` (bold 24), all white.
- **TODAY row** (left, x 28-440): High / Low / Rain, 22 px labels over 50 px bold values. Vertical rule at x 452.
- **TOMORROW · <DAY>** (right): icon, high 76 px bold, `/ low` 44 px, condition 28 px bold right-aligned. Rule at y 212.
- **Chart** (tomorrow 5 AM-10 PM): dashed 4 px temperature line, hairline degree gridlines with 20 px labels, peak temperature labelled (bold 28, paper knockout, 6 px dot), outlined precipitation-probability bars with the percentage (bold 20) above every 3rd bar, "rain" axis label, time labels every 4 h (22 px, y 482).
- **Footer** (rule at y 500): centred 22 px `Sunrise 6:59 AM · Last data 10:00 PM`.

If the user presses reset during quiet hours (after midnight), fetch fresh data and draw this screen with the left row titled "YESTERDAY" and the right titled "TODAY · <DAY>" (the chart then covers today 5 AM-10 PM). Then sleep until 5:00 AM.

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
  alert.h/.cpp        // portable: pick the live-screen alert band (lightning > raining > UV) from WeatherData
  screens.h/.cpp      // portable: drawLive(), drawOvernight(), drawChargeMe(), drawError()
  display_hw.cpp      // ESP32 only: epd_init, epd_poweron, epd_clear, epd_draw_grayscale_image, poweroff
  battery.cpp         // ESP32 only: ADC read -> volts and percent
tools/native/
  main_native.cpp     // loads test/fixtures, renders every screen to out/*.png (stb_image_write)
test/fixtures/
  station_obs.json, better_forecast.json, device_history.json   // real captured responses
  storm/*.json, raining/*.json                                  // hand-edited copies for the alert band states
```

## Fonts

- Font: **Atkinson Hyperlegible** (OFL), regular + bold, from Google Fonts. Download the TTFs into `tools/fonts/`.
- Generate with LilyGo's `fontconvert.py` (in the LilyGo-EPD47 repo, `scripts/`) **without compression**, so the portable renderer can read the 4-bit glyph bitmaps directly. Flash is 16 MB, so size is not a concern.
- Sizes used across the screens (nothing below 20 px): regular 20, 22, 24, 28, 44; bold 20, 22, 24, 28, 34, 46, 50, 56, 76, 136, 170. For 136 and 170 include only digits, degree sign, minus, dot and slash. Edit the list in `tools/fonts/gen_fonts.py` and rerun it; delete sizes that are no longer used.
- Include the degree sign (U+00B0), middle dot (U+00B7), and arrows (U+2191, U+2193) in the character ranges.

## Testing and CI (required for every task)

- **Every portable module gets Unity tests** under `test/` run by `pio test -e native`: gfx (pixel/line/rect/text bounds checked against framebuffer contents), fonts (glyph lookup, text width), units, parse (all fixtures), history bucketing and day summary, alert picker (each trigger, priority, thresholds), schedule (every wake case, DST days, midnight), charts (scaling and auto-range math), screens (render each screen from fixtures without crashing, plus golden-image checksums or pixel spot checks for key regions). A task is not done until its tests exist and pass.
- ESP32-only modules (`tempest_api`, `display_hw`, `battery`, `main`) keep their logic thin: anything testable (URL building, voltage-to-percent, retry decisions) lives in a portable helper with tests.
- **GitHub Actions** (`.github/workflows/ci.yml`, added in task 1): on every push and PR, copy `secrets.example.h` to `secrets.h`, run `pio test -e native`, build `native` and upload the screen preview PNGs as an artifact, build `t5-47` and upload `firmware.bin`.
- **Release** (`.github/workflows/release.yml`): on a `v*` tag, build `t5-47` and attach the binaries to a GitHub Release. These binaries hold placeholder settings; the real device is flashed from a local build with the real `secrets.h`.

## Implementation tasks (for Sonnet subagents)

Run in this order. Tasks 2, 3 and 4 can run in parallel once task 1 is merged. Each task ends with a commit on its own branch.

1. **Scaffold.** Delete old files. Install PlatformIO Core (`pip install platformio`) and MinGW-w64 if missing. Create the PlatformIO project with both envs, `config.h`, keep the existing `secrets.h`/`secrets.example.h`, empty module stubs. Done when `pio run -e t5-47` and `pio run -e native` both build.
2. **Graphics core + fonts.** `gfx.cpp` primitives with clipping, thick lines (2-8 px), dashed lines, rounded rects, polygon fill, text measure/draw with LEFT/CENTER/RIGHT alignment and letter-spacing. Generate fonts. Native program writes a test-card PNG. Done when the test card shows every primitive and font size correctly.
3. **Data layer.** Capture fixtures with curl using the user's secrets, write `weather_data.h`, `units.cpp`, `parse.cpp` (portable), history bucketing and the day summary (high/low/peak gust with times, rain, strikes). Check the stats endpoint. Done when `pio test -e native` parses all fixtures into the expected values.
4. **Icons + charts.** Portable icon set matching the mockup style (outline icons, 3 px strokes, moon, bolt, battery) and chart helpers. Done when a native gallery PNG shows each one.
5. **Screens.** Implement `drawLive()`, `drawOvernight()`, `drawChargeMe()`, `drawError()` and the offline chip, plus `alert.cpp` (formerly the smart slot) as a pure function with a unit test for each priority case. Add the storm and raining fixtures. Compare the PNGs against `docs/mockups.html` side by side. Done when the previews match the mockups and handle edge cases: negative temps, 3-digit temps, no rain, no lightning, missing history, long condition text.
6. **Device integration.** `tempest_api.cpp`, `display_hw.cpp`, `battery.cpp`, `schedule.cpp` (with unit tests for every case in "Device flow" step 3, including DST change days), `main.cpp`, RTC-memory last-good cache, charge-me mode. Done when the board shows live station data, draws the overnight screen at 10 PM, and survives a WiFi failure.
7. **Flash + tune.** With the board on USB: `pio run -e t5-47 -t upload`, then `pio device monitor`. Check legibility at real size, adjust font sizes and greys, measure awake time, and update the battery estimate.

## Still needed from the user

- Fill in `include/secrets.h` (WiFi and Tempest token / station ID).
- Connect the board by USB for task 7.

## Status after first flash (2026-11-21)

Tasks 1-6 are merged. The firmware runs on the board: WiFi, all Tempest calls, render, display refresh and deep sleep work. Measured awake time is 13.5 s on a fresh boot (including the once-a-day stats call, 4.2 s) and about 9 s on a normal update.

## TODO

- [ ] **Check the values on the panel against the Tempest app** (temperature, feels-like, wind/gusts/direction, rain today/yesterday, humidity, dew point, pressure and trend, lightning, rain month/year, sunrise/sunset).
- [ ] **Tune greyscale and sizes from a photo of the panel** (user will take one). Adjust palette.h greys and any font sizes that are hard to read.
- [ ] **Calibrate the battery reading.** The board reports 4.87 V, above an 18650's 4.2 V maximum. Confirm whether the battery was connected, compare with a multimeter, and fix the divider factor in battery.cpp (and the Charge me thresholds if needed).
- [ ] Confirm GitHub CI passes on Linux, especially the golden checksums in test/test_screens.
- [ ] Measure deep-sleep current if possible, and update the battery-life estimate.
