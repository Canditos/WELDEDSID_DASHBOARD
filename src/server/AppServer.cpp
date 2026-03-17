#include "AppServer.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <esp_system.h>
#include <string.h>

namespace {
bool accumulateRequestBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total, String*& body) {
    body = reinterpret_cast<String*>(request->_tempObject);
    if (index == 0) {
        delete body;
        body = new String();
        body->reserve(total);
        request->_tempObject = body;
    }

    if (body == nullptr) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        body->concat(static_cast<char>(data[i]));
    }

    return (index + len) >= total;
}

void cleanupRequestBody(AsyncWebServerRequest* request) {
    String* body = reinterpret_cast<String*>(request->_tempObject);
    delete body;
    request->_tempObject = nullptr;
}

void copyJsonString(char* dest, size_t destSize, JsonVariantConst value) {
    const char* raw = value.as<const char*>();
    if (destSize == 0) return;
    if (raw == nullptr) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, raw, destSize - 1);
    dest[destSize - 1] = '\0';
}

void appendRoleUser(JsonArray array, const SecurityUser& user) {
    JsonObject item = array.createNestedObject();
    item["role"] = SecurityHelpers::roleToString(user.role);
    item["username"] = user.username;
    item["enabled"] = user.enabled;
    item["usingDefault"] = SecurityHelpers::isDefaultCredential(user);
}
}

AppServer::AppServer(ConfigManager& configMgr, HardwareHAL& hal, WiFiManager& wifiMgr, MQTTManager& mqttMgr, ModbusManager& modbusMgr)
    : config(configMgr), hardware(hal), wifi(wifiMgr), mqtt(mqttMgr), modbus(modbusMgr), server(Config::HTTP_PORT), webSocket(Config::WS_PORT), restartScheduled(false), restartAtMs(0) {
    memset(authorizedClients, 0, sizeof(authorizedClients));
    memset(clientRoles, 0, sizeof(clientRoles));
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; ++i) {
        pendingSessions[i].active = false;
    }
}

void AppServer::begin() {
    memset(&securityConfig, 0, sizeof(securityConfig));
    config.loadSecurityConfig(securityConfig);

    Serial.println("[SERVER] Starting SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("[SERVER] ERROR: SPIFFS mount failed!");
    } else {
        Serial.println("[SERVER] SPIFFS mounted successfully.");
        if (SPIFFS.exists("/index.html")) {
            Serial.println("[SERVER] index.html found in SPIFFS.");
        } else {
            Serial.println("[SERVER] ERROR: index.html NOT found!");
        }
    }

    setupRoutes();
    server.begin();
    Serial.println("[SERVER] HTTP Server started on port 80.");

    webSocket.begin();
    webSocket.onEvent([this](uint8_t n, WStype_t t, uint8_t* p, size_t l) {
        this->onWebSocketEvent(n, t, p, l);
    });
    Serial.println("[SERVER] WebSockets started on port 81.");
}

void AppServer::loop() {
    webSocket.loop();
    if (hardware.hasStateChanged()) {
        broadcastUpdate();
    }

    if (restartScheduled && millis() >= restartAtMs) {
        Serial.println("[SERVER] Restarting controller to apply configuration reset.");
        delay(100);
        ESP.restart();
    }
}

void AppServer::setupRoutes() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    server.on("/api/ws-auth", HTTP_GET, [this](AsyncWebServerRequest* request) {
        UserRole matchedRole;
        String username;
        if (!requireRole(request, UserRole::VIEWER, &matchedRole, &username)) return;

        String token = generateWsAuthToken();
        allocatePendingSession(token, matchedRole, username);

        DynamicJsonDocument doc(256);
        doc["token"] = token;
        doc["role"] = SecurityHelpers::roleToString(matchedRole);
        doc["username"] = username;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/security/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::ADMIN)) return;

        DynamicJsonDocument doc(768);
        populateSecurityStatusJson(doc);

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/health", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::VIEWER)) return;

        DynamicJsonDocument doc(512);
        populateHealthJson(doc);

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::VIEWER)) return;

        DynamicJsonDocument doc(1024);
        const DeviceState& state = hardware.getState();
        JsonArray relays = doc.createNestedArray("relays");
        for (int i = 0; i < 8; i++) relays.add(state.relays[i]);

        doc["v1"] = state.dac1_v;
        doc["v2"] = state.dac2_v;
        doc["wifiMode"] = static_cast<int>(wifi.getMode());

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::VIEWER)) return;

        DynamicJsonDocument doc(256);
        doc["ssid"] = WiFi.SSID();
        doc["status"] = static_cast<int>(WiFi.status());
        doc["ip"] = wifi.getIP();
        doc["rssi"] = WiFi.RSSI();
        doc["mode"] = static_cast<int>(wifi.getMode());

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::VIEWER)) return;
        request->send(200, "application/json", wifi.getScanResultsJSON());
    });

    server.on("/api/wifi/startScan", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::ADMIN)) return;
        wifi.scanNetworks();
        request->send(200, "application/json", "{\"status\":\"scanning\"}");
    });

    server.on("/api/wifi/save", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!requireRole(request, UserRole::ADMIN)) {
            cleanupRequestBody(request);
            return;
        }

        String* body = nullptr;
        if (!accumulateRequestBody(request, data, len, index, total, body)) {
            if (body == nullptr) {
                request->send(500, "application/json", "{\"status\":\"error\"}");
            }
            return;
        }

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, *body);
        cleanupRequestBody(request);

        if (error) {
            request->send(400, "application/json", "{\"status\":\"invalid_json\"}");
            return;
        }

        if (doc["ssid"].is<const char*>() && doc["pass"].is<const char*>()) {
            wifi.saveCredentials(doc["ssid"], doc["pass"]);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\"}");
        }
    });

    server.on("/api/security/users", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!requireRole(request, UserRole::ADMIN)) {
            cleanupRequestBody(request);
            return;
        }

        String* body = nullptr;
        if (!accumulateRequestBody(request, data, len, index, total, body)) {
            if (body == nullptr) {
                request->send(500, "application/json", "{\"status\":\"error\"}");
            }
            return;
        }

        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, *body);
        cleanupRequestBody(request);
        if (error) {
            request->send(400, "application/json", "{\"status\":\"invalid_json\"}");
            return;
        }

        SecurityConfig nextConfig = securityConfig;
        JsonArray users = doc["users"].as<JsonArray>();
        if (users.isNull() || users.size() != Config::SECURITY_USER_COUNT) {
            request->send(400, "application/json", "{\"status\":\"invalid_users\"}");
            return;
        }

        for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
            JsonObject user = users[i];
            nextConfig.users[i].role = (i == 0) ? UserRole::ADMIN : (i == 1 ? UserRole::OPERATOR : UserRole::VIEWER);
            nextConfig.users[i].enabled = user["enabled"] | true;
            copyJsonString(nextConfig.users[i].username, sizeof(nextConfig.users[i].username), user["username"]);

            const char* password = user["password"];
            if (password != nullptr && strlen(password) > 0) {
                copyJsonString(nextConfig.users[i].password, sizeof(nextConfig.users[i].password), user["password"]);
            }
        }

        if (doc["otaPassword"].is<const char*>()) {
            copyJsonString(nextConfig.otaPassword, sizeof(nextConfig.otaPassword), doc["otaPassword"]);
        }

        char errorMessage[96];
        if (!SecurityHelpers::validateSecurityConfig(nextConfig, errorMessage, sizeof(errorMessage))) {
            DynamicJsonDocument response(192);
            response["status"] = "invalid_security_config";
            response["message"] = errorMessage;
            String output;
            serializeJson(response, output);
            request->send(400, "application/json", output);
            return;
        }

        securityConfig = nextConfig;
        config.saveSecurityConfig(securityConfig);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::ADMIN)) return;

        DynamicJsonDocument doc(2048);
        populateConfigExportJson(doc);

        String output;
        serializeJsonPretty(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/config/import", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!requireRole(request, UserRole::ADMIN)) {
            cleanupRequestBody(request);
            return;
        }

        String* body = nullptr;
        if (!accumulateRequestBody(request, data, len, index, total, body)) {
            if (body == nullptr) {
                request->send(500, "application/json", "{\"status\":\"error\"}");
            }
            return;
        }

        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, *body);
        cleanupRequestBody(request);
        if (error) {
            request->send(400, "application/json", "{\"status\":\"invalid_json\"}");
            return;
        }

        if (doc["network"].is<JsonObject>()) {
            NetworkConfig net;
            config.loadNetworkConfig(net);
            JsonObject network = doc["network"];
            copyJsonString(net.ssid, sizeof(net.ssid), network["ssid"]);
            copyJsonString(net.pass, sizeof(net.pass), network["pass"]);
            copyJsonString(net.deviceId, sizeof(net.deviceId), network["deviceId"]);
            copyJsonString(net.mqttHost, sizeof(net.mqttHost), network["mqttHost"]);
            net.mqttPort = network["mqttPort"] | net.mqttPort;
            copyJsonString(net.mqttUser, sizeof(net.mqttUser), network["mqttUser"]);
            copyJsonString(net.mqttPass, sizeof(net.mqttPass), network["mqttPass"]);
            net.mqttEnabled = network["mqttEnabled"] | net.mqttEnabled;
            config.saveNetworkConfig(net);
        }

        if (doc["security"].is<JsonObject>()) {
            SecurityConfig nextConfig;
            SecurityHelpers::applyDefaultSecurityConfig(nextConfig);
            JsonObject security = doc["security"];
            JsonArray users = security["users"].as<JsonArray>();
            if (!users.isNull() && users.size() == Config::SECURITY_USER_COUNT) {
                for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
                    JsonObject user = users[i];
                    nextConfig.users[i].role = (i == 0) ? UserRole::ADMIN : (i == 1 ? UserRole::OPERATOR : UserRole::VIEWER);
                    nextConfig.users[i].enabled = user["enabled"] | true;
                    copyJsonString(nextConfig.users[i].username, sizeof(nextConfig.users[i].username), user["username"]);
                    copyJsonString(nextConfig.users[i].password, sizeof(nextConfig.users[i].password), user["password"]);
                }
            }

            if (security["otaPassword"].is<const char*>()) {
                copyJsonString(nextConfig.otaPassword, sizeof(nextConfig.otaPassword), security["otaPassword"]);
            }

            char errorMessage[96];
            if (!SecurityHelpers::validateSecurityConfig(nextConfig, errorMessage, sizeof(errorMessage))) {
                DynamicJsonDocument response(192);
                response["status"] = "invalid_security_config";
                response["message"] = errorMessage;
                String output;
                serializeJson(response, output);
                request->send(400, "application/json", output);
                return;
            }

            securityConfig = nextConfig;
            config.saveSecurityConfig(securityConfig);
        }

        if (doc["hardware"].is<JsonObject>()) {
            JsonObject hardwareConfig = doc["hardware"];
            uint8_t relayMask = hardwareConfig["relayMask"] | 0;
            hardware.setRelayMask(relayMask);
            if (hardwareConfig["dac1"].is<float>()) {
                hardware.setDAC(1, hardwareConfig["dac1"].as<float>());
            }
            if (hardwareConfig["dac2"].is<float>()) {
                hardware.setDAC(2, hardwareConfig["dac2"].as<float>());
            }
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/api/config/reset", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!requireRole(request, UserRole::ADMIN)) {
            cleanupRequestBody(request);
            return;
        }

        String* body = nullptr;
        if (!accumulateRequestBody(request, data, len, index, total, body)) {
            if (body == nullptr) {
                request->send(500, "application/json", "{\"status\":\"error\"}");
            }
            return;
        }

        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, *body);
        cleanupRequestBody(request);
        if (error) {
            request->send(400, "application/json", "{\"status\":\"invalid_json\"}");
            return;
        }

        const char* scope = doc["scope"] | "all";
        bool requiresRestart = false;
        if (strcmp(scope, "network") == 0) {
            config.resetNetworkConfig();
            requiresRestart = true;
        } else if (strcmp(scope, "security") == 0) {
            config.resetSecurityConfig();
            config.loadSecurityConfig(securityConfig);
        } else if (strcmp(scope, "hardware") == 0) {
            config.resetHardwareState();
            hardware.setRelayMask(0);
            hardware.setDAC(1, Config::DAC1_MIN_V);
            hardware.setDAC(2, Config::DAC2_MIN_V);
        } else if (strcmp(scope, "all") == 0) {
            config.resetNetworkConfig();
            config.resetSecurityConfig();
            config.resetHardwareState();
            config.loadSecurityConfig(securityConfig);
            hardware.setRelayMask(0);
            hardware.setDAC(1, Config::DAC1_MIN_V);
            hardware.setDAC(2, Config::DAC2_MIN_V);
            requiresRestart = true;
        } else {
            request->send(400, "application/json", "{\"status\":\"invalid_scope\"}");
            return;
        }

        DynamicJsonDocument response(192);
        response["status"] = "ok";
        response["restartRequired"] = requiresRestart;
        if (requiresRestart) {
            response["message"] = "Controller will restart to apply reset.";
            scheduleRestart(800);
        }

        String output;
        serializeJson(response, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!requireRole(request, UserRole::VIEWER)) return;

        DynamicJsonDocument doc(512);
        doc["heap"] = ESP.getFreeHeap();
        doc["chipId"] = ESP.getEfuseMac();
        doc["sdk"] = ESP.getSdkVersion();

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html");
}

bool AppServer::rejectUnauthorized(AsyncWebServerRequest* request) const {
    request->send(401, "application/json", "{\"status\":\"unauthorized\"}");
    return false;
}

bool AppServer::rejectForbidden(AsyncWebServerRequest* request) const {
    request->send(403, "application/json", "{\"status\":\"forbidden\"}");
    return false;
}

UserRole AppServer::getRequestRole(AsyncWebServerRequest* request, String* username) const {
    for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        const SecurityUser& user = securityConfig.users[i];
        if (!SecurityHelpers::isUserConfigured(user)) continue;
        if (request->authenticate(user.username, user.password)) {
            if (username != nullptr) {
                *username = user.username;
            }
            return user.role;
        }
    }

    if (username != nullptr) {
        username->clear();
    }
    return static_cast<UserRole>(255);
}

bool AppServer::requireRole(AsyncWebServerRequest* request, UserRole requiredRole, UserRole* matchedRole, String* username) const {
    String localUsername;
    UserRole role = getRequestRole(request, &localUsername);
    if (role == static_cast<UserRole>(255)) {
        return rejectUnauthorized(request);
    }

    if (!SecurityHelpers::roleAtLeast(role, requiredRole)) {
        return rejectForbidden(request);
    }

    if (matchedRole != nullptr) {
        *matchedRole = role;
    }
    if (username != nullptr) {
        *username = localUsername;
    }
    return true;
}

void AppServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            setClientAuthorized(num, false);
            setClientRole(num, UserRole::VIEWER, "");
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
            setClientAuthorized(num, false);
            setClientRole(num, UserRole::VIEWER, "");
            break;
        }
        case WStype_TEXT:
            handleWebSocketMessage(num, payload, length);
            break;
        default:
            break;
    }
}

void AppServer::handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) return;

    const char* cmd = doc["cmd"];
    if (cmd == nullptr) return;

    if (strcmp(cmd, "auth") == 0) {
        const char* token = doc["token"];
        int sessionIndex = token != nullptr ? findPendingSessionByToken(token) : -1;
        bool authenticated = sessionIndex >= 0;
        setClientAuthorized(num, authenticated);

        DynamicJsonDocument response(192);
        response["type"] = "auth";
        response["ok"] = authenticated;
        if (authenticated) {
            setClientRole(num, pendingSessions[sessionIndex].role, pendingSessions[sessionIndex].username);
            response["role"] = SecurityHelpers::roleToString(pendingSessions[sessionIndex].role);
            response["username"] = pendingSessions[sessionIndex].username;
            clearPendingSession(sessionIndex);
        }

        String output;
        serializeJson(response, output);
        webSocket.sendTXT(num, output);

        if (authenticated) {
            sendStateToClient(num);
        }
        return;
    }

    if (!isClientAuthorized(num)) {
        DynamicJsonDocument response(128);
        response["type"] = "error";
        response["message"] = "unauthorized";
        String output;
        serializeJson(response, output);
        webSocket.sendTXT(num, output);
        return;
    }

    if (!SecurityHelpers::roleAtLeast(clientRoles[num], UserRole::OPERATOR)) {
        DynamicJsonDocument response(160);
        response["type"] = "error";
        response["message"] = "forbidden";
        String output;
        serializeJson(response, output);
        webSocket.sendTXT(num, output);
        return;
    }

    if (strcmp(cmd, "relay") == 0) {
        int idx = doc["idx"];
        if (!hasValidRelayIndex(idx)) return;
        bool state = doc["state"];
        hardware.setRelay(idx, state);
        broadcastUpdate(idx, state);
    } else if (strcmp(cmd, "relay_all") == 0) {
        bool state = doc["state"];
        hardware.setRelayMask(state ? 0xFF : 0x00);
        broadcastUpdate();
    } else if (strcmp(cmd, "relay_mask") == 0) {
        uint8_t mask = doc["mask"];
        hardware.setRelayMask(mask);
        broadcastUpdate();
    } else if (strcmp(cmd, "dac") == 0) {
        int channel = doc["channel"];
        if (!hasValidDacChannel(channel) || !doc["voltage"].is<float>()) return;
        hardware.setDAC(channel, doc["voltage"].as<float>());
        broadcastUpdate();
    } else if (strcmp(cmd, "dac_all") == 0) {
        hardware.setDAC(1, doc["v1"]);
        hardware.setDAC(2, doc["v2"]);
        broadcastUpdate();
    } else if (strcmp(cmd, "ramp") == 0) {
        int channel = doc["channel"];
        if (!hasValidDacChannel(channel)) return;
        float target = doc["target"];
        uint32_t duration = doc["duration"];
        if (duration == 0) return;
        hardware.startRamp(channel, target, duration);
    } else if (strcmp(cmd, "stop_ramp") == 0) {
        int channel = doc["channel"];
        if (!hasValidDacChannel(channel)) return;
        hardware.stopRamp(channel);
    } else if (strcmp(cmd, "step_ramp") == 0) {
        int channel = doc["channel"];
        if (!hasValidDacChannel(channel)) return;
        float start = doc["start"];
        float target = doc["target"];
        float step = doc["step"];
        uint32_t stepMs = doc["duration"];
        if (step <= 0 || stepMs == 0) return;
        hardware.startStepProgram(channel, start, target, step, stepMs);
    }
}

void AppServer::sendStateToClient(uint8_t num) {
    if (!isClientAuthorized(num)) return;

    DynamicJsonDocument doc(1024);
    const DeviceState& state = hardware.getState();
    doc["type"] = "init";
    JsonArray relays = doc.createNestedArray("relays");
    for (int i = 0; i < 8; i++) relays.add(state.relays[i]);
    doc["v1"] = state.dac1_v;
    doc["v2"] = state.dac2_v;
    doc["wifiMode"] = static_cast<int>(wifi.getMode());
    doc["mqtt"] = mqtt.isConnected();
    doc["modbus"] = modbus.isHealthy();
    doc["role"] = SecurityHelpers::roleToString(clientRoles[num]);
    doc["username"] = clientUsernames[num];

    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(num, output);
}

void AppServer::broadcastUpdate(int singleIdx, int singleState) {
    DynamicJsonDocument doc(1024);
    const DeviceState& state = hardware.getState();
    doc["type"] = "update";

    if (singleIdx != -1) {
        doc["idx"] = singleIdx;
        doc["state"] = static_cast<bool>(singleState);
    } else {
        JsonArray relays = doc.createNestedArray("relays");
        for (int i = 0; i < 8; i++) relays.add(state.relays[i]);
    }

    doc["v1"] = state.dac1_v;
    doc["v2"] = state.dac2_v;
    doc["mqtt"] = mqtt.isConnected();
    doc["wifiMode"] = static_cast<int>(wifi.getMode());
    doc["modbus"] = modbus.isHealthy();

    String output;
    serializeJson(doc, output);
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
        if (isClientAuthorized(i)) {
            webSocket.sendTXT(i, output);
        }
    }
}

bool AppServer::isClientAuthorized(uint8_t num) const {
    return num < MAX_WS_CLIENTS && authorizedClients[num];
}

void AppServer::setClientAuthorized(uint8_t num, bool authorized) {
    if (num < MAX_WS_CLIENTS) {
        authorizedClients[num] = authorized;
    }
}

void AppServer::setClientRole(uint8_t num, UserRole role, const String& username) {
    if (num < MAX_WS_CLIENTS) {
        clientRoles[num] = role;
        clientUsernames[num] = username;
    }
}

String AppServer::generateWsAuthToken() const {
    char token[17];
    snprintf(token, sizeof(token), "%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    return String(token);
}

int AppServer::allocatePendingSession(const String& token, UserRole role, const String& username) {
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; ++i) {
        if (!pendingSessions[i].active) {
            pendingSessions[i].active = true;
            pendingSessions[i].token = token;
            pendingSessions[i].role = role;
            pendingSessions[i].username = username;
            return i;
        }
    }

    pendingSessions[0].active = true;
    pendingSessions[0].token = token;
    pendingSessions[0].role = role;
    pendingSessions[0].username = username;
    return 0;
}

int AppServer::findPendingSessionByToken(const char* token) const {
    if (token == nullptr) return -1;
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; ++i) {
        if (pendingSessions[i].active && pendingSessions[i].token.equals(token)) {
            return i;
        }
    }
    return -1;
}

void AppServer::clearPendingSession(int index) {
    if (index < 0 || index >= MAX_WS_CLIENTS) return;
    pendingSessions[index].active = false;
    pendingSessions[index].token = "";
    pendingSessions[index].role = UserRole::VIEWER;
    pendingSessions[index].username = "";
}

bool AppServer::hasValidRelayIndex(int idx) const {
    return idx >= 0 && idx < Config::RELAY_COUNT;
}

bool AppServer::hasValidDacChannel(int channel) const {
    return channel == 1 || channel == 2;
}

void AppServer::populateHealthJson(DynamicJsonDocument& doc) const {
    doc["wifiConnected"] = wifi.isConnected();
    doc["wifiMode"] = static_cast<int>(wifi.getMode());
    doc["wifiIp"] = wifi.getIP();
    doc["mqttConnected"] = mqtt.isConnected();
    doc["modbusHealthy"] = modbus.isHealthy();
    doc["heap"] = ESP.getFreeHeap();
}

void AppServer::populateSecurityStatusJson(DynamicJsonDocument& doc) const {
    JsonArray users = doc.createNestedArray("users");
    for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        appendRoleUser(users, securityConfig.users[i]);
    }
    doc["hasOtaPassword"] = strlen(securityConfig.otaPassword) > 0;
}

void AppServer::populateConfigExportJson(DynamicJsonDocument& doc) const {
    NetworkConfig networkConfig;
    config.loadNetworkConfig(networkConfig);

    JsonObject network = doc.createNestedObject("network");
    network["ssid"] = networkConfig.ssid;
    network["pass"] = networkConfig.pass;
    network["deviceId"] = networkConfig.deviceId;
    network["mqttHost"] = networkConfig.mqttHost;
    network["mqttPort"] = networkConfig.mqttPort;
    network["mqttUser"] = networkConfig.mqttUser;
    network["mqttPass"] = networkConfig.mqttPass;
    network["mqttEnabled"] = networkConfig.mqttEnabled;

    JsonObject security = doc.createNestedObject("security");
    JsonArray users = security.createNestedArray("users");
    for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        JsonObject item = users.createNestedObject();
        item["role"] = SecurityHelpers::roleToString(securityConfig.users[i].role);
        item["username"] = securityConfig.users[i].username;
        item["password"] = securityConfig.users[i].password;
        item["enabled"] = securityConfig.users[i].enabled;
    }
    security["otaPassword"] = securityConfig.otaPassword;

    const DeviceState& state = hardware.getState();
    JsonObject hw = doc.createNestedObject("hardware");
    uint8_t relayMask = 0;
    for (uint8_t i = 0; i < Config::RELAY_COUNT; ++i) {
        if (state.relays[i]) {
            relayMask |= (1 << i);
        }
    }
    hw["relayMask"] = relayMask;
    hw["dac1"] = state.dac1_v;
    hw["dac2"] = state.dac2_v;
}

void AppServer::scheduleRestart(uint32_t delayMs) {
    restartScheduled = true;
    restartAtMs = millis() + delayMs;
}
