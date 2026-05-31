#include "HardwareHAL.h"

HardwareHAL::HardwareHAL(ConfigManager& configMgr) : config(configMgr) {
    memset(&state, 0, sizeof(DeviceState));
    memset(&prog, 0, sizeof(prog));
    memset(&ramps, 0, sizeof(ramps));
}

void HardwareHAL::begin() {
    // Initialize Relays as OUTPUT and set to HIGH (OFF for active-low)
    for (uint8_t i = 0; i < Config::RELAY_COUNT; i++) {
        pinMode(Config::RELAY_PINS[i], OUTPUT);
        digitalWrite(Config::RELAY_PINS[i], HIGH);
    }
    
    // Initialize I2C for DAC
    Wire.begin(Config::SDA_PIN, Config::SCL_PIN);

    // GP8403 requires explicit output range configuration on every boot.
    // Register 0x01, value 0x11 = 0–10 V on both channels.
    // Without this the chip defaults to 0–5 V and channel 2 (max 9 V) is unusable.
    Wire.beginTransmission(Config::DAC_I2C_ADDR);
    Wire.write(0x01); // range config register
    Wire.write(0x11); // both channels: 0–10 V
    uint8_t i2cErr = Wire.endTransmission();
    if (i2cErr == 0) {
        Serial.println("[HAL] GP8403 range set to 0-10V on both channels.");
    } else {
        Serial.printf("[HAL] GP8403 range config failed (I2C error %u). Check wiring.\n", i2cErr);
    }

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
    uint8_t pin = Config::RELAY_PINS[index];
    
    if (on) {
        // ON: Drive LOW (Active-Low relay)
        digitalWrite(pin, LOW);
        Serial.printf("[HAL] Relay %d (Pin %d) -> ON (LOW)\n", index + 1, pin);
    } else {
        // OFF: Drive HIGH
        digitalWrite(pin, HIGH);
        Serial.printf("[HAL] Relay %d (Pin %d) -> OFF (HIGH)\n", index + 1, pin);
    }
    
    // Save full mask to ensure consistency
    uint8_t mask = 0;
    for (int i = 0; i < Config::RELAY_COUNT; i++) {
        if (state.relays[i]) mask |= (1 << i);
    }
    config.saveRelayMask(mask);
    _changed = true;
}

void HardwareHAL::setRelayMask(uint8_t mask) {
    Serial.printf("[HAL] Applying Relay Mask: 0x%02X\n", mask);
    for (uint8_t i = 0; i < Config::RELAY_COUNT; i++) {
        bool on = (mask >> i) & 0x01;
        state.relays[i] = on;
        digitalWrite(Config::RELAY_PINS[i], on ? LOW : HIGH);
    }
    _changed = true;
    config.saveRelayMask(mask);
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
    
    // Only save to NVS if no ramp is active for this channel to prevent NVS wear/lag
    if (!ramps[channel-1].active) {
        config.saveDACState(channel, voltage);
    }
    _changed = true;
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

const DeviceState& HardwareHAL::getState() const { return state; }
bool HardwareHAL::hasStateChanged() { if(_changed) { _changed = false; return true; } return false; }

void HardwareHAL::writeGP8403(uint8_t channel, uint16_t value) {
    // GP8403 register map: Output 0 → 0x02, Output 1 → 0x04
    // Data format: 12-bit value left-aligned in a 16-bit word (bits [15:4]).
    // Byte order: high byte first, then low byte.
    uint8_t reg = (channel == 0) ? 0x02 : 0x04;
    uint16_t raw = value << 4; // left-align 12-bit value into 16-bit word

    Wire.beginTransmission(Config::DAC_I2C_ADDR);
    Wire.write(reg);
    Wire.write((raw >> 8) & 0xFF); // high byte first
    Wire.write(raw & 0xFF);        // low byte second
    Wire.endTransmission();
}

uint16_t HardwareHAL::voltageToDAC(float voltage) {
    // GP8403 in 0–10 V mode: 12-bit resolution → 4095 = 10.0 V
    float val = (voltage / 10.0f) * 4095.0f;
    if (val > 4095.0f) val = 4095.0f;
    if (val < 0.0f) val = 0.0f;
    return static_cast<uint16_t>(val);
}

void HardwareHAL::startRamp(uint8_t channel, float targetVoltage, uint32_t durationMs) {
    if (channel < 1 || channel > 2) return;
    int idx = channel - 1;
    
    // Stop any active program if a manual ramp is requested
    prog.active = false;
    
    ramps[idx].active = true;
    ramps[idx].startV = getDAC(channel);
    ramps[idx].targetV = targetVoltage;
    ramps[idx].startTime = millis();
    ramps[idx].duration = durationMs;
}

void HardwareHAL::startStepProgram(uint8_t channel, float startV, float targetV, float step, uint32_t stepMs) {
    if (channel < 1 || channel > 2) return;

    ramps[channel - 1].active = false;
    prog.active = true;
    prog.channel = channel;
    prog.startV = startV;
    prog.currentV = startV;
    prog.targetV = targetV;
    prog.stepSize = step;
    prog.stepDuration = stepMs;
    prog.lastStepTime = millis();
    
    // Set initial voltage
    setDAC(channel, startV);
    Serial.printf("[HAL] AutoProgram Start: Ch%d @ %.1fV\n", channel, startV);
}

void HardwareHAL::loop() {
    uint32_t now = millis();
    
    // 1. Handle Smooth Ramps
    for (int i = 0; i < 2; i++) {
        if (!ramps[i].active) continue;
        
        uint32_t elapsed = now - ramps[i].startTime;
        if (elapsed >= ramps[i].duration) {
            setDAC(i + 1, ramps[i].targetV);
            ramps[i].active = false;
            Serial.printf("[HAL] Ramp Ch%d Finished at %.1fV\n", i+1, ramps[i].targetV);
        } else {
            float progress = (float)elapsed / ramps[i].duration;
            float currentV = ramps[i].startV + progress * (ramps[i].targetV - ramps[i].startV);
            setDAC(i + 1, currentV);
        }
    }

    // 2. Handle Stepwise Program
    if (prog.active) {
        if (now - prog.lastStepTime >= prog.stepDuration) {
            prog.lastStepTime = now;
            prog.currentV += prog.stepSize;
            
            if (prog.currentV > prog.targetV) {
                Serial.printf("[HAL] AutoProgram Finished. Resetting to %.1fV\n", prog.startV);
                setDAC(prog.channel, prog.startV);
                prog.active = false;
                _changed = true;
            } else {
                Serial.printf("[HAL] AutoProgram Step: %.1fV\n", prog.currentV);
                setDAC(prog.channel, prog.currentV);
                _changed = true;
            }
        }
    }
}
