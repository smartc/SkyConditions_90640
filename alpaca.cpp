/*
 * alpaca.cpp
 *
 * ASCOM Alpaca ObservingConditions HTTP server (port 11111) and UDP discovery.
 * Uses Arduino WebServer – all handlers run on the main loop; no locking needed.
 *
 * Implemented sensors:  SkyTemperature, Temperature, SkyBrightness, SkyQuality, CloudCover
 * All other ObservingConditions sensors return PropertyNotImplementedException (1035).
 */

#include "alpaca.h"
#include "sky_sensor.h"
#include "debug.h"
#include <ArduinoJson.h>
#include <WiFi.h>

WebServer alpacaServer(ALPACA_PORT);
WiFiUDP   alpacaUdp;

static uint32_t serverTxID = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static String getUniqueID()
{
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[13];
  sprintf(buf, "%02X%02X%02X%02X%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

static void setCommonFields(JsonDocument &json)
{
  uint32_t clientTxID = 0;
  if (alpacaServer.hasArg("ClientTransactionID"))
    clientTxID = alpacaServer.arg("ClientTransactionID").toInt();

  json["ClientTransactionID"] = clientTxID;
  json["ServerTransactionID"] = ++serverTxID;
  if (!json.containsKey("ErrorNumber"))  json["ErrorNumber"]  = 0;
  if (!json.containsKey("ErrorMessage")) json["ErrorMessage"] = "";
}

static void sendJSON(JsonDocument &json)
{
  setCommonFields(json);
  String s;
  serializeJson(json, s);
  alpacaServer.send(200, "application/json", s);
}

static void sendDouble(double value)
{
  StaticJsonDocument<256> json;
  json["Value"] = round(value * 10.0) / 10.0;
  sendJSON(json);
}

static void sendNotImplemented()
{
  StaticJsonDocument<256> json;
  json["ErrorNumber"]  = 1035;  // PropertyNotImplementedException
  json["ErrorMessage"] = "Property not implemented";
  sendJSON(json);
}

static void sendValueNotSet()
{
  StaticJsonDocument<256> json;
  json["ErrorNumber"]  = 1026;  // ValueNotSetException
  json["ErrorMessage"] = "No sensor data available yet";
  sendJSON(json);
}

// ---------------------------------------------------------------------------
// Management API
// ---------------------------------------------------------------------------

static void handleManagementVersions()
{
  StaticJsonDocument<256> json;
  JsonArray arr = json.createNestedArray("Value");
  arr.add(1);
  sendJSON(json);
}

static void handleManagementDevices()
{
  StaticJsonDocument<400> json;
  JsonArray arr    = json.createNestedArray("Value");
  JsonObject dev   = arr.createNestedObject();
  dev["DeviceName"]   = DEVICE_NAME;
  dev["DeviceType"]   = DEVICE_TYPE;
  dev["DeviceNumber"] = DEVICE_NUMBER;
  dev["UniqueID"]     = getUniqueID();
  sendJSON(json);
}

static void handleManagementDescription()
{
  StaticJsonDocument<400> json;
  JsonObject val = json.createNestedObject("Value");
  val["ServerName"]          = SERVER_NAME;
  val["Manufacturer"]        = MANUFACTURER;
  val["ManufacturerVersion"] = MANUFACTURER_V;
  val["Location"]            = LOCATION;
  sendJSON(json);
}

static void handleAlpacaSetup()
{
  String html = "<html><head><title>SkyConditions Setup</title></head>"
                "<body><h1>MLX90640 Sky Conditions Sensor</h1>"
                "<p>ASCOM Alpaca running on port " + String(ALPACA_PORT) + "</p>"
                "<p>IP: " + WiFi.localIP().toString() + "</p>"
                "</body></html>";
  alpacaServer.send(200, "text/html", html);
}

// ---------------------------------------------------------------------------
// Common device methods
// ---------------------------------------------------------------------------

static void handleNotFound()
{
  alpacaServer.send(400, "text/plain", "Not found: " + alpacaServer.uri());
}

static void handleActionNotImpl()
{
  StaticJsonDocument<256> json;
  json["ErrorNumber"]  = 1024;  // NotImplementedException
  json["ErrorMessage"] = "Not implemented";
  sendJSON(json);
}

static void handleGetConnected()
{
  StaticJsonDocument<256> json;
  json["Value"] = skyConditions.getConnected();
  sendJSON(json);
}

static void handlePutConnected()
{
  StaticJsonDocument<256> json;
  if (alpacaServer.hasArg("Connected")) {
    String s = alpacaServer.arg("Connected");
    s.toLowerCase();
    skyConditions.setConnected(s == "true");
    json["Value"] = skyConditions.getConnected();
  } else {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Missing Connected parameter";
  }
  sendJSON(json);
}

static void handleDescription()
{
  StaticJsonDocument<256> json;
  json["Value"] = DESCRIPTION;
  sendJSON(json);
}

static void handleDriverInfo()
{
  StaticJsonDocument<256> json;
  json["Value"] = String(DEVICE_NAME) + " v" + MANUFACTURER_V + " by " + MANUFACTURER;
  sendJSON(json);
}

static void handleDriverVersion()
{
  StaticJsonDocument<256> json;
  json["Value"] = MANUFACTURER_V;
  sendJSON(json);
}

static void handleInterfaceVersion()
{
  StaticJsonDocument<256> json;
  json["Value"] = 1;
  sendJSON(json);
}

static void handleName()
{
  StaticJsonDocument<256> json;
  json["Value"] = DEVICE_NAME;
  sendJSON(json);
}

static void handleSupportedActions()
{
  StaticJsonDocument<256> json;
  json.createNestedArray("Value");
  sendJSON(json);
}

// ---------------------------------------------------------------------------
// ObservingConditions – averageperiod
// ---------------------------------------------------------------------------

static void handleGetAveragePeriod()
{
  sendDouble(skyConditions.getAveragePeriod());
}

static void handlePutAveragePeriod()
{
  StaticJsonDocument<256> json;
  if (alpacaServer.hasArg("AveragePeriod")) {
    double v = alpacaServer.arg("AveragePeriod").toDouble();
    if (!skyConditions.setAveragePeriod(v)) {
      json["ErrorNumber"]  = 1025;
      json["ErrorMessage"] = "AveragePeriod must be >= 0";
    }
  } else {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Missing AveragePeriod parameter";
  }
  sendJSON(json);
}

// ---------------------------------------------------------------------------
// ObservingConditions – implemented sensors
// ---------------------------------------------------------------------------

static void handleGetSkyTemperature()
{
  if (!skyConditions.hasData()) { sendValueNotSet(); return; }
  sendDouble(skyConditions.getSkyTemperature());
}

static void handleGetTemperature()
{
  if (!skyConditions.hasData()) { sendValueNotSet(); return; }
  sendDouble(skyConditions.getAmbientTemperature());
}

static void handleGetCloudCover()
{
  if (!skyConditions.hasData()) { sendValueNotSet(); return; }
  sendDouble(skyConditions.getCloudCover());
}

static void handleGetSkyBrightness()
{
  if (!skyConditions.hasBrightnessData()) { sendValueNotSet(); return; }
  sendDouble(skyConditions.getLux());
}

static void handleGetSkyQuality()
{
  if (!skyConditions.hasBrightnessData()) { sendValueNotSet(); return; }
  sendDouble(skyConditions.getSqm());
}

static void handleGetTimeSinceLastUpdate()
{
  String sensor = alpacaServer.arg("SensorName");
  sensor.toLowerCase();
  if (sensor == "skytemperature" || sensor == "temperature" || sensor == "cloudcover") {
    if (!skyConditions.hasData()) { sendValueNotSet(); return; }
    sendDouble((millis() - skyConditions.getLastUpdateMillis()) / 1000.0);
  } else if (sensor == "skybrightness" || sensor == "skyquality") {
    if (!skyConditions.hasBrightnessData()) { sendValueNotSet(); return; }
    sendDouble((millis() - skyConditions.getLastBrightnessMillis()) / 1000.0);
  } else {
    sendNotImplemented();
  }
}

static void handleGetSensorDescription()
{
  String sensor = alpacaServer.arg("SensorName");
  sensor.toLowerCase();
  StaticJsonDocument<300> json;
  if (sensor == "skytemperature") {
    json["Value"] = "Average temperature of the center 50% of the MLX90640 FOV (192 pixels, 16x12)";
  } else if (sensor == "temperature") {
    json["Value"] = "MLX90640 sensor die/ambient temperature";
  } else if (sensor == "skybrightness") {
    json["Value"] = "TSL2591 broadband visible+IR illuminance in lux";
  } else if (sensor == "skyquality") {
    json["Value"] = "Sky quality in mag/arcsec^2 derived from TSL2591 lux reading";
  } else if (sensor == "cloudcover") {
    json["Value"] = "Cloud cover % estimated from ambient-sky temperature deficit (MLX90640)";
  } else {
    json["ErrorNumber"]  = 1035;
    json["ErrorMessage"] = "Sensor not implemented";
  }
  sendJSON(json);
}

static void handlePutRefresh()
{
  skyConditions.requestRefresh();
  StaticJsonDocument<256> json;
  sendJSON(json);
}

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------

static void handleDeviceBase()
{
  // Convenience response for the bare device URL – not part of the ASCOM spec
  // but handy when browsing to the base URL manually.
  StaticJsonDocument<512> json;
  json["DeviceName"]   = DEVICE_NAME;
  json["DeviceType"]   = DEVICE_TYPE;
  json["DeviceNumber"] = DEVICE_NUMBER;
  json["UniqueID"]     = getUniqueID();
  json["Note"]         = "Append a property name, e.g. /skytemperature";
  sendJSON(json);
}

static void registerRoutes()
{
  alpacaServer.onNotFound(handleNotFound);

  // Management API
  alpacaServer.on("/management/apiversions",          HTTP_GET, handleManagementVersions);
  alpacaServer.on("/management/v1/configureddevices", HTTP_GET, handleManagementDevices);
  alpacaServer.on("/management/v1/description",       HTTP_GET, handleManagementDescription);
  alpacaServer.on("/setup",                           HTTP_GET, handleAlpacaSetup);

  String base = "/api/v1/" + String(DEVICE_TYPE) + "/" + String(DEVICE_NUMBER);

  // Device base path (with and without trailing slash)
  alpacaServer.on(base.c_str(),             HTTP_GET, handleDeviceBase);
  alpacaServer.on((base + "/").c_str(),     HTTP_GET, handleDeviceBase);

  // Common ASCOM methods
  alpacaServer.on((base + "/action").c_str(),           HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((base + "/commandblind").c_str(),     HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((base + "/commandbool").c_str(),      HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((base + "/commandstring").c_str(),    HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((base + "/connected").c_str(),        HTTP_GET, handleGetConnected);
  alpacaServer.on((base + "/connected").c_str(),        HTTP_PUT, handlePutConnected);
  alpacaServer.on((base + "/description").c_str(),      HTTP_GET, handleDescription);
  alpacaServer.on((base + "/driverinfo").c_str(),       HTTP_GET, handleDriverInfo);
  alpacaServer.on((base + "/driverversion").c_str(),    HTTP_GET, handleDriverVersion);
  alpacaServer.on((base + "/interfaceversion").c_str(), HTTP_GET, handleInterfaceVersion);
  alpacaServer.on((base + "/name").c_str(),             HTTP_GET, handleName);
  alpacaServer.on((base + "/supportedactions").c_str(), HTTP_GET, handleSupportedActions);

  // ObservingConditions – averageperiod
  alpacaServer.on((base + "/averageperiod").c_str(),       HTTP_GET, handleGetAveragePeriod);
  alpacaServer.on((base + "/averageperiod").c_str(),       HTTP_PUT, handlePutAveragePeriod);

  // ObservingConditions – implemented sensors
  alpacaServer.on((base + "/skytemperature").c_str(),      HTTP_GET, handleGetSkyTemperature);
  alpacaServer.on((base + "/temperature").c_str(),         HTTP_GET, handleGetTemperature);
  alpacaServer.on((base + "/timesincelastupdate").c_str(), HTTP_GET, handleGetTimeSinceLastUpdate);
  alpacaServer.on((base + "/sensordescription").c_str(),   HTTP_GET, handleGetSensorDescription);
  alpacaServer.on((base + "/refresh").c_str(),             HTTP_PUT, handlePutRefresh);

  // ObservingConditions – TSL2591 brightness sensors
  alpacaServer.on((base + "/skybrightness").c_str(), HTTP_GET, handleGetSkyBrightness);
  alpacaServer.on((base + "/skyquality").c_str(),    HTTP_GET, handleGetSkyQuality);

  // ObservingConditions – not implemented sensors
  alpacaServer.on((base + "/cloudcover").c_str(),    HTTP_GET, handleGetCloudCover);
  alpacaServer.on((base + "/dewpoint").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/humidity").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/pressure").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/rainrate").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/starfwhm").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/winddirection").c_str(), HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/windgust").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/windspeed").c_str(),     HTTP_GET, sendNotImplemented);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void setupAlpacaServer()
{
  registerRoutes();
  alpacaServer.begin();
  Debug.println("ASCOM Alpaca server started on port " + String(ALPACA_PORT));

  alpacaUdp.begin(ALPACA_DISCOVERY_PORT);
  Debug.println("Alpaca discovery started on UDP port " + String(ALPACA_DISCOVERY_PORT));
}

// ---------------------------------------------------------------------------
// UDP Discovery  (call from main loop)
// ---------------------------------------------------------------------------

void handleAlpacaDiscovery()
{
  int packetSize = alpacaUdp.parsePacket();
  if (!packetSize) return;

  char buf[packetSize + 1];
  alpacaUdp.read(buf, packetSize);
  buf[packetSize] = '\0';

  String s = String(buf);
  s.trim();
  if (s == "alpacadiscovery1") {
    String resp = "{\"AlpacaPort\":" + String(ALPACA_PORT) + "}";
    alpacaUdp.beginPacket(alpacaUdp.remoteIP(), alpacaUdp.remotePort());
    alpacaUdp.print(resp);
    alpacaUdp.endPacket();
    Debug.println("Replied to Alpaca discovery from " + alpacaUdp.remoteIP().toString());
  }
}
