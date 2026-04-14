# Kaifa KFM_001 HAN Parser for Arduino

An Arduino sketch for reading Norwegian AMS/HAN meter data from a Kaifa meter using a TTL-level M-Bus adapter such as a TSS721 module.

The sketch receives raw HAN data over `SoftwareSerial`, reconstructs HDLC-style frames, detects Kaifa `KFM_001` payloads, extracts live measurement values, and prints both instantaneous readings and rolling averages to the USB serial monitor.

This project is intended as a lightweight reference implementation for testing, decoding, and logging live HAN data on Arduino Uno/Nano-class boards.

---

## Features

- Receives HAN data from a Kaifa meter through a TTL M-Bus adapter
- Uses `SoftwareSerial` so the hardware USB serial port remains available for debug output
- Detects frame boundaries using `0x7E`
- Handles HDLC byte unescaping (`0x7D`)
- Decodes observed Kaifa `KFM_001` payload fields
- Prints:
  - meter ID
  - meter type
  - timestamp
  - active import/export power
  - reactive import/export power
  - current values
  - voltage values
- Maintains a rolling average over a configurable number of frames
- Optional raw frame hex output for debugging

---

## Supported payload

This sketch is written for the Kaifa `KFM_001` HAN payload format observed on Norwegian AMS installations.

The decoder currently extracts the following values from the long measurement frame:

- Meter ID
- Meter type
- Meter clock / timestamp
- Active import power
- Active export power
- Reactive import power
- Reactive export power
- Current channel 1
- Current channel 2
- Current channel 3
- Voltage channel 1
- Voltage channel 2
- Voltage channel 3

The field mapping is based on observed frames and practical testing with a live meter.

---

## Tested hardware

This project has been tested with:

- Arduino Uno / Nano class board
- TSS721-based M-Bus to TTL adapter
- Kaifa AMS meter exposing `KFM_001` data on the HAN port

---

## Wiring

### Adapter to Arduino

- Adapter data output -> `D2`
- Adapter GND -> `GND`

### Notes

- `D2` is used as the receive pin for `SoftwareSerial`
- `D3` is defined as the required dummy TX pin for `SoftwareSerial`, but is not used
- If your adapter output is inverted, set `HAN_INVERTED = true` in the sketch

---

## Default serial settings

### HAN side

- Baud rate: `2400`

### USB debug output

- Baud rate: `115200`

Open the Arduino Serial Monitor at `115200 baud` to view decoded values.

---

## Installation

1. Clone or download this repository.
2. Open the sketch in the Arduino IDE.
3. Make sure the `SoftwareSerial` library is available. It is included with the standard Arduino AVR core.
4. Connect the adapter output to `D2` and common ground to `GND`.
5. Select the correct board and COM port in the Arduino IDE.
6. Upload the sketch.
7. Open Serial Monitor at `115200 baud`.

---

## Configuration

The most important user settings are located at the top of the sketch:

```cpp
const uint8_t HAN_RX_PIN = 2;
const uint8_t HAN_TX_PIN = 3;
const bool HAN_INVERTED = false;

const unsigned long HAN_BAUD = 2400;
const unsigned long PC_BAUD  = 115200;

const uint8_t AVG_WINDOW_FRAMES = 6;
const bool PRINT_FRAME_HEX = false;




Example output
================================================
Decoded Kaifa KFM_001 power frame
================================================
Meter ID:   6970631408353607
Meter type: MA304H3E
Timestamp:  2026-04-14 21:24:30
Latest values:
  Active import  (P+): 2830 W
  Active export  (P-): 0 W
  Reactive import(Q+): 0 var
  Reactive export(Q-): 93 var
  Current L1: 9.445 A
  Current L2: 3.847 A
  Current L3: 9.548 A
  Voltage L1: 233.1 V
  Voltage L2: 0.0 V
  Voltage L3: 233.1 V
Averages:
  Window frames: 3
  P+ avg: 2812.0 W
  P- avg: 0.0 W
  Q+ avg: 0.0 var
  Q- avg: 94.7 var
  I1 avg: 9.392 A
  I2 avg: 3.874 A
  I3 avg: 9.464 A
  U1 avg: 233.3 V
  U2 avg: 0.0 V
  U3 avg: 232.9 V
