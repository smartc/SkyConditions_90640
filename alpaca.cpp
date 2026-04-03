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
#include "rain_sensor.h"
#include "dht_sensor.h"
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
  StaticJsonDocument<900> json;
  JsonArray arr = json.createNestedArray("Value");

  JsonObject oc = arr.createNestedObject();
  oc["DeviceName"]   = DEVICE_NAME;
  oc["DeviceType"]   = DEVICE_TYPE;
  oc["DeviceNumber"] = DEVICE_NUMBER;
  oc["UniqueID"]     = getUniqueID();

  if (deviceConfig.rainEnabled) {
    JsonObject sm = arr.createNestedObject();
    sm["DeviceName"]   = "Rain/Snow Safety Monitor";
    sm["DeviceType"]   = "safetymonitor";
    sm["DeviceNumber"] = 0;
    sm["UniqueID"]     = getUniqueID() + "-SM";
  }

  {
    JsonObject sw = arr.createNestedObject();
    sw["DeviceName"]   = "Cloud Cover Switch";
    sw["DeviceType"]   = "switch";
    sw["DeviceNumber"] = 0;
    sw["UniqueID"]     = getUniqueID() + "-SW";
  }

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

// Redirect requests from the Alpaca port (11111) to the Web UI port (80).
// The Alpaca spec requires /setup endpoints to return HTML; a 302 satisfies that.
static void redirectToRoot()
{
  String url = "http://" + WiFi.localIP().toString() + "/";
  alpacaServer.sendHeader("Location", url, true);
  alpacaServer.send(302, "text/plain", "");
}

static void redirectToSetup()
{
  String url = "http://" + WiFi.localIP().toString() + "/setup";
  alpacaServer.sendHeader("Location", url, true);
  alpacaServer.send(302, "text/plain", "");
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

static void handleGetHumidity()
{
  // Humidity from DHT11/DHT22 (types 1-2) or BME280 (type 5); BMP180/BMP280 have no humidity
  bool hasHumidity = ((deviceConfig.dhtType >= 1 && deviceConfig.dhtType <= 2) || deviceConfig.dhtType == 5);
  if (!hasHumidity || !dhtData.valid) { sendNotImplemented(); return; }
  StaticJsonDocument<256> json;
  json["Value"] = round(dhtData.humidity * 10.0) / 10.0;
  sendJSON(json);
}

static void handlePutRefresh()
{
  skyConditions.requestRefresh();
  StaticJsonDocument<256> json;
  sendJSON(json);
}

// ---------------------------------------------------------------------------
// SafetyMonitor device – rain/snow relay
// ---------------------------------------------------------------------------

static bool smConnected = false;

static void handleSmGetConnected()
{
  StaticJsonDocument<256> json;
  json["Value"] = smConnected;
  sendJSON(json);
}

static void handleSmPutConnected()
{
  StaticJsonDocument<256> json;
  if (!hasArgCI("Connected")) {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Missing Connected parameter";
    sendJSONStatus(json, 400);
    return;
  }
  String sl = getArgCI("Connected"); sl.toLowerCase();
  if (sl != "true" && sl != "false") {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Connected value must be 'true' or 'false'";
    sendJSONStatus(json, 400);
    return;
  }
  smConnected = (sl == "true");
  sendJSON(json);
}

static void handleSmIsSafe()
{
  StaticJsonDocument<256> json;
  // true = safe (dry), false = unsafe (wet).
  json["Value"] = !rainIsWet();
  sendJSON(json);
}

static void handleSmName()
{
  StaticJsonDocument<256> json;
  json["Value"] = "Rain/Snow Safety Monitor";
  sendJSON(json);
}

static void handleSmDescription()
{
  StaticJsonDocument<256> json;
  json["Value"] = "Rain and snow safety monitor via relay contact sensor";
  sendJSON(json);
}

static void handleSmDriverInfo()
{
  StaticJsonDocument<256> json;
  json["Value"] = String("Rain/Snow Safety Monitor v") + MANUFACTURER_V + " by " + MANUFACTURER;
  sendJSON(json);
}

static void handleSmInterfaceVersion()
{
  StaticJsonDocument<256> json;
  json["Value"] = 1;
  sendJSON(json);
}

// ---------------------------------------------------------------------------
// Switch device – cloud cover indicator (read-only, ISwitchV2)
//
// One switch, Id=0: Cloud Cover
//   GetSwitchValue → cloud cover % (0–100, analog)
//   GetSwitch      → true when cloud cover ≥ 50 %
//   CanWrite       → false (indicator only)
//   Min=0, Max=100, Step=1
// ---------------------------------------------------------------------------

static bool swConnected = false;

// Validates the 'id' parameter (case-insensitive).  Sends error + returns false if invalid.
// Non-numeric Id → HTTP 400 (protocol/syntactic error per Alpaca spec).
// Out-of-range integer Id → HTTP 200 + ErrorNumber 1025 (ASCOM semantic error).
static bool switchIdOk()
{
  if (!hasArgCI("id")) {
    StaticJsonDocument<256> json;
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Missing Id parameter";
    sendJSONStatus(json, 400);
    return false;
  }
  String raw = getArgCI("id");
  // Validate it is a pure integer (optional leading minus + digits only).
  int start = (raw.length() > 0 && raw[0] == '-') ? 1 : 0;
  bool numeric = (raw.length() > (unsigned)start);
  for (int i = start; i < (int)raw.length() && numeric; i++) {
    if (!isdigit((unsigned char)raw[i])) numeric = false;
  }
  if (!numeric) {
    StaticJsonDocument<256> json;
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Id parameter must be an integer";
    sendJSONStatus(json, 400);
    return false;
  }
  int id = raw.toInt();
  if (id < 0 || id >= 1) {
    // Semantically out of range → HTTP 200 with ASCOM error 1025.
    StaticJsonDocument<256> json;
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Invalid switch Id: must be 0";
    sendJSON(json);
    return false;
  }
  return true;
}

static void handleSwGetConnected()
{
  StaticJsonDocument<256> json;
  json["Value"] = swConnected;
  sendJSON(json);
}

static void handleSwPutConnected()
{
  StaticJsonDocument<256> json;
  if (!hasArgCI("Connected")) {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Missing Connected parameter";
    sendJSONStatus(json, 400);
    return;
  }
  String sl = getArgCI("Connected"); sl.toLowerCase();
  if (sl != "true" && sl != "false") {
    json["ErrorNumber"]  = 1025;
    json["ErrorMessage"] = "Connected value must be 'true' or 'false'";
    sendJSONStatus(json, 400);
    return;
  }
  swConnected = (sl == "true");
  sendJSON(json);
}

static void handleSwMaxSwitch()
{
  StaticJsonDocument<256> json;
  json["Value"] = 1;
  sendJSON(json);
}

static void handleSwCanWrite()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<256> json;
  json["Value"] = false;
  sendJSON(json);
}

static void handleSwGetSwitch()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<256> json;
  // Must always return a value per spec; return false when no data yet.
  json["Value"] = skyConditions.hasData() && (skyConditions.getCloudCover() >= 50.0f);
  sendJSON(json);
}

static void handleSwGetSwitchDescription()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<300> json;
  json["Value"] = "Cloud cover percentage (0-100%). GetSwitch is true when cover >= 50%.";
  sendJSON(json);
}

static void handleSwGetSwitchName()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<256> json;
  json["Value"] = "Cloud Cover";
  sendJSON(json);
}

static void handleSwGetSwitchValue()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<256> json;
  // Must always return a value per spec; return 0.0 when no data yet.
  json["Value"] = skyConditions.hasData()
                  ? (double)(round(skyConditions.getCloudCover() * 10.0) / 10.0)
                  : 0.0;
  sendJSON(json);
}

static void handleSwMinSwitchValue()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<256> json;
  json["Value"] = 0.0;
  sendJSON(json);
}

static void handleSwMaxSwitchValue()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<256> json;
  json["Value"] = 100.0;
  sendJSON(json);
}

static void handleSwSwitchStep()
{
  if (!switchIdOk()) return;
  StaticJsonDocument<256> json;
  json["Value"] = 1.0;
  sendJSON(json);
}

static void handleSwReadOnly()
{
  // Validate Id first (1025 for invalid), then return 1024 for valid-but-read-only.
  if (!switchIdOk()) return;
  sendNotImplemented();
}

static void handleSwName()
{
  StaticJsonDocument<256> json;
  json["Value"] = "Cloud Cover Switch";
  sendJSON(json);
}

static void handleSwDescription()
{
  StaticJsonDocument<256> json;
  json["Value"] = "Read-only cloud cover indicator derived from MLX90640 thermal camera";
  sendJSON(json);
}

static void handleSwDriverInfo()
{
  StaticJsonDocument<256> json;
  json["Value"] = String("Cloud Cover Switch v") + MANUFACTURER_V + " by " + MANUFACTURER;
  sendJSON(json);
}

static void handleSwInterfaceVersion()
{
  StaticJsonDocument<256> json;
  json["Value"] = 2;  // ISwitchV2
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
  // Redirect setup/UI paths on the Alpaca port to the Web UI on port 80.
  alpacaServer.on("/",      HTTP_GET, redirectToRoot);
  alpacaServer.on("/setup", HTTP_GET, redirectToSetup);
  // ASCOM Alpaca device setup URLs: /setup/v1/<type>/<n>/setup
  alpacaServer.on("/setup/v1/observingconditions/0/setup", HTTP_GET, redirectToSetup);
  alpacaServer.on("/setup/v1/safetymonitor/0/setup",       HTTP_GET, redirectToSetup);
  alpacaServer.on("/setup/v1/switch/0/setup",              HTTP_GET, redirectToSetup);

  String base = "/api/v1/" + String(DEVICE_TYPE) + "/" + String(DEVICE_NUMBER);

  // Device base path (with and without trailing slash)
  alpacaServer.on(base.c_str(),              HTTP_GET, handleDeviceBase);
  alpacaServer.on((base + "/").c_str(),      HTTP_GET, handleDeviceBase);
  alpacaServer.on((base + "/setup").c_str(), HTTP_GET, redirectToSetup);

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
  alpacaServer.on((base + "/humidity").c_str(),      HTTP_GET, handleGetHumidity);
  alpacaServer.on((base + "/pressure").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/rainrate").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/starfwhm").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/winddirection").c_str(), HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/windgust").c_str(),      HTTP_GET, sendNotImplemented);
  alpacaServer.on((base + "/windspeed").c_str(),     HTTP_GET, sendNotImplemented);

  // SafetyMonitor device
  String sm = "/api/v1/safetymonitor/0";
  alpacaServer.on((sm + "/action").c_str(),           HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sm + "/commandblind").c_str(),     HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sm + "/commandbool").c_str(),      HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sm + "/commandstring").c_str(),    HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sm + "/connected").c_str(),        HTTP_GET, handleSmGetConnected);
  alpacaServer.on((sm + "/connected").c_str(),        HTTP_PUT, handleSmPutConnected);
  alpacaServer.on((sm + "/description").c_str(),      HTTP_GET, handleSmDescription);
  alpacaServer.on((sm + "/driverinfo").c_str(),       HTTP_GET, handleSmDriverInfo);
  alpacaServer.on((sm + "/driverversion").c_str(),    HTTP_GET, handleDriverVersion);
  alpacaServer.on((sm + "/interfaceversion").c_str(), HTTP_GET, handleSmInterfaceVersion);
  alpacaServer.on((sm + "/name").c_str(),             HTTP_GET, handleSmName);
  alpacaServer.on((sm + "/supportedactions").c_str(), HTTP_GET, handleSupportedActions);
  alpacaServer.on((sm + "/issafe").c_str(),           HTTP_GET, handleSmIsSafe);
  alpacaServer.on((sm + "/setup").c_str(),            HTTP_GET, redirectToSetup);

  // Switch device – cloud cover indicator
  String sw = "/api/v1/switch/0";
  alpacaServer.on((sw + "/action").c_str(),              HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sw + "/commandblind").c_str(),        HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sw + "/commandbool").c_str(),         HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sw + "/commandstring").c_str(),       HTTP_PUT, handleActionNotImpl);
  alpacaServer.on((sw + "/connected").c_str(),           HTTP_GET, handleSwGetConnected);
  alpacaServer.on((sw + "/connected").c_str(),           HTTP_PUT, handleSwPutConnected);
  alpacaServer.on((sw + "/description").c_str(),         HTTP_GET, handleSwDescription);
  alpacaServer.on((sw + "/driverinfo").c_str(),          HTTP_GET, handleSwDriverInfo);
  alpacaServer.on((sw + "/driverversion").c_str(),       HTTP_GET, handleDriverVersion);
  alpacaServer.on((sw + "/interfaceversion").c_str(),    HTTP_GET, handleSwInterfaceVersion);
  alpacaServer.on((sw + "/name").c_str(),                HTTP_GET, handleSwName);
  alpacaServer.on((sw + "/supportedactions").c_str(),    HTTP_GET, handleSupportedActions);
  alpacaServer.on((sw + "/maxswitch").c_str(),           HTTP_GET, handleSwMaxSwitch);
  alpacaServer.on((sw + "/canwrite").c_str(),            HTTP_GET, handleSwCanWrite);
  alpacaServer.on((sw + "/getswitch").c_str(),           HTTP_GET, handleSwGetSwitch);
  alpacaServer.on((sw + "/getswitchdescription").c_str(),HTTP_GET, handleSwGetSwitchDescription);
  alpacaServer.on((sw + "/getswitchname").c_str(),       HTTP_GET, handleSwGetSwitchName);
  alpacaServer.on((sw + "/getswitchvalue").c_str(),      HTTP_GET, handleSwGetSwitchValue);
  alpacaServer.on((sw + "/minswitchvalue").c_str(),      HTTP_GET, handleSwMinSwitchValue);
  alpacaServer.on((sw + "/maxswitchvalue").c_str(),      HTTP_GET, handleSwMaxSwitchValue);
  alpacaServer.on((sw + "/switchstep").c_str(),          HTTP_GET, handleSwSwitchStep);
  alpacaServer.on((sw + "/setswitch").c_str(),           HTTP_PUT, handleSwReadOnly);
  alpacaServer.on((sw + "/setswitchname").c_str(),       HTTP_PUT, handleSwReadOnly);
  alpacaServer.on((sw + "/setswitchvalue").c_str(),      HTTP_PUT, handleSwReadOnly);
  alpacaServer.on((sw + "/setup").c_str(),               HTTP_GET, redirectToSetup);
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
  Debug.printf("[Discovery] task started on Core %d, port %d\n",
               xPortGetCoreID(), ALPACA_DISCOVERY_PORT);

  char resp[32];
  snprintf(resp, sizeof(resp), "{\"AlpacaPort\":%d}", ALPACA_PORT);

  for (;;) {
    int pktSize = udp.parsePacket();
    if (pktSize > 0) {
      char buf[32];
      int n = udp.read(buf, sizeof(buf) - 1);
      if (n > 0) {
        // Trim trailing whitespace/newline in-place.
        while (n > 0 && (buf[n-1] == '\r' || buf[n-1] == '\n' || buf[n-1] == ' '))
          n--;
        buf[n] = '\0';
        if (strncasecmp(buf, "alpacadiscovery1", 16) == 0) {
          udp.beginPacket(udp.remoteIP(), udp.remotePort());
          udp.write((const uint8_t *)resp, strlen(resp));
          int ok = udp.endPacket();
          Debug.printf("[Discovery] replied to %s:%d (ok=%d)\n",
                       udp.remoteIP().toString().c_str(),
                       udp.remotePort(), ok);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
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
                          4096, nullptr, 1, nullptr, 0);
}

// No-op: discovery is now handled by the FreeRTOS task above.
void handleAlpacaDiscovery() {}
