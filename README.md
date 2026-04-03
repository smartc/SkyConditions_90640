# SkyConditions_90640

ESP32-S3 ASCOM Alpaca **ObservingConditions** device using an MLX90640 32×24 infrared thermal camera and TSL2591 sky brightness sensor.  Provides a fully standards-compliant Alpaca API for sky temperature, cloud cover, sky brightness, and sky quality monitoring — plus a live thermal viewer and trend charts in the browser.

**Current version: 0.5.3**

---

## Features

- **ASCOM Alpaca ObservingConditions** (device 0, port 11111, Interface v1)
  - SkyTemperature, Temperature, CloudCover, SkyBrightness, SkyQuality, Humidity (when DHT/BME280 enabled)
  - TimeSinceLastUpdate (all sensors, including LatestUpdateTime via empty SensorName)
  - SensorDescription, AveragePeriod, Refresh
- **ASCOM Alpaca SafetyMonitor** (device 0, port 11111, Interface v1)
  - IsSafe backed by rain/snow relay sensor; hidden from discovery when rain sensor is disabled
- **ASCOM Alpaca Switch** (device 0, port 11111, ISwitchV2)
  - One read-only analog switch: cloud cover percentage (0–100%), GetSwitch=true when ≥ 50%
- **UDP discovery** on port 32227 — runs on FreeRTOS Core 0 (independent of sensor reads)
- **Dual cloud cover calculation**
  - *Mean method*: linear interpolation on the center-FOV average ambient−sky delta
  - *Per-pixel method*: per-pixel interpolation across a configurable region, then averaged
  - Both methods trended simultaneously; Setup page toggle selects which value is reported to Alpaca
  - Live thermal view bounding box tracks the configured per-pixel region
- **Optional rain / snow sensor** — two-pin relay detection; relay closure = wet
- **Optional ambient sensor** — runtime-selectable: DHT11, DHT22, BMP180, BMP280, or BME280; replaces MLX90640 die temp as ambient reference for cloud cover calculations
- **MQTT / Home Assistant autodiscovery** — publishes all sensor values + thermal thumbnail; HA entities created automatically on connect
- **ClearDarkSky colormap** for the thermal heatmap — dark navy (clear/cold) through white (overcast/warm), anchored to calibration thresholds
- **Live thermal WebSocket stream** on port 81 — 784-byte binary frames at 2 Hz, bicubic-smoothed in the browser
- **Browser UI** on port 80
  - Home page: current readings, live thermal image, dual cloud cover values, brightness, rain status, humidity
  - Trends page: 60-min and 24-h charts for all sensors
  - Setup page: full calibration, sensor enable/type selection, MQTT, NTP, network settings, WiFi scan & reconnect
  - Debug console (`/console`): live serial log with deduplication and copy button
- **Raw thermal matrix endpoint** — `GET /thermalmatrix` returns the full 32×24 temperature array as JSON
- **OTA firmware updates** via ElegantOTA at `/update`
- **WiFi management** — scan for networks and reconnect without a full credential reset (Setup page → WiFi)
- **History ring buffers**: 30-second buckets (60 min hi-res) + 15-minute buckets (24 h lo-res)
- Persistent configuration in NVS flash (survives reboots)
- Passes ASCOM Conform Universal 4.2.1 with **0 errors, 0 issues** on all three devices

---

## Hardware

### Supported Boards

| Board | Notes |
|-------|-------|
| **Seeed XIAO ESP32-S3 Sense Plus** + Grove Shield | Primary target (`#define BOARD_XIAO_SENSE` in `config.h`) |
| ESP32-S3 Dev Module | Alternate; comment out `BOARD_XIAO_SENSE` |

Enable **PSRAM** in board settings — the thermal JPEG staging buffer requires ~230 KB.

### Sensors

| Component | Part | I2C Address |
|-----------|------|-------------|
| Thermal camera | Melexis MLX90640 32×24 IR array | 0x33 |
| Sky brightness | AMS TSL2591 | 0x29 |
| Ambient (optional) | DHT11 / DHT22 / BMP180 / BMP280 / BME280 | GPIO or I2C |

### Wiring (XIAO ESP32-S3 Sense + Grove Shield)

| Signal | XIAO Pin | GPIO |
|--------|----------|------|
| I2C SDA | D4 | GPIO 5 |
| I2C SCL | D5 | GPIO 6 |
| Rain relay drive | D8 | GPIO 7 |
| Rain relay sense | D9 | GPIO 8 |
| DHT data | D7 | GPIO 44 |

The rain sensor uses two pins: drive pin is held OUTPUT HIGH; relay closure pulls the sense pin HIGH (INPUT_PULLDOWN).  No external components required.

BMP180 / BMP280 / BME280 share the existing I2C bus — no extra GPIO needed.

### Wiring (ESP32-S3 Dev Module)

| Signal | GPIO |
|--------|------|
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |

---

## 3D-Printed Enclosure

The `Enclosure/` directory contains STL files and a master 3MF project for the weatherproof housing.  The design is sized for a **Seeed XIAO ESP32-S3** mounted on the **Grove Expansion Board**.

### Print Files

| File | Material | Notes |
|------|----------|-------|
| `Housing - Base.stl` | ASA or PETG | Main enclosure body |
| `Housing - Top.stl` | ASA or PETG | Lid |
| `Housing - Bracket.stl` | ASA or PETG | Mounting bracket |
| `Housing - Bracket Clamp.stl` | ASA or PETG | Bracket clamp |
| `Housing - Plug.stl` | ASA or PETG | Port plug |
| `Sensor Insert Plate.stl` | ASA or PETG | Sensor mounting plate |
| `Stevenson Screen.stl` | ASA or PETG | Stevenson radiation shield (requires supports) |
| `Stevenson Screen - Base Cap.stl` | ASA or PETG | Base cap for Stevenson screen |
| `TPU Housing Body Gasket.stl` | TPU | Weatherproof lid seal |
| `TPU Sensor Insert Gasket.stl` | TPU | Sensor plate seal |
| `TPU Cable Grommet.stl` | TPU | Cable entry grommet |
| `Sky Conditions 90640 - Enclosure.3mf` | — | Master 3MF project (all parts, orientations, materials) |

### Assembly Notes

- **Material:** Print structural parts in **ASA** (preferred outdoors) or **PETG**.  Print gaskets and grommets in **TPU**.
- **Stevenson screen:** Requires supports due to overhangs.
- **Heatset inserts required:**

  | Size | Location |
  |------|----------|
  | M3 | Lid-to-body and bracket fasteners |
  | M2.5 | PCB/expansion board mounting |
  | M2 | Sensor plate and smaller internal fasteners |

- **Fasteners:** Matching M3 / M2.5 / M2 socket-head or pan-head screws.
- **Lens holder:** The Uxcell 20 mm LED lens holder is glued into the top of the housing directly over the TSL2591.
- **Strain relief:** A 3/8″ PVC strain relief fitting is used for the MLX90640 cable entry.

---

## Bill of Materials

| Component | Part | Notes |
|-----------|------|-------|
| Microcontroller | Seeed XIAO ESP32-S3 | |
| Expansion board | Seeed Grove Expansion Board for XIAO | Provides I2C headers and convenient wiring |
| Thermal camera | MLX90640 (TO-92 can format) | I2C, 32×24 IR array |
| Sky brightness | Adafruit TSL2591 | I2C, auto-gain |
| Rain / snow sensor | Relay-based heated sensor | [AliExpress](https://www.aliexpress.com/item/1005005479538840.html) |
| Lens holder | Uxcell 20 mm LED lens holder | [Amazon](https://a.co/d/01UBlwtW) — glued over TSL2591 aperture |
| Strain relief | 3/8″ PVC strain relief fitting | MLX90640 cable entry |
| Hookup wire | 22–26 AWG | I2C bus, sensor connections |

---

## Required Libraries

Install via Arduino IDE Library Manager unless noted:

| Library | Source |
|---------|--------|
| Adafruit MLX90640 | Library Manager |
| Adafruit TSL2591 | Library Manager |
| Adafruit BMP085 Unified | Library Manager (BMP180 support) |
| Adafruit BMP280 | Library Manager (BMP280 support) |
| Adafruit BME280 | Library Manager (BME280 support) |
| DHT sensor library (Adafruit) | Library Manager |
| WiFiManager (tzapu) | Library Manager |
| arduinoWebSockets (Markus Sattler) | Library Manager |
| ElegantOTA | Library Manager |
| ArduinoJson | Library Manager |
| **PubSubClient** | **Bundled** — `PubSubClient.h/.cpp` in project root |

---

## Build & Flash

1. Open `SkyConditions_90640.ino` in Arduino IDE 2.x
2. Select board: **XIAO_ESP32S3** or **ESP32S3 Dev Module**
3. Enable **PSRAM** in board settings
4. Select the correct port
5. Upload

---

## First Boot / WiFi Setup

On first boot (or after a full WiFi reset) the device creates a captive-portal access point:

- **SSID:** `SkyCond-Setup`
- **Password:** `skycond123`

Connect, choose your home network, and the device restarts and joins it.  The assigned IP is shown on the serial monitor.

To change the WiFi network without a full reset, use **Setup → WiFi → Scan for Networks**, select the target AP, enter the password, and click **Change WiFi**.  The device reconnects in place; credentials are persisted to NVS.

---

## ASCOM Alpaca Interface

All three devices are served on port **11111**.  Setup/UI URLs on port 11111 redirect to the web UI on port 80.

### ObservingConditions (device 0)

| ASCOM Property | Source | Notes |
|----------------|--------|-------|
| `SkyTemperature` | MLX90640 | Average of center 16×12 (192) pixels |
| `Temperature` | MLX90640 / ambient sensor | Die temp, or ambient sensor reading when enabled and valid |
| `CloudCover` | MLX90640 | Configurable: mean or per-pixel method (Setup page) |
| `SkyBrightness` | TSL2591 | Broadband lux with auto-gain |
| `SkyQuality` | TSL2591 | mag/arcsec² from SQM formula |
| `Humidity` | DHT / BME280 | %RH — only implemented when a humidity-capable sensor is enabled |

DewPoint, Pressure, RainRate, StarFWHM, WindDirection, WindGust, WindSpeed return `NotImplementedException`.

### SafetyMonitor (device 0)

| Property | Behaviour |
|----------|-----------|
| `IsSafe` | `true` = relay open (dry); `false` = relay closed (wet/rain) |

Hidden from Alpaca discovery when the rain sensor is disabled in Setup.

### Switch (device 0) — Cloud Cover Indicator

| Switch | Name | Type | Range | Notes |
|--------|------|------|-------|-------|
| 0 | Cloud Cover | Analog, read-only | 0–100 | Same value as `CloudCover`; `GetSwitch` = true when ≥ 50% |

### Discovery

UDP discovery (port 32227) runs in a dedicated FreeRTOS task pinned to Core 0 so it responds immediately regardless of sensor read timing on Core 1.

---

## Web UI

| URL | Content |
|-----|---------|
| `http://<ip>/` | Live status: temperatures, cloud cover, lux/SQM, rain, humidity, thermal image |
| `http://<ip>/trends` | 60-min and 24-h trend charts |
| `http://<ip>/setup` | Configuration form |
| `http://<ip>/console` | Live serial debug console |
| `http://<ip>/thermal.jpg` | Latest thermal JPEG snapshot |
| `http://<ip>/thermalmatrix` | Raw 32×24 temperature matrix as JSON |
| `http://<ip>/history.json?minutes=N` | Raw history JSON (N = 5–1440) |
| `http://<ip>/update` | ElegantOTA firmware update |
| `http://<ip>/wifi/scan` | GET — JSON array of nearby WiFi networks |
| `http://<ip>/wifi/connect` | POST (ssid, password) — reconnect without credential reset |
| `http://<ip>/reset_wifi` | POST — clear WiFi credentials and restart |

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

---

## MQTT / Home Assistant

Enable in Setup → MQTT.  The device publishes to `<prefix>/state` every 30 seconds and on reconnect, and sends a thermal thumbnail to `<prefix>/thumbnail`.  Home Assistant discovery entities are published automatically on connect.

**State payload (JSON):**
```json
{
  "sky_temp": -12.5,    "ambient_temp": 18.2,
  "frame_min": -18.0,   "frame_max": 25.0,   "frame_median": 4.3,
  "cloud_cover": 35.0,  "cloud_cover_mean": 35.0, "cloud_cover_pixel": 32.1,
  "lux": 0.0023,        "sqm": 21.5,
  "has_data": true,     "has_brightness": true,
  "ip": "192.168.x.x",  "version": "0.5.3"
}
```

**HA entities auto-created:** sky_temperature, ambient_temperature, cloud_cover, cloud_cover_mean, cloud_cover_pixel, sky_brightness, sky_quality, frame_min/max/median_temp, sky_thermal_image.

The topic prefix defaults to `skyconditions-<MAC>` to avoid broker collisions.  Override it in Setup → MQTT.

---

## Cloud Cover

Two methods run on every frame and are trended independently:

**Mean method** — computes a single ambient−sky delta using the center-FOV (16×12 pixel) average, then linearly interpolates to 0–100%.

**Per-pixel method** — applies the same interpolation to each pixel in the selected region, then averages the results.

Region options (Setup page): *Centre FOV* (192 pixels) or *Full Frame* with a configurable edge exclusion border.

The **Cloud Cover Method** toggle on the Setup page selects which value is reported to ASCOM Alpaca.  Both are always shown on the home page and in trend charts.  The red bounding box in the live thermal view tracks the per-pixel region.

---

## Configuration (Setup Page)

| Setting | Default | Description |
|---------|---------|-------------|
| SQM Offset | 0.0 | Additive offset on SQM readings (mag/arcsec²) |
| SQM Reference | 108000 lux | Lux level that maps to SQM 0.0 |
| Cloud Clear Delta | 20 °C | Ambient−sky delta at which sky is 0% cloud |
| Cloud Overcast Delta | 5 °C | Ambient−sky delta at which sky is 100% cloud |
| Cloud Cover Method | Mean | Selects mean or per-pixel value for Alpaca |
| Per-Pixel Region | Centre FOV | Region for per-pixel method |
| Edge Exclusion | 2 px | Pixels excluded per edge in Full Frame mode |
| Rain Sensor | Enabled | Enable/disable relay-based rain detection |
| Ambient Sensor | Disabled | Sensor type: Disabled / DHT11 / DHT22 / BMP180 / BMP280 / BME280 |
| Snapshot Interval | 30 s | Thermal JPEG refresh period |
| JPEG Quality | 80 | JPEG encoding quality (1–100) |
| TSL2591 Integration | 300 ms | Integration time for the brightness sensor |
| Average Period | 0.5 s | ASCOM ObservingConditions AveragePeriod |
| Location | Observatory | Shown in Alpaca management API |
| NTP Server | *(blank)* | Preferred NTP server (blank = pool.ntp.org) |
| MQTT Enabled | false | Enable MQTT publishing |
| MQTT Server | *(blank)* | Broker hostname or IP |
| MQTT Port | 1883 | Broker port |
| MQTT User / Password | *(blank)* | Broker credentials |
| MQTT Topic Prefix | skyconditions-*mac* | Topic prefix (auto-suffixed with MAC if unchanged) |

All settings persist to NVS flash across reboots.

---

## Port Summary

| Port | Protocol | Purpose |
|------|----------|---------|
| 80 | HTTP | Web UI |
| 81 | WebSocket | Live thermal binary stream |
| 11111 | HTTP | ASCOM Alpaca API (all three devices) |
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
| `alpaca.h/.cpp` | ASCOM Alpaca HTTP server (port 11111) — ObservingConditions, SafetyMonitor, Switch + UDP discovery |
| `web_ui_handler.h/.cpp` | Web server (port 80) + WebSocket server (port 81) |
| `html_templates.h` | Inline HTML/CSS/JS for browser pages |
| `history.h/.cpp` | Dual-resolution history ring buffers |
| `rain_sensor.h/.cpp` | Two-pin relay rain/snow detection |
| `dht_sensor.h/.cpp` | Optional ambient sensor: DHT11/22, BMP180, BMP280, BME280 |
| `mqtt_handler.h/.cpp` | MQTT client + Home Assistant autodiscovery |
| `PubSubClient.h/.cpp` | Bundled MQTT client library |
