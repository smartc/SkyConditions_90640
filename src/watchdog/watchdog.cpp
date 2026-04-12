/*
 * watchdog.cpp
 *
 * Periodic health monitor.  Call watchdogSetup() once in setup() and
 * watchdogLoop() on every iteration of loop().
 *
 * Health check interval: 60 seconds.
 *
 * Heap thresholds (internal heap only – PSRAM is not used for critical allocs):
 *   < HEAP_WARN_THRESHOLD    → log warning; continue operating
 *   < HEAP_RESTART_THRESHOLD → log critical message then ESP.restart()
 *
 * The millis() counter wraps at ~49.7 days.  An optional compile-time
 * WATCHDOG_MAX_UPTIME_DAYS guard (disabled by default) can force a clean
 * restart before wrap-around to prevent any timing drift in long deployments.
 */

#include "watchdog.h"
#include "../webserver/web_ui_handler.h"
#include "../debug/debug.h"
#include <Arduino.h>

// ── Tunable constants ─────────────────────────────────────────────────────────

#define WATCHDOG_INTERVAL_MS     60000UL   // health report cadence
#define HEAP_WARN_THRESHOLD      30000U    // warn if internal free heap < 30 KB
#define HEAP_RESTART_THRESHOLD   15000U    // restart if internal free heap < 15 KB

// Uncomment to restart automatically after this many days (defense against
// millis() wrap-around and very-long-uptime heap creep).
// #define WATCHDOG_MAX_UPTIME_DAYS  30

// ── Module state ──────────────────────────────────────────────────────────────

static unsigned long lastCheckMs  = 0;
static uint32_t      minFreeHeap  = UINT32_MAX;   // running low-water mark
static uint8_t       warnStreak   = 0;            // consecutive sub-threshold checks

// ── Public API ────────────────────────────────────────────────────────────────

void watchdogSetup()
{
  minFreeHeap = ESP.getFreeHeap();
  Debug.printf("[WDG] init – heap=%u B  psram=%u B\n",
               ESP.getFreeHeap(), ESP.getFreePsram());
}

void watchdogLoop()
{
  unsigned long now = millis();
  if (now - lastCheckMs < WATCHDOG_INTERVAL_MS) return;
  lastCheckMs = now;

  const uint32_t freeHeap   = ESP.getFreeHeap();
  const uint32_t freePsram  = ESP.getFreePsram();
  const uint32_t upSec      = now / 1000;
  const uint8_t  wsClients  = getWsClientCount();

  if (freeHeap < minFreeHeap) minFreeHeap = freeHeap;

  // ── Periodic health report (always) ──────────────────────────────────────
  Debug.printf("[WDG] up=%lus  heap=%u B (min=%u B)  psram=%u B  ws=%u\n",
               (unsigned long)upSec, freeHeap, minFreeHeap, freePsram, wsClients);

  // ── Heap health checks ────────────────────────────────────────────────────
  if (freeHeap < HEAP_RESTART_THRESHOLD) {
    Debug.printf("[WDG] CRITICAL: heap %u B < %u B – restarting now\n",
                 freeHeap, HEAP_RESTART_THRESHOLD);
    delay(300);   // let the log message flush to serial
    ESP.restart();
  }

  if (freeHeap < HEAP_WARN_THRESHOLD) {
    warnStreak++;
    Debug.printf("[WDG] WARNING: heap low (%u B), streak=%u\n", freeHeap, warnStreak);
  } else {
    warnStreak = 0;
  }

  // ── WebSocket connection health ───────────────────────────────────────────
  // More than 4 simultaneous WebSocket clients is unusual for a single-user
  // device and may indicate zombie connections accumulating faster than the
  // heartbeat can clean them.  Log a notice so it's visible in the console.
  if (wsClients > 4) {
    Debug.printf("[WDG] NOTICE: %u WebSocket clients – possible zombie connections\n",
                 wsClients);
  }

#ifdef WATCHDOG_MAX_UPTIME_DAYS
  const uint32_t maxUptimeSec = (uint32_t)WATCHDOG_MAX_UPTIME_DAYS * 86400UL;
  if (upSec >= maxUptimeSec) {
    Debug.printf("[WDG] max uptime %d days reached – restarting for housekeeping\n",
                 WATCHDOG_MAX_UPTIME_DAYS);
    delay(300);
    ESP.restart();
  }
#endif
}
