#ifndef RAIN_HISTORY_H
#define RAIN_HISTORY_H

#include <Arduino.h>
#include <WebServer.h>

/*
 * rain_history.h
 *
 * Persistent per-hour wet-time log covering the last RAIN_HIST_DAYS (90) days,
 * used to render a month-view calendar of accumulated rain/snow duration
 * (see /rainhistory page). Unlike history.h's RAM-only ring buffers, this
 * survives reboot: the ring is written to NVS on every hour rollover.
 *
 * Each bucket holds seconds-wet (0..3600) for one calendar hour (UTC), keyed
 * by epoch-hour (unix time / 3600) modulo RAIN_HIST_RING_HOURS. The frontend
 * re-buckets these hourly UTC samples into the browser's local calendar days.
 *
 * A gap in operation (device powered off, or clock not yet NTP-synced) is
 * recorded as zero seconds for the missed hours, not as "unknown" – this
 * matches the accumulated-duration nature of the log (no reading = no rain
 * recorded) and keeps the storage format simple.
 */

void rainHistorySetup();   // call once in setup(), after configLoad()
void rainHistoryLoop();    // call every loop() iteration when the rain sensor is enabled

// Stream JSON to the client: { "synced":bool, "nowEpoch":u32, "res":3600,
//                              "t0Epoch":u32, "count":N, "hours":[...] }
// "hours" holds `count` seconds-wet values (0..3600), oldest first, one per
// hour ending at nowEpoch. days is clamped to [1, RAIN_HIST_DAYS].
void rainHistoryStreamJSON(WebServer& server, int days);

#endif // RAIN_HISTORY_H
