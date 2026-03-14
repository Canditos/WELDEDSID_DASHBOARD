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
    cy.get("[data-cy='dac-val-1']").should("have.text", "2.4 V");
    cy.get("[data-cy='dac-val-2']").should("have.text", "6.5 V");
  });
});
