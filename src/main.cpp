#include <Arduino.h>
#include "config/ConfigManager.h"
#include "hardware/HardwareHAL.h"
#include "wifi/WiFiManager.h"
#include "mqtt/MQTTManager.h"
#include "modbus/ModbusManager.h"
#include "server/AppServer.h"
#include "ota/OTAManager.h"

// Static instances of core modules
ConfigManager configMgr;
HardwareHAL hardware(configMgr);
WiFiManager wifiMgr(configMgr);
MQTTManager mqttMgr(configMgr, hardware);
ModbusManager modbusMgr(hardware);
AppServer appServer(hardware, wifiMgr);
OTAManager otaMgr;

void setup() {
    // 1. Initialize Debug Serial
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n[SYSTEM] Booting IoT Controller Monolith...");

    // 2. Initialize Persistent Configuration
    if (!configMgr.begin()) {
        Serial.println("[CRITICAL] ConfigManager failed to start NVS.");
    }

    // 3. Initialize Hardware Abstract Layer
    // Immediately restores Relay and DAC states from NVS
    hardware.begin();
    Serial.println("[HAL] Hardware initialized and states restored.");

    // 4. Initialize WiFi (STA with AP Fallback)
    wifiMgr.begin();

    // 5. Initialize MQTT
    mqttMgr.begin();

    // 6. Initialize Modbus TCP/IP
    modbusMgr.begin();

    // 7. Initialize Web Dashboard Server (HTTP & WebSocket)
    appServer.begin();

    // 8. Initialize OTA Update Listener
    NetworkConfig currentNet;
    configMgr.loadNetworkConfig(currentNet);
    otaMgr.begin(currentNet.deviceId);
    
    Serial.println("[SYSTEM] All modules initialized. Ready.");
}

void loop() {
    // Execute module-specific background tasks
    wifiMgr.loop();
    mqttMgr.loop();
    modbusMgr.loop();
    appServer.loop();
    otaMgr.loop();
    
    // Yield to allow background ESP32 tasks (WiFi stack, timers, etc)
    yield();
}
