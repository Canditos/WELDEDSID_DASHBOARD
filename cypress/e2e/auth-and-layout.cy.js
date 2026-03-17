describe("Dashboard auth and layout", () => {
  it("blocks invalid login and loads the main dashboard after valid auth", () => {
    cy.intercept("GET", "/api/ws-auth", { statusCode: 401, body: {} }).as("badAuth");
    cy.visit("/");
    cy.get("#login-username").type("admin");
    cy.get("#login-password").type("wrongpass");
    cy.contains("button", "Authorize").click();
    cy.wait("@badAuth");
    cy.get("#login-error").should("be.visible");

    cy.mountDashboard();
    cy.get(".brand h1").should("contain", "SIEMENS");
    cy.get("[data-cy='status-wifi']").should("have.class", "online");
    cy.get("[data-cy='display-ip']").should("contain", "192.168.0.222");
    cy.get("#network-panel-body").should("not.have.class", "open");
    cy.get("#network-panel-toggle").click();
    cy.get("#network-panel-body").should("have.class", "open");
  });

  it("shows the expected seeded state on first render", () => {
    cy.mountDashboard();
    cy.get("[data-cy='relay-btn-0']").should("have.class", "active");
    cy.get("[data-cy='relay-btn-1']").should("not.have.class", "active");
    cy.get("[data-cy='dac-val-1']").should("have.text", "0.0 V");
    cy.get("[data-cy='dac-val-2']").should("have.text", "0.0 V");
  });

  it("reflects consolidated health and AP fallback state without fake green badges", () => {
    cy.mountDashboard({
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
      }
    });

    cy.get("[data-cy='wifi-status-text']").should("contain", "AP Fallback");
    cy.get("[data-cy='display-ip']").should("contain", "192.168.4.1");
    cy.get("[data-cy='status-wifi']").should("have.class", "offline");
    cy.get("[data-cy='status-mqtt']").should("have.class", "offline");
    cy.get("[data-cy='status-modbus']").should("have.class", "online");
  });
});
