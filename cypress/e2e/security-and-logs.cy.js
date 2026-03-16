describe("Security settings and logs", () => {
  beforeEach(() => {
    cy.mountDashboard();
    cy.get("#network-panel-toggle").click();
  });

  it("loads security status and saves new credentials", () => {
    cy.get("#security-admin-user").should("have.value", "admin");
    cy.get("#security-admin-pass").type("newStrongPass");
    cy.get("#security-operator-pass").type("operatorPass");
    cy.get("#security-viewer-pass").type("viewerPass");
    cy.get("#security-ota-pass").type("ota-pass-123");
    cy.get("#save-security-btn").click();
    cy.wait("@securitySave").its("request.body").should("deep.equal", {
      users: [
        { role: "admin", username: "admin", password: "newStrongPass", enabled: true },
        { role: "operator", username: "operator", password: "operatorPass", enabled: true },
        { role: "viewer", username: "viewer", password: "viewerPass", enabled: true }
      ],
      otaPassword: "ota-pass-123"
    });
    cy.get("[data-cy='log-container']").should("contain.text", "Security settings updated");
  });

  it("refuses too-short admin passwords client-side", () => {
    cy.get("#security-admin-pass").type("123");
    cy.get("#save-security-btn").click();
    cy.get("@securitySave.all").should("have.length", 0);
    cy.get("[data-cy='log-container']").should("contain.text", "ADMIN password must have at least 8 characters");
  });

  it("clears and exports logs", () => {
    cy.get("[data-cy='log-container']").should("contain.text", "User authorized");
    cy.window().then((win) => cy.stub(win.URL, "createObjectURL").returns("blob:test").as("blobUrl"));
    cy.get("#download-logs-btn").click();
    cy.get("@blobUrl").should("have.been.called");
    cy.get("[data-cy='clear-logs-btn']").click();
    cy.get("[data-cy='log-container']").should("contain.text", "Logs cleared");
  });
});
