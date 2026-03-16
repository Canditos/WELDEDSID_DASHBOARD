#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum class WiFiModeState {
    OFF,
    STA_CONNECTING,
    STA_CONNECTED,
    AP_ONLY,
    STA_FAILED_AP_FALLBACK
};

struct DeviceState {
    bool relays[8];
    float dac1_v;
    float dac2_v;
    WiFiModeState wifiMode;
    bool mqttConnected;
    char ipAddress[16];
};

enum class UserRole : uint8_t {
    VIEWER = 0,
    OPERATOR = 1,
    ADMIN = 2
};

struct SecurityUser {
    char username[33];
    char password[64];
    UserRole role;
    bool enabled;
};

struct NetworkConfig {
    char ssid[33];
    char pass[64];
    char deviceId[33];
    char mqttHost[64];
    uint16_t mqttPort;
    char mqttUser[33];
    char mqttPass[64];
    bool mqttEnabled;
};

struct SecurityConfig {
    SecurityUser users[3];
    char otaPassword[64];
};
