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

### Канонические build/runtime каталоги

Для каждой постоянной исполняемой сущности проекта существует **один** канонический build/runtime path. Этот путь используется в документации, ручных запусках и как источник production-development binary для runtime acceptance. Нельзя оставлять параллельные долговечные каталоги по смыслу `server2`, `new_server`, `ready_server`, `final_server`, `latest_server` и т. п.

На текущем Windows/MSYS2 этапе канонические ручные пути считаются:

- `EliteGame`: `build/EliteGame.exe`;
- dedicated `EliteServer`: `build/headless_server/EliteServer.exe`.

Process/runtime acceptance **не имеет альтернативных runtime binaries**: он использует те же `build/EliteGame.exe` и `build/headless_server/EliteServer.exe`, что и ручной запуск. Отдельные compile-only harness builds разрешены только под `build/tests/<suite>/`; test logs живут под `build/test-logs/`. Эти scratch directories никогда не являются источником `EliteGame.exe`/`EliteServer.exe`.

Обычный client configure имеет `ELITE_BUILD_SERVER=OFF` по умолчанию, поэтому `build/EliteServer.exe` больше не должен появляться. Канонический dedicated server строится только явной server-конфигурацией в `build/headless_server`.

Перед ручным acceptance после изменения source выполнить из корня репозитория:

```bash
bash build_mingw64.sh
```

Этот entry point удаляет известные legacy generated directories/files, собирает оба канонических targets и отказывает, если под `build/` найден второй `EliteGame.exe` или `EliteServer.exe`. Для cleanup без сборки есть `bash clean_build_layout_mingw64.sh`. Acceptance считается недействительным, если canonical build не был обновлён после изменения соответствующего source.

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

Сначала из Git Bash в корне репозитория обновить канонические binaries:

```bash
bash build_mingw64.sh
```

Затем сервер, Windows PowerShell:

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
.\EliteGame.exe --profile pilot-a --connect 127.0.0.1:27351
```

Либо из MSYS2 MinGW64:

```bash
cd /d/__elite/work/build
./EliteGame.exe --profile pilot-a --connect 127.0.0.1:27351
```

В remote mode `EliteGame` создаёт `RemoteGameSession`, который владеет только
`TcpClientTransport + GameClient`. Он **не создаёт** `ServerRuntime`,
`ServerWorker` или `GameServer`. Authoritative world продолжает идти в
`EliteServer`, а клиент выполняет только prediction/presentation и отправляет
intent/control messages. `--connect` использует выбранный `--profile` как AccountHandle/local credential-slot key и сначала выполняет `SIGN IN`. На fresh server неизвестный handle будет отклонён и UI вернётся в authorization form; для первичной регистрации нужно нажать `REGISTER`, после чего последующие подключения к тому же server runtime используют `SIGN IN`.


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

## 4. Authentication и authoritative session bootstrap

Remote connection теперь имеет явный authorization step. `pilot-a` / `pilot-b` — это **stable AccountHandle**, одновременно используемый как имя local OS credential slot. Handle передаётся серверу как login identifier, но никогда не является `PlayerId` и не даёт права выбрать корабль. В credential slot хранится opaque bearer token; gameplay authority IDs остаются только server-owned.

AccountHandle имеет единый shared контракт (см. `src/game/identity/AccountHandle.h`): 3–24 символа, только lowercase ASCII `a-z`, цифры, `_`, `-`, первый символ — буква или цифра. Это именно технический login identifier. Будущее отображаемое имя игрока отделено от login и сможет использовать полный Unicode.

Multiplayer form содержит:

```text
SERVER ADDRESS
ACCOUNT HANDLE
<локализованные правила ввода>

SIGN IN
REGISTER
BACK
```

`SIGN IN` загружает уже существующий local credential slot и отправляет `SessionHello{accountHandle, authToken, SignIn}`. Если handle неизвестен серверу, admission возвращает typed `SessionReject::UnknownAccount`; если handle существует, но bearer token не совпадает — `InvalidCredential`. **Никакого implicit enrollment больше нет**.

`REGISTER` является единственным first-contact path: local credential slot может быть создан явно, сервер проверяет уникальность AccountHandle, хэширует bearer token, создаёт `AccountId` и связывает его с server-owned `PlayerId`. Повторная регистрация занятого handle с другим credential получает `AccountHandleTaken`. Клиент не передаёт и не выбирает `AccountId`, `PlayerId`, `ShipInstanceId` или `EntityId`.

После успешного credential resolution server-side последовательность такая:

1. `NetworkServerHost` принимает TCP connection и ограничивает pending authentication count.
2. Connection обязан прислать `SessionHello` до authentication deadline; иначе server disconnect.
3. `ServerRuntime` проверяет AccountHandle, хэширует token и resolve'ит пару `handle + digest`; только при `Register` неизвестный handle может создать binding к свободной authoritative bootstrap player identity.
4. Duplicate live session для того же `PlayerId` получает typed `SessionReject::AlreadyActive`; rejected transport получает короткий flush grace, чтобы причина гарантированно дошла до клиента до закрытия TCP.
5. `PlayerRegistry` разрешает `PlayerId -> current ShipInstanceId`; `ShipInstanceRegistry` — `ShipInstanceId -> materialized EntityId`; `ControlRegistry` подтверждает human control binding.
6. Создаётся transient `ServerSessionId`; `ServerSessionRegistry` хранит `session -> PlayerId`.
7. `SessionWelcome` возвращает server-assigned `playerId`, `controlledShipInstanceId`, `controlledEntityId`, authoritative `fixedStepSeconds` и StarAtlas fingerprint.
8. Hydrated bootstrap snapshot завершает synchronization; только затем `GameClient` становится `Ready`.

При любом remote auth/session reject loading page не является terminal state: `Application` возвращает пользователя на ту же multiplayer authorization form, показывает server-provided reason и оставляет введённый handle доступным для исправления/повтора. Это также относится к переходу **Local -> Multiplayer**: local session не наследуется как remote identity и перед remote attach пользователь явно выбирает account/action.

Для тестов dedicated server принимает `--reset-auth-state`. Reset разрешён только до admission gameplay sessions. Пока `AccountRegistry` in-memory, restart и так создаёт пустой registry; флаг фиксирует будущий admin/test contract, который M8E.3 перенесёт на durable repository.

Тестовый запуск с явным сбросом:

```bash
./build/headless_server/EliteServer.exe --reset-auth-state --listen 127.0.0.1:27351
```

Текущая важная граница: `AccountRegistry` пока in-memory. После restart dedicated server не знает прежних handle/token bindings, поэтому первый вход на fresh server требует `REGISTER`; после регистрации reconnect к тому же живому серверу использует `SIGN IN`. M8E.3 расширен до durable authoritative universe persistence: identity/player/ship ownership является первым slice того же persistence subsystem, который затем хранит изменяемое состояние мира. Формальный password/recovery contract описан в `src/game/identity/AUTHENTICATION_ARCHITECTURE.md`.

### Runtime logging policy

Нормальный `EliteGame`/`EliteServer` запуск должен оставлять консоль пригодной для эксплуатации, а не воспроизводить всю M8E-трассировку. Подробные успешные startup/process/WebView/connect/auth/bootstrap/control события являются opt-in diagnostics и включаются через `ELITE_TRACE_RUNTIME=1`. Ошибки авторизации/session admission, handshake timeout, crash/GLFW failures и threshold-based slow-path события остаются безусловными. Self-test output (`[SELFTEST]`, `[PASS]`, `[FAIL]`) всегда остаётся доступным. Это позволяет вернуть глубокую трассировку без нового патча, но не маскирует реальные runtime failures.

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

- durable account/character storage across dedicated-server restart (`credential digest -> AccountId -> PlayerId -> ShipInstanceId`);
- production registration/account lifecycle beyond the current explicit development/LAN bearer-token flow;
- reconnect/resume token handoff beyond ordinary same-account reconnect and server-owned restoration of persistent identity;
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
interactive remote mode. Ready harness уже пересобирает те же канонические runtime
binaries, которые используются ниже; альтернативных `ready`/`network` server exe больше нет.

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

Automated ready suite и ручной `A -> reconnect A -> B -> reconnect B` gate на canonical binaries 2026-08-16 зелёные; M8E.1 закрыт. Короткие loading-screen stalls остались presentation/responsiveness debt и больше не сопровождаются control anomalies.

M8E.2 authentication/admission layer разделяет `SIGN IN`/`REGISTER`, запрещает implicit enrollment и сохраняет server-owned typed `SessionReject` до `Application`. Ручной acceptance выявил ещё одну presentation race: C++ пытался вызвать `showMultiplayerForm()/setConnectionError()` сразу после `navigate(main_menu.html)`, когда WebView2 всё ещё исполнял `loading.html`. M8E.2f вводит page-ready handshake `main_menu_ready`: pending Multiplayer sub-view и reject code применяются только после готовности нового DOM, поэтому ошибка остаётся в authorization form вместо падения в main actions.

---

## 11. Следующий этап — durable account / character identity storage

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

M8E.2 теперь фиксирует explicit authentication/admission boundary: стабильный AccountHandle + bearer token хранятся/выбираются на клиенте, server хранит handle + SHA-256 token digest, `SIGN IN` не создаёт account, `REGISTER` является явной операцией, duplicate login rejected typed reason'ом, а gameplay authority IDs остаются server-owned.

Следующий этап — **M8E.3 durable authoritative universe persistence**, а не отдельная временная account DB или новый selector hack:

1. общий `PersistenceCoordinator` задаёт schema/version/checkpoint/recovery lifecycle, но identity/player/ship/world остаются раздельными repositories;
2. первый slice сохраняет `credential digest -> AccountId -> PlayerId -> owned/current ShipInstanceId` атомарно и восстанавливает ownership после restart server;
3. `IUniverseStore` сохраняет `UniverseId`, save sequence, authoritative universe epoch и mutable world records; static catalogs остаются endpoint-local и проверяются fingerprints;
4. ownership/current-ship model хранится отдельно от control authority; runtime `EntityId` никогда не является durable key;
5. persistent ship/object records должны существовать и без materialized `EntityId`, чтобы `Scheduled <-> Coarse <-> Prewarm <-> Active` были lifecycle states одной persistent сущности;
6. checkpoint снимается на authoritative fixed-step boundary как immutable image, а disk I/O выполняется вне simulation thread;
7. файловый backend обязан иметь versioned schema, atomic temp->replace, last-known-good backup и fail-loud corruption policy; позднее append journal позволяет crash recovery без full rewrite на каждый tick;
8. password/auth storage проектируется через отдельный `IPasswordHasher`: plaintext password не сохраняется, fast unsalted SHA-256 для паролей запрещён; первый UI policy — 12–64 printable ASCII, CSPRNG generator предлагает 20+ символов;
9. account recovery является отдельным `AccountRecoveryService`: high-entropy recovery secret показывается один раз, server хранит только digest, успешное recovery ротирует password/device tokens и инвалидирует live sessions; внешняя почта/provider позже подключается через `IRecoveryChannel`;
10. определить production registration lifecycle (создание/удаление/rotation credential), rate limits и explicit resume/takeover policy отдельно от gameplay session;
11. Local game использует тот же persistence schema/code с отдельным save root, чтобы не возникла вторая несовместимая модель мира.

---

## 12. Near-term session/menu and persistence semantics

### Multiplayer ESC / account semantics

Multiplayer cannot pause the authoritative world. `Esc` opens an in-session menu/overlay with at least:

- Resume;
- Settings;
- Return to Main Menu / Disconnect (keeps remembered-device credential);
- Sign out and Return to Main Menu (disconnects, revokes/removes remembered-device credential, next login requires password/recovery);
- Quit.

Returning to the main menu must make session ownership explicit; it must not leave a hidden local authoritative world running behind a remote menu.

### Local-game menu and manual save policy

Local game uses the same persistence schema/backend code as dedicated multiplayer but has user-visible save slots. `Esc` exposes at least Resume, New Game, Save, Load, Return to Main Menu and Quit.

Manual Save **and Load** are allowed only when the authoritative local world reports a safe-save condition (base/docked/safe zone according to gameplay policy). The WebUI button state is only presentation: authoritative `SavePermissionService`/equivalent performs the same check and rejects attempts outside a safe-save state.

Dedicated multiplayer has no player-controlled load/rollback. It persists authoritative state continuously.

### Offline player ship continuity

Disconnecting a human does not despawn, stop or reset the owned ship. Session/control state is transient; physical ship state is universe state.

On disconnect:

```text
human session ends
  -> manual continuous control input is neutralized
  -> current velocity / angular velocity / orientation / damage / cargo remain
  -> explicit durable autopilot/flight-plan task may continue
  -> ship remains Active while observed/interacting
  -> otherwise it may collapse to Prewarm/Coarse/Scheduled state
```

A player may accelerate a ship, log out, and later find it far from the logout point. Manual held thrust does not remain magically pressed after disconnect; inertial velocity continues. A durable autopilot/order is a separate persistent command and may continue by design.

Offline-owned ships remain part of the world for other players and NPCs. They can be detected, collide, take damage or participate in interactions according to the same Active/Coarse lifecycle rules. Logout is not invulnerability.

Durable `ShipContinuityRecord` is keyed by `ShipInstanceId`, never by transient `EntityId`, and includes at minimum stable location/reference frame, position, velocity, orientation, angular velocity, condition/resources/cargo, lifecycle state and `lastSimulatedUniverseTime`.

When no observer requires full simulation, coarse propagation advances state from the persisted timestamp rather than simulating every render/physics tick. A simple inertial leg can be advanced analytically; gravity/autopilot/interaction cases use the appropriate coarse propagator or scheduled events. Re-entry into interest creates a new runtime `EntityId` from the same `ShipInstanceId`.

Dedicated persistent worlds target wall-clock catch-up for coarse/scheduled state after server downtime once M8E.3d is ready; local save slots default to paused universe time while the local game is closed. These are explicit `UniverseClockPolicy` choices, not accidental consequences of process uptime.
