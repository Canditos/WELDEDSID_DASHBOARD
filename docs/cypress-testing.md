# Cypress Testing Guide

This document explains how Cypress is configured in this project, how the tests are executed, which variables affect execution, how the test harness works, and how to adapt the existing tests when adding new scenarios.

## Purpose

The Cypress suite is designed to validate the embedded dashboard locally without depending on a live ESP32, real Wi-Fi changes, or live backend services.

Instead of talking to the actual device, the suite:
- serves the real frontend from the `data/` folder
- mocks HTTP API responses with `cy.intercept()`
- replaces the browser `WebSocket` implementation with a local mock
- captures sent WebSocket commands through `window.__wsMessages`

This makes the suite safe, deterministic and fast for UI regression testing.

## Where Cypress Lives

Main files:

```text
cypress.config.mjs
cypress/
|-- e2e/
|   |-- auth-and-layout.cy.js
|   |-- dac-controls.cy.js
|   |-- integrated-controls.cy.js
|   |-- relay-controls.cy.js
|   |-- security-and-logs.cy.js
|   `-- wifi-safe-flows.cy.js
`-- support/
    `-- e2e.js
```

Related execution files:

```text
package.json
scripts/serve-data.mjs
```

## How the Tests Are Executed

### Local preview server

The dashboard is served locally from the real `data/` folder with:

```bash
npm run serve:data
```

By default this serves the app at:

```text
http://127.0.0.1:4173
```

### Open Cypress interactively

```bash
npm run cypress:open
```

### Run headless

```bash
npm run cypress:run
```

### Typical local flow

Terminal 1:

```bash
npm run serve:data
```

Terminal 2:

```bash
npm run cypress:run
```

## Configuration

The main Cypress configuration is in [cypress.config.mjs](../cypress.config.mjs).

Current behavior:
- `baseUrl` defaults to `http://127.0.0.1:4173`
- `specPattern` is `cypress/e2e/**/*.cy.{js,mjs}`
- support file is `cypress/support/e2e.js`
- video is disabled
- screenshots on failure are enabled
- viewport is `1440 x 1100`

## Environment Variables

### `CYPRESS_BASE_URL`

The only runtime variable currently used directly by Cypress config is:

```bash
CYPRESS_BASE_URL
```

It overrides the default base URL:

```bash
CYPRESS_BASE_URL=http://127.0.0.1:4173 npm run cypress:run
```

On Windows PowerShell:

```powershell
$env:CYPRESS_BASE_URL='http://127.0.0.1:4173'
npm run cypress:run
```

Most of the time you do not need to set it, because the project already assumes the local preview server.

## The Test Harness

The shared harness lives in [cypress/support/e2e.js](../cypress/support/e2e.js).

It does 4 important things:

1. seeds a default dashboard state
2. mocks REST endpoints with `cy.intercept()`
3. overrides `window.WebSocket` with a custom mock class
4. exposes sent WebSocket messages in `window.__wsMessages`

### Default seeded state

The suite starts from a default mocked device state:

```js
const DEFAULT_STATE = {
  type: "init",
  relays: [true, false, true, false, false, true, false, true],
  v1: 2.4,
  v2: 6.5,
  mqtt: true,
  modbus: true,
  wifiMode: 2
};
```

This is useful because the dashboard is not tested from an empty DOM only; it is tested against a realistic first render.

### Mocked REST endpoints

The shared command `cy.mountDashboard()` currently intercepts:

- `GET /api/ws-auth`
- `GET /api/wifi/status`
- `GET /api/health`
- `GET /api/security/status`
- `POST /api/wifi/startScan`
- `GET /api/wifi/scan`
- `POST /api/security/users`
- `POST /api/wifi/save`

### Mocked WebSocket

The custom WebSocket mock responds to:

- `auth`
- `relay`
- `relay_mask`
- `dac`
- `step_ramp`

It also sends back `init` or `update` payloads so the frontend behaves as if it were connected to the real device.

### Captured command log

Every WebSocket command sent by the frontend is pushed into:

```js
window.__wsMessages
```

This is the main mechanism used to assert that the dashboard sent the expected control commands.

## Shared Command: `cy.mountDashboard()`

This command is the foundation of most tests.

Typical usage:

```js
beforeEach(() => {
  cy.mountDashboard();
});
```

It:
- visits the dashboard
- injects the WebSocket mock before app code runs
- performs login using the mocked auth flow
- waits for initial API calls
- ensures the main dashboard is visible

### Supported overrides

You can customize the mounted state by passing an options object:

```js
cy.mountDashboard({
  initialState: {
    relays: [false, false, false, false, false, false, false, false],
    v1: 2.1,
    v2: 4.5,
    mqtt: false,
    modbus: true
  },
  wifiStatus: {
    ssid: "",
    status: 6,
    ip: "192.168.4.1",
    rssi: 0,
    mode: 4
  },
  healthStatus: {
    wifiConnected: false,
    wifiMode: 4,
    wifiIp: "192.168.4.1",
    mqttConnected: false,
    modbusHealthy: true,
    heap: 150000
  },
  securityStatus: {
    adminUser: "admin",
    hasOtaPassword: true,
    usingDefaultPassword: false
  },
  wifiScan: [
    { ssid: "Workshop_AP", rssi: -61, secure: false }
  ]
});
```

These overrides are the easiest way to build new scenarios without rewriting the harness.

## Current Test Structure

### [auth-and-layout.cy.js](../cypress/e2e/auth-and-layout.cy.js)

Focus:
- invalid login behavior
- valid login flow
- first render layout
- seeded default state
- health/AP fallback status rendering

Use this file as base when testing:
- topbar states
- layout sections
- visual boot state
- auth-dependent content visibility

### [relay-controls.cy.js](../cypress/e2e/relay-controls.cy.js)

Focus:
- single relay toggles
- reset all relays
- group reset behavior

Use this file as base when testing:
- new relay groups
- button active states
- new relay command variants
- new operator actions around relay control

### [dac-controls.cy.js](../cypress/e2e/dac-controls.cy.js)

Focus:
- DAC numeric input
- DAC reset
- step program trigger

Use this file as base when testing:
- DAC clamping
- slider behavior
- presets
- special DAC command flows

### [integrated-controls.cy.js](../cypress/e2e/integrated-controls.cy.js)

Focus:
- mixed operator flow in a single session
- relays + DAC + ramp in one scenario

Use this file as base when testing:
- long operator sequences
- "happy path" end-to-end interactions
- regressions caused by multiple control areas interacting

### [security-and-logs.cy.js](../cypress/e2e/security-and-logs.cy.js)

Focus:
- loading security state
- saving multi-role users and OTA configuration
- client-side validation
- clear/export log flows

Use this file as base when testing:
- password rules
- role/permission changes
- auth UX
- new log features such as filters or categories

### [wifi-safe-flows.cy.js](../cypress/e2e/wifi-safe-flows.cy.js)

Focus:
- scan and list networks
- guard against empty SSID
- safe mocked save flow

Use this file as base when testing:
- Wi-Fi provisioning UX
- AP fallback flows
- validation around saved credentials

## How Assertions Are Done

The suite uses 3 main assertion styles:

### 1. DOM assertions

Example:

```js
cy.get("[data-cy='dac-val-2']").should("have.text", "8.5 V");
```

Use these when validating:
- displayed values
- visible sections
- button classes
- status labels

### 2. Intercept assertions

Example:

```js
cy.wait("@wifiSave");
```

Use these when validating:
- a request was made
- a request was not made
- sequence of calls

### 3. WebSocket command assertions

Example:

```js
cy.window().its("__wsMessages").should((messages) => {
  expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2)).to.equal(true);
});
```

Use these when validating:
- relay commands
- DAC commands
- step program trigger
- command payload correctness

## Selector Strategy

The suite relies mostly on `data-cy` attributes.

Examples:
- `data-cy='relay-btn-1'`
- `data-cy='dac-val-2'`
- `data-cy='execute-stepwise-btn'`
- `data-cy='status-wifi'`

Recommendation:
- keep using `data-cy` for stable selectors
- do not rely on visual text alone when a dedicated selector exists
- if you add a new dashboard control, also add a `data-cy`

## How to Create New Tests From the Existing Ones

### Pattern 1: Clone an existing spec and narrow the scope

If you want to add a new relay test:

1. start from [relay-controls.cy.js](../cypress/e2e/relay-controls.cy.js)
2. keep `cy.mountDashboard()`
3. trigger the new action
4. assert on `window.__wsMessages`
5. assert on the relevant DOM state

### Pattern 2: Reuse `cy.mountDashboard()` with overrides

If you want to simulate AP fallback:

1. start from [auth-and-layout.cy.js](../cypress/e2e/auth-and-layout.cy.js)
2. pass a custom `wifiStatus`
3. pass a custom `healthStatus`
4. assert topbar and status chips

This avoids changing the shared harness when the scenario is just a different initial state.

### Pattern 3: Add a new integrated operator flow

If you want to test a realistic workflow:

1. start from [integrated-controls.cy.js](../cypress/e2e/integrated-controls.cy.js)
2. keep a single `it(...)`
3. simulate a sequence of user actions
4. assert final UI state and command list

This is useful for regression tests after UI redesigns.

## Example: Add a New DAC Preset Test

Example structure:

```js
describe("DAC presets", () => {
  beforeEach(() => {
    cy.mountDashboard();
  });

  it("applies the DISPENSER TEMP mid preset", () => {
    cy.get("[data-dac-preset='2'][data-value='6.5']").click();
    cy.get("[data-cy='dac-val-2']").should("have.text", "6.5 V");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2 && msg.voltage === 6.5)).to.equal(true);
    });
  });
});
```

## Example: Add a New Health Badge Test

```js
it("shows MQTT as offline while Modbus stays ready", () => {
  cy.mountDashboard({
    healthStatus: {
      wifiConnected: true,
      wifiMode: 2,
      wifiIp: "192.168.0.222",
      mqttConnected: false,
      modbusHealthy: true,
      heap: 150000
    }
  });

  cy.get("[data-cy='status-mqtt']").should("have.class", "offline");
  cy.get("[data-cy='status-modbus']").should("have.class", "online");
});
```

## Best Practices for This Project

- Keep tests hardware-safe by mocking network and WebSocket paths.
- Prefer extending `cy.mountDashboard()` inputs before duplicating setup logic.
- Use `data-cy` selectors for every new interactive control.
- Keep one concern per spec unless you are intentionally writing an integrated flow.
- When the UI is redesigned, update selectors first, then behavior assertions.
- If a test is flaky because of UI transition timing, assert the command and the final state rather than transient animation classes.

## Current Limitations

- The WebSocket mock only covers the commands currently used by the dashboard tests.
- The suite validates the dashboard behavior, not real firmware/network side effects.
- The local server is static and does not emulate the full device stack.
- Browser downloads and screenshots are generated locally and should remain ignored by Git unless explicitly needed.

## Troubleshooting

### Cypress says it cannot connect to the base URL

Symptoms:
- warning about `Cannot Connect Base Url`
- tests do not start

Check:
1. the local server is running with `npm run serve:data`
2. the server is reachable at `http://127.0.0.1:4173`
3. `CYPRESS_BASE_URL` is not pointing to an old address

### The browser opens a native authentication popup

This is the floating login dialog shown by the browser itself, not the dashboard login UI.

Cause:
- a backend route replies with `WWW-Authenticate` and the browser decides to show a native Basic Auth prompt

Current project status:
- API routes were adjusted to return JSON `401` for unauthorized API requests instead of triggering the browser-native Basic Auth popup
- the intended login flow is the dashboard overlay, not the browser popup

If this ever comes back:
1. inspect unauthorized API responses
2. confirm they return JSON `401`
3. avoid `requestAuthentication()` on dashboard API endpoints

### A test fails after a UI redesign

Check in this order:
1. did a `data-cy` selector change
2. did visible text change
3. did timing change because of animations or async UI state
4. did the harness stop mocking a route the UI now depends on

### A WebSocket assertion fails

Remember that Cypress does not use a real backend socket here.

Check:
1. the command is implemented in `buildMockWebSocket()`
2. the dashboard really sends that command
3. the assertion is checking the correct payload in `window.__wsMessages`

### A log-related assertion becomes flaky

The dashboard now has log filters and more UI state.

Prefer:
- asserting the log text exists in the container
- or asserting only visible entries when a filter is active using `.log-entry:not(.hidden)`

### Screenshots or downloads appear locally

This is expected during Cypress runs:
- `cypress/screenshots/`
- `cypress/downloads/`

These should remain ignored by Git unless you intentionally want to keep a captured artifact.

## Suggested Next Improvements

- Add Cypress tests for the new DAC preset buttons.
- Add a log filter test for `ALL / AUTH / NET / DAC / RELAY`.
- Add a WebSocket reconnect scenario using the current topbar state chips.
- Add a visual smoke test for mobile layout if the dashboard keeps evolving.
