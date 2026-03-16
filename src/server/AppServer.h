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
#include "../security/SecurityHelpers.h"

class AppServer {
public:
    AppServer(ConfigManager& configMgr, HardwareHAL& hal, WiFiManager& wifiMgr, MQTTManager& mqttMgr, ModbusManager& modbusMgr);
    void begin();
    void loop();
    
    void broadcastUpdate(int singleIdx = -1, int singleState = -1);

private:
    static const uint8_t MAX_WS_CLIENTS = 8;
    struct PendingWsSession {
        bool active;
        String token;
        UserRole role;
        String username;
    };

    ConfigManager& config;
    HardwareHAL& hardware;
    WiFiManager& wifi;
    MQTTManager& mqtt;
    ModbusManager& modbus;
    AsyncWebServer server;
    WebSocketsServer webSocket;
    bool authorizedClients[MAX_WS_CLIENTS];
    UserRole clientRoles[MAX_WS_CLIENTS];
    String clientUsernames[MAX_WS_CLIENTS];
    PendingWsSession pendingSessions[MAX_WS_CLIENTS];
    SecurityConfig securityConfig;
    bool restartScheduled;
    uint32_t restartAtMs;
    
    void setupRoutes();
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length);
    void sendStateToClient(uint8_t num);
    bool isClientAuthorized(uint8_t num) const;
    void setClientAuthorized(uint8_t num, bool authorized);
    void setClientRole(uint8_t num, UserRole role, const String& username);
    String generateWsAuthToken() const;
    int allocatePendingSession(const String& token, UserRole role, const String& username);
    int findPendingSessionByToken(const char* token) const;
    void clearPendingSession(int index);
    bool hasValidRelayIndex(int idx) const;
    bool hasValidDacChannel(int channel) const;
    void scheduleRestart(uint32_t delayMs);
    void populateHealthJson(DynamicJsonDocument& doc) const;
    void populateSecurityStatusJson(DynamicJsonDocument& doc) const;
    void populateConfigExportJson(DynamicJsonDocument& doc) const;
    bool rejectUnauthorized(AsyncWebServerRequest *request) const;
    bool rejectForbidden(AsyncWebServerRequest *request) const;
    UserRole getRequestRole(AsyncWebServerRequest *request, String* username = nullptr) const;
    bool requireRole(AsyncWebServerRequest *request, UserRole requiredRole, UserRole* matchedRole = nullptr, String* username = nullptr) const;
};
