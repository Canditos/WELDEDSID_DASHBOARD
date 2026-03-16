#include "ConfigManager.h"
#include <string.h>
#include "../security/SecurityHelpers.h"

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

void ConfigManager::resetNetworkConfig() {
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");
    prefs.remove("device_id");
    prefs.remove("mqtt_host");
    prefs.remove("mqtt_port");
    prefs.remove("mqtt_user");
    prefs.remove("mqtt_pass");
    prefs.remove("mqtt_en");
}

void ConfigManager::loadSecurityConfig(SecurityConfig& config) {
    SecurityHelpers::applyDefaultSecurityConfig(config);
    prefs.getString("ota_pass", config.otaPassword, sizeof(config.otaPassword));

    bool foundNewFormat = false;
    for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        char key[20];

        snprintf(key, sizeof(key), "user_%u", i);
        if (prefs.isKey(key)) {
            prefs.getString(key, config.users[i].username, sizeof(config.users[i].username));
            foundNewFormat = true;
        }

        snprintf(key, sizeof(key), "pass_%u", i);
        if (prefs.isKey(key)) {
            prefs.getString(key, config.users[i].password, sizeof(config.users[i].password));
            foundNewFormat = true;
        }

        snprintf(key, sizeof(key), "enabled_%u", i);
        if (prefs.isKey(key)) {
            config.users[i].enabled = prefs.getBool(key, config.users[i].enabled);
            foundNewFormat = true;
        }

        snprintf(key, sizeof(key), "role_%u", i);
        if (prefs.isKey(key)) {
            config.users[i].role = static_cast<UserRole>(prefs.getUChar(key, static_cast<uint8_t>(config.users[i].role)));
            foundNewFormat = true;
        }
    }

    if (!foundNewFormat) {
        prefs.getString("admin_user", config.users[0].username, sizeof(config.users[0].username));
        prefs.getString("admin_pass", config.users[0].password, sizeof(config.users[0].password));
        config.users[0].role = UserRole::ADMIN;
        config.users[0].enabled = true;
    }
}

void ConfigManager::saveSecurityConfig(const SecurityConfig& config) {
    for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        char key[20];

        snprintf(key, sizeof(key), "user_%u", i);
        prefs.putString(key, config.users[i].username);

        snprintf(key, sizeof(key), "pass_%u", i);
        prefs.putString(key, config.users[i].password);

        snprintf(key, sizeof(key), "enabled_%u", i);
        prefs.putBool(key, config.users[i].enabled);

        snprintf(key, sizeof(key), "role_%u", i);
        prefs.putUChar(key, static_cast<uint8_t>(config.users[i].role));
    }

    prefs.remove("admin_user");
    prefs.remove("admin_pass");
    prefs.putString("ota_pass", config.otaPassword);
}

void ConfigManager::resetSecurityConfig() {
    for (uint8_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        char key[20];
        snprintf(key, sizeof(key), "user_%u", i);
        prefs.remove(key);
        snprintf(key, sizeof(key), "pass_%u", i);
        prefs.remove(key);
        snprintf(key, sizeof(key), "enabled_%u", i);
        prefs.remove(key);
        snprintf(key, sizeof(key), "role_%u", i);
        prefs.remove(key);
    }

    prefs.remove("admin_user");
    prefs.remove("admin_pass");
    prefs.remove("ota_pass");
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

void ConfigManager::resetHardwareState() {
    prefs.remove("relays_mask");
    prefs.remove("dac1");
    prefs.remove("dac2");
}
