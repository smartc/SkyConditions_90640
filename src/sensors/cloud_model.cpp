#include "cloud_model.h"
#include <algorithm>
#include <math.h>

float cloudDewPointC(float tempC, float rhPercent)
{
  if (rhPercent < 0.1f)   rhPercent = 0.1f;   // ln(0) guard
  if (rhPercent > 100.0f) rhPercent = 100.0f;
  const float a = 17.62f, b = 243.12f;
  float g = logf(rhPercent / 100.0f) + a * tempC / (b + tempC);
  return b * g / (a - g);
}

CloudThresholds cloudEffectiveThresholds(const CloudParams &p, float ambient, float dewPoint)
{
  float offset = p.ambSlope * (ambient - p.ambRef);
  if (p.dewSlope != 0.0f && !isnan(dewPoint))
    offset += p.dewSlope * (dewPoint - p.dewRef);
  CloudThresholds t;
  t.clear    = p.clearDelta    + offset;
  t.overcast = p.overcastDelta + offset;
  return t;
}

float cloudCoverFromDelta(float delta, const CloudThresholds &t)
{
  float span = t.clear - t.overcast;
  if (span <= 0.01f) return 50.0f;
  float f = (delta - t.overcast) / span;
  if (f < 0.0f) f = 0.0f;
  if (f > 1.0f) f = 1.0f;
  return (1.0f - f) * 100.0f;
}

CloudRegion cloudPixelRegion(uint8_t region, uint8_t edgeExclude)
{
  CloudRegion r = { CENTER_ROW_START, CENTER_ROW_END, CENTER_COL_START, CENTER_COL_END };
  if (region == 1) {
    int edge = (int)edgeExclude;
    CloudRegion full = { edge, SENSOR_ROWS - 1 - edge, edge, SENSOR_COLS - 1 - edge };
    // Guard against degenerate config (edge too large).
    if (full.rowStart <= full.rowEnd && full.colStart <= full.colEnd) r = full;
  }
  return r;
}

float cloudCoverPixel(const float *frame, float ambient,
                      const CloudThresholds &t, const CloudRegion &r)
{
  if (t.clear - t.overcast <= 0.01f) return 50.0f;
  float sum = 0.0f;
  for (int row = r.rowStart; row <= r.rowEnd; row++)
    for (int col = r.colStart; col <= r.colEnd; col++)
      sum += cloudCoverFromDelta(ambient - frame[row * SENSOR_COLS + col], t);
  int count = (r.rowEnd - r.rowStart + 1) * (r.colEnd - r.colStart + 1);
  return sum / count;
}

// ---------------------------------------------------------------------------
// AmbientSmoother
// ---------------------------------------------------------------------------

float AmbientSmoother::update(float raw, float dtSec, float tauSec, float resetJumpC, bool forceReset)
{
  if (!_valid || forceReset || tauSec <= 0.0f || dtSec < 0.0f ||
      fabsf(raw - _value) > resetJumpC) {
    _value = raw;
    _valid = true;
    return _value;
  }
  float alpha = dtSec / (tauSec + dtSec);
  _value += alpha * (raw - _value);
  return _value;
}

// ---------------------------------------------------------------------------
// CloudCoverWindow
// ---------------------------------------------------------------------------

void CloudCoverWindow::push(uint32_t ms, float mean, float pixel)
{
  _ms[_head]    = ms;
  _mean[_head]  = mean;
  _pixel[_head] = pixel;
  _head = (_head + 1) % CLOUD_AVG_SAMPLES;
  if (_count < CLOUD_AVG_SAMPLES) _count++;
}

// Median of the first n values of buf (sorts buf in place).
static float medianOf(float *buf, int n)
{
  std::sort(buf, buf + n);
  return (n % 2) ? buf[n / 2] : 0.5f * (buf[n / 2 - 1] + buf[n / 2]);
}

bool CloudCoverWindow::median(uint32_t nowMs, uint32_t windowMs, float &mean, float &pixel, int &samples)
{
  if (_count == 0) return false;

  // Walk newest → oldest; the newest sample is always taken.
  int n = 0;
  for (int k = 0; k < _count; k++) {
    int i = (_head - 1 - k + CLOUD_AVG_SAMPLES) % CLOUD_AVG_SAMPLES;
    if (k > 0 && (uint32_t)(nowMs - _ms[i]) > windowMs) break;
    n++;
  }

  for (int k = 0; k < n; k++)
    _scratch[k] = _mean[(_head - 1 - k + CLOUD_AVG_SAMPLES) % CLOUD_AVG_SAMPLES];
  mean = medianOf(_scratch, n);
  for (int k = 0; k < n; k++)
    _scratch[k] = _pixel[(_head - 1 - k + CLOUD_AVG_SAMPLES) % CLOUD_AVG_SAMPLES];
  pixel = medianOf(_scratch, n);
  samples = n;
  return true;
}
