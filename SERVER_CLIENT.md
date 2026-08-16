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
- server-owned identity/control chain `ServerSessionId -> PlayerId -> ControlRegistry -> EntityId`;
- persistent ship-instance mapping `PlayerId -> ShipInstanceId -> materialized EntityId`;
- authoritative input consumption/acknowledgement;
- replication interest, sparse ship publication и lifecycle rows;
- network admission/detach remote clients.

Headless target не должен зависеть от GLFW/OpenGL/Freetype/WebView/UI/render.

---

## 2. Два штатных режима игры

### A. Local / embedded development mode

Windows PowerShell:

```powershell
cd D:\__elite\work\build
.\EliteGame.exe
```

MSYS2 MinGW64:

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

Сначала сервер, Windows PowerShell:

```powershell
cd D:\__elite\work\build\headless_server
.\EliteServer.exe --listen 127.0.0.1:27351
```

Эквивалент из MSYS2 MinGW64:

```bash
cd /d/__elite/work/build/headless_server
./EliteServer.exe --listen 127.0.0.1:27351
```

Ожидаемый признак успешного старта:

```text
[EliteServer] listening endpoint=127.0.0.1:27351 fixed_step_s=0.02
```

Затем клиент в другом PowerShell:

```powershell
cd D:\__elite\work\build
.\EliteGame.exe --connect 127.0.0.1:27351
```

Либо из MSYS2 MinGW64:

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

## 3. Runtime dependencies и переносимость запуска

`EliteGame` и `EliteServer` должны запускаться из обычного Windows
PowerShell/CMD, а не только из MinGW shell. Build-time environment не является
частью runtime contract.

Для MinGW/Windows после линковки обоих executable CMake рекурсивно сканирует
их PE imports через toolchain `objdump` и копирует найденные non-system DLL из
MinGW/MSYS runtime directories рядом с соответствующим `.exe`. Список не
захардкожен: кроме `libstdc++-6.dll`, `libgcc_s_seh-1.dll` и
`libwinpthread-1.dll` клиент автоматически получает также транзитивные DLL
GLFW/Freetype и других динамических зависимостей, если они реально присутствуют
в import graph. Windows system DLL остаются системными.

Это проверяется отдельным acceptance test, который намеренно запускает оба exe
с `PATH`, очищенным от MSYS2/MinGW:

```bash
bash tests/runtime_standalone/run_mingw64.sh
```

Ожидаемый финал:

```text
[PASS] EliteServer and EliteGame launch without MinGW/MSYS2 on PATH
```

Linux не использует Windows staging branch. Для ELF targets build RPATH
делается относительным (`BUILD_RPATH_USE_ORIGIN`), а системные `libc`,
`libstdc++`, pthread и другие distribution-owned libraries не копируются рядом
с executable. Текущий headless server должен собираться отдельно через
`ELITE_BUILD_CLIENT=OFF`; этот runtime fix не означает, что графический
`EliteGame` уже полностью портирован на Linux — его UI/render/toolchain
boundary проверяется отдельно.

## 4. Особенности authoritative session bootstrap

При TCP admission клиент не выбирает себе корабль.

Server-side последовательность:

1. `NetworkServerHost` принимает TCP connection.
2. `ServerRuntime` выбирает доступный server-owned `PlayerId`; клиент не выбирает `EntityId`/ship authority.
3. `PlayerRegistry` разрешает persistent assignment `PlayerId -> current ShipInstanceId`.
4. `ShipInstanceRegistry` разрешает `ShipInstanceId -> current materialized EntityId`.
5. `ControlRegistry` подтверждает human control binding `PlayerId -> EntityId`.
6. Создаётся transient `ServerSessionId`, и `ServerSessionRegistry` сохраняет `session -> PlayerId`, а не ship/entity identity.
7. Сервер отправляет `SessionWelcome` с `playerId`, `controlledShipInstanceId`, `controlledEntityId`, authoritative `fixedStepSeconds` и fingerprint статического StarAtlas catalog.
8. Сервер отправляет hydrated bootstrap snapshot; `ShipSnapshot.instanceId` сохраняет persistent identity конкретного корабля через replication.
9. Только после успешной синхронизации `GameClient` переходит в `Ready`.

Текущая admission policy всё ещё **временная**: dedicated bootstrap создаёт два
persistent player/ship bindings, чьи materialized корабли находятся примерно в
50 м друг от друга. Свободный `PlayerId` выдаётся сервером по фактическому accept
order — фактически «кто первый подключился, тот получил первый доступный слот».
Это не production identity contract и не повод добавлять client-authoritative
`--player`, `--ship` или `--entity` switch.

Ручной graphical acceptance подтвердил, что два отдельных remote `EliteGame`
одновременно получают разные identities и видят оба корабля в **одном общем
authoritative world**. Следующий слой должен заменить временный bootstrap pool
полноценной server-owned цепочкой `AccountId -> PlayerId -> owned/current
ShipInstanceId -> session/control authority`, с duplicate-login и reconnect/resume
policy. Клиент по-прежнему не должен присылать произвольный `EntityId` для
получения управления.

---

## 5. Multi-process client preflight: WebUI и ожидание сервера

Каждый `EliteGame` process должен владеть собственным локальным WebUI endpoint.
`HtmlUiServer` запускается на port `0`, операционная система выбирает свободный
ephemeral port, а `Application` строит URL WebView из фактически назначенного
порта. Поэтому два клиента на одном PC не делят `localhost:8090` и не могут
случайно открыть WebUI другого процесса. WebView2 user-data folder также изолирован
по PID.

При реальном запуске двух graphical clients был отдельно диагностирован Win32
crash внутри GLFW 3.4: `_glfwPollEventsWin32()` мог получить active HWND другого
`EliteGame` process, прочитать его window property `GLFW` и попытаться разыменовать
foreign-process `_GLFWwindow*`. Внешний pre-check оказался TOCTOU-гонкой. Поэтому
Windows `Window::pollEvents()` использует native `PeekMessageW/TranslateMessage/
DispatchMessageW` event pump и не входит в опасный GLFW post-poll path; GLFW WndProc
по-прежнему получает обычные window/input messages. Это platform compatibility
boundary, а не gameplay/session logic.

Remote client также может стартовать раньше dedicated server. Начальный TCP
`connection refused`/отсутствующий listener не является `Session Failed`:
`RemoteGameSession` остаётся в `WaitingForServer` и периодически повторяет
connect. После первого успешного TCP connection ошибки protocol/catalog/admission
и последующий disconnect остаются fatal до отдельного reconnect/resume stage.

Loading screen отображает `loading.stage.waiting_server` через общий localization
domain (`en/ru/zh-Hans/es/ja`). Presentation анимирован: обычные локализованные
строки печатаются терминальным блочным курсором, после паузы стираются и
повторяются; для Simplified Chinese используется визуальная pinyin-IME имитация:
`zheng zai -> 正在`, `deng dai -> 等待`, `fu wu qi -> 服务器`. Это только WebUI
presentation и не является частью сетевого/session protocol.

---

## 6. Static definitions: локально на каждом endpoint

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

## 7. Что сейчас идёт по сети

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

## 8. Remote debug и текущие ограничения

На M8D remote debug control ещё не является сетевым authoritative protocol.
`RemoteGameSession` использует безопасный no-op debug facade. Debug mutation,
authorization и remote tooling должны проектироваться отдельно от gameplay
transport.

Также пока **не завершены**:

- durable account/character authentication and ownership storage (`AccountId -> PlayerId -> ShipInstanceId`);
- reconnect/resume token handoff and server-owned restoration of the same persistent player identity;
- authenticated/encrypted Internet-facing session layer;
- true multi-system authoritative runtime для игроков одновременно в разных
  materialized systems;
- field-level delta replication.

До появления authentication/encryption текущий TCP listener следует считать
разработческим/LAN transport, а не публичным Internet server.

---

## 9. Автоматическая проверка process boundary

Полный ready harness:

```bash
bash tests/run_all_mingw64.sh
```

В него входят в том числе standalone-runtime и process-boundary gates.

Отдельно проверка запуска без MinGW/MSYS2 в `PATH`:

```bash
bash tests/runtime_standalone/run_mingw64.sh
```

Отдельно process acceptance:

```bash
bash tests/network_process_acceptance/run_mingw64.sh
```

Перед server/process acceptance harness проверяет, не запущен ли уже внешний
`EliteServer`. Если найден, тест немедленно завершается с ошибкой и печатает
PID/path процесса; developer-owned server автоматически не завершается. Это
отдельная диагностическая защита от Windows executable lock/stale-binary.

Сам `EliteServer` дополнительно держит атомарный OS-level single-instance lock
в течение всей жизни authoritative runtime. Второй `EliteServer` на той же
машине должен сообщить о конфликте и завершиться с ненулевым кодом. `EliteGame`
этим правилом сейчас не ограничен: M8E требует одновременно как минимум два
client process.

Этот тест:

1. выбирает свободный localhost TCP port;
2. **сначала** запускает отдельный `EliteGame.exe --self-test-remote-client`;
3. подтверждает, что initial connection refusal не завершил client process;
4. затем запускает отдельный `EliteServer.exe`;
5. ждёт реальный listening state и автоматический client retry;
6. пробует запустить второй `EliteServer` и требует отказа single-instance guard;
7. проверяет bootstrap -> `GameClient::Ready`;
8. отправляет numbered control input и ждёт authoritative acknowledgement;
9. завершает client и проверяет server-side detach;
10. тем самым доказывает client-before-server lifecycle, single-server ownership и обмен через kernel TCP между executable processes.

Test-only flags:

```text
EliteServer.exe --listen HOST:PORT --self-test-one-client
EliteGame.exe --self-test-remote-client HOST:PORT
```

Они предназначены для acceptance harness, а не для обычной игры.

---

## 10. Ручная проверка process/multi-client boundary

После полностью зелёного `tests/run_all_mingw64.sh` отдельно проверить реальный
interactive remote mode.

### Check A — local mode regression

```powershell
cd D:\__elite\work\build
.\EliteGame.exe
```

Проверить:

- normal boot;
- движение/поворот/тягу;
- Newtonian / Assisted;
- NPC motion;
- F9-F12 maps и переходы;
- отсутствие новых presentation regressions.

### Check B — dedicated server + one graphical remote client

PowerShell 1:

```powershell
cd D:\__elite\work\build\headless_server
.\EliteServer.exe --listen 127.0.0.1:27351
```

PowerShell 2:

```powershell
cd D:\__elite\work\build
.\EliteGame.exe --connect 127.0.0.1:27351
```

Проверить тот же gameplay surface, особенно:

- клиент входит в мир без embedded server;
- управление реально отражается authoritative server state;
- NPC продолжают двигаться;
- карты получают dynamic state и не теряют local composition;
- остановка `EliteServer` прекращает authoritative session, а client не
  продолжает изображать живую сетевую authority.

Reconnect того же running client после рестарта сервера **пока не ожидается**:
это задача следующего authorization/reconnect этапа.

### Check C — два graphical client processes

Ручной acceptance на 2026-08-15 уже подтвердил базовый общий-мир сценарий:

- один dedicated `EliteServer`;
- два отдельных graphical `EliteGame --connect` process;
- разные server-owned `PlayerId`, `ShipInstanceId` и materialized `EntityId`;
- оба клиента одновременно живут в одном authoritative world;
- оба клиента видят друг друга примерно в 50 м;
- WebUI/WebView2 state process-local, а второй client process не убивает первый.

Эта проверка **не закрывает** ещё duplicate login, reconnect/resume, durable account
identity и полный disconnect/input-isolation lifecycle. Их надо проверять после
следующего identity/authorization этапа, а не обходить временным client-selected
player/entity CLI switch.

---

## 10.1. M8E.1 — текущий recovery gate: reconnect control epoch и Win32 modal stalls

На ручном two-process прогоне 2026-08-16 подтверждены две независимые проблемы,
которые нельзя диагностировать как один общий «фриз двух клиентов»:

1. **P0 — reconnect control epoch.** Новый `GameClient` начинает
   `ShipControlState::controlTick` с `1`, но серверная
   `FixedStepControlQueue` до исправления переживала disconnect по тому же
   materialized `EntityId`. Bootstrap нового клиента поэтому мог получить старый
   `acknowledgedControlTick`; новые ticks считались `Stale`, local prediction
   кратко показывала поворот, следующий authoritative snapshot откатывал его,
   а нормальное управление начиналось только после догоняния старого номера.
   Session boundary обязан уничтожать старый numbered-input stream, удалять
   pending one-shot commands и neutralize непрерывный `ShipControlState`.

2. **P1 — Win32 non-client modal loop.** Native `PeekMessageW/DispatchMessageW`
   остаётся правильной защитой от GLFW 3.4 foreign-HWND AV, но обычный
   `WM_NCLBUTTONDOWN` с `HTCAPTION` может законно удерживать `DispatchMessageW`
   внутри Windows move/size loop до отпускания мыши. Логи показывали паузы до
   ~2.5 s. Это UI-thread responsiveness issue конкретного процесса, а не
   shared-server/session cross-talk. Исправлять его надо отдельно после P0.

### Обязательный порядок проверки

После любого изменения session/control lifecycle:

```bash
bash tests/architecture_contracts/run_mingw64.sh
bash tests/multiplayer_client_acceptance/run_mingw64.sh
bash tests/network_process_acceptance/run_mingw64.sh
bash tests/run_all_mingw64.sh
```

Затем ручной gate без рестарта `EliteServer`:

1. Подключить `pilot-a`, поуправлять и закрыть только клиент.
2. Повторно подключить `pilot-a` к тому же серверу и сразу дать yaw/thrust.
   Новый stream должен начинаться с fresh epoch без периода rollback/«прорыва».
3. Не закрывая `pilot-a`, запустить `pilot-b`; во время его загрузки вернуть
   foreground `pilot-a` и проверить непрерывное управление.
4. В логах различать `[M8E-CONTROL]` session reset/ACK и
   `[M8E-XPROC][dispatch] msg=0xa1 wparam=0x2` (оконный modal loop).

Только после зелёного automated + manual gate M8E.1 можно продолжать
authorization/resume/persistence.

---

## 11. Следующий этап — authorization / persistent identity foundation

Текущий код уже имеет правильный in-memory backbone:

```text
PlayerId          persistent player/character identity
ShipInstanceId    stable concrete ship identity
ServerSessionId   transient connection/session identity
EntityId          current materialized simulation handle
```

и server-owned chain:

```text
ServerSessionId
    -> PlayerId
    -> PlayerRegistry.currentShipId
    -> ShipInstanceRegistry.materializedEntity
    -> ControlRegistry
    -> EntityId
```

Следующий этап должен сделать **полноценную заготовку production identity layer**,
а не временный dev selector:

1. ввести `AccountId`/authentication boundary отдельно от `PlayerId`;
2. определить persistent `Account -> Player/character` relation;
3. определить ownership/current-ship model отдельно от control authority;
4. запретить две live gameplay sessions для одного `PlayerId`, пока явно не
   введена takeover/reconnect policy;
5. определить reconnect/resume token и server-owned handoff старой identity;
6. сделать persistent ship registry пригодным для кораблей без materialized
   `EntityId`;
7. постепенно вывести `ShipRole::Player/NPC` из роли фундаментального control
   discriminator: human/AI/autopilot/none должны жить в отдельной control axis;
8. после фиксации identity/ownership storage boundary начать реальное население
   вселенной и подключить `Scheduled <-> Coarse <-> Prewarm <-> Active`
   materialization/collapse для торговцев, челноков, барж, пассажирских кораблей,
   пиратов, флотов и alien fleets.
