/*
 * ESP32-S3 MLX90640 Sky Conditions Sensor
 * MQTT / Home Assistant Auto-Discovery Handler
 */

#include "mqtt_handler.h"
#include "config_store.h"
#include "sky_sensor.h"
#include "rain_sensor.h"
#include "web_ui_handler.h"
#include "debug.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Module-private state
// ---------------------------------------------------------------------------

static WiFiClient   espClient;
static PubSubClient mqttClient(espClient);

// Derived topic strings built in setupMQTT()
static char topicState[MQTT_TOPIC_SIZE];
static char topicImage[MQTT_TOPIC_SIZE];
static char topicAvailability[MQTT_TOPIC_SIZE];

static unsigned long lastPublishMs   = 0;
static unsigned long lastReconnectMs = 0;
#define RECONNECT_INTERVAL_MS 15000   // retry connection every 15 s

// ---------------------------------------------------------------------------
// Unique device ID (derived from MAC, computed once)
// ---------------------------------------------------------------------------

static String uniqueID;

static void ensureUniqueID()
{
  if (uniqueID.length() > 0) return;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  uniqueID = "skycond_";
  for (int i = 0; i < 6; i++) {
    char buf[3];
    sprintf(buf, "%02x", mac[i]);
    uniqueID += buf;
  }
}

// ---------------------------------------------------------------------------
// Topic construction
// ---------------------------------------------------------------------------

static void buildTopics()
{
  // If the user left the prefix at the default, append the MAC so multiple
  // devices on the same broker don't collide.
  char prefix[MQTT_TOPIC_SIZE];
  strncpy(prefix, deviceConfig.mqttTopicPrefix, sizeof(prefix) - 1);
  prefix[sizeof(prefix) - 1] = '\0';

  if (strcmp(prefix, DEFAULT_MQTT_TOPIC_PREFIX) == 0) {
    ensureUniqueID();
    snprintf(prefix, sizeof(prefix), "%s_%s", DEFAULT_MQTT_TOPIC_PREFIX,
             uniqueID.c_str() + 8);  // strip "skycond_" → just hex MAC
  }

  snprintf(topicState,        sizeof(topicState),        "%s/state",        prefix);
  snprintf(topicImage,        sizeof(topicImage),        "%s/thumbnail",    prefix);
  snprintf(topicAvailability, sizeof(topicAvailability), "%s/availability", prefix);
}

// ---------------------------------------------------------------------------
// Shared device info block (identical across all discovery messages)
// ---------------------------------------------------------------------------

static void addDeviceInfo(JsonDocument &doc)
{
  JsonObject device = doc.createNestedObject("device");
  JsonArray  ids    = device.createNestedArray("identifiers");
  ids.add(uniqueID);
  device["name"]              = String(deviceConfig.location) + " Sky Conditions";
  device["model"]             = "MLX90640 Sky Conditions Sensor";
  device["manufacturer"]      = MANUFACTURER;
  device["sw_version"]        = MANUFACTURER_V;
  device["configuration_url"] = "http://" + WiFi.localIP().toString();
}

// ---------------------------------------------------------------------------
// Discovery helpers
// ---------------------------------------------------------------------------

static void publishSensor(const String &deviceId,
                          const char *name,
                          const char *objId,
                          const char *valueTemplate,
                          const char *unitOfMeasurement,
                          const char *deviceClass,   // nullptr → omit
                          const char *icon)
{
  DynamicJsonDocument doc(768);

  doc["name"]           = name;
  doc["object_id"]      = objId;
  doc["unique_id"]      = deviceId + "_" + objId;
  doc["state_topic"]    = topicState;
  doc["value_template"] = valueTemplate;
  if (unitOfMeasurement && unitOfMeasurement[0])
    doc["unit_of_measurement"] = unitOfMeasurement;
  if (deviceClass && deviceClass[0])
    doc["device_class"] = deviceClass;
  if (icon && icon[0])
    doc["icon"] = icon;
  doc["availability_topic"] = topicAvailability;
  doc["state_class"]        = "measurement";
  addDeviceInfo(doc);

  String payload;
  serializeJson(doc, payload);

  String topic = "homeassistant/sensor/" + deviceId + "_" + String(objId) + "/config";
  if (mqttClient.publish(topic.c_str(), payload.c_str(), /*retain=*/true)) {
    Debug.printf("MQTT discovery OK: %s\n", objId);
  } else {
    Debug.printf("MQTT discovery FAIL: %s\n", objId);
  }
  delay(50);
}

// ---------------------------------------------------------------------------
// publishDiscovery()
// ---------------------------------------------------------------------------

void publishDiscovery()
{
  if (!mqttClient.connected()) return;

  ensureUniqueID();
  const String &devId = uniqueID;

  // ── Sensor entities ────────────────────────────────────────────────────────

  publishSensor(devId, "Sky Temperature",     "sky_temperature",
                "{{ value_json.sky_temp | round(1) }}",
                "°C", "temperature", "mdi:weather-night");

  publishSensor(devId, "Ambient Temperature", "ambient_temperature",
                "{{ value_json.ambient_temp | round(1) }}",
                "°C", "temperature", "mdi:thermometer");

  publishSensor(devId, "Cloud Cover",         "cloud_cover",
                "{{ value_json.cloud_cover | round(1) }}",
                "%", nullptr, "mdi:weather-cloudy");

  publishSensor(devId, "Cloud Cover (Mean)",  "cloud_cover_mean",
                "{{ value_json.cloud_cover_mean | round(1) }}",
                "%", nullptr, "mdi:weather-cloudy");

  publishSensor(devId, "Cloud Cover (Per-Pixel)", "cloud_cover_pixel",
                "{{ value_json.cloud_cover_pixel | round(1) }}",
                "%", nullptr, "mdi:weather-cloudy");

  publishSensor(devId, "Sky Brightness",      "sky_brightness",
                "{{ value_json.lux | round(4) }}",
                "lx", "illuminance", "mdi:brightness-5");

  publishSensor(devId, "Sky Quality (SQM)",   "sky_quality",
                "{{ value_json.sqm | round(2) }}",
                "mag/arcsec²", nullptr, "mdi:telescope");

  publishSensor(devId, "Frame Min Temp",      "frame_min_temp",
                "{{ value_json.frame_min | round(1) }}",
                "°C", "temperature", "mdi:thermometer-minus");

  publishSensor(devId, "Frame Max Temp",      "frame_max_temp",
                "{{ value_json.frame_max | round(1) }}",
                "°C", "temperature", "mdi:thermometer-plus");

  publishSensor(devId, "Frame Median Temp",   "frame_median_temp",
                "{{ value_json.frame_median | round(1) }}",
                "°C", "temperature", "mdi:thermometer");

  // ── Rain sensor binary sensor ──────────────────────────────────────────────
  {
    DynamicJsonDocument doc(768);
    doc["name"]              = "Rain / Snow";
    doc["object_id"]         = "rain_wet";
    doc["unique_id"]         = devId + "_rain_wet";
    doc["state_topic"]       = topicState;
    doc["value_template"]    = "{{ 'ON' if value_json.rain_wet else 'OFF' }}";
    doc["device_class"]      = "moisture";
    doc["icon"]              = "mdi:weather-rainy";
    doc["availability_topic"] = topicAvailability;
    addDeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);

    String topic = "homeassistant/binary_sensor/" + devId + "_rain_wet/config";
    if (mqttClient.publish(topic.c_str(), payload.c_str(), /*retain=*/true))
      Debug.println("MQTT discovery OK: rain_wet");
    else
      Debug.println("MQTT discovery FAIL: rain_wet");
    delay(50);
  }

  // ── Image entity ───────────────────────────────────────────────────────────
  {
    DynamicJsonDocument doc(768);
    doc["name"]               = "Sky Thermal Image";
    doc["object_id"]          = "sky_thermal_image";
    doc["unique_id"]          = devId + "_sky_thermal_image";
    doc["image_topic"]        = topicImage;   // NOTE: image_topic, not topic
    doc["content_type"]       = "image/jpeg";
    doc["image_encoding"]     = "none";       // raw binary JPEG, not base64
    doc["availability_topic"] = topicAvailability;
    addDeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);

    String topic = "homeassistant/image/" + devId + "_sky_thermal_image/config";
    if (mqttClient.publish(topic.c_str(), payload.c_str(), /*retain=*/true)) {
      Debug.println("MQTT discovery OK: sky_thermal_image");
    } else {
      Debug.println("MQTT discovery FAIL: sky_thermal_image");
    }
    delay(50);
  }

  Debug.println("MQTT HA discovery complete");
}

// ---------------------------------------------------------------------------
// publishThermalImage() – streams the cached /thermal.jpg via MQTT
// ---------------------------------------------------------------------------

static void publishThermalImage()
{
  if (!mqttClient.connected()) return;

  const uint8_t *jpegBuf = nullptr;
  size_t         jpegLen = 0;
  if (!getThermalJpeg(&jpegBuf, &jpegLen)) {
    Debug.println("MQTT image: no thumbnail available yet");
    return;
  }

  if (!mqttClient.beginPublish(topicImage, jpegLen, /*retain=*/true)) {
    Debug.println("MQTT image: beginPublish failed");
    return;
  }

  // Stream directly to TCP – no full-payload buffer needed
  size_t written = mqttClient.write(jpegBuf, jpegLen);
  mqttClient.endPublish();

  if (written == jpegLen) {
    Debug.printf("MQTT image: sent %u B\n", (unsigned)jpegLen);
  } else {
    Debug.printf("MQTT image: only sent %u / %u B\n", (unsigned)written, (unsigned)jpegLen);
  }
}

// ---------------------------------------------------------------------------
// publishStatusToMQTT() – full sensor state JSON + thumbnail
// ---------------------------------------------------------------------------

void publishStatusToMQTT()
{
  if (!deviceConfig.mqttEnabled || !mqttClient.connected()) return;
  if (!skyConditions.hasData()) return;

  // ── State JSON ─────────────────────────────────────────────────────────────
  DynamicJsonDocument doc(512);

  doc["sky_temp"]          = round(skyConditions.getSkyTemperature()    * 100.0f) / 100.0f;
  doc["ambient_temp"]      = round(skyConditions.getAmbientTemperature()* 100.0f) / 100.0f;
  doc["frame_min"]         = round(skyConditions.getMinTemperature()    * 100.0f) / 100.0f;
  doc["frame_max"]         = round(skyConditions.getMaxTemperature()    * 100.0f) / 100.0f;
  doc["frame_median"]      = round(skyConditions.getMedianTemperature() * 100.0f) / 100.0f;
  doc["cloud_cover"]       = round(skyConditions.getCloudCover()        *  10.0f) /  10.0f;
  doc["cloud_cover_mean"]  = round(skyConditions.getCloudCoverMean()    *  10.0f) /  10.0f;
  doc["cloud_cover_pixel"] = round(skyConditions.getCloudCoverPixel()   *  10.0f) /  10.0f;
  doc["lux"]               = skyConditions.getLux();
  doc["sqm"]               = round(skyConditions.getSqm()               * 100.0f) / 100.0f;
  doc["rain_wet"]          = rainIsWet();
  doc["rain_relay"]        = rainData.relayWet;
  doc["rain_modbus"]       = rainData.modbusOk ? (bool)rainData.modbusWet : (bool)false;
  doc["has_data"]          = skyConditions.hasData();
  doc["has_brightness"]    = skyConditions.hasBrightnessData();
  doc["ip"]                = WiFi.localIP().toString();
  doc["version"]           = MANUFACTURER_V;

  String payload;
  serializeJson(doc, payload);

  if (mqttClient.publish(topicState, payload.c_str())) {
    Debug.println("MQTT state published");
  } else {
    Debug.println("MQTT state publish failed");
  }

  // ── Thermal thumbnail ──────────────────────────────────────────────────────
  publishThermalImage();
}

// ---------------------------------------------------------------------------
// setupMQTT() – lightweight; real work happens lazily in reconnectMQTT()
// ---------------------------------------------------------------------------

void setupMQTT()
{
  // Buffer only needs to hold MQTT header + longest topic string.
  // JPEG thumbnail is streamed via beginPublish/write/endPublish, bypassing this buffer.
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(DEFAULT_MQTT_KEEPALIVE);
  // Connection attempt deferred to mqttLoop() so setup() isn't blocked.
}

// ---------------------------------------------------------------------------
// reconnectMQTT() – called from mqttLoop on disconnect
// ---------------------------------------------------------------------------

static void reconnectMQTT()
{
  if (mqttClient.connected()) return;

  // Rebuild topics and re-point server every attempt so config changes
  // made via the Setup page after boot are always picked up.
  ensureUniqueID();
  buildTopics();
  mqttClient.setServer(deviceConfig.mqttServer, deviceConfig.mqttPort);

  Debug.printf("MQTT connecting to %s:%d ...", deviceConfig.mqttServer, deviceConfig.mqttPort);

  String clientId = "skycond_" + uniqueID.substring(8);  // hex MAC suffix

  const char *user = strlen(deviceConfig.mqttUser)     ? deviceConfig.mqttUser     : nullptr;
  const char *pass = strlen(deviceConfig.mqttPassword) ? deviceConfig.mqttPassword : nullptr;

  if (mqttClient.connect(clientId.c_str(), user, pass,
                         topicAvailability, /*qos*/1, /*retain*/true, "offline")) {
    Debug.println(" connected");
    mqttClient.publish(topicAvailability, "online", /*retain=*/true);
    publishDiscovery();
    publishStatusToMQTT();
  } else {
    Debug.printf(" failed (rc=%d)\n", mqttClient.state());
  }
}

// ---------------------------------------------------------------------------
// mqttLoop() – call from main loop()
// ---------------------------------------------------------------------------

void mqttLoop()
{
  if (!deviceConfig.mqttEnabled) return;
  if (strlen(deviceConfig.mqttServer) == 0) return;

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectMs >= RECONNECT_INTERVAL_MS) {
      lastReconnectMs = now;
      reconnectMQTT();
    }
    return;
  }

  mqttClient.loop();

  // Periodic publish – state JSON + thumbnail, no more than once per interval
  unsigned long now = millis();
  if (now - lastPublishMs >= MQTT_PUBLISH_INTERVAL) {
    lastPublishMs = now;
    publishStatusToMQTT();
  }
}
