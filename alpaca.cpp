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
#include "config_store.h"
#include "debug.h"
#include <ArduinoJson.h>
#include <WiFi.h>

WebServer alpacaServer(ALPACA_PORT);
WiFiUDP   alpacaUdp;

static uint32_t serverTxID = 0;

// ---------------------------------------------------------------------------
// Case-insensitive query-parameter helpers
// (Alpaca spec requires servers to accept parameters regardless of casing.)
// ---------------------------------------------------------------------------

static bool hasArgCI(const String& name)
{
  String lower = name; lower.toLowerCase();
  for (int i = 0; i < alpacaServer.args(); i++) {
    String n = alpacaServer.argName(i); n.toLowerCase();
    if (n == lower) return true;
  }
  return false;
}

static String getArgCI(const String& name)
{
  String lower = name; lower.toLowerCase();
  for (int i = 0; i < alpacaServer.args(); i++) {
    String n = alpacaServer.argName(i); n.toLowerCase();
    if (n == lower) return alpacaServer.arg(i);
  }
  return "";
}

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
  if (hasArgCI("ClientTransactionID")) {
    long raw = getArgCI("ClientTransactionID").toInt();
    if (raw > 0) clientTxID = (uint32_t)raw;
    // negative or non-numeric: leave as 0 per Alpaca spec
  }

  json["ClientTransactionID"] = clientTxID;
  json["ServerTransactionID"] = ++serverTxID;
  if (!json.containsKey("ErrorNumber"))  json["ErrorNumber"]  = 0;
  if (!json.containsKey("ErrorMessage")) json["ErrorMessage"] = "";
}

static void sendJSONStatus(JsonDocument &json, int httpStatus)
{
  setCommonFields(json);
  String s;
  serializeJson(json, s);
  alpacaServer.send(httpStatus, "application/json", s);
}

static void sendJSON(JsonDocument &json)
{
  sendJSONStatus(json, 200);
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
  json["ErrorNumber"]  = 1024;  // NotImplementedException – tells Conform not to retry
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
  val["Location"]            = deviceConfig.location;
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
  StaticJsonDocument<256> json;
  json["ErrorNumber"]  = 1024;
  json["ErrorMessage"] = "Not found: " + alpacaServer.uri();
  setCommonFields(json);
  String s; serializeJson(json, s);
  alpacaServer.send(400, "application/json", s);
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
  if (!hasArgCI("Connected")) {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Missing Connected parameter";
    sendJSONStatus(json, 400);
    return;
  }
  String s = getArgCI("Connected");
  String sl = s; sl.toLowerCase();
  if (sl != "true" && sl != "false") {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Connected value must be 'true' or 'false'";
    sendJSONStatus(json, 400);
    return;
  }
  skyConditions.setConnected(sl == "true");
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
  // Parameter name is case-sensitive per Alpaca spec (only ClientID/ClientTransactionID are not).
  if (!alpacaServer.hasArg("AveragePeriod")) {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Missing AveragePeriod parameter";
    sendJSONStatus(json, 400);
    return;
  }
  // Validate the parameter is actually a numeric string before parsing.
  // toDouble() silently returns 0.0 for non-numeric strings, masking malformed requests.
  // Syntactically malformed (non-numeric) → HTTP 400; semantically invalid (negative) → HTTP 200 + error.
  String valStr = alpacaServer.arg("AveragePeriod");
  valStr.trim();
  bool isNumeric = valStr.length() > 0;
  if (isNumeric) {
    int start = (valStr[0] == '-' || valStr[0] == '+') ? 1 : 0;
    if (start >= (int)valStr.length()) isNumeric = false;
    bool hasDot = false;
    for (int i = start; i < (int)valStr.length() && isNumeric; i++) {
      if (valStr[i] == '.' && !hasDot) hasDot = true;
      else if (!isDigit(valStr[i]))    isNumeric = false;
    }
  }
  if (!isNumeric) {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "AveragePeriod value is not a valid number";
    sendJSONStatus(json, 400);
    return;
  }
  double v = valStr.toDouble();
  if (v < 0.0) {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "AveragePeriod must be >= 0";
    sendJSON(json);  // HTTP 200 + error code per Alpaca spec (semantic error, not malformed request)
    return;
  }
  skyConditions.setAveragePeriod(v);
  deviceConfig.averagePeriod = v;
  configSave(deviceConfig);
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

// Full set of valid ObservingConditions sensor names (lowercase).
// SensorName "" means "all sensors" and is also valid.
static bool isValidSensorName(const String& nameLower)
{
  return nameLower == ""             || nameLower == "cloudcover"    ||
         nameLower == "dewpoint"     || nameLower == "humidity"      ||
         nameLower == "pressure"     || nameLower == "rainrate"      ||
         nameLower == "skybrightness"|| nameLower == "skyquality"    ||
         nameLower == "skytemperature"|| nameLower == "starfwhm"     ||
         nameLower == "temperature"  || nameLower == "winddirection"  ||
         nameLower == "windgust"     || nameLower == "windspeed";
}

static void handleGetTimeSinceLastUpdate()
{
  String sensor = alpacaServer.arg("SensorName");
  String sensorLow = sensor; sensorLow.toLowerCase();

  if (!isValidSensorName(sensorLow)) {
    StaticJsonDocument<256> json;
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Unknown SensorName: " + sensor;
    sendJSONStatus(json, 400);
    return;
  }

  if (sensorLow == "") {
    // Empty SensorName = LatestUpdateTime: time since the most recent update across ALL sensors.
    if (!skyConditions.hasData() && !skyConditions.hasBrightnessData()) { sendValueNotSet(); return; }
    unsigned long mostRecent = skyConditions.hasData() ? skyConditions.getLastUpdateMillis() : 0;
    if (skyConditions.hasBrightnessData()) {
      unsigned long bMillis = skyConditions.getLastBrightnessMillis();
      if (bMillis > mostRecent) mostRecent = bMillis;
    }
    sendDouble((millis() - mostRecent) / 1000.0);
  } else if (sensorLow == "skytemperature" || sensorLow == "temperature" || sensorLow == "cloudcover") {
    if (!skyConditions.hasData()) { sendValueNotSet(); return; }
    sendDouble((millis() - skyConditions.getLastUpdateMillis()) / 1000.0);
  } else if (sensorLow == "skybrightness" || sensorLow == "skyquality") {
    if (!skyConditions.hasBrightnessData()) { sendValueNotSet(); return; }
    sendDouble((millis() - skyConditions.getLastBrightnessMillis()) / 1000.0);
  } else {
    sendNotImplemented();
  }
}

static void handleGetSensorDescription()
{
  String sensor = alpacaServer.arg("SensorName");
  String sensorLow = sensor; sensorLow.toLowerCase();

  if (!isValidSensorName(sensorLow)) {
    StaticJsonDocument<256> json;
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Unknown SensorName: " + sensor;
    sendJSONStatus(json, 400);
    return;
  }

  StaticJsonDocument<300> json;
  if (sensorLow == "skytemperature") {
    json["Value"] = "Average temperature of the center 50% of the MLX90640 FOV (192 pixels, 16x12)";
  } else if (sensorLow == "temperature") {
    json["Value"] = "MLX90640 sensor die/ambient temperature";
  } else if (sensorLow == "skybrightness") {
    json["Value"] = "TSL2591 broadband visible+IR illuminance in lux";
  } else if (sensorLow == "skyquality") {
    json["Value"] = "Sky quality in mag/arcsec^2 derived from TSL2591 lux reading";
  } else if (sensorLow == "cloudcover") {
    json["Value"] = "Cloud cover % estimated from ambient-sky temperature deficit (MLX90640)";
  } else {
    json["ErrorNumber"]  = 1024;  // NotImplementedException – tells Conform not to retry
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
// UDP Discovery – FreeRTOS task on Core 0 (WiFi core)
//
// Runs independently of the main loop so that blocking sensor reads
// (mlx.getFrame() can hold Core 1 for ~500 ms) do not delay the response.
// Uses its own WiFiUDP object to avoid any shared-state issues.
// ---------------------------------------------------------------------------

static void discoveryTask(void *)
{
  WiFiUDP udp;
  udp.begin(ALPACA_DISCOVERY_PORT);
  Debug.println("[Discovery] task started on Core " + String(xPortGetCoreID()));

  for (;;) {
    int pktSize = udp.parsePacket();
    if (pktSize > 0) {
      char buf[64];
      int n = udp.read(buf, sizeof(buf) - 1);
      if (n > 0) {
        buf[n] = '\0';
        String s = String(buf);
        s.trim();
        if (s.equalsIgnoreCase("alpacadiscovery1")) {
          String resp = "{\"AlpacaPort\":" + String(ALPACA_PORT) + "}";
          udp.beginPacket(udp.remoteIP(), udp.remotePort());
          udp.print(resp);
          int ok = udp.endPacket();
          Debug.printf("[Discovery] replied to %s (ok=%d)\n",
                       udp.remoteIP().toString().c_str(), ok);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // 10 ms poll – fast enough, not a busy-wait
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void setupAlpacaServer()
{
  // Restore persisted ASCOM state.
  skyConditions.setAveragePeriod(deviceConfig.averagePeriod);

  registerRoutes();
  alpacaServer.begin();
  Debug.println("ASCOM Alpaca server started on port " + String(ALPACA_PORT));

  // Pin the discovery task to Core 0 alongside the WiFi stack so it responds
  // immediately regardless of what Core 1 (the Arduino loop) is doing.
  xTaskCreatePinnedToCore(discoveryTask, "alpaca_disc",
                          2048, nullptr, 1, nullptr, 0);
}

// No-op: discovery is now handled by the FreeRTOS task above.
void handleAlpacaDiscovery() {}
