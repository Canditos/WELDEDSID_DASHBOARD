# WELDEDSID_DASHBOARD Project Presentation

**Languages:** [English](apresentacao-projeto.en.md) | [Portugues](apresentacao-projeto.md)

## Goal

The `WELDEDSID_DASHBOARD` project delivers an industrial control panel based on an `ESP32`, with a web dashboard embedded directly on the device. The system was designed to monitor and command digital outputs and analog channels in a simple, visual, and secure way.

From a presentation standpoint, the project demonstrates:
- real-time remote relay control
- analog voltage adjustment via DAC
- monitoring of network and service states
- Wi-Fi and security configuration from the dashboard
- automated UI testing with Cypress

## Architecture Summary

The system is split into two main parts:

1. Firmware running on the `ESP32`
   - initializes hardware, network, web server, WebSocket, MQTT, Modbus, and OTA
   - stores configuration in persistent `NVS`
   - executes commands received from the dashboard

2. Embedded web dashboard
   - served directly by the ESP32 from `SPIFFS`
   - operates relays, DACs, Wi-Fi config, and security
   - receives real-time updates through `WebSocket`

## Main Features

### 1. Relay Control

The system exposes `8 relays`, grouped by function:
- `Group 1`: Feedback Relay
- `Group 2`: 24V DC
- `Group 3`: Main

The dashboard allows:
- turning each relay on/off individually
- group reset
- global reset for all relays
- real-time visual state feedback

### 2. Analog Control via DAC

The project includes `2 DAC channels`:
- `TF Voltage` with a `0.0V to 10.0V` range
- `Dispenser Temp` with a `0.0V to 10.0V` range

The dashboard supports:
- slider adjustment
- numeric input adjustment
- preset values `DEFAULT`, `MID`, and `MAX`
- the `Dispenser Temp STEP` mode

### 3. Operational Status

At the top of the dashboard the operational indicators show:
- Wi-Fi state
- MQTT state
- Modbus state
- WebSocket state
- current device IP

This makes it easier to read the overall system status during operation and demos.

### 4. Configuration and Security

The project includes:
- Wi-Fi network configuration
- different user profiles
- persistent OTA password
- configuration backup, import, and reset

These features make the solution closer to a real product, not just a technical prototype.

### 5. Logs and Diagnostics

The dashboard includes a log area with:
- important event records
- category filters
- pagination to keep the UI readable
- log export and clear actions

## Technologies Used

- `ESP32`
- `PlatformIO`
- `Arduino Framework`
- `SPIFFS`
- `ESPAsyncWebServer`
- `WebSocket`
- `MQTT`
- `Modbus TCP`
- `OTA`
- `HTML`, `CSS`, `JavaScript`
- `Cypress`
- `Playwright`

## Project Structure

- [src](../src)
  Main firmware and system modules
- [data](../data)
  Embedded dashboard assets
- [cypress](../cypress)
  Automated UI tests
- [docs](../docs)
  Technical and presentation documentation

## Project Value

This project shows the integration between hardware, firmware, web UI, and automated testing. In a professional context, it demonstrates skills in:
- embedded systems
- networks and communications
- frontend development
- test automation
- project organization and documentation

## Cypress Demo

A demo spec is available at:
- [presentation-demo.cy.js](../cypress/e2e/presentation-demo.cy.js)

This demo:
- uses mocks to avoid touching real Wi-Fi
- logs in automatically
- shows system state chips
- toggles relays
- changes DAC values
- runs the `Dispenser Temp STEP`
- navigates logs with visible pauses
- shows numbered steps in the Cypress runner for live demos

### How to run the demo

1. Start the local dashboard server:

```powershell
npm run serve:data
```

2. Run the demo in visual mode:

```powershell
npm run cypress:demo
```

### What you see during the demo

- login flow
- operational dashboard
- relay actions
- DAC actions
- step mode execution
- visual log confirmation

## Final Note for Presentation

For a work presentation, a good flow is:

1. The problem the system solves
2. Overall architecture `ESP32 + web dashboard`
3. Main features
4. Security and configuration
5. Cypress tests
6. Live demo

This keeps the presentation balanced between functional overview, technical value, and practical proof.
