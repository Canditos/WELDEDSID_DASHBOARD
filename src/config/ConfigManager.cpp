#include "ConfigManager.h"
#include <string.h>

ConfigManager::ConfigManager() {}

bool ConfigManager::begin() {
    return prefs.begin("iot_ctrl", false);
}

void ConfigManager::loadNetworkConfig(NetworkConfig& config) {
    prefs.getString("wifi_ssid", config.ssid, sizeof(config.ssid));
    prefs.getString("wifi_pass", config.pass, sizeof(config.pass));
    prefs.getString("device_id", config.deviceId, sizeof(config.deviceId));
    
    if (strlen(config.deviceId) == 0) {
        strncpy(config.deviceId, Config::DEFAULT_DEVICE_ID, sizeof(config.deviceId));
    }

    prefs.getString("mqtt_host", config.mqttHost, sizeof(config.mqttHost));
    config.mqttPort = prefs.getUShort("mqtt_port", Config::MQTT_DEFAULT_PORT);
    prefs.getString("mqtt_user", config.mqttUser, sizeof(config.mqttUser));
    prefs.getString("mqtt_pass", config.mqttPass, sizeof(config.mqttPass));
    config.mqttEnabled = prefs.getBool("mqtt_en", false);
}

void ConfigManager::saveNetworkConfig(const NetworkConfig& config) {
    prefs.putString("wifi_ssid", config.ssid);
    prefs.putString("wifi_pass", config.pass);
    prefs.putString("device_id", config.deviceId);
    prefs.putString("mqtt_host", config.mqttHost);
    prefs.putUShort("mqtt_port", config.mqttPort);
    prefs.putString("mqtt_user", config.mqttUser);
    prefs.putString("mqtt_pass", config.mqttPass);
    prefs.putBool("mqtt_en", config.mqttEnabled);
}

void ConfigManager::loadHardwareState(DeviceState& state) {
    for (uint8_t i = 0; i < Config::RELAY_COUNT; i++) {
        char key[10];
        snprintf(key, sizeof(key), "r_%d", i);
        state.relays[i] = prefs.getBool(key, false);
    }
    state.dac1_v = prefs.getFloat("dac1", Config::DAC1_MIN_V);
    state.dac2_v = prefs.getFloat("dac2", Config::DAC2_MIN_V);
}

void ConfigManager::saveRelayState(uint8_t index, bool state) {
    if (index < Config::RELAY_COUNT) {
        char key[10];
        snprintf(key, sizeof(key), "r_%d", index);
        prefs.putBool(key, state);
    }
}

void ConfigManager::saveDACState(uint8_t channel, float voltage) {
    if (channel == 1) {
        prefs.putFloat("dac1", voltage);
    } else if (channel == 2) {
        prefs.putFloat("dac2", voltage);
    }
}
