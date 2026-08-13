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

**Общий статус: `[~]` фундамент разделения готов, presentation migration продолжается.**

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

### Текущая незаконченная граница

- `[~]` Migration Stage 3: map-specific infrastructure/hub metadata ещё частично server-built.
- `[~]` Detail map DTO всё ещё в значительной степени строится сервером.
- `[~]` Hub map DTO всё ещё в значительной степени строится сервером.
- `[~]` StarAtlas считается client-owned static data концептуально, но текущий protocol всё ещё содержит `StarAtlasRequest` / presentation-data response path. Это следующий очевидный ownership cleanup.
- `[ ]` Отдельный headless `EliteServer` executable target. Compile seam уже подготовлен, но отдельного серверного бинарника ещё нет.

### Ближайший рекомендуемый порядок

1. `[ ]` Убрать StarAtlas payload из транспорта: client/server независимо загружают одинаковый static catalog; handshake проверяет version/hash.
2. `[ ]` Завершить System-map migration: infrastructure/hub identities/state приходят обычной authoritative entity replication, а presentation собирается клиентом.
3. `[ ]` Перевести Details на client-side composition из authoritative entity state + local static/celestial data.
4. `[ ]` Перевести Hub на ту же модель.
5. `[ ]` Создать отдельный headless `EliteServer` target и использовать compile/link boundary как жёсткую проверку server independence от render/UI.

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
- `[~]` Activation policy уже влияет на часть AI cadence, но ещё не является полной системой снижения стоимости physics/signals/replication.
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

**Статус: `[~]` migration уже идёт, но server-built presentation DTO ещё не устранены полностью.**

- `[x]` Есть Galaxy / System / Details / Hub modes.
- `[x]` Navigation grid/cube hierarchy является базовой системой адресации/навигации.
- `[x]` Часть Galaxy/System composition уже перенесена на клиент.
- `[~]` Постепенно убрать с сервера map-specific infrastructure и stations/hubs там, где их можно собрать из runtime + persistent metadata.
- `[~]` Убрать оставшиеся тяжёлые Detail/Hub map DTO и закончить client-side composition.
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

Локализация и sky cultures на текущем этапе уже закрыты. После нынешней архитектурной миграции остаются пять крупных связанных направлений:

1. `[~]` **Завершить client/server presentation migration** — StarAtlas ownership, System/Details/Hub composition, headless server target.
2. `[ ]` **Навигационный compass / azimuth / elevation + модель определения абсолютных координат**.
3. `[ ]` **J и полноценный inter-system runtime**.
4. `[ ]` **On-demand trajectories + модель знания игрока о маршрутах/истории движения**.
5. `[~]` **Persistent universe: реальные корабли + Scheduled/Coarse/Prewarm/Active materialization**.

Эти направления образуют одну связанную навигационно-информационную модель мира, а не набор независимых фич.

### Следующий рабочий блок

На текущем этапе:

1. Локализация — **закрыта**.
2. Звёздное небо/созвездия — **закрыты**.
3. `Ctrl+F10` flight-mode switching — **исправлено**.
4. Следующий фокус — **продолжение client/server presentation migration**.
5. Первый рекомендуемый технический шаг — **убрать StarAtlas из transport payload и сделать static catalog действительно локально загружаемым на обеих сторонах с version/hash compatibility check**.

---

## 16. Когда обновлять

Обновлять `PROJECT_STATE.md`, когда:

- закрыт этап;
- принято новое фундаментальное решение;
- меняется ближайший приоритет;
- сознательно откладывается важная задача;
- обнаружено новое архитектурное ограничение, которое будущая работа не должна забыть.

Подробности реализации должны оставаться в коде, специализированных MD и regression/architecture/acceptance tests. Этот файл отвечает только на вопрос: **«где сейчас проект и куда мы идём дальше?»**
