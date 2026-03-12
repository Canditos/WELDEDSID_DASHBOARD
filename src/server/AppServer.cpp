#include "AppServer.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>

AppServer::AppServer(HardwareHAL& hal, WiFiManager& wifiMgr) 
    : hardware(hal), wifi(wifiMgr), server(Config::HTTP_PORT), webSocket(Config::WS_PORT) {}

void AppServer::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
    }

    setupRoutes();
    server.begin();
    
    webSocket.begin();
    webSocket.onEvent([this](uint8_t n, WStype_t t, uint8_t* p, size_t l) { 
        this->onWebSocketEvent(n, t, p, l); 
    });
}

void AppServer::loop() {
    webSocket.loop();
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

    // API: WiFi Scan
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
                doc["wifiMode"] = "STA"; // Placeholder logic for now
                doc["mqtt"] = false;
                
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
    }
}

void AppServer::broadcastUpdate() {
    DynamicJsonDocument doc(1024);
    const DeviceState& state = hardware.getState();
    doc["type"] = "update";
    JsonArray relays = doc.createNestedArray("relays");
    for(int i=0; i<8; i++) relays.add(state.relays[i]);
    doc["v1"] = state.dac1_v;
    doc["v2"] = state.dac2_v;
    
    String output;
    serializeJson(doc, output);
    webSocket.broadcastTXT(output);
}
