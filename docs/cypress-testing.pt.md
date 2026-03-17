# Guia de Testes Cypress

**Idiomas:** [Portugues](cypress-testing.pt.md) | [English](cypress-testing.md)

Este documento explica como o Cypress esta configurado neste projeto, como os testes sao executados, que variaveis afetam a execucao, como funciona o harness de testes e como adaptar os testes existentes ao criar novos cenarios.

## Objetivo

A suite Cypress foi desenhada para validar a dashboard embebida localmente sem depender de um ESP32 real, alteracoes reais de Wi-Fi ou servicos backend vivos.

Em vez de falar com o dispositivo real, a suite:
- serve o frontend real da pasta `data/`
- faz mock de respostas HTTP com `cy.intercept()`
- substitui o `WebSocket` do browser por um mock local
- captura comandos WebSocket enviados em `window.__wsMessages`

Isto torna a suite segura, deterministica e rapida para testes de regressao da UI.

## Onde esta o Cypress

Ficheiros principais:

```text
cypress.config.mjs
cypress/
|-- e2e/
|   |-- auth-and-layout.cy.js
|   |-- dac-controls.cy.js
|   |-- integrated-controls.cy.js
|   |-- relay-controls.cy.js
|   |-- security-and-logs.cy.js
|   `-- wifi-safe-flows.cy.js
`-- support/
    `-- e2e.js
```

Ficheiros de execucao relacionados:

```text
package.json
scripts/serve-data.mjs
```

## Como os testes sao executados

### Servidor de preview local

A dashboard e servida localmente a partir da pasta real `data/` com:

```bash
npm run serve:data
```

Por defeito, serve a app em:

```text
http://127.0.0.1:4173
```

### Abrir o Cypress de forma interativa

```bash
npm run cypress:open
```

### Correr headless

```bash
npm run cypress:run
```

### Fluxo local tipico

Terminal 1:

```bash
npm run serve:data
```

Terminal 2:

```bash
npm run cypress:run
```

## Configuracao

A configuracao principal do Cypress esta em [cypress.config.mjs](../cypress.config.mjs).

Comportamento atual:
- `baseUrl` por defeito e `http://127.0.0.1:4173`
- `specPattern` e `cypress/e2e/**/*.cy.{js,mjs}`
- support file e `cypress/support/e2e.js`
- video desativado
- screenshots em falha ativos
- viewport `1440 x 1100`

## Variaveis de Ambiente

### `CYPRESS_BASE_URL`

A unica variavel de runtime usada diretamente na config e:

```bash
CYPRESS_BASE_URL
```

Substitui o base URL por defeito:

```bash
CYPRESS_BASE_URL=http://127.0.0.1:4173 npm run cypress:run
```

No Windows PowerShell:

```powershell
$env:CYPRESS_BASE_URL='http://127.0.0.1:4173'
npm run cypress:run
```

Na maioria dos casos nao e preciso definir, porque o projeto ja assume o servidor local.

## O Harness de Teste

O harness partilhado esta em [cypress/support/e2e.js](../cypress/support/e2e.js).

Faz 4 coisas importantes:

1. semeia um estado default da dashboard
2. faz mock de endpoints REST com `cy.intercept()`
3. substitui `window.WebSocket` por uma classe mock
4. expõe mensagens WebSocket enviadas em `window.__wsMessages`

### Estado default

A suite arranca de um estado mockado default:

```js
const DEFAULT_STATE = {
  type: "init",
  relays: [true, false, true, false, false, true, false, true],
  v1: 0.0,
  v2: 0.0,
  mqtt: true,
  modbus: true,
  wifiMode: 2
};
```

Isto e util porque a dashboard nao e testada apenas num DOM vazio; e testada contra um primeiro render realista.

### Endpoints REST mockados

O comando partilhado `cy.mountDashboard()` intercepta atualmente:

- `GET /api/ws-auth`
- `GET /api/wifi/status`
- `GET /api/health`
- `GET /api/security/status`
- `POST /api/wifi/startScan`
- `GET /api/wifi/scan`
- `POST /api/security/users`
- `POST /api/wifi/save`

### WebSocket mockado

O WebSocket mock responde a:

- `auth`
- `relay`
- `relay_mask`
- `dac`
- `step_ramp`

Tambem envia payloads `init` ou `update` para o frontend se comportar como se estivesse ligado ao dispositivo real.

### Log de comandos capturados

Cada comando WebSocket enviado pelo frontend e guardado em:

```js
window.__wsMessages
```

Este e o mecanismo principal para validar que a dashboard enviou os comandos esperados.

## Comando Partilhado: `cy.mountDashboard()`

Este comando e a base da maioria dos testes.

Uso tipico:

```js
beforeEach(() => {
  cy.mountDashboard();
});
```

Ele:
- visita a dashboard
- injeta o WebSocket mock antes do codigo da app correr
- faz login via fluxo de auth mockado
- espera as chamadas iniciais de API
- garante que a dashboard principal esta visivel

### Overrides suportados

Podes customizar o estado montado com um objeto de opcoes:

```js
cy.mountDashboard({
  initialState: {
    relays: [false, false, false, false, false, false, false, false],
    v1: 2.1,
    v2: 4.5,
    mqtt: false,
    modbus: true
  },
  wifiStatus: {
    ssid: "",
    status: 6,
    ip: "192.168.4.1",
    rssi: 0,
    mode: 4
  },
  healthStatus: {
    wifiConnected: false,
    wifiMode: 4,
    wifiIp: "192.168.4.1",
    mqttConnected: false,
    modbusHealthy: true,
    heap: 150000
  },
  securityStatus: {
    adminUser: "admin",
    hasOtaPassword: true,
    usingDefaultPassword: false
  },
  wifiScan: [
    { ssid: "Workshop_AP", rssi: -61, secure: false }
  ]
});
```

Estes overrides sao a forma mais simples de criar novos cenarios sem reescrever o harness.

## Estrutura Atual de Testes

### [auth-and-layout.cy.js](../cypress/e2e/auth-and-layout.cy.js)

Foco:
- comportamento de login invalido
- fluxo de login valido
- layout de primeiro render
- estado default semeado
- renderizacao de health/AP fallback

Use este ficheiro como base para testar:
- estados da topbar
- secoes do layout
- estado visual de boot
- visibilidade de conteudo dependente de auth

### [relay-controls.cy.js](../cypress/e2e/relay-controls.cy.js)

Foco:
- toggles de rele individual
- reset de todos os reles
- reset por grupo

Use este ficheiro como base para testar:
- novos grupos de rele
- estados ativos de botoes
- variantes de comando para reles
- novas acoes de operador em controlo de reles

### [dac-controls.cy.js](../cypress/e2e/dac-controls.cy.js)

Foco:
- input numerico de DAC
- reset de DAC
- trigger do programa step

Use este ficheiro como base para testar:
- clamping do DAC
- comportamento do slider
- presets
- fluxos de comando DAC especiais

### [integrated-controls.cy.js](../cypress/e2e/integrated-controls.cy.js)

Foco:
- fluxo misto de operador numa sessao
- reles + DAC + rampa num unico cenario

Use este ficheiro como base para testar:
- sequencias longas de operacao
- interacoes end-to-end "happy path"
- regressões causadas por interacao de varias areas

### [security-and-logs.cy.js](../cypress/e2e/security-and-logs.cy.js)

Foco:
- carregamento do estado de seguranca
- gravacao de utilizadores e OTA
- validacao client-side
- acoes de limpar/exportar logs

Use este ficheiro como base para testar:
- regras de password
- alteracoes de roles/permissoes
- UX de autenticacao
- novas features de logs (filtros/categorias)

### [wifi-safe-flows.cy.js](../cypress/e2e/wifi-safe-flows.cy.js)

Foco:
- scan e lista de redes
- validacao de SSID vazio
- fluxo de save mockado e seguro

Use este ficheiro como base para testar:
- UX de provisionamento Wi-Fi
- fluxos de AP fallback
- validacao em credenciais guardadas

## Como as assercoes sao feitas

A suite usa 3 estilos principais:

### 1. Assercoes DOM

Exemplo:

```js
cy.get("[data-cy='dac-val-2']").should("have.text", "8.5 V");
```

Use para validar:
- valores mostrados
- secoes visiveis
- classes de botoes
- labels de estado

### 2. Assercoes de intercept

Exemplo:

```js
cy.wait("@wifiSave");
```

Use para validar:
- se um request aconteceu
- se um request nao aconteceu
- sequencia de chamadas

### 3. Assercoes de comandos WebSocket

Exemplo:

```js
cy.window().its("__wsMessages").should((messages) => {
  expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2)).to.equal(true);
});
```

Use para validar:
- comandos de rele
- comandos DAC
- trigger do programa step
- corretude do payload

## Estrategia de Selectors

A suite depende sobretudo de atributos `data-cy`.

Exemplos:
- `data-cy='relay-btn-1'`
- `data-cy='dac-val-2'`
- `data-cy='execute-stepwise-btn'`
- `data-cy='status-wifi'`

Recomendacao:
- continuar a usar `data-cy` para selectors estaveis
- nao depender apenas de texto visivel quando existe selector dedicado
- ao adicionar um controlo novo, adicionar tambem `data-cy`

## Como criar novos testes a partir dos existentes

### Padrao 1: Clonar uma spec existente e reduzir o scope

Se quiseres adicionar um teste de rele:

1. partir de [relay-controls.cy.js](../cypress/e2e/relay-controls.cy.js)
2. manter `cy.mountDashboard()`
3. disparar a nova acao
4. afirmar `window.__wsMessages`
5. afirmar o estado relevante no DOM

### Padrao 2: Reutilizar `cy.mountDashboard()` com overrides

Se quiseres simular AP fallback:

1. partir de [auth-and-layout.cy.js](../cypress/e2e/auth-and-layout.cy.js)
2. passar um `wifiStatus` custom
3. passar um `healthStatus` custom
4. validar topbar e chips de estado

Isto evita mexer no harness quando o cenario e apenas um estado inicial diferente.

### Padrao 3: Adicionar um fluxo integrado de operador

Se quiseres testar um workflow realista:

1. partir de [integrated-controls.cy.js](../cypress/e2e/integrated-controls.cy.js)
2. manter um unico `it(...)`
3. simular uma sequencia de acoes
4. validar estado final da UI e lista de comandos

Isto e util para regressao apos redesign da UI.

## Exemplo: adicionar um teste de preset DAC

Estrutura exemplo:

```js
describe("DAC presets", () => {
  beforeEach(() => {
    cy.mountDashboard();
  });

  it("applies the DISPENSER TEMP mid preset", () => {
    cy.get("[data-dac-preset='2'][data-value='5.0']").click();
    cy.get("[data-cy='dac-val-2']").should("have.text", "5.0 V");
    cy.window().its("__wsMessages").should((messages) => {
      expect(messages.some((msg) => msg.cmd === "dac" && msg.channel === 2 && msg.voltage === 5.0)).to.equal(true);
    });
  });
});
```

## Exemplo: adicionar um teste de badge de health

```js
it("shows MQTT as offline while Modbus stays ready", () => {
  cy.mountDashboard({
    healthStatus: {
      wifiConnected: true,
      wifiMode: 2,
      wifiIp: "192.168.0.222",
      mqttConnected: false,
      modbusHealthy: true,
      heap: 150000
    }
  });

  cy.get("[data-cy='status-mqtt']").should("have.class", "offline");
  cy.get("[data-cy='status-modbus']").should("have.class", "online");
});
```

## Boas Praticas para este projeto

- Mantem os testes seguros ao fazer mock de rede e WebSocket.
- Prefere extender inputs de `cy.mountDashboard()` antes de duplicar setup.
- Usa `data-cy` para cada novo controlo interativo.
- Mantem uma preocupacao por spec, exceto quando for um fluxo integrado.
- Em redesigns, atualiza selectors primeiro e depois as assercoes de comportamento.
- Se um teste for instavel por timing, valida o comando e o estado final, nao classes transitorias.

## Limitacoes atuais

- O WebSocket mock cobre apenas os comandos usados nos testes atuais.
- A suite valida o comportamento da dashboard, nao os efeitos reais do firmware/rede.
- O servidor local e estatico e nao emula o stack completo do dispositivo.
- Downloads e screenshots sao gerados localmente e devem ficar ignorados no Git.

## Troubleshooting

### Cypress nao consegue ligar ao base URL

Sintomas:
- aviso `Cannot Connect Base Url`
- testes nao arrancam

Verificar:
1. o servidor local esta a correr com `npm run serve:data`
2. o servidor responde em `http://127.0.0.1:4173`
3. `CYPRESS_BASE_URL` nao aponta para um endereco antigo

### O browser abre um popup nativo de autenticacao

Este e o dialogo flutuante do browser, nao o login da dashboard.

Causa:
- uma rota devolve `WWW-Authenticate` e o browser abre o popup

Estado atual do projeto:
- as rotas foram ajustadas para devolver JSON `401` em vez de disparar o popup
- o fluxo correto e o login overlay da dashboard

Se isto voltar:
1. inspeciona respostas nao autorizadas
2. confirma que devolvem JSON `401`
3. evita `requestAuthentication()` em endpoints da dashboard

### Um teste falha apos redesign da UI

Verificar:
1. mudou algum selector `data-cy`
2. mudou algum texto visivel
3. mudou o timing por animacoes ou estado async
4. o harness deixou de mockar uma rota necessaria

### Uma assercao WebSocket falha

Lembra que o Cypress nao usa socket real.

Verificar:
1. o comando esta implementado em `buildMockWebSocket()`
2. a dashboard envia mesmo esse comando
3. a assercao valida o payload correto em `window.__wsMessages`

### Uma assercao de logs fica instavel

Agora ha filtros e mais estado na UI.

Preferir:
- validar que o texto existe no container
- ou validar apenas entradas visiveis quando um filtro esta ativo com `.log-entry:not(.hidden)`

### Screenshots ou downloads aparecem localmente

Isto e esperado em runs do Cypress:
- `cypress/screenshots/`
- `cypress/downloads/`

Devem ficar ignorados no Git, exceto se quiseres guardar o artefacto.

## Melhorias sugeridas

- Adicionar testes para os novos presets de DAC.
- Adicionar teste de filtros `ALL / AUTH / NET / DAC / RELAY`.
- Adicionar cenario de reconexao WebSocket usando os chips da topbar.
- Adicionar teste visual para layout mobile se a dashboard continuar a evoluir.
