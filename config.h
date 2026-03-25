#ifndef CONFIG_H
#define CONFIG_H

// MLX90640 I2C Configuration
#define THERMAL_SDA_PIN     8
#define THERMAL_SCL_PIN     9
#define THERMAL_I2C_FREQ    400000   // 400 kHz

// MLX90640 Sensor Geometry
#define SENSOR_COLS         32
#define SENSOR_ROWS         24
#define SENSOR_PIXELS       768      // 32 * 24

// Center 50% FOV Region (center half of each dimension = 16x12 pixels)
// Row-major order: pixel index = row * SENSOR_COLS + col
#define CENTER_COL_START    8        // Columns 8..23 = 16 cols (50% of 32)
#define CENTER_COL_END      23
#define CENTER_ROW_START    6        // Rows 6..17   = 12 rows (50% of 24)
#define CENTER_ROW_END      17
#define CENTER_PIXEL_COUNT  192      // 16 * 12

// Sensor Refresh
#define FRAME_INTERVAL_MS       500    // 500 ms = 2 Hz (MLX90640)
#define BRIGHTNESS_INTERVAL_MS  1000   // 1 s   = 1 Hz  (TSL2591)
#define JPEG_INTERVAL_MS        30000  // 30 s  thermal JPEG snapshot

// ASCOM Alpaca Configuration
#define ALPACA_PORT           11111
#define ALPACA_DISCOVERY_PORT 32227

// Device Information
#define SERVER_NAME       "SkyConditions_90640"
#define MANUFACTURER      "Corey Smart"
#define MANUFACTURER_V    "0.4.1"
#define LOCATION          "Observatory"
#define DEVICE_NAME       "MLX90640 Sky Conditions Sensor"
#define DESCRIPTION       "ESP32-S3 ASCOM Alpaca ObservingConditions device using MLX90640 thermal camera"
#define DEVICE_TYPE       "observingconditions"
#define DEVICE_NUMBER     0

// Rain / Snow Sensor (ZTS-3000-YUX-NO1RO1-H, MAX3485ED transceiver)
#define RAIN_RELAY_PIN       38    // IO38: INPUT_PULLUP; LOW when wet (relay closes)
#define RAIN_RS485_DE_PIN    39    // IO39: MAX3485ED DE/RE# (HIGH=TX, LOW=RX)
#define RAIN_RS485_TX_PIN    40    // IO40: MAX3485ED DI  (ESP32 → sensor)
#define RAIN_RS485_RX_PIN    41    // IO41: MAX3485ED RO  (sensor → ESP32)
#define RAIN_BAUD_RATE       9600
#define RAIN_MODBUS_ADDR     0x01  // Default slave address per datasheet
#define RAIN_POLL_MS         5000  // Modbus poll interval (ms)

// Preferences Namespace
#define PREFERENCES_NAMESPACE "skyCond"

// MQTT Configuration
#define DEFAULT_MQTT_SERVER        ""
#define DEFAULT_MQTT_PORT          1883
#define DEFAULT_MQTT_USER          ""
#define DEFAULT_MQTT_PASSWORD      ""
#define DEFAULT_MQTT_TOPIC_PREFIX  "skyconditions"
#define DEFAULT_MQTT_KEEPALIVE     60       // seconds; LWT fires after ~1.5×
#define MQTT_PUBLISH_INTERVAL      30000    // ms between periodic state publishes
#define MQTT_SERVER_SIZE           64
#define MQTT_USER_SIZE             32
#define MQTT_PASSWORD_SIZE         64
#define MQTT_TOPIC_SIZE            80

// Thermal JPEG snapshot scale factor (applied before JPEG encoding).
// scaledBuf is placed in PSRAM (EXT_RAM_BSS_ATTR) so internal heap is unaffected.
// 10 → 320×240 px (~230 KB staging in PSRAM).  Requires PSRAM enabled in board settings.
#define THERMAL_JPEG_SCALE  10

// WebSocket binary frame layout (sent to browser)
// Bytes  0-3:  float32 min temperature (full frame)
// Bytes  4-7:  float32 max temperature (full frame)
// Bytes  8-11: float32 median temperature (full frame)
// Bytes 12-15: float32 sky temperature (center 50% FOV average)
// Bytes 16-783: uint8  normalized pixel values [0..255]
#define WS_HEADER_SIZE      16
#define WS_FRAME_SIZE       (WS_HEADER_SIZE + SENSOR_PIXELS)  // 784 bytes

#endif // CONFIG_H
