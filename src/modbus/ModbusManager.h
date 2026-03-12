#pragma once
#include <ModbusIP_ESP8266.h>
#include "../common/Constants.h"
#include "../hardware/HardwareHAL.h"

class ModbusManager {
public:
    ModbusManager(HardwareHAL& hal);
    void begin();
    void loop();

private:
    HardwareHAL& hardware;
    ModbusIP mb;
    
    static const uint16_t COIL_RELAY_START = 0;
    static const uint16_t HREG_DAC_START = 0;
};
