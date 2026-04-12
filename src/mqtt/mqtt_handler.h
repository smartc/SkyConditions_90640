/*
 * ESP32-S3 MLX90640 Sky Conditions Sensor
 * MQTT / Home Assistant Auto-Discovery Handler
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "config.h"
#include "PubSubClient.h"
#include <WiFi.h>
#include <ArduinoJson.h>

// Function prototypes
void setupMQTT();
void mqttLoop();
void publishStatusToMQTT();
void publishDiscovery();

#endif // MQTT_HANDLER_H
