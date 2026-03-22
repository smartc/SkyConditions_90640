#include "sky_sensor.h"
#include "debug.h"
#include <algorithm>
#include <cstring>
#include <math.h>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SkyConditions::SkyConditions()
  : _skyTemperature(0.0f),
    _minTemperature(0.0f),
    _maxTemperature(0.0f),
    _medianTemperature(0.0f),
    _ambientTemperature(0.0f),
    _lastUpdateMillis(0),
    _hasData(false),
    _lux(0.0f),
    _sqm(0.0f),
    _hasBrightnessData(false),
    _lastBrightnessMillis(0),
    _averagePeriod(0.5),
    _connected(true),
    _refreshRequested(false)
{
  memset(_frame, 0, sizeof(_frame));
}

// ---------------------------------------------------------------------------
// Main-loop interface
// ---------------------------------------------------------------------------

void SkyConditions::update(float *frame, float ambientTemp)
{
  memcpy(_frame, frame, SENSOR_PIXELS * sizeof(float));
  computeStats();
  _ambientTemperature = ambientTemp;
  _lastUpdateMillis   = millis();
  _hasData            = true;
}

void SkyConditions::computeStats()
{
  // Center 50% FOV average (sky temperature).
  float sum = 0.0f;
  for (int row = CENTER_ROW_START; row <= CENTER_ROW_END; row++)
    for (int col = CENTER_COL_START; col <= CENTER_COL_END; col++)
      sum += _frame[row * SENSOR_COLS + col];
  _skyTemperature = sum / CENTER_PIXEL_COUNT;

  // Full-frame min / max.
  float minT = _frame[0], maxT = _frame[0];
  for (int i = 1; i < SENSOR_PIXELS; i++) {
    if (_frame[i] < minT) minT = _frame[i];
    if (_frame[i] > maxT) maxT = _frame[i];
  }
  _minTemperature = minT;
  _maxTemperature = maxT;

  // Full-frame median (sort a scratch copy).
  memcpy(_sorted, _frame, SENSOR_PIXELS * sizeof(float));
  std::sort(_sorted, _sorted + SENSOR_PIXELS);
  _medianTemperature = (_sorted[SENSOR_PIXELS / 2 - 1] + _sorted[SENSOR_PIXELS / 2]) / 2.0f;
}

void SkyConditions::fillRGBBuffer(uint8_t *rgb) const
{
  float range = _maxTemperature - _minTemperature;
  if (range < 1.0f) range = 1.0f;

  for (int i = 0; i < SENSOR_PIXELS; i++) {
    float t = (_frame[i] - _minTemperature) / range;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // Jet colourmap – identical to the JavaScript version in html_templates.h.
    float r = 1.5f - fabsf(4.0f * t - 3.0f);
    float g = 1.5f - fabsf(4.0f * t - 2.0f);
    float b = 1.5f - fabsf(4.0f * t - 1.0f);
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f; if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f; if (b > 1.0f) b = 1.0f;

    rgb[i * 3]     = (uint8_t)(r * 255.0f);
    rgb[i * 3 + 1] = (uint8_t)(g * 255.0f);
    rgb[i * 3 + 2] = (uint8_t)(b * 255.0f);
  }
}

void SkyConditions::updateBrightness(float lux)
{
  // lux < 0 signals sensor overflow – skip the update.
  if (lux < 0.0f) return;

  _lux = lux;

  // SQM (mag/arcsec²) approximation from illuminance.
  // Clamped to 1e-6 lux minimum to avoid log(0).
  float safeLux = (lux > 1e-6f) ? lux : 1e-6f;
  _sqm = -2.5f * log10f(safeLux / 108000.0f);

  _lastBrightnessMillis = millis();
  _hasBrightnessData    = true;
}

bool SkyConditions::setAveragePeriod(double v)
{
  if (v < 0.0) return false;
  _averagePeriod = v;
  return true;
}

// ---------------------------------------------------------------------------
// WebSocket binary frame
// ---------------------------------------------------------------------------

void SkyConditions::fillWebSocketBuffer(uint8_t *buf) const
{
  float minT = _minTemperature;
  float maxT = _maxTemperature;
  float medT = _medianTemperature;
  float skyT = _skyTemperature;

  memcpy(buf,      &minT, 4);
  memcpy(buf +  4, &maxT, 4);
  memcpy(buf +  8, &medT, 4);
  memcpy(buf + 12, &skyT, 4);

  float range = maxT - minT;
  if (range < 1.0f) range = 1.0f;

  for (int i = 0; i < SENSOR_PIXELS; i++) {
    float norm = (_frame[i] - minT) / range;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    buf[WS_HEADER_SIZE + i] = (uint8_t)(norm * 255.0f);
  }
}
