#ifndef CONFIG_H
#define CONFIG_H

// Rain Sensor Pins
#define RAIN_POWER_PIN 13  // Pin to power the rain sensor
#define RAIN_DO_PIN    35  // Digital Output pin from the rain sensor
#define RAIN_AO_PIN    33  // Analog Output pin from the rain sensor

// Alpaca Configuration
#define ALPACA_PORT           11111         // Port on which Alpaca device will serve data
#define ALPACA_DISCOVERY      32227         // Alpaca discovery port number

// Device information
#define SERVER_NAME       "Rain_Safety_Monitor"
#define MANUFACTURER      "Corey Smart"
#define MANUFACTURER_V    "0.1.2"
#define LOCATION          "Observatory"
#define DEVICE_NAME       "Rain Safety Sensor"
#define DEVICE_TYPE       "safetymonitor"
#define DEVICE_NUMBER     0
#define DESCRIPTION       "ESP32 Based Rain Sensor Safety Monitor"
#define UNIQUE_ID         ""  // Will be set at runtime based on MAC address

// Rain sensor configuration
#define RAIN_CHECK_INTERVAL 10000  // Check rain sensor every 10 seconds
#define RAIN_SAMPLES 5            // Number of analog samples to average
#define DEFAULT_ANALOG_THRESHOLD 3000  // Default threshold for analog reading (0-4095)
#define DEFAULT_USE_ANALOG_MODE true   // Default to using analog mode instead of digital
#define DEFAULT_HOLD_TIME 0            // Default hold time in minutes (0 = no hold)

// Preferences namespace and keys
#define PREFERENCES_NAMESPACE "rainSensor"
#define PREF_ANALOG_THRESHOLD "threshold"
#define PREF_USE_ANALOG_MODE "useAnalog"
#define PREF_HOLD_TIME "holdTime"

#endif // CONFIG_H