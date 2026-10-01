#ifndef SKY_SENSOR_H
#define SKY_SENSOR_H

#include <Arduino.h>
#include "config.h"

/*
 * SkyConditions
 *
 * Holds MLX90640 frame data and computes:
 *   SkyTemperature  – average of the center 50% FOV pixels (16×12 = 192)
 *   Temperature     – sensor die temperature (ambient proxy)
 *   Min / Max / Median – full-frame statistics for the WebSocket viewer
 *
 * All methods are called from the Arduino main loop; no locking is needed.
 * ASCOM Alpaca HTTP handling lives in alpaca.h/.cpp.
 */
class SkyConditions
{
public:
  SkyConditions();

  // Called from the main loop each time a new thermal frame arrives.
  // frame must be SENSOR_PIXELS floats in row-major order.
  void update(float *frame, float ambientTemp);

  // Set/cleared by the ASCOM PUT /refresh handler and the main loop.
  bool isRefreshRequested() const { return _refreshRequested; }
  void clearRefreshRequest()      { _refreshRequested = false; }
  void requestRefresh()           { _refreshRequested = true; }

  // Read-only accessors used by the web UI and ASCOM handlers.
  float getSkyTemperature()     const { return _skyTemperature; }
  float getMinTemperature()     const { return _minTemperature; }
  float getMaxTemperature()     const { return _maxTemperature; }
  float getMedianTemperature()  const { return _medianTemperature; }
  float getAmbientTemperature() const { return _ambientTemperature; }
  unsigned long getLastUpdateMillis() const { return _lastUpdateMillis; }
  bool hasData() const { return _hasData; }

  // Called from the main loop each time a new TSL2591 reading is available.
  // Computes SQM from lux internally.  Pass lux < 0 to signal sensor overflow.
  void updateBrightness(float lux);

  float getLux()  const { return _lux; }
  float getSqm()  const { return _sqm; }
  bool  hasBrightnessData()        const { return _hasBrightnessData; }
  unsigned long getLastBrightnessMillis() const { return _lastBrightnessMillis; }

  // Cloud cover (0–100 %).  Valid whenever hasData() is true.
  // getCloudCover() returns whichever method is selected in deviceConfig.cloudCoverMethod.
  float getCloudCover()      const;
  float getCloudCoverMean()  const { return _cloudCover; }
  float getCloudCoverPixel() const { return _cloudCoverPixel; }

  // ASCOM device state (read/written by alpaca.cpp handlers).
  bool   getConnected()      const { return _connected; }
  void   setConnected(bool v)      { _connected = v; }
  double getAveragePeriod()  const { return _averagePeriod; }
  bool   setAveragePeriod(double v);  // returns false if v < 0

  // Raw pixel access – returns pointer to the internal SENSOR_PIXELS float array.
  // Only valid while hasData() is true; caller must not hold across update().
  const float* getFrame() const { return _frame; }

  // Fills a WebSocket binary frame buffer; buf must be >= WS_FRAME_SIZE bytes.
  void fillWebSocketBuffer(uint8_t *buf) const;

  // Fills an RGB888 buffer (SENSOR_PIXELS * 3 bytes) with heatmap colours.
  // Uses the same palette indices and heatmapColor() as the live canvas.
  void fillRGBBuffer(uint8_t *rgb888) const;

private:
  // Temperature mapped to palette position 0 (coldEnd), the span to the top of
  // the palette, and the threshold above which pixels get HEATMAP_HOT_INDEX.
  // Floating mode anchors to the cloud-cover deltas around ambient; fixed mode
  // uses deviceConfig.heatmapFixedMin/Max.  Display only – cloud cover
  // calculations are unaffected.
  void getDisplayRange(float &coldEnd, float &range, float &hotAbove) const;

  // Palette index (0..HEATMAP_HOT_INDEX) for each pixel of the current frame.
  void fillPaletteIndices(uint8_t *idx) const;

  // Raw frame – main-loop only.
  float _frame[SENSOR_PIXELS];
  float _sorted[SENSOR_PIXELS];  // scratch for median

  // Computed stats.
  float         _skyTemperature;
  float         _minTemperature;
  float         _maxTemperature;
  float         _medianTemperature;
  float         _ambientTemperature;
  unsigned long _lastUpdateMillis;
  bool          _hasData;

  // TSL2591 brightness data.
  float         _lux;
  float         _sqm;
  bool          _hasBrightnessData;
  unsigned long _lastBrightnessMillis;

  // Cloud cover derived from thermal delta.
  float         _cloudCover;       // mean method (center FOV average delta)
  float         _cloudCoverPixel;  // per-pixel method (center FOV pixel-by-pixel)

  // ASCOM device state.
  double        _averagePeriod;
  bool          _connected;
  volatile bool _refreshRequested;

  void computeStats();
};

// Singleton shared between the main sketch, web_ui_handler, and alpaca.
extern SkyConditions skyConditions;

// Heatmap colour for a palette index under the current deviceConfig range
// mode.  Shared by the JPEG snapshot and the browser lookup table so both
// render identically.
void heatmapColor(uint8_t idx, uint8_t &r, uint8_t &g, uint8_t &b);

#endif // SKY_SENSOR_H
