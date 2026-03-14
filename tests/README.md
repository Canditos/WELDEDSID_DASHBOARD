# SICHARGE D Monitor - E2E Testing Suite

This directory contains exhaustive End-to-End (E2E) tests for the SIEMENS SICHARGE D Monitor web interface.

## Prerequisites

- **Node.js**: Installed on your machine.
- **ESP32**: Powered on and connected to your network.

## Installation

1. Open a terminal in this folder (`NOVO_ESP/tests`).
2. Install dependencies:
   ```bash
   npm install
   ```

## Configuration

The tests are currently configured to point to `http://192.168.0.213`. 
If your device IP changes:
- Update `baseUrl` in `cypress/e2e/exhaustive_tests.cy.js`.
- Update `baseUrl` in `playwright/exhaustive_tests.spec.js`.

## Running Tests

### 1. Cypress (Interactive)
Best for development and watching the tests run in real-time.
```bash
npm run cypress:open
```

### 2. Cypress (Headless)
Runs the tests in the background.
```bash
npm run cypress:run
```

### 3. Playwright (Multi-browser)
Tests the app across Chromium, Firefox, and WebKit simultaneously.
```bash
npm run playwright:test
```

## Features Covered

- **Authentication**: Valid and invalid login attempts.
- **DAC Control**: Sliders, text inputs, and reset buttons.
- **Relays**: All 8 relays (DC1-DC4 + DC+ / DC-), including "Reset All".
- **Status Monitoring**: Verification of WebSocket and MQTT badges.
- **Logs**: Ensuring system log container is present and operational.

---
*Created by Antigravity AI Assistant*
