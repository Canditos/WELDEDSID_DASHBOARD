# Notas de Protocolos

**Idiomas:** [Portugues](protocols.pt.md) | [English](protocols.md)

Este projeto expõe varias superficies de controlo e observabilidade.

## HTTP / REST

Rotas principais expostas pelo `AppServer`:

| Rota | Metodo | Finalidade |
| --- | --- | --- |
| `/api/ws-auth` | `GET` | Emite token de auth para WebSocket apos Basic Auth |
| `/api/health` | `GET` | Resumo de health de Wi-Fi, MQTT e Modbus |
| `/api/state` | `GET` | Snapshot atual de reles e DAC |
| `/api/wifi/status` | `GET` | Modo Wi-Fi, status, SSID, RSSI e IP |
| `/api/wifi/startScan` | `POST` | Inicia scan Wi-Fi |
| `/api/wifi/scan` | `GET` | Devolve resultados do scan |
| `/api/wifi/save` | `POST` | Guarda credenciais Wi-Fi |
| `/api/security/status` | `GET` | Estado atual da configuracao de seguranca |
| `/api/security/passwords` | `POST` | Atualiza credenciais admin e OTA |
| `/api/system/info` | `GET` | Info basica de runtime |

Todas as rotas acima estao protegidas por Basic Auth.

## WebSocket

Porta: `81`

A dashboard abre um WebSocket e deve autenticar com o token obtido em `/api/ws-auth`.

Comandos atuais:

| Comando | Finalidade |
| --- | --- |
| `auth` | Autenticar cliente WebSocket |
| `relay` | Definir um rele individual |
| `relay_mask` | Definir reles por bitmask |
| `dac` | Definir tensao de um canal DAC |
| `ramp` | Iniciar rampa temporizada de DAC |
| `stop_ramp` | Parar uma rampa ou programa step em execucao |
| `step_ramp` | Iniciar programa stepwise de DAC |

## Modbus TCP

Porta: `502` (slave id `1`)

O enderecamento no firmware e 0-based. Para clientes Modbus que usam 1-based, usa os equivalentes entre parentesis.

### Coils (Reles)

| Funcao | 0-based | 1-based |
| --- | --- | --- |
| Relay 1 (DC1 FB) | 0 | 00001 |
| Relay 2 (DC2 FB) | 1 | 00002 |
| Relay 3 (DC3 FB) | 2 | 00003 |
| Relay 4 (DC4 FB) | 3 | 00004 |
| Relay 5 (DC1 24V) | 4 | 00005 |
| Relay 6 (DC2 24V) | 5 | 00006 |
| Relay 7 (MG1 FB) | 6 | 00007 |
| Relay 8 (MG2 FB) | 7 | 00008 |

### Holding Registers (DAC em mV)

| Funcao | 0-based | 1-based | Unidades |
| --- | --- | --- | --- |
| DAC Canal 1 (TF Voltage) | 0 | 40001 | milivolts |
| DAC Canal 2 (Dispenser Temp) | 1 | 40002 | milivolts |

Notas:
- Escrever num coil ou holding register atualiza o hardware.
- Alteracoes de hardware (via UI/WebSocket) sao sincronizadas de volta para os registos Modbus.

## MQTT

O MQTT e opcional e so arranca quando esta ativado nas definicoes de rede.

### Definicoes do Broker

- Host: configurado via dashboard
- Porta: configurada via dashboard (default `1883`)
- Credenciais: configuradas via dashboard

### Topics

Topico base: `esp32/{deviceId}/`

| Topico | Direcao | Payload |
| --- | --- | --- |
| `esp32/{deviceId}/status` | publish (LWT) | `online` / `offline` |
| `esp32/{deviceId}/command` | subscribe | JSON command (ver abaixo) |
| `esp32/{deviceId}/status/req` | subscribe | qualquer payload dispara publish |
| `esp32/{deviceId}/relay/{i}/state` | publish | `ON` / `OFF` |
| `esp32/{deviceId}/voltage/1` | publish | string float (volts) |
| `esp32/{deviceId}/voltage/2` | publish | string float (volts) |
| `esp32/{deviceId}/ip` | publish | string do IP |

O indice `{i}` dos reles e 0-based (0-7).

### Payloads de Comando

Definir um rele:

```json
{ "relay": 0, "state": true }
```

Definir um canal DAC:

```json
{ "dac": 2, "value": 7.5 }
```
