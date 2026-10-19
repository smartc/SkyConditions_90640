#include "ntp_sync.h"
#include "config_store.h"
#include "../debug/debug.h"
#include <esp_sntp.h>

static volatile unsigned long lastSyncMs = 0;

// Runs in the lwIP/SNTP task, so it only records the time.
static void onTimeSync(struct timeval *)
{
  lastSyncMs = millis();
  if (lastSyncMs == 0) lastSyncMs = 1;
}

void ntpApply()
{
  if (!deviceConfig.ntpEnabled) {
    if (esp_sntp_enabled()) esp_sntp_stop();
    Debug.println("NTP sync disabled");
    return;
  }

  // SNTP keeps the name pointers, so they point into deviceConfig (static).
  const char *servers[3] = {nullptr, nullptr, nullptr};
  int n = 0;
  for (const char *s : {deviceConfig.ntpServer, deviceConfig.ntpServer2, deviceConfig.ntpServer3})
    if (s[0]) servers[n++] = s;
  if (n == 0) servers[n++] = NTP_DEFAULT_SERVER;

  sntp_set_time_sync_notification_cb(onTimeSync);
  configTime(0, 0, servers[0], servers[1], servers[2]);
  Debug.printf("NTP sync started (UTC): %s%s%s%s%s\n", servers[0],
               servers[1] ? ", " : "", servers[1] ? servers[1] : "",
               servers[2] ? ", " : "", servers[2] ? servers[2] : "");
}

unsigned long ntpLastSyncMs()
{
  return lastSyncMs;
}
