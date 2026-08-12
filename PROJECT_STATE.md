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
- `[x]` В HUD показывается текущий язык и активная sky culture; читаемость поддержана фоновыми плашками.
- `[D]` Release packaging локализации в единый `.locpak`/binary package с простым преобразованием/обфускацией, checksum и при необходимости signature. Нужен ближе к release, не сейчас.

---

## 3. Звёздное небо

**Статус: `[x]` текущий этап закрыт.**

- `[x]` Реальные звёзды хранят 3D galactic positions и пересчитываются относительно текущей позиции наблюдателя.
- `[x]` Созвездия привязаны к физическим звёздам и геометрически искажаются при смене точки наблюдения.
- `[x]` 23 вспомогательные constellation reference stars имеют конечные 3D positions/distances и участвуют в том же observer-relative процессе; fixed sky-direction путь удалён.
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

## 6. Runtime activation / persistent universe

**Статус: `[~]` инфраструктура есть, production migration ещё не закончена.**

- `[x]` Есть entity runtime-policy foundation: `EntityType`, `MotionModel`, `SimulationMode`, `TimelineDomain`, authority/presentation policy.
- `[x]` Есть режимы `Scheduled / Coarse / Prewarm / Active` и explicit transition rules.
- `[x]` Есть activation anchors, spatial broad-phase, predicted interaction/CPA logic и hysteresis/state machine.
- `[~]` Activation policy уже влияет на часть AI cadence, но ещё не является полной системой снижения стоимости physics/signals/replication.
- `[ ]` Перевести production ships/hubs/modules на новую runtime policy системно.
- `[ ]` Реализовать полноценный coarse/scheduled motion для дальних persistent entities.
- `[ ]` Materialization: `Scheduled -> Prewarm -> Active` с восстановлением полноценного dynamic state.
- `[ ]` Collapse обратно в coarse/scheduled представление без потери persistent identity/history.

---

## 7. Multi-system runtime и межзвёздные перелёты

- `[~]` Мир и карты знают несколько систем, но production simulation ещё не является полноценным multi-system runtime.
- `[ ]` Несколько server system runtime contexts / activation по системам.
- `[ ]` Реальный inter-system transfer entity между runtime domains.
- `[ ]` Финальная механика J/travel mode после стабилизации runtime ownership и activation.
- `[ ]` Навигационные маяки/лоровая система определения координат и точности навигации.

---

## 8. Траектории и исторические данные

Это обязательная будущая возможность карт, даже если источник данных будет зависеть от gameplay-информации.

- `[ ]` Отрисовка фактической траектории выбранного судна.
- `[ ]` Отрисовка возможных траекторий достижения цели/объекта.
- `[ ]` Исторические траектории всех известных судов в выбранном кубе за интервал времени.
- `[ ]` Источник знания отделён от renderer: локальный radar observation, transponder, SOS, station/network data, intelligence и т. п.
- `[ ]` Renderer должен уметь показывать trajectory data независимо от того, откуда gameplay получил эти данные.

---

## 9. Persistent ships и экономика мира

- `[~]` Принцип принят: корабли — в основном конечные persistent entities мира, а не бесконечный spawn/despawn фон.
- `[x]` Тип корабля отделён от конкретного экземпляра с persistent identity.
- `[ ]` Persistent ship records: имя, serial/identity, owner, condition, cargo/state и текущий motion/runtime representation.
- `[ ]` Дальние суда могут существовать как расписание/траектория и materialize при необходимости.
- `[ ]` Экономическая симуляция должна учитывать существующие суда, задержки, нападения, потерю груза, ремонт и т. п. без обязательной full-rate симуляции каждого корабля.

---

## 10. Карты и навигационная иерархия

- `[x]` Есть Galaxy / System / Details / Hub modes.
- `[x]` Navigation grid/cube hierarchy является базовой системой адресации/навигации.
- `[~]` Presentation ownership карт ещё переносится с server-built DTO на client-side composition; см. раздел 5.
- `[ ]` После завершения migration добавить trajectory presentation как отдельный независимый слой карт.

---

## 11. Render / visual future

- `[x]` Render style считается исключительно client presentation policy и не должен менять authoritative world state.
- `[ ]` Два взаимоисключающих presentation style: technical/wire и anime/cel-shaded.
- `[ ]` Дальнейшие bloom/haze/grading/softening/vignette — после стабилизации основных runtime и map ownership границ.

---

## 12. Следующий рабочий блок

На момент создания этого файла:

1. Локализация — **закрыта на текущем этапе**.
2. Звёздное небо/созвездия — **закрыты на текущем этапе**.
3. `Ctrl+F10` flight-mode switching — **исправлено**.
4. Следующий фокус — **продолжение client/server presentation migration**.
5. Первый рекомендуемый технический шаг — **убрать StarAtlas из transport payload и сделать static catalog действительно локально загружаемым на обеих сторонах с version/hash compatibility check**.

---

## Когда обновлять

Обновлять `PROJECT_STATE.md`, когда:

- закрыт этап;
- принято новое фундаментальное решение;
- меняется ближайший приоритет;
- сознательно откладывается важная задача;
- обнаружено новое архитектурное ограничение, которое будущая работа не должна забыть.

Подробности реализации должны оставаться в коде, специализированных MD и regression/architecture/acceptance tests. Этот файл отвечает только на вопрос: **«где сейчас проект и куда мы идём дальше?»**
