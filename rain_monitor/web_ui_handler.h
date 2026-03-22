#ifndef WEB_UI_HANDLER_H
#define WEB_UI_HANDLER_H

#include <WebServer.h>
#include <WiFiManager.h>
#include "config.h"
#include "debug.h"

// Forward declarations for html_templates.h functions
String getHomePage(bool isRaining, int rainLevel);
String getSetupPage();

// External references
extern WebServer webUiServer;
extern bool isConnected;
extern bool isRaining;
extern bool isActuallyRaining;
extern int rainLevel;
extern bool useAnalogMode;
extern int analogThreshold;
extern int holdTimeMinutes;
extern void saveSettings();

// Function prototypes
void initWebUI();
void handleWebUI();
void handleRoot();
void handleSetup();
void handleWebUINotFound();
void handleSetupPost();
void handleResetWifi();

#endif // WEB_UI_HANDLER_H