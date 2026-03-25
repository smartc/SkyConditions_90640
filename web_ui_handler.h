#ifndef WEB_UI_HANDLER_H
#define WEB_UI_HANDLER_H

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include "config.h"
#include "debug.h"
#include "sky_sensor.h"

// Forward declarations for HTML template functions
String getHomePage();
String getSetupPage();

// Shared server instances (used by main sketch for ElegantOTA)
extern WebServer      webUiServer;
extern WebSocketsServer wsServer;

// Function prototypes
void initWebUI();
void handleWebUI();
void broadcastThermalFrame();
void broadcastSensorState();    // push ambient, cloud cover, lux, SQM as JSON text
void broadcastRainState();      // push relay wet/dry to all WebSocket clients
void updateThermalSnapshot();   // regenerate cached /thermal.jpg

// Returns pointer to the current cached thermal JPEG and its size.
// Returns false if no snapshot is available yet.
bool getThermalJpeg(const uint8_t **buf, size_t *len);

#endif // WEB_UI_HANDLER_H
