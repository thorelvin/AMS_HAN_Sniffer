This project is an Arduino-based HAN reader for Norwegian AMS meters using the Kaifa KFM_001 data format. It receives TTL-level data from an M-Bus adapter such as a TSS721 module, reconstructs HDLC-style frames, extracts the most relevant measurement values, and outputs both instantaneous readings and rolling averages over USB serial.

The sketch is designed as a practical and lightweight solution for testing, logging, and understanding live meter data without requiring a more complex embedded platform. It currently decodes meter identity, timestamp, active and reactive power, per-channel current, and per-channel voltage values from observed KFM_001 payloads.

This implementation uses SoftwareSerial on Arduino Uno/Nano-class boards, which makes it convenient for quick testing with minimal hardware. For long-term or production use, a board with an additional hardware UART is recommended.

A slightly more professional version

This repository contains an Arduino sketch for receiving and decoding Norwegian AMS HAN telegrams from Kaifa meters that expose the KFM_001 payload format. The sketch reads serial data from a TTL M-Bus interface, detects HDLC frame boundaries, parses measurement fields, and reports live power data together with rolling averages over the USB serial interface.

It is intended as a compact, easy-to-understand reference implementation for development and field testing. The project has been validated with a TSS721-based M-Bus adapter and is suitable for bench testing, protocol inspection, and lightweight local monitoring.
