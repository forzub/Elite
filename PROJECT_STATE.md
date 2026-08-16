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

- `[x]` System-map production infrastructure/hubs теперь берутся из ordinary authoritative `SimulationSnapshot` history на exact response epoch и client-composed; map response больше не является вторым production world-state каналом.
- `[x]` Details client-composed из endpoint-local StarAtlas/celestial state + exact-epoch ordinary replication; server-built Detail presentation DTO удалён.
- `[x]` Hub client-composed из parent celestial state + exact-epoch hub/modules/ships replication; server-built Hub presentation DTO удалён.
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
7. `[~]` **M8E.1 recovery gate (2026-08-16):** graphical two-client world/identity separation работает, но найден reconnect control-epoch defect: `GameClient::controlTick` начинается заново для нового процесса, тогда как `GameServer::m_controlStreams` жил по persistent materialized `EntityId`. Это давало prediction -> authoritative rollback до тех пор, пока новый tick не догонял старый. Исправление должно сбрасывать session-owned input/one-shot queues и neutralize continuous control на session create/disconnect; regression обязан запускать новый `GameClient` для того же `PlayerId/ShipInstanceId/EntityId` и доказать fresh epoch.
8. `[~]` После закрытия M8E.1 — проверить Win32 `WM_NCLBUTTONDOWN` modal-loop stalls отдельно от networking. Native event pump устранил cross-process GLFW AV, но `DispatchMessageW(WM_NCLBUTTONDOWN/HTCAPTION)` всё ещё может удерживать UI thread на 0.1–2.5 s при манипуляции окном. Это не должно смешиваться с reconnect/reconciliation диагнозом.
9. `[ ]` Только после зелёного two-process control/reconnect gate продолжать authorization/persistent-world backbone и durable ship registry; не строить следующий слой поверх нестабильного session-control lifecycle.

### Multiplayer / session authority foundation

**Статус: `[~]` transport/process foundation дошёл до реального graphical two-client baseline. Один dedicated `EliteServer` одновременно обслуживает два отдельных `EliteGame`; оба получают разные server-owned player/ship identities и находятся в одном authoritative world. Однако acceptance ещё нельзя считать закрытым по управлению: 2026-08-16 найден mismatch lifetime между session-local `controlTick` и EntityId-owned fixed-step queue, проявляющийся как слабая локальная реакция/rollback и последующее внезапное восстановление после догоняния старого tick. До зелёного reconnect-control regression это P0. Persistent identity Phase 1 уже разделяет `PlayerId`, `ShipInstanceId`, `ServerSessionId` и materialized `EntityId`; полноценные resume semantics и durable storage всё ещё впереди.**

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
- `[x]` Stage M8E graphical baseline: WebUI endpoint process-local через OS-assigned ephemeral port, WebView2 user-data folder изолирован по process, remote client умеет ждать ещё не запущенный server, а `EliteServer` защищён OS-level singleton guard. Диагностика доказала GLFW 3.4 Win32 cross-process AV: `_glfwPollEventsWin32()` мог получить active HWND другого `EliteGame` и разыменовать его `GLFW` property как локальный `_GLFWwindow*`. На Windows client event pump теперь использует native `PeekMessage/TranslateMessage/DispatchMessage` path и не входит в опасный GLFW post-poll path; два graphical клиента после этого держатся одновременно стабильно.
- `[x]` Identity backbone Phase 1: dedicated bootstrap создаёт два persistent `PlayerId`/`ShipInstanceId` bindings, materialized ships разведены на 50 м, `SessionWelcome` несёт `playerId + controlledShipInstanceId + controlledEntityId`, а `ShipSnapshot` несёт `instanceId`. Ручной graphical acceptance подтвердил: оба клиента видят оба корабля в одном authoritative world; прежний симптом «второго игрока нет / параллельные реальности» исчез после исправления identity structure.
- `[~]` Текущая admission policy всё ещё временная: свободный `PlayerId` выдаётся server-side по фактическому accept order («кто первый — того и слот»). Это не production identity. Следующий этап — полноценная account/player authorization skeleton, one-live-session-per-player rule, server-owned reconnect/resume и durable ship ownership; не вводить временный client-selected `--player`/`--entity` authority shortcut.
- `[ ]` Отдельно добавить Linux headless build/smoke для `EliteServer`; текущий server/session/runtime код остаётся platform-neutral, а Win32/POSIX детали должны жить только в transport/platform adapters.
- `[ ]` Несколько игроков в разных звёздных системах требуют последующего multi-system runtime; multiplayer session foundation не должна снова зашивать один global `m_activeCelestialSystemId`.

---

## 6. Навигация и определение положения

**Статус: `[ ]` концепция определена частично, реализация впереди.**

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

**Статус: `[ ]` отдельная крупная механика, практически ещё не реализована.**

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

**Статус: `[~]` presentation ownership migration основных карт завершена; остаются отдельные функциональные возможности карт.**

- `[x]` Есть Galaxy / System / Details / Hub modes.
- `[x]` Navigation grid/cube hierarchy является базовой системой адресации/навигации.
- `[x]` Galaxy/System/Details/Hub production presentation собирается клиентом из local catalogs/celestial state + authoritative replicated facts/epochs.
- `[x]` Map-specific production infrastructure/hub state не является параллельным world-state каналом.
- `[x]` Тяжёлые server-built Detail/Hub presentation DTO удалены.
- `[ ]` Полноценная selection кораблей на System Map -> Details.
- `[ ]` После завершения migration добавить trajectory presentation как отдельный независимый слой карт.

Целевая схема:

```text
SERVER
authoritative facts / IDs / state

        ↓

CLIENT
local catalogs
+ replicated runtime
+ metadata
+ deterministic celestial state

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

## 15. Крупные будущие блоки и текущий приоритет

Локализация и sky cultures на текущем этапе уже закрыты. Крупная дорожная карта содержит несколько связанных архитектурных направлений плюс отдельные будущие gameplay/content blocks; первый архитектурный блок уже закрыт на текущем уровне:

1. `[x]` **Client/server presentation migration основных карт + headless server boundary** — StarAtlas ownership, Galaxy/System/Details/Hub composition и отдельный `EliteServer` target готовы.
2. `[~]` **Multiplayer transport/session foundation** — M1–M8D готовы, а graphical two-client baseline вручную подтверждён: два отдельных клиента работают на одном dedicated authoritative world и видят друг друга. Остались disconnect/reconnect hardening и полная auth identity.
3. `[~]` **Player/ship/control identity + authorization backbone — текущий приоритет.** Phase 1 уже отделил `PlayerId`, `ShipInstanceId`, `ServerSessionId`, `EntityId` и ввёл `PlayerRegistry`/`ShipInstanceRegistry`/`ControlRegistry`. Следующий шаг — server-owned `AccountId -> PlayerId -> current/owned ShipInstanceId -> session/control authority`, без временного client-selected player/entity shortcut.
4. `[~]` **Persistent universe: реальные корабли + Scheduled/Coarse/Prewarm/Active materialization** — Stage 4A/4B materialized execution и coarse motion-control cadences готовы; после durable ship/player ownership registry возвращаемся к Scheduled lifecycle/materialize-collapse.
5. `[ ]` **Навигационный compass / azimuth / elevation + модель определения абсолютных координат**.
6. `[ ]` **J и полноценный inter-system / multi-system runtime**.
7. `[ ]` **On-demand trajectories + модель знания игрока о маршрутах/истории движения**.
8. `[ ]` **Lemmings-like group rescue / evacuation gameplay** — отдельный будущий realtime-сценарий: например аварийный generation ship, где игрок не микроменеджит каждого человека, а проводит группу выживших к шлюзу коллективными командами/ограничениями. Сценарий должен быть multiplayer-safe и не требовать глобально уникального набора NPC: такой квест можно выдавать игрокам независимо в разных местах/экземплярах, обычно один раз на конкретный сюжетный эпизод.

Первые семь направлений образуют связанную навигационно-информационную модель мира. Lemmings-like rescue — отдельный gameplay/content block и не должен тормозить текущую серверную архитектуру.

### Следующий рабочий блок

На текущем этапе:

1. Локализация — **закрыта**.
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
12. Persistent identity Phase 1 — **готов как in-memory backbone**: `PlayerId`, `ShipInstanceId`, `ServerSessionId`, `EntityId`, `PlayerRegistry`, `ShipInstanceRegistry`, `ControlRegistry`, session welcome/snapshot identity fields. Это ещё не account/login и не durable universe database.
13. **Следующий рабочий блок — полноценная server-owned authorization/identity skeleton**, а не временный CLI-костыль выбора игрока: `AccountId -> PlayerId -> owned/current ShipInstanceId -> ServerSessionId -> ControlAuthority`, duplicate-login/reconnect policy и persistent storage boundary. После фиксации этого слоя начинаем реально населять вселенную и подключать `Scheduled <-> Coarse <-> Prewarm <-> Active` materialization/collapse к реестру всех кораблей.

---

## 16. Когда обновлять

Обновлять `PROJECT_STATE.md`, когда:

- закрыт этап;
- принято новое фундаментальное решение;
- меняется ближайший приоритет;
- сознательно откладывается важная задача;
- обнаружено новое архитектурное ограничение, которое будущая работа не должна забыть.

Подробности реализации должны оставаться в коде, специализированных MD и regression/architecture/acceptance tests. Этот файл отвечает только на вопрос: **«где сейчас проект и куда мы идём дальше?»**
