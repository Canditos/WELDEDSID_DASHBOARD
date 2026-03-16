describe("Presentation demo flow", () => {
  const stepPause = 1200;
  const shortPause = 700;

  const announceStep = (label, detail) => {
    cy.log(`================ ${label} ================`);
    cy.log(detail);
  };

  beforeEach(() => {
    cy.mountDashboard({
      initialState: {
        relays: [false, false, false, false, false, false, false, false],
        v1: 2.0,
        v2: 4.0,
        mqtt: true,
        modbus: true
      }
    });
    cy.window().then((win) => {
      win.__wsMessages = win.__wsMessages.filter((msg) => msg.cmd === "auth");
    });
  });

  it("shows the main project features in a slow visual walkthrough", () => {
    announceStep("PASSO 1", "Login concluido e dashboard pronta para demonstracao");
    cy.get("#ws-mode-chip").should("contain.text", "WS: LIVE");
    cy.get("#display-ip").should("contain.text", "Device:");
    cy.wait(stepPause);

    announceStep("PASSO 2", "Mostrar os estados principais do sistema no topo");
    cy.get("[data-cy='status-wifi']").should("contain.text", "WIFI");
    cy.get("[data-cy='status-mqtt']").should("contain.text", "MQTT");
    cy.get("[data-cy='status-modbus']").should("contain.text", "MODBUS");
    cy.wait(stepPause);

    announceStep("PASSO 3", "Ativar reles para demonstrar o controlo digital");
    cy.get("[data-cy='relay-btn-0']").click();
    cy.wait(shortPause);
    cy.get("[data-cy='relay-btn-4']").click();
    cy.wait(stepPause);
    cy.get("[data-cy='relay-btn-0']").should("have.class", "active");
    cy.get("[data-cy='relay-btn-4']").should("have.class", "active");

    announceStep("PASSO 4", "Ajustar TF Voltage atraves do campo numerico");
    cy.get("[data-cy='dac-num-1']").clear().type("2.8").blur();
    cy.wait(stepPause);
    cy.get("[data-cy='dac-val-1']").should("have.text", "2.8 V");

    announceStep("PASSO 5", "Ajustar Dispenser Temp atraves do slider");
    cy.get("[data-cy='dac-range-2']").invoke("val", 7.5).trigger("input");
    cy.wait(stepPause);
    cy.get("[data-cy='dac-val-2']").should("have.text", "7.5 V");

    announceStep("PASSO 6", "Executar o modo step para o segundo DAC");
    cy.get("[data-cy='execute-stepwise-btn']").click();
    cy.wait(stepPause);

    announceStep("PASSO 7", "Mostrar os logs e a filtragem por categoria");
    cy.get("[data-cy='log-container']").should("contain.text", "Executing Dispenser Temp STEP");
    cy.contains(".log-filter-btn", "DAC").click();
    cy.wait(shortPause);
    cy.get(".log-filter-btn.active").should("contain.text", "DAC");
    cy.contains(".log-filter-btn", "ALL").click();
    cy.wait(shortPause);

    announceStep("PASSO 8", "Confirmar os comandos emitidos pela interface");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "relay" && msg.idx === 0 && msg.state === true)).to.equal(true);
      expect(messages.some((msg) => msg.cmd === "relay" && msg.idx === 4 && msg.state === true)).to.equal(true);
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 1 && msg.voltage === 2.8)).to.equal(true);
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2 && msg.voltage === 7.5)).to.equal(true);
      expect(messages.some((msg) => msg.cmd === "step_ramp" && msg.channel === 2)).to.equal(true);
    });

    announceStep("FIM", "Demo concluida com sucesso");
    cy.wait(stepPause);
  });
});
