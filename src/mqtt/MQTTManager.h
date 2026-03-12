#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include "../common/Constants.h"
#include "../common/Types.h"
#include "../config/ConfigManager.h"
#include "../hardware/HardwareHAL.h"

class MQTTManager {
public:
    MQTTManager(ConfigManager& configMgr, HardwareHAL& hal);
    void begin();
    void loop();
    
    bool isConnected() { return client.connected(); }

private:
    ConfigManager& config;
    HardwareHAL& hardware;
    WiFiClient espClient;
    PubSubClient client;
    NetworkConfig netConfig;
    uint32_t lastReconnectAttempt;
    uint32_t lastPublishTime;
    
    void reconnect();
    void callback(char* topic, byte* payload, unsigned int length);
    void publishStatus();
};
