describe("Hardware Simulation - Full Test", () => {
  beforeEach(() => {
    cy.mountDashboard();
  });

  it("toggles all 10 relays and verifies websocket commands", () => {
    cy.window().then((win) => {
      // Clear previous messages to have a clean state
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });

    // We have 12 relays, index 0 to 11
    for (let i = 0; i < 12; i++) {
      cy.get(`[data-cy='relay-btn-${i}']`).click({ force: true });
      
      // Wait a tiny bit and then check if the WS message was logged
      cy.window().its("__wsMessages").should((messages) => {
        // Find the latest message for this relay
        const msg = messages.find(m => m.cmd === "relay" && m.idx === i);
        expect(msg).to.exist;
        expect(typeof msg.state).to.equal('boolean');
      });
    }
  });

  it("changes DAC sliders and verifies websocket commands", () => {
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });

    // DAC 1
    // Range is 2.5 to 4.0. Let's set it to 3.5.
    cy.get("[data-cy='dac-range-1']").invoke('val', 3.5).trigger('input').trigger('change');
    
    cy.window().its("__wsMessages").should((messages) => {
      const msg = messages.find(m => m.cmd === "dac" && m.channel === 1);
      expect(msg).to.exist;
      expect(msg.voltage).to.equal(3.5);
    });

    // DAC 2
    // Range is 4.0 to 9.0. Let's set it to 7.0.
    cy.get("[data-cy='dac-range-2']").invoke('val', 7.0).trigger('input').trigger('change');
    
    cy.window().its("__wsMessages").should((messages) => {
      const msg = messages.find(m => m.cmd === "dac" && m.channel === 2);
      expect(msg).to.exist;
      expect(msg.voltage).to.equal(7.0);
    });
  });
});

