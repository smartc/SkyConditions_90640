/*
 * Web UI handler – HTTP server (port 80) and WebSocket server (port 81)
 *
 * Port 80:  Human-readable status and setup pages.
 * Port 81:  Binary WebSocket stream of thermal frames (784-byte frames at 2 Hz).
 *           Frame layout is defined in config.h (WS_FRAME_SIZE / WS_HEADER_SIZE).
 */

#include "web_ui_handler.h"
#include "html_templates.h"
#include "config_store.h"
#include "history.h"
#include "mqtt_handler.h"
#include "rain_sensor.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <img_converters.h>
#include <esp_heap_caps.h>

WebServer        webUiServer(80);
WebSocketsServer wsServer(81);

static uint8_t wsFrameBuffer[WS_FRAME_SIZE];

// ---------------------------------------------------------------------------
// Thermal snapshot – static buffers; no heap allocation during updates.
//
// scaledBuf  – nearest-neighbour upscale staging:
//              (SENSOR_COLS * THERMAL_JPEG_SCALE) × (SENSOR_ROWS * THERMAL_JPEG_SCALE) RGB888
//              At scale=4: 128×96×3 = 36,864 bytes
//
// jpegOutBuf – JPEG output written via fmt2jpg_cb callback.
//              20 KB is well above the worst-case size for 128×96 quality-80.
//
// Both live in the BSS segment (zero-cost at runtime, ~57 KB total).
// ---------------------------------------------------------------------------
#define SNAP_W  (SENSOR_COLS * THERMAL_JPEG_SCALE)
#define SNAP_H  (SENSOR_ROWS * THERMAL_JPEG_SCALE)

static uint8_t*      scaledBuf   = nullptr;    // allocated in PSRAM by initWebUI()
static uint8_t       jpegOutBuf[48 * 1024];   // 48 KB – comfortable for 320×240 quality-80
static size_t        jpegOutLen    = 0;
static unsigned long lastJpegUpdate = 0;

// ---------------------------------------------------------------------------
// fmt2jpg_cb output callback – writes JPEG chunks into jpegOutBuf.
// ---------------------------------------------------------------------------
struct JpegOutCtx { size_t written; bool overflow; };

static size_t jpegOutCb(void *arg, size_t index, const void *data, size_t len)
{
  JpegOutCtx *ctx = (JpegOutCtx *)arg;
  if (index == 0) { ctx->written = 0; ctx->overflow = false; }
  if (ctx->overflow || ctx->written + len > sizeof(jpegOutBuf)) {
    ctx->overflow = true;
    return 0;   // returning 0 aborts encoding
  }
  memcpy(jpegOutBuf + ctx->written, data, len);
  ctx->written += len;
  return len;
}

// ---------------------------------------------------------------------------
// WebSocket event handler
// ---------------------------------------------------------------------------
static void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *, size_t)
{
  switch (type) {
    case WStype_CONNECTED:
      Debug.printf("[WS] Client %u connected\n", num);
      break;
    case WStype_DISCONNECTED:
      Debug.printf("[WS] Client %u disconnected\n", num);
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Thermal snapshot
// ---------------------------------------------------------------------------

void updateThermalSnapshot()
{
  if (!skyConditions.hasData()) return;
  if (!scaledBuf) { Debug.println("updateThermalSnapshot: scaledBuf not allocated"); return; }

  // Source: native 32×24 RGB888 (stack-allocated, 2304 bytes)
  uint8_t srcRgb[SENSOR_PIXELS * 3];
  skyConditions.fillRGBBuffer(srcRgb);

  // Nearest-neighbour upscale into static staging buffer.
  for (int row = 0; row < SENSOR_ROWS; row++) {
    for (int col = 0; col < SENSOR_COLS; col++) {
      const uint8_t *src = &srcRgb[(row * SENSOR_COLS + col) * 3];
      for (int dy = 0; dy < THERMAL_JPEG_SCALE; dy++) {
        uint8_t *dst = &scaledBuf[
          ((row * THERMAL_JPEG_SCALE + dy) * SNAP_W + col * THERMAL_JPEG_SCALE) * 3];
        for (int dx = 0; dx < THERMAL_JPEG_SCALE; dx++, dst += 3) {
          dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0];  // R↔B swap: fmt2jpg_cb expects BGR
        }
      }
    }
  }

  // JPEG-encode via callback – fmt2jpg_cb does not allocate an output buffer.
  JpegOutCtx ctx = {0, false};
  const size_t scaledBytes = (size_t)SNAP_W * SNAP_H * 3;
  bool ok = fmt2jpg_cb(scaledBuf, scaledBytes,
                       SNAP_W, SNAP_H, PIXFORMAT_RGB888, deviceConfig.jpegQuality,
                       jpegOutCb, &ctx);
  if (ok && !ctx.overflow) {
    jpegOutLen     = ctx.written;
    lastJpegUpdate = millis();
    Debug.printf("updateThermalSnapshot: %dx%d JPEG %u B  heap=%u B\n",
                 SNAP_W, SNAP_H, (unsigned)jpegOutLen, ESP.getFreeHeap());
  } else {
    Debug.printf("updateThermalSnapshot: encode failed (ok=%d overflow=%d heap=%u B)\n",
                 (int)ok, (int)ctx.overflow, ESP.getFreeHeap());
  }
}

static void handleThermalJpeg()
{
  if (jpegOutLen == 0) {
    webUiServer.send(503, "text/plain", "No thermal data yet");
    return;
  }
  webUiServer.send_P(200, "image/jpeg", (const char*)jpegOutBuf, jpegOutLen);
}

// ---------------------------------------------------------------------------
// HTTP handlers
// ---------------------------------------------------------------------------

static void handleRoot()
{
  webUiServer.send(200, "text/html", getHomePage());
}

static void handleSetup()
{
  webUiServer.send(200, "text/html", getSetupPage());
}

static void handleResetWifi()
{
  webUiServer.send(200, "text/plain", "Resetting WiFi settings and restarting...");
  Debug.println("WiFi reset requested – restarting device");
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.restart();
}

static void handleReboot()
{
  webUiServer.send(200, "text/plain", "Rebooting...");
  Debug.println("Reboot requested via web UI");
  delay(500);
  ESP.restart();
}

static void handleTrends()
{
  webUiServer.send(200, "text/html", getTrendsPage());
}

static void handleHistoryJSON()
{
  int minutes = 60;
  if (webUiServer.hasArg("minutes"))
    minutes = webUiServer.arg("minutes").toInt();
  historyStreamJSON(webUiServer, minutes);
}

static void handleSaveConfig()
{
  if (webUiServer.hasArg("sqmOffset"))
    deviceConfig.sqmOffset = webUiServer.arg("sqmOffset").toFloat();
  if (webUiServer.hasArg("sqmRef"))
    deviceConfig.sqmReference = webUiServer.arg("sqmRef").toFloat();
  if (webUiServer.hasArg("cloudClear"))
    deviceConfig.cloudClearDelta = webUiServer.arg("cloudClear").toFloat();
  if (webUiServer.hasArg("cloudOvercast"))
    deviceConfig.cloudOvercastDelta = webUiServer.arg("cloudOvercast").toFloat();
  if (webUiServer.hasArg("snapSec"))
    deviceConfig.snapshotIntervalSec = (uint16_t)webUiServer.arg("snapSec").toInt();
  if (webUiServer.hasArg("jpegQuality"))
    deviceConfig.jpegQuality = (uint8_t)constrain(webUiServer.arg("jpegQuality").toInt(), 1, 100);
  if (webUiServer.hasArg("tslInteg"))
    deviceConfig.tsl2591Integration = (uint8_t)constrain(webUiServer.arg("tslInteg").toInt(), 0, 5);
  if (webUiServer.hasArg("avgPeriod"))
    deviceConfig.averagePeriod = webUiServer.arg("avgPeriod").toDouble();
  if (webUiServer.hasArg("location")) {
    String loc = webUiServer.arg("location");
    strncpy(deviceConfig.location, loc.c_str(), sizeof(deviceConfig.location) - 1);
    deviceConfig.location[sizeof(deviceConfig.location) - 1] = '\0';
  }
  if (webUiServer.hasArg("ntpServer")) {
    String ntp = webUiServer.arg("ntpServer");
    ntp.trim();
    strncpy(deviceConfig.ntpServer, ntp.c_str(), sizeof(deviceConfig.ntpServer) - 1);
    deviceConfig.ntpServer[sizeof(deviceConfig.ntpServer) - 1] = '\0';
  }
  if (webUiServer.hasArg("cloudMethod"))
    deviceConfig.cloudCoverMethod = (uint8_t)(webUiServer.arg("cloudMethod").toInt() != 0 ? 1 : 0);
  if (webUiServer.hasArg("cloudPxRgn"))
    deviceConfig.cloudPixelRegion = (uint8_t)(webUiServer.arg("cloudPxRgn").toInt() != 0 ? 1 : 0);
  if (webUiServer.hasArg("cloudEdge"))
    deviceConfig.cloudEdgeExclude = (uint8_t)constrain(webUiServer.arg("cloudEdge").toInt(), 0, 10);

  // MQTT
  deviceConfig.mqttEnabled = webUiServer.hasArg("mqttEnabled");
  if (webUiServer.hasArg("mqttServer")) {
    String s = webUiServer.arg("mqttServer"); s.trim();
    strncpy(deviceConfig.mqttServer, s.c_str(), sizeof(deviceConfig.mqttServer) - 1);
    deviceConfig.mqttServer[sizeof(deviceConfig.mqttServer) - 1] = '\0';
  }
  if (webUiServer.hasArg("mqttPort"))
    deviceConfig.mqttPort = (uint16_t)constrain(webUiServer.arg("mqttPort").toInt(), 1, 65535);
  if (webUiServer.hasArg("mqttUser")) {
    String s = webUiServer.arg("mqttUser"); s.trim();
    strncpy(deviceConfig.mqttUser, s.c_str(), sizeof(deviceConfig.mqttUser) - 1);
    deviceConfig.mqttUser[sizeof(deviceConfig.mqttUser) - 1] = '\0';
  }
  if (webUiServer.hasArg("mqttPass")) {
    String s = webUiServer.arg("mqttPass");
    strncpy(deviceConfig.mqttPassword, s.c_str(), sizeof(deviceConfig.mqttPassword) - 1);
    deviceConfig.mqttPassword[sizeof(deviceConfig.mqttPassword) - 1] = '\0';
  }
  if (webUiServer.hasArg("mqttTopicPfx")) {
    String s = webUiServer.arg("mqttTopicPfx"); s.trim();
    if (s.length() > 0) {
      strncpy(deviceConfig.mqttTopicPrefix, s.c_str(), sizeof(deviceConfig.mqttTopicPrefix) - 1);
      deviceConfig.mqttTopicPrefix[sizeof(deviceConfig.mqttTopicPrefix) - 1] = '\0';
    }
  }

  // Rain sensor
  if (webUiServer.hasArg("rainMode"))
    deviceConfig.rainMode = (uint8_t)(webUiServer.arg("rainMode").toInt() != 0 ? 1 : 0);

  configSave(deviceConfig);
  Debug.println("Config saved via web UI");
  webUiServer.sendHeader("Location", "/setup");
  webUiServer.send(303, "text/plain", "Redirecting...");
}

static void handleThermalMatrix()
{
  if (!skyConditions.hasData()) {
    webUiServer.send(503, "application/json", "{\"error\":\"No thermal data yet\"}");
    return;
  }

  const float *frame = skyConditions.getFrame();
  char buf[32];

  webUiServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webUiServer.send(200, "application/json", "");

  snprintf(buf, sizeof(buf), "{\"rows\":%d,\"cols\":%d", SENSOR_ROWS, SENSOR_COLS);
  webUiServer.sendContent(buf);

  snprintf(buf, sizeof(buf), ",\"ambient\":%.2f", skyConditions.getAmbientTemperature());
  webUiServer.sendContent(buf);

  snprintf(buf, sizeof(buf), ",\"sky\":%.2f", skyConditions.getSkyTemperature());
  webUiServer.sendContent(buf);

  webUiServer.sendContent(",\"pixels\":[");
  for (int row = 0; row < SENSOR_ROWS; row++) {
    webUiServer.sendContent(row == 0 ? "[" : ",[");
    for (int col = 0; col < SENSOR_COLS; col++) {
      snprintf(buf, sizeof(buf), col == 0 ? "%.2f" : ",%.2f",
               frame[row * SENSOR_COLS + col]);
      webUiServer.sendContent(buf);
    }
    webUiServer.sendContent("]");
  }
  webUiServer.sendContent("]}");
}

bool getThermalJpeg(const uint8_t **buf, size_t *len)
{
  if (jpegOutLen == 0) return false;
  *buf = jpegOutBuf;
  *len = jpegOutLen;
  return true;
}

static void handleNotFound()
{
  webUiServer.send(404, "text/plain",
    "Not Found: " + webUiServer.uri());
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initWebUI()
{
  // Allocate the JPEG staging buffer from PSRAM so it doesn't eat internal heap.
  const size_t scaledBytes = (size_t)SNAP_W * SNAP_H * 3;
  scaledBuf = (uint8_t*)heap_caps_malloc(scaledBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (scaledBuf) {
    Debug.printf("scaledBuf: %u B in PSRAM\n", (unsigned)scaledBytes);
  } else {
    // PSRAM unavailable – fall back to internal heap (only feasible at small scales).
    scaledBuf = (uint8_t*)malloc(scaledBytes);
    Debug.printf("scaledBuf: %u B in internal heap (PSRAM unavailable)\n", (unsigned)scaledBytes);
  }

  webUiServer.on("/",             HTTP_GET,  handleRoot);
  webUiServer.on("/setup",        HTTP_GET,  handleSetup);
  webUiServer.on("/trends",       HTTP_GET,  handleTrends);
  webUiServer.on("/history.json", HTTP_GET,  handleHistoryJSON);
  webUiServer.on("/thermal.jpg",    HTTP_GET,  handleThermalJpeg);
  webUiServer.on("/thermalmatrix",  HTTP_GET,  handleThermalMatrix);
  webUiServer.on("/reset_wifi",   HTTP_POST, handleResetWifi);
  webUiServer.on("/reboot",       HTTP_POST, handleReboot);
  webUiServer.on("/save_config",  HTTP_POST, handleSaveConfig);
  webUiServer.onNotFound(handleNotFound);
  webUiServer.begin();
  Debug.println("Web UI server started on port 80");

  wsServer.begin();
  wsServer.onEvent(onWebSocketEvent);
  Debug.println("WebSocket server started on port 81");
}

void handleWebUI()
{
  webUiServer.handleClient();
  wsServer.loop();

  if (millis() - lastJpegUpdate >= (unsigned long)deviceConfig.snapshotIntervalSec * 1000UL) {
    updateThermalSnapshot();
  }
}

// Called after each thermal or brightness read to push live values to the browser.
void broadcastSensorState()
{
  if (wsServer.connectedClients() == 0) return;
  if (!skyConditions.hasData()) return;

  char buf[192];
  snprintf(buf, sizeof(buf),
    "{\"amb\":%.1f,\"cloud_mean\":%.1f,\"cloud_px\":%.1f"
    ",\"lux\":%.6f,\"sqm\":%.2f"
    ",\"has_data\":true,\"has_brightness\":%s}",
    skyConditions.getAmbientTemperature(),
    skyConditions.getCloudCoverMean(),
    skyConditions.getCloudCoverPixel(),
    skyConditions.getLux(),
    skyConditions.getSqm(),
    skyConditions.hasBrightnessData() ? "true" : "false");
  wsServer.broadcastTXT(buf);
}

// Called from rainSensorLoop() whenever the relay state changes (relay mode only).
void broadcastRainState()
{
  if (wsServer.connectedClients() == 0) return;
  String msg = String("{\"rain\":\"") + (rainIsWet() ? "WET" : "DRY") + "\"}";
  wsServer.broadcastTXT(msg);
}

// Called from the main loop after every new sensor frame.
void broadcastThermalFrame()
{
  if (wsServer.connectedClients() == 0) return;
  if (!skyConditions.hasData())         return;

  skyConditions.fillWebSocketBuffer(wsFrameBuffer);
  wsServer.broadcastBIN(wsFrameBuffer, WS_FRAME_SIZE);
}
