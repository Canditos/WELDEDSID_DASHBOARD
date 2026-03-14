describe("Integrated control flow", () => {
  beforeEach(() => {
    cy.mountDashboard();
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
  });

  it("runs relay, DAC and step-ramp actions in one realistic session", () => {
    cy.get("[data-cy='relay-btn-1']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "relay" && msg.idx === 1 && msg.state === true)).to.equal(true);
    });

    cy.get("[data-cy='relay-btn-4']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "relay" && msg.idx === 4 && msg.state === true)).to.equal(true);
    });

    cy.get("[data-cy='dac-num-1']").clear().type("2.8").blur();
    cy.get("[data-cy='dac-val-1']").should("have.text", "2.8 V");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 1 && msg.voltage === 2.8)).to.equal(true);
    });

    cy.get("[data-cy='dac-range-2']").invoke("val", 8.5).trigger("input");
    cy.wait(180);
    cy.get("[data-cy='dac-val-2']").should("have.text", "8.5 V");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2 && msg.voltage === 8.5)).to.equal(true);
    });

    cy.get("[data-cy='execute-stepwise-btn']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "step_ramp" && msg.channel === 2 && msg.start === 4 && msg.target === 9)).to.equal(true);
    });

    cy.get("[data-cy='log-container']").should("contain.text", "Executing stepwise program on Ch2");
  });
});
