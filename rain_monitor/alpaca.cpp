#include "alpaca.h"
#include "debug.h"

void setupAlpacaServer() {
  // Start the WebServer
  alpacaServer.begin();
  Debug.println("HTTP alpaca server started on port " + String(ALPACA_PORT));
  
  // Set connected state to true by default
  isConnected = true;
  Debug.println("Device connected state initialized to true");
  
  // Start the UDP server for discovery
  udp.begin(ALPACA_DISCOVERY);
  Debug.println("Alpaca discovery service started on port " + String(ALPACA_DISCOVERY));
}

// Debug function to print all registered handlers
void printAllRegisteredHandlers() {
  Debug.println("Registered URI handlers (this may not work with WebServer library):");
  Debug.println("Listening on:");
  Debug.print("http://");
  Debug.print(WiFi.localIP().toString());
  Debug.print(":");
  Debug.println(ALPACA_PORT);
  Debug.println("/api/v1/safetymonitor/0/issafe - should return safety status");
}

void setupAlpacaRoutes() {
  // Common device endpoints - use a C string for safety
  String baseUrlStr = "/api/v1/" + String(DEVICE_TYPE) + "/" + String(DEVICE_NUMBER);
  const char* baseUrl = baseUrlStr.c_str();
  
  // Print debug info about routes being set up
  Debug.println("Setting up Alpaca routes");
  Debug.print("Base URL: ");
  Debug.println(baseUrlStr);
  
  // General handler for 404s
  alpacaServer.onNotFound(handleNotFound);
  
  // Mandatory Common Methods
  alpacaServer.on((baseUrlStr + "/action").c_str(), HTTP_PUT, handleAction);
  alpacaServer.on((baseUrlStr + "/commandblind").c_str(), HTTP_PUT, handleCommandBlind);
  alpacaServer.on((baseUrlStr + "/commandbool").c_str(), HTTP_PUT, handleCommandBool);
  alpacaServer.on((baseUrlStr + "/commandstring").c_str(), HTTP_PUT, handleCommandString);
  alpacaServer.on((baseUrlStr + "/connected").c_str(), HTTP_GET, handleConnected);
  alpacaServer.on((baseUrlStr + "/connected").c_str(), HTTP_PUT, handleConnectedPut);
  alpacaServer.on((baseUrlStr + "/description").c_str(), HTTP_GET, handleDescription);
  alpacaServer.on((baseUrlStr + "/driverinfo").c_str(), HTTP_GET, handleDriverInfo);
  alpacaServer.on((baseUrlStr + "/driverversion").c_str(), HTTP_GET, handleDriverVersion);
  alpacaServer.on((baseUrlStr + "/interfaceversion").c_str(), HTTP_GET, handleInterfaceVersion);
  alpacaServer.on((baseUrlStr + "/name").c_str(), HTTP_GET, handleName);
  alpacaServer.on((baseUrlStr + "/supportedactions").c_str(), HTTP_GET, handleSupportedActions);
  
  // SafetyMonitor specific methods
  Debug.println("Registering SafetyMonitor specific methods");
  alpacaServer.on((baseUrlStr + "/issafe").c_str(), HTTP_GET, handleIsSafe);
  
  // Management API
  alpacaServer.on("/management/apiversions", HTTP_GET, managementAPIVersions);
  alpacaServer.on("/management/v1/configureddevices", HTTP_GET, managementConfiguredDevices);
  alpacaServer.on("/management/v1/description", HTTP_GET, managementDescription);
  alpacaServer.on("/setup", HTTP_GET, managementSetup);
}

// Helper Functions
void setCommonJSONFields(JsonDocument &json) {
  // Get client ID and transaction ID if available
  uint32_t clientID = 0;
  uint32_t clientTransactionID = 0;
  
  if (alpacaServer.hasArg("ClientID")) {
    clientID = alpacaServer.arg("ClientID").toInt();
  }
  
  if (alpacaServer.hasArg("ClientTransactionID")) {
    clientTransactionID = alpacaServer.arg("ClientTransactionID").toInt();
  }
  
  // Set client and server transaction IDs
  json["ClientTransactionID"] = clientTransactionID;
  json["ServerTransactionID"] = ++serverTransactionID;
  
  // Set default success values
  if (!json.containsKey("ErrorNumber")) {
    json["ErrorNumber"] = 0;
  }
  if (!json.containsKey("ErrorMessage")) {
    json["ErrorMessage"] = "";
  }
}

void sendJSONResponse(JsonDocument &json) {
  String response;
  serializeJson(json, response);
  alpacaServer.send(200, "application/json", response);
}

// Request Handlers
void handleNotFound() {
  alpacaServer.send(400, "text/plain", "Not found");
}

// Common Method Handlers
void handleAction() {
  StaticJsonDocument<256> json;
  
  // Not implemented
  json["ErrorNumber"] = 0x400; // 1024 - Not implemented
  json["ErrorMessage"] = "Action not implemented";
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleCommandBlind() {
  StaticJsonDocument<256> json;
  
  // Not implemented
  json["ErrorNumber"] = 0x400; // 1024 - Not implemented
  json["ErrorMessage"] = "CommandBlind not implemented";
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleCommandBool() {
  StaticJsonDocument<256> json;
  
  // Not implemented
  json["ErrorNumber"] = 0x400; // 1024 - Not implemented
  json["ErrorMessage"] = "CommandBool not implemented";
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleCommandString() {
  StaticJsonDocument<256> json;
  
  // Not implemented
  json["ErrorNumber"] = 0x400; // 1024 - Not implemented
  json["ErrorMessage"] = "CommandString not implemented";
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleConnected() {
  StaticJsonDocument<256> json;
  json["Value"] = isConnected;
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleConnectedPut() {
  StaticJsonDocument<256> json;
  
  // Check if the Connected parameter exists
  if (alpacaServer.hasArg("Connected")) {
    String connectedStr = alpacaServer.arg("Connected");
    connectedStr.toLowerCase();
    isConnected = (connectedStr == "true");
    json["Value"] = isConnected;
  } else {
    json["ErrorNumber"] = 0x401; // 1025 - Invalid value
    json["ErrorMessage"] = "Missing Connected parameter";
  }
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleDescription() {
  StaticJsonDocument<256> json;
  json["Value"] = DESCRIPTION;
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleDriverInfo() {
  StaticJsonDocument<256> json;
  json["Value"] = String(DEVICE_NAME) + " " + String(MANUFACTURER_V);
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleDriverVersion() {
  StaticJsonDocument<256> json;
  json["Value"] = MANUFACTURER_V;
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleInterfaceVersion() {
  StaticJsonDocument<256> json;
  json["Value"] = 1;  // ASCOM SafetyMonitor interface version
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleName() {
  StaticJsonDocument<256> json;
  json["Value"] = DEVICE_NAME;
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

void handleSupportedActions() {
  StaticJsonDocument<256> json;
  JsonArray actions = json.createNestedArray("Value");
  // No supported actions
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

// SafetyMonitor specific method
void handleIsSafe() {
  StaticJsonDocument<256> json;
  
  // According to ASCOM standard, must report unsafe (false) when not connected
  if (!isConnected) {
    json["Value"] = false;  // Always report unsafe when not connected
    Debug.println("IsSafe check when not connected - reporting false");
  } else {
    json["Value"] = !isRaining;  // Safe when not raining and connected
    Debug.print("IsSafe check while connected - reporting ");
    Debug.println(!isRaining ? "true (safe)" : "false (unsafe)");
  }
  
  setCommonJSONFields(json);
  sendJSONResponse(json);
}

// Management API Methods
void managementAPIVersions() {
  StaticJsonDocument<256> json;
  
  JsonArray valueArray = json.createNestedArray("Value");
  valueArray.add(1);
  
  uint32_t clientTransactionID = 0;
  if (alpacaServer.hasArg("ClientTransactionID")) {
    clientTransactionID = alpacaServer.arg("ClientTransactionID").toInt();
  }
  
  json["ClientTransactionID"] = clientTransactionID;
  json["ServerTransactionID"] = ++serverTransactionID;
  
  String response;
  serializeJson(json, response);
  alpacaServer.send(200, "application/json", response);
}

void managementConfiguredDevices() {
  StaticJsonDocument<400> json;
  
  JsonArray valueArray = json.createNestedArray("Value");
  JsonObject device = valueArray.createNestedObject();
  device["DeviceName"] = DEVICE_NAME;
  device["DeviceType"] = DEVICE_TYPE;
  device["DeviceNumber"] = DEVICE_NUMBER;
  device["UniqueID"] = getUniqueID();
  
  uint32_t clientTransactionID = 0;
  if (alpacaServer.hasArg("ClientTransactionID")) {
    clientTransactionID = alpacaServer.arg("ClientTransactionID").toInt();
  }
  
  json["ClientTransactionID"] = clientTransactionID;
  json["ServerTransactionID"] = ++serverTransactionID;
  
  String response;
  serializeJson(json, response);
  alpacaServer.send(200, "application/json", response);
}

void managementDescription() {
  StaticJsonDocument<400> json;
  
  JsonObject valueObject = json.createNestedObject("Value");
  valueObject["ServerName"] = SERVER_NAME;
  valueObject["Manufacturer"] = MANUFACTURER;
  valueObject["ManufacturerVersion"] = MANUFACTURER_V;
  valueObject["Location"] = LOCATION;
  
  uint32_t clientTransactionID = 0;
  if (alpacaServer.hasArg("ClientTransactionID")) {
    clientTransactionID = alpacaServer.arg("ClientTransactionID").toInt();
  }
  
  json["ClientTransactionID"] = clientTransactionID;
  json["ServerTransactionID"] = ++serverTransactionID;
  
  String response;
  serializeJson(json, response);
  alpacaServer.send(200, "application/json", response);
}

// Simple HTML setup page
void managementSetup() {
  String html = "<html><head><title>Rain Sensor Setup</title></head>";
  html += "<body><h1>Rain Sensor Safety Monitor</h1>";
  html += "<p>Device is running and configured for Alpaca on port " + String(ALPACA_PORT) + "</p>";
  html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
  html += "</body></html>";
  
  alpacaServer.send(200, "text/html", html);
}

// Alpaca Discovery
void handleAlpacaDiscovery() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[packetSize + 1];
    udp.read(incomingPacket, packetSize);
    incomingPacket[packetSize] = 0; // Null-terminate the string
    
    String incomingString = String(incomingPacket);
    incomingString.trim();
    
    // Check if this is an Alpaca Discovery message
    if (incomingString == "alpacadiscovery1") {
      String response = "{\"AlpacaPort\":" + String(ALPACA_PORT) + "}";
      
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.print(response);
      udp.endPacket();
      
      Debug.println("Responding to Alpaca discovery from " + udp.remoteIP().toString() + ":" + String(udp.remotePort()));
    }
  }
}

// Helper Functions
String getUniqueID() {
  // Generate unique ID based on MAC address for ESP32
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  char uniqueID[13]; // 12 characters + null terminator
  sprintf(uniqueID, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  return String(uniqueID);
}