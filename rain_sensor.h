#ifndef RAIN_SENSOR_H
#define RAIN_SENSOR_H

#include <Arduino.h>

/*
 * Rain / Snow Sensor – ZTS-3000-YUX-NO1RO1-H
 *
 * Two independent detection paths:
 *   Relay  – IO38 pulled HIGH; relay contact to GND closes when wet → reads LOW.
 *   Modbus – ZTS-3000 RS485 Modbus RTU via MAX3485ED on IO39/40/41.
 *            Register 0x0000 is queried every RAIN_POLL_MS milliseconds.
 *            A register scan across 0x0000–0x0007 is printed at startup.
 *
 * Both paths operate independently; rainIsWet() returns true if either fires.
 */

struct RainData {
    bool          relayWet;       // true = relay closed (IO38 LOW)
    bool          modbusWet;      // true = register 0x0000 != 0
    bool          modbusOk;       // true = last Modbus transaction succeeded
    unsigned long lastModbusMs;   // millis() of last successful Modbus read
};

extern RainData rainData;

void rainSensorSetup();   // call once in setup(), after Serial is ready
void rainSensorLoop();    // call every loop() iteration

// Returns true if either the relay or Modbus reports precipitation.
bool rainIsWet();

#endif // RAIN_SENSOR_H
