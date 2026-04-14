#include <SoftwareSerial.h>

/**
 * @file kaifa_kfm001_han_parser.ino
 * @brief Receive and decode Kaifa KFM_001 HAN frames over SoftwareSerial.
 *
 * This sketch reads Norwegian AMS/HAN telegrams from a Kaifa meter through a
 * TTL-level M-Bus adapter, detects HDLC-style frame boundaries, decodes the
 * Kaifa KFM_001 payload, and prints instantaneous values and rolling averages
 * to the USB serial port.
 *
 * Intended wiring:
 *   - Adapter data output -> Arduino D2
 *   - Adapter GND         -> Arduino GND
 *
 * Notes:
 *   - This implementation is based on the observed KFM_001 payload layout.
 *   - SoftwareSerial is used because D2 was verified to work reliably in this setup.
 *   - A board with a spare hardware UART is recommended for production use.
 */

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Arduino pin receiving HAN data from the adapter.
 */
const uint8_t HAN_RX_PIN = 2;

/**
 * @brief Dummy transmit pin required by SoftwareSerial.
 *
 * This application is receive-only, so the TX pin is not used.
 */
const uint8_t HAN_TX_PIN = 3;

/**
 * @brief Invert SoftwareSerial input logic.
 *
 * Set to true only if the working adapter signal is inverted.
 */
const bool HAN_INVERTED = false;

/**
 * @brief Baud rate used on the HAN serial interface.
 */
const unsigned long HAN_BAUD = 2400;

/**
 * @brief Baud rate used for USB serial debug output.
 */
const unsigned long PC_BAUD = 115200;

/**
 * @brief Number of decoded frames included in the rolling average.
 *
 * Kaifa List 2 is typically transmitted every 10 seconds.
 * With 6 frames, the rolling average covers roughly 1 minute.
 */
const uint8_t AVG_WINDOW_FRAMES = 6;

/**
 * @brief Enable printing of complete decoded frame payloads as hexadecimal.
 *
 * Useful for troubleshooting or verifying field layout.
 */
const bool PRINT_FRAME_HEX = false;

// ============================================================================
// Serial interface
// ============================================================================

/**
 * @brief SoftwareSerial instance connected to the HAN adapter.
 */
SoftwareSerial han(HAN_RX_PIN, HAN_TX_PIN, HAN_INVERTED);

// ============================================================================
// Frame assembly state
// ============================================================================

/**
 * @brief Maximum supported payload size for one decoded frame.
 *
 * Frames larger than this value are discarded.
 */
const size_t MAX_FRAME_LEN = 220;

/**
 * @brief Buffer used to assemble one complete frame.
 */
uint8_t frameBuf[MAX_FRAME_LEN];

/**
 * @brief Number of bytes currently stored in @ref frameBuf.
 */
size_t frameLen = 0;

/**
 * @brief True after the start delimiter has been seen.
 */
bool frameStarted = false;

/**
 * @brief True when the next byte must be unescaped.
 */
bool escapeNext = false;

// ============================================================================
// Decoded payload model
// ============================================================================

/**
 * @brief Decoded Kaifa KFM_001 measurement frame.
 *
 * Scaling:
 *   - Power values are reported directly in W / var
 *   - Current values are stored in mA
 *   - Voltage values are stored in dV
 */
struct PowerFrame {
  bool valid = false;

  char listId[8] = {0};      ///< Expected to contain "KFM_001"
  char meterId[17] = {0};    ///< 16-byte ASCII meter ID + null terminator
  char meterType[9] = {0};   ///< 8-byte ASCII meter type + null terminator

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;

  uint32_t pImportW = 0;     ///< Active import power
  uint32_t pExportW = 0;     ///< Active export power
  uint32_t qImportVar = 0;   ///< Reactive import power
  uint32_t qExportVar = 0;   ///< Reactive export power

  uint32_t i1mA = 0;         ///< Current channel 1
  uint32_t i2mA = 0;         ///< Current channel 2
  uint32_t i3mA = 0;         ///< Current channel 3

  uint32_t u1dV = 0;         ///< Voltage channel 1
  uint32_t u2dV = 0;         ///< Voltage channel 2
  uint32_t u3dV = 0;         ///< Voltage channel 3
};

// ============================================================================
// Rolling average storage
// ============================================================================

/**
 * @brief Circular buffer of recently decoded frames.
 */
PowerFrame history[AVG_WINDOW_FRAMES];

/**
 * @brief Number of valid entries currently stored in @ref history.
 */
uint8_t historyCount = 0;

/**
 * @brief Index where the next frame will be written.
 */
uint8_t historyHead = 0;

// ============================================================================
// Utility functions
// ============================================================================

/**
 * @brief Read a 32-bit big-endian unsigned integer.
 *
 * @param p Pointer to the first byte.
 * @return Decoded unsigned 32-bit value.
 */
uint32_t readBE32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) |
         ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8)  |
         ((uint32_t)p[3]);
}

/**
 * @brief Find the first occurrence of a byte pattern in a buffer.
 *
 * @param buf Input buffer.
 * @param len Length of input buffer.
 * @param pat Pattern to search for.
 * @param patLen Length of the pattern.
 * @return Start index of the pattern, or -1 if not found.
 */
int findPattern(const uint8_t *buf, size_t len, const uint8_t *pat, size_t patLen) {
  if (patLen == 0 || len < patLen) {
    return -1;
  }

  for (size_t i = 0; i <= len - patLen; i++) {
    if (memcmp(buf + i, pat, patLen) == 0) {
      return (int)i;
    }
  }

  return -1;
}

/**
 * @brief Print a frame as hexadecimal bytes.
 *
 * @param buf Pointer to frame buffer.
 * @param len Frame length in bytes.
 */
void printHexFrame(const uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

// ============================================================================
// Frame decoding
// ============================================================================

/**
 * @brief Decode a Kaifa KFM_001 payload from a frame buffer.
 *
 * Parsing strategy:
 *   1. Locate the ASCII list identifier "KFM_001"
 *   2. Locate the DLMS clock field preceding the list identifier
 *   3. Extract meter ID and meter type
 *   4. Extract the ten 32-bit measurement values that follow
 *
 * Expected field order after KFM_001:
 *   - Meter ID      : 09 10 <16 ASCII bytes>
 *   - Meter type    : 09 08 <8 ASCII bytes>
 *   - 10 x 32-bit values tagged with 0x06
 *
 * @param buf Input frame buffer.
 * @param len Input frame length.
 * @param out Decoded output structure.
 * @return true if a valid KFM_001 payload was found and decoded.
 * @return false otherwise.
 */
bool parseKaifaKFM001(const uint8_t *buf, size_t len, PowerFrame &out) {
  PowerFrame pf;

  const uint8_t listPattern[] = {0x09, 0x07, 'K', 'F', 'M', '_', '0', '0', '1'};
  int listPos = findPattern(buf, len, listPattern, sizeof(listPattern));
  if (listPos < 0) {
    return false;
  }

  // Locate the first DLMS clock field before the list identifier.
  const uint8_t clockPattern[] = {0x09, 0x0C};
  int clockPos = -1;
  for (int i = 0; i <= listPos - 2; i++) {
    if (buf[i] == clockPattern[0] && buf[i + 1] == clockPattern[1]) {
      clockPos = i;
      break;
    }
  }

  if (clockPos >= 0 && (size_t)(clockPos + 14) <= len) {
    const uint8_t *t = buf + clockPos + 2;

    // DLMS clock octet string (12 bytes)
    pf.year   = ((uint16_t)t[0] << 8) | t[1];
    pf.month  = t[2];
    pf.day    = t[3];
    // t[4] contains day-of-week and is not used here
    pf.hour   = t[5];
    pf.minute = t[6];
    pf.second = t[7];
  }

  memcpy(pf.listId, "KFM_001", 8);

  size_t p = (size_t)listPos + sizeof(listPattern);

  // Meter ID: 09 10 <16 ASCII bytes>
  if (p + 18 > len || buf[p] != 0x09 || buf[p + 1] != 0x10) {
    return false;
  }
  for (uint8_t i = 0; i < 16; i++) {
    pf.meterId[i] = (char)buf[p + 2 + i];
  }
  pf.meterId[16] = '\0';
  p += 18;

  // Meter type: 09 08 <8 ASCII bytes>
  if (p + 10 > len || buf[p] != 0x09 || buf[p + 1] != 0x08) {
    return false;
  }
  for (uint8_t i = 0; i < 8; i++) {
    pf.meterType[i] = (char)buf[p + 2 + i];
  }
  pf.meterType[8] = '\0';
  p += 10;

  /**
   * Measurement order in the observed KFM_001 payload:
   *   1. Active import power
   *   2. Active export power
   *   3. Reactive import power
   *   4. Reactive export power
   *   5. Current channel 1
   *   6. Current channel 2
   *   7. Current channel 3
   *   8. Voltage channel 1
   *   9. Voltage channel 2
   *  10. Voltage channel 3
   */
  uint32_t vals[10];

  for (uint8_t i = 0; i < 10; i++) {
    if (p + 5 > len || buf[p] != 0x06) {
      return false;
    }
    vals[i] = readBE32(buf + p + 1);
    p += 5;
  }

  pf.pImportW   = vals[0];
  pf.pExportW   = vals[1];
  pf.qImportVar = vals[2];
  pf.qExportVar = vals[3];
  pf.i1mA       = vals[4];
  pf.i2mA       = vals[5];
  pf.i3mA       = vals[6];
  pf.u1dV       = vals[7];
  pf.u2dV       = vals[8];
  pf.u3dV       = vals[9];

  pf.valid = true;
  out = pf;
  return true;
}

// ============================================================================
// Averaging
// ============================================================================

/**
 * @brief Add a decoded frame to the rolling history buffer.
 *
 * @param pf Decoded frame to store.
 */
void addToHistory(const PowerFrame &pf) {
  history[historyHead] = pf;
  historyHead = (historyHead + 1) % AVG_WINDOW_FRAMES;

  if (historyCount < AVG_WINDOW_FRAMES) {
    historyCount++;
  }
}

/**
 * @brief Print rolling average values based on stored history.
 */
void printAverages() {
  if (historyCount == 0) {
    return;
  }

  uint64_t sumPImport = 0;
  uint64_t sumPExport = 0;
  uint64_t sumQImport = 0;
  uint64_t sumQExport = 0;

  uint64_t sumI1 = 0;
  uint64_t sumI2 = 0;
  uint64_t sumI3 = 0;

  uint64_t sumU1 = 0;
  uint64_t sumU2 = 0;
  uint64_t sumU3 = 0;

  for (uint8_t i = 0; i < historyCount; i++) {
    sumPImport += history[i].pImportW;
    sumPExport += history[i].pExportW;
    sumQImport += history[i].qImportVar;
    sumQExport += history[i].qExportVar;

    sumI1 += history[i].i1mA;
    sumI2 += history[i].i2mA;
    sumI3 += history[i].i3mA;

    sumU1 += history[i].u1dV;
    sumU2 += history[i].u2dV;
    sumU3 += history[i].u3dV;
  }

  Serial.println(F("Averages:"));
  Serial.print(F("  Window frames: ")); Serial.println(historyCount);

  Serial.print(F("  P+ avg: ")); Serial.print((float)sumPImport / historyCount, 1); Serial.println(F(" W"));
  Serial.print(F("  P- avg: ")); Serial.print((float)sumPExport / historyCount, 1); Serial.println(F(" W"));
  Serial.print(F("  Q+ avg: ")); Serial.print((float)sumQImport / historyCount, 1); Serial.println(F(" var"));
  Serial.print(F("  Q- avg: ")); Serial.print((float)sumQExport / historyCount, 1); Serial.println(F(" var"));

  Serial.print(F("  I1 avg: ")); Serial.print(((float)sumI1 / historyCount) / 1000.0, 3); Serial.println(F(" A"));
  Serial.print(F("  I2 avg: ")); Serial.print(((float)sumI2 / historyCount) / 1000.0, 3); Serial.println(F(" A"));
  Serial.print(F("  I3 avg: ")); Serial.print(((float)sumI3 / historyCount) / 1000.0, 3); Serial.println(F(" A"));

  Serial.print(F("  U1 avg: ")); Serial.print(((float)sumU1 / historyCount) / 10.0, 1); Serial.println(F(" V"));
  Serial.print(F("  U2 avg: ")); Serial.print(((float)sumU2 / historyCount) / 10.0, 1); Serial.println(F(" V"));
  Serial.print(F("  U3 avg: ")); Serial.print(((float)sumU3 / historyCount) / 10.0, 1); Serial.println(F(" V"));
}

// ============================================================================
// Reporting
// ============================================================================

/**
 * @brief Print one decoded frame and the current rolling averages.
 *
 * @param pf Decoded frame to print.
 */
void printReport(const PowerFrame &pf) {
  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F("Decoded Kaifa KFM_001 power frame"));
  Serial.println(F("================================================"));

  Serial.print(F("Meter ID:   "));
  Serial.println(pf.meterId);

  Serial.print(F("Meter type: "));
  Serial.println(pf.meterType);

  Serial.print(F("Timestamp:  "));
  Serial.print(pf.year);
  Serial.print('-');
  if (pf.month < 10) Serial.print('0');
  Serial.print(pf.month);
  Serial.print('-');
  if (pf.day < 10) Serial.print('0');
  Serial.print(pf.day);
  Serial.print(' ');
  if (pf.hour < 10) Serial.print('0');
  Serial.print(pf.hour);
  Serial.print(':');
  if (pf.minute < 10) Serial.print('0');
  Serial.print(pf.minute);
  Serial.print(':');
  if (pf.second < 10) Serial.print('0');
  Serial.println(pf.second);

  Serial.println(F("Latest values:"));
  Serial.print(F("  Active import  (P+): ")); Serial.print(pf.pImportW);   Serial.println(F(" W"));
  Serial.print(F("  Active export  (P-): ")); Serial.print(pf.pExportW);   Serial.println(F(" W"));
  Serial.print(F("  Reactive import(Q+): ")); Serial.print(pf.qImportVar); Serial.println(F(" var"));
  Serial.print(F("  Reactive export(Q-): ")); Serial.print(pf.qExportVar); Serial.println(F(" var"));

  Serial.print(F("  Current L1: ")); Serial.print(pf.i1mA / 1000.0, 3); Serial.println(F(" A"));
  Serial.print(F("  Current L2: ")); Serial.print(pf.i2mA / 1000.0, 3); Serial.println(F(" A"));
  Serial.print(F("  Current L3: ")); Serial.print(pf.i3mA / 1000.0, 3); Serial.println(F(" A"));

  Serial.print(F("  Voltage L1: ")); Serial.print(pf.u1dV / 10.0, 1); Serial.println(F(" V"));
  Serial.print(F("  Voltage L2: ")); Serial.print(pf.u2dV / 10.0, 1); Serial.println(F(" V"));
  Serial.print(F("  Voltage L3: ")); Serial.print(pf.u3dV / 10.0, 1); Serial.println(F(" V"));

  printAverages();
}

// ============================================================================
// Frame processing
// ============================================================================

/**
 * @brief Process one complete frame.
 *
 * @param buf Pointer to frame payload.
 * @param len Payload length in bytes.
 */
void processFrame(const uint8_t *buf, size_t len) {
  if (len == 0) {
    return;
  }

  if (PRINT_FRAME_HEX) {
    Serial.println(F("Raw frame:"));
    printHexFrame(buf, len);
  }

  PowerFrame pf;
  if (parseKaifaKFM001(buf, len, pf)) {
    addToHistory(pf);
    printReport(pf);
  }
}

/**
 * @brief Feed one incoming byte into the frame assembler.
 *
 * Handles:
 *   - Frame delimiters (0x7E)
 *   - HDLC byte escaping (0x7D)
 *   - Frame overflow protection
 *
 * @param b Incoming byte from the HAN stream.
 */
void handleIncomingByte(uint8_t b) {
  // HDLC frame delimiter
  if (b == 0x7E) {
    if (frameStarted && frameLen > 0) {
      processFrame(frameBuf, frameLen);
    }

    // Start a new frame after every delimiter
    frameStarted = true;
    frameLen = 0;
    escapeNext = false;
    return;
  }

  // Ignore data until the first delimiter has been received
  if (!frameStarted) {
    return;
  }

  // Start of escaped byte
  if (b == 0x7D) {
    escapeNext = true;
    return;
  }

  // Unescape next byte if required
  if (escapeNext) {
    b ^= 0x20;
    escapeNext = false;
  }

  if (frameLen < MAX_FRAME_LEN) {
    frameBuf[frameLen++] = b;
  } else {
    // Drop oversized frames and wait for the next delimiter
    frameStarted = false;
    frameLen = 0;
    escapeNext = false;
    Serial.println(F("Frame overflow - dropped"));
  }
}

// ============================================================================
// Arduino entry points
// ============================================================================

/**
 * @brief Arduino setup function.
 */
void setup() {
  Serial.begin(PC_BAUD);
  while (!Serial) {
    ; // Safe on Uno/Nano, required on some USB-native boards
  }

  han.begin(HAN_BAUD);

  Serial.println();
  Serial.println(F("Kaifa KFM_001 HAN parser started"));
  Serial.print(F("RX pin: ")); Serial.println(HAN_RX_PIN);
  Serial.print(F("HAN baud: ")); Serial.println(HAN_BAUD);
  Serial.print(F("Inverted: ")); Serial.println(HAN_INVERTED ? F("true") : F("false"));
  Serial.print(F("Average window: ")); Serial.print(AVG_WINDOW_FRAMES); Serial.println(F(" frames"));
  Serial.println();
}

/**
 * @brief Arduino main loop.
 */
void loop() {
  while (han.available() > 0) {
    uint8_t b = (uint8_t)han.read();
    handleIncomingByte(b);
  }
}
