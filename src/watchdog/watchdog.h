#ifndef WATCHDOG_H
#define WATCHDOG_H

// Periodic health monitor for long-running operation.
//
// Checks every WATCHDOG_INTERVAL_MS:
//   - Free heap / min-ever free heap
//   - Free PSRAM
//   - WebSocket client count (surfacing zombie connections)
//   - Uptime
//
// If free heap drops below HEAP_RESTART_THRESHOLD the device restarts
// cleanly via ESP.restart() before OOM can cause unpredictable failures.
//
// All health reports are written through Debug so they appear in the
// web console at /console as well as on serial.

void watchdogSetup();
void watchdogLoop();

#endif // WATCHDOG_H
