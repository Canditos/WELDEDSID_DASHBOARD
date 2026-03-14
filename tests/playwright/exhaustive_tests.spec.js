const { test, expect } = require('@playwright/test');

const baseUrl = 'http://192.168.0.213';

test.describe('SIEMENS - SICHARGE D MONITOR - Exhaustive E2E Tests', () => {

  test.beforeEach(async ({ page }) => {
    await page.goto(baseUrl);
  });

  test.describe('Authentication Flow', () => {
    test('should show error with invalid credentials', async ({ page }) => {
      await page.locator('[data-cy="login-username"]').fill('wrong_user');
      await page.locator('[data-cy="login-password"]').fill('wrong_pass');
      await page.locator('[data-cy="login-submit"]').click();
      
      const error = page.locator('#login-error');
      await expect(error).toBeVisible();
      await expect(error).toContainText('Invalid credentials');
    });

    test('should login successfully with valid credentials', async ({ page }) => {
      await page.locator('[data-cy="login-username"]').fill('admin');
      await page.locator('[data-cy="login-password"]').fill('admin123');
      await page.locator('[data-cy="login-submit"]').click();
      
      await expect(page.locator('#login-overlay')).not.toBeVisible();
      await expect(page.locator('#main-container')).toBeVisible();
      await expect(page.locator('[data-cy="status-device-ip"]')).toContainText('192.168.0.213');
    });
  });

  test.describe('Dashboard Interaction', () => {
    test.beforeEach(async ({ page }) => {
      await page.locator('[data-cy="login-username"]').fill('admin');
      await page.locator('[data-cy="login-password"]').fill('admin123');
      await page.locator('[data-cy="login-submit"]').click();
    });

    test('should update DAC 1 (TF Voltage) slider and input', async ({ page }) => {
      const targetValue = '2.5';
      const slider = page.locator('[data-cy="dac-slider-ch1"]');
      
      // Playwright native slider handling
      await slider.fill(targetValue);
      
      await expect(page.locator('[data-cy="dac-value-display-ch1"]')).toContainText(`${targetValue} V`);
      await expect(page.locator('[data-cy="dac-input-ch1"]')).toHaveValue(targetValue);
      
      // Test manual input
      await page.locator('[data-cy="dac-input-ch1"]').fill('2.8');
      await page.locator('[data-cy="dac-input-ch1"]').press('Enter');
      
      await expect(page.locator('[data-cy="dac-value-display-ch1"]')).toContainText('2.8 V');
      await expect(slider).toHaveValue('2.8');
    });

    test('should toggle Relay 0 (DC1)', async ({ page }) => {
      const relayBtn = page.locator('[data-cy="relay-toggle-btn-0"]');
      
      await relayBtn.click();
      await expect(relayBtn).toHaveClass(/active/);
      
      await relayBtn.click();
      await expect(relayBtn).not.toHaveClass(/active/);
    });

    test('should reset all relays', async ({ page }) => {
      await page.locator('[data-cy="relay-toggle-btn-0"]').click();
      await page.locator('[data-cy="relay-toggle-btn-4"]').click();
      
      await page.locator('[data-cy="relay-reset-all-btn"]').click();
      
      await expect(page.locator('[data-cy="relay-toggle-btn-0"]')).not.toHaveClass(/active/);
      await expect(page.locator('[data-cy="relay-toggle-btn-4"]')).not.toHaveClass(/active/);
    });

    test('should verify status badges presence', async ({ page }) => {
      await expect(page.locator('[data-cy="status-ws-badge"]')).toBeVisible();
      await expect(page.locator('[data-cy="status-mqtt-badge"]')).toBeVisible();
    });
  });
});
