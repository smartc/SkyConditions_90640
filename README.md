# SkyConditions_90640

ESP32-S3 ASCOM Alpaca **ObservingConditions** device using an MLX90640 32×24 infrared thermal camera and TSL2591 sky brightness sensor.  Provides a fully standards-compliant Alpaca API for sky temperature, cloud cover, sky brightness, and sky quality monitoring — plus a live thermal viewer and trend charts in the browser.

**Current version: 0.3.1**

---

## Features

- **ASCOM Alpaca ObservingConditions** API on port 11111 (Alpaca Interface v1)
  - SkyTemperature, Temperature, CloudCover, SkyBrightness, SkyQuality
  - TimeSinceLastUpdate (all sensors, including LatestUpdateTime via empty SensorName)
  - SensorDescription, AveragePeriod, Refresh
  - UDP discovery on port 32227 (runs on FreeRTOS Core 0, independent of sensor reads)
- **Dual cloud cover calculation**
  - *Mean method*: linear interpolation on the center-FOV average ambient−sky delta
  - *Per-pixel method*: per-pixel interpolation across a configurable region (center FOV or full frame with edge exclusion), then averaged
  - Both methods trended simultaneously; Setup page toggle selects which value is reported to Alpaca
- **ClearDarkSky colormap** for the thermal heatmap — dark navy (clear/cold) through white (overcast/warm), anchored to calibration thresholds so minor temperature gradients in a clear sky stay in the dark-blue range
- **Live thermal WebSocket stream** on port 81 — 784-byte binary frames at 2 Hz
- **Browser UI** on port 80
  - Home page: current readings, live thermal image, dual cloud cover values, brightness
  - Trends page: 60-min and 24-h charts for all sensors including both cloud cover series
  - Setup page: full calibration and configuration including cloud cover method/region
- **Raw thermal matrix endpoint** — `GET /thermalmatrix` returns the full 32×24 pixel temperature array as JSON
- **OTA firmware updates** via ElegantOTA at `/update`
- **WiFiManager** captive portal for first-boot WiFi setup
- **History ring buffers**: 30-second buckets (60 min hi-res) + 15-minute buckets (24 h lo-res)
- Persistent configuration in NVS flash (survives reboots)
- Passes ASCOM Conform Universal 4.2.1 with 0 errors, 0 issues

---

## Hardware

| Component | Part |
|-----------|------|
| MCU | ESP32-S3 Dev Module (PSRAM required) |
| Thermal camera | Melexis MLX90640 (32×24 IR array) |
| Sky brightness | AMS TSL2591 (lux / SQM) |

### Wiring

Both sensors share the same I2C bus:

| Signal | ESP32-S3 Pin |
|--------|-------------|
| SDA | GPIO 8 |
| SCL | GPIO 9 |
| Bus speed | 400 kHz |

---

## Required Libraries

Install via Arduino IDE Library Manager unless noted:

| Library | Source |
|---------|--------|
| Adafruit MLX90640 | Library Manager |
| Adafruit TSL2591 | Library Manager |
| WiFiManager (tzapu) | Library Manager |
| arduinoWebSockets (Markus Sattler) | Library Manager |
| ElegantOTA | Library Manager |
| ArduinoJson | Library Manager |

---

## Build & Flash

1. Open `SkyConditions_90640.ino` in Arduino IDE 2.x
2. Board: **ESP32S3 Dev Module**
3. Enable **PSRAM** in board settings (the thermal JPEG staging buffer is 320×240×3 = 230 KB)
4. Select the correct port
5. Upload

---

## First Boot / WiFi Setup

On first boot (or after navigating to `/reset_wifi`), the device creates a captive-portal access point:

- **SSID:** `SkyCond-Setup`
- **Password:** `skycond123`

Connect to it from any phone or laptop, choose your home WiFi network, and the device will restart and join it.  The assigned IP is shown on the serial monitor and the web UI.

---

## ASCOM Alpaca Interface

The device registers as `ObservingConditions/0` on port **11111**.

### Implemented sensors

| ASCOM Property | Source | Notes |
|----------------|--------|-------|
| `SkyTemperature` | MLX90640 | Average of center 16×12 (192) pixels |
| `Temperature` | MLX90640 | Sensor die temperature (ambient proxy) |
| `CloudCover` | MLX90640 | Configurable: mean or per-pixel method (see Setup) |
| `SkyBrightness` | TSL2591 | Broadband lux with auto-gain |
| `SkyQuality` | TSL2591 | mag/arcsec² from SQM formula |

### Not implemented (return NotImplementedException)

DewPoint, Humidity, Pressure, RainRate, StarFWHM, WindDirection, WindGust, WindSpeed

### Discovery

Alpaca UDP discovery (port 32227) runs in a dedicated FreeRTOS task pinned to Core 0, so it responds immediately even while the MLX90640 frame read (~500 ms) blocks Core 1.

---

## Web UI

| URL | Content |
|-----|---------|
| `http://<ip>/` | Live status: temperatures, cloud cover (both methods), lux/SQM, thermal image |
| `http://<ip>/trends` | 60-min and 24-h trend charts including dual cloud cover series |
| `http://<ip>/setup` | Configuration form |
| `http://<ip>/thermal.jpg` | Latest thermal JPEG snapshot (320×240) |
| `http://<ip>/thermalmatrix` | Raw 32×24 pixel temperature matrix as JSON |
| `http://<ip>/history.json?minutes=N` | Raw history JSON (N = 5–1440) |
| `http://<ip>/update` | ElegantOTA firmware update |
| `http://<ip>/reset_wifi` | Clear WiFi credentials and restart |

WebSocket live stream: `ws://<ip>:81` — 784-byte binary frames (see below).

---

## WebSocket Frame Format (784 bytes)

| Bytes | Type | Content |
|-------|------|---------|
| 0–3 | float32 | Frame minimum temperature (°C) |
| 4–7 | float32 | Frame maximum temperature (°C) |
| 8–11 | float32 | Frame median temperature (°C) |
| 12–15 | float32 | Sky temperature — center 50% FOV average (°C) |
| 16–783 | uint8 × 768 | Pixels normalized 0–255 on the cloud-cover calibration scale |

Pixel values are normalized between `ambient − cloudClearDelta` (cold/clear → 0) and `ambient − cloudOvercastDelta` (warm/overcast → 255), so the canvas colormap stays anchored to calibration thresholds rather than auto-stretching to the frame min/max.

---

## Thermal Heatmap Colormap

The live canvas and JPEG thumbnail both use the **ClearDarkSky** cloud cover palette:

| Value | Color | Meaning |
|-------|-------|---------|
| Cold (t=0) | Dark navy #003E7E | Clear sky |
| → | Blue shades | Partly clear |
| → | Cyan/grey | Partly cloudy |
| Warm (t=1) | Near-white #FAFAFA | Overcast |

The scale is fixed to the cloud-cover calibration thresholds, so a clear sky with a minor temperature gradient stays uniformly dark blue rather than spanning the full palette.

---

## Cloud Cover

Two methods are calculated on every frame and both are trended:

### Mean method (default)
Computes a single ambient−sky delta using the center-FOV (16×12 pixel) average sky temperature, then linearly interpolates to 0–100%.

### Per-pixel method
Applies the same interpolation independently to each pixel in the selected region, then averages the result.

**Region options** (Setup page):
- *Centre FOV*: center 16×12 pixels (192 pixels)
- *Full Frame*: all 32×24 pixels minus a configurable edge border (default: 2 pixels excluded on each side)

The Setup page toggle **Cloud Cover Method** selects which value is reported to ASCOM Alpaca. Both values are always shown on the home page and trended on the charts.

---

## Configuration (Setup Page)

| Setting | Default | Description |
|---------|---------|-------------|
| SQM Offset | 0.0 | mag/arcsec² additive offset on SQM readings |
| SQM Reference | 108000 lux | Lux level that maps to SQM 0.0 |
| Cloud Clear Delta | 20 °C | Ambient−sky delta at or above which sky is 0% cloud |
| Cloud Overcast Delta | 5 °C | Ambient−sky delta at or below which sky is 100% cloud |
| Cloud Cover Method | Mean | Selects mean or per-pixel value reported to Alpaca |
| Per-Pixel Region | Centre FOV | Region used for per-pixel method |
| Edge Exclusion | 2 px | Pixels excluded from each edge in Full Frame mode |
| Snapshot Interval | 30 s | Thermal JPEG refresh period |
| JPEG Quality | 80 | JPEG encoding quality (1–100) |
| TSL2591 Integration | 300 ms | Integration time for the brightness sensor |
| Average Period | 0.5 s | ASCOM ObservingConditions AveragePeriod |
| Location | Observatory | Appears in Alpaca management API response |
| NTP Server | *(blank)* | Preferred NTP server (blank = pool.ntp.org / time.nist.gov) |

All settings are persisted to NVS flash and survive reboots.

---

## Thermal Matrix Endpoint

`GET http://<ip>/thermalmatrix` returns a JSON object with the full 32×24 pixel temperature array:

```json
{
  "rows": 24,
  "cols": 32,
  "ambient": 18.43,
  "sky": 3.21,
  "pixels": [
    [3.10, 3.14, ..., 3.08],
    ...
    [3.22, 3.19, ..., 3.31]
  ]
}
```

Values are in °C, rounded to 2 decimal places.  `sky` is the center-FOV average (the value reported to ASCOM as `SkyTemperature`).

---

## Port Summary

| Port | Protocol | Purpose |
|------|----------|---------|
| 80 | HTTP | Web UI |
| 81 | WebSocket | Live thermal binary stream |
| 11111 | HTTP | ASCOM Alpaca API |
| 32227 | UDP | Alpaca discovery |

---

## File Structure

| File | Purpose |
|------|---------|
| `SkyConditions_90640.ino` | Entry point: WiFi, sensor init, main loop |
| `config.h` | Compile-time constants (pins, ports, frame geometry, version) |
| `config_store.h/.cpp` | NVS persistent config load/save |
| `debug.h/.cpp` | Conditional serial debug wrapper |
| `sky_sensor.h/.cpp` | `SkyConditions` class — sensor data, stats, dual cloud cover, colormap |
| `alpaca.h/.cpp` | ASCOM Alpaca HTTP server (port 11111) + UDP discovery |
| `web_ui_handler.h/.cpp` | Web server (port 80) + WebSocket server (port 81) |
| `html_templates.h` | Inline HTML/CSS/JS for browser pages |
| `history.h/.cpp` | Dual-resolution history ring buffers |
