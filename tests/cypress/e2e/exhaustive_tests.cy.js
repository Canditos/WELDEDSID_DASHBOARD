describe('SIEMENS - SICHARGE D MONITOR - Exhaustive E2E Tests', () => {
  beforeEach(() => {
    cy.visit('/');
  });

  describe('Authentication Flow', () => {
    it('should show error with invalid credentials', () => {
      cy.get('[data-cy="login-username"]').type('wrong_user');
      cy.get('[data-cy="login-password"]').type('wrong_pass');
      cy.get('[data-cy="login-submit"]').click();
      cy.get('#login-error').should('be.visible').and('contain', 'Invalid credentials');
    });

    it('should login successfully with valid credentials', () => {
      cy.get('[data-cy="login-username"]').type('admin');
      cy.get('[data-cy="login-password"]').type('admin123');
      cy.get('[data-cy="login-submit"]').click();
      cy.get('#login-overlay').should('not.be.visible');
      cy.get('#main-container').should('be.visible');
      cy.get('[data-cy="status-device-ip"]').should('contain', '192.168.0.215');
    });
  });

  describe('Dashboard Interaction', () => {
    beforeEach(() => {
      // Login before each test in this block
      cy.get('[data-cy="login-username"]').type('admin');
      cy.get('[data-cy="login-password"]').type('admin123');
      cy.get('[data-cy="login-submit"]').click();
    });

    it('should update DAC 1 (TF Voltage) slider and input', () => {
      const targetValue = '2.5';
      cy.get('[data-cy="dac-slider-ch1"]').invoke('val', targetValue).trigger('input');
      cy.get('[data-cy="dac-value-display-ch1"]').should('contain', `${targetValue} V`);
      cy.get('[data-cy="dac-input-ch1"]').should('have.value', targetValue);
      
      // Test manual input
      cy.get('[data-cy="dac-input-ch1"]').clear().type('2.8{enter}');
      cy.get('[data-cy="dac-value-display-ch1"]').should('contain', '2.8 V');
      cy.get('[data-cy="dac-slider-ch1"]').should('have.value', '2.8');
    });

    it('should reset DAC 1 value', () => {
      cy.get('[data-cy="dac-input-ch1"]').clear().type('3.0{enter}');
      cy.get('[data-cy="dac-reset-btn-ch1"]').click();
      cy.get('[data-cy="dac-value-display-ch1"]').should('contain', '2.0 V');
    });

    it('should toggle Relay 0 (DC1)', () => {
      cy.get('[data-cy="relay-toggle-btn-0"]').click().should('have.class', 'active');
      cy.get('[data-cy="relay-toggle-btn-0"]').click().should('not.have.class', 'active');
    });

    it('should reset all relays', () => {
      // Activate a few relays
      cy.get('[data-cy="relay-toggle-btn-0"]').click();
      cy.get('[data-cy="relay-toggle-btn-4"]').click();
      
      cy.get('[data-cy="relay-reset-all-btn"]').click();
      
      cy.get('[data-cy="relay-toggle-btn-0"]').should('not.have.class', 'active');
      cy.get('[data-cy="relay-toggle-btn-4"]').should('not.have.class', 'active');
    });

    it('should verify status badges presence', () => {
      cy.get('[data-cy="status-ws-badge"]').should('be.visible');
      cy.get('[data-cy="status-mqtt-badge"]').should('be.visible');
    });

    it('should logout and return to login screen', () => {
      cy.get('[data-cy="auth-logout-btn"]').click();
      cy.get('#login-overlay').should('be.visible');
    });
  });
});
