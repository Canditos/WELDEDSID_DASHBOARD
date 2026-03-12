#pragma once
#include <Wire.h>
#include "../common/Constants.h"
#include "../common/Types.h"
#include "../config/ConfigManager.h"

class HardwareHAL {
public:
    HardwareHAL(ConfigManager& configMgr);
    void begin();
    
    void setRelay(uint8_t index, bool state);
    bool getRelay(uint8_t index) const;
    
    void setDAC(uint8_t channel, float voltage);
    float getDAC(uint8_t channel) const;
    
    float readADC(uint8_t channel);
    
    const DeviceState& getState() const { return state; }

private:
    ConfigManager& config;
    DeviceState state;
    
    void writeGP8403(uint8_t channel, uint16_t value);
    uint16_t voltageToDAC(float voltage);
};
