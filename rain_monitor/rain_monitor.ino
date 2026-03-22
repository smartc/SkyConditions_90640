/*
 * ESP32 Rain Sensor ASCOM Alpaca Driver
 * 
 * This program implements an ASCOM Alpaca API compatible safety monitor
 * that uses a rain sensor to detect precipitation and provide safety monitoring
 * for astronomical equipment.
 */

#include "debug.h"
#include "alpaca.h"
#include "config.h"
#include "web_ui_handler.h"
#include <Preferences.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <ElegantOTA.h>

// Objects
WebServer alpacaServer(ALPACA_PORT);
WiFiUDP udp;
uint32_t serverTransactionID = 0;
Preferences preferences;

// State variables
bool isConnected = true;       // Is the Alpaca client connected to the device? Default to connected
bool isRaining = false;        // Current rain state detected by the sensor
bool isActuallyRaining = false; // The actual sensor reading (without hold time)
int rainLevel = 0;             // Analog reading of rain level (0-4095 on ESP32)
unsigned long lastRainUpdate = 0;  // Last time the rain sensor was checked
unsigned long lastWetTime = 0;     // Last time a wet reading was detected

// Detection settings
bool useAnalogMode = DEFAULT_USE_ANALOG_MODE;  // Use analog reading instead of digital pin
int analogThreshold = DEFAULT_ANALOG_THRESHOLD; // Threshold for analog reading
int holdTimeMinutes = DEFAULT_HOLD_TIME;       // Hold rain state for this many minutes after last wet reading

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(100); // Allow serial to initialize
  Debug.println("\nRain Safety Monitor - ASCOM Alpaca Driver");
  
  // Load settings from preferences
  loadSettings();
  
  // Initialize pins
  pinMode(RAIN_POWER_PIN, OUTPUT);
  pinMode(RAIN_DO_PIN, INPUT);
  digitalWrite(RAIN_POWER_PIN, LOW);  // Initially power off
  
  // ESP32 specific: Set analog read resolution to 12 bits (0-4095) for better precision
  analogReadResolution(12);
  
  // WiFiManager setup
  WiFiManager wifiManager;
  
  // Set timeout for configuration portal
  wifiManager.setConfigPortalTimeout(180); // 3 minutes
  
  // Custom parameters for a more informative portal
  WiFiManagerParameter custom_text("<p>Rain Safety Monitor - WiFi Setup</p>");
  wifiManager.addParameter(&custom_text);
  
  // Try to connect using saved credentials, or create AP
  Debug.println("Connecting to WiFi or starting portal");
  bool connected = wifiManager.autoConnect("RainSensor-Setup", "rainsensor");
  
  if (!connected) {
    Debug.println("Failed to connect and timed out. Restarting...");
    delay(3000);
    ESP.restart();
  }
  
  Debug.println("Connected to WiFi");
  Debug.print("IP Address: ");
  Debug.println(WiFi.localIP().toString());
  
  // Setup Alpaca routes
  setupAlpacaRoutes();
  
  // Start Alpaca server
  setupAlpacaServer();
  
  // Initialize Web UI server
  initWebUI();

  // Intialize ElegantOTA server
  ElegantOTA.begin(&webUiServer);
  
  // Initial rain check
  updateRainStatus();
}

void loop() {
  // Handle HTTP client requests
  alpacaServer.handleClient();
  
  // Handle Web UI requests
  handleWebUI();
  
  // Handle Alpaca discovery
  handleAlpacaDiscovery();
  
  // Check rain sensor periodically
  if (millis() - lastRainUpdate > RAIN_CHECK_INTERVAL) {
    updateRainStatus();
  }

  // Handle OTA updates
  ElegantOTA.loop();
}

// Update the rain sensor reading and state
void updateRainStatus() {
  // Power on the sensor
  digitalWrite(RAIN_POWER_PIN, HIGH);
  delay(10);  // Short delay for sensor to stabilize
  
  // Read the analog output - ESP32 has 12-bit ADC (0-4095)
  // Take multiple readings and average for stability
  int readings = 0;
  for (int i = 0; i < RAIN_SAMPLES; i++) {
    readings += analogRead(RAIN_AO_PIN);
    delay(2);
  }
  int newRainLevel = readings / RAIN_SAMPLES;
  
  // Determine rain state based on selected mode
  bool newActuallyRaining;
  if (useAnalogMode) {
    // Analog mode: compare reading to threshold
    newActuallyRaining = (newRainLevel < analogThreshold);
    Debug.println("Using analog mode with threshold: " + String(analogThreshold));
  } else {
    // Digital mode: use digital pin reading
    newActuallyRaining = (digitalRead(RAIN_DO_PIN) == LOW);
    Debug.println("Using digital mode from DO pin");
  }
  
  // Power off the sensor to reduce corrosion
  digitalWrite(RAIN_POWER_PIN, LOW);
  
  // Update level and actual rain state
  rainLevel = newRainLevel;
  isActuallyRaining = newActuallyRaining;
  
  // Handle hold time logic
  unsigned long currentTime = millis();
  
  // If it's actually raining, update the lastWetTime
  if (isActuallyRaining) {
    lastWetTime = currentTime;
    isRaining = true;  // Set raining state to true
    Debug.println("Wet condition detected, updating lastWetTime");
  } 
  // If it's not actually raining but holdTimeMinutes is configured
  else if (holdTimeMinutes > 0) {
    // Check if we're within the hold time
    unsigned long holdTimeMs = holdTimeMinutes * 60 * 1000; // Convert minutes to milliseconds
    
    // Handle millis() rollover by checking if lastWetTime is greater than current time
    // or if we're still within the hold period
    if ((lastWetTime > currentTime || (currentTime - lastWetTime) < holdTimeMs) && (currentTime > holdTimeMs)) {
      isRaining = true;  // Still considered raining due to hold time
      Debug.print("Hold time active: ");
      unsigned long elapsedSecs = (currentTime - lastWetTime) / 1000;
      unsigned long remainingSecs = (holdTimeMs / 1000) - elapsedSecs;
      Debug.print(elapsedSecs);
      Debug.print(" seconds elapsed, ");
      Debug.print(remainingSecs);
      Debug.println(" seconds remaining");
    } else {
      isRaining = false;  // Hold time expired
      Debug.println("Hold time expired, reporting dry condition");
    }
  } else {
    // No hold time, directly use the actual rain state
    isRaining = isActuallyRaining;
  }
  
  // Log the status
  Debug.print("Sensor Reading: ");
  Debug.println(isActuallyRaining ? "WET" : "DRY");
  Debug.print("Reported Status: ");
  Debug.println(isRaining ? "RAINING" : "DRY");
  Debug.print("Rain Level: ");
  Debug.println(rainLevel);
  
  // Calculate percentage (inverted as more water = lower value)
  int percentage = map(rainLevel, 0, 4095, 100, 0);
  Debug.print("Level Percentage: ");
  Debug.print(percentage);
  Debug.println("%");
  
  // Show detection method info
  Debug.print("Detection method: ");
  Debug.println(useAnalogMode ? "Analog" : "Digital");
  if (useAnalogMode) {
    Debug.print("Current threshold: ");
    Debug.println(analogThreshold);
    Debug.print("Threshold comparison: ");
    Debug.print(rainLevel);
    Debug.print(" < ");
    Debug.print(analogThreshold);
    Debug.print(" = ");
    Debug.println(rainLevel < analogThreshold ? "true (rain)" : "false (dry)");
  }
  
  // Show hold time info
  if (holdTimeMinutes > 0) {
    Debug.print("Hold time: ");
    Debug.print(holdTimeMinutes);
    Debug.println(" minutes");
  }
  
  lastRainUpdate = currentTime;
}

// Load settings from persistent storage
void loadSettings() {
  preferences.begin(PREFERENCES_NAMESPACE, false);
  
  // Load analog threshold setting (if exists)
  if (preferences.isKey(PREF_ANALOG_THRESHOLD)) {
    analogThreshold = preferences.getInt(PREF_ANALOG_THRESHOLD, DEFAULT_ANALOG_THRESHOLD);
  } else {
    analogThreshold = DEFAULT_ANALOG_THRESHOLD;
  }
  
  // Load detection mode setting (if exists)
  if (preferences.isKey(PREF_USE_ANALOG_MODE)) {
    useAnalogMode = preferences.getBool(PREF_USE_ANALOG_MODE, DEFAULT_USE_ANALOG_MODE);
  } else {
    useAnalogMode = DEFAULT_USE_ANALOG_MODE;
  }
  
  // Load hold time setting (if exists)
  if (preferences.isKey(PREF_HOLD_TIME)) {
    holdTimeMinutes = preferences.getInt(PREF_HOLD_TIME, DEFAULT_HOLD_TIME);
  } else {
    holdTimeMinutes = DEFAULT_HOLD_TIME;
  }
  
  preferences.end();
  
  Debug.println("Loaded settings:");
  Debug.print("Detection mode: ");
  Debug.println(useAnalogMode ? "Analog" : "Digital");
  Debug.print("Analog threshold: ");
  Debug.println(analogThreshold);
  Debug.print("Hold time: ");
  Debug.print(holdTimeMinutes);
  Debug.println(" minutes");
}

// Save settings to persistent storage
void saveSettings() {
  preferences.begin(PREFERENCES_NAMESPACE, false);
  
  preferences.putInt(PREF_ANALOG_THRESHOLD, analogThreshold);
  preferences.putBool(PREF_USE_ANALOG_MODE, useAnalogMode);
  preferences.putInt(PREF_HOLD_TIME, holdTimeMinutes);
  
  preferences.end();
  
  Debug.println("Settings saved:");
  Debug.print("Detection mode: ");
  Debug.println(useAnalogMode ? "Analog" : "Digital");
  Debug.print("Analog threshold: ");
  Debug.println(analogThreshold);
  Debug.print("Hold time: ");
  Debug.print(holdTimeMinutes);
  Debug.println(" minutes");
}