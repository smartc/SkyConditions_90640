#ifndef ALPACA_H
#define ALPACA_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "config.h"

// Global variables
extern bool isConnected;
extern bool isRaining;
extern int rainLevel;
extern uint32_t serverTransactionID;
extern WebServer alpacaServer;
extern WiFiUDP udp;

// Function declarations
void setupAlpacaServer();
void setupAlpacaRoutes();
void handleAlpacaDiscovery();
String getUniqueID();

// Common device endpoints
void handleNotFound();
void handleAction();
void handleCommandBlind();
void handleCommandBool();
void handleCommandString();
void handleConnected();
void handleConnectedPut();
void handleDescription();
void handleDriverInfo();
void handleDriverVersion();
void handleInterfaceVersion();
void handleName();
void handleSupportedActions();

// SafetyMonitor specific method
void handleIsSafe();

// Management API Methods
void managementAPIVersions();
void managementConfiguredDevices();
void managementDescription();
void managementSetup();

// Helper functions
void setCommonJSONFields(JsonDocument &json);
void sendJSONResponse(JsonDocument &json);

#endif // ALPACA_H