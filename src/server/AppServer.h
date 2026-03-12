#pragma once
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include "../common/Constants.h"
#include "../common/Types.h"
#include "../hardware/HardwareHAL.h"
#include "../wifi/WiFiManager.h"

class AppServer {
public:
    AppServer(HardwareHAL& hal, WiFiManager& wifiMgr);
    void begin();
    void loop();
    
    void broadcastUpdate();

private:
    HardwareHAL& hardware;
    WiFiManager& wifi;
    AsyncWebServer server;
    WebSocketsServer webSocket;
    
    void setupRoutes();
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length);
    
    bool authenticate(AsyncWebServerRequest *request);
};
