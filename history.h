#ifndef HISTORY_H
#define HISTORY_H

#include <Arduino.h>
#include <WebServer.h>

// Metric indices – order must match JSON key array in history.cpp
enum { HM_SKY=0, HM_FMIN, HM_FMAX, HM_MED, HM_AMB, HM_CLOUD, HM_LUX, HM_SQM, HM_N };

// Sentinel stored in a bucket slot when no samples arrived for that metric.
#define HIST_NO_DATA  -999.0f

// Hi-res ring: one bucket per 30 s, covers 60 min.
#define HIST_HI_N     120
#define HIST_HI_MS    30000UL

// Lo-res ring: one bucket per 15 min (= 30 hi-res buckets), covers 24 h.
#define HIST_LO_N     96
#define HIST_LO_EVERY 30   // hi-res buckets per lo-res bucket

struct HistBucket {
  float    avg[HM_N];
  float    lo[HM_N];
  float    hi[HM_N];
  uint32_t t;          // millis() when bucket was closed
};

void historySetup();
void historyLoop();
void historyAccumulateThermal(float sky, float fmin, float fmax, float med, float amb, float cloud);
void historyAccumulateBrightness(float lux, float sqm);

// Stream history JSON to server; minutes: 5, 30, 60, 120, 360, 720, 1440.
void historyStreamJSON(WebServer& server, int minutes);

#endif // HISTORY_H
