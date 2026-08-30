#include "config_store.h"

DeviceConfig deviceConfig;

void configLoad(DeviceConfig &cfg)
{
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, /*readOnly=*/true);

  // Calibration
  cfg.sqmOffset          = prefs.getFloat("sqmOffset",    0.0f);
  cfg.sqmReference       = prefs.getFloat("sqmRef",   108000.0f);
  cfg.cloudClearDelta    = prefs.getFloat("cldClear",    20.0f);
  cfg.cloudOvercastDelta = prefs.getFloat("cldOvercast",  5.0f);

  // Imaging
  cfg.snapshotIntervalSec = prefs.getUShort("snapSec",     JPEG_INTERVAL_MS / 1000);
  cfg.jpegQuality         = prefs.getUChar ("jpegQuality", 80);

  // Brightness sensor (2 = TSL2591_INTEGRATIONTIME_300MS)
  cfg.tsl2591Integration  = prefs.getUChar ("tslInteg",    2);

  // ASCOM
  cfg.averagePeriod       = prefs.getDouble("avgPeriod",   0.5);

  // Identity
  String loc = prefs.getString("location", LOCATION);
  strncpy(cfg.location, loc.c_str(), sizeof(cfg.location) - 1);
  cfg.location[sizeof(cfg.location) - 1] = '\0';

  // Network
  String ntp = prefs.getString("ntpServer", "");
  strncpy(cfg.ntpServer, ntp.c_str(), sizeof(cfg.ntpServer) - 1);
  cfg.ntpServer[sizeof(cfg.ntpServer) - 1] = '\0';

  // MQTT
  cfg.mqttEnabled = prefs.getBool  ("mqttEn",       false);
  cfg.mqttPort    = prefs.getUShort("mqttPort",      DEFAULT_MQTT_PORT);
  String ms = prefs.getString("mqttServer",  DEFAULT_MQTT_SERVER);
  strncpy(cfg.mqttServer,      ms.c_str(),  sizeof(cfg.mqttServer)  - 1);
  cfg.mqttServer[sizeof(cfg.mqttServer) - 1] = '\0';
  String mu = prefs.getString("mqttUser",    DEFAULT_MQTT_USER);
  strncpy(cfg.mqttUser,        mu.c_str(),  sizeof(cfg.mqttUser)    - 1);
  cfg.mqttUser[sizeof(cfg.mqttUser) - 1] = '\0';
  String mp = prefs.getString("mqttPass",    DEFAULT_MQTT_PASSWORD);
  strncpy(cfg.mqttPassword,    mp.c_str(),  sizeof(cfg.mqttPassword)- 1);
  cfg.mqttPassword[sizeof(cfg.mqttPassword) - 1] = '\0';
  String mt = prefs.getString("mqttTopicPfx",DEFAULT_MQTT_TOPIC_PREFIX);
  strncpy(cfg.mqttTopicPrefix, mt.c_str(),  sizeof(cfg.mqttTopicPrefix) - 1);
  cfg.mqttTopicPrefix[sizeof(cfg.mqttTopicPrefix) - 1] = '\0';

  // Cloud cover
  cfg.cloudCoverMethod   = prefs.getUChar("cloudMethod", 0);
  cfg.cloudPixelRegion   = prefs.getUChar("cloudPxRgn",  0);
  cfg.cloudEdgeExclude   = prefs.getUChar("cloudEdge",   2);

  // Rain sensor
  cfg.rainMode           = prefs.getUChar("rainMode",    0);     // default: Relay
  cfg.rainEnabled        = prefs.getBool ("rainEn",      true);  // default: enabled

  // Ambient sensor (0=disabled, 1=DHT11, 2=DHT22, 3=BMP180, 4=BMP280, 5=BME280)
  cfg.dhtType            = prefs.getUChar("dhtType",     0);     // default: disabled
  if (cfg.dhtType > 5) cfg.dhtType = 0;                          // clamp unknown values
  cfg.bmp280Addr         = prefs.getUChar("bmp280Addr",  0x76);  // default: SDO→GND
  if (cfg.bmp280Addr != 0x76 && cfg.bmp280Addr != 0x77) cfg.bmp280Addr = 0x76;

  // Safety sensor UDP broadcast
  cfg.safetyBcastEnabled     = prefs.getBool  ("safetyBcastEn",   false);
  cfg.safetyBcastPort        = prefs.getUShort("safetyBcastPort", RAIN_SAFETY_UDP_PORT_DEFAULT);
  cfg.safetyBcastIntervalSec = prefs.getUShort("safetyBcastSec",  RAIN_SAFETY_INTERVAL_DEFAULT_SEC);
  if (cfg.safetyBcastIntervalSec < 5)  cfg.safetyBcastIntervalSec = 5;    // minimum 5 s
  if (cfg.safetyBcastPort == 0)        cfg.safetyBcastPort = RAIN_SAFETY_UDP_PORT_DEFAULT;

  // Sky History thresholds
  cfg.nightLuxThreshold   = prefs.getFloat("nightLux",    1.0f);
  cfg.clearCloudThreshold = prefs.getFloat("clearCloud", 20.0f);
  if (cfg.nightLuxThreshold < 0)                              cfg.nightLuxThreshold = 0;
  if (cfg.clearCloudThreshold < 0 || cfg.clearCloudThreshold > 100) cfg.clearCloudThreshold = 20.0f;

  prefs.end();
}

void configSave(const DeviceConfig &cfg)
{
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, /*readOnly=*/false);

  prefs.putFloat ("sqmOffset",   cfg.sqmOffset);
  prefs.putFloat ("sqmRef",      cfg.sqmReference);
  prefs.putFloat ("cldClear",    cfg.cloudClearDelta);
  prefs.putFloat ("cldOvercast", cfg.cloudOvercastDelta);
  prefs.putUShort("snapSec",     cfg.snapshotIntervalSec);
  prefs.putUChar ("jpegQuality", cfg.jpegQuality);
  prefs.putUChar ("tslInteg",    cfg.tsl2591Integration);
  prefs.putDouble("avgPeriod",   cfg.averagePeriod);
  prefs.putString("location",    cfg.location);
  prefs.putString("ntpServer",   cfg.ntpServer);
  prefs.putBool  ("mqttEn",       cfg.mqttEnabled);
  prefs.putString("mqttServer",   cfg.mqttServer);
  prefs.putUShort("mqttPort",     cfg.mqttPort);
  prefs.putString("mqttUser",     cfg.mqttUser);
  prefs.putString("mqttPass",     cfg.mqttPassword);
  prefs.putString("mqttTopicPfx", cfg.mqttTopicPrefix);
  prefs.putUChar ("cloudMethod", cfg.cloudCoverMethod);
  prefs.putUChar ("cloudPxRgn",  cfg.cloudPixelRegion);
  prefs.putUChar ("cloudEdge",   cfg.cloudEdgeExclude);
  prefs.putUChar ("rainMode",    cfg.rainMode);
  prefs.putBool  ("rainEn",      cfg.rainEnabled);
  prefs.putUChar ("dhtType",     cfg.dhtType);
  prefs.putUChar ("bmp280Addr",  cfg.bmp280Addr);
  prefs.putBool  ("safetyBcastEn",   cfg.safetyBcastEnabled);
  prefs.putUShort("safetyBcastPort", cfg.safetyBcastPort);
  prefs.putUShort("safetyBcastSec",  cfg.safetyBcastIntervalSec);
  prefs.putFloat ("nightLux",    cfg.nightLuxThreshold);
  prefs.putFloat ("clearCloud",  cfg.clearCloudThreshold);

  prefs.end();
}
