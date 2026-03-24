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

  prefs.end();
}
