#include "sky_sensor.h"
#include "config_store.h"
#include "../debug/debug.h"
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
    _cloudCover(50.0f),
    _cloudCoverPixel(50.0f),
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

  // Cloud cover: linear interpolation on (ambient – sky) delta.
  // delta >= cloudClearDelta    → 0 % (clear)
  // delta <= cloudOvercastDelta → 100 % (overcast)
  float span = deviceConfig.cloudClearDelta - deviceConfig.cloudOvercastDelta;

  // Method 1: mean — single delta from center-FOV average temperature.
  if (span > 0.01f) {
    float delta = ambientTemp - _skyTemperature;
    float t = (delta - deviceConfig.cloudOvercastDelta) / span;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    _cloudCover = (1.0f - t) * 100.0f;
  } else {
    _cloudCover = 50.0f;
  }

  // Method 2: per-pixel — apply the same interpolation to each pixel in the
  // selected region independently, then average.
  if (span > 0.01f) {
    int rowStart, rowEnd, colStart, colEnd;
    if (deviceConfig.cloudPixelRegion == 1) {
      // Full frame minus configurable edge border.
      int edge = (int)deviceConfig.cloudEdgeExclude;
      rowStart = edge;
      rowEnd   = SENSOR_ROWS - 1 - edge;
      colStart = edge;
      colEnd   = SENSOR_COLS - 1 - edge;
      // Guard against degenerate config (edge too large).
      if (rowStart > rowEnd || colStart > colEnd) {
        rowStart = CENTER_ROW_START; rowEnd = CENTER_ROW_END;
        colStart = CENTER_COL_START; colEnd = CENTER_COL_END;
      }
    } else {
      rowStart = CENTER_ROW_START; rowEnd = CENTER_ROW_END;
      colStart = CENTER_COL_START; colEnd = CENTER_COL_END;
    }
    int pixCount = (rowEnd - rowStart + 1) * (colEnd - colStart + 1);

    float cloudSum = 0.0f;
    for (int row = rowStart; row <= rowEnd; row++) {
      for (int col = colStart; col <= colEnd; col++) {
        float delta = ambientTemp - _frame[row * SENSOR_COLS + col];
        float t = (delta - deviceConfig.cloudOvercastDelta) / span;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        cloudSum += (1.0f - t);
      }
    }
    _cloudCoverPixel = (cloudSum / pixCount) * 100.0f;
  } else {
    _cloudCoverPixel = 50.0f;
  }

  _lastUpdateMillis = millis();
  _hasData          = true;
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

// ClearDarkSky cloud-cover palette: t=0 (coldest/clear) → dark navy,
// t=1 (warmest/overcast) → white.  11 stops at 0.0, 0.1 … 1.0.
static void cdsColormap(float t, uint8_t &r, uint8_t &g, uint8_t &b)
{
  static const uint8_t stops[11][3] = {
    {  0,  62, 126},  // 0.0 – Clear         #003E7E
    { 19,  83, 147},  // 0.1                  #135393
    { 38, 102, 166},  // 0.2                  #2666A6
    { 78, 142, 206},  // 0.3                  #4E8ECE
    { 98, 162, 226},  // 0.4                  #62A2E2
    {118, 182, 246},  // 0.5                  #76B6F6
    {153, 217, 217},  // 0.6                  #99D9D9
    {173, 237, 237},  // 0.7                  #ADEDED
    {193, 193, 193},  // 0.8                  #C1C1C1
    {233, 233, 233},  // 0.9                  #E9E9E9
    {250, 250, 250},  // 1.0 – Overcast       #FAFAFA
  };
  if (t <= 0.0f) { r = stops[0][0]; g = stops[0][1]; b = stops[0][2]; return; }
  if (t >= 1.0f) { r = stops[10][0]; g = stops[10][1]; b = stops[10][2]; return; }
  int   lo = (int)(t * 10.0f);
  float f  = t * 10.0f - lo;
  r = (uint8_t)(stops[lo][0] + f * (stops[lo+1][0] - stops[lo][0]));
  g = (uint8_t)(stops[lo][1] + f * (stops[lo+1][1] - stops[lo][1]));
  b = (uint8_t)(stops[lo][2] + f * (stops[lo+1][2] - stops[lo][2]));
}

// Fixed-mode warm tail: white → yellow → orange → red → dark red, positioned
// as fractions of the span from HEATMAP_WARM_START_C to heatmapFixedMax.
static const float   WARM_POS[5]      = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
static const uint8_t WARM_STOPS[5][3] = {
  {250, 250, 250},  // matches the top of the ClearDarkSky palette
  {255, 230,  90},  // yellow
  {255, 150,  40},  // orange
  {220,  40,  30},  // red
  {150,   0,  40},  // dark red
};

void heatmapColor(uint8_t idx, uint8_t &r, uint8_t &g, uint8_t &b)
{
  if (idx == HEATMAP_HOT_INDEX) { r = 255; g = 0; b = 255; return; }  // magenta
  float t = idx / (float)(HEATMAP_HOT_INDEX - 1);

  if (deviceConfig.heatmapRangeMode != 1) { cdsColormap(t, r, g, b); return; }

  float lo    = deviceConfig.heatmapFixedMin;
  float hi    = deviceConfig.heatmapFixedMax;
  float T     = lo + t * (hi - lo);
  float split = constrain(HEATMAP_WARM_START_C, lo, hi);
  if (T <= split) {
    cdsColormap(split > lo ? (T - lo) / (split - lo) : 1.0f, r, g, b);
    return;
  }
  float w = (T - split) / (hi - split);
  int   k = 1;
  while (k < 4 && w > WARM_POS[k]) k++;
  float f = (w - WARM_POS[k - 1]) / (WARM_POS[k] - WARM_POS[k - 1]);
  r = (uint8_t)(WARM_STOPS[k-1][0] + f * (WARM_STOPS[k][0] - WARM_STOPS[k-1][0]));
  g = (uint8_t)(WARM_STOPS[k-1][1] + f * (WARM_STOPS[k][1] - WARM_STOPS[k-1][1]));
  b = (uint8_t)(WARM_STOPS[k-1][2] + f * (WARM_STOPS[k][2] - WARM_STOPS[k-1][2]));
}

void SkyConditions::getDisplayRange(float &coldEnd, float &range, float &hotAbove) const
{
  if (deviceConfig.heatmapRangeMode == 1) {
    coldEnd  = deviceConfig.heatmapFixedMin;
    range    = deviceConfig.heatmapFixedMax - deviceConfig.heatmapFixedMin;
    if (range < 1.0f) range = 1.0f;
    hotAbove = coldEnd + range;   // anything past the top of the scale
  } else {
    // Anchor the colormap to the cloud-cover calibration scale so that a clear
    // sky with a minor temperature gradient stays in the dark-blue region rather
    // than spanning the full range to white.
    //   coldEnd = temperature a "clearly clear" sky would have (ambient - clearDelta)
    //   warmEnd = temperature an "overcast" sky would have   (ambient - overcastDelta)
    coldEnd  = _ambientTemperature - deviceConfig.cloudClearDelta;
    range    = deviceConfig.cloudClearDelta - deviceConfig.cloudOvercastDelta;
    if (range < 1.0f) range = 1.0f;
    // The top of the floating scale sits below ambient, so flag only pixels
    // well above ambient (e.g. the sun) rather than the warm frame edges.
    hotAbove = _ambientTemperature + HEATMAP_HOT_MARGIN_C;
  }
}

void SkyConditions::fillPaletteIndices(uint8_t *idx) const
{
  float coldEnd, range, hotAbove;
  getDisplayRange(coldEnd, range, hotAbove);

  // Smooth a display-only copy of the frame with a 3x3 box average.  The
  // MLX90640 reads each frame as two interleaved sub-frames that rarely match
  // exactly, which shows up as a faint per-pixel checkerboard – most visible
  // on an otherwise-uniform clear sky.  Averaging neighbours cancels it out.
  // ASCOM readings and cloud cover read _frame directly and are unaffected.
  for (int r = 0; r < SENSOR_ROWS; r++) {
    for (int c = 0; c < SENSOR_COLS; c++) {
      float sum = 0.0f;
      int   n   = 0;
      for (int dr = -1; dr <= 1; dr++) {
        int rr = r + dr;
        if (rr < 0 || rr >= SENSOR_ROWS) continue;
        for (int dc = -1; dc <= 1; dc++) {
          int cc = c + dc;
          if (cc < 0 || cc >= SENSOR_COLS) continue;
          sum += _frame[rr * SENSOR_COLS + cc];
          n++;
        }
      }
      _blurScratch[r * SENSOR_COLS + c] = sum / n;
    }
  }

  for (int i = 0; i < SENSOR_PIXELS; i++) {
    // Genuine single-pixel extremes (e.g. the sun) are exempt from smoothing
    // so they still clamp to the end colour (or show as the hot marker)
    // instead of being diluted into their much colder neighbours.
    bool  hot = _frame[i] > hotAbove;
    if (hot && deviceConfig.heatmapHotMarker) { idx[i] = HEATMAP_HOT_INDEX; continue; }
    float T = hot ? _frame[i] : _blurScratch[i];
    float t = (T - coldEnd) / range;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    idx[i] = (uint8_t)lroundf(t * (HEATMAP_HOT_INDEX - 1));
  }
}

void SkyConditions::fillRGBBuffer(uint8_t *rgb) const
{
  uint8_t idx[SENSOR_PIXELS];
  fillPaletteIndices(idx);
  for (int i = 0; i < SENSOR_PIXELS; i++)
    heatmapColor(idx[i], rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
}

void SkyConditions::updateBrightness(float lux)
{
  // lux < 0 signals sensor overflow – skip the update.
  if (lux < 0.0f) return;

  _lux = lux;

  // SQM (mag/arcsec²) approximation from illuminance.
  // Clamped to 1e-6 lux minimum to avoid log(0).
  float safeLux = (lux > 1e-6f) ? lux : 1e-6f;
  _sqm = -2.5f * log10f(safeLux / deviceConfig.sqmReference) + deviceConfig.sqmOffset;

  _lastBrightnessMillis = millis();
  _hasBrightnessData    = true;
}

float SkyConditions::getCloudCover() const
{
  return (deviceConfig.cloudCoverMethod == 1) ? _cloudCoverPixel : _cloudCover;
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

  // Same palette indices as fillRGBBuffer, so the canvas and the JPEG snapshot
  // share one colour scale.
  fillPaletteIndices(buf + WS_HEADER_SIZE);
}
