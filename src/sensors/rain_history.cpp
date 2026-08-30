/*
 * rain_history.cpp
 *
 * Persistent hourly wet-time ring buffer, 90 days deep, backing the
 * /rainhistory calendar page. See rain_history.h for the storage model.
 */

#include "rain_history.h"
#include "config.h"
#include "config_store.h"
#include "rain_sensor.h"
#include "../debug/debug.h"
#include <Preferences.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Ring buffer – seconds wet (0..3600) per epoch-hour, indexed mod RAIN_HIST_RING_HOURS.
// ---------------------------------------------------------------------------

static uint16_t wetSecRing[RAIN_HIST_RING_HOURS];

static uint32_t currentHour       = 0;   // epoch-hour currently accumulating; 0 = not yet initialised
static uint32_t lastCommittedHour = 0;   // most recent hour written to the ring; 0 = never (fresh device)
static uint32_t currentHourWetSec = 0;   // RAM accumulator for currentHour, committed on rollover
static time_t   lastSampleEpoch   = 0;
static bool     nvsDirty          = false;
static unsigned long lastFlushMs  = 0;

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

static void loadFromNVS()
{
  Preferences prefs;
  prefs.begin(RAIN_HIST_NAMESPACE, /*readOnly=*/true);
  size_t got = prefs.getBytes("wetSec", wetSecRing, sizeof(wetSecRing));
  if (got != sizeof(wetSecRing)) memset(wetSecRing, 0, sizeof(wetSecRing));
  lastCommittedHour = prefs.getUInt("lastHour", 0);
  prefs.end();
}

static void saveToNVS()
{
  Preferences prefs;
  prefs.begin(RAIN_HIST_NAMESPACE, /*readOnly=*/false);
  prefs.putBytes("wetSec", wetSecRing, sizeof(wetSecRing));
  prefs.putUInt("lastHour", lastCommittedHour);
  prefs.end();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static inline void commitHour(uint32_t hour, uint16_t wetSec)
{
  wetSecRing[hour % RAIN_HIST_RING_HOURS] = wetSec;
}

// Zero-fill hours strictly between fromHour and toHour (exclusive both ends),
// bounded to the ring size – a gap longer than the ring overwrites everything
// on its own as we wrap, so there is no need to iterate further than that.
static void zeroFillGap(uint32_t fromHour, uint32_t toHour)
{
  uint32_t gap = toHour - fromHour - 1;
  if (gap > RAIN_HIST_RING_HOURS) gap = RAIN_HIST_RING_HOURS;
  uint32_t start = toHour - gap;
  for (uint32_t h = start; h < toHour; h++) {
    wetSecRing[h % RAIN_HIST_RING_HOURS] = 0;
  }
  if (gap > 0) Debug.printf("[RainHist] Gap detected: zero-filled %u hour(s)\n", (unsigned)gap);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void rainHistorySetup()
{
  loadFromNVS();
  currentHour       = 0;
  currentHourWetSec = 0;
  lastSampleEpoch   = 0;
  Debug.printf("[RainHist] Loaded – lastCommittedHour=%u\n", (unsigned)lastCommittedHour);
}

void rainHistoryLoop()
{
  if (!deviceConfig.rainEnabled) return;

  time_t now = time(nullptr);
  if (now < (time_t)RAIN_HIST_MIN_VALID_EPOCH) return;  // clock not synced yet

  uint32_t nowHour = (uint32_t)(now / 3600);

  if (currentHour == 0) {
    // First valid-clock tick since boot – start accumulating without a
    // spurious first delta, and gap-fill for any hours missed while off.
    currentHour     = nowHour;
    lastSampleEpoch = now;
    if (lastCommittedHour != 0 && nowHour > lastCommittedHour + 1) {
      zeroFillGap(lastCommittedHour, nowHour);
    }
    return;
  }

  time_t delta = now - lastSampleEpoch;
  lastSampleEpoch = now;
  if (delta > 0 && delta < 3600 && rainIsWet()) {
    currentHourWetSec += (uint32_t)delta;
  }

  if (nowHour != currentHour) {
    commitHour(currentHour, (uint16_t)min(currentHourWetSec, 3600UL));
    lastCommittedHour = currentHour;
    nvsDirty = true;

    if (nowHour > currentHour + 1) zeroFillGap(currentHour, nowHour);

    currentHour       = nowHour;
    currentHourWetSec = 0;
  }

  // Debounce the NVS write slightly in case several hours roll over back to
  // back (e.g. after a long sleep) so we don't hammer flash in a tight loop.
  if (nvsDirty && millis() - lastFlushMs > 5000) {
    saveToNVS();
    nvsDirty    = false;
    lastFlushMs = millis();
  }
}

// ---------------------------------------------------------------------------
// JSON streaming
// ---------------------------------------------------------------------------

// Batches output into 512-byte chunks flushed via WebServer::sendContent(),
// avoiding one TCP write per value for the up-to-2160-entry array.
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

void rainHistoryStreamJSON(WebServer& server, int days)
{
  if (days < 1) days = 1;
  if (days > RAIN_HIST_DAYS) days = RAIN_HIST_DAYS;

  time_t now = time(nullptr);
  bool synced = (now >= (time_t)RAIN_HIST_MIN_VALID_EPOCH);

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  if (!synced) {
    server.sendContent("{\"synced\":false,\"count\":0,\"hours\":[]}");
    return;
  }

  uint32_t nowHour   = (uint32_t)(now / 3600);
  uint32_t wantHours = (uint32_t)days * 24;
  if (wantHours > RAIN_HIST_RING_HOURS) wantHours = RAIN_HIST_RING_HOURS;
  uint32_t t0Hour    = nowHour - wantHours + 1;

  RStream rs(server);

  char head[128];
  snprintf(head, sizeof(head),
    "{\"synced\":true,\"nowEpoch\":%lu,\"res\":3600,\"t0Epoch\":%lu,\"count\":%lu,\"hours\":[",
    (unsigned long)now, (unsigned long)(t0Hour * 3600UL), (unsigned long)wantHours);
  rs.add(head);

  for (uint32_t i = 0; i < wantHours; i++) {
    uint32_t hour = t0Hour + i;
    uint32_t sec  = wetSecRing[hour % RAIN_HIST_RING_HOURS];
    if (hour == currentHour) sec = min(currentHourWetSec, 3600UL);  // live partial hour
    char num[12];
    snprintf(num, sizeof(num), i == 0 ? "%lu" : ",%lu", (unsigned long)sec);
    rs.add(num);
  }

  rs.add("]}");
}
