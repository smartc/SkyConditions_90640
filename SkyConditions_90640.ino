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
#include "config_store.h"
#include "src/debug/debug.h"
#include "src/alpaca/alpaca.h"
#include "src/sensors/sky_sensor.h"
#include "src/sensors/dht_sensor.h"
#include "src/sensors/rain_sensor.h"
#include "src/sensors/history.h"
#include "src/sensors/sky_history.h"
#include "src/webserver/web_ui_handler.h"
#include "src/mqtt/mqtt_handler.h"
#include "src/safety/rain_safety_broadcast.h"
#include "src/watchdog/watchdog.h"

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
  Debug.println("Board (Arduino IDE selection): " IDE_BOARD_ID);
#if defined(BOARD_AUTO_FALLBACK)
  Debug.println("WARNING: board id not recognised – defaulted pinout profile to " BOARD_NAME
                ". Verify I2C pins on /setup, or force a profile in config.h.");
#else
  Debug.println("Pinout profile (auto-detected): " BOARD_NAME);
#endif

  // ── I2C bus recovery (bit-bang) ───────────────────────────────────────────
  // If a previous boot crashed mid-transaction, a slave may be holding SDA low
  // (bus lockup). Send 9 SCL pulses to clock out the stuck byte, then a STOP
  // condition to release the bus, before handing control to Wire.
  {
    pinMode(THERMAL_SCL_PIN, OUTPUT);
    pinMode(THERMAL_SDA_PIN, INPUT_PULLUP);   // let slave release SDA naturally
    digitalWrite(THERMAL_SCL_PIN, HIGH);
    for (int i = 0; i < 9; i++) {
      digitalWrite(THERMAL_SCL_PIN, LOW);  delayMicroseconds(10);
      digitalWrite(THERMAL_SCL_PIN, HIGH); delayMicroseconds(10);
      if (digitalRead(THERMAL_SDA_PIN)) break;  // SDA released – bus is free
    }
    // Generate STOP: SDA LOW→HIGH while SCL is HIGH
    pinMode(THERMAL_SDA_PIN, OUTPUT);
    digitalWrite(THERMAL_SDA_PIN, LOW);  delayMicroseconds(10);
    digitalWrite(THERMAL_SCL_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(THERMAL_SDA_PIN, HIGH); delayMicroseconds(10);
    // Release both pins before Wire takes over
    pinMode(THERMAL_SDA_PIN, INPUT);
    pinMode(THERMAL_SCL_PIN, INPUT);
    delay(10);
  }

  // ── I2C + sensors ─────────────────────────────────────────────────────────
  Wire.end();
  Wire.setPins(THERMAL_SDA_PIN, THERMAL_SCL_PIN);
  //Wire.begin(THERMAL_SDA_PIN, THERMAL_SCL_PIN);
  Wire.begin();
  Wire.setClock(THERMAL_I2C_FREQ);
  delay(50);

  // Multi-pair I2C pin scanner.
  // Tries every candidate SDA/SCL pair and reports which one sees the sensors.
  // Expected: MLX90640 = 0x33, TSL2591 = 0x29.
  {
    struct PinPair { int sda; int scl; const char *label; };
    static const PinPair candidates[] = {
      {  5,  6, "GPIO5/6  (XIAO D4/D5)" },
      {  6,  5, "GPIO6/5  (XIAO D4/D5 reversed)" },
      {  6,  7, "GPIO6/7  (XIAO D5/D6)" },
      {  7,  6, "GPIO7/6  (XIAO D5/D6 reversed)" },
      {  4,  5, "GPIO4/5  (XIAO D3/D4)" },
      {  5,  4, "GPIO5/4  (XIAO D3/D4 reversed)" },
      {  3,  4, "GPIO3/4  (XIAO D2/D3)" },
      {  4,  3, "GPIO4/3  (XIAO D2/D3 reversed)" },
      {  8,  9, "GPIO8/9  (ESP32-S3 Dev)" },
      {  9,  8, "GPIO9/8  (ESP32-S3 Dev reversed)" },
    };
    // Stop as soon as we find the sensors – scanning wrong pairs after a hit
    // drives the sensor's SCL/SDA lines as the opposite role, causing bus lockup.
    Debug.println("--- I2C pin scan (looking for 0x29/0x33) ---");
    bool pinsFound = false;
    for (const auto &p : candidates) {
      Wire.end();
      Wire.begin(p.sda, p.scl);
      Wire.setClock(100000);
      delay(20);
      Wire.beginTransmission(0x33); bool found33 = (Wire.endTransmission() == 0);
      Wire.beginTransmission(0x29); bool found29 = (Wire.endTransmission() == 0);
      if (found33 || found29) {
        Debug.printf("  *** HIT on %s: %s %s – initialising libraries now\n", p.label,
          found33 ? "MLX90640(0x33)" : "",
          found29 ? "TSL2591(0x29)"  : "");
        pinsFound = true;

        // Init libraries immediately while Wire is live on the correct pins.
        // TSL2591 first: its internal Wire.begin() reinit is survivable and
        // helps settle the bus before the more sensitive MLX90640 init.
        if (found29) {
          if (!tsl.begin(&Wire)) {
            Debug.println("WARNING: TSL2591 not found – check wiring!");
          } else {
            tsl.setGain(TSL2591_GAIN_MAX);
            tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
            brightnessReady = true;
            Debug.println("TSL2591 initialised (MAX gain, 300 ms integration)");
          }
        }
        // MLX90640 after TSL2591 – retry a few times; the sensor may need a
        // moment to recover after the library's internal Wire.begin() reinit.
        if (found33) {
          for (int attempt = 1; attempt <= 5 && !sensorReady; attempt++) {
            if (attempt > 1) {
              Debug.printf("MLX90640 init retry %d/5...\n", attempt);
              delay(200);
            }
            if (mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
              mlx.setMode(MLX90640_CHESS);
              mlx.setResolution(MLX90640_ADC_18BIT);
              mlx.setRefreshRate(MLX90640_2_HZ);
              sensorReady = true;
              Debug.printf("MLX90640 initialised on attempt %d\n", attempt);
              mlx.getFrame(thermalFrame);
            }
          }
          if (!sensorReady) Debug.println("WARNING: MLX90640 not found – check wiring!");
        }
        break;
      } else {
        Debug.printf("      miss: %s\n", p.label);
      }
    }
    Debug.println("--- pin scan complete ---");
    if (!pinsFound) {
      Debug.println("WARNING: sensors not found on any pin pair");
    }
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

  // ── MQTT / Home Assistant autodiscovery ───────────────────────────────────
  setupMQTT();

  // ── Rain / snow sensor (relay + RS485 Modbus) ────────────────────────────
  if (deviceConfig.rainEnabled) rainSensorSetup();

  // ── Safety sensor UDP broadcast (DDA-compatible) ──────────────────────────
  rainSafetyBroadcastSetup();

  // ── DHT ambient temperature / humidity sensor (optional) ─────────────────
  // For I2C-based BMP sensors, run a full address scan first to confirm what's on the bus.
  if (deviceConfig.dhtType == 3 || deviceConfig.dhtType == 4) {
    Debug.println("--- I2C full scan (BMP sensor configured) ---");
    bool anyFound = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        // Read chip ID register (0xD0) to identify the device
        Wire.beginTransmission(addr);
        Wire.write(0xD0);
        Wire.endTransmission(false);
        Wire.requestFrom(addr, (uint8_t)1);
        uint8_t chipId = Wire.available() ? Wire.read() : 0xFF;
        Debug.printf("  I2C device at 0x%02X  chipId=0x%02X", addr, chipId);
        if      (addr == 0x29 || addr == 0x28) Debug.print(" (TSL2591)");
        else if (addr == 0x33)                 Debug.print(" (MLX90640)");
        else if (chipId == 0x58)               Debug.print(" (BMP280)");
        else if (chipId == 0x60)               Debug.print(" (BME280)");
        else if (chipId == 0x50)               Debug.print(" (BMP388)");
        else if (chipId == 0x42)               Debug.print(" (BMP390)");
        Debug.println("");
        anyFound = true;
      }
    }
    if (!anyFound) Debug.println("  No I2C devices found on bus!");
    Debug.println("--- I2C full scan complete ---");
  }
  if (deviceConfig.dhtType != 0) dhtSetup();

  // ── Health watchdog ───────────────────────────────────────────────────────
  watchdogSetup();

  // ── History ring buffers ──────────────────────────────────────────────────
  historySetup();
  skyHistorySetup();

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

  // Service MQTT.
  mqttLoop();

  // Health watchdog – heap monitor, periodic log.
  watchdogLoop();

  // Rain / snow sensor – relay read + periodic Modbus poll.
  if (deviceConfig.rainEnabled) {
    rainSensorLoop();
    historyAccumulateRain(rainIsWet());
  }

  // Sky history hour clock – always runs (SQM/clear-sky tracking doesn't
  // depend on the rain sensor); rain wet-time is only folded in above when
  // deviceConfig.rainEnabled is true.
  skyHistoryLoop();

  // Safety sensor UDP broadcast – periodic keep-alive + immediate on state change.
  rainSafetyBroadcastLoop();

  // DHT ambient sensor – periodic read.
  if (deviceConfig.dhtType != 0) dhtLoop();

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

  // Ambient temperature: prefer DHT when enabled and valid (MLX die temp runs high
  // due to self-heating; an external DHT gives a true ambient reference).
  float ambientTemp = (deviceConfig.dhtType != 0 && dhtData.valid)
                      ? dhtData.temperature
                      : mlx.getTa(false);

  skyConditions.update(thermalFrame, ambientTemp);

  historyAccumulateThermal(
    skyConditions.getSkyTemperature(),
    skyConditions.getMinTemperature(),
    skyConditions.getMaxTemperature(),
    skyConditions.getMedianTemperature(),
    skyConditions.getAmbientTemperature(),
    skyConditions.getCloudCoverMean(),
    skyConditions.getCloudCoverPixel());

  if (skyConditions.hasBrightnessData())
    skyHistoryAccumulateCloud(skyConditions.getCloudCover(), skyConditions.getLux());

  broadcastThermalFrame();
  broadcastSensorState();

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
    skyHistoryAccumulateSqm(skyConditions.getSqm(), skyConditions.getLux());
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
  broadcastSensorState();
}
