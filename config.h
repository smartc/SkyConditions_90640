#ifndef CONFIG_H
#define CONFIG_H

// ── Board Selection ───────────────────────────────────────────────────────────
// Uncomment the line matching your target board.
// With ESP32S3 Dev Module:  SDA = GPIO8, SCL = GPIO9.
// With XIAO ESP32-S3 Sense: external I2C pads are GPIO5 (SDA) / GPIO6 (SCL).
//   Using D-number macros is unreliable across board definitions; numeric GPIOs are used.
#define BOARD_XIAO_SENSE     // Seeed XIAO ESP32-S3 Sense
// #define BOARD_ESP32S3_DEV // ESP32-S3 Dev Module (comment the line above first)

// MLX90640 I2C Configuration
#if defined(BOARD_XIAO_SENSE)
  #pragma message "Board: XIAO ESP32-S3 Sense (SDA=GPIO5, SCL=GPIO6)"
  #define THERMAL_SDA_PIN   5    // GPIO5 = D4 pad on XIAO ESP32-S3 Sense
  #define THERMAL_SCL_PIN   6    // GPIO6 = D5 pad on XIAO ESP32-S3 Sense
#else
  #pragma message "Board: ESP32-S3 Dev Module (SDA=GPIO8, SCL=GPIO9)"
  #define THERMAL_SDA_PIN   8    // GPIO8 on ESP32-S3 Dev Module
  #define THERMAL_SCL_PIN   9    // GPIO9 on ESP32-S3 Dev Module
#endif
#if defined(BOARD_XIAO_SENSE)
  #define THERMAL_I2C_FREQ  100000   // 100 kHz – XIAO header has no external pull-ups
#else
  #define THERMAL_I2C_FREQ  400000   // 400 kHz
#endif

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
#define FRAME_INTERVAL_MS       5000   // 5 s (MLX90640)
#define BRIGHTNESS_INTERVAL_MS  5000   // 5 s (TSL2591)
#define JPEG_INTERVAL_MS        30000  // 30 s  thermal JPEG snapshot

// Ambient temperature sensor (optional – see dht_sensor.h/.cpp)
// Sensor type is selected at runtime via Setup page → ambientSensorType (NVS key "dhtType"):
//   0 = disabled, 1 = DHT11, 2 = DHT22, 3 = BMP180
//
// DHT11/DHT22 use the D6/D7 Grove connector; data line on D7 (GPIO44, UART0 RX).
// BMP180 is I2C – shares the existing SDA/SCL bus (no extra GPIO needed), addr 0x77.
#if defined(BOARD_XIAO_SENSE)
  #define DHT_DATA_PIN   44    // D7 = GPIO44
#else
  #define DHT_DATA_PIN   (-1)  // not assigned for Dev board – define as needed
#endif
#define DHT_INTERVAL_MS   5000  // match sensor read rate; DHT11 minimum is 1 s
#define BMP180_I2C_ADDR   0x77  // BMP180 default I2C address
#define BMP280_I2C_ADDR   0x76  // BMP280 default I2C address (SDO→GND); use 0x77 if SDO→VCC

// ASCOM Alpaca Configuration
#define ALPACA_PORT           11111
#define ALPACA_DISCOVERY_PORT 32227

// Device Information
#define SERVER_NAME       "SkyConditions_90640"
#define MANUFACTURER      "Corey Smart"
#define MANUFACTURER_V    "0.5.1"
#define LOCATION          "Observatory"
#define DEVICE_NAME       "MLX90640 Sky Conditions Sensor"
#define DESCRIPTION       "ESP32-S3 ASCOM Alpaca ObservingConditions device using MLX90640 thermal camera"
#define DEVICE_TYPE       "observingconditions"
#define DEVICE_NUMBER     0

// Rain / Snow Sensor (ZTS-3000-YUX-NO1RO1-H, MAX3485ED transceiver)
//
// Relay wiring varies by board:
//   XIAO Grove Shield: relay bridged between D8 and D9.
//     DRIVE pin (D8/GPIO7) → OUTPUT HIGH feeds one relay contact.
//     SENSE pin (D9/GPIO8) → INPUT_PULLDOWN; reads HIGH when relay closes (wet).
//   ESP32-S3 Dev: relay contact pulls IO38 to GND (original single-pin scheme).
//     DRIVE pin not used (-1).
//     SENSE pin (IO38) → INPUT_PULLUP; reads LOW when relay closes (wet).
#if defined(BOARD_XIAO_SENSE)
  #define RAIN_RELAY_DRIVE_PIN  7    // D8 = GPIO7: OUTPUT HIGH, drives relay contact
  #define RAIN_RELAY_SENSE_PIN  8    // D9 = GPIO8: INPUT_PULLDOWN; HIGH = wet
#else
  #define RAIN_RELAY_DRIVE_PIN  (-1) // not used; relay return is GND
  #define RAIN_RELAY_SENSE_PIN  38   // IO38: INPUT_PULLUP; LOW = wet
#endif
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
