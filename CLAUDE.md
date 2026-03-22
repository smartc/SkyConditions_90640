# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 ASCOM Alpaca **ObservingConditions** device using an MLX90640 32×24 infrared thermal camera.  It exposes a standard Alpaca API for sky temperature monitoring, plus a live thermal viewer in the browser.

## Development Environment

Arduino IDE 2.x project (`.ino` entry point).  Build and flash:
1. Open `SkyConditions_90640.ino` in Arduino IDE 2.x
2. Board: **ESP32S3 Dev Module** (or equivalent)
3. Install all required libraries (see below)
4. Set port, then Upload

No CLI build system — use Arduino IDE or `arduino-cli`.

## Required Libraries

| Library | Source | Notes |
|---------|--------|-------|
| Adafruit MLX90640 | Library Manager | Thermal sensor driver |
| WiFiManager (tzapu) | Library Manager | Captive-portal WiFi setup |
| arduinoWebSockets (Markus Sattler) | Library Manager | `WebSocketsServer.h` |
| ElegantOTA | Library Manager | OTA updates via `/update` |
| esp32-idf-ascom-alpaca (Dark Dragons Astro) | ZIP install | ASCOM Alpaca framework |

The Dark Dragons library is an ESP-IDF component that also works inside Arduino-ESP32.  Install it as a ZIP via **Sketch → Include Library → Add .ZIP Library**.

## Architecture

The project follows the same multi-file structure as the `rain_monitor` reference device.

### File Map

| File | Purpose |
|------|---------|
| `SkyConditions_90640.ino` | Entry point: WiFi, sensor init, ASCOM server, main loop |
| `config.h` | All compile-time constants (pins, ports, FOV bounds, frame format) |
| `debug.h` / `debug.cpp` | Conditional serial debug wrapper |
| `sky_sensor.h` / `sky_sensor.cpp` | `SkyConditions` class – extends `AlpacaServer::ObservingConditions` |
| `web_ui_handler.h` / `web_ui_handler.cpp` | Arduino `WebServer` (port 80) + `WebSocketsServer` (port 81) |
| `html_templates.h` | Inline HTML/CSS/JS generators for home and setup pages |
| `rain_monitor/` | Reference implementation (rain safety monitor) – do not modify |

### Communication Stack

```
MLX90640 --[I2C SDA=8/SCL=9 @ 400kHz]--> ESP32-S3 --[WiFi]-->
    Port  80: Web UI (Arduino WebServer)
    Port  81: Live thermal WebSocket (binary, 784 bytes/frame @ 2 Hz)
    Port 11111: ASCOM Alpaca API (ESP-IDF httpd, Dark Dragons library)
    UDP 32227: Alpaca discovery
```

### ASCOM Integration

The `SkyConditions` class in `sky_sensor.h/.cpp` extends `AlpacaServer::ObservingConditions` from the Dark Dragons library.  The `AlpacaServer::Api` object is created in `setup()` and calls `register_routes()` on an ESP-IDF `httpd_handle_t` server running on port 11111.

**Implemented ASCOM properties:**
- `SkyTemperature` — average of the center 50% FOV pixels (16×12 = 192 pixels)
- `Temperature` — MLX90640 die temperature (ambient proxy)
- `TimeSinceLastUpdate`, `SensorDescription`, `AveragePeriod`, `Refresh`

All other `ObservingConditions` properties return `ALPACA_ERR_NOT_IMPLEMENTED`.

### Thread Safety

The ASCOM HTTP handlers run in a separate ESP-IDF task.  `SkyConditions` uses a `portMUX_TYPE` critical section to protect the five scalar stats (`_skyTemperature`, `_minTemperature`, `_maxTemperature`, `_medianTemperature`, `_ambientTemperature`).  The raw `_frame[768]` array is only accessed from the Arduino main loop and does not need locking.

### WebSocket Binary Frame Format (784 bytes)

| Bytes | Content |
|-------|---------|
| 0–3 | float32 – frame minimum temperature |
| 4–7 | float32 – frame maximum temperature |
| 8–11 | float32 – frame median temperature |
| 12–15 | float32 – sky temperature (center 50% FOV average) |
| 16–783 | uint8 × 768 – pixels normalized 0–255 to frame min/max range |

### Center FOV Definition

The sensor is 32 cols × 24 rows.  The center 50% of each linear dimension is:
- Columns 8–23 (16 cols)
- Rows 6–17 (12 rows)
- 192 pixels total

Constants in `config.h`: `CENTER_COL_START`, `CENTER_COL_END`, `CENTER_ROW_START`, `CENTER_ROW_END`.

## WiFi

Credentials are managed by WiFiManager.  On first boot (or after `/reset_wifi`) the device creates a captive portal AP named **SkyCond-Setup** (password: `skycond123`).

## Planned Extensions

A second I2C device for sky brightness will be added later.  The `SkyConditions` class already stubs out `get_skybrightness()` returning `ALPACA_ERR_NOT_IMPLEMENTED`.
