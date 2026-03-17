#pragma once
#include <stdint.h>

namespace Config {
    // WiFi Configuration
    const char* const DEFAULT_AP_SSID = "WELDEDSID";
    const uint32_t WIFI_STA_TIMEOUT_MS = 30000;

    // Device Configuration
    const char* const DEFAULT_DEVICE_ID = "esp32_01";

    // Hardware Configuration - Relays
    const uint8_t RELAY_PINS[] = {23, 32, 33, 19, 18, 5, 17, 16};
    const uint8_t RELAY_COUNT = 8;

    // Hardware Configuration - DAC (GP8413)
    const uint8_t DAC_I2C_ADDR = 0x58;
    const uint8_t SDA_PIN = 21;
    const uint8_t SCL_PIN = 22;

    // DAC Voltage Constraints (GP8413: 0-10V range when module is in 0-10V mode)
    const float DAC1_MIN_V = 0.0f;
    const float DAC1_MAX_V = 10.0f;
    const float DAC2_MIN_V = 0.0f;
    const float DAC2_MAX_V = 10.0f;

    // Hardware Configuration - ADC
    const uint8_t ADC1_PIN = 34;
    const uint8_t ADC2_PIN = 35;

    // Web Server Configuration
    const uint16_t HTTP_PORT = 80;
    const uint16_t WS_PORT = 81;
    const char* const ADMIN_USER = "admin";
    const char* const ADMIN_PASS = "admin123";
    const char* const OPERATOR_USER = "operator";
    const char* const OPERATOR_PASS = "operator123";
    const char* const VIEWER_USER = "viewer";
    const char* const VIEWER_PASS = "viewer123";
    const uint8_t SECURITY_USER_COUNT = 3;

    // MQTT Configuration
    const uint16_t MQTT_DEFAULT_PORT = 1883;
    const uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;
    const uint32_t MQTT_PUBLISH_INTERVAL_MS = 5000;

    // Modbus Configuration
    const uint16_t MODBUS_PORT = 502;
    const uint8_t MODBUS_SLAVE_ID = 1;
}
