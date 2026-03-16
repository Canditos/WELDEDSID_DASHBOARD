# Apresentacao do Projeto WELDEDSID_DASHBOARD

## Objetivo

O projeto `WELDEDSID_DASHBOARD` implementa um painel de controlo industrial baseado em `ESP32`, com uma dashboard web embebida no proprio dispositivo. O sistema foi pensado para monitorizar e comandar saidas digitais e canais analogicos de forma simples, visual e segura.

Numa perspetiva de apresentacao, o projeto demonstra:
- controlo remoto de reles em tempo real
- ajuste de tensoes analogicas por DAC
- monitorizacao de estados de rede e servicos
- configuracao de Wi-Fi e seguranca a partir da dashboard
- testes automatizados da interface com Cypress

## Arquitetura Resumida

O sistema esta dividido em duas grandes partes:

1. Firmware embarcado no `ESP32`
   - inicializa hardware, rede, servidor web, WebSocket, MQTT, Modbus e OTA
   - guarda configuracoes em memoria persistente `NVS`
   - executa os comandos recebidos pela dashboard

2. Dashboard web embebida
   - servida diretamente pelo ESP32 a partir de `SPIFFS`
   - permite operar reles, DACs, configuracao Wi-Fi e seguranca
   - recebe atualizacoes em tempo real via `WebSocket`

## Funcionalidades Principais

### 1. Controlo de Reles

O sistema disponibiliza `8 reles`, organizados em grupos funcionais:
- `Group 1`: Feedback Relay
- `Group 2`: 24V DC
- `Group 3`: Main

A dashboard permite:
- ligar e desligar cada rele individualmente
- executar reset por grupo
- executar reset global de todos os reles
- acompanhar o estado visual dos reles em tempo real

### 2. Controlo Analogico por DAC

O projeto inclui `2 canais DAC`:
- `TF Voltage` com gama de `2.0V a 3.0V`
- `Dispenser Temp` com gama de `4.0V a 9.0V`

A dashboard permite:
- ajustar o valor por slider
- ajustar o valor por campo numerico
- usar valores predefinidos `DEFAULT`, `MID` e `MAX`
- iniciar o modo `Dispenser Temp STEP`

### 3. Estado Operacional

No topo da dashboard sao apresentados indicadores operacionais como:
- estado do Wi-Fi
- estado do MQTT
- estado do Modbus
- estado do WebSocket
- IP atual do dispositivo

Isto facilita a leitura do estado geral do sistema em contexto de operacao e demonstracao.

### 4. Configuracao e Seguranca

O projeto inclui:
- configuracao de rede Wi-Fi
- diferentes perfis de utilizador
- password OTA persistente
- backup, importacao e reset de configuracoes

Estas funcionalidades tornam a solucao mais proxima de um produto real e nao apenas de um prototipo tecnico.

### 5. Logs e Diagnostico

A dashboard inclui uma area de logs com:
- registo de eventos importantes
- filtros por categoria
- paginacao para manter a interface legivel
- exportacao e limpeza de logs

## Tecnologias Utilizadas

- `ESP32`
- `PlatformIO`
- `Arduino Framework`
- `SPIFFS`
- `ESPAsyncWebServer`
- `WebSocket`
- `MQTT`
- `Modbus TCP`
- `OTA`
- `HTML`, `CSS`, `JavaScript`
- `Cypress`
- `Playwright`

## Estrutura do Projeto

- [src](../src)
  Firmware principal e modulos do sistema
- [data](../data)
  Dashboard embebida no ESP32
- [cypress](../cypress)
  Testes automatizados da interface
- [docs](../docs)
  Documentacao tecnica e de apresentacao

## Valor do Projeto

Este projeto mostra a integracao entre hardware, firmware, interface web e testes automatizados. Em contexto profissional, demonstra competencias em:
- sistemas embebidos
- redes e comunicacoes
- desenvolvimento frontend
- automatizacao de testes
- organizacao e documentacao de projeto

## Demonstracao com Cypress

Foi preparada uma spec de demonstracao em:
- [presentation-demo.cy.js](../cypress/e2e/presentation-demo.cy.js)

Esta demonstracao:
- usa `mocks` para nao alterar o Wi-Fi real
- faz login automaticamente
- mostra estados do sistema
- comuta reles
- altera DACs
- executa o `Dispenser Temp STEP`
- navega pelos logs com pausas visiveis
- apresenta passos numerados no runner do Cypress para ser mais facil acompanhar ao vivo

### Como correr a demonstracao

1. Arrancar o servidor local da dashboard:

```powershell
npm run serve:data
```

2. Correr a demo em modo visual:

```powershell
npm run cypress:demo
```

### O que se ve durante a demo

- login no sistema
- dashboard operacional
- acao em reles
- acao em DACs
- execucao do modo step
- confirmacao visual nos logs

## Nota Final para Apresentacao

Para apresentar este projeto no trabalho, o ideal e estruturar a explicacao assim:

1. Problema que o sistema resolve
2. Arquitetura geral `ESP32 + dashboard web`
3. Principais funcionalidades
4. Seguranca e configuracao
5. Testes Cypress
6. Demonstracao ao vivo

Desta forma, a apresentacao fica equilibrada entre visao funcional, valor tecnico e prova pratica.
