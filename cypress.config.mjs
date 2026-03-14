import { defineConfig } from "cypress";

export default defineConfig({
  e2e: {
    baseUrl: process.env.CYPRESS_BASE_URL || "http://127.0.0.1:4173",
    specPattern: "cypress/e2e/**/*.cy.{js,mjs}",
    supportFile: "cypress/support/e2e.js",
    video: false,
    screenshotOnRunFailure: true,
    viewportWidth: 1440,
    viewportHeight: 1100
  }
});
