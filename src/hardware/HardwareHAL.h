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
    void setRelayMask(uint8_t mask);
    bool getRelay(uint8_t index) const;
    
    void setDAC(uint8_t channel, float voltage);
    float getDAC(uint8_t channel) const;
    
    void startRamp(uint8_t channel, float targetVoltage, uint32_t durationMs);
    void stopRamp(uint8_t channel);
    void startStepProgram(uint8_t channel, float startV, float targetV, float step, uint32_t stepMs);
    void loop();
    
    float readADC(uint8_t channel);
    
    const DeviceState& getState() const;
    bool hasStateChanged();

private:
    ConfigManager& config;
    DeviceState state;
    bool _changed = false;
    
    // Automation state
    struct RampState {
        bool active = false;
        float startV = 0;
        float targetV = 0;
        uint32_t startTime = 0;
        uint32_t duration = 0;
    } ramps[2];

    struct StepProgram {
        bool active = false;
        uint8_t channel = 0;
        float startV = 0;
        float currentV = 0;
        float targetV = 0;
        float stepSize = 0;
        uint32_t stepDuration = 0;
        uint32_t lastStepTime = 0;
    } prog;
    
    void writeGP8413(uint8_t channel, uint16_t value);
    uint16_t voltageToDAC(uint8_t channel, float voltage);
};
