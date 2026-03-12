#include "HardwareHAL.h"

HardwareHAL::HardwareHAL(ConfigManager& configMgr) : config(configMgr) {
    memset(&state, 0, sizeof(DeviceState));
}

void HardwareHAL::begin() {
    // Initialize Relays as INPUT (OFF state for open drain)
    for (uint8_t i = 0; i < Config::RELAY_COUNT; i++) {
        pinMode(Config::RELAY_PINS[i], INPUT);
    }
    
    // Initialize I2C for DAC
    Wire.begin(Config::SDA_PIN, Config::SCL_PIN);
    
    // Restore states from NVS
    config.loadHardwareState(state);
    
    // Apply restored states
    for (uint8_t i = 0; i < Config::RELAY_COUNT; i++) {
        setRelay(i, state.relays[i]);
    }
    setDAC(1, state.dac1_v);
    setDAC(2, state.dac2_v);
}

void HardwareHAL::setRelay(uint8_t index, bool on) {
    if (index >= Config::RELAY_COUNT) return;
    
    state.relays[index] = on;
    if (on) {
        // ON: Drive LOW
        pinMode(Config::RELAY_PINS[index], OUTPUT);
        digitalWrite(Config::RELAY_PINS[index], LOW);
    } else {
        // OFF: High Impedance (Floating)
        pinMode(Config::RELAY_PINS[index], INPUT);
    }
    config.saveRelayState(index, on);
}

bool HardwareHAL::getRelay(uint8_t index) const {
    if (index >= Config::RELAY_COUNT) return false;
    return state.relays[index];
}

void HardwareHAL::setDAC(uint8_t channel, float voltage) {
    // Clamp voltages
    if (channel == 1) {
        if (voltage < Config::DAC1_MIN_V) voltage = Config::DAC1_MIN_V;
        if (voltage > Config::DAC1_MAX_V) voltage = Config::DAC1_MAX_V;
        state.dac1_v = voltage;
    } else if (channel == 2) {
        if (voltage < Config::DAC2_MIN_V) voltage = Config::DAC2_MIN_V;
        if (voltage > Config::DAC2_MAX_V) voltage = Config::DAC2_MAX_V;
        state.dac2_v = voltage;
    } else {
        return;
    }
    
    uint16_t dacVal = voltageToDAC(voltage);
    writeGP8403(channel - 1, dacVal); // GP8403 uses 0-indexed channels internally
    config.saveDACState(channel, voltage);
}

float HardwareHAL::getDAC(uint8_t channel) const {
    return (channel == 1) ? state.dac1_v : (channel == 2 ? state.dac2_v : 0.0f);
}

float HardwareHAL::readADC(uint8_t channel) {
    uint8_t pin = (channel == 1) ? Config::ADC1_PIN : Config::ADC2_PIN;
    int raw = analogRead(pin);
    // Standard ESP32 ADC: 0-4095 for 0-3.3V (approx)
    // For production, a more precise calibration would be needed
    return (raw / 4095.0f) * 3.3f;
}

void HardwareHAL::writeGP8403(uint8_t channel, uint16_t value) {
    // GP8403 Protocol: [Addr] [Reg] [DataL] [DataH]
    // Reg for Output 0: 0x02, Output 1: 0x04
    uint8_t reg = (channel == 0) ? 0x02 : 0x04;
    
    Wire.beginTransmission(Config::DAC_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value & 0xFF);        // Low byte
    Wire.write((value >> 8) & 0xFF); // High byte
    Wire.endTransmission();
}

uint16_t HardwareHAL::voltageToDAC(float voltage) {
    // Internal conversion formula: uint16_t dacValue = (voltage / 10.0) * 4095;
    // Note: GP8403 is 12-bit usually, but check datasheet for specific range.
    // The requirement says (voltage / 10.0) * 4095.
    float val = (voltage / 10.0f) * 4095.0f;
    if (val > 4095) val = 4095;
    if (val < 0) val = 0;
    return (uint16_t)val;
}
