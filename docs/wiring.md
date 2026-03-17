# Wiring Notes

This document summarizes the current pin allocation exposed by the firmware.

## Visual GPIO Map

```mermaid
flowchart TB
    ESP["ESP32 DevKit"]
    ESP --> R1["GPIO23 -> R1 / DC1 FB"]
    ESP --> R2["GPIO32 -> R2 / DC2 FB"]
    ESP --> R3["GPIO33 -> R3 / DC3 FB"]
    ESP --> R4["GPIO19 -> R4 / DC4 FB"]
    ESP --> R5["GPIO18 -> R5 / DC1 24V"]
    ESP --> R6["GPIO5 -> R6 / DC2 24V"]
    ESP --> R7["GPIO17 -> R7 / MG1 FB"]
    ESP --> R8["GPIO16 -> R8 / MG2 FB"]
    ESP --> I2C["I2C GP8413"]
    I2C --> SDA["GPIO21 -> SDA"]
    I2C --> SCL["GPIO22 -> SCL"]
    ESP --> ADC1["GPIO34 -> ADC1"]
    ESP --> ADC2["GPIO35 -> ADC2"]
```

## Relay Outputs

| Relay | Label | GPIO |
| --- | --- | --- |
| R1 | DC1 FB | GPIO23 |
| R2 | DC2 FB | GPIO32 |
| R3 | DC3 FB | GPIO33 |
| R4 | DC4 FB | GPIO19 |
| R5 | DC1 24V | GPIO18 |
| R6 | DC2 24V | GPIO5 |
| R7 | MG1 FB | GPIO17 |
| R8 | MG2 FB | GPIO16 |

## DAC and Analog

| Function | Value |
| --- | --- |
| DAC device | GP8413 |
| I2C address | `0x58` |
| SDA | GPIO21 |
| SCL | GPIO22 |
| ADC1 | GPIO34 |
| ADC2 | GPIO35 |

## Network and Service Ports

| Service | Port |
| --- | --- |
| HTTP dashboard | `80` |
| WebSocket | `81` |
| MQTT | `1883` |
| Modbus TCP | `502` |

## Commissioning Checklist

- Confirm relay driver polarity before energizing outputs.
- Confirm the GP8413 supply and reference path before writing voltages.
- Validate DAC output ranges with a meter on first boot.
- Change default credentials before connecting the controller to a wider LAN.
- Upload both firmware and SPIFFS content during commissioning.
