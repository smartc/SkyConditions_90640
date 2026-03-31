#include "debug.h"

DebugClass Debug;

// ── Console log ring buffer ───────────────────────────────────────────────────
// 40 entries × 96 chars = ~4 KB static RAM.
// Consecutive identical lines are collapsed: only the repeat count increments.
// logSeq increments on every change so the browser can detect updates cheaply.

#define LOG_ENTRIES  40
#define LOG_MSG_LEN  96

struct LogEntry {
  uint32_t ms;              // millis() at most-recent occurrence
  uint16_t count;           // repeat count (1 = seen once)
  char     msg[LOG_MSG_LEN];
};

static LogEntry  logRing[LOG_ENTRIES];
static int       logHead  = 0;   // index of oldest valid entry
static int       logCount = 0;   // number of valid entries (0..LOG_ENTRIES)
static uint32_t  logSeq   = 0;   // increments on every change; used by browser to detect updates

// Line accumulator – text is appended char-by-char until a '\n' triggers a flush.
static char   lineAccum[LOG_MSG_LEN];
static size_t lineLen = 0;

// Push a complete (newline-stripped) line into the ring buffer.
static void pushLine(const char *line)
{
  uint32_t now = millis();

  // Dedup: if the last entry is identical, just increment its count.
  if (logCount > 0) {
    int last = (logHead + logCount - 1) % LOG_ENTRIES;
    if (strncmp(logRing[last].msg, line, LOG_MSG_LEN) == 0) {
      logRing[last].count++;
      logRing[last].ms = now;
      logSeq++;
      return;
    }
  }

  // New entry: overwrite oldest slot when full.
  int idx;
  if (logCount < LOG_ENTRIES) {
    idx = (logHead + logCount) % LOG_ENTRIES;
    logCount++;
  } else {
    idx = logHead;
    logHead = (logHead + 1) % LOG_ENTRIES;
  }
  strncpy(logRing[idx].msg, line, LOG_MSG_LEN - 1);
  logRing[idx].msg[LOG_MSG_LEN - 1] = '\0';
  logRing[idx].ms    = now;
  logRing[idx].count = 1;
  logSeq++;
}

void logAppend(const char *text)
{
  for (const char *p = text; *p; p++) {
    if (*p == '\n') {
      if (lineLen > 0) {
        lineAccum[lineLen] = '\0';
        pushLine(lineAccum);
        lineLen = 0;
      }
    } else if (*p != '\r' && lineLen < LOG_MSG_LEN - 1) {
      lineAccum[lineLen++] = *p;
    }
  }
}

// Escape a C string for embedding in a JSON string value.
static void jsonEscape(String &out, const char *s)
{
  for (; *s; s++) {
    if      (*s == '"')  { out += "\\\""; }
    else if (*s == '\\') { out += "\\\\"; }
    else if ((uint8_t)*s < 0x20) { /* skip control chars */ }
    else                 { out += *s; }
  }
}

void logGetJSON(String &out)
{
  out = "{\"seq\":";
  out += logSeq;
  out += ",\"entries\":[";
  for (int i = 0; i < logCount; i++) {
    int idx = (logHead + i) % LOG_ENTRIES;
    if (i > 0) out += ',';
    out += "{\"ms\":";
    out += logRing[idx].ms;
    out += ",\"n\":";
    out += logRing[idx].count;
    out += ",\"msg\":\"";
    jsonEscape(out, logRing[idx].msg);
    out += "\"}";
  }
  out += "]}";
}
