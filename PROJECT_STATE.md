# PROJECT STATE

Краткая живая записка о состоянии проекта. Это не замена архитектурным контрактам и тестам, а быстрый ориентир: что уже принято, что сейчас делаем и что намеренно оставлено на потом.

## Как пользоваться этим файлом

- Обновлять после завершения заметного этапа или принятия важного архитектурного решения.
- Не дублировать сюда подробные технические контракты. Для runtime/client-server деталей см. `src/game/ARCHITECTURE_STATUS.md`.
- Принятая механика должна быть защищена regression/architecture/acceptance tests; отметка `[x]` здесь сама по себе не является доказательством.

Статусы:

- `[x]` — этап принят и на текущем уровне завершён.
- `[~]` — работает частично / миграция продолжается.
- `[ ]` — запланировано.
- `[D]` — сознательно отложено.

---

## 1. Базовые принципы проекта

- `[x]` Сервер authoritative для gameplay-состояния и решений.
- `[x]` Предсказуемые/детерминированные данные не должны постоянно передаваться сервером: клиент может вычислять presentation локально, сервер вычисляет их только когда они нужны authoritative gameplay-логике.
- `[x]` Статические определения кораблей, мешей, небесных тел и других типов могут независимо загружаться сервером и клиентом из общей версии данных.
- `[x]` Persistent identity конкретной сущности отделена от её типа/статического определения.
- `[x]` Принятая механика не должна тихо меняться архитектурными патчами; критические решения фиксируются тестами.
- `[x]` Целевой стиль — правдоподобная кинематографичность: внутренняя последовательность важна, но игра не должна превращаться в инженерный тренажёр.

---

## 2. Локализация и языки

**Статус: `[x]` текущий этап закрыт.**

- `[x]` Единый recursive localization asset domain: `src/assets/localization/`.
- `[x]` Переводы разложены по категориям: UI/maps/cockpit, sky cultures, star systems, navigation regions, game object types, future interstellar objects.
- `[x]` Звёздные системы изолированы по отдельным файлам; имя файла не является authoritative ID.
- `[x]` Повреждённый JSON пропускается без падения всей локализации; конфликты/ошибки должны диагностироваться.
- `[x]` WebUI и native OpenGL UI используют общий источник локализации.
- `[x]` Native map/object/Route overlays получают разрешённый `NavigationMapTextProfile` из `LocalizationService`; route actions, delete confirmations и Arrival Profile больше не содержат `ru/zh/es/ja` ветвлений в renderer C++.
- `[x]` Работают English / Russian / Simplified Chinese / Spanish / Japanese и fallback на English.
- `[x]` CJK font fallback работает в native renderer.
- `[x]` Три системы созвездий: Western/IAU, curated Traditional Chinese, Hawaiian Star Lines.
- `[x]` `Ctrl+Alt+F12` переключает глобальный язык пользовательского интерфейса.
- `[x]` В HUD показывается текущий язык и активная sky culture; читаемость поддержана фоновыми плашками.
- `[x]` Сервер локализации не знает: по protocol/runtime должны ходить stable IDs и authoritative state, а display names принадлежат клиенту.
- `[ ]` Manufacturer/cockpit-native labels при необходимости должны уметь оставаться на родном языке производителя независимо от глобального языка UI.
- `[D]` Release packaging локализации в единый `.locpak`/binary package с простым преобразованием/обфускацией, checksum и при необходимости signature. Нужен ближе к release, не сейчас.

---

## 3. Звёздное небо

**Статус: `[x]` текущий этап закрыт.**

- `[x]` Реальные звёзды хранят 3D galactic positions и пересчитываются относительно текущей позиции наблюдателя.
- `[x]` Созвездия привязаны к физическим звёздам и геометрически искажаются при смене точки наблюдения.
- `[x]` 23 вспомогательные constellation reference stars имеют конечные 3D positions/distances и участвуют в том же observer-relative процессе; fixed sky-direction путь удалён.
- `[x]` Подписи созвездий локализованы, camera-facing и относятся к выбранной sky culture.
- `[x]` Текущий видимый каталог ориентирован прежде всего на правдоподобное небо в окрестностях Sol и игровых систем порядка ~60 ly.
- `[D]` Proper motion звёзд во времени 2026–3026 сознательно не моделируем на текущем этапе.
- `[D]` Расширение каталога для астрономически корректного неба на сотнях/тысячах световых лет от Sol не является текущей целью.

---

## 4. Управление кораблём

- `[x]` Newtonian / Assisted переключаются через `Ctrl+F10`.
- `[x]` Дискретная команда переживает render-frame -> fixed-step boundary и не теряется между simulation steps.
- `[x]` Переключение выполняется после полноценного press/release цикла с debounce/latch, без пролистывания режима.

---

## 5. Client / Server separation

Подробный технический источник: `src/game/ARCHITECTURE_STATUS.md`.
Operational contract, режимы запуска и ручная проверка: `SERVER_CLIENT.md`.

**Общий статус: `[~]` client/server separation, real TCP process boundary и multi-client authoritative world уже работают. Текущий архитектурный этап — persistent identity / authorization / ship-control backbone перед полноценным населением и materialization мира.**

### Уже сделано

- `[x]` Client-facing runtime не зависит напрямую от `GameServer`.
- `[x]` `ITransport` / `IServerTransport` образуют protocol boundary.
- `[x]` Local loopback transport не владеет `GameServer&`.
- `[x]` Authoritative `ServerRuntime`/`GameServer` создаётся, выполняется и уничтожается на отдельном `ServerWorker` thread.
- `[x]` Server timeline / universe timeline / client presentation clocks разделены и имеют revision-safe contracts.
- `[x]` Local player использует fixed-step prediction/reconciliation.
- `[x]` Remote dynamic entities используют snapshot history и общий presentation window/interpolation.
- `[x]` CPU/GPU ownership assembly разделён; authoritative/headless headers не должны зависеть от OpenGL ownership.
- `[x]` Galaxy map в основном client-composed из локального StarAtlas + authoritative overlays.
- `[x]` System map celestial layer client-owned.
- `[x]` Обычные player/NPC ships на System map берутся из normal replication history, а не отдельного map ship channel.

### Текущая граница

- `[x]` System-map production infrastructure/hubs/ships берутся из ordinary authoritative `SimulationSnapshot` history и client-composed на metadata последнего уже принятого snapshot; `SystemMapRequest/Response` удалены, отдельного server-time handshake при F10 больше нет.
- `[x]` Details client-composed из endpoint-local StarAtlas/celestial state + ordinary replication на metadata последнего принятого `SimulationSnapshot`; `DetailMapRequest/Response` удалены, F11 не ждёт map RPC/response epoch.
- `[x]` Hub client-composed из parent celestial state + ordinary hub/modules/ships replication на той же accepted snapshot epoch; `HubMapRequest/Response` удалены, F12 не ждёт map RPC/response epoch.
- `[x]` StarAtlas — реально endpoint-local static data: client/server независимо грузят один и тот же каталог; `StarAtlasRequest`/presentation-data transport удалён.
- `[x]` `SessionWelcome` передаёт только `schemaVersion + contentFingerprint`; несовместимый локальный StarAtlas останавливает синхронизацию вместо тихого рассогласования мира.
- `[x]` Отдельный headless `EliteServer` executable target: собирается с `ELITE_BUILD_CLIENT=OFF`, не линкует GLFW/OpenGL/Freetype/WebView/UI/render и имеет authoritative runtime self-test.

### Ближайший рекомендуемый порядок

1. `[x]` Убрать StarAtlas payload из транспорта: client/server независимо загружают одинаковый static catalog; handshake проверяет schema/content fingerprint.
2. `[x]` Завершить System-map migration: infrastructure/hub identities/state приходят обычной authoritative replication, а presentation собирается клиентом.
3. `[x]` Перевести Details на client-side composition из authoritative entity/hub state + local static/celestial data.
4. `[x]` Перевести Hub на ту же модель.
5. `[x]` Создать отдельный headless `EliteServer` target и использовать configure/compile/link boundary как жёсткую проверку server independence от render/UI.
6. `[x]` Real process/multi-client foundation: M8A-M8D дают versioned wire protocol, canonical snapshot schema, Asio TCP adapters и отдельные `EliteServer`/`EliteGame` процессы. Два graphical remote клиента одновременно держатся на одном dedicated server и видят друг друга в одном authoritative world; process-local WebUI/WebView2 и Win32 event-pump fix убрали multi-process crash.
7. `[x]` **M8E.1 recovery gate (2026-08-16):** full MinGW ready suite зелёный; ручной `pilot-a -> reconnect A -> pilot-b -> reconnect B` на одном long-running canonical `EliteServer` также зелёный. Оба клиента сразу получают управление, reconnect rollback/«прорыв» исчез, загрузка одного клиента не нарушает управление второго.
8. `[x]` **P0.5 — canonical build/runtime layout и cleanup:** runtime paths зафиксированы как `build/EliteGame.exe` и `build/headless_server/EliteServer.exe`; acceptance использует эти же binaries, test scratch живёт под `build/tests/<suite>/`, legacy/dubious runtime directories удаляются, toolchain preflight не допускает ложный ready gate.
9. `[x]` **M8E.2a — recoverable session admission UI:** `GameSessionState::Failed` больше не оставляет WebView на terminal `loading.html`. Remote failure возвращает multiplayer authorization form с причиной ошибки и повторными SIGN IN/REGISTER/BACK; local failure возвращает main menu.
10. `[x]` **M8E.2b — explicit authentication/admission:** `SessionHello` несёт stable AccountHandle + opaque bearer credential + явный `SignIn/Register` intent; неизвестный handle на `SignIn` получает typed `UnknownAccount`, wrong token для известного handle — `InvalidCredential`, и ни один случай больше не enroll'ится автоматически. Только явный `Register` может создать `AccountId -> PlayerId` binding; занятый handle с другим token получает `AccountHandleTaken`, duplicate live login — `AlreadyActive`. Typed `SessionReject` проходит через loopback/TCP до `GameClient`, а rejected TCP получает bounded flush grace вместо мгновенного обрыва.
11. `[x]` **M8E.2c — multiplayer authorization UI:** multiplayer form содержит endpoint + stable AccountHandle и отдельные `SIGN IN` / `REGISTER`. Переход Local -> Multiplayer всегда приходит в эту форму; локальная session не определяет remote account identity. Sign-in читает только существующий credential slot, register может явно создать новый slot. Server-owned `AccountId/PlayerId/ShipInstanceId/EntityId` клиент по-прежнему не выбирает.
12. `[x]` **M8E.2d — admission hardening/cleanup:** unauthenticated TCP имеет handshake deadline и cap pending connections; stale GLFW guard удалён; runtime source больше не содержит `D:/__elite/work` fallback и защищён отдельным architecture contract.
13. `[x]` **M8E.2e — runtime log hygiene:** обычный client/server runtime больше не печатает подробную M8E startup/connect/auth-success трассировку. Verbose process/startup/WebView/bootstrap/control diagnostics доступны только при `ELITE_TRACE_RUNTIME=1`; реальные ошибки/reject/timeout/crash и редкие slow-path (`frame-gap`, Win32 dispatch/pump/swap, remote-sync) остаются видимыми всегда. Self-test `[SELFTEST]/[PASS]/[FAIL]` output не подавляется. Fresh local/network-process acceptance bootstrap использует явный `REGISTER`, а не старый implicit sign-in.
14. `[x]` **M8E.2f — authorization-form recovery correctness:** ручной acceptance подтвердил `main_menu_ready` handshake: после typed reject пользователь возвращается именно в Multiplayer authorization form, `REGISTER`/reconnect работают, а Local -> Multiplayer больше не подставляет скрытый `default` credential. Local runtime полностью отделён от remote credential slot.
15. `[x]` **M8E.2g — account/auth polish:** stable AccountHandle получает единый shared grammar `3..24 lowercase a-z0-9_-`, WebUI локализует правила и client-side input restriction, server проверяет handle независимо от UI, short-height menu получает responsive `clamp()/compact/scroll` layout, dedicated server — dev/test `--reset-auth-state`. Password/recovery пока не притворяются реализованными: их formal contract вынесен в `src/game/identity/AUTHENTICATION_ARCHITECTURE.md`.
16. `[ ]` **M8E.3 — durable authoritative universe persistence:** следующий storage layer охватывает не только account binding, а весь authoritative universe. Первый slice сохраняет `AccountHandle + credential/password/recovery records -> AccountId -> PlayerId -> current/owned ShipInstanceId`; затем тем же versioned persistence subsystem сохраняются universe epoch, dynamic ship/object/hub state, scheduled/coarse lifecycle и другие изменяемые world facts. Runtime `EntityId` в save не является durable identity и восстанавливается при materialization.
16. `[ ]` После durable persistence профилировать оставшиеся короткие loading-screen stalls; Win32 `WM_NCLBUTTONDOWN/HTCAPTION` modal-loop stress держать отдельной UI-thread responsiveness задачей, если он ещё воспроизводится.

### Multiplayer / session authority foundation

**Статус: `[x]` transport/process + reconnect-control + explicit authentication/admission baseline защищены automated/manual acceptance. Identity/runtime backbone готов, но M8E.3 storage implementation ещё не начат. Client-navigation executor/ownership cleanup закрыт: Route Plan вынесен из renderer в `ClientNavigationWorkspace`, ship route identity использует `ShipInstanceId`, START хранит явный `NavigationAssetRef`, а server session публикует только принадлежащие игроку commandable navigation assets. Shared trajectory predictor implementation добавлен как PATCH C и ждёт полного ready gate; после зелёного gate следующий navigation layer — projected/dashed trajectory presentation и route/intercept solver. Server/world persistence остаётся отдельным незакрытым фундаментальным track и понадобится до durable offline orders/autopilot/cross-restart universe continuity.**

Server runtime принимает несколько transport/session endpoints в одном authoritative execution context. Session больше не является идентичностью корабля: текущая authority chain — `ServerSessionId -> PlayerId -> ControlRegistry -> EntityId`, а persistent assignment игрока хранит `PlayerId -> ShipInstanceId`; `ShipInstanceRegistry` связывает стабильный экземпляр корабля с текущей materialized `EntityId`. `m_playerNavigation` удалён; legacy `m_playerId` остаётся compatibility alias старых single-player/debug paths. **Один global active celestial-system context** всё ещё остаётся отдельным ограничением world runtime до multi-system stage.

- `[x]` Stage M1 + identity migration: `ServerSessionId` остаётся transient connection identity, но `ServerSessionRegistry` теперь хранит `PlayerId`, а не `controlledEntityId`. `GameServer` разрешает `session -> PlayerId -> ControlRegistry -> EntityId`; client packet по-прежнему не может выбрать произвольный корабль.
- `[x]` Persistent identity Phase 1: `PlayerRegistry` хранит persistent player identity и назначенный `ShipInstanceId`; `ShipInstanceRegistry` отделяет стабильный экземпляр корабля от materialized `EntityId`; `ControlRegistry` отделяет human/AI/autopilot control axis от ship type/role.
- `[x]` Stage M1: NPC/activation authority больше не определяет игрока через singleton `m_playerId`; simulation хранит явный set player-controlled entities, они исключены из NPC authority и pinned `Active`. `m_playerId` пока остаётся compatibility alias для старых single-player navigation/diagnostic paths.
- `[x]` Stage M2: один `ServerRunner` обслуживает несколько `(IServerTransport*, ServerSessionId)` bindings: сначала принимает inbound со всех sessions, затем выполняет **ровно один** authoritative `GameServer::update()`, после чего fan-out'ит ответы обратно по session ownership.
- `[x]` Stage M2: `ServerRuntime` умеет authoritative admission/detach вторичной player session, публикует ей собственный `SessionWelcome` и bootstrap snapshot; secondary disconnect не останавливает primary session.
- `[x]` Stage M2: map request/response сохраняет destination `ServerSessionId`, time-sync отвечает через тот же connection endpoint; cross-session response leakage запрещён regression guard'ом.
- `[x]` Stage M2: обычный world snapshot пока остаётся full-world/full-presence, но `snapshot.session.playerNavigation` собирается отдельно для `controlledEntityId` каждой session.
- `[x]` Stage M2: headless `EliteServer --self-test` поднимает две transport sessions в одном runtime, назначает два разных controlled ships, проверяет независимые control ticks, map/time-sync routing, per-session navigation и disconnect secondary session.
- `[x]` Registry защищён от stale reconnect: старая disconnected session не может вернуть authority, если тем же кораблём уже владеет новая connected session.
- `[x]` Stage M3: клиент больше не использует `ShipRole::Player` как признак **локально** управляемого корабля. `SessionWelcome.controlledEntityId` сохраняется в `ClientWorldState` и является единственной identity для prediction/fractional local presentation/player-system lookup и локального маркера на System/Details/Hub maps. Другой `ShipRole::Player` остаётся remote entity и идёт через обычную snapshot interpolation.
- `[x]` Stage M4: singleton `GameServer::m_playerNavigation` удалён. Shared replication snapshot не содержит чью-либо navigation identity; `copySnapshotForSession()` вычисляет `PlayerNavigationState` только через server-owned `session -> controlledEntityId -> navigationStateForEntity()`. World-runtime celestial context теперь выбирается отдельно от session navigation и не имеет права следовать за произвольным «primary player».
- `[x]` Local loopback остаётся нормальной односессионной реализацией того же protocol boundary; обычный `EliteGame.exe` не требует внешнего сервера.
- `[x]` Stage M5: два настоящих `GameClient` одновременно работают через два transport endpoints на одном `ServerRuntime`: обе state machine доходят до `Ready`, получают разные server-assigned controlled entities, видят общий authoritative world, но применяют local prediction только к своему кораблю; per-session navigation и независимые numbered input/ack streams проверяются отдельным acceptance gate.
- `[x]` Stage M6: **simulation activation** и **replication interest** разделены. Для каждой destination session сервер строит ship-interest plan от её `controlledEntityId`/system/distance (`Controlled / Tactical / Nearby / Coarse / None`) с отдельной target publication cadence; этот policy не является sensor/visibility правилом и не меняет authoritative simulation mode.
- `[x]` Stage M6: protocol введены `FullAuthoritativeSet` и `SparseRetainMissing` + explicit `removedShipIds/removedObjectIds/removedHubIds`. `ClientWorldState` при sparse omission сохраняет entity и удаляет только explicit lifecycle rows; retained snapshot history materializes canonical full samples для interpolation/maps.
- `[x]` Stage M7: production `ServerRunner` реально применяет per-session interest cadence к ship payload. `Controlled/Tactical` публикуются на normal snapshot cadence, `Nearby/Coarse` — по target interval, `None` получает explicit interest-exit removal. Destroy/remove не ждёт coarse deadline.
- `[x]` Stage M7: late join и re-entry не зависят от текущих dirty graph flags. `GameServer` удерживает canonical field-complete replication state; bootstrap и первый packet после re-entry берут hydrated row из него. Objects/hubs пока остаются full-cadence, но их lifecycle removal уже explicit под `SparseRetainMissing`.
- `[~]` Multiplayer contract теперь покрывает server sessions, multi-transport routing, client local-vs-remote identity, session-derived navigation, two-`GameClient` acceptance и реальный sparse per-session ship publication. Stage M8A добавил ABI-independent network-byte-order framing/version fence и codecs для control plane; Stage M8B добавил полный snapshot/map-response data plane через отдельный канонический ordered schema и schema-blind compression seam.
- `[ ]` Перед фактическим **visibility hiding** server transport interest должен пересекаться с authoritative world-knowledge/visibility policy, чтобы network interest не стал утечкой скрытых объектов. Текущий M7 оптимизирует transport cadence существующего replicated world, а не моделирует radar/knowledge.
- `[ ]` После фиксации network packet/reliability semantics можно добавить field-level delta compression с baseline/versioning; нельзя строить хрупкие deltas, предполагая доставку каждого предыдущего sparse packet.
- `[x]` Stage M8A: wire format отделён от C++ object layout: fixed magic/version/kind/length/sequence header, network byte order, bounded payload/string lengths, stream fragmentation/coalescing decoder и explicit control-plane codecs. Win32/POSIX socket API в этот слой не попадает.
- `[x]` Stage M8A portability hygiene: исправлены project-local include-case mismatches (`EntityId.h`/`EntityID.h`, `Log.h`/`log.h`) и добавлен regression guard, потому что Windows это прощает, а Linux filesystem — нет.
- `[D]` Первый production process transport строим как **reliable ordered byte stream (TCP)**. Это упрощает bootstrap/sparse lifecycle и не заставляет deltas предполагать потерянные unordered packets. UDP/датаграммы можно добавить позже для отдельных high-rate каналов только с собственными sequence/ack/baseline rules.
- `[x]` Stage M8B: полный `SimulationSnapshot`/`MapResponse` wire codec готов. Field order хранится в одном `WireDataSchema.h` и используется одинаково encode/decode; generic binaryizer знает только primitive/vector/variant/schema mechanics. Sparse envelope, lifecycle removals, runtime graph, session state и map-response payload проходят round-trip. Отдельный `IWireCompressor` получает только bytes; текущий `NoWireCompression` — passthrough seam перед будущим LZ4/Zstd без знания entity/module counts.
- `[x]` Stage M8C: добавлены schema-blind `TcpWireStream`/listener на standalone Asio и typed `TcpClientTransport`/`TcpServerTransport` adapters под существующие `ITransport`/`IServerTransport`. TCP слой видит только framed bytes, имеет per-direction sequence validation и bounded pending-write queue; localhost contract реально гоняет control plane + sparse/hydrated `SimulationSnapshot` + `MapResponse` через kernel TCP.
- `[x]` Stage M8D: TCP adapters подключены к process host/session lifecycle: `EliteServer --listen HOST:PORT` + remote `EliteGame --connect HOST:PORT`/`RemoteGameSession`. Dedicated runtime может стартовать с нулём gameplay transports; accepted connection получает controlled entity только через server-owned admission. `SessionWelcome` передаёт authoritative fixed-step cadence для remote prediction. Ready harness запускает `EliteServer.exe` и `EliteGame.exe` как разные процессы и проверяет bootstrap -> synchronization -> numbered input/ack -> disconnect.
- `[x]` Stage M8D hardening: separate-process acceptance выявил скрытую зависимость клиента от server-initialized process globals. `GameClient` теперь сам bootstrap’ит endpoint-local `ObjectDescriptorRegistry`/`ObjectAssemblyRegistry` через one-time `ensureInitialized()`, а wire byte-order contract следует текущему `WireProtocolVersion` после version bump.
- `[x]` Stage M8E graphical baseline: первый graphical client получает фиксированный process-local WebUI/Debug Control endpoint `localhost:8090` через cross-process lease, дополнительные клиенты используют OS-assigned ephemeral ports; WebView2 user-data folder изолирован по process, remote client умеет ждать ещё не запущенный server, а `EliteServer` защищён OS-level singleton guard. Диагностика доказала GLFW 3.4 Win32 cross-process AV: `_glfwPollEventsWin32()` мог получить active HWND другого `EliteGame` и разыменовать его `GLFW` property как локальный `_GLFWwindow*`. На Windows client event pump теперь использует native `PeekMessage/TranslateMessage/DispatchMessage` path и не входит в опасный GLFW post-poll path; два graphical клиента после этого держатся одновременно стабильно.
- `[x]` Identity backbone Phase 1: dedicated bootstrap создаёт два persistent `PlayerId`/`ShipInstanceId` bindings, materialized ships разведены на 50 м, `SessionWelcome` несёт `playerId + controlledShipInstanceId + controlledEntityId`, а `ShipSnapshot` несёт `instanceId`. Ручной graphical acceptance подтвердил: оба клиента видят оба корабля в одном authoritative world; прежний симптом «второго игрока нет / параллельные реальности» исчез после исправления identity structure.
- `[x]` M8E.2 убрал implicit first-connect enrollment: unknown `SignIn` больше не получает свободный `PlayerId`; binding создаётся только явным `Register`, duplicate live session rejected typed `AlreadyActive`, а reconnect сохраняет server-owned identity/control rules. `[x]` M8E.2f закрыл WebView recovery race и неявный process-level `default` credential. `[x]` M8E.2g фиксирует stable AccountHandle/validation/reset/responsive UI. `[~]` UI platform теперь выносит shared shell/components/preferences до full account/social screens. `[ ]` M8E.3 делает identity и mutable universe state durable across restart; client-selected `--player`/`--entity` authority shortcut запрещён.
- `[ ]` Отдельно добавить Linux headless build/smoke для `EliteServer`; текущий server/session/runtime код остаётся platform-neutral, а Win32/POSIX детали должны жить только в transport/platform adapters.
- `[ ]` Несколько игроков в разных звёздных системах требуют последующего multi-system runtime; multiplayer session foundation не должна снова зашивать один global `m_activeCelestialSystemId`.

---

## 6. Навигация и определение положения

**Статус: `[~]` Route Plan закрыт; shared trajectory predictor PATCH C реализован. Активный local-guidance corridor уже имеет map + cockpit presentation; общие history/prediction/long-range route producers, solver и autopilot впереди.**

### Client navigation tracking

- `[x]` Открытые карточки кораблей/хабов становятся персонально отслеживаемыми tactical targets и дают постоянные cockpit HUD markers; active target при этом остаётся один, открытых/tracked карточек может быть много.
- `[x]` Cockpit-marker использует HUD safe-frame/off-screen projection: размер метки не зависит от дальности, направление сохраняется за границей экрана, показываются тип/имя/скорость/расстояние.
- `[x]` В маневровых/non-cruise режимах tactical marker показывает собственную скорость цели в текущем travel frame игрока — в той же системе, что `|localVelocityMps|` HUD самого игрока, а не скорость сближения; в `Cruise`/`JumpTransit` показывается global speed, а без валидного общего frame используется честный global fallback.
- `[x]` System Map поддерживает несколько одновременно открытых карточек звёзд/планет/лун; они являются client-only tracked celestial targets и дают cockpit markers без speed/azimuth/elevation rows.
- `[x]` Galaxy/System пустой navigation cube, а также карточки физических объектов могут добавлять route intent; корабль как промежуточная динамическая цель имеет отдельный смысл `ROUTE RENDEZVOUS` (перехват + match velocity + продолжение), а не замороженную координату. `FINISH` единственный: пока он существует, другие карточки не предлагают второй Finish, а карточка самого Finish не предлагает `WAYPOINT`.
- `[x]` Единый Route Container присутствует на Galaxy/System/Detail/Hub: сворачивается, хранит Finish последним, поддерживает **live drag reorder** промежуточных точек (строка следует за мышью и порядок меняется до отпускания), master/per-point HUD toggles и явное подтверждение удаления в footer (`удалить маршрут/точку? -> да/нет`). Single-click/drag выделяет строку и соответствующий видимый map marker; double-click возвращает authored Galaxy/System/Detail/Hub context, при необходимости сначала загружая другую систему/пустой сектор, и раскрывает соответствующую инфокарточку.
- `[x]` Route point на карте — зелёный квадрат с центральной точкой и route-order number; cockpit route marker также всегда показывает порядковый номер, включая Finish как последнюю точку.
- `[x]` Finish хранит простой Arrival Profile с четырьмя квадратными локализованными режимами: `SAFE`, `FOLLOW`, `FORMATION`, `PARADE`; текущая WorldPosition служит обновляемым fallback.
- `[x]` Navigation ownership приведён к целевой архитектуре текущего этапа: `SpaceState` владеет `ClientNavigationWorkspace`, внутри отдельно живут transient `TargetTrackingState` и persistent `RoutePlan`; карты/HUD только редактируют/читают workspace.
- `[x]` Route identity typed/stable: корабль хранится как `ShipInstanceId`, Hub/body — stable domain ID, free-space — canonical `WorldPosition`; `EntityId` остаётся только transient presentation binding. Drag/delete/HUD/reorder используют route-node ID.
- `[x]` Номера tactical target принадлежат только кораблям. Небесные тела и Hub на System Map/HUD не получают target number; route-order numbers — отдельная нумерация и сохраняются у точек маршрута.
- `[x]` Tracking/cards/waypoints не реплицируются на сервер: это персональная навигационная память клиента. Серверный command boundary понадобится только при реальном исполнении маршрута/autopilot.
- `[x]` Native map/route UI снова подчиняется единому localization contract: строки Route Container, Arrival Profile, object-card actions и confirmations хранятся в `assets/localization`, а C++ renderers получают уже разрешённый `NavigationMapTextProfile` без `ru/zh/es/ja` ветвлений.
- `[~]` Позиция tracked celestial body вне открытой System Map пока хранится последним client-composed sample; отдельный continuous ephemeris resolver для произвольных отслеживаемых систем ещё не сделан.
- `[x]` PATCH A: renderer-independent navigation ownership + typed stable route target identity (`ShipInstanceId`/Hub/body/spatial address) закрыт и защищён architecture guards.
- `[x]` PATCH B: Route Plan получил обязательный explicit START/execution asset; START не участвует в reorder/delete, содержит stable `NavigationAssetRef` и не рисуется в cockpit HUD, когда executor совпадает с локально управляемым кораблём. Серверная `ownedNavigationAssets` projection строится из authoritative `ShipOwnershipRegistry`, отдельно от `ControlRegistry`; `DroneInstanceId` зарезервирован для будущих durable owned drones. Мёртвый `NavigationPlan` удалён из `DynamicMotionState`/wire schema.
- `[~]` PATCH C: общий renderer/server-neutral `TrajectoryPredictor` реализован и покрыт отдельным test block/architecture guard; полный MinGW ready gate ещё должен подтвердить интеграцию. Predictor принимает system-local kinematic seed, gravity sources, proper-acceleration program и caller-selected acceleration/jerk envelope; выдаёт time-series position/velocity/acceleration, отдельно gravity/proper acceleration, proper crew G-load и accumulated delta-v. `TrajectoryMapAdapter` переводит результат в существующий `MapObjectTrajectory` seam без обратной зависимости predictor -> renderer.
- `[~]` **Ближайший шаг:** live-валидация docking guidance: direct dock selection, X-only information-card lifetime, dock-card-scoped task cancellation, dashed planned path на System/Detail/Hub и 6-DOF GuidanceTunnel в Flight HUD. Selection/empty-map clicks никогда не закрывают карточки; только `X` dock-card отменяет docking guidance. Затем oriented swept-volume collision checking/richer local avoidance. Общий route/intercept solver и long-range trajectory layer идут после стабилизации этого local-guidance vertical slice; autopilot остаётся позже. Финальный продуктовый контракт: `src/game/navigation/ROUTE_NAVIGATION_CONTRACT.md`.

### Космический компас

- `[ ]` В кокпите нужна горизонтальная шкала условного азимута и вертикальная шкала elevation.
- `[ ]` Компас показывает направление **носа корабля относительно выбранного navigation/reference frame**, а не просто координатных осей XYZ мира.
- `[ ]` Отдельно от компаса всегда остаётся **вектор фактической скорости**. В Newtonian режиме особенно важно одновременно видеть: «куда смотрю» и «куда лечу».
- `[ ]` Навигационные базисы должны учитывать всю иерархию координат: `galactic -> system/local -> kinematic/reference frame -> player-relative render`.
- `[ ]` Для Hub естественная ориентация строится на базисе `prograde / radial / normal`.
- `[ ]` Для свободного пространства нужно определить соответствующий system/galactic reference frame и правила его выбора/отображения.

### Ориентация против абсолютных координат

- `[x]` Принцип принят: направление/ориентацию корабля в пространстве можно определять очень точно по звёздному небу.
- `[ ]` Абсолютные координаты — отдельная навигационная задача; знание ориентации само по себе их не даёт.
- `[ ]` Для абсолютного положения предполагаются навигационные маяки и/или известные объекты с известными эфемеридами.
- `[ ]` Без внешней опоры корабль может знать ориентацию и собственную кинематику, но ошибка абсолютного положения должна со временем накапливаться.
- `[D]` Квазары не вводим как обязательную gameplay-механику. Принцип определения координат можно оформить лором и инфраструктурой навигационной сети.
- `[ ]` Определить модель точности навигации: начальная ошибка, накопление drift, коррекция по маякам/объектам и поведение при потере связи/опорных сигналов.

---

## 7. J / межсистемный полёт

**Статус: `[ ]` окончательная механика J пока не реализована.**

- `[ ]` Формализовать переход объекта из system-local пространства в **interstellar/galactic domain** (`systemId < 0`).
- `[ ]` Реализовать реальную транзакцию `system -> interstellar -> system`, а не телепортацию между двумя system-local состояниями.
- `[ ]` Поддержать несколько одновременных server system-runtime contexts.
- `[ ]` Корабль вне системы не должен оставаться логически привязанным к старому Hub или старому system runtime.
- `[ ]` Дальний межсистемный полёт не должен симулироваться миллионами обычных physics ticks.
- `[ ]` Для дальнего участка должна существовать scheduled/coarse trajectory, которая materialize в полноценную физику только когда это становится необходимо.
- `[ ]` Финальный gameplay J-mode проектировать после стабилизации runtime ownership, activation и межсистемного transfer boundary.

---

## 8. Траектории на картах

**Статус: `[~]` data seam существует (`MapObjectTrajectory` + History/Prediction/Planned); первый реальный producer/consumer slice готов для активного local `GuidanceCorridor`: System/Detail/Hub проецируют его samples и рисуют plain solid planned path. Общие history/prediction/on-demand producers ещё впереди.**

Карты должны уметь по запросу отвечать на вопросы:

- `[ ]` «Покажи траекторию этого судна».
- `[ ]` «Покажи возможные траектории, которыми оно может достичь этого астероида».
- `[ ]` «Какими траекториями оно могло попасть туда».
- `[ ]` «Покажи траектории всех известных судов в этом кубе за последний час».

### Архитектурный принцип

Renderer траекторий должен быть **независим от происхождения данных**. Источником знания могут быть:

- наш radar observation;
- transponder;
- SOS;
- station/beacon;
- navigation network;
- historical log;
- intelligence/recon data.

Существование реальной траектории корабля и **знание игрока об этой траектории — разные вещи**.

Нужно различать как минимум:

- instantaneous velocity vector;
- planned route;
- predicted physical trajectory;
- historical recorded trajectory.

- `[ ]` Не передавать массивы trajectory points в каждом обычном snapshot.
- `[ ]` Сделать отдельный **on-demand trajectory query/cache** с собственным lifetime/versioning.
- `[ ]` Источник/качество знания должно определять, какие части траектории игрок вообще имеет право видеть и с какой точностью.

---

## 9. Дальние корабли и масштабирование мира

**Статус: `[~]` архитектурный каркас есть, production materialization ещё не завершена.**

Базовая цепочка runtime fidelity:

```text
Scheduled
-> Coarse
-> Prewarm
-> Active
```

и обратно.

- `[x]` Есть entity runtime-policy foundation: `EntityType`, `MotionModel`, `SimulationMode`, `TimelineDomain`, authority/presentation policy.
- `[x]` Есть режимы `Scheduled / Coarse / Prewarm / Active` и explicit transition rules.
- `[x]` Есть activation anchors, spatial broad-phase, predicted interaction/CPA logic и hysteresis/state machine.
- `[x]` Stage 4A: activation policy реально снижает materialized CPU work — NPC tactical AI, reactor/thermal/cooling/life-support и structural/repair maintenance получают разные `Active / Prewarm / Coarse` cadence. Накопленный `dt` сохраняет прошедшее service-time.
- `[x]` Stage 4A снизил cadence NPC AI, internal ship systems и structural/repair maintenance без изменения motion trajectory.
- `[x]` Stage 4B разделил motion на дорогой **control/rate evaluation** и дешёвую **kinematic propagation**: `Active` сохраняет старый 50 Hz путь, `Prewarm` пересчитывает control примерно 25 Hz, `Coarse` примерно 5 Hz, но orientation/translation продолжают authoritative fixed-step propagation между этими расчётами. HubTactical engine acceleration удерживается между coarse control updates, поэтому сущность не замирает и не телепортируется.
- `[x]` Stage 4B сохраняет full-rate authoritative simulation/source publication semantics; Stage M7 уже отдельно decimate'ит **per-session network ship rows** без изменения simulation activation. Signals, objects и hubs пока остаются на прежней publication cadence.
- `[~]` Activation ещё не является полной системой снижения стоимости мира: multiplayer session/interest + sparse ship transport уже готовы; далее network transport и `Scheduled` materialization/collapse.
- `[ ]` Корабль, летящий месяц между точками, не должен считаться полноценной физикой 50 раз/сек.
- `[ ]` При приближении игрока дальняя сущность materialize в правильной точке своей траектории и получает полноценную физику/NPC/повреждения/столкновения.
- `[ ]` При удалении сущность должна снова collapse в coarse/scheduled representation без потери persistent identity, состояния и истории.
- `[ ]` Перевести production ships/hubs/modules на новую runtime policy системно.

- `[D]` Перед массовой materialization утверждённых моделей нужен offline **compiled object geometry/physics package**: authoring OBJ/mesh один раз компилируется в server-needed collision/LOD0 geometry, bounds, hit volumes, seams/structural links и другие immutable physical definitions. Headless server должен загружать готовые бинарные данные, а не пересчитывать hit/seam topology при каждом старте; render LOD1+/materials/textures остаются client-only. Пока модели и hit topology активно меняются, runtime pipeline не ломаем.

---

## 10. Persistent ships

**Статус: `[~]` stable ship identity и runtime registry уже введены; durable persistent records/storage и Scheduled lifecycle ещё не завершены.**

Корабли мира должны быть в основном реальными persistent-сущностями, а не бесконечно рождающимся NPC-фоном.

Нужно чётко разделять три уровня:

```text
typeId
    Cobra Mk.I

persistent ship identity
    ShipInstanceId
    собственное имя
    serial / registration
    owner
    faction
    condition
    cargo
    history

runtime entity
    текущая материализованная физическая сущность
```

И отдельно:

```text
SignalIdentity
```

- `[x]` Тип корабля отделён от конкретного экземпляра с persistent identity.
- `[x]` Введён стабильный `ShipInstanceId`, отличный от runtime `EntityId`; instance ID проходит через `SimulationSnapshot`, поэтому два клиента могут ссылаться на один и тот же конкретный корабль независимо от materialization handle.
- `[x]` `ShipInstanceRegistry` хранит текущую связь `ShipInstanceId <-> materialized EntityId` и имеет explicit dematerialization seam. Текущий registry пока runtime/in-memory и bootstrap'ится из materialized initial world — это ещё не durable universe database.
- `[ ]` Завершить durable persistent ship records, ownership/location/state storage и lifecycle.
- `[ ]` Один persistent ship должен существовать без materialized runtime entity в `Scheduled/Coarse` состоянии.
- `[ ]` Runtime entity должна создаваться/уничтожаться без потери identity, cargo, damage, history и owner/faction state.
- `[ ]` `SignalIdentity` отделить от фактического `typeId`: физически Cobra может называться «Матроскин», а транспондер заявлять `Agricultural Waste Processing Vessel Mk.II`.
- `[ ]` Экономическая симуляция должна учитывать существующие суда, задержки, нападения, потерю груза, ремонт и т. п. без обязательной full-rate симуляции каждого корабля.

---

## 11. Информация о корабле / WorldSignal

**Статус: `[ ]` модель UI/идентификации обсуждена, полноценный data path впереди.**

Предполагаемая метка объекта:

- основной крупный текст — тип/класс объекта, потому что пилоту прежде всего важно понимать, с чем он столкнулся;
- вторая строка — собственное имя, позывной, registration/serial или другой instance identifier.

Важно: отображаемый класс определяется не обязательно фактическим `typeId`, а **качеством и источником идентификации**. Radar, визуальное распознавание, transponder и внешняя intelligence могут давать разные ответы.

- `[ ]` Развести factual entity identity, signal identity и player-known identity.
- `[ ]` WorldSignal/label presentation строить из того, что игроку известно, а не напрямую из authoritative hidden metadata.

---

## 12. Карты

**Статус: `[x]` базовая навигационная и presentation-архитектура Galaxy / System-or-Space / Details / Hub зафиксирована regression-контрактами.**

Каноническое подробное описание текущей механики:
- `src/game/system_map/MAP_NAVIGATION_CONTRACT.md` — адресация и переходы между слоями;
- `src/game/system_map/MAP_OBJECT_OVERLAY_CONTRACT.md` — корабли/хабы, velocity vectors, карточки, track IDs и trajectory seam;
- `src/game/navigation/NAVIGATION_TRACKING_CONTRACT.md` — client-only tracked objects/bodies, cockpit markers и route-waypoint baseline.

### `[x]` То, что считаем важной рабочей механикой

- Есть четыре внутренних режима: `Galaxy`, `System`, `Detail`, `Hub`.
- Пользовательская иерархия: `Galaxy -> System/Space -> Details -> Hub`.
- `SPACE` сейчас является presentation-именем `MapMode::System` для пустого межзвёздного сектора; отдельного `MapMode::Space` пока нет.
- Galaxy cubic navigation умеет входить как в известную систему, так и в выбранный пустой сектор.
- Пустой сектор получает уникальный отрицательный runtime `systemId`; это временное представление, но пока оно является защищённой частью механики адресации.
- System/Space Details строится из **загруженного контекста карты**, а не из `playerNavigation().currentSystemId`.
- Выбранный непредельный System-куб автоматически разрешается в центрального потомка на максимальном уровне; вручную проходить все уровни перед Details не требуется.
- Terminal spatial cube в пустом секторе открывает настоящий пустой `SpatialVolume`; реальная система/Солнце не подставляются.
- В известной системе terminal spatial cube остаётся spatial volume, кроме случая, когда центр адреса физически находится внутри celestial body: тогда Details семантически становится Details этого body.
- Details поддерживает `CelestialBody`, `LocalObject` и `SpatialVolume` targets.
- Выбранный Hub может открывать и `Details`, и `Hub`; прямой `System -> Hub` сначала готовит точный parent Details target, поэтому `Hub -> Details` возвращается туда же.
- Native STAR ATLAS panel использует typed actions; `Close` и старый fixed toggle-slot удалены.
- Матрица панели:
  - Galaxy: `SYSTEM/SPACE`, `DETAIL disabled`, `HUB disabled`;
  - System/Space: `GALAXY`, `DETAIL conditional`, `HUB conditional`;
  - Details: `SYSTEM/SPACE`, `GALAXY`, `HUB conditional`;
  - Hub: `DETAIL`, `SYSTEM/SPACE`, `GALAXY`.
- Для обычного выбранного cube/body активен `DETAIL`. Для активного tactical object кнопка `HUB` становится context-sensitive local drill: объект внутри Hub ведёт в родительский Hub, свободный объект на System Map — в terminal cube его текущей окрестности; простой выбор cube/body сам по себе `HUB` не включает.
- F9-F12 являются **player-relative direct selectors**, а не contextual drill:
  - F9 Galaxy;
  - F10 current System / meaningful interstellar Space sector;
  - F11 current player Details context, либо fallback к System/Space когда Details адреса нет;
  - F12 matched Hub либо существующий player-local fallback.
- Повторный direct selector текущего navigation-level — no-op; более новый pending selector вытесняет старый.
- `Ctrl+F11` меняет глобальный формат координат; `Ctrl+Alt+F12` меняет UI locale.
- Camera / picking / cubic-grid состояние и map presentation готовятся до input; input и render одного application frame используют одну подготовленную snapshot/presentation границу.
- Внутренние map-to-map transitions deferred: outgoing frame не должен преждевременно стать destination-mode.
- Galaxy/System/Details/Hub production presentation собирается клиентом из local catalogs/celestial state + ordinary authoritative replication/epochs. System/Details/Hub не имеют отдельного authoritative world-state канала.
- Timeline revision сбрасывает старые snapshot/interpolation данные, но сохраняет semantic loaded target для восстановления той же карты на новой ветке времени.
- Tactical object overlay заменяет debug axes/cross/cube markers кораблей и map-scale хабов компактными треугольными glyphs: нос показывает ориентацию, отдельная стрелка — фактический velocity vector.
- Global velocity показывается синей стрелкой на System и обычных Details; local/relative velocity — зелёной на Hub Map и terminal empty-space SpatialVolume. В empty-space local travel-frame vector обязательно переводится обратно в world/reference direction перед экранной проекцией.
- Центральный Hub на Hub Map сохраняет существующую структурную геометрию и получает широкую полупрозрачную стрелку глобального движения; его карточка при этом остаётся в локальной системе отсчёта.
- Одновременно можно открыть несколько независимых карточек объектов; каждая перетаскивается, имеет собственный z-order и leader line к текущей screen projection объекта. Открытые карточки и selection разделены: карточек много, активный tactical object один. Клик по карточке возвращает фокус её объекту, а `X` закрывает только информацию.
- Клик/drag tactical overlay захватывает gesture до release и не должен одновременно запускать camera orbit/pan. Активный tactical object остаётся активным даже когда его parent Hub сохранён только как local-navigation context; `HUB` ведёт в parent Hub либо, для свободной System-map цели, в terminal cube вокруг объекта. Обычный выбор тела/куба по-прежнему снимает tactical-object focus.
- Длина local/global velocity arrows кодирует модуль скорости линейно с отдельным local/global saturation и прежним максимальным экранным размером.
- Только корабли получают короткие стабильные в пределах текущего overlay-state target/track numbers для визуальной идентификации во времени; игрок зарезервирован как `0`, но этот номер не рисуется. Небесные тела и Hub на System Map/HUD не нумеруются как цели. Cockpit ship marker использует тот же map track number, а route-order numbers существуют отдельно и принадлежат точкам маршрута.
- Cockpit tracked-object marker снова является фиксированным полупрозрачным контурным треугольником, всегда направленным вверх и не зависящим от расстояния. Track number вынесен из символа в левую текстовую колонку под speed и выровнен с ней по правому краю.
- Route intent теперь живёт независимо и от cube focus, и от source-card lifetime. Единый Route Container на всех четырёх картах держит один Finish последним и несколько нумеруемых Waypoint, поддерживает live drag reorder с перестановкой во время удержания мыши, HUD visibility, double-click authored-context recall и подтверждаемое удаление; закрытие исходной инфокарточки маршрут больше не меняет. Ship waypoint трактуется как rendezvous checkpoint, а не как старая координата корабля.
- Glyph/velocity arrow имеют clamped zoom-aware scale: вдали остаются читаемыми, вблизи растут вместе с projected physical size объекта.
- Hub Map допускает close inspection (`maxZoom >= 64`) и zoom-dependent pan allowance, поэтому к летающим вокруг станции кораблям можно приблизиться.
- `MapObjectTrajectory` уже выделен отдельным presentation-контрактом для history/prediction/planned samples, но текущий velocity **не превращается автоматически в выдуманную траекторию**.

### `[~]` Временные / legacy механики, которые пока оставлены

- `MapMode::System` одновременно обслуживает известную систему и пустой `SPACE`; это naming/model debt, а не две разные реализации.
- Отрицательные synthetic empty-sector ids — временный runtime-механизм до появления first-class spatial-domain address/type.
- В map input всё ещё есть legacy `P -> setSystemMapDetailMode()`.
- В map input всё ещё есть legacy `Backspace -> setSystemMapCurrentSystemMode()`; это **не** canonical parent-navigation path. Native panel возвращается через `setSystemMapLoadedSystemMode()` и сохраняет inspected context.
- `SystemMapRenderer` ещё остаётся facade/coordinator для Galaxy/System и часть low-level backend всё ещё лежит в `.inl`; Detail/Hub ownership уже вынесен отдельно.

### `[ ]` Чего пока нет / что не считать готовой механикой

- Полноценная ship selection на System Map -> Details.
- Historical и произвольные selected-object predicted trajectory producers/rendering; active local planned `GuidanceCorridor` уже является первым реальным trajectory producer и рисуется на System/Detail/Hub.
- Persistent ship target/track numbers между reconnect/restart; текущие ship short IDs стабильны только в lifetime overlay-state.
- Authoritative/player-known faction source; `factionColor` seam существует, текущие цвета — presentation defaults.
- First-class durable `SpaceSectorId` / spatial-domain type вместо negative synthetic ids.
- Отдельный публичный `MapMode::Space`.
- Browser/WebView command transport для STAR ATLAS — удалён и не должен возвращаться как параллельный navigation path.
- Кнопка `Close` — удалена и не является частью дизайна.

### Regression gates

```bash
bash tests/system_map/run_mingw64.sh
bash tests/architecture_contracts/run_mingw64.sh
bash tests/client_acceptance/run_mingw64.sh
```

`tests/system_map` отдельно фиксирует semantic panel action matrix/command routing, запрещает потерю loaded navigation context и охраняет tactical object overlay (`check_object_overlay.py` + C++ behavioral tests). Любое намеренное изменение пунктов `[x]` должно менять одновременно код, тест и соответствующий MD-контракт; тест нельзя ослаблять только ради того, чтобы случайный behavioral drift снова стал зелёным.

Целевая ownership-схема остаётся:

```text
SERVER
authoritative facts / IDs / state / epochs

        ↓

CLIENT
local catalogs
+ replicated runtime
+ deterministic celestial state
+ loaded navigation context

        ↓

maps / presentation
```

---

## 13. Кто считает физику / delegated simulation islands

**Статус: `[D]` исследовательская идея, не текущий production plan.**

Рассматривалась модель, в которой при единственном наблюдателе около Hub его клиент может временно считать часть дорогой локальной детальной симуляции, например:

- collisions;
- rotating/destructible structures;
- локальные NPC interactions.

При появлении второго игрока или другой причины authority должна возвращаться серверу:

```text
client simulation lease
        ↓
server takes simulation authority
        ↓
both clients are observers/predictors
```

Ограничения:

- `[x]` Модель «клиент сообщил `damage=800`, сервер поверил» неприемлема.
- `[ ]` Если к этой идее возвращаться, нужна формальная authority lease / simulation-island transaction с server-controlled grant/revoke/validation.
- `[D]` До стабилизации headless server, activation и multiplayer-authority boundary эту механику не реализуем.

---

## 14. Render / visual future

- `[x]` Render style считается исключительно client presentation policy и не должен менять authoritative world state.
- `[ ]` Два взаимоисключающих presentation style: technical/wire и anime/cel-shaded.
- `[ ]` Дальнейшие bloom/haze/grading/softening/vignette — после стабилизации основных runtime и map ownership границ.

---

## 14A. M8E.3 — архитектура durable authoritative universe

M8E.3 не делится на «сохранение аккаунтов» и отдельную позднюю «базу мира». Нужен один persistence subsystem с общими правилами версии/атомарности/восстановления, но с раздельными repositories, чтобы identity, player/ship ownership и universe state не слиплись в god-object.

**Фактический статус текущего кода:** M8E.3 пока **не начат как storage implementation**. В runtime ещё нет `PersistenceCoordinator`, `IAccountStore`, `IPlayerStore`, `IShipStore`, `IUniverseStore`, `IPasswordHasher`, durable `UniverseId` или `ShipContinuityRecord`. `AccountRegistry`, player/ship ownership и mutable universe state живут в памяти процесса; restart dedicated server не является восстановлением того же authoritative universe. То, что уже готово перед M8E.3 — только правильные domain IDs/registry boundaries, explicit auth protocol/UI, runtime lifecycle modes и client preferences/remembered-device seam.

Планируемая граница:

- `PersistenceCoordinator` — lifecycle checkpoint/load/recovery, не владеет gameplay rules;
- `IAccountStore` / identity repository — `AccountHandle -> password/device/recovery credential records -> AccountId -> PlayerId`; raw bearer token/password/recovery secret на серверный диск не пишется;
- `IPlayerStore` — persistent player/character record, owned/current `ShipInstanceId`, позже экономика/репутация/организации;
- `IShipStore` — durable concrete ship instances keyed by `ShipInstanceId`;
- `IUniverseStore` — universe metadata + изменяемые authoritative world records, scheduled events и lifecycle state;
- optional append-only journal поверх checkpoint store — для crash recovery без синхронной полной перезаписи мира на каждом tick.
- `IPasswordHasher` / credential security boundary — password 12–64 printable ASCII в первом UI, CSPRNG-generated 20+ character suggestion, unique salt + slow password KDF (Argon2id preferred), plaintext/fast unsalted password hashes запрещены;
- `AccountRecoveryService` — high-entropy one-time recovery secret (server stores digest only), password/device-token rotation и live-session invalidation; optional external `IRecoveryChannel` later.

Durable IDs и runtime handles разделяются жёстко. `AccountId`, `PlayerId`, `ShipInstanceId`, authored/static IDs и будущие stable dynamic object IDs могут быть ключами save. `EntityId` — только materialized runtime handle и после restart/rematerialization может измениться; persistence никогда не строится вокруг него.

Static deterministic content (`StarAtlas`, celestial definitions, ship/object definitions, localization) не дублируется в save. Checkpoint хранит schema version + content/catalog fingerprints и только mutable facts. Несовместимый catalog должен давать явную migration/incompatibility ошибку, а не тихо подменять мир.

Save-image снимается на authoritative fixed-step boundary как immutable snapshot. Disk I/O не выполняется на simulation thread: `ServerWorker/GameServer` формирует согласованный persistence image, persistence worker пишет его отдельно. Базовый формат обязан поддерживать atomic `temp -> replace`, backup/last-known-good и fail-loud policy при повреждённом существующем save; нельзя молча создать новый мир поверх corrupted universe.

Рекомендуемая разбивка M8E.3:

1. `[ ]` **M8E.3a — persistence kernel:** embedded transactional backend (первый кандидат SQLite behind interface), `PersistenceCoordinator`, schema migrations, `UniverseId`, save sequence/epoch, WAL/atomic checkpoint semantics, background persistence worker и test backend.
2. `[ ]` **M8E.3b — durable account/security:** full registration record, password KDF, remembered-device credentials, recovery secret/channel boundary, consent/locale fields, `ClientPreferencesStore`, explicit logout/revocation; `AccountId -> PlayerId -> owned/current ShipInstanceId` переживает restart независимо от login order.
3. `[ ]` **M8E.3c — ship continuity + dynamic checkpoint:** durable `ShipContinuityRecord` сохраняет canonical spatial/kinematic/condition state, cargo/resources/damage и `lastSimulatedUniverseTime`; disconnect не despawn/stop, restart восстанавливает тот же `ShipInstanceId` в том же физическом состоянии.
4. `[ ]` **M8E.3d — persistent world lifecycle/journal:** остальные dynamic objects/hubs/NPC facts, `Scheduled/Coarse/Prewarm/Active`, offline-owner interactions, coarse propagation, append journal + checkpoint compaction и explicit dedicated/local clock policies.
5. `[ ]` **M8E.3e — local save slots + session menus:** local manual Save/Load только по authoritative safe-zone/base policy; multiplayer continuously persists and has no rollback/load. ESC semantics distinguish Disconnect from Sign out.
6. `[ ]` **M8E.3f — restart/crash/security acceptance:** fly-away/restart restore, clean restart, interrupted write, corrupt newest checkpoint with backup recovery, login-order independence, password/device rotation, recovery flow, offline/coarse motion and materialize/dematerialize round-trip.

Local game и dedicated server должны использовать тот же schema/storage code, но разные save roots. Это не даёт local-mode сформировать вторую несовместимую модель мира.

### Ближайшие cross-cutting требования вокруг M8E.3

Перед и во время M8E.3 фиксируются следующие связанные задачи, чтобы не строить persistence поверх временного UI/auth контракта:

- `[x]` **UI platform / binary resource pack:** `ELITEUI1` + `UiResourcePack`, global Noto/font-license layer, reusable `EliteUiKit`, native `UiNavigationState`, non-secret `ClientPreferencesStore` и общий GameWebView service/session shell готовы. MainMenu больше не дублируется через legacy `HtmlUiManager`, старый `ConfirmExitState` удалён.
- `[~]` **Presentation atomicity / UI-map stability:** после покадровой Windows/DWM трассировки отказались от самой многослойной модели F1-F12. Теперь **вся активная игровая presentation domain использует один GLFW/OpenGL surface**: F1-F4 Flight, F5-F8 нативные service panels, F9-F12 Navigation вместе с правой STAR ATLAS панелью. `service_shell.html`, `service_panel.js`, `system_map_panel.html`, отдельный MapPanel `GameWebView`, JSON/`*_prepared` map-panel handshake и browser route для F5-F8 удалены. `SystemMapPanelPresentation` стал типизированной C++ presentation model, которую `InSessionPresentationRenderer` рисует в том же кадре; service placeholders также рисуются им напрямую. После первой single-surface миграции была выявлена функциональная регрессия: вместе с WebView ошибочно исчезли STAR ATLAS dropdown + map actions, а `prepareRequestedPresentation()` переигрывал committed F9 target каждый кадр и отменял terminal Galaxy cube -> System/empty-sector drill. Панель теперь использует semantic layer actions вместо фиксированных `toggle/DETAIL/HUB/CLOSE` слотов: Close удалён, текущий слой не дублируется, Hub показывает родительские `DETAIL -> SYSTEM/SPACE -> GALAXY`, а в System/Space выбор Hub активирует DETAIL и HUB. Empty-sector System layer называется SPACE; Details строится строго из `m_loadedSystemMapId`/selected terminal spatial cell и больше не подменяет отрицательный synthetic empty-sector id текущей системой игрока. Для непредельного выбранного куба сохраняется центральный terminal descendant. Direct System->Hub предварительно сохраняет точный parent Detail target, чтобы возврат Hub->Detail не зависел от истории переходов. `SystemMapPanelPresentation` выдаёт typed semantic actions, terminal Galaxy `MapIntent` сохраняется, а внутренние Galaxy/System/Detail/Hub переходы синхронизируют `GamePresentationCoordinator` через `adoptNavigationView()` без повторной F-key preparation. Добавлены acceptance/architecture/system-map guards именно на эти функции. Два `m_documentWebViews` остаются только для Main Menu / Loading / ESC Session Menu и больше не участвуют в F1-F12. Direct-selector/latest-request-wins, message-backed physical F-key edges, post-process full-clear и frame-bound `request -> prepare -> render -> swap -> commit` сохранены. Direct F9-F12 не используют outgoing map crossfade. Добавлен архитектурный invariant: активные F1-F12 не имеют права зависеть от WebView show/hide/navigation/z-order/DWM. Пункт остаётся `[~]` до полного MinGW gate и ручной проверки rapid F1-F12, Service<->Map, Map<->Flight, ESC->F, native map-panel actions, terminal cube drill и resize; native complex-script font fallback для перенесённых C++ панелей остаётся отдельным UI follow-up.
- `[x]` **Map tactical overlay follow-up:** crowded tactical picks are now size-ranked rather than insertion/nearest-first: within one direct hit cluster the physically largest object wins, so Hub beats overlapping ships and a directly hit planet/moon can beat overlapping Hub/ship glyphs on System Map. Direct player-Hub entry now materializes parent System + Details context before Hub; Hub panel parent buttons restore loaded context first and fall back to player context only when a parent was never available. Detail now gives Hub the same temporary kilometre-scale presentation envelope as System, so the diagnostic analytic cube/ships cannot outrank it merely because Hub size was missing. Hub glyphs are visually distinct compact cubes outside Hub Map, localized card labels wrap by measured width, and the first graphical client now owns stable `localhost:8090` directly while additional clients keep isolated ephemeral WebUI endpoints; the compatibility redirect remains a fallback for dynamic clients, and application startup still prints the exact direct URL in the form `http://localhost:<port>/debug_control.html`. Tactical selection is now independent from card lifetime: one active object can be restored by clicking any open card, Hub focus re-enables Hub navigation, and velocity-arrow length uses bounded logarithmic speed scaling. Guards/tests lock these rules.
- `[~]` **Bundled font coverage:** добавлены pinned Noto manifest/fetcher, OFL/license registry, WebUI fallback chain для Latin/Cyrillic/Greek, CJK SC/TC/HK/JP/KR, Arabic/Hebrew, major Indic/SE Asian/Armenian/Georgian/Ethiopic + symbols/emoji и RTL locale metadata. Font binaries fetchятся локально перед release и пакуются в `elite_ui.pak`; native complex-script shaping/FontRegistry остаются отдельным follow-up.
- `[~]` **Full registration form:** отдельный REGISTER view уже содержит целевой DisplayName, AccountHandle, password/confirmation, recovery, locale и consent shape. Пока server-side durable account security отсутствует, security/profile/consent поля fail-closed и не передаются; live остаётся только явно помеченный development AccountHandle + remembered-device bootstrap.
- `[x]` **Remembered device + last account:** `ClientPreferencesStore` запоминает последний успешно вошедший AccountHandle на конкретном server endpoint и preferred locale, а Windows Credential Manager хранит секретный remembered-device credential. Password/recovery в preferences не пишутся.
- `[ ]` **Explicit sign-out:** обычный disconnect/quit сохраняет remembered-device credential; `Sign out` удаляет/ревокает его и требует password/recovery при следующем входе.
- `[ ]` **Display-name moderation:** normalization + obfuscation-resistant deterministic rules + optional moderation/ML provider; result `Allow/Reject/SuggestAlternative`. Persisted имя не изменяется молча. Сервис позже переиспользуется для ship/org/public names.
- `[ ]` **Commerce seam:** `CommerceService -> IEntitlementProvider` закладывается как отдельная provider boundary; game server не хранит raw payment instruments и не связывает payment processing с gameplay identity. Реальная платежная интеграция не блокирует M8E.3.
- `[~]` **ESC/session-menu client shell:** Local и Multiplayer физически разделены на `local_session_menu.html` и `multiplayer_session_menu.html`. Pause policy отделена от presentation: ESC в Local немедленно останавливает embedded authoritative session (`SpaceState::update -> LocalGameSession::advance`) до Resume/выбора F1-F12; ESC в Multiplayer мир не останавливает. F1-F12 никогда не меняют simulation clock. Service и multiplayer session overlays продолжают update мира, но публикуют neutral manual input, чтобы сервер не удерживал последнюю тягу/rotation после перехвата keyboard focus. ESC остаётся dual-path (native foreground latch + DOM forwarding) и безопасен благодаря idempotent target requests. Save/Load и explicit Sign out остаются fail-closed; ручная Windows focus/pause/rapid-selector acceptance обязательна до `[x]`.
- `[ ]` **Offline-owned ship continuity:** logout neutralizes manual input, но не velocity/world state; корабль продолжает существовать для других игроков/NPC и может перейти Active -> Coarse/Scheduled без потери `ShipInstanceId`.
- `[ ]` **Clock policy:** dedicated universe в итоге поддерживает coarse/scheduled wall-clock catch-up через downtime; local save по умолчанию замораживает universe time пока игра закрыта.


---

## 15. Крупные будущие блоки и текущий приоритет

Базовая localization platform и sky cultures закрыты, но новые native UI функции обязаны сразу идти через общий localization storage; после Route Plan был устранён кратковременный regression с hardcoded map/route translations. Крупная дорожная карта содержит несколько связанных архитектурных направлений плюс отдельные будущие gameplay/content blocks:

1. `[x]` **Client/server presentation migration основных карт + headless server boundary** — StarAtlas ownership, Galaxy/System/Details/Hub composition и отдельный `EliteServer` target готовы.
2. `[~]` **Multiplayer transport/session foundation** — M1–M8E.2f готовы, graphical two-client/reconnect/auth-form baseline вручную подтверждён. M8E.2g закрывает account-handle validation, dev reset и responsive authorization UI перед persistence.
3. `[~]` **UI/auth client platform + single-surface in-session presentation — финальный client-side polish перед M8E.3.** Binary `elite_ui.pak`, WebUI font/license registry, `EliteUiKit`, `ClientPreferencesStore` и `GamePresentationCoordinator` готовы. F1-F12 проходят через message-backed `Window` queue -> direct router -> one OpenGL scene surface: Flight F1-F4, native Services F5-F8, Navigation F9-F12. Browser front/back surfaces теперь ограничены Main Menu / Loading / ESC и не участвуют в игровых F-переходах. Local ESC pause и Multiplayer non-pause остаются отдельной session policy. Текущая acceptance-задача — полный MinGW gate и ручная Windows проверка отсутствия split-generation/background кадров на rapid F1-F12/Service<->Map/ESC transitions, а также native font coverage для перенесённых C++ панелей. Password/recovery/sign-out backend ждёт M8E.3b.
4. `[ ]` **M8E.3 durable authoritative universe persistence — главный незакрытый server/world фундаментальный track.** Он идёт отдельно от текущего client-navigation trajectory track; PATCH A уже закрыт. Persistence проектируется сразу для identity + world, а не как отдельная временная account DB: stable AccountHandle/password/device/recovery records + `AccountId/PlayerId/ShipInstanceId` ownership, universe epoch, dynamic ship/object/NPC records и lifecycle state живут за versioned storage boundary; transient `EntityId` не сохраняется как durable key.
5. `[~]` **Persistent universe execution: реальные корабли + Scheduled/Coarse/Prewarm/Active materialization** — Stage 4A/4B materialized execution и coarse motion-control cadences готовы; M8E.3 даст durable records/checkpoints, после чего Scheduled lifecycle/materialize-collapse сможет работать поверх постоянного universe state.
6. `[ ]` **Навигационный compass / azimuth / elevation + модель определения абсолютных координат**.
7. `[ ]` **J и полноценный inter-system / multi-system runtime**.
8. `[ ]` **On-demand trajectories + модель знания игрока о маршрутах/истории движения**.
9. `[ ]` **Lemmings-like group rescue / evacuation gameplay** — отдельный будущий realtime-сценарий: например аварийный generation ship, где игрок не микроменеджит каждого человека, а проводит группу выживших к шлюзу коллективными командами/ограничениями. Сценарий должен быть multiplayer-safe и не требовать глобально уникального набора NPC: такой квест можно выдавать игрокам независимо в разных местах/экземплярах, обычно один раз на конкретный сюжетный эпизод.

Первые восемь направлений образуют связанную навигационно-информационную модель мира. Lemmings-like rescue — отдельный gameplay/content block и не должен тормозить текущую серверную архитектуру.

### Следующий рабочий блок

На текущем этапе:

1. Локализация — **platform закрыта; Route/map cleanup синхронизирован**: все текущие native Route/Object overlay строки снова берутся из `assets/localization`, renderer-side language branches запрещены guard'ами.
2. Звёздное небо/созвездия — **закрыты**.
3. `Ctrl+F10` flight-mode switching — **исправлено**.
4. Client/server presentation ownership основных карт — **закрыт на текущем этапе**.
5. Headless `EliteServer` executable — **готов**; ready harness отдельно конфигурирует его без client/render dependencies и запускает authoritative smoke.
6. Stage M7 — **готов**: per-session sparse ship publication реально включён, а full bootstrap/re-entry hydration и explicit lifecycle работают поверх canonical retained server state.
7. Stage M8A — **готов**: зафиксирован ABI-independent, versioned reliable-byte-stream wire framing и control-plane serialization без Win32/POSIX деталей.
8. Stage M8B — **готов**: `SimulationSnapshot` + `MapResponse` проходят canonical ordered schema -> raw bytes -> schema-blind compression envelope -> framing; adding a field normally touches DTO + `WireDataSchema.h` + schema-version/test, но не TCP/compressor/ServerRunner.
9. Stage M8C — **готов на transport boundary**: standalone Asio `TcpWireStream` переносит только `WireMessageKind + opaque payload`, typed adapters реализуют существующие transport interfaces, а localhost contract проверяет обе стороны реальным kernel TCP.
10. Stage M8D — **готов на process boundary**: dedicated `ServerRuntime` может стартовать без synthetic primary connection; `NetworkServerHost` принимает TCP и делает server-owned entity admission; `RemoteGameSession` не содержит embedded server; `EliteServer --listen` и `EliteGame --connect` работают как отдельные процессы. Ready harness проверяет реальный two-process bootstrap/input/disconnect. Operational contract и команды запуска зафиксированы в `SERVER_CLIENT.md`.
11. Stage M8E graphical baseline — **ручная проверка пройдена**: два отдельных `EliteGame --connect` одновременно работают на одном dedicated server, получают разные persistent player/ship identities и видят друг друга на расстоянии 50 м. Process-local WebUI/WebView2 и Win32 GLFW event-pump issue закрыты на текущем Windows runtime уровне.
12. Persistent identity Phase 1 — **готов как in-memory backbone**: `PlayerId`, `ShipInstanceId`, `ServerSessionId`, `EntityId`, `PlayerRegistry`, `ShipInstanceRegistry`, `ControlRegistry`, session welcome/snapshot identity fields. M8E.2 добавил explicit account sign-in/register поверх этого backbone; durable authoritative universe storage всё ещё отсутствует.
13. Canonical build/runtime recovery — **закрыт**: runtime paths единичны (`build/EliteGame.exe`, `build/headless_server/EliteServer.exe`), scratch builds изолированы под `build/tests/`, полный MinGW ready gate и ручной reconnect/two-client acceptance зелёные.
14. Stage M8E.2 explicit authentication/admission — **protocol/auth boundary готов**: AccountHandle server-known, typed rejection/REGISTER/SIGN IN работают; password/recovery/sign-out durability ждут M8E.3b.
15. Route Plan — **PATCH A/PATCH B закрыты**. PATCH C shared `TrajectoryPredictor` реализован: reusable kinematic samples, gravity/proper-acceleration split и G/jerk diagnostics отделены от Route Plan/server/render. Текущий vertical slice использует эти контракты для request-driven docking guidance с map planned-path + cockpit GuidanceTunnel; после его live-валидации идут swept-volume/local avoidance, затем общий route/intercept solver.
16. **M8E.3 durable authoritative universe persistence остаётся незакрытым server/world фундаментом и кодом ещё не начат.** Он не блокирует client-side ownership cleanup/predictor prototype, но должен быть завершён до того, как durable autopilot/orders, offline ship continuity и cross-restart route/formation state будут считаться production-механикой.

---

## 16. Когда обновлять

Обновлять `PROJECT_STATE.md`, когда:

- закрыт этап;
- принято новое фундаментальное решение;
- меняется ближайший приоритет;
- сознательно откладывается важная задача;
- обнаружено новое архитектурное ограничение, которое будущая работа не должна забыть.

Подробности реализации должны оставаться в коде, специализированных MD и regression/architecture/acceptance tests. Этот файл отвечает только на вопрос: **«где сейчас проект и куда мы идём дальше?»**

### Navigation Guidance Layer — Wave 4

- `[x]` Navigation features have a shared modular enable/disable seam via
  `NavigationModuleState`: physics/planning/source modules and HUD layers are
  independent. Target markers, Route markers, Guidance Corridor, Galactic
  Compass and flight-vector HUD can be hidden independently without disabling
  safety computation; `SpaceState` exposes stable set/toggle methods for future
  cockpit controls rather than consuming new arbitrary hotkeys.
- `[x]` `NavigationPlanningSnapshot` formalizes official lanes/beacon coverage,
  moving obstacles, restricted volumes, scheduled large-vessel traffic and
  uncertainty. `NavigationPlanningSnapshotBuilder` is the radar/transponder/
  beacon refinement seam; sensor precision may improve kinematics but cannot
  shrink authoritative physical/separation envelopes.
- `[x]` `TrajectorySafetyEvaluator` performs time-aware closest-approach checks
  against moving obstacles, restricted volumes and scheduled traffic. Traffic
  exists only in its published window plus timing uncertainty and is not frozen
  forever at its final schedule point.
- `[x]` `GuidanceCorridor` is the universal time-aware visual/operational path
  product for RouteSolver, local planner, docking computer, server ATC,
  mission/fleet and emergency sources. HUD presentation uses a short sliding
  time window, making the same frames useful as docking guidance and as motion
  feedback during long autopilot flight.
- `[x]` `LocalGuidancePlanner` V1 reuses `TrajectoryPredictor` +
  `TrajectorySafetyEvaluator` for a direct moving-target candidate and, on a
  conflict, tries simple left/right lateral detour candidates through that same
  prediction+safety pipeline. If neither candidate is safe it returns `Blocked`;
  more advanced obstacle search remains a planner strategy, never predictor logic.
- `[x]` Hub docking/landing/attack/etc. construction semantics are separated
  from mesh geometry through `HubSemanticAnchor`. Diagnostic rotating cube and
  cylinder dock meshes live only in `assets/models/hub/guidance_test/`; semantic
  gates live in `assets/data/navigation/hub_semantic_anchors.json` and can
  survive future mesh replacement.
- `[x]` Galactic Compass foundation uses standard galactic longitude `l` and
  latitude `b`, GC/GAC and NGP/SGP, reading the same Milky Way orientation data
  as the galaxy renderer. It reports ship-nose orientation; actual velocity
  remains the separate flight-vector instrument.
- `[x]` Hub Motion Lab now has a live local-guidance producer: rotating semantic
  docking gates are resolved from replicated module pose/angular velocity, a
  short-lived planning snapshot is built, and accepted local corridors are
  republished about every 0.2 s. Known blocking diagnostic modules participate
  as moving conservative obstacles.
- `[~]` **Current docking-guidance slice:** live trace exposed two concrete
  calculation/presentation errors and the current work is intentionally narrowed
  to **solver + map trajectory**; HUD is not being changed in this slice. The old
  30-second solution integrated the ship through system gravity while extrapolating
  the moving Hub/dock along its instantaneous world-space tangent, then declared
  `Ready` on collision safety alone and visually snapped the last corridor frame
  to the dock despite a kilometre-scale physical miss. The target now receives a
  short-horizon Hub co-moving ephemeris, the local planning horizon can extend to
  120 seconds, iterative shooting corrects the candidate after real predictor
  integration, and `Ready` additionally requires terminal position/relative-speed
  convergence. The raw predictor end is never teleported to the target. For live
  validation System/Detail/Hub maps render literal planner samples as one plain
  solid non-blinking line without Bezier smoothing or sample/endpoint decorations;
  Hub Map transforms each future sample through the predicted Hub frame at the
  same time, removing common orbital motion. The first physical sample is retained,
  rotating attached modules now use the same universe-time phase as the planner,
  and a single near-threshold `NoTerminalSolution` replan receives only a bounded
  0.65 s presentation grace instead of blinking the last accepted path off. Raw
  terminal diagnostics remain in `docking_guidance_trace.txt`. Existing info-card/
  selection lifecycle is not part of this change. No `TrajectoryFollower`/autopilot
  is connected.
- `[ ]` **Next after live validation:** oriented/swept-volume collision checking
  beyond the first conservative vehicle sphere, richer local avoidance, then
  long-range RouteSolver + real radar/transponder/beacon fusion/server ATC seams.
  `TrajectoryFollower` stays deferred until the displayed docking trajectory and
  GuidanceTunnel are accepted.
