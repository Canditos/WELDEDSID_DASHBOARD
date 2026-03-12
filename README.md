# ESP32 IoT Controller - SICHARGE D Monitor

A monolithic, production-ready ESP32 IoT project featuring a modern web dashboard inspired by the Siemens SICHARGE D charging station monitor.

## 🚀 Features
- **Modern Dashboard**: Responsive Single Page Application (SPA).
- **WiFi Management**: Robust AP/STA fallback lifecycle.
- **Protocols**: WebSockets, MQTT, Modbus TCP/IP, and ArduinoOTA.
- **Hardware Control**: 
  - 8-Channel Relay Control (Open Drain).
  - Dual DAC Voltage Control (GP8403).
  - Persistent storage in NVS (Restores states on boot).

## 📁 Repository Structure
- `/src`: C++ Source files (Modular architecture).
- `/data`: Web assets (HTML/CSS/JS/Images).
- `/platformio.ini`: Project configuration.

## 🛠️ Setup Instructions
1. Install [PlatformIO](https://platformio.org/).
2. Clone this repository.
3. Open the project folder in VS Code with PlatformIO.
4. Run "Upload Filesystem Image" to flash the `data` folder.
5. Run "Upload" to flash the firmware.

## 👤 Credentials
- **Username**: `admin`
- **Password**: `admin123`

---
*Inspired by Siemens SICHARGE D Monitor.*
