/*
 * sky_history.cpp
 *
 * Persistent hourly sky-conditions ring buffers, 90 days deep, backing the
 * /rainhistory calendar page. See sky_history.h for the storage model.
 */

#include "sky_history.h"
#include "config.h"
#include "config_store.h"
#include "rain_sensor.h"
#include "../debug/debug.h"
#include <Preferences.h>
#include <time.h>
#include <limits.h>

// ---------------------------------------------------------------------------
// Ring buffers – one slot per epoch-hour, indexed mod RAIN_HIST_RING_HOURS.
// ---------------------------------------------------------------------------

#define SQM_NO_DATA  ((int16_t)INT16_MIN)   // sentinel: no night SQM sample this hour

static uint16_t wetSecRing[RAIN_HIST_RING_HOURS];         // rain/snow wet-seconds (0..3600)
static uint16_t clearDaySecRing[RAIN_HIST_RING_HOURS];    // clear-sky seconds while "day" (0..3600)
static uint16_t clearNightSecRing[RAIN_HIST_RING_HOURS];  // clear-sky seconds while "night" (0..3600)
static int16_t  sqmAvgRing[RAIN_HIST_RING_HOURS];         // night SQM average * 100, or SQM_NO_DATA
static int16_t  sqmPeakRing[RAIN_HIST_RING_HOURS];        // night SQM peak (darkest) * 100, or SQM_NO_DATA

static uint32_t currentHour       = 0;   // epoch-hour currently accumulating; 0 = not yet initialised
static uint32_t lastCommittedHour = 0;   // most recent hour written to the rings; 0 = never (fresh device)
static bool     nvsDirty          = false;
static unsigned long lastFlushMs  = 0;

// Current-hour RAM accumulators, committed to the rings on rollover.
static uint32_t currentHourWetSec        = 0;
static uint32_t currentHourClearDaySec   = 0;
static uint32_t currentHourClearNightSec = 0;
static float    currentHourSqmSum        = 0.0f;
static uint16_t currentHourSqmCount      = 0;
static int16_t  currentHourSqmPeakScaled = SQM_NO_DATA;

// Independent elapsed-time trackers – rain advances on every loop() tick,
// cloud cover on its own (~5 s) thermal-read cadence, so each needs its own
// "time of last sample" to compute a correct delta.
static time_t lastRainSampleEpoch  = 0;
static time_t lastCloudSampleEpoch = 0;

// ---------------------------------------------------------------------------
// Fixed-point SQM helper (mag/arcsec² * 100, clamped to a sane range)
// ---------------------------------------------------------------------------

static int16_t sqmToFixed(float sqm)
{
  float v = sqm * 100.0f;
  if (v < -3000.0f) v = -3000.0f;   // -30.00 .. 30.00 mag/arcsec² is generous headroom
  if (v >  3000.0f) v =  3000.0f;
  return (int16_t)(v >= 0 ? v + 0.5f : v - 0.5f);
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

static void loadFromNVS()
{
  Preferences prefs;
  prefs.begin(RAIN_HIST_NAMESPACE, /*readOnly=*/true);

  if (prefs.getBytes("wetSec", wetSecRing, sizeof(wetSecRing)) != sizeof(wetSecRing))
    memset(wetSecRing, 0, sizeof(wetSecRing));
  if (prefs.getBytes("clearDay", clearDaySecRing, sizeof(clearDaySecRing)) != sizeof(clearDaySecRing))
    memset(clearDaySecRing, 0, sizeof(clearDaySecRing));
  if (prefs.getBytes("clearNight", clearNightSecRing, sizeof(clearNightSecRing)) != sizeof(clearNightSecRing))
    memset(clearNightSecRing, 0, sizeof(clearNightSecRing));
  if (prefs.getBytes("sqmAvg", sqmAvgRing, sizeof(sqmAvgRing)) != sizeof(sqmAvgRing))
    for (uint32_t i = 0; i < RAIN_HIST_RING_HOURS; i++) sqmAvgRing[i] = SQM_NO_DATA;
  if (prefs.getBytes("sqmPeak", sqmPeakRing, sizeof(sqmPeakRing)) != sizeof(sqmPeakRing))
    for (uint32_t i = 0; i < RAIN_HIST_RING_HOURS; i++) sqmPeakRing[i] = SQM_NO_DATA;

  lastCommittedHour = prefs.getUInt("lastHour", 0);
  prefs.end();
}

static void saveToNVS()
{
  Preferences prefs;
  prefs.begin(RAIN_HIST_NAMESPACE, /*readOnly=*/false);
  prefs.putBytes("wetSec",     wetSecRing,        sizeof(wetSecRing));
  prefs.putBytes("clearDay",   clearDaySecRing,   sizeof(clearDaySecRing));
  prefs.putBytes("clearNight", clearNightSecRing, sizeof(clearNightSecRing));
  prefs.putBytes("sqmAvg",     sqmAvgRing,        sizeof(sqmAvgRing));
  prefs.putBytes("sqmPeak",    sqmPeakRing,       sizeof(sqmPeakRing));
  prefs.putUInt ("lastHour",   lastCommittedHour);
  prefs.end();
}

// ---------------------------------------------------------------------------
// Hour-clock helpers
// ---------------------------------------------------------------------------

static void resetCurrentHourAccumulators()
{
  currentHourWetSec        = 0;
  currentHourClearDaySec   = 0;
  currentHourClearNightSec = 0;
  currentHourSqmSum        = 0.0f;
  currentHourSqmCount      = 0;
  currentHourSqmPeakScaled = SQM_NO_DATA;
}

static void commitHour(uint32_t hour)
{
  uint32_t idx = hour % RAIN_HIST_RING_HOURS;
  wetSecRing[idx]        = (uint16_t)min(currentHourWetSec,        3600UL);
  clearDaySecRing[idx]   = (uint16_t)min(currentHourClearDaySec,   3600UL);
  clearNightSecRing[idx] = (uint16_t)min(currentHourClearNightSec, 3600UL);
  if (currentHourSqmCount > 0) {
    sqmAvgRing[idx]  = sqmToFixed(currentHourSqmSum / currentHourSqmCount);
    sqmPeakRing[idx] = currentHourSqmPeakScaled;
  } else {
    sqmAvgRing[idx]  = SQM_NO_DATA;
    sqmPeakRing[idx] = SQM_NO_DATA;
  }
}

// Zero/sentinel-fill hours strictly between fromHour and toHour (exclusive
// both ends), bounded to the ring size – a gap longer than the ring
// overwrites everything on its own as we wrap, so there is no need to
// iterate further than that.
static void zeroFillGap(uint32_t fromHour, uint32_t toHour)
{
  uint32_t gap = toHour - fromHour - 1;
  if (gap > RAIN_HIST_RING_HOURS) gap = RAIN_HIST_RING_HOURS;
  uint32_t start = toHour - gap;
  for (uint32_t h = start; h < toHour; h++) {
    uint32_t idx = h % RAIN_HIST_RING_HOURS;
    wetSecRing[idx]        = 0;
    clearDaySecRing[idx]   = 0;
    clearNightSecRing[idx] = 0;
    sqmAvgRing[idx]        = SQM_NO_DATA;
    sqmPeakRing[idx]       = SQM_NO_DATA;
  }
  if (gap > 0) Debug.printf("[SkyHist] Gap detected: zero-filled %u hour(s)\n", (unsigned)gap);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void skyHistorySetup()
{
  loadFromNVS();
  currentHour          = 0;
  lastRainSampleEpoch  = 0;
  lastCloudSampleEpoch = 0;
  resetCurrentHourAccumulators();
  Debug.printf("[SkyHist] Loaded – lastCommittedHour=%u\n", (unsigned)lastCommittedHour);
}

void skyHistoryLoop()
{
  time_t now = time(nullptr);
  if (now < (time_t)RAIN_HIST_MIN_VALID_EPOCH) return;  // clock not synced yet

  uint32_t nowHour = (uint32_t)(now / 3600);

  if (currentHour == 0) {
    // First valid-clock tick since boot – start accumulating without a
    // spurious first delta, and gap-fill for any hours missed while off.
    currentHour         = nowHour;
    lastRainSampleEpoch = now;
    if (lastCommittedHour != 0 && nowHour > lastCommittedHour + 1) {
      zeroFillGap(lastCommittedHour, nowHour);
    }
    return;
  }

  // Rain wet-time – only meaningful while the rain sensor is enabled, but
  // the hour clock itself (below) always runs so SQM/clear-sky tracking
  // works even with the rain sensor disabled or absent.
  time_t delta = now - lastRainSampleEpoch;
  lastRainSampleEpoch = now;
  if (deviceConfig.rainEnabled && delta > 0 && delta < 3600 && rainIsWet()) {
    currentHourWetSec += (uint32_t)delta;
  }

  if (nowHour != currentHour) {
    commitHour(currentHour);
    lastCommittedHour = currentHour;
    nvsDirty = true;

    if (nowHour > currentHour + 1) zeroFillGap(currentHour, nowHour);

    currentHour = nowHour;
    resetCurrentHourAccumulators();
  }

  // Debounce the NVS write slightly in case several hours roll over back to
  // back (e.g. after a long sleep) so we don't hammer flash in a tight loop.
  if (nvsDirty && millis() - lastFlushMs > 5000) {
    saveToNVS();
    nvsDirty    = false;
    lastFlushMs = millis();
  }
}

void skyHistoryAccumulateCloud(float cloudCoverPct, float lux)
{
  if (currentHour == 0) return;  // clock not synced yet

  time_t now = time(nullptr);
  if (lastCloudSampleEpoch == 0) { lastCloudSampleEpoch = now; return; }
  time_t delta = now - lastCloudSampleEpoch;
  lastCloudSampleEpoch = now;
  if (delta <= 0 || delta >= 3600) return;  // clock jump / first-call guard

  if (cloudCoverPct >= deviceConfig.clearCloudThreshold) return;  // not clear

  if (lux < deviceConfig.nightLuxThreshold) currentHourClearNightSec += (uint32_t)delta;
  else                                      currentHourClearDaySec  += (uint32_t)delta;
}

void skyHistoryAccumulateSqm(float sqm, float lux)
{
  if (currentHour == 0) return;              // clock not synced yet
  if (lux >= deviceConfig.nightLuxThreshold) return;  // daytime – SQM not meaningful

  currentHourSqmSum += sqm;
  currentHourSqmCount++;

  int16_t scaled = sqmToFixed(sqm);
  if (currentHourSqmPeakScaled == SQM_NO_DATA || scaled > currentHourSqmPeakScaled)
    currentHourSqmPeakScaled = scaled;
}

// ---------------------------------------------------------------------------
// JSON streaming
// ---------------------------------------------------------------------------

// Batches output into 512-byte chunks flushed via WebServer::sendContent(),
// avoiding one TCP write per value for the up-to-2160-entry arrays.
struct RStream {
  WebServer& srv;
  char  buf[512];
  int   pos;

  RStream(WebServer& s) : srv(s), pos(0) {}
  ~RStream() { flush(); }

  void flush() {
    if (pos > 0) { buf[pos] = '\0'; srv.sendContent(buf); pos = 0; }
  }
  void add(const char* s) {
    while (*s) { if (pos >= 500) flush(); buf[pos++] = *s++; }
  }
};

// Live values for the in-progress current hour, mirroring commitHour()'s logic
// without touching the ring (used only when streaming hour == currentHour).
static void liveCurrentHourValues(uint32_t &rainSec, uint32_t &clearDay, uint32_t &clearNight,
                                   int16_t &sqmAvg, int16_t &sqmPeak)
{
  rainSec    = min(currentHourWetSec,        3600UL);
  clearDay   = min(currentHourClearDaySec,   3600UL);
  clearNight = min(currentHourClearNightSec, 3600UL);
  if (currentHourSqmCount > 0) {
    sqmAvg  = sqmToFixed(currentHourSqmSum / currentHourSqmCount);
    sqmPeak = currentHourSqmPeakScaled;
  } else {
    sqmAvg = sqmPeak = SQM_NO_DATA;
  }
}

static void addU32Array(RStream &rs, const char *key, uint32_t t0Hour, uint32_t wantHours,
                         const uint16_t *ring, bool first)
{
  rs.add(first ? "\"" : ",\"");
  rs.add(key);
  rs.add("\":[");
  for (uint32_t i = 0; i < wantHours; i++) {
    uint32_t hour = t0Hour + i;
    uint32_t v    = ring[hour % RAIN_HIST_RING_HOURS];
    if (hour == currentHour) {
      uint32_t rainSec, clearDay, clearNight; int16_t sqmAvg, sqmPeak;
      liveCurrentHourValues(rainSec, clearDay, clearNight, sqmAvg, sqmPeak);
      v = (ring == wetSecRing) ? rainSec : (ring == clearDaySecRing) ? clearDay : clearNight;
    }
    char num[12];
    snprintf(num, sizeof(num), i == 0 ? "%lu" : ",%lu", (unsigned long)v);
    rs.add(num);
  }
  rs.add("]");
}

static void addSqmArray(RStream &rs, const char *key, uint32_t t0Hour, uint32_t wantHours,
                         const int16_t *ring)
{
  rs.add(",\"");
  rs.add(key);
  rs.add("\":[");
  for (uint32_t i = 0; i < wantHours; i++) {
    uint32_t hour = t0Hour + i;
    int16_t  v    = ring[hour % RAIN_HIST_RING_HOURS];
    if (hour == currentHour) {
      uint32_t rainSec, clearDay, clearNight; int16_t sqmAvg, sqmPeak;
      liveCurrentHourValues(rainSec, clearDay, clearNight, sqmAvg, sqmPeak);
      v = (ring == sqmAvgRing) ? sqmAvg : sqmPeak;
    }
    char num[16];
    if (v == SQM_NO_DATA) snprintf(num, sizeof(num), i == 0 ? "null" : ",null");
    else                  snprintf(num, sizeof(num), i == 0 ? "%.2f" : ",%.2f", v / 100.0f);
    rs.add(num);
  }
  rs.add("]");
}

void skyHistoryStreamJSON(WebServer& server, int days)
{
  if (days < 1) days = 1;
  if (days > RAIN_HIST_DAYS) days = RAIN_HIST_DAYS;

  time_t now = time(nullptr);
  bool synced = (now >= (time_t)RAIN_HIST_MIN_VALID_EPOCH);

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  if (!synced) {
    server.sendContent("{\"synced\":false,\"count\":0,\"rainSec\":[],\"clearDaySec\":[],"
                        "\"clearNightSec\":[],\"sqmAvg\":[],\"sqmPeak\":[]}");
    return;
  }

  uint32_t nowHour   = (uint32_t)(now / 3600);
  uint32_t wantHours = (uint32_t)days * 24;
  if (wantHours > RAIN_HIST_RING_HOURS) wantHours = RAIN_HIST_RING_HOURS;
  uint32_t t0Hour    = nowHour - wantHours + 1;

  RStream rs(server);

  char head[128];
  snprintf(head, sizeof(head),
    "{\"synced\":true,\"nowEpoch\":%lu,\"res\":3600,\"t0Epoch\":%lu,\"count\":%lu,",
    (unsigned long)now, (unsigned long)(t0Hour * 3600UL), (unsigned long)wantHours);
  rs.add(head);

  addU32Array(rs, "rainSec",       t0Hour, wantHours, wetSecRing,        true);
  addU32Array(rs, "clearDaySec",   t0Hour, wantHours, clearDaySecRing,   false);
  addU32Array(rs, "clearNightSec", t0Hour, wantHours, clearNightSecRing, false);
  addSqmArray(rs, "sqmAvg",  t0Hour, wantHours, sqmAvgRing);
  addSqmArray(rs, "sqmPeak", t0Hour, wantHours, sqmPeakRing);

  rs.add("}");
}
