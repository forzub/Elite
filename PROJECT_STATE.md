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

**Общий статус: `[~]` фундамент разделения и presentation ownership основных карт готовы; следующий этап — runtime scaling/multi-system separation.**

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
6. `[~]` Runtime scaling: Stage 4A/4B уже применяют `Active / Prewarm / Coarse` к реальной стоимости materialized ships. AI/service/maintenance cadence снижена, а дорогой motion-control solver отделён от дешёвой fixed-step kinematic propagation. **Sparse replication временно отложена до multiplayer session/interest foundation**, потому что cadence/omission должна определяться отдельно для каждого подключённого клиента.

### Multiplayer / session authority foundation

**Статус: `[~]` Stage M2 server-side multi-session transport fan-out реализован; следующий риск — корректно отделить локально управляемого human ship от remote human ships на клиенте, затем per-client interest.**

Server runtime уже принимает несколько transport/session endpoints в одном authoritative execution context. При этом часть старых single-player compatibility state (`m_playerId`, primary `m_playerNavigation`, один global active celestial-system context) пока сознательно остаётся и должна удаляться отдельными этапами, а не маскироваться.

- `[x]` Stage M1: введён server-owned `ServerSessionId` и platform-neutral `ServerSessionRegistry`; session identity отделён от `EntityId`.
- `[x]` Stage M1: каждая зарегистрированная session имеет authoritative `controlledEntityId`; `GameServer::receiveClientMessage()` сначала разрешает session -> entity, поэтому клиентский packet не выбирает произвольный корабль.
- `[x]` Stage M1: NPC/activation authority больше не определяет игрока через singleton `m_playerId`; simulation хранит явный set player-controlled entities, они исключены из NPC authority и pinned `Active`. `m_playerId` пока остаётся compatibility alias для старых single-player navigation/diagnostic paths.
- `[x]` Stage M2: один `ServerRunner` обслуживает несколько `(IServerTransport*, ServerSessionId)` bindings: сначала принимает inbound со всех sessions, затем выполняет **ровно один** authoritative `GameServer::update()`, после чего fan-out'ит ответы обратно по session ownership.
- `[x]` Stage M2: `ServerRuntime` умеет authoritative admission/detach вторичной player session, публикует ей собственный `SessionWelcome` и bootstrap snapshot; secondary disconnect не останавливает primary session.
- `[x]` Stage M2: map request/response сохраняет destination `ServerSessionId`, time-sync отвечает через тот же connection endpoint; cross-session response leakage запрещён regression guard'ом.
- `[x]` Stage M2: обычный world snapshot пока остаётся full-world/full-presence, но `snapshot.session.playerNavigation` собирается отдельно для `controlledEntityId` каждой session. Это уже устраняет передачу primary-player navigation второму клиенту, хотя primary `m_playerNavigation` ещё остаётся серверным compatibility state для текущего single-system runtime.
- `[x]` Stage M2: headless `EliteServer --self-test` поднимает две transport sessions в одном runtime, назначает два разных controlled ships, проверяет независимые control ticks, map/time-sync routing, per-session navigation и disconnect secondary session.
- `[x]` Registry защищён от stale reconnect: старая disconnected session не может вернуть authority, если тем же кораблём уже владеет новая connected session.
- `[ ]` Stage M3: на клиенте перестать использовать `ShipRole::Player` как признак **локально** управляемого корабля. Истина для prediction/input/local presentation — `SessionWelcome.controlledEntityId`; remote human ship должен интерполироваться как remote entity, а не становиться вторым «моим» кораблём.
- `[ ]` После M3 убрать/разделить оставшиеся singleton navigation assumptions: authoritative navigation/current-system view должен быть per-session/per-controlled-entity; `m_playerNavigation` не должен определять мир для нескольких игроков.
- `[x]` Local loopback остаётся нормальной односессионной реализацией того же protocol boundary; обычный `EliteGame.exe` не требует внешнего сервера.
- `[ ]` Разделить **simulation activation** и **replication interest**: сущность может быть `Active` из-за Player A, но для Player B иметь редкий cadence или вообще не входить в его current interest set.
- `[ ]` После session/local-player identity foundation реализовать per-client interest, затем sparse/delta replication с явной семантикой retain/update/remove; временное отсутствие entity в пакете не означает уничтожение.
- `[~]` Multiplayer contract уже покрывает две server sessions/two controlled ships и независимый routing; полноценный two-`GameClient` regression добавлять после M3 local-vs-remote human identity.
- `[ ]` Добавить настоящий network transport + connection accept/auth/admission lifecycle; конкретный socket/wire protocol выбирать отдельно от `GameServer`/`ServerRuntime`.
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
- `[x]` Stage 4B сознательно оставляет signals и присутствие сущности в обычных snapshots full-rate/full-presence; sparse replication требует отдельной семантики omission/retention.
- `[~]` Activation ещё не является полной системой снижения стоимости мира: **сначала multiplayer session/interest foundation**, затем sparse replication и Scheduled materialization/collapse.
- `[ ]` Корабль, летящий месяц между точками, не должен считаться полноценной физикой 50 раз/сек.
- `[ ]` При приближении игрока дальняя сущность materialize в правильной точке своей траектории и получает полноценную физику/NPC/повреждения/столкновения.
- `[ ]` При удалении сущность должна снова collapse в coarse/scheduled representation без потери persistent identity, состояния и истории.
- `[ ]` Перевести production ships/hubs/modules на новую runtime policy системно.

---

## 10. Persistent ships

**Статус: `[~]` модель принята, полноценные persistent records ещё не завершены.**

Корабли мира должны быть в основном реальными persistent-сущностями, а не бесконечно рождающимся NPC-фоном.

Нужно чётко разделять три уровня:

```text
typeId
    Cobra Mk.I

persistent ship identity
    ShipPersistentId
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
- `[ ]` Завершить persistent ship records и их lifecycle.
- `[ ]` Один persistent ship может существовать без materialized runtime entity в `Scheduled/Coarse` состоянии.
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

Локализация и sky cultures на текущем этапе уже закрыты. Крупная дорожная карта содержит пять связанных направлений; первое из них теперь также закрыто на текущем архитектурном уровне:

1. `[x]` **Client/server presentation migration основных карт + headless server boundary** — StarAtlas ownership, Galaxy/System/Details/Hub composition и отдельный `EliteServer` target готовы.
2. `[~]` **Multiplayer session/player authority foundation** — Stage M1/M2 (`ServerSessionId`, registry, session->controlled-entity authority, multi-player activation ownership, multi-transport fan-out и two-session authoritative smoke) готовы; далее M3 local-vs-remote human identity на клиенте, затем per-client replication interest.
3. `[~]` **Persistent universe: реальные корабли + Scheduled/Coarse/Prewarm/Active materialization** — Stage 4A/4B materialized execution и coarse motion-control cadences готовы; после multiplayer foundation идут sparse replication и Scheduled lifecycle.
4. `[ ]` **Навигационный compass / azimuth / elevation + модель определения абсолютных координат**.
5. `[ ]` **J и полноценный inter-system / multi-system runtime**.
6. `[ ]` **On-demand trajectories + модель знания игрока о маршрутах/истории движения**.

Эти направления образуют одну связанную навигационно-информационную модель мира, а не набор независимых фич.

### Следующий рабочий блок

На текущем этапе:

1. Локализация — **закрыта**.
2. Звёздное небо/созвездия — **закрыты**.
3. `Ctrl+F10` flight-mode switching — **исправлено**.
4. Client/server presentation ownership основных карт — **закрыт на текущем этапе**.
5. Headless `EliteServer` executable — **готов**; ready harness отдельно конфигурирует его без client/render dependencies и запускает authoritative smoke.
6. Текущий следующий технический шаг — **Multiplayer Stage M3: local-controlled identity на клиенте**. `SessionWelcome.controlledEntityId` должен стать единственным признаком того, какой human ship получает local input/prediction; остальные human ships остаются remote/interpolated. Затем — per-session navigation cleanup, per-client interest + sparse replication semantics, после них `Scheduled <-> Coarse <-> Prewarm <-> Active` materialization/collapse. Обычный snapshot не должен трактовать временно неотправленную coarse entity как уничтоженную.

---

## 16. Когда обновлять

Обновлять `PROJECT_STATE.md`, когда:

- закрыт этап;
- принято новое фундаментальное решение;
- меняется ближайший приоритет;
- сознательно откладывается важная задача;
- обнаружено новое архитектурное ограничение, которое будущая работа не должна забыть.

Подробности реализации должны оставаться в коде, специализированных MD и regression/architecture/acceptance tests. Этот файл отвечает только на вопрос: **«где сейчас проект и куда мы идём дальше?»**
