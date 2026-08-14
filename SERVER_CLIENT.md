# SERVER / CLIENT RUNTIME AND LAUNCH

Этот файл фиксирует текущий operational contract между `EliteServer` и
`EliteGame`: кто чем владеет, какие режимы запуска считаются штатными, что
передаётся через сеть и как проверять реальную process boundary.

Технические детали и история миграции: `src/game/ARCHITECTURE_STATUS.md`.
Текущий общий статус проекта: `PROJECT_STATE.md`.

---

## 1. Исполняемые процессы

### `EliteGame.exe`

`EliteGame` — графический клиент и presentation runtime.

Он владеет:

- окном, OpenGL/render/UI/WebUI;
- `GameClient`;
- client prediction/reconciliation локально управляемого корабля;
- interpolation/presentation history удалённых dynamic entities;
- endpoint-local static definitions и `StarAtlasDatabase`;
- client-side composition Galaxy/System/Details/Hub presentation.

`EliteGame` **не должен получать прямой доступ к authoritative `GameServer`**.
Даже local single-player проходит через тот же transport/session contract.

### `EliteServer.exe`

`EliteServer` — headless authoritative process.

Он владеет:

- одним authoritative `ServerRuntime` / `GameServer` execution context;
- fixed-step authoritative simulation;
- universe/server timeline authority;
- server sessions и `session -> controlledEntityId` ownership;
- authoritative input consumption/acknowledgement;
- replication interest, sparse ship publication и lifecycle rows;
- network admission/detach remote clients.

Headless target не должен зависеть от GLFW/OpenGL/Freetype/WebView/UI/render.

---

## 2. Два штатных режима игры

### A. Local / embedded development mode

Запуск:

```bash
cd /d/__elite/work/build
./EliteGame.exe
```

В этом режиме `Application` создаёт `LocalGameSession`. Authoritative
`ServerRuntime` существует в том же OS process, но выполняется отдельно на
`ServerWorker` thread. Client и server общаются через explicit local transport
boundary; клиент не получает `GameServer&` и не читает authoritative memory
напрямую.

Этот режим удобен для обычной разработки, render/UI и gameplay regression.
Он **не доказывает process independence**, потому что client и server всё ещё
находятся в одном адресном пространстве.

### B. Dedicated server + remote client

Сначала сервер:

```bash
cd /d/__elite/work/build/headless_server
./EliteServer.exe --listen 127.0.0.1:27351
```

Ожидаемый признак успешного старта:

```text
[EliteServer] listening endpoint=127.0.0.1:27351 fixed_step_s=0.02
```

Затем клиент в другом терминале:

```bash
cd /d/__elite/work/build
./EliteGame.exe --connect 127.0.0.1:27351
```

В remote mode `EliteGame` создаёт `RemoteGameSession`, который владеет только
`TcpClientTransport + GameClient`. Он **не создаёт** `ServerRuntime`,
`ServerWorker` или `GameServer`. Authoritative world продолжает идти в
`EliteServer`, а клиент выполняет только prediction/presentation и отправляет
intent/control messages.

---

## 3. Особенности authoritative session bootstrap

При TCP admission клиент не выбирает себе корабль.

Server-side последовательность:

1. `NetworkServerHost` принимает TCP connection.
2. `ServerRuntime` сам выбирает доступный player entity.
3. Создаётся server-owned `ServerSessionId`.
4. Connection привязывается к `session -> controlledEntityId`.
5. Сервер отправляет `SessionWelcome`.
6. Клиент получает authoritative `fixedStepSeconds`, controlled entity и
   fingerprint статического StarAtlas catalog.
7. Сервер отправляет hydrated bootstrap snapshot.
8. Только после успешной синхронизации `GameClient` переходит в `Ready`.

Текущая admission policy временная: сервер назначает существующий свободный
materialized ship. Persistent account/character/ship ownership будет отдельным
слоем и не должен менять сам transport/session authority contract.

---

## 4. Static definitions: локально на каждом endpoint

Server и client независимо загружают одинаковые immutable/static catalogs.
Обычная replication передаёт instance/runtime state и compact type IDs, а не
полные определения типов кораблей/модулей/мешей.

Важно:

- client не должен наследовать initialized global registries только потому, что
  рядом в том же process уже запускался server;
- object/assembly descriptor registries bootstrap'ятся независимо и one-time;
- StarAtlas загружается независимо на client/server;
- `SessionWelcome` сверяет schema/fingerprint и останавливает synchronization
  при несовместимом catalog вместо тихого рассогласования мира.

Это одна из причин, почему separate-process acceptance обязателен: in-process
режим способен случайно скрыть неправильное ownership глобальных/static данных.

---

## 5. Что сейчас идёт по сети

Текущий production transport — reliable ordered TCP.

Process boundary использует:

- versioned/network-byte-order wire framing;
- explicit control-plane codecs;
- canonical ordered data schema для `SimulationSnapshot` и `MapResponse`;
- schema-blind compression seam;
- bounded payload/write queues и frame sequencing;
- per-session sparse ship replication с hydrated bootstrap/re-entry baseline.

Socket/Asio details находятся ниже `ITransport` / `IServerTransport` и не должны
проникать в gameplay/server authority code.

Field-level delta compression пока намеренно не введён: сначала должны быть
жёстко определены reconnect/delivery/baseline semantics.

---

## 6. Remote debug и текущие ограничения

На M8D remote debug control ещё не является сетевым authoritative protocol.
`RemoteGameSession` использует безопасный no-op debug facade. Debug mutation,
authorization и remote tooling должны проектироваться отдельно от gameplay
transport.

Также пока **не завершены**:

- reconnect/resume прежней session identity;
- persistent account/character/ship ownership;
- authenticated/encrypted Internet-facing session layer;
- true multi-system authoritative runtime для игроков одновременно в разных
  materialized systems;
- field-level delta replication.

До появления authentication/encryption текущий TCP listener следует считать
разработческим/LAN transport, а не публичным Internet server.

---

## 7. Автоматическая проверка process boundary

Полный ready harness:

```bash
bash tests/run_all_mingw64.sh
```

Отдельно process acceptance:

```bash
bash tests/network_process_acceptance/run_mingw64.sh
```

Этот тест:

1. выбирает свободный localhost TCP port;
2. запускает отдельный `EliteServer.exe`;
3. ждёт реальный listening state;
4. запускает отдельный `EliteGame.exe --self-test-remote-client`;
5. проверяет bootstrap -> `GameClient::Ready`;
6. отправляет numbered control input;
7. ждёт authoritative acknowledgement;
8. завершает client и проверяет server-side detach;
9. тем самым доказывает обмен через kernel TCP между двумя executable processes.

Test-only flags:

```text
EliteServer.exe --listen HOST:PORT --self-test-one-client
EliteGame.exe --self-test-remote-client HOST:PORT
```

Они предназначены для acceptance harness, а не для обычной игры.

---

## 8. Ручная проверка после M8D

После полностью зелёного `tests/run_all_mingw64.sh` отдельно проверить реальный
interactive remote mode.

### Check A — local mode regression

```bash
cd /d/__elite/work/build
./EliteGame.exe
```

Проверить:

- normal boot;
- движение/поворот/тягу;
- Newtonian / Assisted;
- NPC motion;
- F9-F12 maps и переходы;
- отсутствие новых presentation regressions.

### Check B — dedicated server + one graphical remote client

Terminal 1:

```bash
cd /d/__elite/work/build/headless_server
./EliteServer.exe --listen 127.0.0.1:27351
```

Terminal 2:

```bash
cd /d/__elite/work/build
./EliteGame.exe --connect 127.0.0.1:27351
```

Проверить тот же gameplay surface, особенно:

- клиент входит в мир без embedded server;
- управление реально отражается authoritative server state;
- NPC продолжают двигаться;
- карты получают dynamic state и не теряют local composition;
- остановка `EliteServer` прекращает authoritative session, а client не
  продолжает изображать живую сетевую authority.

Reconnect того же running client после рестарта сервера **пока не ожидается**:
это задача M8E.

### Check C — подготовка M8E / два client processes

Следующий acceptance должен поднять **один dedicated server и два отдельных
remote `EliteGame` process**, проверить разные server-assigned controlled
entities, независимые input/ack streams и disconnect одного клиента без влияния
на второго.

Перед этим есть известный client-process blocker: `Application` сейчас жёстко
поднимает локальный WebUI server на `localhost:8090`. Два полноценных
`EliteGame.exe` на одном PC будут претендовать на один и тот же port. Для M8E
нужно сначала сделать WebUI port per-process/configurable/ephemeral либо иначе
изолировать этот локальный UI endpoint. Это client tooling/presentation issue,
не gameplay TCP issue.

---

## 9. Следующий сетевой этап — M8E

M8E должен закрыть базовую multiplayer process foundation следующими gates:

1. один `EliteServer`, два настоящих remote client processes;
2. разные server-assigned controlled entities;
3. independent prediction/input/ack streams;
4. общий authoritative world без cross-session leakage;
5. disconnect client A не останавливает client B;
6. reconnect получает новую/явно resumed identity только по server-owned rule;
7. stale disconnected session не может вернуть authority;
8. после этого reconnect/baseline semantics фиксируются regression tests.

После M8E можно считать базовый multiplayer transport/lifecycle достаточно
стабильным и возвращаться к persistent universe:
`Scheduled <-> Coarse <-> Prewarm <-> Active` materialization/collapse.
