#ifndef CLOUD_MODEL_H
#define CLOUD_MODEL_H

#include <stdint.h>
#include "config.h"

/*
 * Cloud-cover maths with no Arduino dependencies, so the same code runs on the
 * device and in the desktop replay tool (tools/cloud_replay), which feeds it
 * frames recorded by CloudRoll.
 *
 * Cloud cover is a linear interpolation on (ambient − sky) between two
 * thresholds.  A clear sky's (ambient − sky) grows with ambient temperature,
 * so both thresholds can be shifted by the same ambient (and optionally
 * dew-point) dependent offset:
 *
 *   offset   = ambSlope × (ambient − ambRef) [+ dewSlope × (dewPoint − dewRef)]
 *   clear    = clearDelta    + offset
 *   overcast = overcastDelta + offset
 *
 * With both slopes at 0 the thresholds are exactly clearDelta / overcastDelta.
 */

struct CloudParams {
  float clearDelta;     // °C (ambient − sky) at or above which cover is 0 %, before offset
  float overcastDelta;  // °C (ambient − sky) at or below which cover is 100 %, before offset
  float ambSlope;       // threshold shift per °C of ambient above ambRef
  float ambRef;         // °C
  float dewSlope;       // threshold shift per °C of dew point above dewRef
  float dewRef;         // °C
};

struct CloudThresholds {
  float clear;
  float overcast;
};

// Inclusive pixel bounds of the per-pixel cloud-cover region.
struct CloudRegion {
  int rowStart, rowEnd, colStart, colEnd;
};

// Dew point (°C) via the Magnus formula (Sonntag 1990 constants).
float cloudDewPointC(float tempC, float rhPercent);

// Effective thresholds for the given ambient.  Pass NAN for dewPoint when no
// humidity reading is available; the dew-point term is then skipped.
CloudThresholds cloudEffectiveThresholds(const CloudParams &p, float ambient, float dewPoint);

// 0–100 % for one (ambient − sky) delta.  Returns 50 % if the thresholds are
// degenerate (span ≤ 0.01 °C).
float cloudCoverFromDelta(float delta, const CloudThresholds &t);

// Per-pixel region: 0 = centre FOV, 1 = full frame minus edgeExclude on each
// side (falls back to centre FOV if the edge leaves nothing).
CloudRegion cloudPixelRegion(uint8_t region, uint8_t edgeExclude);

// Per-pixel method: cloudCoverFromDelta() on each pixel of the region, averaged.
// frame is SENSOR_PIXELS floats in row-major order.
float cloudCoverPixel(const float *frame, float ambient,
                      const CloudThresholds &t, const CloudRegion &r);

/*
 * Exponential moving average of the ambient reference used for cloud cover.
 * alpha = dt / (tau + dt) from the real elapsed time between frames.  The
 * filter resets to the raw value on the first reading, when forced (ambient
 * source changed, e.g. a BME280 dropout falling back to the MLX die), when
 * tau is 0 (off), or when raw differs from the filtered value by more than
 * resetJumpC.
 */
class AmbientSmoother {
public:
  AmbientSmoother() : _value(0.0f), _valid(false) {}
  float update(float raw, float dtSec, float tauSec, float resetJumpC, bool forceReset);
  float value() const { return _value; }

private:
  float _value;
  bool  _valid;
};

/*
 * Ring buffer of per-frame cloud cover (both methods) with millisecond
 * timestamps; reports the median over a trailing window.  The newest sample
 * is always included, so a window shorter than the frame interval (including
 * 0) returns the latest frame.  Windows longer than the buffer use all of it.
 */
class CloudCoverWindow {
public:
  CloudCoverWindow() : _head(0), _count(0) {}
  void push(uint32_t ms, float mean, float pixel);
  // Returns false (outputs untouched) when no samples have been pushed.
  bool median(uint32_t nowMs, uint32_t windowMs, float &mean, float &pixel, int &samples);

private:
  uint32_t _ms[CLOUD_AVG_SAMPLES];
  float    _mean[CLOUD_AVG_SAMPLES];
  float    _pixel[CLOUD_AVG_SAMPLES];
  float    _scratch[CLOUD_AVG_SAMPLES];
  int      _head;   // next write position
  int      _count;
};

#endif // CLOUD_MODEL_H
