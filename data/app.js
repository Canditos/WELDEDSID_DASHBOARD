const RELAY_NAMES = [
    "DC1 FB", "DC2 FB", "DC3 FB", "DC4 FB",
    "DC1 24V", "DC2 24V", "MG1 FB", "MG2 FB"
];

let ws;
let wsToken = "";
let authHeader = "";
let authUser = "";
let relayStates = new Array(8).fill(null);
let dacValues = [null, null];
let firstLoadProcessed = false;
let wifiPollTimer = null;
let reconnectTimer = null;
let scanTimer = null;
let logFilter = "ALL";
const dacSendTimers = {};
const relayPendingTimers = {};
const dacPendingTimers = {};
let programRunningTimer = null;
let networkPanelOpen = false;

function getWifiModeLabel(mode) {
    switch (Number(mode)) {
        case 1:
            return "Connecting";
        case 2:
            return "Connected";
        case 3:
            return "AP Only";
        case 4:
            return "AP Fallback";
        default:
            return "Offline";
    }
}

function setVisible(id, visible, displayValue = "block") {
    document.getElementById(id).style.display = visible ? displayValue : "none";
}

function escapeHtml(value) {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#39;");
}

function setChipState(element, label, state) {
    if (!element) return;
    element.textContent = label;
    element.classList.remove("live", "warn", "offline");
    if (state) {
        element.classList.add(state);
    }
}

function updateModeChip(id, label, state) {
    setChipState(document.getElementById(id), label, state);
}

function updateOpsSnapshot() {
    const activeRelays = relayStates.filter((state) => state === true).length;
    const dac1 = dacValues[0] ?? "2.0";
    const dac2 = dacValues[1] ?? "4.0";

    document.getElementById("ops-relay-value").innerText = `${activeRelays} / 8`;
    document.getElementById("ops-relay-meta").innerText = activeRelays > 0 ? "Relays currently energized" : "All relay outputs idle";
    document.getElementById("ops-dac-value").innerText = `${dac1} V / ${dac2} V`;
}

function applyLogFilter() {
    const entries = document.querySelectorAll(".log-entry");
    entries.forEach((entry) => {
        const entryType = entry.dataset.type || "";
        const visible = logFilter === "ALL" || entryType === logFilter;
        entry.classList.toggle("hidden", !visible);
    });
}

function addLog(msg, type = "SYS") {
    const container = document.getElementById("log-container");
    if (!container) return;

    const now = new Date();
    const time = now.toLocaleTimeString("en-GB");
    const normalizedType = String(type).toUpperCase();
    const entry = document.createElement("div");
    entry.className = "log-entry";
    entry.dataset.type = normalizedType;
    entry.innerHTML = `<span class="log-time">${time}</span><span class="log-type">${escapeHtml(normalizedType)}</span><span class="log-msg">${escapeHtml(msg)}</span>`;
    container.prepend(entry);
    while (container.children.length > 200) {
        container.removeChild(container.lastChild);
    }
    applyLogFilter();
}

async function fetchJSON(path, options = {}) {
    const headers = { ...(options.headers || {}) };
    if (authHeader) {
        headers.Authorization = authHeader;
    }
    if (options.body && !headers["Content-Type"]) {
        headers["Content-Type"] = "application/json";
    }

    const response = await fetch(path, {
        ...options,
        headers
    });

    if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
    }

    return response.json();
}

function setRelayPending(index, pending) {
    const button = document.getElementById(`btn-r${index}`);
    if (!button) return;
    button.classList.toggle("pending", pending);
}

function setDacPending(channel, pending) {
    const chip = document.getElementById(`dac-chip-${channel}`);
    if (!chip) return;

    if (pending) {
        setChipState(chip, "UPDATING", "warn");
    } else {
        setChipState(chip, "READY", "live");
    }
}

function setProgramRunning(running) {
    const button = document.getElementById("execute-stepwise-btn");
    if (!button) return;
    button.classList.toggle("running", running);
    button.innerText = running ? "DISPENSER TEMP STEP RUNNING" : "DISPENSER TEMP STEP (4V-9V)";
    setChipState(document.getElementById("dac-chip-2"), running ? "STEP ACTIVE" : "READY", running ? "warn" : "live");
}

async function handleLogin(event) {
    event.preventDefault();

    const username = document.getElementById("login-username").value.trim();
    const password = document.getElementById("login-password").value;
    authHeader = `Basic ${btoa(`${username}:${password}`)}`;
    authUser = username;

    try {
        const data = await fetchJSON("/api/ws-auth");
        wsToken = data.token || "";
        if (!wsToken) {
            throw new Error("Missing token");
        }

        setVisible("login-overlay", false);
        setVisible("main-container", true, "block");
        setVisible("login-error", false);

        await Promise.all([updateWifiStatus(), updateSystemHealth(), loadSecurityStatus()]);
        initWS();

        if (wifiPollTimer) clearInterval(wifiPollTimer);
        wifiPollTimer = setInterval(() => {
            updateWifiStatus();
            updateSystemHealth();
        }, 15000);
        addLog(`User authorized: ${username}`, "AUTH");
    } catch (error) {
        setVisible("login-error", true);
        addLog("Authentication failed", "ERR");
    }
}

function logout() {
    if (ws) ws.close();
    authHeader = "";
    wsToken = "";
    authUser = "";
    relayStates = new Array(8).fill(null);
    dacValues = [null, null];
    firstLoadProcessed = false;
    setVisible("main-container", false);
    setVisible("offline-overlay", false);
    setVisible("login-overlay", true, "flex");
    document.getElementById("login-password").value = "";
    updateModeChip("ws-mode-chip", "WS: IDLE", "offline");
}

function initWS() {
    if (!wsToken) return;

    updateModeChip("ws-mode-chip", "WS: CONNECTING", "warn");
    ws = new WebSocket(`ws://${location.hostname}:81/`);
    ws.onopen = () => {
        addLog("Real-time connection open", "NET");
        updateModeChip("ws-mode-chip", "WS: LIVE", "live");
        document.getElementById("ops-command-value").innerText = "Linked";
        document.getElementById("ops-command-meta").innerText = "WebSocket session established";
        ws.send(JSON.stringify({ cmd: "auth", token: wsToken }));
    };
    ws.onclose = () => {
        addLog("Connection lost. Retrying...", "ERR");
        updateModeChip("ws-mode-chip", "WS: RETRYING", "warn");
        document.getElementById("ops-command-value").innerText = "Retrying";
        document.getElementById("ops-command-meta").innerText = "Command bus reconnect in progress";
        if (reconnectTimer) clearTimeout(reconnectTimer);
        reconnectTimer = setTimeout(initWS, 2000);
    };
    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        if (data.type === "auth" && !data.ok) {
            addLog("WebSocket authentication failed", "ERR");
            updateModeChip("ws-mode-chip", "WS: AUTH FAIL", "offline");
            ws.close();
            return;
        }
        if (data.type === "init" || data.type === "update") {
            updateUI(data);
        }
    };
}

function updateStatusBadge(id, isOnline) {
    const badge = document.getElementById(`badge-${id}`);
    if (!badge) return;
    badge.classList.remove("online", "offline");
    badge.classList.add(isOnline ? "online" : "offline");
}

function updateRelayButton(index, nextState) {
    const oldState = relayStates[index];
    relayStates[index] = !!nextState;
    const button = document.getElementById(`btn-r${index}`);
    if (!button) return;
    button.classList.toggle("active", !!nextState);
    setRelayPending(index, false);
    if (relayPendingTimers[index]) {
        clearTimeout(relayPendingTimers[index]);
        relayPendingTimers[index] = null;
    }
    if (firstLoadProcessed && oldState !== null && oldState !== nextState) {
        addLog(`${RELAY_NAMES[index]} is now ${nextState ? "ON" : "OFF"}`, "RELAY");
    }
    updateOpsSnapshot();
}

function updateDacVisual(channel, voltage) {
    if (Number.isNaN(voltage)) return;
    const value = voltage.toFixed(1);
    const index = channel - 1;
    const oldValue = dacValues[index];
    document.getElementById(`val-dac${channel}`).innerText = `${value} V`;
    document.getElementById(`range-dac${channel}`).value = value;
    document.getElementById(`num-dac${channel}`).value = value;
    dacValues[index] = value;
    setDacPending(channel, false);
    if (dacPendingTimers[channel]) {
        clearTimeout(dacPendingTimers[channel]);
        dacPendingTimers[channel] = null;
    }

    if (firstLoadProcessed && oldValue !== null && oldValue !== value) {
        addLog(`${channel === 1 ? "TF VOLTAGE" : "DISPENSER TEMP"} set to ${value}V`, "DAC");
    }
    updateOpsSnapshot();
}

function updateUI(data) {
    if (data.mqtt !== undefined) {
        updateStatusBadge("mqtt", !!data.mqtt);
        updateModeChip("mqtt-mode-chip", data.mqtt ? "MQTT: LIVE" : "MQTT: STANDBY", data.mqtt ? "live" : "warn");
    }
    if (data.modbus !== undefined) {
        updateStatusBadge("modbus", !!data.modbus);
        updateModeChip("modbus-mode-chip", data.modbus ? "MODBUS: READY" : "MODBUS: CHECK", data.modbus ? "live" : "warn");
    }

    if (Array.isArray(data.relays)) {
        data.relays.forEach((state, index) => updateRelayButton(index, state));
        firstLoadProcessed = true;
    } else if (data.idx !== undefined) {
        updateRelayButton(data.idx, data.state);
    }

    if (data.v1 !== undefined) updateDacVisual(1, parseFloat(data.v1));
    if (data.v2 !== undefined) updateDacVisual(2, parseFloat(data.v2));
}

function sendWsCommand(payload) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        addLog("WebSocket not connected", "ERR");
        updateModeChip("ws-mode-chip", "WS: OFFLINE", "offline");
        return;
    }

    document.getElementById("ops-command-value").innerText = "Dispatching";
    document.getElementById("ops-command-meta").innerText = `Last command: ${payload.cmd.toUpperCase()}`;
    ws.send(JSON.stringify(payload));
}

function toggleRelay(index) {
    setRelayPending(index, true);
    relayPendingTimers[index] = setTimeout(() => setRelayPending(index, false), 1600);
    sendWsCommand({ cmd: "relay", idx: index, state: !relayStates[index] });
}

function resetRelayGroup(indices) {
    addLog("Resetting relay group", "INFO");
    indices.forEach((idx) => {
        setRelayPending(idx, true);
        relayPendingTimers[idx] = setTimeout(() => setRelayPending(idx, false), 1600);
        sendWsCommand({ cmd: "relay", idx, state: false });
    });
}

function resetAllRelays() {
    addLog("Global Reset initiated", "WARN");
    for (let idx = 0; idx < RELAY_NAMES.length; idx += 1) {
        setRelayPending(idx, true);
        relayPendingTimers[idx] = setTimeout(() => setRelayPending(idx, false), 1600);
    }
    sendWsCommand({ cmd: "relay_mask", mask: 0 });
}

function syncDAC(channel, value, source) {
    let nextValue = parseFloat(value);
    const min = channel === 1 ? 2.0 : 4.0;
    const max = channel === 1 ? 3.0 : 9.0;
    if (Number.isNaN(nextValue)) return;

    nextValue = Math.min(Math.max(nextValue, min), max);
    updateDacVisual(channel, nextValue);
    setDacPending(channel, true);

    clearTimeout(dacSendTimers[channel]);
    clearTimeout(dacPendingTimers[channel]);
    dacPendingTimers[channel] = setTimeout(() => setDacPending(channel, false), 1800);
    const delay = source === "range" ? 120 : 0;
    dacSendTimers[channel] = setTimeout(() => {
        sendWsCommand({ cmd: "dac", channel, voltage: nextValue });
    }, delay);
}

function resetDAC(channel) {
    syncDAC(channel, channel === 1 ? 2.0 : 4.0, "reset");
}

function startAutoProgram(channel) {
    addLog("Executing Dispenser Temp STEP", "INFO");
    clearTimeout(programRunningTimer);
    setProgramRunning(true);
    document.getElementById("ops-command-value").innerText = "Program Active";
    document.getElementById("ops-command-meta").innerText = "Dispenser Temp step profile running";
    programRunningTimer = setTimeout(() => setProgramRunning(false), 5500);
    sendWsCommand({
        cmd: "step_ramp",
        channel,
        start: 4.0,
        target: 9.0,
        step: 0.5,
        duration: 5000
    });
}

async function updateWifiStatus() {
    try {
        const data = await fetchJSON("/api/wifi/status");
        const ipText = data.ip || "---.---.---.---";
        const modeLabel = getWifiModeLabel(data.mode);
        const statusText = data.status === 3 && data.ssid
            ? `Connected to ${data.ssid}`
            : `${modeLabel}${data.ssid ? ` (${data.ssid})` : ""}`;

        document.getElementById("display-ip").innerText = `Connected IP: ${ipText}`;
        document.getElementById("wifi-status").innerText = `Status: ${statusText}`;
        document.getElementById("ops-network-value").innerText = modeLabel;
        document.getElementById("ops-network-meta").innerText = data.ssid ? `${data.ssid} | ${ipText}` : `Controller IP ${ipText}`;
        updateStatusBadge("wifi", data.status === 3);

        if (data.status === 3 && document.getElementById("offline-overlay").style.display === "flex") {
            location.reload();
        }
    } catch (error) {
        updateStatusBadge("wifi", false);
        document.getElementById("wifi-status").innerText = "Status: Unable to load Wi-Fi status";
        document.getElementById("ops-network-value").innerText = "Unavailable";
        document.getElementById("ops-network-meta").innerText = "No network status response";
    }
}

async function updateSystemHealth() {
    try {
        const data = await fetchJSON("/api/health");
        updateStatusBadge("wifi", !!data.wifiConnected);
        updateStatusBadge("mqtt", !!data.mqttConnected);
        updateStatusBadge("modbus", !!data.modbusHealthy);

        updateModeChip("wifi-mode-chip", `WIFI MODE: ${getWifiModeLabel(data.wifiMode).toUpperCase()}`, data.wifiConnected ? "live" : "warn");
        updateModeChip("mqtt-mode-chip", data.mqttConnected ? "MQTT: LIVE" : "MQTT: STANDBY", data.mqttConnected ? "live" : "warn");
        updateModeChip("modbus-mode-chip", data.modbusHealthy ? "MODBUS: READY" : "MODBUS: CHECK", data.modbusHealthy ? "live" : "warn");
        if (!document.getElementById("ops-network-meta").innerText || document.getElementById("ops-network-meta").innerText === "No network status response") {
            document.getElementById("ops-network-meta").innerText = `Controller IP ${data.wifiIp || "---.---.---.---"}`;
        }
    } catch (error) {
        updateStatusBadge("wifi", false);
        updateStatusBadge("mqtt", false);
        updateStatusBadge("modbus", false);
        updateModeChip("wifi-mode-chip", "WIFI MODE: UNKNOWN", "offline");
        updateModeChip("mqtt-mode-chip", "MQTT: UNKNOWN", "offline");
        updateModeChip("modbus-mode-chip", "MODBUS: UNKNOWN", "offline");
    }
}

function renderWifiList(networks) {
    const container = document.getElementById("wifi-list");
    if (!Array.isArray(networks) || networks.length === 0) {
        container.innerHTML = '<div class="empty-state">No networks found</div>';
        return;
    }

    container.innerHTML = networks.map((network) => {
        const ssid = escapeHtml(network.ssid || "(hidden)");
        const secure = network.secure ? "Lock" : "Open";
        return `
            <button type="button" class="wifi-item" data-ssid="${ssid}">
                <span class="wifi-name">${ssid}</span>
                <span class="wifi-meta">${network.rssi} dBm | ${secure}</span>
            </button>
        `;
    }).join("");

    container.querySelectorAll(".wifi-item").forEach((element) => {
        element.addEventListener("click", () => {
            document.getElementById("wifi-ssid").value = element.dataset.ssid;
            addLog(`Selected network: ${element.dataset.ssid}`, "INFO");
        });
    });
}

async function startScan() {
    document.getElementById("wifi-list").innerHTML = '<div class="empty-state">Scanning...</div>';
    addLog("Scanning for Wi-Fi networks...", "INFO");
    await fetchJSON("/api/wifi/startScan", { method: "POST" });

    if (scanTimer) clearInterval(scanTimer);
    let attempts = 0;
    scanTimer = setInterval(async () => {
        attempts += 1;
        try {
            const networks = await fetchJSON("/api/wifi/scan");
            if (Array.isArray(networks) && networks.length > 0) {
                clearInterval(scanTimer);
                renderWifiList(networks);
            } else if (attempts > 10) {
                clearInterval(scanTimer);
                renderWifiList([]);
            }
        } catch (error) {
            if (attempts > 10) {
                clearInterval(scanTimer);
                renderWifiList([]);
            }
        }
    }, 2000);
}

async function connectWifi() {
    const ssid = document.getElementById("wifi-ssid").value.trim();
    const pass = document.getElementById("wifi-pass").value;
    if (!ssid) {
        alert("Select a network first");
        return;
    }

    setVisible("offline-overlay", true, "flex");
    addLog(`Saving credentials for ${ssid}`, "INFO");

    try {
        await fetchJSON("/api/wifi/save", {
            method: "POST",
            body: JSON.stringify({ ssid, pass })
        });
    } catch (error) {
        addLog("Wi-Fi save request interrupted, waiting for device", "WARN");
    }

    setInterval(() => {
        updateWifiStatus();
        updateSystemHealth();
    }, 5000);
}

async function loadSecurityStatus() {
    try {
        const data = await fetchJSON("/api/security/status");
        document.getElementById("security-admin-user").value = data.adminUser || authUser;
        document.getElementById("security-ota-pass").placeholder = data.hasOtaPassword ? "OTA password configured" : "OTA password";
        document.getElementById("security-status-text").innerText =
            data.usingDefaultPassword ? "Default admin password still in use." : "Custom credentials active.";
    } catch (error) {
        document.getElementById("security-status-text").innerText = "Unable to load security settings.";
    }
}

async function saveSecuritySettings() {
    const adminUser = document.getElementById("security-admin-user").value.trim();
    const adminPass = document.getElementById("security-admin-pass").value;
    const otaPassword = document.getElementById("security-ota-pass").value;

    if (!adminUser || adminPass.length < 8) {
        addLog("Admin password must have at least 8 characters", "ERR");
        return;
    }

    try {
        await fetchJSON("/api/security/passwords", {
            method: "POST",
            body: JSON.stringify({ adminUser, adminPass, otaPassword })
        });

        authHeader = `Basic ${btoa(`${adminUser}:${adminPass}`)}`;
        authUser = adminUser;
        document.getElementById("security-admin-pass").value = "";

        const data = await fetchJSON("/api/ws-auth");
        wsToken = data.token || "";
        if (ws) ws.close();
        initWS();
        await loadSecurityStatus();
        await updateSystemHealth();
        addLog("Security settings updated", "AUTH");
    } catch (error) {
        addLog("Failed to update security settings", "ERR");
    }
}

function clearLogs() {
    document.getElementById("log-container").innerHTML = "";
    addLog("Logs cleared", "INFO");
}

function updateNetworkPanel() {
    const body = document.getElementById("network-panel-body");
    const arrow = document.getElementById("network-panel-arrow");
    body.classList.toggle("open", networkPanelOpen);
    arrow.textContent = networkPanelOpen ? "▲" : "▼";
}

function toggleNetworkPanel() {
    networkPanelOpen = !networkPanelOpen;
    updateNetworkPanel();
}

function downloadLogs() {
    const container = document.getElementById("log-container");
    const lines = Array.from(container.querySelectorAll(".log-entry:not(.hidden)")).reverse().map((entry) => {
        return Array.from(entry.children).map((node) => node.textContent).join(" | ");
    });

    const blob = new Blob([lines.join("\n")], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = `sicharge_logs_${new Date().toISOString().replaceAll(":", "-")}.txt`;
    anchor.click();
    URL.revokeObjectURL(url);
}

function initializeLogFilters() {
    document.querySelectorAll(".log-filter-btn").forEach((button) => {
        button.addEventListener("click", () => {
            logFilter = button.dataset.filter || "ALL";
            document.querySelectorAll(".log-filter-btn").forEach((item) => item.classList.toggle("active", item === button));
            applyLogFilter();
        });
    });
}

function initializeDacPresets() {
    document.querySelectorAll("[data-dac-preset]").forEach((button) => {
        button.addEventListener("click", () => {
            const channel = Number(button.dataset.dacPreset);
            const value = Number(button.dataset.value);
            syncDAC(channel, value, "preset");
        });
    });
}

document.getElementById("login-form").addEventListener("submit", handleLogin);
document.getElementById("logout-btn").addEventListener("click", logout);
document.getElementById("scan-wifi-btn").addEventListener("click", startScan);
document.getElementById("save-connect-btn").addEventListener("click", connectWifi);
document.getElementById("save-security-btn").addEventListener("click", saveSecuritySettings);
document.getElementById("clear-logs-btn").addEventListener("click", clearLogs);
document.getElementById("download-logs-btn").addEventListener("click", downloadLogs);
document.getElementById("network-panel-toggle").addEventListener("click", toggleNetworkPanel);
document.getElementById("reset-all-relays-btn").addEventListener("click", resetAllRelays);
document.getElementById("reset-group-1-btn").addEventListener("click", () => resetRelayGroup([0, 1, 2, 3]));
document.getElementById("reset-group-2-btn").addEventListener("click", () => resetRelayGroup([4, 5]));
document.getElementById("reset-group-3-btn").addEventListener("click", () => resetRelayGroup([6, 7]));
document.getElementById("reset-dac-1-btn").addEventListener("click", () => resetDAC(1));
document.getElementById("reset-dac-2-btn").addEventListener("click", () => resetDAC(2));
document.getElementById("execute-stepwise-btn").addEventListener("click", () => startAutoProgram(2));

for (let i = 0; i < RELAY_NAMES.length; i += 1) {
    document.getElementById(`btn-r${i}`).addEventListener("click", () => toggleRelay(i));
}

document.getElementById("range-dac1").addEventListener("input", (event) => syncDAC(1, event.target.value, "range"));
document.getElementById("range-dac2").addEventListener("input", (event) => syncDAC(2, event.target.value, "range"));
document.getElementById("num-dac1").addEventListener("change", (event) => syncDAC(1, event.target.value, "num"));
document.getElementById("num-dac2").addEventListener("change", (event) => syncDAC(2, event.target.value, "num"));

updateNetworkPanel();
initializeLogFilters();
initializeDacPresets();
updateModeChip("wifi-mode-chip", "WIFI MODE: OFFLINE", "offline");
updateModeChip("mqtt-mode-chip", "MQTT: WAITING", "warn");
updateModeChip("modbus-mode-chip", "MODBUS: READY", "live");
updateModeChip("ws-mode-chip", "WS: IDLE", "offline");
setDacPending(1, false);
setDacPending(2, false);
setProgramRunning(false);
updateOpsSnapshot();
