const DEFAULT_STATE = {
  type: "init",
  relays: [true, false, true, false, false, true, false, true],
  v1: 0.0,
  v2: 4.0,
  mqtt: true,
  modbus: true,
  wifiMode: 2,
  role: "admin",
  username: "admin"
};

function buildMockWebSocket(initialState, ownerWindow) {
  const currentState = {
    ...initialState,
    relays: [...initialState.relays]
  };

  return class MockWebSocket {
    static OPEN = 1;

    constructor() {
      this.readyState = 1;
      this.sentMessages = [];
      this.ownerWindow = ownerWindow;
      setTimeout(() => this.onopen && this.onopen(), 10);
    }

    send(payload) {
      const data = JSON.parse(payload);
      this.sentMessages.push(data);
      if (!Array.isArray(this.ownerWindow.__wsMessages)) {
        this.ownerWindow.__wsMessages = [];
      }
      this.ownerWindow.__wsMessages.push(data);

      if (data.cmd === "auth") {
        setTimeout(() => {
          this.onmessage && this.onmessage({ data: JSON.stringify({ type: "auth", ok: true, role: currentState.role, username: currentState.username }) });
          this.onmessage && this.onmessage({ data: JSON.stringify(currentState) });
        }, 20);
        return;
      }

      if (data.cmd === "relay") {
        currentState.relays[data.idx] = data.state;
        setTimeout(() => {
          this.onmessage && this.onmessage({
            data: JSON.stringify({
              type: "update",
              idx: data.idx,
              state: data.state,
              v1: currentState.v1,
              v2: currentState.v2,
              mqtt: currentState.mqtt,
              modbus: currentState.modbus
            })
          });
        }, 20);
        return;
      }

      if (data.cmd === "relay_mask") {
        currentState.relays = currentState.relays.map(() => false);
        setTimeout(() => {
          this.onmessage && this.onmessage({
            data: JSON.stringify({
              type: "update",
              relays: currentState.relays,
              v1: currentState.v1,
              v2: currentState.v2,
              mqtt: currentState.mqtt,
              modbus: currentState.modbus
            })
          });
        }, 20);
        return;
      }

      if (data.cmd === "dac") {
        if (data.channel === 1) currentState.v1 = data.voltage;
        if (data.channel === 2) currentState.v2 = data.voltage;
        setTimeout(() => {
          this.onmessage && this.onmessage({
            data: JSON.stringify({
              type: "update",
              v1: currentState.v1,
              v2: currentState.v2,
              mqtt: currentState.mqtt,
              modbus: currentState.modbus
            })
          });
        }, 20);
        return;
      }

      if (data.cmd === "step_ramp") {
        setTimeout(() => {
          this.onmessage && this.onmessage({
            data: JSON.stringify({
              type: "update",
              v1: currentState.v1,
              v2: currentState.v2,
              mqtt: currentState.mqtt,
              modbus: currentState.modbus
            })
          });
        }, 20);
      }
    }

    close() {
      this.readyState = 3;
      this.onclose && this.onclose();
    }
  };
}

Cypress.Commands.add("mountDashboard", (options = {}) => {
  const initialState = { ...DEFAULT_STATE, ...(options.initialState || {}) };
  const wifiStatus = options.wifiStatus || {
    ssid: "Preview_Network",
    status: 3,
    ip: "192.168.0.222",
    rssi: -48,
    mode: 2
  };
  const healthStatus = options.healthStatus || {
    wifiConnected: true,
    wifiMode: 2,
    wifiIp: "192.168.0.222",
    mqttConnected: true,
    modbusHealthy: true,
    heap: 182344
  };
  const securityStatus = options.securityStatus || {
    users: [
      { role: "admin", username: "admin", enabled: true, usingDefault: false },
      { role: "operator", username: "operator", enabled: true, usingDefault: false },
      { role: "viewer", username: "viewer", enabled: true, usingDefault: false }
    ],
    hasOtaPassword: true
  };
  const wifiScan = options.wifiScan || [
    { ssid: "Preview_Network", rssi: -48, secure: true },
    { ssid: "Workshop_AP", rssi: -61, secure: false }
  ];

  cy.intercept("GET", "/api/ws-auth", { statusCode: 200, body: { token: "preview-token", role: "admin", username: "admin" } }).as("wsAuth");
  cy.intercept("GET", "/api/wifi/status", { statusCode: 200, body: wifiStatus }).as("wifiStatus");
  cy.intercept("GET", "/api/health", { statusCode: 200, body: healthStatus }).as("healthStatus");
  cy.intercept("GET", "/api/security/status", { statusCode: 200, body: securityStatus }).as("securityStatus");
  cy.intercept("POST", "/api/wifi/startScan", { statusCode: 200, body: { status: "scanning" } }).as("startScan");
  cy.intercept("GET", "/api/wifi/scan", { statusCode: 200, body: wifiScan }).as("wifiScan");
  cy.intercept("POST", "/api/security/users", (req) => {
    req.reply({ statusCode: 200, body: { status: "ok" } });
  }).as("securitySave");
  cy.intercept("POST", "/api/wifi/save", (req) => {
    req.reply({ statusCode: 200, body: { status: "ok" } });
  }).as("wifiSave");

  cy.visit("/", {
    onBeforeLoad(win) {
      win.__wsMessages = [];
      win.WebSocket = buildMockWebSocket(initialState, win);
    }
  });

  cy.get("#login-username").clear().type("admin");
  cy.get("#login-password").type("admin123");
  cy.contains("button", "Authorize").click();
  cy.wait("@wsAuth");
  cy.wait("@wifiStatus");
  cy.wait("@healthStatus");
  cy.wait("@securityStatus");
  cy.get("[data-cy='relay-control-center-title']").should("be.visible");
  cy.get("#ws-mode-chip").should("contain.text", "WS: LIVE");
});
