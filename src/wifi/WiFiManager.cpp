#include "WiFiManager.h"
#include <ArduinoJson.h>

WiFiManager::WiFiManager(ConfigManager& configMgr) 
    : config(configMgr), currentMode(WiFiModeState::OFF), isScanning(false) {}

void WiFiManager::begin() {
    config.loadNetworkConfig(netConfig);
    
    if (strlen(netConfig.ssid) == 0) {
        startAP();
    } else {
        startSTA();
    }
}

void WiFiManager::loop() {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck < 1000) return;
    lastCheck = millis();

    switch (currentMode) {
        case WiFiModeState::STA_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                currentMode = WiFiModeState::STA_CONNECTED;
                Serial.print("STA Connected. IP: ");
                Serial.println(WiFi.localIP());
            } else if (millis() - connectionStartTime > Config::WIFI_STA_TIMEOUT_MS) {
                handleSTAFailure();
            }
            break;
            
        case WiFiModeState::STA_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("STA Lost. Reconnecting...");
                startSTA();
            }
            break;
            
        default:
            break;
    }
}

void WiFiManager::startSTA() {
    Serial.printf("Connecting to %s...\n", netConfig.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(netConfig.ssid, netConfig.pass);
    currentMode = WiFiModeState::STA_CONNECTING;
    connectionStartTime = millis();
}

void WiFiManager::startAP() {
    Serial.printf("Starting AP: %s\n", Config::DEFAULT_AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(Config::DEFAULT_AP_SSID);
    currentMode = WiFiModeState::AP_ONLY;
}

void WiFiManager::handleSTAFailure() {
    Serial.println("STA Connection Failed. Falling back to AP.");
    WiFi.mode(WIFI_AP_STA); // Keep STA scan possible while in AP mode
    WiFi.softAP(Config::DEFAULT_AP_SSID);
    currentMode = WiFiModeState::STA_FAILED_AP_FALLBACK;
}

void WiFiManager::scanNetworks() {
    if (isScanning) return;
    WiFi.scanNetworks(true); // Async scan
    isScanning = true;
}

String WiFiManager::getScanResultsJSON() {
    int n = WiFi.scanComplete();
    if (n < 0) return "[]";
    
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    
    for (int i = 0; i < n; ++i) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"] = WiFi.SSID(i);
        obj["rssi"] = WiFi.RSSI(i);
        obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String output;
    serializeJson(arr, output);
    WiFi.scanDelete();
    isScanning = false;
    return output;
}

bool WiFiManager::saveCredentials(const char* ssid, const char* pass) {
    strncpy(netConfig.ssid, ssid, sizeof(netConfig.ssid));
    strncpy(netConfig.pass, pass, sizeof(netConfig.pass));
    config.saveNetworkConfig(netConfig);
    
    startSTA();
    return true;
}
