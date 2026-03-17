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
| `step_ramp` | Start the stepwise DAC program |

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
