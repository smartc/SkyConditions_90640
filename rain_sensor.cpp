/*
 * Rain / Snow Sensor – ZTS-3000-YUX-NO1RO1-H
 * Relay input (IO38) + RS485 Modbus RTU via MAX3485ED (IO39/40/41)
 */

#include "rain_sensor.h"
#include "config.h"
#include "config_store.h"
#include "debug.h"
#include "web_ui_handler.h"

RainData rainData = {};

static HardwareSerial &rs485 = Serial2;

// ---------------------------------------------------------------------------
// Modbus CRC16
// ---------------------------------------------------------------------------

static uint16_t crc16(const uint8_t *buf, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Single Modbus register read (Function Code 03)
//
// Returns true and sets *val on success.
// The DE/RE# control pin on IO39 is asserted HIGH (TX) for the request,
// then pulled LOW (RX) after flush() so the reply can be received.
// ---------------------------------------------------------------------------

static bool modbusRead(uint8_t addr, uint16_t reg, uint16_t *val)
{
    // Build 8-byte Modbus RTU request
    uint8_t req[8];
    req[0] = addr;
    req[1] = 0x03;          // Read Holding Registers
    req[2] = reg >> 8;
    req[3] = reg & 0xFF;
    req[4] = 0x00;
    req[5] = 0x01;          // quantity = 1 register
    uint16_t c = crc16(req, 6);
    req[6] = c & 0xFF;
    req[7] = c >> 8;

    // Discard any stale bytes in the RX buffer
    while (rs485.available()) rs485.read();

    // --- Transmit ---
    // Assert TX mode, send all 8 bytes, wait for the last bit to leave the
    // shift register, then immediately release back to RX mode.
    // flush() on ESP32 Arduino blocks until the UART shift register is empty,
    // so the line is idle when we pull DE LOW.
    // The extra 150 µs (~1.5 bit-times at 9600) gives the line a clean idle
    // state before the transceiver switches direction.
    digitalWrite(RAIN_RS485_DE_PIN, HIGH);
    rs485.write(req, 8);
    rs485.flush();
    delayMicroseconds(150);
    digitalWrite(RAIN_RS485_DE_PIN, LOW);   // now listening

    // --- Receive ---
    // Expected response: addr(1) + FC(1) + byte_count(1) + data(2) + CRC(2) = 7 bytes
    unsigned long t0 = millis();
    while (rs485.available() < 7 && millis() - t0 < 200) { /* spin */ }

    int avail = rs485.available();
    if (avail < 7) {
        return false;   // caller reports state transitions; don't spam on every poll
    }

    uint8_t resp[7];
    rs485.readBytes(resp, 7);

    Debug.printf("[Rain] RS485 rx: %02X %02X %02X %02X %02X %02X %02X\n",
        resp[0], resp[1], resp[2], resp[3], resp[4], resp[5], resp[6]);

    // Validate CRC
    uint16_t rxCrc   = (uint16_t)resp[5] | ((uint16_t)resp[6] << 8);
    uint16_t calcCrc = crc16(resp, 5);
    if (rxCrc != calcCrc) {
        Debug.printf("[Rain] CRC mismatch: received=%04X  calculated=%04X\n", rxCrc, calcCrc);
        return false;
    }

    // Validate header (address, function code, byte count for 1 register = 2)
    if (resp[0] != addr || resp[1] != 0x03 || resp[2] != 0x02) {
        Debug.printf("[Rain] Unexpected header: %02X %02X %02X\n",
            resp[0], resp[1], resp[2]);
        return false;
    }

    *val = ((uint16_t)resp[3] << 8) | resp[4];
    return true;
}

// ---------------------------------------------------------------------------
// Baud-rate scanner – tries common rates and reports any bytes received.
// Returns the working baud rate, or 0 if nothing responded.
//
// TODO: if baud scan finds nothing, add Phase 2 – address scan (0x01..0x20)
//       at 9600 baud in case the sensor shipped with a non-default address.
//       Next debug step: re-try with A/B swapped now that 12V supply is
//       confirmed good, then run the address scan if still no response.
// ---------------------------------------------------------------------------

static const uint32_t BAUD_CANDIDATES[] = { 4800, 9600, 19200, 38400, 115200 };

static uint32_t baudScan()
{
    Debug.println("[Rain] --- RS485 baud scan (addr=0x01, reg 0x0000) ---");

    for (uint32_t baud : BAUD_CANDIDATES) {
        rs485.end();
        rs485.begin(baud, SERIAL_8N1, RAIN_RS485_RX_PIN, RAIN_RS485_TX_PIN);
        delay(50);

        uint8_t req[8] = { RAIN_MODBUS_ADDR, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0 };
        uint16_t c = crc16(req, 6);
        req[6] = c & 0xFF;
        req[7] = c >> 8;

        while (rs485.available()) rs485.read();
        digitalWrite(RAIN_RS485_DE_PIN, HIGH);
        rs485.write(req, 8);
        rs485.flush();
        delayMicroseconds(150);
        digitalWrite(RAIN_RS485_DE_PIN, LOW);

        unsigned long t0 = millis();
        while (rs485.available() == 0 && millis() - t0 < 300) { /* wait */ }

        int avail = rs485.available();
        if (avail > 0) {
            String raw = "";
            while (rs485.available()) {
                char buf[4];
                sprintf(buf, "%02X ", rs485.read());
                raw += buf;
            }
            Debug.printf("[Rain]   %6u baud: GOT %d byte(s): %s\n", baud, avail, raw.c_str());
            if (avail >= 7) {
                Debug.printf("[Rain] Working baud rate: %u\n", baud);
                return baud;
            }
        } else {
            Debug.printf("[Rain]   %6u baud: no response\n", baud);
        }
        delay(50);
    }

    Debug.println("[Rain] --- baud scan complete: no response at any rate ---");
    return 0;
}

// ---------------------------------------------------------------------------
// Register scan – run once we know the baud rate is correct.
// ---------------------------------------------------------------------------

static void registerScan()
{
    Debug.println("[Rain] --- RS485 register scan (addr=0x01, regs 0x0000..0x0007) ---");
    for (uint16_t r = 0; r <= 7; r++) {
        uint16_t val = 0;
        if (modbusRead(RAIN_MODBUS_ADDR, r, &val)) {
            Debug.printf("[Rain]   reg[0x%04X] = %u  (0x%04X)\n", r, val, val);
        } else {
            Debug.printf("[Rain]   reg[0x%04X] = <no response>\n", r);
        }
        delay(50);
    }
    Debug.println("[Rain] --- scan complete ---");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void rainSensorSetup()
{
    // Relay input is always initialised (negligible cost, and mode can change in config).
    pinMode(RAIN_RELAY_PIN, INPUT_PULLUP);
    Debug.printf("[Rain] Relay on IO%d (INPUT_PULLUP; LOW=wet)\n", RAIN_RELAY_PIN);

    if (deviceConfig.rainMode == 1) {
        // RS485 direction control – start in receive mode (RE# low, DE low)
        pinMode(RAIN_RS485_DE_PIN, OUTPUT);
        digitalWrite(RAIN_RS485_DE_PIN, LOW);

        // UART2: IO41=RX (MAX3485ED RO), IO40=TX (MAX3485ED DI)
        rs485.begin(RAIN_BAUD_RATE, SERIAL_8N1, RAIN_RS485_RX_PIN, RAIN_RS485_TX_PIN);
        Debug.printf("[Rain] RS485 UART2  %d 8N1  RX=IO%d  TX=IO%d  DE/RE=IO%d\n",
            RAIN_BAUD_RATE, RAIN_RS485_RX_PIN, RAIN_RS485_TX_PIN, RAIN_RS485_DE_PIN);

        delay(500);
        uint32_t workingBaud = baudScan();
        if (workingBaud > 0) registerScan();
    } else {
        Debug.println("[Rain] Mode: Relay only – RS485 disabled");
    }
}

void rainSensorLoop()
{
    if (deviceConfig.rainMode == 0) {
        // --- Relay mode ---
        bool prevRelay = rainData.relayWet;
        rainData.relayWet = (digitalRead(RAIN_RELAY_PIN) == LOW);
        if (rainData.relayWet != prevRelay) {
            Debug.printf("[Rain] Relay changed: %s\n", rainData.relayWet ? "WET" : "DRY");
            broadcastRainState();
        }
    } else {
        // --- RS485 Modbus mode – polled on interval ---
        static unsigned long lastPoll = 0;
        if (millis() - lastPoll < RAIN_POLL_MS) return;
        lastPoll = millis();

        uint16_t val = 0;
        if (modbusRead(RAIN_MODBUS_ADDR, 0x0000, &val)) {
            bool wet = (val != 0);
            if (wet != rainData.modbusWet || !rainData.modbusOk)
                Debug.printf("[Rain] Modbus reg[0x0000]=%u → %s\n", val, wet ? "WET" : "DRY");
            rainData.modbusWet    = wet;
            rainData.modbusOk     = true;
            rainData.lastModbusMs = millis();
        } else {
            if (rainData.modbusOk)
                Debug.println("[Rain] Modbus: lost communication");
            rainData.modbusOk = false;
        }
    }
}

bool rainIsWet()
{
    if (deviceConfig.rainMode == 0)
        return rainData.relayWet;
    return rainData.modbusOk && rainData.modbusWet;
}
