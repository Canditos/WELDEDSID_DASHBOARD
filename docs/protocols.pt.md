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
