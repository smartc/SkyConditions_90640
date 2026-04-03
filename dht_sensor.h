#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#include <Arduino.h>

/*
 * Optional ambient temperature sensor (DHT or BMP180).
 *
 * Selected at runtime via deviceConfig.dhtType (NVS key "dhtType"):
 *   0 = disabled, 1 = DHT11, 2 = DHT22, 3 = BMP180, 4 = BMP280, 5 = BME280
 * When valid, dhtData.temperature replaces the MLX90640 die temperature
 * as the ambient reference for cloud-cover calculations.
 *
 * DHT11/DHT22 pin is compile-time per board (see config.h):
 *   XIAO Grove Shield D6/D7 connector → data on D7 (GPIO44, UART0 RX – clean input)
 * BMP180/BMP280/BME280 use the shared I2C bus (SDA/SCL) – no extra pin needed.
 *
 * humidity is only populated for DHT11/DHT22 and BME280; others leave it 0.
 * pressure (hPa) is only populated for BMP180/BMP280/BME280; DHT leaves it 0.
 */

struct DhtData {
  float temperature;  // °C – last valid reading
  float humidity;     // %RH – last valid reading (DHT only; 0 for BMP180)
  float pressure;     // hPa – last valid reading (BMP180 only; 0 for DHT)
  bool  valid;        // true = last read succeeded
};

extern DhtData dhtData;

void dhtSetup();   // call once in setup() when dhtType != 0
void dhtLoop();    // call every loop() iteration when dhtType != 0

#endif // DHT_SENSOR_H
