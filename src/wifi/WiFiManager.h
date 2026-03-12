#pragma once
#include <WiFi.h>
#include "../common/Constants.h"
#include "../common/Types.h"
#include "../config/ConfigManager.h"

class WiFiManager {
public:
    WiFiManager(ConfigManager& configMgr);
    void begin();
    void loop();
    
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    WiFiModeState getMode() const { return currentMode; }
    String getIP() const { return WiFi.localIP().toString(); }
    
    void scanNetworks();
    String getScanResultsJSON();
    bool saveCredentials(const char* ssid, const char* pass);

private:
    ConfigManager& config;
    NetworkConfig netConfig;
    WiFiModeState currentMode;
    uint32_t connectionStartTime;
    bool isScanning;
    
    void startSTA();
    void startAP();
    void handleSTAFailure();
};
