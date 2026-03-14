#include "ConfigManager.h"
#include <string.h>

ConfigManager::ConfigManager() {}

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
}

bool ConfigManager::begin() {
    return prefs.begin("iot_ctrl", false);
}

void ConfigManager::loadNetworkConfig(NetworkConfig& config) {
    memset(&config, 0, sizeof(NetworkConfig));
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

void ConfigManager::loadSecurityConfig(SecurityConfig& config) {
    memset(&config, 0, sizeof(SecurityConfig));

    prefs.getString("admin_user", config.adminUser, sizeof(config.adminUser));
    prefs.getString("admin_pass", config.adminPass, sizeof(config.adminPass));
    prefs.getString("ota_pass", config.otaPassword, sizeof(config.otaPassword));

    if (strlen(config.adminUser) == 0) {
        copyCString(config.adminUser, sizeof(config.adminUser), Config::ADMIN_USER);
    }

    if (strlen(config.adminPass) == 0) {
        copyCString(config.adminPass, sizeof(config.adminPass), Config::ADMIN_PASS);
    }
}

void ConfigManager::saveSecurityConfig(const SecurityConfig& config) {
    prefs.putString("admin_user", config.adminUser);
    prefs.putString("admin_pass", config.adminPass);
    prefs.putString("ota_pass", config.otaPassword);
}

void ConfigManager::loadHardwareState(DeviceState& state) {
    uint8_t mask = prefs.getUChar("relays_mask", 0);
    for (uint8_t i = 0; i < Config::RELAY_COUNT; i++) {
        state.relays[i] = (mask >> i) & 0x01;
    }
    state.dac1_v = prefs.getFloat("dac1", Config::DAC1_MIN_V);
    state.dac2_v = prefs.getFloat("dac2", Config::DAC2_MIN_V);
}

void ConfigManager::saveRelayState(uint8_t index, bool state) {
    if (index < Config::RELAY_COUNT) {
        uint8_t mask = prefs.getUChar("relays_mask", 0);
        if (state) mask |= (1 << index);
        else mask &= ~(1 << index);
        prefs.putUChar("relays_mask", mask);
    }
}

void ConfigManager::saveRelayMask(uint8_t mask) {
    uint8_t current = prefs.getUChar("relays_mask", 0);
    if (current == mask) return; // Prevent redundant writes
    prefs.putUChar("relays_mask", mask);
}

void ConfigManager::saveDACState(uint8_t channel, float voltage) {
    if (channel == 1) {
        prefs.putFloat("dac1", voltage);
    } else if (channel == 2) {
        prefs.putFloat("dac2", voltage);
    }
}
