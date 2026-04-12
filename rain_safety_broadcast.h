#ifndef RAIN_SAFETY_BROADCAST_H
#define RAIN_SAFETY_BROADCAST_H

/*
 * DDA-Compatible Safety Sensor UDP Broadcast
 *
 * Periodically broadcasts a JSON safety-status packet on the local subnet,
 * compatible with the Dark Dragon Astronomy park-sensor UDP protocol.
 * Other devices (e.g. a Roll-Off Roof controller) can listen on the
 * configured port to discover this device and react to its safety state.
 *
 * Packet format:
 *   {
 *     "serialNumber": "<mac-hex>",      // stable, unique per device
 *     "deviceType":   "SafetySensor",
 *     "name":         "<location> Safety Sensor",
 *     "isSafe":       true|false,       // true = dry (safe), false = wet (unsafe)
 *     "rainState":    "dry"|"wet",
 *     "ambTemp":      <float>|null      // °C; null only before first MLX frame
 *   }
 *
 * Broadcast triggers:
 *   - Immediately on rain-sensor state change (dry↔wet)
 *   - Periodically at deviceConfig.safetyBcastIntervalSec (default 30 s)
 *
 * All three settings (enabled, port, interval) are runtime-configurable via
 * the Setup page and take effect immediately – no reboot required.
 */

void rainSafetyBroadcastSetup();   // call once in setup() after WiFi connects
void rainSafetyBroadcastLoop();    // call every loop() iteration

#endif // RAIN_SAFETY_BROADCAST_H
