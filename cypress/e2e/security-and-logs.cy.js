describe("Security settings and logs", () => {
  beforeEach(() => {
    cy.mountDashboard();
    cy.get("#network-panel-toggle").click();
  });

  it("loads security status and saves new credentials", () => {
    cy.get("#security-admin-user").should("have.value", "admin");
    cy.get("#security-admin-pass").type("newStrongPass");
    cy.get("#security-ota-pass").type("ota-pass-123");
    cy.get("#save-security-btn").click();
    cy.wait("@securitySave").its("request.body").should("deep.equal", {
      adminUser: "admin",
      adminPass: "newStrongPass",
      otaPassword: "ota-pass-123"
    });
    cy.get("[data-cy='log-container']").should("contain.text", "Security settings updated");
  });

  it("refuses too-short admin passwords client-side", () => {
    cy.get("#security-admin-pass").type("123");
    cy.get("#save-security-btn").click();
    cy.get("@securitySave.all").should("have.length", 0);
    cy.get("[data-cy='log-container']").should("contain.text", "Admin password must have at least 8 characters");
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
