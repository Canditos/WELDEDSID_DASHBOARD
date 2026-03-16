import fs from "node:fs/promises";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const projectRoot = path.resolve(__dirname, "..");
const dataRoot = path.join(projectRoot, "data");
const outputPath = path.join(projectRoot, "preview-dashboard.png");

const mimeTypes = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "application/javascript; charset=utf-8",
  ".png": "image/png"
};

function startServer() {
  const server = http.createServer(async (req, res) => {
    try {
      let reqPath = (req.url || "/").split("?")[0];
      if (reqPath === "/") reqPath = "/index.html";
      const filePath = path.join(dataRoot, reqPath);
      if (!filePath.startsWith(dataRoot)) {
        res.writeHead(403);
        res.end("Forbidden");
        return;
      }

      const content = await fs.readFile(filePath);
      res.setHeader("Content-Type", mimeTypes[path.extname(filePath)] || "application/octet-stream");
      res.end(content);
    } catch {
      res.writeHead(404);
      res.end("Not found");
    }
  });

  return new Promise((resolve) => {
    server.listen(4173, "127.0.0.1", () => resolve(server));
  });
}

const initialState = {
  type: "init",
  relays: [true, false, true, false, false, true, false, true],
  v1: 2.4,
  v2: 6.5,
  mqtt: true,
  modbus: true,
  wifiMode: 2
};

async function main() {
  const server = await startServer();
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 1300 } });

  await page.route("**/api/ws-auth", async (route) => {
    await route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({ token: "preview-token" })
    });
  });

  await page.route("**/api/wifi/status", async (route) => {
    await route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({
        ssid: "Preview_Network",
        status: 3,
        ip: "192.168.0.222",
        rssi: -48,
        mode: 2
      })
    });
  });

  await page.route("**/api/health", async (route) => {
    await route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({
        wifiConnected: true,
        wifiMode: 2,
        wifiIp: "192.168.0.222",
        mqttConnected: true,
        modbusHealthy: true,
        heap: 182344
      })
    });
  });

  await page.route("**/api/security/status", async (route) => {
    await route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({
        adminUser: "admin",
        hasOtaPassword: true,
        usingDefaultPassword: false
      })
    });
  });

  await page.route("**/api/wifi/startScan", async (route) => {
    await route.fulfill({ status: 200, contentType: "application/json", body: "{\"status\":\"scanning\"}" });
  });

  await page.route("**/api/wifi/scan", async (route) => {
    await route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify([
        { ssid: "Preview_Network", rssi: -48, secure: true },
        { ssid: "Workshop_AP", rssi: -61, secure: false }
      ])
    });
  });

  await page.route("**/api/security/passwords", async (route) => {
    await route.fulfill({ status: 200, contentType: "application/json", body: "{\"status\":\"ok\"}" });
  });

  await page.route("**/api/wifi/save", async (route) => {
    await route.fulfill({ status: 200, contentType: "application/json", body: "{\"status\":\"ok\"}" });
  });

  await page.addInitScript((state) => {
    class MockWebSocket {
      static OPEN = 1;
      readyState = 1;

      constructor() {
        setTimeout(() => this.onopen && this.onopen(), 30);
      }

      send(payload) {
        const data = JSON.parse(payload);
        if (data.cmd === "auth") {
          setTimeout(() => {
            this.onmessage && this.onmessage({ data: JSON.stringify({ type: "auth", ok: true }) });
            this.onmessage && this.onmessage({ data: JSON.stringify(state) });
          }, 60);
        }
      }

      close() {
        this.readyState = 3;
        this.onclose && this.onclose();
      }
    }

    window.WebSocket = MockWebSocket;
  }, initialState);

  await page.goto("http://127.0.0.1:4173/");
  await page.fill("#login-username", "admin");
  await page.fill("#login-password", "preview-password");
  await page.click("button:has-text('Authorize')");
  await page.waitForSelector("[data-cy='relay-control-center-title']");
  await page.waitForTimeout(400);
  await page.screenshot({ path: outputPath, fullPage: true });

  await browser.close();
  await new Promise((resolve) => server.close(resolve));
  console.log(outputPath);
}

main().catch(async (error) => {
  console.error(error);
  process.exitCode = 1;
});
