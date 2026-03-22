/*
 * history.cpp
 *
 * Circular ring buffers for time-series data.
 *
 * Hi-res  : 30-second buckets, last 60 minutes  (120 slots)
 * Lo-res  : 15-minute buckets, last 24 hours    ( 96 slots)
 *
 * Both hold avg / lo / hi for 7 metrics:
 *   sky temperature, frame min, frame max, frame median,
 *   ambient temperature, lux, SQM
 */

#include "history.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Ring buffers
// ---------------------------------------------------------------------------

static HistBucket hiRing[HIST_HI_N];
static uint8_t    hiHead  = 0;   // index where NEXT write goes
static uint8_t    hiCount = 0;   // valid entries (0 … HIST_HI_N)

static HistBucket loRing[HIST_LO_N];
static uint8_t    loHead  = 0;
static uint8_t    loCount = 0;

// ---------------------------------------------------------------------------
// Current-bucket accumulator
// ---------------------------------------------------------------------------

static float    accumSum[HM_N];
static float    accumLo [HM_N];
static float    accumHi [HM_N];
static uint16_t accumCnt[HM_N];
static uint32_t bucketStartMs = 0;

// Lo-res running totals (fed from closed hi-res buckets)
static float    loSum[HM_N];
static float    loLo [HM_N];
static float    loHi [HM_N];
static uint16_t loCnt[HM_N];
static uint8_t  loBucketCount = 0;  // hi-res buckets accumulated toward next lo close

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void resetAccum()
{
  for (int i = 0; i < HM_N; i++) {
    accumSum[i] = 0.0f;
    accumLo [i] =  1e10f;
    accumHi [i] = -1e10f;
    accumCnt[i] = 0;
  }
  bucketStartMs = millis();
}

static void resetLoAccum()
{
  for (int i = 0; i < HM_N; i++) {
    loSum[i] = 0.0f;
    loLo [i] =  1e10f;
    loHi [i] = -1e10f;
    loCnt[i] = 0;
  }
  loBucketCount = 0;
}

static void accumulate(int m, float v)
{
  if (m < 0 || m >= HM_N || v == HIST_NO_DATA) return;
  accumSum[m] += v;
  if (v < accumLo[m]) accumLo[m] = v;
  if (v > accumHi[m]) accumHi[m] = v;
  accumCnt[m]++;
}

// ---------------------------------------------------------------------------
// Close the current 30-second bucket
// ---------------------------------------------------------------------------

static void closeBucket()
{
  HistBucket b;
  b.t = millis();

  for (int i = 0; i < HM_N; i++) {
    if (accumCnt[i] > 0) {
      b.avg[i] = accumSum[i] / accumCnt[i];
      b.lo [i] = accumLo[i];
      b.hi [i] = accumHi[i];
    } else {
      b.avg[i] = b.lo[i] = b.hi[i] = HIST_NO_DATA;
    }
  }

  // Push to hi-res ring
  hiRing[hiHead] = b;
  hiHead = (hiHead + 1) % HIST_HI_N;
  if (hiCount < HIST_HI_N) hiCount++;

  // Accumulate into lo-res running totals
  for (int i = 0; i < HM_N; i++) {
    if (b.avg[i] == HIST_NO_DATA) continue;
    loSum[i] += b.avg[i];
    if (b.lo[i] < loLo[i]) loLo[i] = b.lo[i];
    if (b.hi[i] > loHi[i]) loHi[i] = b.hi[i];
    loCnt[i]++;
  }
  loBucketCount++;

  // Close a lo-res bucket every HIST_LO_EVERY hi-res buckets (= 15 min)
  if (loBucketCount >= HIST_LO_EVERY) {
    HistBucket lb;
    lb.t = millis();
    for (int i = 0; i < HM_N; i++) {
      if (loCnt[i] > 0) {
        lb.avg[i] = loSum[i] / loCnt[i];
        lb.lo [i] = loLo[i];
        lb.hi [i] = loHi[i];
      } else {
        lb.avg[i] = lb.lo[i] = lb.hi[i] = HIST_NO_DATA;
      }
    }
    loRing[loHead] = lb;
    loHead = (loHead + 1) % HIST_LO_N;
    if (loCount < HIST_LO_N) loCount++;
    resetLoAccum();
  }

  resetAccum();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void historySetup()
{
  resetAccum();
  resetLoAccum();
}

void historyLoop()
{
  if (millis() - bucketStartMs >= HIST_HI_MS) {
    closeBucket();
  }
}

void historyAccumulateThermal(float sky, float fmin, float fmax, float med, float amb, float cloud)
{
  accumulate(HM_SKY,   sky);
  accumulate(HM_FMIN,  fmin);
  accumulate(HM_FMAX,  fmax);
  accumulate(HM_MED,   med);
  accumulate(HM_AMB,   amb);
  accumulate(HM_CLOUD, cloud);
}

void historyAccumulateBrightness(float lux, float sqm)
{
  accumulate(HM_LUX, lux);
  accumulate(HM_SQM, sqm);
}

// ---------------------------------------------------------------------------
// JSON streaming
// ---------------------------------------------------------------------------

// Tiny stream helper: batches output into 512-byte chunks, flushed via
// WebServer::sendContent() to avoid building the full JSON string in RAM.
struct JStream {
  WebServer& srv;
  char  buf[512];
  int   pos;

  JStream(WebServer& s) : srv(s), pos(0) {}
  ~JStream() { flush(); }

  void flush() {
    if (pos > 0) { buf[pos] = '\0'; srv.sendContent(buf); pos = 0; }
  }
  void add(const char* s) {
    while (*s) { if (pos >= 510) flush(); buf[pos++] = *s++; }
  }
  void addFloat(float v) {
    if (v == HIST_NO_DATA) { add("null"); return; }
    char tmp[16]; snprintf(tmp, sizeof(tmp), "%.2f", v); add(tmp);
  }
  void addU32(uint32_t v) {
    char tmp[12]; snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)v); add(tmp);
  }
};

// Return a pointer to the i-th most-recent bucket (0 = newest) from a ring.
// head points to the NEXT write slot; count is the number of valid entries.
static const HistBucket* getBucket(const HistBucket* ring, uint8_t head,
                                   uint8_t count, int n, int idxFromNewest)
{
  if (idxFromNewest >= count) return nullptr;
  int pos = ((int)head - 1 - idxFromNewest + n) % n;
  return &ring[pos];
}

void historyStreamJSON(WebServer& server, int minutes)
{
  const bool useLo   = (minutes > 60);
  const int  resSecs = useLo ? 900 : 30;
  const int  wantN   = useLo ? (minutes / 15) : (minutes * 2);
  const int  ringN   = useLo ? HIST_LO_N      : HIST_HI_N;
  const int  avail   = useLo ? loCount         : hiCount;
  const int  count   = min(wantN, avail);

  const HistBucket* ring  = useLo ? loRing : hiRing;
  const uint8_t     rHead = useLo ? loHead : hiHead;

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  JStream js(server);

  // Preamble
  js.add("{\"res\":"); js.addU32(resSecs);
  js.add(",\"now\":"); js.addU32(millis());
  js.add(",\"count\":"); js.addU32(count);

  // Metric arrays – oldest first in each array.
  // Outer loop over each of the three stat types (avg, lo, hi),
  // inner loop over metrics, innermost loop over buckets.
  static const char* keys[HM_N] = {
    "sky","fmin","fmax","med","amb","cloud","lux","sqm"
  };
  static const char* suffixes[3] = { "", "_lo", "_hi" };

  for (int s = 0; s < 3; s++) {         // avg / lo / hi
    for (int m = 0; m < HM_N; m++) {
      js.add(",\""); js.add(keys[m]); js.add(suffixes[s]); js.add("\":[");
      for (int i = count - 1; i >= 0; i--) {   // oldest → newest
        const HistBucket* b = getBucket(ring, rHead, avail, ringN, i);
        if (i < count - 1) js.add(",");
        if (!b) { js.add("null"); continue; }
        float v = (s == 0) ? b->avg[m] : (s == 1) ? b->lo[m] : b->hi[m];
        js.addFloat(v);
      }
      js.add("]");
    }
  }

  // Timestamps (oldest first)
  js.add(",\"t\":[");
  for (int i = count - 1; i >= 0; i--) {
    const HistBucket* b = getBucket(ring, rHead, avail, ringN, i);
    if (i < count - 1) js.add(",");
    if (b) js.addU32(b->t); else js.add("0");
  }
  js.add("]}");
}
