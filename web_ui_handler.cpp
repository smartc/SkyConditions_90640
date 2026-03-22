/*
 * Web UI handler – HTTP server (port 80) and WebSocket server (port 81)
 *
 * Port 80:  Human-readable status and setup pages.
 * Port 81:  Binary WebSocket stream of thermal frames (784-byte frames at 2 Hz).
 *           Frame layout is defined in config.h (WS_FRAME_SIZE / WS_HEADER_SIZE).
 */

#include "web_ui_handler.h"
#include "html_templates.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <img_converters.h>

WebServer        webUiServer(80);
WebSocketsServer wsServer(81);

static uint8_t wsFrameBuffer[WS_FRAME_SIZE];

// Cached JPEG snapshot for /thermal.jpg
static uint8_t*      jpegBuf        = nullptr;
static size_t        jpegLen        = 0;
static unsigned long lastJpegUpdate = 0;

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
// JPEG snapshot
// ---------------------------------------------------------------------------

void updateThermalSnapshot()
{
  if (!skyConditions.hasData()) return;

  uint8_t rgb[SENSOR_PIXELS * 3];
  skyConditions.fillRGBBuffer(rgb);

  uint8_t* newBuf = nullptr;
  size_t   newLen = 0;
  if (fmt2jpg(rgb, sizeof(rgb), SENSOR_COLS, SENSOR_ROWS,
              PIXFORMAT_RGB888, 80, &newBuf, &newLen)) {
    if (jpegBuf) free(jpegBuf);
    jpegBuf        = newBuf;
    jpegLen        = newLen;
    lastJpegUpdate = millis();
  }
}

static void handleThermalJpeg()
{
  if (!jpegBuf || jpegLen == 0) {
    webUiServer.send(503, "text/plain", "No thermal data yet");
    return;
  }
  webUiServer.send_P(200, "image/jpeg", (const char*)jpegBuf, jpegLen);
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
  webUiServer.on("/",            HTTP_GET,  handleRoot);
  webUiServer.on("/setup",       HTTP_GET,  handleSetup);
  webUiServer.on("/thermal.jpg", HTTP_GET,  handleThermalJpeg);
  webUiServer.on("/reset_wifi",  HTTP_POST, handleResetWifi);
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

  if (millis() - lastJpegUpdate >= JPEG_INTERVAL_MS) {
    updateThermalSnapshot();
  }
}

// Called from the main loop after every new sensor frame.
void broadcastThermalFrame()
{
  if (wsServer.connectedClients() == 0) return;
  if (!skyConditions.hasData())         return;

  skyConditions.fillWebSocketBuffer(wsFrameBuffer);
  wsServer.broadcastBIN(wsFrameBuffer, WS_FRAME_SIZE);
}
