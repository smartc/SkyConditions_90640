#include "config_store.h"

DeviceConfig deviceConfig;

void configLoad(DeviceConfig &cfg)
{
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, /*readOnly=*/true);
  cfg.sqmOffset          = prefs.getFloat("sqmOffset",   0.0f);
  cfg.cloudClearDelta    = prefs.getFloat("cldClear",   20.0f);
  cfg.cloudOvercastDelta = prefs.getFloat("cldOvercast",  5.0f);
  prefs.end();
}

void configSave(const DeviceConfig &cfg)
{
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, /*readOnly=*/false);
  prefs.putFloat("sqmOffset",   cfg.sqmOffset);
  prefs.putFloat("cldClear",    cfg.cloudClearDelta);
  prefs.putFloat("cldOvercast", cfg.cloudOvercastDelta);
  prefs.end();
}
