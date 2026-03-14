describe("Wi-Fi safe flows", () => {
  beforeEach(() => {
    cy.mountDashboard();
    cy.get("#network-panel-toggle").click();
  });

  it("scans and lists available networks", () => {
    cy.get("[data-cy='scan-wifi-btn']").click();
    cy.wait("@startScan");
    cy.wait("@wifiScan");
    cy.get(".wifi-item").should("have.length", 2);
    cy.contains(".wifi-item", "Preview_Network").click();
    cy.get("[data-cy='wifi-ssid-input']").should("have.value", "Preview_Network");
  });

  it("does not call wifi save when SSID is empty", () => {
    cy.get("[data-cy='wifi-ssid-input']").clear();
    cy.window().then((win) => cy.stub(win, "alert").as("alert"));
    cy.get("[data-cy='save-connect-btn']").click();
    cy.get("@alert").should("have.been.called");
    cy.get("@wifiSave.all").should("have.length", 0);
  });

  it("submits wifi save through a mocked request only", () => {
    cy.get("[data-cy='wifi-ssid-input']").type("Workshop_AP");
    cy.get("[data-cy='wifi-pass-input']").type("secret-pass");
    cy.get("[data-cy='save-connect-btn']").click();
    cy.wait("@wifiSave").its("request.body").should("deep.equal", {
      ssid: "Workshop_AP",
      pass: "secret-pass"
    });
    cy.get("#offline-overlay").should("have.css", "display", "flex");
  });
});
