#ifndef SKY_HISTORY_H
#define SKY_HISTORY_H

#include <Arduino.h>
#include <WebServer.h>

/*
 * sky_history.h
 *
 * Persistent per-hour sky-conditions log covering the last RAIN_HIST_DAYS
 * (90) days, used to render a month-view calendar (see /skyhistory page).
 * Unlike history.h's RAM-only ring buffers, this survives reboot: all rings
 * are written to NVS on every hour rollover.
 *
 * Tracks, per epoch-hour (UTC), keyed by (unix time / 3600) modulo
 * RAIN_HIST_RING_HOURS:
 *   - rain/snow wet-time (seconds)
 *   - clear-sky time, split into day and night seconds (cloud cover below
 *     deviceConfig.clearCloudThreshold)
 *   - night SQM average and peak (mag/arcsec² * 100 fixed-point; only
 *     sampled while lux < deviceConfig.nightLuxThreshold)
 *
 * Day/night classification uses instantaneous lux vs. nightLuxThreshold
 * rather than true sunrise/sunset, so a hint of dawn/dusk twilight can land
 * on either side – acceptable for this calendar's granularity.
 *
 * A gap in operation (device powered off, or clock not yet NTP-synced) is
 * recorded as zero seconds / no SQM sample for the missed hours, not as
 * "unknown" – this matches the accumulated-duration nature of the log
 * (no reading = nothing recorded) and keeps the storage format simple.
 *
 * NVS namespace/keys are unchanged from the original rain-only version
 * (RAIN_HIST_NAMESPACE = "rainHist") so already-accumulated rain data
 * survives this module's broadened scope.
 */

void skyHistorySetup();   // call once in setup(), after configLoad()
void skyHistoryLoop();    // call every loop() iteration – advances the hour clock
                           // and accumulates rain wet-time when the rain sensor is enabled

// Call after each new thermal read (cloud cover available). Accumulates
// clear-sky seconds into the day or night bucket for the current hour,
// based on whether cloudCoverPct is below deviceConfig.clearCloudThreshold
// and lux is below deviceConfig.nightLuxThreshold.
void skyHistoryAccumulateCloud(float cloudCoverPct, float lux);

// Call after each new brightness read (SQM available). Folds sqm into the
// current hour's night average/peak only when lux < nightLuxThreshold;
// a no-op during daytime.
void skyHistoryAccumulateSqm(float sqm, float lux);

// Stream JSON to the client:
// { "synced":bool, "nowEpoch":u32, "res":3600, "t0Epoch":u32, "count":N,
//   "rainSec":[...], "clearDaySec":[...], "clearNightSec":[...],
//   "sqmAvg":[...], "sqmPeak":[...] }
// Each array holds `count` values, oldest first, one per hour ending at
// nowEpoch. sqmAvg/sqmPeak entries are null when no night sample landed in
// that hour. days is clamped to [1, RAIN_HIST_DAYS].
void skyHistoryStreamJSON(WebServer& server, int days);

#endif // SKY_HISTORY_H
