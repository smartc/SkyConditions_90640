#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

/*
 * DeviceConfig – calibration and threshold values stored in NVS flash.
 *
 * Defaults:
 *   sqmOffset          =  0.0   (mag/arcsec² additive correction)
 *   cloudClearDelta    = 20.0   (ambient − sky ≥ this → 0 % cloud cover)
 *   cloudOvercastDelta =  5.0   (ambient − sky ≤ this → 100 % cloud cover)
 */
struct DeviceConfig {
  float sqmOffset;           // mag/arcsec² additive offset on SQM
  float cloudClearDelta;     // °C delta for "clear" (fully clear sky)
  float cloudOvercastDelta;  // °C delta for "overcast" (fully overcast)
};

// Load settings from NVS; fills in defaults if keys are absent.
void configLoad(DeviceConfig &cfg);

// Save settings to NVS.
void configSave(const DeviceConfig &cfg);

// Singleton shared between the .ino, sky_sensor, web_ui_handler, and html_templates.
extern DeviceConfig deviceConfig;

#endif // CONFIG_STORE_H
