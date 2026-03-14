describe("Relay controls", () => {
  beforeEach(() => {
    cy.mountDashboard();
  });

  it("sends relay toggle commands and updates button state from websocket init", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });

    cy.get("[data-cy='relay-btn-1']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "relay" && msg.idx === 1 && msg.state === true)).to.equal(true);
    });
  });

  it("sends a global reset command", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
    cy.get("[data-cy='reset-all-relays-btn']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "relay_mask" && msg.mask === 0)).to.equal(true);
    });
    cy.get("[data-cy='log-container']").should("contain.text", "Global Reset initiated");
  });

  it("sends group reset commands for each relay in a group", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });

    cy.get("[data-cy='reset-group-2-btn']").click();
    cy.window().its("__wsMessages").should((messages) => {
      const resets = messages.filter((msg) => msg.cmd === "relay" && msg.state === false);
      expect(resets.map((msg) => msg.idx)).to.include.members([4, 5]);
    });
  });
});
