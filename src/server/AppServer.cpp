#include "AppServer.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>

AppServer::AppServer(HardwareHAL& hal, WiFiManager& wifiMgr, MQTTManager& mqttMgr) 
    : hardware(hal), wifi(wifiMgr), mqtt(mqttMgr), server(Config::HTTP_PORT), webSocket(Config::WS_PORT) {}

void AppServer::begin() {
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
    // Basic Auth wrapper
    auto protectedHandler = [this](AsyncWebServerRequest *request, ArRequestHandlerFunction handler) {
        if (!authenticate(request)) {
            return request->requestAuthentication();
        }
        handler(request);
    };

    // Serve Index
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return request->requestAuthentication();
        request->send(SPIFFS, "/index.html", "text/html");
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
        doc["wifiMode"] = (int)state.wifiMode;
        
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
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        
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
        
        DynamicJsonDocument doc(512);
        deserializeJson(doc, data);
        
        if (doc.containsKey("ssid") && doc.containsKey("pass")) {
            wifi.saveCredentials(doc["ssid"], doc["pass"]);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\"}");
        }
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
    server.serveStatic("/", SPIFFS, "/");
}

bool AppServer::authenticate(AsyncWebServerRequest *request) {
    return request->authenticate(Config::ADMIN_USER, Config::ADMIN_PASS);
}

void AppServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
                
                // Send initial state
                DynamicJsonDocument doc(1024);
                const DeviceState& state = hardware.getState();
                doc["type"] = "init";
                JsonArray relays = doc.createNestedArray("relays");
                for(int i=0; i<8; i++) relays.add(state.relays[i]);
                doc["v1"] = state.dac1_v;
                doc["v2"] = state.dac2_v;
                doc["wifiMode"] = "STA"; 
                doc["mqtt"] = mqtt.isConnected();
                doc["modbus"] = true; // Always true if mb.task() is running as slave
                
                String output;
                serializeJson(doc, output);
                webSocket.sendTXT(num, output);
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
    if (strcmp(cmd, "relay") == 0) {
        int idx = doc["idx"];
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
        float voltage = doc["voltage"];
        hardware.setDAC(channel, voltage);
        broadcastUpdate();
    } else if (strcmp(cmd, "dac_all") == 0) {
        hardware.setDAC(1, doc["v1"]);
        hardware.setDAC(2, doc["v2"]);
        broadcastUpdate();
    } else if (strcmp(cmd, "ramp") == 0) {
        int channel = doc["channel"];
        float target = doc["target"];
        uint32_t duration = doc["duration"];
        hardware.startRamp(channel, target, duration);
    } else if (strcmp(cmd, "step_ramp") == 0) {
        int channel = doc["channel"];
        float start = doc["start"];
        float target = doc["target"];
        float step = doc["step"];
        uint32_t stepMs = doc["duration"];
        hardware.startStepProgram(channel, start, target, step, stepMs);
    }
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
    doc["modbus"] = true;
    
    String output;
    serializeJson(doc, output);
    webSocket.broadcastTXT(output);
}
