#pragma once
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include "../common/Constants.h"
#include "../common/Types.h"
#include "../config/ConfigManager.h"
#include "../hardware/HardwareHAL.h"
#include "../wifi/WiFiManager.h"
#include "../mqtt/MQTTManager.h"
#include "../modbus/ModbusManager.h"

class AppServer {
public:
    AppServer(ConfigManager& configMgr, HardwareHAL& hal, WiFiManager& wifiMgr, MQTTManager& mqttMgr, ModbusManager& modbusMgr);
    void begin();
    void loop();
    
    void broadcastUpdate(int singleIdx = -1, int singleState = -1);

private:
    static const uint8_t MAX_WS_CLIENTS = 8;
    ConfigManager& config;
    HardwareHAL& hardware;
    WiFiManager& wifi;
    MQTTManager& mqtt;
    ModbusManager& modbus;
    AsyncWebServer server;
    WebSocketsServer webSocket;
    String wsAuthToken;
    bool authorizedClients[MAX_WS_CLIENTS];
    SecurityConfig securityConfig;
    
    void setupRoutes();
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length);
    void sendStateToClient(uint8_t num);
    bool isClientAuthorized(uint8_t num) const;
    void setClientAuthorized(uint8_t num, bool authorized);
    String generateWsAuthToken() const;
    bool hasValidRelayIndex(int idx) const;
    bool hasValidDacChannel(int channel) const;
    void populateHealthJson(DynamicJsonDocument& doc) const;
    
    bool authenticate(AsyncWebServerRequest *request);
};
