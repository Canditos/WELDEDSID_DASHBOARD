# Protocol Notes

**Languages:** [English](protocols.md) | [Portugues](protocols.pt.md)

This project currently exposes multiple control and observability surfaces.

## HTTP / REST

Main routes exposed by `AppServer`:

| Route | Method | Purpose |
| --- | --- | --- |
| `/api/ws-auth` | `GET` | Issues a WebSocket auth token after Basic Auth |
| `/api/health` | `GET` | Consolidated health summary for Wi-Fi, MQTT and Modbus |
| `/api/state` | `GET` | Current relay and DAC state snapshot |
| `/api/wifi/status` | `GET` | Wi-Fi mode, status, SSID, RSSI and IP |
| `/api/wifi/startScan` | `POST` | Starts a Wi-Fi scan |
| `/api/wifi/scan` | `GET` | Returns Wi-Fi scan results |
| `/api/wifi/save` | `POST` | Saves Wi-Fi credentials |
| `/api/security/status` | `GET` | Reads current security config status |
| `/api/security/passwords` | `POST` | Updates admin and OTA credentials |
| `/api/system/info` | `GET` | Returns basic runtime info |

All routes above are protected with Basic Auth.

## WebSocket

Port: `81`

The dashboard opens a WebSocket and must authenticate with the token retrieved from `/api/ws-auth`.

Current commands:

| Command | Purpose |
| --- | --- |
| `auth` | Authenticate a WebSocket client |
| `relay` | Set a single relay |
| `relay_mask` | Set relays from bitmask |
| `dac` | Set DAC channel voltage |
| `ramp` | Start a timed DAC ramp |
| `stop_ramp` | Stop a running ramp or step program |
| `step_ramp` | Start the stepwise DAC program |

## Modbus TCP

Port: `502` (slave id `1`)

Addressing in the firmware is 0-based. For Modbus clients that show 1-based addresses, use the 1-based equivalents in parentheses.

### Coils (Relays)

| Function | 0-based | 1-based |
| --- | --- | --- |
| Relay 1 (DC1 FB) | 0 | 00001 |
| Relay 2 (DC2 FB) | 1 | 00002 |
| Relay 3 (DC3 FB) | 2 | 00003 |
| Relay 4 (DC4 FB) | 3 | 00004 |
| Relay 5 (DC1 24V) | 4 | 00005 |
| Relay 6 (DC2 24V) | 5 | 00006 |
| Relay 7 (MG1 FB) | 6 | 00007 |
| Relay 8 (MG2 FB) | 7 | 00008 |

### Holding Registers (DAC in mV)

| Function | 0-based | 1-based | Units |
| --- | --- | --- | --- |
| DAC Channel 1 (TF Voltage) | 0 | 40001 | millivolts |
| DAC Channel 2 (Dispenser Temp) | 1 | 40002 | millivolts |

Notes:
- Writing to a coil or holding register updates the hardware.
- Hardware changes (via UI/WebSocket) are synced back into Modbus registers.

## MQTT

MQTT is optional and only runs when enabled in network settings.

### Broker Settings

- Host: configured via the dashboard
- Port: configured via the dashboard (default `1883`)
- Credentials: configured via the dashboard

### Topics

Base topic: `esp32/{deviceId}/`

| Topic | Direction | Payload |
| --- | --- | --- |
| `esp32/{deviceId}/status` | publish (LWT) | `online` / `offline` |
| `esp32/{deviceId}/command` | subscribe | JSON command (see below) |
| `esp32/{deviceId}/status/req` | subscribe | any payload triggers publish |
| `esp32/{deviceId}/relay/{i}/state` | publish | `ON` / `OFF` |
| `esp32/{deviceId}/voltage/1` | publish | float string (volts) |
| `esp32/{deviceId}/voltage/2` | publish | float string (volts) |
| `esp32/{deviceId}/ip` | publish | IP string |

Relay index `{i}` is 0-based (0-7).

### Command Payloads

Set a relay:

```json
{ "relay": 0, "state": true }
```

Set a DAC channel:

```json
{ "dac": 2, "value": 7.5 }
```

## mDNS (Service Discovery)

When the ESP32 connects to Wi-Fi (STA), it advertises an mDNS hostname.

- URL format: `http://{deviceId}.local`
- If the device id is empty or invalid, it falls back to `http://weldedsid.local`

The server sends:
- `init` payloads for first full state
- `update` payloads for incremental or full changes
- `error` payloads for unauthorized clients

## MQTT

MQTT is optional and depends on persisted network settings.

Observed topic pattern:

```text
esp32/<deviceId>/...
```

Examples:
- `esp32/<deviceId>/status`
- `esp32/<deviceId>/command`
- `esp32/<deviceId>/relay/<index>/state`
- `esp32/<deviceId>/voltage/1`
- `esp32/<deviceId>/voltage/2`
- `esp32/<deviceId>/ip`

## Modbus TCP

The controller exposes:
- relay states as coils
- DAC values as holding registers in millivolts

Current base addresses from `ModbusManager`:

| Type | Start |
| --- | --- |
| Coils | `0` |
| Holding registers | `0` |

## Operational Recommendation

Because HTTP, WebSocket, MQTT, Modbus TCP and OTA can all be active in the same device, deploy the controller on a trusted network segment and change credentials during commissioning.
