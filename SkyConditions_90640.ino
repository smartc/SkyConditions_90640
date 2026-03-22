/*
 * ESP32-S3 MLX90640 Sky Conditions Sensor
 * ASCOM Alpaca ObservingConditions Device
 *
 * Provides:
 *   - ASCOM Alpaca ObservingConditions API (port 11111)
 *     SkyTemperature = average of center 50% FOV pixels
 *     Temperature    = sensor ambient/die temperature
 *   - Live thermal WebSocket stream (port 81, 784-byte binary frames at 2 Hz)
 *   - Web status/setup UI (port 80)
 *
 * Libraries required (install via Library Manager):
 *   - Adafruit MLX90640
 *   - WiFiManager (tzapu)
 *   - arduinoWebSockets (Markus Sattler)
 *   - ElegantOTA
 *   - ArduinoJson
 *
 * Board: ESP32S3 Dev Module (or compatible)
 */

#include "config.h"
#include "debug.h"
#include "sky_sensor.h"
#include "web_ui_handler.h"
#include "alpaca.h"
#include "history.h"
#include "config_store.h"

#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <Adafruit_TSL2591.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ElegantOTA.h>

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

// Singleton device object – declared extern in sky_sensor.h, defined here.
SkyConditions skyConditions;

// Thermal sensor (MLX90640)
static Adafruit_MLX90640 mlx;
static float thermalFrame[SENSOR_PIXELS];
static bool  sensorReady    = false;
static unsigned long lastSensorRead = 0;

// Brightness sensor (TSL2591)
static Adafruit_TSL2591 tsl(2591);
static bool  brightnessReady   = false;
static unsigned long lastBrightnessRead = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void readSensor();
void readBrightness();

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(100);
  Debug.println("\nMLX90640 Sky Conditions Sensor – ASCOM Alpaca Driver v" MANUFACTURER_V);

  // ── I2C + MLX90640 ────────────────────────────────────────────────────────
  Wire.begin(THERMAL_SDA_PIN, THERMAL_SCL_PIN);
  Wire.setClock(THERMAL_I2C_FREQ);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Debug.println("WARNING: MLX90640 not found – check wiring!");
  } else {
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx.setRefreshRate(MLX90640_2_HZ);
    sensorReady = true;
    Debug.println("MLX90640 initialised (CHESS mode, 18-bit ADC, 2 Hz)");

    // Prime the internal buffer – the first frame often contains stale data.
    mlx.getFrame(thermalFrame);
  }

  // ── TSL2591 brightness sensor (same I2C bus) ───────────────────────────
  if (!tsl.begin(&Wire)) {
    Debug.println("WARNING: TSL2591 not found – check wiring!");
  } else {
    // MAX gain + 300 ms integration: optimised for night-sky measurements.
    // calculateLux() returns -1 on overflow (bright daytime); handled in readBrightness().
    tsl.setGain(TSL2591_GAIN_MAX);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);  // default; overridden by config on first read
    brightnessReady = true;
    Debug.println("TSL2591 initialised (MAX gain, 300 ms integration)");
  }

  // ── WiFi via WiFiManager ──────────────────────────────────────────────────
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);

  WiFiManagerParameter portalTitle("<p>MLX90640 Sky Conditions Sensor</p>");
  wifiManager.addParameter(&portalTitle);

  Debug.println("Connecting to WiFi (or launching setup portal)...");
  bool wifiOk = wifiManager.autoConnect("SkyCond-Setup", "skycond123");

  if (!wifiOk) {
    Debug.println("WiFi connection failed. Restarting in 3 s...");
    delay(3000);
    ESP.restart();
  }

  Debug.print("WiFi connected. IP: ");
  Debug.println(WiFi.localIP().toString());

  // ── Persistent config (calibration, thresholds) ──────────────────────────
  configLoad(deviceConfig);
  Debug.printf("Config: sqmOffset=%.2f  cloudClear=%.1f  cloudOvercast=%.1f\n",
    deviceConfig.sqmOffset, deviceConfig.cloudClearDelta, deviceConfig.cloudOvercastDelta);

  // ── NTP sync – system clock; configurable preferred server ───────────────
  if (strlen(deviceConfig.ntpServer) > 0) {
    configTime(0, 0, deviceConfig.ntpServer, "pool.ntp.org", "time.nist.gov");
    Debug.println("NTP sync started (UTC) – preferred: " + String(deviceConfig.ntpServer));
  } else {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Debug.println("NTP sync started (UTC) – using pool.ntp.org / time.nist.gov");
  }

  // ── ASCOM Alpaca server (Arduino WebServer on port 11111) ─────────────────
  setupAlpacaServer();

  // ── Web UI (port 80) + WebSocket thermal stream (port 81) ────────────────
  initWebUI();

  // ── OTA firmware updates ──────────────────────────────────────────────────
  ElegantOTA.begin(&webUiServer);

  // ── History ring buffers ──────────────────────────────────────────────────
  historySetup();

  // ── Initial sensor reads ──────────────────────────────────────────────────
  if (sensorReady)     readSensor();
  if (brightnessReady) readBrightness();
  updateThermalSnapshot();  // prime the /thermal.jpg cache before first request

  Debug.println("Setup complete.");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop()
{
  // Close history buckets on schedule.
  historyLoop();

  // Service ASCOM Alpaca HTTP requests and UDP discovery.
  alpacaServer.handleClient();
  handleAlpacaDiscovery();

  // Service web UI HTTP requests and WebSocket frames.
  handleWebUI();

  // Service OTA.
  ElegantOTA.loop();

  // Honour an ASCOM PUT /refresh request from a connected client.
  if (skyConditions.isRefreshRequested()) {
    skyConditions.clearRefreshRequest();
    if (sensorReady) readSensor();
    return;
  }

  // Periodic thermal read at FRAME_INTERVAL_MS.
  if (sensorReady && (millis() - lastSensorRead >= FRAME_INTERVAL_MS)) {
    readSensor();
  }

  // Periodic brightness read at BRIGHTNESS_INTERVAL_MS.
  if (brightnessReady && (millis() - lastBrightnessRead >= BRIGHTNESS_INTERVAL_MS)) {
    readBrightness();
  }
}

// ---------------------------------------------------------------------------
// readSensor()
// ---------------------------------------------------------------------------
void readSensor()
{
  if (mlx.getFrame(thermalFrame) != 0) {
    Debug.println("MLX90640 getFrame() error – skipping this cycle");
    return;
  }

  // getTa(false) returns the sensor die temperature in °C.
  float ambientTemp = mlx.getTa(false);

  skyConditions.update(thermalFrame, ambientTemp);

  historyAccumulateThermal(
    skyConditions.getSkyTemperature(),
    skyConditions.getMinTemperature(),
    skyConditions.getMaxTemperature(),
    skyConditions.getMedianTemperature(),
    skyConditions.getAmbientTemperature(),
    skyConditions.getCloudCover());

  broadcastThermalFrame();

  lastSensorRead = millis();

  Debug.printf("Sky: %.2f C  Min: %.2f C  Max: %.2f C  Median: %.2f C  Amb: %.2f C\n",
    skyConditions.getSkyTemperature(),
    skyConditions.getMinTemperature(),
    skyConditions.getMaxTemperature(),
    skyConditions.getMedianTemperature(),
    skyConditions.getAmbientTemperature());
}

// ---------------------------------------------------------------------------
// readBrightness()
// ---------------------------------------------------------------------------
void readBrightness()
{
  // Auto-gain state – persists across calls.
  static tsl2591Gain_t currentGain    = TSL2591_GAIN_MAX;
  static uint8_t       lastIntegration = 0xFF;  // force apply on first call

  // Apply integration time from config (only when it changes).
  if (deviceConfig.tsl2591Integration != lastIntegration) {
    tsl.setTiming((tsl2591IntegrationTime_t)deviceConfig.tsl2591Integration);
    lastIntegration = deviceConfig.tsl2591Integration;
  }

  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ch0 = lum & 0xFFFF;
  uint16_t ch1 = lum >> 16;
  float    lux = tsl.calculateLux(ch0, ch1);  // -1 on overflow

  if (lux < 0) {
    // Saturated – step down one gain level and wait for the next read.
    switch (currentGain) {
      case TSL2591_GAIN_MAX:  currentGain = TSL2591_GAIN_HIGH; break;
      case TSL2591_GAIN_HIGH: currentGain = TSL2591_GAIN_MED;  break;
      case TSL2591_GAIN_MED:  currentGain = TSL2591_GAIN_LOW;  break;
      default: break;  // Already at LOW; nothing more to try
    }
    tsl.setGain(currentGain);
    Debug.println("TSL2591: saturated, gain decreased");
  } else {
    skyConditions.updateBrightness(lux);
    historyAccumulateBrightness(skyConditions.getLux(), skyConditions.getSqm());
    Debug.printf("Lux: %.4f  SQM: %.2f mag/arcsec^2\n",
      skyConditions.getLux(), skyConditions.getSqm());

    // Signal very weak – step up one gain level for the next read.
    // Threshold of 256/65535 provides hysteresis against the saturation boundary.
    if (ch0 < 256 && currentGain != TSL2591_GAIN_MAX) {
      switch (currentGain) {
        case TSL2591_GAIN_LOW:  currentGain = TSL2591_GAIN_MED;  break;
        case TSL2591_GAIN_MED:  currentGain = TSL2591_GAIN_HIGH; break;
        case TSL2591_GAIN_HIGH: currentGain = TSL2591_GAIN_MAX;  break;
        default: break;
      }
      tsl.setGain(currentGain);
      Debug.println("TSL2591: weak signal, gain increased");
    }
  }

  lastBrightnessRead = millis();
}
