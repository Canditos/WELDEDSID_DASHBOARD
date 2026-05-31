#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config/ConfigManager.h"
#include "hardware/HardwareHAL.h"
#include "wifi/WiFiManager.h"
#include "mqtt/MQTTManager.h"
#include "modbus/ModbusManager.h"
#include "server/AppServer.h"
#include "ota/OTAManager.h"

#define WDT_TIMEOUT_S 30  // reset automático se loop travar mais de 30s

// Static instances of core modules
ConfigManager configMgr;
HardwareHAL hardware(configMgr);
WiFiManager wifiMgr(configMgr);
MQTTManager mqttMgr(configMgr, hardware);
ModbusManager modbusMgr(hardware);
AppServer appServer(configMgr, hardware, wifiMgr, mqttMgr, modbusMgr);
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

    // 4. Initialize WiFi
    Serial.println("[SYSTEM] Starting WiFi...");
    wifiMgr.begin();
    Serial.println("[SYSTEM] WiFi module initialized.");

    // 5. Initialize MQTT
    Serial.println("[SYSTEM] Starting MQTT...");
    mqttMgr.begin();
    Serial.println("[SYSTEM] MQTT module initialized.");

    // 6. Initialize Modbus TCP/IP
    Serial.println("[SYSTEM] Starting Modbus...");
    modbusMgr.begin();
    Serial.println("[SYSTEM] Modbus module initialized.");

    // 7. Initialize Web Dashboard Server
    Serial.println("[SYSTEM] Starting AppServer...");
    appServer.begin();
    Serial.println("[SYSTEM] AppServer initialized.");

    // 8. Initialize OTA Update Listener
    Serial.println("[SYSTEM] Starting OTA...");
    NetworkConfig currentNet;
    SecurityConfig securityConfig;
    configMgr.loadNetworkConfig(currentNet);
    configMgr.loadSecurityConfig(securityConfig);
    otaMgr.begin(currentNet.deviceId, 
                 strlen(securityConfig.otaPassword) > 0 
                     ? securityConfig.otaPassword 
                     : Config::DEFAULT_OTA_PASS);
    Serial.println("[SYSTEM] OTA initialized.");
    
    Serial.println("[SYSTEM] ALL MODULES INITIALIZED. Setup finished.");

    // 9. Watchdog Timer — reset automático se loop() travar > 30s
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);
    Serial.printf("[WDT] Watchdog iniciado — timeout %ds.\n", WDT_TIMEOUT_S);
}

void loop() {
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        lastHeartbeat = millis();
        Serial.printf("[HEARTBEAT] Uptime: %lu ms | Free Heap: %u\n", millis(), ESP.getFreeHeap());
    }

    // Execute module-specific background tasks
    hardware.loop();
    wifiMgr.loop();
    mqttMgr.loop();
    modbusMgr.loop();
    appServer.loop();
    otaMgr.loop();

    esp_task_wdt_reset(); // alimentar watchdog — prova que o loop está vivo
    yield();
}
