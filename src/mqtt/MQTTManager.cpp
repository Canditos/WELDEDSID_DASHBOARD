#include "MQTTManager.h"
#include <ArduinoJson.h>

MQTTManager::MQTTManager(ConfigManager& configMgr, HardwareHAL& hal) 
    : config(configMgr), hardware(hal), client(espClient), 
      lastReconnectAttempt(0), lastPublishTime(0), connected(false) {}

void MQTTManager::begin() {
    config.loadNetworkConfig(netConfig);
    if (!netConfig.mqttEnabled) return;
    
    client.setServer(netConfig.mqttHost, netConfig.mqttPort);
    client.setCallback([this](char* t, byte* p, unsigned int l) { this->callback(t, p, l); });
}

void MQTTManager::loop() {
    if (!netConfig.mqttEnabled || WiFi.status() != WL_CONNECTED) return;
    
    connected = client.connected();
    if (!connected) {
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
    Serial.printf("[MQTT] A ligar a %s:%d ...\n", netConfig.mqttHost, netConfig.mqttPort);
    String clientId = String(netConfig.deviceId) + "_" + String(random(0xffff), HEX);
    String statusTopic = "esp32/" + String(netConfig.deviceId) + "/status";
    
    bool ok = (strlen(netConfig.mqttUser) > 0)
        ? client.connect(clientId.c_str(), netConfig.mqttUser, netConfig.mqttPass,
                         statusTopic.c_str(), 1, true, "offline")
        : client.connect(clientId.c_str(), nullptr, nullptr,
                         statusTopic.c_str(), 1, true, "offline");

    if (ok) {
        Serial.printf("[MQTT] Ligado! ClientID: %s\n", clientId.c_str());
        client.publish(statusTopic.c_str(), "online", true);
        String subTopic = "esp32/" + String(netConfig.deviceId) + "/#";
        client.subscribe(subTopic.c_str());
        Serial.printf("[MQTT] Subscrito em: %s\n", subTopic.c_str());
    } else {
        Serial.printf("[MQTT] Falha ligação — state=%d (1=bad proto, 2=bad id, 3=unavail, 4=bad cred, 5=unauth)\n",
                      client.state());
    }
}

void MQTTManager::callback(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload, length);
    
    String base = "esp32/" + String(netConfig.deviceId) + "/";
    
    if (t == base + "command") {
        if (error) return;
        if (doc.containsKey("relay")) {
            int idx = doc["relay"];
            bool state = doc["state"];
            hardware.setRelay(idx, state);
        }
        if (doc.containsKey("dac")) {
            int ch = doc["dac"];
            float v = doc["value"];
            hardware.setDAC(ch, v);
        }
        publishStatus();
    } else if (t == base + "status/req") {
        publishStatus();
    }
}

void MQTTManager::publishStatus() {
    String base = "esp32/" + String(netConfig.deviceId) + "/";
    
    // Relés
    for (int i = 0; i < 8; i++) {
        client.publish((base + "relay/" + String(i) + "/state").c_str(),
                       hardware.getRelay(i) ? "ON" : "OFF");
    }
    
    // DAC — tensão com 2 casas decimais
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", hardware.getDAC(1));
    client.publish((base + "voltage/1").c_str(), buf);
    snprintf(buf, sizeof(buf), "%.2f", hardware.getDAC(2));
    client.publish((base + "voltage/2").c_str(), buf);

    // Sistema
    client.publish((base + "ip").c_str(), WiFi.localIP().toString().c_str());
    snprintf(buf, sizeof(buf), "%u", ESP.getFreeHeap());
    client.publish((base + "heap").c_str(), buf);
    snprintf(buf, sizeof(buf), "%lu", millis() / 1000);
    client.publish((base + "uptime").c_str(), buf);

    Serial.println("[MQTT] Status publicado.");
}
