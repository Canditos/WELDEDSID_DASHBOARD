import { test, expect } from '@playwright/test';

test.describe('ESP32 SICHARGE D Monitor Dashboard', () => {

  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.fill('#login-username', 'admin');
    await page.fill('#login-password', 'admin123');
    await page.click('button:has-text("Authorize")');
    
    // Ensure dashboard is visible
    await expect(page.locator('[data-cy="relay-control-center-title"]')).toBeVisible();
  });

  test('should toggle relay and log the event', async ({ page }) => {
    const relayBtn = page.locator('[data-cy="relay-btn-0"]'); // DC1 FB
    const logContainer = page.locator('[data-cy="log-container"]');

    // Toggle ON
    await relayBtn.click();
    await expect(relayBtn).toHaveClass(/active/);
    await expect(logContainer).toContainText('DC1 FB is now ON');

    // Toggle OFF
    await relayBtn.click();
    await expect(relayBtn).not.toHaveClass(/active/);
    await expect(logContainer).toContainText('DC1 FB is now OFF');
  });

  test('should change DAC voltage and log correctly', async ({ page }) => {
    const dacSlider = page.locator('[data-cy="dac-range-2"]'); // DISPENSER TEMP (DAC 2)
    const logContainer = page.locator('[data-cy="log-container"]');

    // Set slider to 6.5V
    await dacSlider.fill('6.5');
    await expect(page.locator('[data-cy="dac-val-2"]')).toHaveText('6.5 V');
    await expect(logContainer).toContainText('DISPENSER TEMP set to 6.5V');
  });

  test('should perform global relay reset', async ({ page }) => {
    await page.locator('[data-cy="relay-btn-0"]').click();
    await page.locator('[data-cy="reset-all-relays-btn"]').click();
    await expect(page.locator('[data-cy="relay-btn-0"]')).not.toHaveClass(/active/);
    
    const logContainer = page.locator('[data-cy="log-container"]');
    await expect(logContainer).toContainText('Global Reset initiated');
  });

});
