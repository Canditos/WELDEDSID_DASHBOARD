#include "ModbusManager.h"

ModbusManager::ModbusManager(HardwareHAL& hal) : hardware(hal) {}

void ModbusManager::begin() {
    mb.server(); // Initialize as TCP Slave
    started = true;
    
    // Add Coils for Relays and init tracking
    for (int i = 0; i < 8; i++) {
        bool s = hardware.getRelay(i);
        mb.addCoil(COIL_RELAY_START + i, s);
        lastRelayStates[i] = s;
    }
    
    // Add Holding Registers for DACs and init tracking
    lastDACStates[0] = (uint16_t)(hardware.getDAC(1) * 1000);
    lastDACStates[1] = (uint16_t)(hardware.getDAC(2) * 1000);
    mb.addHreg(HREG_DAC_START + 0, lastDACStates[0]);
    mb.addHreg(HREG_DAC_START + 1, lastDACStates[1]);
}

void ModbusManager::loop() {
    if (!started) return;
    mb.task();
    
    // 1. Detect and apply external Modbus Master writes
    for (int i = 0; i < 8; i++) {
        bool currentCoil = mb.Coil(COIL_RELAY_START + i);
        if (currentCoil != lastRelayStates[i]) {
            // This was a change in the Modbus register (external write)
            Serial.printf("[MODBUS] Master write Relay %d -> %s\n", i+1, currentCoil ? "ON" : "OFF");
            hardware.setRelay(i, currentCoil);
            lastRelayStates[i] = currentCoil;
        }
    }
    
    // 2. Detect and apply external Modbus Master writes for DAC (in mV)
    for (int i = 0; i < 2; i++) {
        uint16_t currentMBVal = mb.Hreg(HREG_DAC_START + i);
        if (currentMBVal != lastDACStates[i]) {
            // Modbus Master wrote a new value (HReg is in mV)
            Serial.printf("[MODBUS] Master write DAC %d -> %u mV\n", i+1, currentMBVal);
            float voltage = currentMBVal / 1000.0f;
            hardware.setDAC(i + 1, voltage);
            lastDACStates[i] = currentMBVal;
        } else {
            // Sync current hardware state to Modbus register (HReg is in mV)
            uint16_t hwValMV = (uint16_t)(hardware.getDAC(i + 1) * 1000.0f);
            if (hwValMV != currentMBVal) {
                mb.Hreg(HREG_DAC_START + i, hwValMV);
                lastDACStates[i] = hwValMV;
            }
        }
    }
    
    // 3. Sync Hardware changes (e.g. from WebSocket) back to Modbus for Relays
    for (int i = 0; i < 8; i++) {
        bool hwState = hardware.getRelay(i);
        if (hwState != lastRelayStates[i]) {
            mb.Coil(COIL_RELAY_START + i, hwState);
            lastRelayStates[i] = hwState;
        }
    }
}
