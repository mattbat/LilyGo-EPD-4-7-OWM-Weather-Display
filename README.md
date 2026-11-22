# Tempest E-ink Display

Firmware for the LilyGo T5 4.7" e-paper board (ESP32-WROVER-E, 16 MB flash, 8 MB PSRAM) that shows data from your own Tempest weather station (WeatherFlow REST API): a live screen with 48 h charts, an overnight summary, and a "Charge me" screen. All drawing code is portable C++, so every screen can also be previewed on a PC as a PNG. See `docs/PLAN.md` for the design and `docs/mockups.html` for the layouts.

## Setup

1. Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/): `python -m pip install --user platformio` (or use the VS Code extension).
2. Copy `include/secrets.example.h` to `include/secrets.h` and fill in your WiFi name/password, Tempest token and station ID. `secrets.h` is gitignored.
3. Non-secret settings (location label, time zone, schedule, battery thresholds, units) are in `include/config.h`.

## Build, upload, monitor (board on USB)

```
pio run -e t5-47
pio run -e t5-47 -t upload
pio device monitor
```

## Native preview and tests (no hardware)

Needs a host C++ compiler on PATH (`g++`; on Windows install MinGW-w64, e.g. `winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT`).

```
pio test -e native      # unit tests
pio run -e native       # builds the preview program
.pio/build/native/program   # renders screens to out/*.png (program.exe on Windows)
```

## CI and releases

GitHub Actions runs the unit tests, builds the native preview and the firmware on every push and pull request, and uploads `screen-previews` and `firmware` artifacts. Pushing a tag like `v1.0.0` publishes a GitHub Release with `firmware.bin`, `bootloader.bin` and `partitions.bin`.

Release and CI binaries are built with the placeholder values from `secrets.example.h`, so they contain no real WiFi or Tempest settings. Flash your own device from a local build (`pio run -e t5-47 -t upload`).
