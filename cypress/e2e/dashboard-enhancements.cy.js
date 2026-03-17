describe("Dashboard V2 enhancements", () => {
  beforeEach(() => {
    cy.mountDashboard();
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
  });

  it("applies DAC presets and sends the expected command", () => {
    cy.contains("[data-dac-preset='2']", "MID").click();
    cy.get("[data-cy='dac-val-2']").should("have.text", "5.0 V");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2 && msg.voltage === 5)).to.equal(true);
    });
  });

  it("filters logs by category without deleting entries", () => {
    cy.get("[data-cy='relay-btn-1']").click();
    cy.get("[data-cy='execute-stepwise-btn']").click();
    cy.get("[data-cy='log-container']").should("contain.text", "DC2 FB is now ON");
    cy.contains(".log-filter-btn", "RELAY").click();
    cy.get(".log-filter-btn.active").should("contain.text", "RELAY");
    cy.get(".log-entry .log-type").each(($item) => {
      cy.wrap($item).should("have.text", "RELAY");
    });
    cy.contains(".log-filter-btn", "ALL").click();
    cy.get(".log-filter-btn.active").should("contain.text", "ALL");
    cy.get("[data-cy='log-container']").should("contain.text", "Executing Dispenser Temp STEP");
  });

  it("shows operational summary chips and cards with live state", () => {
    cy.get("#wifi-mode-chip").should("contain.text", "WIFI MODE:");
    cy.get("#mqtt-mode-chip").should("contain.text", "MQTT:");
    cy.get("#modbus-mode-chip").should("contain.text", "MODBUS:");
    cy.get("#ws-mode-chip").should("contain.text", "WS: LIVE");
    cy.get("#ops-network-value").should("contain.text", "Connected");
    cy.get("#ops-relay-value").should("contain.text", "4 / 8");
    cy.get("#ops-dac-value").should("contain.text", "0.0 V / 0.0 V");
  });
});
