/*
 * DHT ambient temperature / humidity sensor
 * Reads DHT_DATA_PIN at DHT_INTERVAL_MS; valid data available via dhtData.
 * Sensor type (DHT11 / DHT22) is selected at runtime from deviceConfig.dhtType.
 */

#include "dht_sensor.h"
#include "config.h"
#include "config_store.h"
#include "debug.h"

#if DHT_DATA_PIN >= 0
#include <DHT.h>
static DHT *dhtObj = nullptr;
#endif

DhtData dhtData = {};

void dhtSetup()
{
#if DHT_DATA_PIN >= 0
  uint8_t type = (deviceConfig.dhtType == 2) ? DHT22 : DHT11;
  dhtObj = new DHT(DHT_DATA_PIN, type);
  dhtObj->begin();
  const char *typeName = (type == DHT22) ? "DHT22" : "DHT11";
  Debug.printf("[DHT] %s on GPIO%d – interval %d ms\n",
               typeName, DHT_DATA_PIN, DHT_INTERVAL_MS);
#else
  Debug.println("[DHT] no pin defined for this board – sensor disabled");
#endif
}

void dhtLoop()
{
#if DHT_DATA_PIN >= 0
  if (!dhtObj) return;

  static unsigned long lastRead = 0;
  if (millis() - lastRead < DHT_INTERVAL_MS) return;
  lastRead = millis();

  float t = dhtObj->readTemperature();
  float h = dhtObj->readHumidity();

  if (!isnan(t) && !isnan(h)) {
    bool changed = (!dhtData.valid
                    || fabsf(t - dhtData.temperature) >= 0.5f
                    || fabsf(h - dhtData.humidity)     >= 1.0f);
    dhtData.temperature = t;
    dhtData.humidity    = h;
    dhtData.valid       = true;
    if (changed)
      Debug.printf("[DHT] T=%.1f°C  RH=%.0f%%\n", t, h);
  } else {
    if (dhtData.valid) Debug.println("[DHT] read failed");
    dhtData.valid = false;
  }
#endif
}
