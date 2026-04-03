/*
 * Ambient temperature sensor: DHT11, DHT22, BMP180, or BMP280.
 * Sensor type is selected at runtime from deviceConfig.dhtType (0=off,1=DHT11,2=DHT22,3=BMP180,4=BMP280).
 * Valid data is available via dhtData; consumers are sensor-type agnostic.
 */

#include "dht_sensor.h"
#include "config.h"
#include "config_store.h"
#include "debug.h"

// ── DHT (types 1 & 2) ────────────────────────────────────────────────────────
#if DHT_DATA_PIN >= 0
#include <DHT.h>
static DHT *dhtObj = nullptr;
#endif

// ── BMP180 (type 3) ──────────────────────────────────────────────────────────
#include <Adafruit_BMP085_U.h>
#include <Adafruit_Sensor.h>
static Adafruit_BMP085_Unified bmp180(10085);  // arbitrary sensor ID
static bool bmp180Ready = false;

// ── BMP280 (type 4) ──────────────────────────────────────────────────────────
#include <Adafruit_BMP280.h>
static Adafruit_BMP280 bmp280;
static bool bmp280Ready = false;

// ── BME280 (type 5) ──────────────────────────────────────────────────────────
#include <Adafruit_BME280.h>
static Adafruit_BME280 bme280;
static bool bme280Ready = false;

DhtData dhtData = {};

void dhtSetup()
{
  if (deviceConfig.dhtType == 3) {
    // BMP180 – I2C, shares the existing bus
    if (bmp180.begin()) {
      bmp180Ready = true;
      Debug.printf("[BMP180] found at 0x%02X – interval %d ms\n",
                   BMP180_I2C_ADDR, DHT_INTERVAL_MS);
    } else {
      Debug.println("[BMP180] not found – check wiring (SDA/SCL)");
    }
    return;
  }

  if (deviceConfig.dhtType == 4) {
    // BMP280 – I2C, shares the existing bus; addr set at runtime (0x76 SDO→GND, 0x77 SDO→VCC)
    if (bmp280.begin(deviceConfig.bmp280Addr)) {
      bmp280Ready = true;
      Debug.printf("[BMP280] found at 0x%02X – interval %d ms\n",
                   deviceConfig.bmp280Addr, DHT_INTERVAL_MS);
    } else {
      Debug.printf("[BMP280] not found at 0x%02X – check wiring / SDO pin\n",
                   deviceConfig.bmp280Addr);
    }
    return;
  }

  if (deviceConfig.dhtType == 5) {
    // BME280 – I2C, shares the existing bus; same address options as BMP280.
    // Explicit setSampling() is required: ctrl_hum must be written before
    // ctrl_meas, and some Adafruit library versions leave humidity oversampling
    // unset when relying solely on begin()'s internal defaults.
    if (bme280.begin(deviceConfig.bmp280Addr)) {
      bme280.setSampling(Adafruit_BME280::MODE_NORMAL,
                         Adafruit_BME280::SAMPLING_X2,   // temperature
                         Adafruit_BME280::SAMPLING_X16,  // pressure
                         Adafruit_BME280::SAMPLING_X1,   // humidity
                         Adafruit_BME280::FILTER_X16,
                         Adafruit_BME280::STANDBY_MS_0_5);
      bme280Ready = true;
      Debug.printf("[BME280] found at 0x%02X – interval %d ms\n",
                   deviceConfig.bmp280Addr, DHT_INTERVAL_MS);
    } else {
      Debug.printf("[BME280] not found at 0x%02X – check wiring / SDO pin\n",
                   deviceConfig.bmp280Addr);
    }
    return;
  }

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
  static unsigned long lastRead = 0;
  if (millis() - lastRead < DHT_INTERVAL_MS) return;
  lastRead = millis();

  // ── BMP180 path ──────────────────────────────────────────────────────────
  if (deviceConfig.dhtType == 3) {
    if (!bmp180Ready) return;
    sensors_event_t event;
    bmp180.getEvent(&event);
    float t;
    bmp180.getTemperature(&t);
    float p = event.pressure;  // already in hPa

    if (!isnan(t) && p > 0.0f) {
      bool changed = (!dhtData.valid
                      || fabsf(t - dhtData.temperature) >= 0.1f
                      || fabsf(p - dhtData.pressure)    >= 0.5f);
      dhtData.temperature = t;
      dhtData.pressure    = p;
      dhtData.humidity    = 0.0f;
      dhtData.valid       = true;
      if (changed)
        Debug.printf("[BMP180] T=%.1f°C  P=%.1f hPa\n", t, p);
    } else {
      if (dhtData.valid) Debug.println("[BMP180] read failed");
      dhtData.valid = false;
    }
    return;
  }

  // ── BMP280 path ──────────────────────────────────────────────────────────
  if (deviceConfig.dhtType == 4) {
    if (!bmp280Ready) return;
    float t = bmp280.readTemperature();
    float p = bmp280.readPressure() / 100.0f;  // Pa → hPa

    if (!isnan(t) && p > 0.0f) {
      bool changed = (!dhtData.valid
                      || fabsf(t - dhtData.temperature) >= 0.1f
                      || fabsf(p - dhtData.pressure)    >= 0.5f);
      dhtData.temperature = t;
      dhtData.pressure    = p;
      dhtData.humidity    = 0.0f;
      dhtData.valid       = true;
      if (changed)
        Debug.printf("[BMP280] T=%.1f°C  P=%.1f hPa\n", t, p);
    } else {
      if (dhtData.valid) Debug.println("[BMP280] read failed");
      dhtData.valid = false;
    }
    return;
  }

  // ── BME280 path ──────────────────────────────────────────────────────────
  if (deviceConfig.dhtType == 5) {
    if (!bme280Ready) return;
    float t = bme280.readTemperature();
    float p = bme280.readPressure() / 100.0f;  // Pa → hPa
    float h = bme280.readHumidity();

    if (!isnan(t) && p > 0.0f && !isnan(h)) {
      bool changed = (!dhtData.valid
                      || fabsf(t - dhtData.temperature) >= 0.1f
                      || fabsf(p - dhtData.pressure)    >= 0.5f
                      || fabsf(h - dhtData.humidity)    >= 1.0f);
      dhtData.temperature = t;
      dhtData.pressure    = p;
      dhtData.humidity    = h;
      dhtData.valid       = true;
      if (changed)
        Debug.printf("[BME280] T=%.1f°C  P=%.1f hPa  RH=%.0f%%\n", t, p, h);
    } else {
      if (dhtData.valid) Debug.println("[BME280] read failed");
      dhtData.valid = false;
    }
    return;
  }

  // ── DHT path ─────────────────────────────────────────────────────────────
#if DHT_DATA_PIN >= 0
  if (!dhtObj) return;

  float t = dhtObj->readTemperature();
  float h = dhtObj->readHumidity();

  if (!isnan(t) && !isnan(h)) {
    bool changed = (!dhtData.valid
                    || fabsf(t - dhtData.temperature) >= 0.5f
                    || fabsf(h - dhtData.humidity)     >= 1.0f);
    dhtData.temperature = t;
    dhtData.humidity    = h;
    dhtData.pressure    = 0.0f;
    dhtData.valid       = true;
    if (changed)
      Debug.printf("[DHT] T=%.1f°C  RH=%.0f%%\n", t, h);
  } else {
    if (dhtData.valid) Debug.println("[DHT] read failed");
    dhtData.valid = false;
  }
#endif
}
