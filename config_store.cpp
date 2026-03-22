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

  prefs.end();
}
