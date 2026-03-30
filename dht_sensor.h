#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#include <Arduino.h>

/*
 * Optional DHT ambient temperature / humidity sensor.
 *
 * Selected at runtime via deviceConfig.dhtType (NVS key "dhtType"):
 *   0 = disabled, 1 = DHT11, 2 = DHT22.
 * When valid, dhtData.temperature replaces the MLX90640 die temperature
 * as the ambient reference for cloud-cover calculations.
 *
 * Pin is compile-time per board (see config.h):
 *   XIAO Grove Shield D6/D7 connector → data on D7 (GPIO44, UART0 RX – clean input)
 */

struct DhtData {
  float temperature;  // °C – last valid reading
  float humidity;     // %RH – last valid reading
  bool  valid;        // true = last read succeeded
};

extern DhtData dhtData;

void dhtSetup();   // call once in setup() when dhtType != 0
void dhtLoop();    // call every loop() iteration when dhtType != 0

#endif // DHT_SENSOR_H
