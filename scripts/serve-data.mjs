import fs from "node:fs/promises";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const projectRoot = path.resolve(__dirname, "..");
const dataRoot = path.join(projectRoot, "data");
const port = Number(process.env.PREVIEW_PORT || 4173);

const mimeTypes = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "application/javascript; charset=utf-8",
  ".png": "image/png",
  ".svg": "image/svg+xml"
};

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

server.listen(port, "127.0.0.1", () => {
  console.log(`SERVE_DATA_READY:${port}`);
});
