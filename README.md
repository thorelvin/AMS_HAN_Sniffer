# Kaifa KFM_001 HAN Parser for Arduino

A lightweight Arduino project for reading Norwegian AMS/HAN smart meter data from a Kaifa meter using a TTL-level M-Bus adapter.

This repository focuses on the Arduino side only. It is intended as a practical and low-cost reference implementation for receiving HAN data, reconstructing frames, decoding observed Kaifa `KFM_001` payloads, and printing live measurement values to the USB serial monitor.

The project is designed to run on Arduino Uno/Nano-class hardware using simple, widely available parts.

---

## Why this project

This project was created as a practical exploration of HAN/AMS communication and live smart meter data handling on low-cost embedded hardware.

It demonstrates:

- serial acquisition of HAN data through an M-Bus adapter
- HDLC-style frame reconstruction on Arduino
- decoding of observed Kaifa `KFM_001` payload fields
- live display of active power, reactive power, current, voltage, and timestamp data
- lightweight embedded handling without requiring expensive hardware

It is intended as a useful tool for testing, learning, and practical experimentation with Norwegian AMS/HAN meter data.

---

## Key features

- Reads HAN data from a Kaifa meter through a TTL-level M-Bus adapter
- Uses `SoftwareSerial` so the main USB serial port remains available for output
- Detects frame boundaries using `0x7E`
- Handles HDLC byte unescaping with `0x7D`
- Decodes observed Kaifa `KFM_001` payload data
- Prints:
  - meter ID
  - meter type
  - timestamp
  - active import power
  - active export power
  - reactive import power
  - reactive export power
  - current values
  - voltage values
- Maintains a rolling average over a configurable number of frames
- Supports optional raw hex frame output for debugging

---

## Professional relevance

This project demonstrates practical work with HAN/AMS meter data, serial communication, lightweight embedded decoding, and structured presentation of electrical measurement values. It is relevant as a hands-on example of smart metering, communication reliability, and troubleshooting-oriented data handling on embedded hardware.

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

The hardware setup has been verified in practice.

---

## Low-cost approach

This project is built around inexpensive and widely available hardware. It shows how a simple Arduino board and a low-cost M-Bus adapter can be used to access and interpret HAN/AMS meter data without specialized or expensive equipment.

By making live meter values easier to read and understand, the setup can help users study consumption patterns, identify costly load peaks, and improve everyday power usage through better awareness of their own data.

---

## Wiring diagram

The hardware is connected as follows:

- **Smart meter HAN port**
  - Use the HAN / RJ45 output from the meter
  - In this setup, the M-Bus pair is taken from **pin 1** and **pin 2**
  - Polarity does **not** matter for this connection

- **HAN / M-Bus to TTL adapter**
  - Connect the two HAN wires from the smart meter to the M-Bus input on the adapter
  - The adapter used in this setup is based on the **TI TSS721** M-Bus transceiver

- **Adapter to Arduino**
  - **Adapter data output / RXD** -> **Arduino D2**
  - **Adapter GND** -> **Arduino GND**

- **Arduino to PC**
  - Connect the Arduino to the PC with USB
  - The Arduino prints decoded values to the USB serial monitor
 
---
 
## Hardware reference images

### AMS Meter Nuri Kaifa
![AMS Meter](https://github.com/thorelvin/AMS_HAN_Sniffer_PC/blob/main/HAN/20260415_134246.jpg)

### Cat termination adapter
![Cat cable adapter](https://github.com/thorelvin/AMS_HAN_Sniffer_PC/blob/main/HAN/20260415_134317.jpg)

### Pin 1 and 2 connected, polarity does not matter
![Cat pin 1 and 2](https://github.com/thorelvin/AMS_HAN_Sniffer_PC/blob/main/HAN/20260415_134356.jpg)

### M-Bus adapter wiring  
The pin marked **RXD** (yellow cable) on the M-Bus adapter is the signal line used as data input to the Arduino in this setup.
![MBUS adapter wiring](https://github.com/thorelvin/AMS_HAN_Sniffer_PC/blob/main/HAN/20260415_134414.jpg)

### Arduino wiring  
Arduino wiring: data frames are received on **D2**.
![Arduino wiring](https://github.com/thorelvin/AMS_HAN_Sniffer_PC/blob/main/HAN/20260415_134532.jpg)

### Full wiring overview
![Complete setup](https://github.com/thorelvin/AMS_HAN_Sniffer_PC/blob/main/HAN/20260415_134558.jpg)

---

### Summary

```text
Smart meter HAN port (RJ45 pin 1 + pin 2)
        ->
M-Bus / HAN to TTL adapter
        ->
Adapter data output -> Arduino D2
Adapter GND         -> Arduino GND
        ->
Arduino USB
        ->
Serial Monitor / PC
```

### Notes

- `D2` is used as the receive pin for `SoftwareSerial`
- `D3` is defined as the required dummy TX pin for `SoftwareSerial`, but is not used
- If your adapter output is inverted, set `HAN_INVERTED = true` in the sketch

---

## Default serial settings

### HAN side
- Baud rate: `2400`

### USB serial output
- Baud rate: `115200`

Open the Arduino Serial Monitor at `115200 baud` to view decoded values.

---

## Installation

1. Clone or download this repository.
2. Open `kaifa_kfm001_han_parser.ino` in the Arduino IDE.
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
```

You can adjust these values depending on your adapter and preferred output behavior.

---

## Example output

```text
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
```

---

## Example use cases

- Verify that HAN data is being received correctly from the smart meter
- Inspect live import, export, current, and voltage values directly from Arduino
- Test M-Bus to TTL adapters with Norwegian AMS/HAN meters
- Capture and decode Kaifa `KFM_001` measurement frames
- Use Arduino as a simple decoding platform before moving data to a larger PC-based dashboard
- Learn how HAN/AMS data can be reconstructed and interpreted on embedded hardware

---

## Related repositories

This Arduino repository can be used together with the related PC-side repository:

- [AMS_HAN_Sniffer](https://github.com/thorelvin/AMS_HAN_Sniffer)  
  Arduino-focused repository for HAN frame acquisition and decoding.

- [AMS_HAN_Sniffer_PC](https://github.com/thorelvin/AMS_HAN_Sniffer_PC)  
  Python desktop application for live monitoring, CSV logging, visualization, event detection, phase analysis, and price-aware consumption insights.

Together, the two repositories form a complete HAN/AMS monitoring setup with clear separation between embedded frame handling and PC-based analysis.

---

## Important note

This project is a **personal learning and demonstration project**.

It is **not** a certified measuring instrument and should **not** be used as the sole basis for:

- electrical safety decisions
- official metering purposes
- billing verification
- formal fault diagnosis

Any suspected electrical faults, wiring issues, or safety concerns should be evaluated by a qualified electrician.

---

## Current status

This is an active practical project focused on:

- reliable HAN frame reception
- Kaifa `KFM_001` frame decoding
- stable serial output on Arduino Uno/Nano-class boards
- practical use of low-cost hardware for smart meter data access

---

## Future improvements

Possible next steps include:

- support for additional HAN payload formats
- support for additional meter models
- cleaner machine-readable serial output mode
- bridge mode for forwarding complete frames to a PC application
- improved long-term logging support
- optional indicators or status LEDs for frame reception

---

## Author

**Thor Elvin Valø**

---

## License

**MIT License**

