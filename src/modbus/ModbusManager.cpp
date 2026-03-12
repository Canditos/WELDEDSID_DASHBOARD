#include "ModbusManager.h"

ModbusManager::ModbusManager(HardwareHAL& hal) : hardware(hal) {}

void ModbusManager::begin() {
    mb.server(); // Initialize as TCP Slave
    
    // Add Coils for Relays
    for (int i = 0; i < 8; i++) {
        mb.addCoil(COIL_RELAY_START + i, hardware.getRelay(i));
    }
    
    // Add Holding Registers for DACs (in millivolts)
    mb.addHreg(HREG_DAC_START + 0, (uint16_t)(hardware.getDAC(1) * 1000));
    mb.addHreg(HREG_DAC_START + 1, (uint16_t)(hardware.getDAC(2) * 1000));
}

void ModbusManager::loop() {
    mb.task();
    
    // Sync Coils -> Hardware
    for (int i = 0; i < 8; i++) {
        bool coilVal = mb.coil(COIL_RELAY_START + i);
        if (coilVal != hardware.getRelay(i)) {
            hardware.setRelay(i, coilVal);
        }
    }
    
    // Sync Holding Registers -> Hardware
    uint16_t mv1 = mb.Hreg(HREG_DAC_START + 0);
    if (mv1 != (uint16_t)(hardware.getDAC(1) * 1000)) {
        hardware.setDAC(1, mv1 / 1000.0f);
    }
    
    uint16_t mv2 = mb.Hreg(HREG_DAC_START + 1);
    if (mv2 != (uint16_t)(hardware.getDAC(2) * 1000)) {
        hardware.setDAC(2, mv2 / 1000.0f);
    }
    
    // Sync Hardware -> Coils/Registers (External updates like WebSocket)
    for (int i = 0; i < 8; i++) {
        mb.coil(COIL_RELAY_START + i, hardware.getRelay(i));
    }
    mb.Hreg(HREG_DAC_START + 0, (uint16_t)(hardware.getDAC(1) * 1000));
    mb.Hreg(HREG_DAC_START + 1, (uint16_t)(hardware.getDAC(2) * 1000));
}
