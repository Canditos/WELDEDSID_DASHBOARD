#pragma once
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include "../common/Constants.h"
#include "../common/Types.h"
#include "../hardware/HardwareHAL.h"
#include "../wifi/WiFiManager.h"
#include "../mqtt/MQTTManager.h"

class AppServer {
public:
    AppServer(HardwareHAL& hal, WiFiManager& wifiMgr, MQTTManager& mqttMgr);
    void begin();
    void loop();
    
    void broadcastUpdate(int singleIdx = -1, int singleState = -1);

private:
    HardwareHAL& hardware;
    WiFiManager& wifi;
    MQTTManager& mqtt;
    AsyncWebServer server;
    WebSocketsServer webSocket;
    
    void setupRoutes();
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length);
    
    bool authenticate(AsyncWebServerRequest *request);
};
