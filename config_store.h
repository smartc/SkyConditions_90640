#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

/*
 * DeviceConfig – all runtime-tunable settings stored in NVS flash.
 */
struct DeviceConfig {
  // Calibration
  float    sqmOffset;            // mag/arcsec² additive offset on SQM
  float    sqmReference;         // lux value that maps to SQM 0.0 (formula: -2.5*log10(lux/ref)+offset)
  float    cloudClearDelta;      // °C (ambient−sky) ≥ this → 0% cloud cover
  float    cloudOvercastDelta;   // °C (ambient−sky) ≤ this → 100% cloud cover

  // Imaging
  uint16_t snapshotIntervalSec;  // thermal JPEG refresh interval (seconds)
  uint8_t  jpegQuality;          // JPEG quality 0–100

  // Brightness sensor
  uint8_t  tsl2591Integration;   // TSL2591 integration time enum (0=100ms … 5=600ms)

  // ASCOM
  double   averagePeriod;        // ObservingConditions AveragePeriod (seconds)

  // Identity
  char     location[32];         // Location string in Alpaca management API

  // Network
  char     ntpServer[64];        // Preferred NTP server (blank = use defaults only)

  // MQTT / Home Assistant
  bool     mqttEnabled;
  char     mqttServer[MQTT_SERVER_SIZE];
  uint16_t mqttPort;
  char     mqttUser[MQTT_USER_SIZE];
  char     mqttPassword[MQTT_PASSWORD_SIZE];
  char     mqttTopicPrefix[MQTT_TOPIC_SIZE];

  // Cloud cover
  uint8_t  cloudCoverMethod;     // 0 = mean (center FOV avg), 1 = per-pixel
  uint8_t  cloudPixelRegion;     // 0 = center FOV, 1 = full frame (per-pixel only)
  uint8_t  cloudEdgeExclude;     // pixels to ignore on each edge when using full frame

  // Rain sensor
  uint8_t  rainMode;             // 0 = Relay (IO38), 1 = RS485 Modbus
};

// Load settings from NVS; fills in defaults if keys are absent.
void configLoad(DeviceConfig &cfg);

// Save settings to NVS.
void configSave(const DeviceConfig &cfg);

// Singleton shared between the .ino, sky_sensor, web_ui_handler, and html_templates.
extern DeviceConfig deviceConfig;

#endif // CONFIG_STORE_H
