#include "WiFiManager.h"
#include <ArduinoJson.h>

namespace {
void copyCString(char* dest, size_t destSize, const char* src) {
    if (destSize == 0) return;
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

void resetWiFiRadio(bool keepStoredCredentials = true) {
    WiFi.scanDelete();
    WiFi.disconnect(true, !keepStoredCredentials);
    WiFi.softAPdisconnect(true);
    delay(100);
}
}

WiFiManager::WiFiManager(ConfigManager& configMgr) 
    : config(configMgr), currentMode(WiFiModeState::OFF), isScanning(false) {}

void WiFiManager::begin() {
    WiFi.persistent(false);
    WiFi.setSleep(false);
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
    resetWiFiRadio();
    WiFi.mode(WIFI_STA);
    WiFi.begin(netConfig.ssid, netConfig.pass);
    currentMode = WiFiModeState::STA_CONNECTING;
    connectionStartTime = millis();
}

void WiFiManager::startAP() {
    Serial.printf("Starting AP: %s\n", Config::DEFAULT_AP_SSID);
    resetWiFiRadio();
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(Config::DEFAULT_AP_SSID);
    if (success) {
        Serial.print("AP Started. IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("AP Failed to start!");
    }
    currentMode = WiFiModeState::AP_ONLY;
}

void WiFiManager::handleSTAFailure() {
    Serial.println("STA Connection Failed. Falling back to AP.");
    resetWiFiRadio();
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(Config::DEFAULT_AP_SSID);
    if (success) {
        Serial.print("Fallback AP Started. IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("Fallback AP Failed to start!");
    }
    currentMode = WiFiModeState::STA_FAILED_AP_FALLBACK;
}

void WiFiManager::scanNetworks() {
    if (isScanning) return;
    Serial.println("[WIFI] Starting Async Scan...");
    WiFi.scanNetworks(true); // Async scan
    isScanning = true;
}

String WiFiManager::getScanResultsJSON() {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return "[]";
    if (n == WIFI_SCAN_FAILED || n < 0) {
        isScanning = false;
        return "[]";
    }
    
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
    copyCString(netConfig.ssid, sizeof(netConfig.ssid), ssid);
    copyCString(netConfig.pass, sizeof(netConfig.pass), pass);
    config.saveNetworkConfig(netConfig);
    
    startSTA();
    return true;
}
