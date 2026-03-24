# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 ASCOM Alpaca **ObservingConditions** device using an MLX90640 32×24 infrared thermal camera and TSL2591 sky brightness sensor.  Exposes a standards-compliant Alpaca API for sky temperature, cloud cover, sky brightness, and sky quality monitoring, plus a live thermal viewer and trend charts in the browser.

## Development Environment

Arduino IDE 2.x project (`.ino` entry point).  Build and flash:
1. Open `SkyConditions_90640.ino` in Arduino IDE 2.x
2. Board: **ESP32S3 Dev Module**
3. Enable **PSRAM** in board settings (thermal JPEG staging buffer is 230 KB)
4. Install all required libraries (see below)
5. Set port, then Upload

No CLI build system — use Arduino IDE or `arduino-cli`.

## Required Libraries

| Library | Source |
|---------|--------|
| Adafruit MLX90640 | Library Manager |
| Adafruit TSL2591 | Library Manager |
| WiFiManager (tzapu) | Library Manager |
| arduinoWebSockets (Markus Sattler) | Library Manager |
| ElegantOTA | Library Manager |
| ArduinoJson | Library Manager |
| PubSubClient | **Bundled** (`PubSubClient.h/.cpp` in project root – no Library Manager install needed) |

The ASCOM Alpaca protocol is implemented directly using the Arduino `WebServer` and `WiFiUDP` classes — no external Alpaca framework library is used.

## Architecture

### File Map

| File | Purpose |
|------|---------|
| `SkyConditions_90640.ino` | Entry point: WiFi, NTP, sensor init, main loop |
| `config.h` | All compile-time constants (pins, ports, FOV bounds, frame format) |
| `config_store.h/.cpp` | `DeviceConfig` struct + NVS Preferences load/save |
| `debug.h/.cpp` | Conditional serial debug wrapper (`Debug.print*`) |
| `sky_sensor.h/.cpp` | `SkyConditions` class – sensor data, stats, cloud cover, WebSocket frame |
| `alpaca.h/.cpp` | ASCOM Alpaca HTTP server (port 11111) + UDP discovery FreeRTOS task |
| `web_ui_handler.h/.cpp` | Arduino `WebServer` (port 80) + `WebSocketsServer` (port 81) |
| `html_templates.h` | Inline HTML/CSS/JS generators for home, trends, and setup pages |
| `history.h/.cpp` | Dual-resolution history ring buffers (30 s / 15 min buckets) |
| `mqtt_handler.h/.cpp` | MQTT client + Home Assistant autodiscovery (PubSubClient) |
| `PubSubClient.h/.cpp` | Bundled MQTT client library (copied from ror_controller) |

### Communication Stack

```
MLX90640 ──[I2C SDA=8/SCL=9 @ 400kHz]──┐
TSL2591  ──[I2C SDA=8/SCL=9 @ 400kHz]──┴──> ESP32-S3 ──[WiFi]──>
    Port     80: Web UI (Arduino WebServer)
    Port     81: Live thermal WebSocket (binary, 784 bytes/frame @ 2 Hz)
    Port  11111: ASCOM Alpaca API (Arduino WebServer, custom implementation)
    UDP   32227: Alpaca discovery (FreeRTOS task, Core 0)
```

### ASCOM Alpaca Implementation

The Alpaca protocol is implemented from scratch in `alpaca.cpp` using the Arduino `WebServer` on port 11111.  There is no external Alpaca framework dependency.

**Implemented ASCOM properties/methods:**

| Property / Method | Source | Notes |
|-------------------|--------|-------|
| `SkyTemperature` | MLX90640 | Average of center 16×12 = 192 pixels |
| `Temperature` | MLX90640 | Sensor die temperature |
| `CloudCover` | MLX90640 | Linear interp on ambient−sky delta |
| `SkyBrightness` | TSL2591 | Broadband lux, auto-gain |
| `SkyQuality` | TSL2591 | SQM mag/arcsec² |
| `TimeSinceLastUpdate(name)` | — | All implemented sensors + `""` (LatestUpdateTime) |
| `SensorDescription(name)` | — | All implemented sensors |
| `AveragePeriod` | NVS | Persisted across reboots |
| `Refresh` | — | Forces immediate sensor read |

All other `ObservingConditions` properties return `NotImplementedException` (error 1024).

**Key Alpaca protocol details:**
- `ClientID` / `ClientTransactionID` are matched case-insensitively; all other PUT parameters (e.g. `AveragePeriod`, `Connected`) are case-sensitive per spec
- Syntactically malformed requests (missing parameter, non-numeric value) → HTTP 400
- Semantically invalid values (e.g. negative `AveragePeriod`) → HTTP 200 + `ErrorNumber: 1025`
- `LatestUpdateTime` is tested by Conform as `TimeSinceLastUpdate("")` — returns a `double` (seconds since most recent sensor update across all sensors), not a DateTime string
- `SensorDescription` and `TimeSinceLastUpdate` for valid-but-unimplemented sensor names return error 1024, which prevents Conform's 6-retry loop

### UDP Discovery

Discovery runs in a dedicated FreeRTOS task pinned to Core 0 (`alpaca_disc`, stack 2048 bytes, priority 1).  This ensures immediate responses even while `mlx.getFrame()` blocks Core 1 for ~500 ms at 2 Hz.  The task owns its own `WiFiUDP` object; `handleAlpacaDiscovery()` in the main loop is a no-op.

### Thread Safety

Both the ASCOM HTTP handlers and the Web UI handlers run on Core 1 (Arduino main loop) — no locking is needed between them.  The UDP discovery task on Core 0 does not access any shared data.

The `_frame[768]` array in `SkyConditions` is only written from `readSensor()` and only read from the same main loop, so no mutex is needed.

### WebSocket Binary Frame Format (784 bytes)

| Bytes | Content |
|-------|---------|
| 0–3 | float32 – frame minimum temperature |
| 4–7 | float32 – frame maximum temperature |
| 8–11 | float32 – frame median temperature |
| 12–15 | float32 – sky temperature (center 50% FOV average) |
| 16–783 | uint8 × 768 – pixels normalized 0–255 to frame min/max range |

### Center FOV Definition

32 cols × 24 rows.  Center 50% of each linear dimension:
- Columns 8–23 (16 cols), Rows 6–17 (12 rows) = 192 pixels total

Constants in `config.h`: `CENTER_COL_START`, `CENTER_COL_END`, `CENTER_ROW_START`, `CENTER_ROW_END`.

### History Ring Buffers

Two resolutions in `history.h/.cpp`:
- **Hi-res**: 120 buckets × 30 s = 60 minutes
- **Lo-res**: 96 buckets × 15 min = 24 hours

Each bucket stores avg/lo/hi for 8 metrics: sky temp, frame min/max, median, ambient, cloud cover, lux, SQM.  Served as JSON from `GET /history.json?minutes=N`.

### Persistent Configuration (`DeviceConfig`)

All runtime-tunable settings are stored in NVS under the `"skyCond"` namespace (keys ≤ 15 chars).  Loaded in `setup()` before `configTime()` and the Alpaca server start so that `ntpServer` is available for the NTP call.

| Field | NVS key | Default |
|-------|---------|---------|
| `sqmOffset` | `sqmOffset` | 0.0 |
| `sqmReference` | `sqmRef` | 108000.0 lux |
| `cloudClearDelta` | `cldClear` | 20.0 °C |
| `cloudOvercastDelta` | `cldOvercast` | 5.0 °C |
| `snapshotIntervalSec` | `snapSec` | 30 s |
| `jpegQuality` | `jpegQuality` | 80 |
| `tsl2591Integration` | `tslInteg` | 2 (300 ms) |
| `averagePeriod` | `avgPeriod` | 0.5 s |
| `location` | `location` | "Observatory" |
| `ntpServer` | `ntpServer` | "" (use pool.ntp.org / time.nist.gov) |
| `mqttEnabled` | `mqttEn` | false |
| `mqttServer` | `mqttServer` | "" |
| `mqttPort` | `mqttPort` | 1883 |
| `mqttUser` | `mqttUser` | "" |
| `mqttPassword` | `mqttPass` | "" |
| `mqttTopicPrefix` | `mqttTopicPfx` | "skyconditions" (+ MAC suffix if unchanged) |

### NTP

`configTime(0, 0, ...)` is called after WiFi connects.  If `deviceConfig.ntpServer` is non-empty it is used as the preferred server (first argument); `pool.ntp.org` and `time.nist.gov` are always passed as fallbacks.  Configurable via the Setup page → Network section.

### MQTT / Home Assistant Autodiscovery

Implemented in `mqtt_handler.h/.cpp` using the bundled PubSubClient library.

**State topic:** `<prefix>/state` — JSON payload published every `MQTT_PUBLISH_INTERVAL` (30 s) and on reconnect.

```json
{
  "sky_temp": -12.5,    "ambient_temp": 18.2,
  "frame_min": -18.0,   "frame_max": 25.0,   "frame_median": 4.3,
  "cloud_cover": 35.0,  "cloud_cover_mean": 35.0, "cloud_cover_pixel": 32.1,
  "lux": 0.0023,        "sqm": 21.5,
  "has_data": true,     "has_brightness": true,
  "ip": "192.168.x.x",  "version": "0.3.1"
}
```

**Image topic:** `<prefix>/thumbnail` — retained binary JPEG (the same image served at `/thermal.jpg`), published alongside state.

**Availability topic:** `<prefix>/availability` — LWT publishes `"offline"`, normal connect publishes `"online"` (retained).

**Publish rate:** state + thumbnail published together, at most once every 30 s (`MQTT_PUBLISH_INTERVAL`).  Also published once on (re)connect.

**HA discovery entities** (published with retain):
- `sky_temperature` — °C, device_class temperature
- `ambient_temperature` — °C, device_class temperature
- `cloud_cover` / `cloud_cover_mean` / `cloud_cover_pixel` — %
- `sky_brightness` — lx, device_class illuminance
- `sky_quality` — mag/arcsec²
- `frame_min_temp` / `frame_max_temp` / `frame_median_temp` — °C
- `sky_thermal_image` — `image` entity, `image/jpeg`, points to thumbnail topic

**Topic prefix default:** `skyconditions` auto-appended with the lowercase MAC hex to avoid broker collisions.  Set a custom prefix in Setup → MQTT to override.

**No inbound command topic** — this is a read-only sensor device.

## WiFi

Credentials managed by WiFiManager.  On first boot (or after `POST /reset_wifi`) the device creates captive portal AP **SkyCond-Setup** (password: `skycond123`).

## CONFORM Status

Passes ASCOM Conform Universal 4.2.1 with **0 errors, 0 issues**.  The Alpaca Protocol Check also passes with 0 errors, 0 issues (2 INFO messages about case-insensitive `ClientTransactionID` handling, which is correct per spec and cannot be suppressed).
