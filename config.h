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
#define MANUFACTURER_V    "0.2.0"
#define LOCATION          "Observatory"
#define DEVICE_NAME       "MLX90640 Sky Conditions Sensor"
#define DESCRIPTION       "ESP32-S3 ASCOM Alpaca ObservingConditions device using MLX90640 thermal camera"
#define DEVICE_TYPE       "observingconditions"
#define DEVICE_NUMBER     0

// Preferences Namespace
#define PREFERENCES_NAMESPACE "skyCond"

// WebSocket binary frame layout (sent to browser)
// Bytes  0-3:  float32 min temperature (full frame)
// Bytes  4-7:  float32 max temperature (full frame)
// Bytes  8-11: float32 median temperature (full frame)
// Bytes 12-15: float32 sky temperature (center 50% FOV average)
// Bytes 16-783: uint8  normalized pixel values [0..255]
#define WS_HEADER_SIZE      16
#define WS_FRAME_SIZE       (WS_HEADER_SIZE + SENSOR_PIXELS)  // 784 bytes

#endif // CONFIG_H
