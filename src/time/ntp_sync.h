#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include <Arduino.h>

/*
 * NTP (SNTP) clock sync, configured from deviceConfig:
 *   ntpEnabled               - false = SNTP stopped; the clock keeps whatever time it has
 *   ntpServer / 2 / 3        - up to three sources, tried in order; blanks are skipped
 *                              and all-blank falls back to NTP_DEFAULT_SERVER
 *
 * The system clock always runs in UTC (Alpaca, Sky History and MQTT rely on
 * it).  deviceConfig.utcOffsetMin only changes how the Web UI displays time.
 */

#define NTP_DEFAULT_SERVER  "time.google.com"

void ntpApply();                 // (re)start or stop SNTP from deviceConfig; call after WiFi is up
unsigned long ntpLastSyncMs();   // millis() of the last successful sync, 0 = none since boot

#endif // NTP_SYNC_H
