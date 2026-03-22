describe("DAC controls", () => {
  beforeEach(() => {
    cy.mountDashboard();
  });

  it("updates DAC slider and sends command with clamped value", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });

    cy.get("[data-cy='dac-num-2']").clear().type("7.5").blur();
    cy.wait(180);
    cy.get("[data-cy='dac-val-2']").should("have.text", "7.5 V");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2 && msg.voltage === 7.5)).to.equal(true);
    });
  });

  it("resets DAC1 to its default value", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
    cy.get("[data-cy='reset-dac-1-btn']").click();
    cy.wait(100);
    cy.get("[data-cy='dac-val-1']").should("have.text", "0.0 V");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 1 && msg.voltage === 0)).to.equal(true);
    });
  });

  it("starts the stepwise program without touching Wi-Fi", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
    cy.get("[data-cy='execute-stepwise-btn']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "step_ramp" && msg.channel === 2)).to.equal(true);
    });
    cy.get("[data-cy='log-container']").should("contain.text", "Executing Dispenser Temp STEP");
  });

  it("stops the stepwise program when clicking the button again", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
    cy.get("[data-cy='execute-stepwise-btn']").click();
    cy.wait(120);
    cy.get("[data-cy='execute-stepwise-btn']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "stop_ramp" && msg.channel === 2)).to.equal(true);
    });
    cy.get("[data-cy='log-container']").should("contain.text", "Stopping Dispenser Temp STEP");
  });

  it("stops the step program when resetting DAC2", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
    cy.get("[data-cy='execute-stepwise-btn']").click();
    cy.wait(120);
    cy.get("[data-cy='reset-dac-2-btn']").click();
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "stop_ramp" && msg.channel === 2)).to.equal(true);
    });
    cy.get("[data-cy='dac-val-2']").should("have.text", "4.0 V");
  });
});
