#include "MQTTManager.h"
#include <ArduinoJson.h>

MQTTManager::MQTTManager(ConfigManager& configMgr, HardwareHAL& hal) 
    : config(configMgr), hardware(hal), client(espClient), 
      lastReconnectAttempt(0), lastPublishTime(0) {}

void MQTTManager::begin() {
    config.loadNetworkConfig(netConfig);
    if (!netConfig.mqttEnabled) return;
    
    client.setServer(netConfig.mqttHost, netConfig.mqttPort);
    client.setCallback([this](char* t, byte* p, unsigned int l) { this->callback(t, p, l); });
}

void MQTTManager::loop() {
    if (!netConfig.mqttEnabled || WiFi.status() != WL_CONNECTED) return;
    
    if (!client.connected()) {
        uint32_t now = millis();
        if (now - lastReconnectAttempt > Config::MQTT_RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = now;
            reconnect();
        }
    } else {
        client.loop();
        
        uint32_t now = millis();
        if (now - lastPublishTime > Config::MQTT_PUBLISH_INTERVAL_MS) {
            lastPublishTime = now;
            publishStatus();
        }
    }
}

void MQTTManager::reconnect() {
    Serial.println("Attempting MQTT connection...");
    String clientId = String(netConfig.deviceId) + "_" + String(random(0xffff), HEX);
    String statusTopic = "esp32/" + String(netConfig.deviceId) + "/status";
    
    if (client.connect(clientId.c_str(), netConfig.mqttUser, netConfig.mqttPass, statusTopic.c_str(), 1, true, "offline")) {
        Serial.println("MQTT Connected");
        client.publish(statusTopic.c_str(), "online", true);
        
        // Subscribe to control topics
        String subTopic = "esp32/" + String(netConfig.deviceId) + "/#";
        client.subscribe(subTopic.c_str());
    }
}

void MQTTManager::callback(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    char msg[64];
    uint16_t len = (length < 63) ? length : 63;
    memcpy(msg, payload, len);
    msg[len] = '\0';
    String p = String(msg);
    
    String base = "esp32/" + String(netConfig.deviceId) + "/";
    
    if (t.startsWith(base + "relay/")) {
        int idx = t.substring((base + "relay/").length(), t.lastIndexOf("/")).toInt();
        if (t.endsWith("/set")) {
            bool state = (p == "ON" || p == "1" || p == "true");
            hardware.setRelay(idx, state);
        }
    } else if (t == base + "output/set") {
        DynamicJsonDocument doc(256);
        deserializeJson(doc, p);
        if (doc.containsKey("v1")) hardware.setDAC(1, doc["v1"]);
        if (doc.containsKey("v2")) hardware.setDAC(2, doc["v2"]);
    }
}

void MQTTManager::publishStatus() {
    String base = "esp32/" + String(netConfig.deviceId) + "/";
    
    for (int i = 0; i < 8; i++) {
        client.publish((base + "relay/" + String(i) + "/state").c_str(), hardware.getRelay(i) ? "ON" : "OFF");
    }
    
    client.publish((base + "voltage/1").c_str(), String(hardware.getDAC(1)).c_str());
    client.publish((base + "voltage/2").c_str(), String(hardware.getDAC(2)).c_str());
    client.publish((base + "ip").c_str(), WiFi.localIP().toString().c_str());
}
