#pragma once
#include <Preferences.h>
#include "../common/Constants.h"
#include "../common/Types.h"

class ConfigManager {
public:
    ConfigManager();
    bool begin();
    
    void loadNetworkConfig(NetworkConfig& config);
    void saveNetworkConfig(const NetworkConfig& config);

    void loadSecurityConfig(SecurityConfig& config);
    void saveSecurityConfig(const SecurityConfig& config);
    
    void loadHardwareState(DeviceState& state);
    void saveRelayState(uint8_t index, bool state);
    void saveRelayMask(uint8_t mask);
    void saveDACState(uint8_t channel, float voltage);

private:
    Preferences prefs;
};
