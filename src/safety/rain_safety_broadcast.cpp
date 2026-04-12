/*
 * DDA-Compatible Safety Sensor UDP Broadcast
 */

#include "rain_safety_broadcast.h"
#include "config.h"
#include "config_store.h"
#include "../sensors/rain_sensor.h"
#include "../sensors/sky_sensor.h"
#include "../debug/debug.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

static WiFiUDP   safetyUdp;
static char      serialNumber[13];  // lowercase MAC hex, no colons + null
static bool      lastIsSafe    = true;
static unsigned long lastBroadcast = 0;  // 0 = never broadcast; triggers immediate send

// ---------------------------------------------------------------------------
// Internal: build and send one broadcast packet
// ---------------------------------------------------------------------------
static void sendBroadcast()
{
    bool isSafe = !rainIsWet();

    StaticJsonDocument<320> doc;
    doc["serialNumber"] = serialNumber;
    doc["deviceType"]   = RAIN_SAFETY_DEVICE_TYPE;

    char name[48];
    snprintf(name, sizeof(name), "%s Safety Sensor", deviceConfig.location);
    doc["name"]         = name;

    doc["isSafe"]       = isSafe;
    doc["rainState"]    = isSafe ? "dry" : "wet";

    // Ambient temperature: skyConditions.getAmbientTemperature() already applies
    // the DHT/BMP priority over the MLX die temp; only null before first frame.
    if (skyConditions.hasData()) {
        doc["ambTemp"] = skyConditions.getAmbientTemperature();
    } else {
        doc["ambTemp"] = nullptr;
    }

    char payload[320];
    size_t n = serializeJson(doc, payload, sizeof(payload));

    safetyUdp.beginPacket(IPAddress(255, 255, 255, 255), deviceConfig.safetyBcastPort);
    safetyUdp.write((uint8_t*)payload, n);
    safetyUdp.endPacket();

    lastBroadcast = millis();
    Debug.printf("[SafetyBcast] %s\n", payload);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void rainSafetyBroadcastSetup()
{
    // Derive serial number from MAC – stable across reboots, unique per device.
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(serialNumber, sizeof(serialNumber), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Debug.printf("[SafetyBcast] serial=%s  enabled=%d  port=%d  interval=%ds\n",
                 serialNumber,
                 (int)deviceConfig.safetyBcastEnabled,
                 deviceConfig.safetyBcastPort,
                 deviceConfig.safetyBcastIntervalSec);
}

void rainSafetyBroadcastLoop()
{
    if (!deviceConfig.safetyBcastEnabled) return;

    bool currentIsSafe = !rainIsWet();
    unsigned long now  = millis();
    unsigned long intervalMs = (unsigned long)deviceConfig.safetyBcastIntervalSec * 1000UL;

    bool stateChanged    = (currentIsSafe != lastIsSafe);
    bool intervalElapsed = (lastBroadcast == 0) || (now - lastBroadcast >= intervalMs);

    if (stateChanged || intervalElapsed) {
        if (stateChanged) {
            Debug.printf("[SafetyBcast] State change: %s → %s\n",
                         lastIsSafe    ? "SAFE" : "UNSAFE",
                         currentIsSafe ? "SAFE" : "UNSAFE");
        }
        sendBroadcast();
        lastIsSafe = currentIsSafe;
    }
}
