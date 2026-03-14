#include "AppServer.h"
#include <ArduinoJson.h>
#include <esp_system.h>
#include <SPIFFS.h>

AppServer::AppServer(ConfigManager& configMgr, HardwareHAL& hal, WiFiManager& wifiMgr, MQTTManager& mqttMgr, ModbusManager& modbusMgr) 
    : config(configMgr), hardware(hal), wifi(wifiMgr), mqtt(mqttMgr), modbus(modbusMgr), server(Config::HTTP_PORT), webSocket(Config::WS_PORT) {
    memset(authorizedClients, 0, sizeof(authorizedClients));
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
}

void AppServer::setupRoutes() {
    // Serve Index
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    server.on("/api/ws-auth", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();

        wsAuthToken = generateWsAuthToken();
        memset(authorizedClients, 0, sizeof(authorizedClients));

        DynamicJsonDocument doc(128);
        doc["token"] = wsAuthToken;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/security/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();

        DynamicJsonDocument doc(256);
        doc["adminUser"] = securityConfig.adminUser;
        doc["hasOtaPassword"] = strlen(securityConfig.otaPassword) > 0;
        doc["usingDefaultPassword"] = strcmp(securityConfig.adminPass, Config::ADMIN_PASS) == 0;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/health", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();

        DynamicJsonDocument doc(512);
        populateHealthJson(doc);

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // API: Get State
    server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();
        DynamicJsonDocument doc(1024);
        const DeviceState& state = hardware.getState();
        
        JsonArray relays = doc.createNestedArray("relays");
        for(int i=0; i<8; i++) relays.add(state.relays[i]);
        
        doc["v1"] = state.dac1_v;
        doc["v2"] = state.dac2_v;
        doc["wifiMode"] = (int)wifi.getMode();
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // API: WiFi Status
    server.on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();
        DynamicJsonDocument doc(256);
        doc["ssid"] = WiFi.SSID();
        doc["status"] = (int)WiFi.status();
        doc["ip"] = wifi.getIP();
        doc["rssi"] = WiFi.RSSI();
        doc["mode"] = (int)wifi.getMode();
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // API: WiFi Scan Results
    server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();
        request->send(200, "application/json", wifi.getScanResultsJSON());
    });
    
    server.on("/api/wifi/startScan", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();
        wifi.scanNetworks();
        request->send(200, "application/json", "{\"status\":\"scanning\"}");
    });

    // API: WiFi Save
    server.on("/api/wifi/save", HTTP_POST, [](AsyncWebServerRequest * request) {}, NULL, [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!authenticate(request)) return request->requestAuthentication();

        String* body = reinterpret_cast<String*>(request->_tempObject);
        if (index == 0) {
            delete body;
            body = new String();
            body->reserve(total);
            request->_tempObject = body;
        }

        if (body == nullptr) {
            request->send(500, "application/json", "{\"status\":\"error\"}");
            return;
        }

        for (size_t i = 0; i < len; i++) {
            body->concat((char)data[i]);
        }
        if ((index + len) < total) {
            return;
        }

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, *body);
        delete body;
        request->_tempObject = nullptr;

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

    server.on("/api/security/passwords", HTTP_POST, [](AsyncWebServerRequest * request) {}, NULL, [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!authenticate(request)) return request->requestAuthentication();

        String* body = reinterpret_cast<String*>(request->_tempObject);
        if (index == 0) {
            delete body;
            body = new String();
            body->reserve(total);
            request->_tempObject = body;
        }

        if (body == nullptr) {
            request->send(500, "application/json", "{\"status\":\"error\"}");
            return;
        }

        for (size_t i = 0; i < len; i++) {
            body->concat((char)data[i]);
        }
        if ((index + len) < total) {
            return;
        }

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, *body);
        delete body;
        request->_tempObject = nullptr;

        if (error) {
            request->send(400, "application/json", "{\"status\":\"invalid_json\"}");
            return;
        }

        const char* adminUser = doc["adminUser"];
        const char* adminPass = doc["adminPass"];
        const char* otaPassword = doc["otaPassword"];
        if (adminUser == nullptr || adminPass == nullptr || strlen(adminUser) == 0 || strlen(adminPass) < 8) {
            request->send(400, "application/json", "{\"status\":\"invalid_security_config\"}");
            return;
        }

        strncpy(securityConfig.adminUser, adminUser, sizeof(securityConfig.adminUser) - 1);
        securityConfig.adminUser[sizeof(securityConfig.adminUser) - 1] = '\0';
        strncpy(securityConfig.adminPass, adminPass, sizeof(securityConfig.adminPass) - 1);
        securityConfig.adminPass[sizeof(securityConfig.adminPass) - 1] = '\0';

        if (otaPassword != nullptr) {
            strncpy(securityConfig.otaPassword, otaPassword, sizeof(securityConfig.otaPassword) - 1);
            securityConfig.otaPassword[sizeof(securityConfig.otaPassword) - 1] = '\0';
        }

        config.saveSecurityConfig(securityConfig);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // API: System Info
    server.on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();
        DynamicJsonDocument doc(512);
        doc["heap"] = ESP.getFreeHeap();
        doc["chipId"] = ESP.getEfuseMac();
        doc["sdk"] = ESP.getSdkVersion();
        
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // Fallback
    server.serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html");
}

bool AppServer::authenticate(AsyncWebServerRequest *request) {
    return request->authenticate(securityConfig.adminUser, securityConfig.adminPass);
}

void AppServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            setClientAuthorized(num, false);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
                setClientAuthorized(num, false);
            }
            break;
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
        bool authenticated = token != nullptr && wsAuthToken.length() > 0 && wsAuthToken.equals(token);
        setClientAuthorized(num, authenticated);

        DynamicJsonDocument response(128);
        response["type"] = "auth";
        response["ok"] = authenticated;
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
        float voltage = doc["voltage"];
        hardware.setDAC(channel, voltage);
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
    doc["wifiMode"] = (int)wifi.getMode();
    doc["mqtt"] = mqtt.isConnected();
    doc["modbus"] = modbus.isHealthy();

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
        doc["state"] = (bool)singleState;
    } else {
        JsonArray relays = doc.createNestedArray("relays");
        for(int i=0; i<8; i++) relays.add(state.relays[i]);
    }
    
    doc["v1"] = state.dac1_v;
    doc["v2"] = state.dac2_v;
    doc["mqtt"] = mqtt.isConnected();
    doc["wifiMode"] = (int)wifi.getMode();
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

String AppServer::generateWsAuthToken() const {
    char token[17];
    snprintf(token, sizeof(token), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    return String(token);
}

bool AppServer::hasValidRelayIndex(int idx) const {
    return idx >= 0 && idx < Config::RELAY_COUNT;
}

bool AppServer::hasValidDacChannel(int channel) const {
    return channel == 1 || channel == 2;
}

void AppServer::populateHealthJson(DynamicJsonDocument& doc) const {
    doc["wifiConnected"] = wifi.isConnected();
    doc["wifiMode"] = (int)wifi.getMode();
    doc["wifiIp"] = wifi.getIP();
    doc["mqttConnected"] = mqtt.isConnected();
    doc["modbusHealthy"] = modbus.isHealthy();
    doc["heap"] = ESP.getFreeHeap();
}
