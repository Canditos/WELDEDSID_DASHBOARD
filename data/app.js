const RELAY_NAMES = [
    "DC1 FB", "DC2 FB", "DC3 FB", "DC4 FB",
    "DC1 24V", "DC2 24V", "MG1 FB", "MG2 FB"
];

let ws;
let wsToken = "";
let authHeader = "";
let authUser = "";
let currentUserRole = "viewer";
let relayStates = new Array(8).fill(null);
let dacValues = [null, null];
let firstLoadProcessed = false;
let wifiPollTimer = null;
let reconnectTimer = null;
let scanTimer = null;
let logFilter = "ALL";
let currentLogPage = 1;
const LOGS_PER_PAGE = 16;
const logEntries = [];
const dacSendTimers = {};
const relayPendingTimers = {};
const dacPendingTimers = {};
let programRunningTimer = null;
let networkPanelOpen = false;
let commandStateTimer = null;
let lastReportedDeviceIp = "";

const ROLE_LEVELS = {
    viewer: 0,
    operator: 1,
    admin: 2
};

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

function hasRole(role) {
    return (ROLE_LEVELS[currentUserRole] ?? 0) >= (ROLE_LEVELS[role] ?? 0);
}

function enforceRole(role, deniedMessage) {
    if (hasRole(role)) {
        return true;
    }

    addLog(deniedMessage, "AUTH");
    showToast("Access denied", deniedMessage, "warn");
    return false;
}

function setOpsCardState(id, state) {
    const card = document.getElementById(id);
    if (!card) return;
    card.classList.remove("live", "warn", "offline", "sync");
    if (state) {
        card.classList.add(state);
    }
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

function showToast(title, message = "", tone = "info", duration = 2600) {
    const stack = document.getElementById("toast-stack");
    if (!stack) return;

    const toast = document.createElement("div");
    toast.className = `toast ${tone}`;
    toast.innerHTML = `
        <span class="toast-title">${escapeHtml(title)}</span>
        <span class="toast-body">${escapeHtml(message)}</span>
    `;
    stack.appendChild(toast);

    window.setTimeout(() => {
        toast.remove();
    }, duration);
}

function setCommandState(value, meta, state = "", autoResetMs = 0) {
    document.getElementById("ops-command-value").innerText = value;
    document.getElementById("ops-command-meta").innerText = meta;
    setOpsCardState("ops-command-card", state);

    if (commandStateTimer) {
        clearTimeout(commandStateTimer);
        commandStateTimer = null;
    }

    if (autoResetMs > 0) {
        commandStateTimer = setTimeout(() => {
            if (ws && ws.readyState === WebSocket.OPEN) {
                setCommandState("Linked", "WebSocket session established", "live");
            }
        }, autoResetMs);
    }
}

function updateAccessLocation(deviceIp) {
    const accessLabel = document.getElementById("display-ip");
    const accessLink = document.getElementById("device-open-link");
    if (!accessLabel || !accessLink) return;

    const viewHost = window.location.host || window.location.hostname || "offline";
    const viewHostName = window.location.hostname || "";
    const normalizedDeviceIp = deviceIp || "---.---.---.---";
    const isPreview = viewHostName === "127.0.0.1" || viewHostName === "localhost";
    const sameHost = deviceIp && viewHostName === deviceIp;

    if (isPreview) {
        accessLabel.innerText = `Preview: ${viewHost} | Device: ${normalizedDeviceIp}`;
    } else if (sameHost) {
        accessLabel.innerText = `Device IP: ${normalizedDeviceIp}`;
    } else {
        accessLabel.innerText = `View: ${viewHost} | Device: ${normalizedDeviceIp}`;
    }

    accessLabel.classList.toggle("warn", !!deviceIp && !sameHost && !isPreview);

    if (deviceIp && (!sameHost || isPreview)) {
        accessLink.hidden = false;
        accessLink.href = `${window.location.protocol}//${deviceIp}/`;
        accessLink.innerText = isPreview ? "OPEN DEVICE" : "SYNC TO DEVICE";
    } else {
        accessLink.hidden = true;
        accessLink.removeAttribute("href");
    }

    if (deviceIp && lastReportedDeviceIp && lastReportedDeviceIp !== deviceIp) {
        showToast("Device IP updated", `Controller now reports ${deviceIp}`, "info", 3400);
    }

    if (deviceIp) {
        lastReportedDeviceIp = deviceIp;
    }
}

function updateOpsSnapshot() {
    const activeRelays = relayStates.filter((state) => state === true).length;
    const dac1 = dacValues[0] ?? "2.0";
    const dac2 = dacValues[1] ?? "4.0";

    document.getElementById("ops-relay-value").innerText = `${activeRelays} / 8`;
    document.getElementById("ops-relay-meta").innerText = activeRelays > 0 ? "Relays currently energized" : "All relay outputs idle";
    document.getElementById("ops-dac-value").innerText = `${dac1} V / ${dac2} V`;
    setOpsCardState("ops-relay-card", activeRelays > 0 ? "live" : "");
    setOpsCardState("ops-dac-card", "live");
}

function setElementDisabled(id, disabled) {
    const element = document.getElementById(id);
    if (!element) return;
    element.disabled = disabled;
    element.classList.toggle("disabled", disabled);
}

function applyRolePermissions() {
    const operatorLocked = !hasRole("operator");
    const adminLocked = !hasRole("admin");

    [
        "reset-all-relays-btn",
        "reset-group-1-btn",
        "reset-group-2-btn",
        "reset-group-3-btn",
        "reset-dac-1-btn",
        "reset-dac-2-btn",
        "execute-stepwise-btn",
        "range-dac1",
        "range-dac2",
        "num-dac1",
        "num-dac2"
    ].forEach((id) => setElementDisabled(id, operatorLocked));

    for (let i = 0; i < RELAY_NAMES.length; i += 1) {
        setElementDisabled(`btn-r${i}`, operatorLocked);
    }

    [
        "scan-wifi-btn",
        "save-connect-btn",
        "save-security-btn",
        "export-config-btn",
        "import-config-btn",
        "reset-network-config-btn",
        "reset-all-config-btn",
        "security-admin-user",
        "security-admin-pass",
        "security-operator-user",
        "security-operator-pass",
        "security-viewer-user",
        "security-viewer-pass",
        "security-operator-enabled",
        "security-viewer-enabled",
        "security-ota-pass",
        "wifi-ssid",
        "wifi-pass"
    ].forEach((id) => setElementDisabled(id, adminLocked));

    document.getElementById("security-status-text").innerText = hasRole("admin")
        ? "Admin access active. User and config management unlocked."
        : hasRole("operator")
            ? "Operator access active. Control commands unlocked, configuration locked."
            : "Viewer access active. Dashboard is in read-only mode.";
}

function applyLogFilter() {
    renderLogPage();
}

function getFilteredLogEntries() {
    return logEntries.filter((entry) => logFilter === "ALL" || entry.type === logFilter);
}

function renderLogPage() {
    const container = document.getElementById("log-container");
    const pageStatus = document.getElementById("log-page-status");
    const prevButton = document.getElementById("log-prev-btn");
    const nextButton = document.getElementById("log-next-btn");
    if (!container || !pageStatus || !prevButton || !nextButton) return;

    const filteredEntries = getFilteredLogEntries();
    const totalPages = Math.max(1, Math.ceil(filteredEntries.length / LOGS_PER_PAGE));
    currentLogPage = Math.min(currentLogPage, totalPages);
    currentLogPage = Math.max(1, currentLogPage);

    const startIndex = (currentLogPage - 1) * LOGS_PER_PAGE;
    const visibleEntries = filteredEntries.slice(startIndex, startIndex + LOGS_PER_PAGE);

    container.innerHTML = visibleEntries.map((entry) => `
        <div class="log-entry type-${entry.type}" data-type="${entry.type}">
            <span class="log-time">${escapeHtml(entry.time)}</span>
            <span class="log-type">${escapeHtml(entry.type)}</span>
            <span class="log-msg">${escapeHtml(entry.message)}</span>
        </div>
    `).join("");

    if (visibleEntries.length === 0) {
        container.innerHTML = '<div class="empty-state">No log entries for this filter</div>';
    }

    pageStatus.textContent = `PAGE ${currentLogPage} / ${totalPages}`;
    prevButton.disabled = currentLogPage === 1;
    nextButton.disabled = currentLogPage === totalPages;
}

function addLog(msg, type = "SYS") {
    const now = new Date();
    const time = now.toLocaleTimeString("en-GB");
    const normalizedType = String(type).toUpperCase();
    logEntries.unshift({ time, type: normalizedType, message: msg });
    while (logEntries.length > 200) {
        logEntries.pop();
    }
    currentLogPage = 1;
    renderLogPage();
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
    button.dataset.state = pending ? "pending" : (relayStates[index] ? "active" : "idle");
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
        currentUserRole = data.role || "viewer";
        authUser = data.username || username;
        if (!wsToken) {
            throw new Error("Missing token");
        }

        document.body.style.overflow = "hidden";
        updateAccessLocation("");
        setVisible("login-overlay", false);
        setVisible("main-container", true, "flex");
        setVisible("login-error", false);
        applyRolePermissions();

        await Promise.all([
            updateWifiStatus(),
            updateSystemHealth(),
            hasRole("admin") ? loadSecurityStatus() : Promise.resolve()
        ]);
        initWS();

        if (wifiPollTimer) clearInterval(wifiPollTimer);
        wifiPollTimer = setInterval(() => {
            updateWifiStatus();
            updateSystemHealth();
        }, 15000);
        addLog(`User authorized: ${username}`, "AUTH");
        showToast("Session opened", `Signed in as ${currentUserRole}`, "success");
    } catch (error) {
        document.getElementById("login-error").innerText = "Invalid credentials or inactive account";
        setVisible("login-error", true);
        currentUserRole = "viewer";
        addLog("Authentication failed", "ERR");
        showToast("Login failed", "Check username, password, or active role.", "error");
    }
}

function logout() {
    if (ws) ws.close();
    authHeader = "";
    wsToken = "";
    authUser = "";
    currentUserRole = "viewer";
    relayStates = new Array(8).fill(null);
    dacValues = [null, null];
    firstLoadProcessed = false;
    document.body.style.overflow = "";
    setOpsCardState("ops-network-card", "");
    setOpsCardState("ops-command-card", "");
    setOpsCardState("ops-relay-card", "");
    setOpsCardState("ops-dac-card", "");
    updateAccessLocation("");
    setVisible("main-container", false);
    setVisible("offline-overlay", false);
    setVisible("login-overlay", true, "flex");
    document.getElementById("login-password").value = "";
    updateModeChip("ws-mode-chip", "WS: IDLE", "offline");
    applyRolePermissions();
}

function initWS() {
    if (!wsToken) return;

    updateModeChip("ws-mode-chip", "WS: CONNECTING", "warn");
    setCommandState("Connecting", "Opening WebSocket session", "warn");
    ws = new WebSocket(`ws://${location.hostname}:81/`);
    ws.onopen = () => {
        addLog("Real-time connection open", "NET");
        updateModeChip("ws-mode-chip", "WS: LIVE", "live");
        setCommandState("Linked", "WebSocket session established", "live");
        ws.send(JSON.stringify({ cmd: "auth", token: wsToken }));
    };
    ws.onclose = () => {
        addLog("Connection lost. Retrying...", "ERR");
        updateModeChip("ws-mode-chip", "WS: RETRYING", "warn");
        setCommandState("Retrying", "Command bus reconnect in progress", "offline");
        if (reconnectTimer) clearTimeout(reconnectTimer);
        reconnectTimer = setTimeout(initWS, 2000);
    };
    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        if (data.type === "auth" && !data.ok) {
            addLog("WebSocket authentication failed", "ERR");
            updateModeChip("ws-mode-chip", "WS: AUTH FAIL", "offline");
            setCommandState("Auth fail", "Realtime control session rejected", "offline");
            showToast("Realtime session failed", "WebSocket authentication was rejected.", "error");
            ws.close();
            return;
        }
        if (data.type === "auth" && data.ok) {
            currentUserRole = data.role || currentUserRole;
            authUser = data.username || authUser;
            applyRolePermissions();
            addLog(`Realtime session active for ${authUser} (${currentUserRole})`, "AUTH");
            setCommandState("Ready", `Controls armed for ${authUser}`, "live", 1500);
            return;
        }
        if (data.type === "init" || data.type === "update") {
            updateUI(data);
        }
        if (data.type === "error" && data.message === "forbidden") {
            addLog("Current role cannot execute that command", "AUTH");
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
        setCommandState("Synchronized", `${RELAY_NAMES[index]} is now ${nextState ? "ON" : "OFF"}`, "live", 1400);
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
        setCommandState("Synchronized", `${channel === 1 ? "TF VOLTAGE" : "DISPENSER TEMP"} set to ${value}V`, "live", 1400);
    }
    updateOpsSnapshot();
}

function updateUI(data) {
    if (data.role) {
        currentUserRole = data.role;
        applyRolePermissions();
    }
    if (data.username) {
        authUser = data.username;
    }
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
    if (!enforceRole("operator", "This account does not have control permissions")) {
        return;
    }

    if (!ws || ws.readyState !== WebSocket.OPEN) {
        addLog("WebSocket not connected", "ERR");
        updateModeChip("ws-mode-chip", "WS: OFFLINE", "offline");
        setCommandState("Offline", "WebSocket transport unavailable", "offline");
        showToast("Command not sent", "Realtime connection is not available.", "error");
        return;
    }

    setCommandState("Dispatching", `Last command: ${payload.cmd.toUpperCase()}`, "sync");
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
    showToast("Reset all relays", "Turning all relay outputs OFF.", "warn");
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
    setCommandState("Program Active", "Dispenser Temp step profile running", "warn");
    showToast("Program started", "Dispenser Temp STEP is now running.", "info");
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

        updateAccessLocation(ipText);
        document.getElementById("wifi-status").innerText = `Status: ${statusText}`;
        document.getElementById("ops-network-value").innerText = modeLabel;
        document.getElementById("ops-network-meta").innerText = data.ssid ? `${data.ssid} | ${ipText}` : `Controller IP ${ipText}`;
        setOpsCardState("ops-network-card", data.status === 3 ? "live" : "warn");
        updateStatusBadge("wifi", data.status === 3);

        if (data.status === 3 && document.getElementById("offline-overlay").style.display === "flex") {
            location.reload();
        }
    } catch (error) {
        updateStatusBadge("wifi", false);
        document.getElementById("wifi-status").innerText = "Status: Unable to load Wi-Fi status";
        document.getElementById("ops-network-value").innerText = "Unavailable";
        document.getElementById("ops-network-meta").innerText = "No network status response";
        setOpsCardState("ops-network-card", "offline");
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
        updateAccessLocation(data.wifiIp || "");
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
        setOpsCardState("ops-network-card", "offline");
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
    if (!enforceRole("admin", "Only admin users can scan or change network settings")) return;
    document.getElementById("wifi-list").innerHTML = '<div class="empty-state">Scanning...</div>';
    addLog("Scanning for Wi-Fi networks...", "INFO");
    showToast("Wi-Fi scan", "Searching for nearby networks.", "info");
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
    if (!enforceRole("admin", "Only admin users can change Wi-Fi settings")) return;
    const ssid = document.getElementById("wifi-ssid").value.trim();
    const pass = document.getElementById("wifi-pass").value;
    if (!ssid) {
        showToast("Select a network", "Choose or type an SSID before saving.", "warn");
        return;
    }

    setVisible("offline-overlay", true, "flex");
    addLog(`Saving credentials for ${ssid}`, "INFO");
    showToast("Applying Wi-Fi", `Saving credentials for ${ssid}.`, "info", 3200);

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
        const users = Array.isArray(data.users) ? data.users : [];
        const admin = users.find((user) => user.role === "admin") || {};
        const operator = users.find((user) => user.role === "operator") || {};
        const viewer = users.find((user) => user.role === "viewer") || {};

        document.getElementById("security-admin-user").value = admin.username || authUser;
        document.getElementById("security-operator-user").value = operator.username || "";
        document.getElementById("security-viewer-user").value = viewer.username || "";
        document.getElementById("security-admin-pass").value = "";
        document.getElementById("security-operator-pass").value = "";
        document.getElementById("security-viewer-pass").value = "";
        document.getElementById("security-admin-pass").placeholder = "Leave blank to keep current admin password";
        document.getElementById("security-operator-pass").placeholder = "Leave blank to keep current operator password";
        document.getElementById("security-viewer-pass").placeholder = "Leave blank to keep current viewer password";
        document.getElementById("security-operator-enabled").checked = operator.enabled !== false;
        document.getElementById("security-viewer-enabled").checked = viewer.enabled !== false;
        document.getElementById("security-ota-pass").placeholder = data.hasOtaPassword ? "OTA password configured" : "OTA password";

        const usingDefaults = users.filter((user) => user.usingDefault).map((user) => user.role.toUpperCase());
        if (usingDefaults.length > 0) {
            document.getElementById("security-status-text").innerText = `Default credentials still active for: ${usingDefaults.join(", ")}.`;
        } else {
            document.getElementById("security-status-text").innerText = "Custom credentials active for all enabled roles.";
        }
    } catch (error) {
        document.getElementById("security-status-text").innerText = "Unable to load security settings.";
    }
}

async function exportConfig() {
    if (!enforceRole("admin", "Only admin users can export configuration")) return;
    try {
        const data = await fetchJSON("/api/config/export");
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
        const url = URL.createObjectURL(blob);
        const anchor = document.createElement("a");
        anchor.href = url;
        anchor.download = `sicharge_backup_${new Date().toISOString().replaceAll(":", "-")}.json`;
        anchor.click();
        URL.revokeObjectURL(url);
        addLog("Configuration backup exported", "AUTH");
        showToast("Backup exported", "Configuration backup downloaded.", "success");
    } catch (error) {
        addLog("Failed to export configuration", "ERR");
        showToast("Export failed", "Could not export the controller configuration.", "error");
    }
}

function triggerImportConfig() {
    if (!enforceRole("admin", "Only admin users can import configuration")) return;
    document.getElementById("import-config-file").click();
}

async function handleImportConfig(event) {
    const [file] = event.target.files || [];
    if (!file) return;

    try {
        const text = await file.text();
        await fetchJSON("/api/config/import", {
            method: "POST",
            body: text
        });
        addLog("Configuration import applied", "AUTH");
        showToast("Configuration imported", "The backup file was applied successfully.", "success");
        await Promise.all([loadSecurityStatus(), updateWifiStatus(), updateSystemHealth()]);
    } catch (error) {
        addLog("Failed to import configuration", "ERR");
        showToast("Import failed", "The selected backup could not be applied.", "error");
    } finally {
        event.target.value = "";
    }
}

async function resetConfiguration(scope) {
    if (!enforceRole("admin", "Only admin users can reset configuration")) return;

    try {
        const result = await fetchJSON("/api/config/reset", {
            method: "POST",
            body: JSON.stringify({ scope })
        });

        addLog(`Configuration reset executed for scope: ${scope}`, "AUTH");
        if (result.restartRequired) {
            showToast("Controller restarting", "Reset applied. The controller will reboot and may return in AP mode.", "warn", 5000);
            addLog("Controller restart scheduled to apply reset", "WARN");
            logout();
            return;
        }

        showToast("Configuration reset", `Reset completed for ${scope}.`, "warn");
        if (scope === "all") {
            addLog("Security was reset to defaults. Please sign in again.", "WARN");
            logout();
            return;
        }
        await Promise.all([loadSecurityStatus(), updateWifiStatus(), updateSystemHealth()]);
    } catch (error) {
        addLog(`Failed to reset configuration scope: ${scope}`, "ERR");
        showToast("Reset failed", `Could not reset scope ${scope}.`, "error");
    }
}

async function saveSecuritySettings() {
    if (!enforceRole("admin", "Only admin users can update users and configuration")) return;

    const otaPassword = document.getElementById("security-ota-pass").value;
    const users = [
        {
            role: "admin",
            username: document.getElementById("security-admin-user").value.trim(),
            password: document.getElementById("security-admin-pass").value,
            enabled: true
        },
        {
            role: "operator",
            username: document.getElementById("security-operator-user").value.trim(),
            password: document.getElementById("security-operator-pass").value,
            enabled: document.getElementById("security-operator-enabled").checked
        },
        {
            role: "viewer",
            username: document.getElementById("security-viewer-user").value.trim(),
            password: document.getElementById("security-viewer-pass").value,
            enabled: document.getElementById("security-viewer-enabled").checked
        }
    ];

    if (!users[0].username) {
        addLog("Admin username cannot be empty", "ERR");
        showToast("Validation error", "Admin username cannot be empty.", "error");
        return;
    }

    const invalidRole = users.find((user) => user.enabled && user.password && user.password.length < 8);
    if (invalidRole) {
        addLog(`${invalidRole.role.toUpperCase()} password must have at least 8 characters`, "ERR");
        showToast("Validation error", `${invalidRole.role.toUpperCase()} password must have at least 8 characters.`, "error");
        return;
    }

    if (users[0].username !== authUser && !users[0].password) {
        addLog("Changing the admin username requires a new admin password in the same save", "ERR");
        showToast("Validation error", "Changing the admin username also requires a new admin password.", "error");
        return;
    }

    try {
        await fetchJSON("/api/security/users", {
            method: "POST",
            body: JSON.stringify({ users, otaPassword })
        });

        const nextAdminPassword = users[0].password || document.getElementById("login-password").value || "";
        if (users[0].password) {
            authHeader = `Basic ${btoa(`${users[0].username}:${users[0].password}`)}`;
        } else if (users[0].username !== authUser && nextAdminPassword) {
            authHeader = `Basic ${btoa(`${users[0].username}:${nextAdminPassword}`)}`;
        }
        authUser = users[0].username;
        document.getElementById("security-admin-pass").value = "";
        document.getElementById("security-operator-pass").value = "";
        document.getElementById("security-viewer-pass").value = "";

        const data = await fetchJSON("/api/ws-auth");
        wsToken = data.token || "";
        currentUserRole = data.role || "admin";
        if (ws) ws.close();
        initWS();
        if (hasRole("admin")) {
            await loadSecurityStatus();
        }
        await updateSystemHealth();
        addLog("Security settings updated", "AUTH");
        showToast("Security updated", "Users and passwords were saved.", "success");
    } catch (error) {
        addLog("Failed to update security settings", "ERR");
        showToast("Security update failed", "Could not save users or passwords.", "error");
    }
}

function clearLogs() {
    logEntries.length = 0;
    currentLogPage = 1;
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
    const lines = getFilteredLogEntries()
        .slice()
        .reverse()
        .map((entry) => `${entry.time} | ${entry.type} | ${entry.message}`);

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
            currentLogPage = 1;
            document.querySelectorAll(".log-filter-btn").forEach((item) => item.classList.toggle("active", item === button));
            applyLogFilter();
        });
    });
}

function initializeLogPagination() {
    document.getElementById("log-prev-btn").addEventListener("click", () => {
        currentLogPage = Math.max(1, currentLogPage - 1);
        renderLogPage();
    });

    document.getElementById("log-next-btn").addEventListener("click", () => {
        currentLogPage += 1;
        renderLogPage();
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
document.getElementById("export-config-btn").addEventListener("click", exportConfig);
document.getElementById("import-config-btn").addEventListener("click", triggerImportConfig);
document.getElementById("import-config-file").addEventListener("change", handleImportConfig);
document.getElementById("reset-network-config-btn").addEventListener("click", () => resetConfiguration("network"));
document.getElementById("reset-all-config-btn").addEventListener("click", () => resetConfiguration("all"));
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
initializeLogPagination();
initializeDacPresets();
updateModeChip("wifi-mode-chip", "WIFI MODE: OFFLINE", "offline");
updateModeChip("mqtt-mode-chip", "MQTT: WAITING", "warn");
updateModeChip("modbus-mode-chip", "MODBUS: READY", "live");
updateModeChip("ws-mode-chip", "WS: IDLE", "offline");
setDacPending(1, false);
setDacPending(2, false);
setProgramRunning(false);
updateOpsSnapshot();
applyRolePermissions();
renderLogPage();
