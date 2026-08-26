# Render Foundation Gate

**База проекта:** `src(20260826-092251).zip`  
**Статус:** `ACTIVE` — блокирует дальнейшее расширение мира  
**Дата:** 2026-08-26

## 0. Решение

До дальнейшего расширения мира, docking, traffic/NPC density, материалов, эффектов и новых presentation-систем необходимо полностью проверить и привести в порядок client render/runtime foundation.

Цель этапа — не «переписать рендер потому что можно», а:

- убрать архитектурные причины фризов;
- сделать стоимость кадра измеримой и предсказуемой;
- убрать накопившиеся render-path костыли и мёртвые переходные решения;
- определить осознанный OpenGL baseline;
- закрепить новую архитектуру тестами;
- только после этого продолжать строить мир.

**Жёсткое правило:** в каждом патче меняем только то, что необходимо для конкретной задачи. Работающие соседние механики не трогаем без прямой необходимости. Каждый патч должен быть узким, проверяемым отдельно и не менять gameplay-поведение, если это прямо не требуется задачей.

Обсуждённая перед паузой архитектура ручной/автоматической стыковки зафиксирована в конце этого файла и считается замороженной до завершения Render Foundation Gate. Механика RCS/маневровых двигателей также считается уже существующей зависимостью и не входит в рефакторинг рендера.

---

# 1. Уже подтверждённые проблемы

## 1.1. Presentation thread всё ещё может ждать локальный authoritative server

Сейчас `ServerWorker::advance()` использует single-flight back-pressure через блокирующий `m_condition.wait(...)`, если предыдущий authoritative batch ещё выполняется.

В реальном запуске уже наблюдались:

- `session-start-update` примерно **331–760 ms**;
- задержка попадает непосредственно в client/presentation path;
- наличие отдельного server worker thread не спасает от frame stall, потому что latency сервера возвращается в главный поток через wait.

Это нарушает realtime boundary: authoritative simulation может отставать, но не должна останавливать presentation.

**Требуемый контракт:** никакой server fixed-step batch, replication build, persistence, debug request или local transport operation не имеет права синхронно блокировать presentation/render thread.

---

## 1.2. GPU resources лениво создаются из frame preparation

`SceneRenderer::prepareScene()` может вызвать `AssemblyGpuLibrary::get()`. При cache miss вызывается `buildGpuResources()`, который загружает в GPU:

- LOD0;
- LOD1;
- whole-ship proxy.

То есть первое появление нового типа объекта в кадре способно вызвать allocation/upload прямо из frame preparation.

Уже наблюдалось:

- первый игровой кадр: `prepare_ms ≈ 227 ms`.

**Требуемый контракт:** `prepareScene()` и render passes получают только готовые GPU handles. Создание/upload относится к явному loading/warm-up/streaming lifecycle.

---

## 1.3. OpenGL может продолжать render/swap под полностью закрывающим его WebView

Для pending-session loading уже есть специальная no-swap ветка, но общий Main Menu/document WebView path всё ещё способен оставлять GLFW/OpenGL presentation активным под полностью закрывающим его document surface.

До появления нормальной игровой сцены наблюдались:

- Win32 message dispatch: примерно **125–136 ms**;
- `SwapBuffers`: примерно **110–172 ms**.

**Требуемый контракт:** если document/WebView полностью владеет экраном, скрытая GL surface не выполняет обычный scene render/swap без явной необходимости. Native event pump при этом остаётся живым.

---

## 1.4. LOD1 сейчас не является дешёвым режимом рендера

Текущий modular ship/object path во многих случаях всё ещё делает два прохода на каждую деталь:

- fill;
- edge.

В реальном кадре наблюдалось:

- `parts = 102`;
- `draw_calls ≈ 205` только для main camera;
- edge pass использует geometry-shader expansion;
- включён 4× MSAA.

LOD1 уменьшает геометрию mesh, но почти не уменьшает:

- количество draw calls;
- количество state changes;
- edge-pass overhead;
- secondary-camera overhead.

**Требуемый контракт:** LOD обязан уменьшать **полную стоимость представления**, а не только polygon count.

---

## 1.5. Rear camera остаётся полноценным вторым render workload

Задняя камера уже правильно переиспользует `PreparedScene`, но всё равно выполняет второй scene render на своих кадрах. Сейчас для неё используется в основном `forceAssemblyLod1`, а не отдельная жёсткая secondary-camera policy.

CPU-side `rear_ms` уже составляет несколько миллисекунд; отдельного GPU measurement пока нет.

**Требуемый контракт:** rear/secondary camera имеет собственные:

- update cadence;
- resolution;
- LOD/representation policy;
- список разрешённых эффектов;
- CPU/GPU budget.

Она не должна молча удваивать дорогой main-scene path.

---

## 1.6. `prepareScene()` недостаточно зависит от render policy

Часть `SceneRenderPolicy` применяется уже после подготовки сцены. Поэтому `prepareScene()` способен собирать/искать данные, которые конкретная camera/pass потом вообще не будет рисовать.

**Требуемый контракт:** eligibility, visibility, camera policy и representation/LOD selection должны происходить настолько рано, насколько это возможно. Неиспользуемая работа не должна выполняться «на всякий случай».

---

## 1.7. Нужен полный аудит dynamic GPU buffers

В runtime есть несколько повторяющихся `glBufferData` путей. Часть относится к initialization, часть — к recurring rendering и требует проверки.

Обязательный аудит минимум для:

- `SystemMapRenderer`;
- `HudPrimitiveBatch`;
- `TextRenderer`;
- `WorldLabelRenderer`;
- debug line rendering;
- cockpit dynamic geometry;
- UI/minicamera paths.

**Требуемый контракт:** recurring render path не должен регулярно пересоздавать GPU storage без доказанной необходимости. Для динамических данных используются заранее выделенные reusable buffers / ring buffers / streaming strategy.

---

## 1.8. Procedural presentation work всё ещё может синхронно выполняться на render thread

`ARCHITECTURE_STATUS.md` уже фиксирует procedural cloud texture/morphology generation как синхронную render-thread performance проблему.

**Требуемый контракт:** тяжёлая procedural generation должна быть:

- precomputed;
- amortized;
- asynchronous;
- либо иметь явный жёсткий frame budget.

Неконтролируемые frame spikes запрещены.

---

## 1.9. Текущие тесты почти не защищают frame-time архитектуру

Существующие tests хорошо защищают ownership и функциональные seams, но плохо защищают latency/performance seams.

Более того, текущий `check_server_worker_thread.py` прямо требует наличие блокирующего `m_condition.wait()` в `ServerWorker::advance()` — то есть тест сейчас закрепляет решение, которое разрешает presentation stall.

Большинство automated suites не проходят реальный путь:

`Windows -> GLFW -> OpenGL -> WebView2/DWM -> real GPU -> SwapBuffers`.

Поэтому они физически не способны увидеть часть фризов, которые видны руками.

**Требуемый контракт:** static architecture guards запрещают известные источники stall, а отдельный graphical acceptance path реально запускает renderer и измеряет startup / first-frame / steady-state CPU+GPU поведение.

---

# 2. Решение по версии OpenGL

Сейчас окно запрашивает:

- OpenGL **3.3**;
- **compatibility profile**;
- 4× MSAA.

Переходить на более новый OpenGL только ради номера версии нельзя. Сначала устраняем API-independent архитектурные ошибки и измеряем реальные bottlenecks.

## 2.1. Decision gate

После instrumentation и полного hot-path audit сравнить нужные renderer features с минимальным поддерживаемым GPU/driver.

### Предпочтительная цель, если upgrade оправдан

**OpenGL 4.5 Core** — предварительно наиболее разумная цель.

Почему:

- Direct State Access упрощает resource ownership и уменьшает implicit binding-state hazards;
- `buffer_storage` / persistent mapped buffers доступны в современном feature set;
- Multi-Draw Indirect доступен;
- SSBO и buffer-driven batching доступны;
- современный debug output доступен;
- полезные для нашей архитектуры функции уже присутствуют, а обязательной потребности в 4.6 пока нет.

### OpenGL 4.6

Переходить на 4.6 только если:

1. есть конкретная нужная 4.6 feature;
2. она действительно приносит измеримый выигрыш/упрощение;
3. минимальный поддерживаемый hardware floor это позволяет.

Сам по себе номер `4.6` преимуществом не является.

### Если остаёмся на 3.3

Оставляем 3.3 только если:

- это необходимо для целевого старого hardware;
- после оптимизации renderer укладывается в budgets.

Даже в этом случае нужно по возможности уйти от compatibility-profile assumptions и привести resource lifetime/batching/streaming в порядок.

## 2.2. Главное правило миграции

Новый OpenGL не должен маскировать плохую архитектуру.

Независимо от версии запрещены:

- file/model loading в frame hot path;
- shader compilation в gameplay frame;
- server waits в presentation thread;
- GPU upload по факту первого появления объекта;
- неограниченные allocations/queues;
- неизмеряемые secondary passes;
- неявное resource ownership.

---

# 3. Целевая render/runtime архитектура

## 3.1. Контракт presentation thread

Presentation thread имеет право:

- читать уже опубликованное client state;
- обновлять presentation state;
- строить bounded frame description;
- подавать GPU commands;
- обслуживать необходимые native events.

Presentation thread **не имеет права синхронно ждать**:

- authoritative server simulation;
- network transport;
- persistence;
- disk/file I/O;
- OBJ/asset parsing;
- shader compilation;
- texture/procedural generation;
- GPU resource creation;
- завершения background worker.

Если данные ещё не готовы — используется последний опубликованный valid state, placeholder/fallback representation или явный loading state.

## 3.2. GPU resource lifecycle

Целевая схема:

```text
asset definition
    -> CPU load/compile
    -> resource request
    -> GPU upload/warm-up
    -> READY handle
    -> frame use
    -> deferred destruction
```

`prepareScene()` и render passes работают только с `READY` resources.

## 3.3. Frame pipeline

```text
published client snapshot
        |
        v
presentation update
        |
        v
visibility + representation selection
        |
        v
prepared frame / render packets
        |
        +--> main camera
        +--> secondary camera policy
        +--> HUD/UI
        |
        v
batched GPU submission
        |
        v
post-process / present
```

Ни один слой не должен сбоку запускать asset loading, server execution или unrelated state mutation.

---

# 4. План работ

## R0 — Измерения и полный inventory render path

**Цель:** до оптимизации знать, куда уходят CPU/GPU/synchronization milliseconds.

- [ ] Вернуть/гарантировать видимый вывод Debug Control URL, не включая обратно общий stdout noise.
- [ ] Добавить GPU timer queries минимум для:
  - [ ] main opaque/fill meshes;
  - [ ] edge/wire pass;
  - [ ] real/visual ships и objects по необходимости;
  - [ ] rear camera;
  - [ ] HUD/native UI;
  - [ ] post-process;
  - [ ] map rendering.
- [ ] Сохранить CPU timings для:
  - [ ] prepare;
  - [ ] scene submit;
  - [ ] client update;
  - [ ] server execution/wait;
  - [ ] native event pump;
  - [ ] swap/present.
- [ ] Добавить counters:
  - [ ] draw calls по passes;
  - [ ] parts/instances;
  - [ ] triangles/indices, где практически возможно;
  - [ ] GPU uploads и uploaded bytes per frame;
  - [ ] buffer storage reallocations per frame;
  - [ ] shader/program creation после startup;
  - [ ] resource cache misses после входа в gameplay.
- [ ] Снять baseline для сценариев:
  - [ ] cold application startup;
  - [ ] Main Menu idle;
  - [ ] Loading -> first Flight frame;
  - [ ] пустой/лёгкий Flight;
  - [ ] близкий LOD0 ship/station;
  - [ ] LOD1 modular model;
  - [ ] proxy/far representation;
  - [ ] rear camera;
  - [ ] System/Detail/Hub map;
  - [ ] rapid F1-F12 transitions.
- [ ] Сделать полный inventory всех runtime вызовов:
  - [ ] `glGen*`;
  - [ ] `glBufferData`/buffer allocation;
  - [ ] texture allocation/upload;
  - [ ] shader compile/link;
  - [ ] framebuffer creation;
  - [ ] `glGet*`/readback;
  - [ ] sync/fence/wait;
  - [ ] file/model parsing.
- [ ] Для каждого вызова классифицировать lifecycle: `init / load / stream / frame`.

**Exit:** ни одна серьёзная оптимизация не принимается без before/after measurement.

---

## R1 — Убрать архитектурные frame stalls

### R1.1. Server/presentation decoupling

- [ ] Убрать blocking `ServerWorker::advance()` back-pressure из presentation thread.
- [ ] Сохранить корректную authoritative fixed-step/input semantics — нельзя просто выбрасывать simulation input ради FPS.
- [ ] Определить явную bounded debt/coalescing policy, если server execution отстаёт.
- [ ] Исправить architecture tests, которые сейчас требуют `m_condition.wait()`.
- [ ] Добавить guard: blocking waits запрещены в presentation-frame call chain.

### R1.2. GPU upload warm-up

- [ ] Убрать `AssemblyGpuLibrary` cache-miss upload из `SceneRenderer::prepareScene()`.
- [ ] Добавить explicit preload/warm-up GPU resources, необходимых initial scene.
- [ ] Для будущего streaming ввести `Loading / Requested / Ready / Failed`.
- [ ] Пока ресурс не Ready — использовать дешёвый fallback, а не блокировать frame.
- [ ] Запретить `glGen*`/mesh upload/texture allocation только потому, что объект впервые стал видимым.

### R1.3. Covered OpenGL surface

- [ ] Расширить существующий loading no-swap contract на все full-screen document/WebView states.
- [ ] Оставить native event pumping живым.
- [ ] Проверить Main Menu, Loading, ESC menu, resize и multi-client на Windows.

### R1.4. Остальные синхронные generators

- [ ] Унести тяжёлую procedural cloud/texture generation из неконтролируемого render-thread execution.
- [ ] Проверить font/glyph и прочие lazy presentation caches на first-use spikes.

**Exit:** нет известной CPU-side архитектурной операции, которой разрешено заморозить presentation frame на сотни миллисекунд.

---

## R2 — GPU resource / streaming architecture

- [ ] Ввести единый явный contract/owner для client GPU resources либо формально объединить существующие libraries под одним lifecycle contract.
- [ ] Разделить immutable/static GPU buffers и dynamic/streaming buffers.
- [ ] Убрать recurring storage reallocation для HUD/map/text/debug streams.
- [ ] Ввести reusable dynamic buffer arenas/ring buffers.
- [ ] Если принимаем GL 4.4/4.5 — проверить persistent mapped buffers на измеренных dynamic-stream bottlenecks.
- [ ] Ввести безопасное deferred destruction GPU resources.
- [ ] Shader compile/link/pipeline validation выполнять в loading/warm-up, не при первом gameplay use.
- [ ] Добавить cache-miss/resource-lifecycle telemetry и guards.

**Exit:** gameplay frame выполняет bounded data writes + draw submission, но не строит ресурсы.

---

## R3 — Main scene hot path

### R3.1. Готовить только то, что реально будет отрисовано

- [ ] Применять camera/pass policy до дорогой подготовки данных.
- [ ] Убрать повторные assembly/resource lookup из глубоких per-part loops.
- [ ] Собирать компактные render packets, сгруппированные по representation/material/pipeline.
- [ ] Убрать безопасно устранимые duplicate conversion/culling/lookup между passes.

### R3.2. Новый LOD contract

LOD определяется **полной стоимостью и визуальной функцией**, а не только polygon count.

- [ ] Измерить vertex/index counts LOD0/LOD1/proxy.
- [ ] Зафиксировать representation tiers с measurable budgets.
- [ ] LOD1 обязан уменьшать draw/state/edge cost.
- [ ] Whole-object proxy/impostor должен включаться достаточно рано, чтобы реально экономить.
- [ ] Не рисовать modular subparts на дистанции, где их topology уже визуально не читается.
- [ ] При необходимости добавить hysteresis на LOD switching.

### R3.3. Batching / instancing

- [ ] Довести/проверить instancing repeated proxy и повторяющихся mesh/material groups.
- [ ] Batch compatible modular part draws.
- [ ] При GL 4.3+ рассматривать Multi-Draw Indirect только после нормального обычного batching.
- [ ] Минимизировать shader/material/state switches.

### R3.4. Edge/wire representation

Нынешний thick-edge geometry-shader path сначала измеряем, затем решаем судьбу.

- [ ] Отдельно измерить GPU cost edge pass.
- [ ] Проверить альтернативы:
  - [ ] дешёвые lines/edges на дистанции;
  - [ ] precomputed edge mesh;
  - [ ] screen-space outline/cel edge;
  - [ ] отключение/упрощение edges на LOD1/proxy;
  - [ ] batched/instanced edge path.
- [ ] После выбора удалить abandoned edge implementations.

### R3.5. MSAA / post-process

- [ ] Измерить реальную стоимость 4× MSAA.
- [ ] Определить, всем ли passes вообще нужен MSAA.
- [ ] Для rear camera/maps/HUD проверить более дешёвые варианты.
- [ ] Убедиться, что framebuffer resolve/post-process не делают redundant copies.

**Exit:** main scene имеет явную cost model и предсказуемо масштабируется с количеством объектов.

---

## R4 — Secondary cameras, maps, HUD и native UI

### R4.1. Rear camera

- [ ] Дать rear camera отдельную representation policy вместо одного `forceAssemblyLod1`.
- [ ] Зафиксировать target update rate.
- [ ] Зафиксировать resolution.
- [ ] Отключить дорогие edges/effects, не имеющие смысла при размере rear display.
- [ ] Переиспользовать prepare/culling data только там, где это действительно дешевле.
- [ ] Мерить rear-camera GPU time отдельно.

### R4.2. Map renderer

- [ ] Проверить `SystemMapRenderer` dynamic buffer update paths.
- [ ] Убрать recurring GPU storage reallocations.
- [ ] Проверить map framebuffer/MSAA/resolve lifecycle.
- [ ] Не менять существующие map ownership/presentation mechanics без необходимости.

### R4.3. HUD / text / labels / debug

- [ ] Проверить `HudPrimitiveBatch`, `TextRenderer`, `WorldLabelRenderer`, debug lines.
- [ ] Batch text/primitive submissions, где это оправдано.
- [ ] Glyph/geometry resources кешировать вне gameplay hot path.
- [ ] Debug rendering при выключении должен иметь практически нулевую recurring cost.

**Exit:** secondary presentation не способна неожиданно стать главным bottleneck кадра.

---

## R5 — OpenGL modernization gate

Выполнять только после R0–R4, когда уже понятно, какие features действительно нужны.

### Если переходим на OpenGL 4.5 Core

- [ ] Проверить minimum supported GPU/driver на целевых машинах.
- [ ] Обновить GL loader под выбранную core version/extensions.
- [ ] Перевести GLFW context с `3.3 compatibility` на выбранный core profile.
- [ ] Найти и убрать compatibility-only assumptions.
- [ ] В dev builds включить debug context/output.
- [ ] DSA вводить постепенно, а не массовым переписыванием всего рабочего кода.
- [ ] Persistent mapping использовать только для измеренных dynamic-buffer bottlenecks.
- [ ] MDI/SSBO использовать только там, где batching data доказывает пользу.
- [ ] На startup печатать capability report.

### Если остаёмся на OpenGL 3.3

- [ ] Всё равно сделать core-safe resource/state ownership, насколько возможно.
- [ ] Реализовать batching/streaming 3.3-compatible методами.
- [ ] Документировать, от каких оптимизаций сознательно отказались ради hardware compatibility.

**Exit:** версия/profile OpenGL — осознанный architectural decision, а не случайный legacy setting.

---

## R6 — Performance tests и architecture guards

### Static / architecture guards

Добавить тесты, которые падают, если:

- [ ] presentation frame снова содержит blocking server-worker wait;
- [ ] `prepareScene()`/обычные render passes могут создавать/upload assembly GPU resources;
- [ ] shader compile/link доступен из gameplay frame path;
- [ ] filesystem/model parsing доступен из gameplay frame path;
- [ ] full-screen document presentation снова выполняет normal GL scene swap;
- [ ] recurring dynamic path пересоздаёт buffer storage без explicit exemption;
- [ ] rear camera снова получает полный main-camera feature set;
- [ ] render/OpenGL ownership протекает обратно в authoritative/headless code.

### Runtime graphical acceptance

Создать реальный executable benchmark/smoke mode, который проходит:

- [ ] настоящий GLFW/OpenGL context;
- [ ] shader/resource warm-up;
- [ ] first gameplay frame;
- [ ] representative LOD0 scene;
- [ ] representative modular LOD1 scene;
- [ ] proxy/far scene;
- [ ] rear camera;
- [ ] map scene;
- [ ] GPU timer queries;
- [ ] frame-time histogram/spike counter.

CI не должен зависеть от абсолютных milliseconds одной конкретной видеокарты. Используем:

- zero-tolerance architecture events (`runtime upload`, `shader compile`, `blocking wait`);
- configurable performance budgets;
- относительные regression thresholds к сохранённому baseline там, где это разумно.

**Exit:** класс фриза, который мы уже нашли, нельзя тихо вернуть зелёным патчем.

---

## R7 — Уборка архитектуры рендера и костылей

После того как новый hot path работает и измерен:

- [ ] Полностью проверить `SceneRenderer` на dead `#if 0`, abandoned fallback paths и параллельно существующие старые/новые реализации.
- [ ] Удалить закомментированные экспериментальные paths после принятия replacement.
- [ ] Разделить oversized renderer functions по стабильным responsibilities там, где это реально уменьшает coupling.
- [ ] Удалить compatibility shims, которые больше не нужны поддерживаемому hardware.
- [ ] Проверить global/static GL caches, destruction/reset/context lifetime.
- [ ] Проверить ownership GL state между passes.
- [ ] Убрать дублирующиеся culling/LOD thresholds и собрать representation policy в одном месте.
- [ ] Проверить `MeshRenderer`, map renderers, HUD renderers, postprocess, cockpit, secondary cameras на duplicated state setup и redundant work.
- [ ] Обновить `ARCHITECTURE_STATUS.md`, чтобы он описывал итоговую архитектуру, а не исторические промежуточные костыли.
- [ ] Обновить tests согласно финальным contracts.

**Exit:** ни один обнаруженный workaround не остаётся только потому, что «так уже работает».

---

# 5. Начальные performance targets

Это инженерные ориентиры. После R0 их можно уточнить на reference hardware.

## Жёсткие архитектурные цели

- **0** blocking authoritative waits на presentation thread.
- **0** shader compile/link в normal gameplay frames.
- **0** asset-file parsing в normal gameplay frames.
- **0** surprise GPU uploads из-за первого visibility после warm-up.
- **0** unbounded server/render/resource queues.

## Frame targets

Основная цель — стабильные **60 FPS** на reference hardware там, где это позволяет выбранное качество.

- nominal budget: **16.67 ms**;
- CPU и GPU frame time измеряются отдельно;
- отслеживаются P50/P95/P99 и spikes, а не только average;
- startup/first-frame/loading transitions имеют отдельный budget и не считаются обычными gameplay frames;
- secondary cameras получают отдельный sub-budget;
- если budget не помещается, сначала деградирует LOD/secondary presentation, а не стабильность frame pacing.

---

# 6. Порядок выполнения

Не начинаем с переписывания shaders и не начинаем с смены версии OpenGL.

1. **R0 — instrumentation + полный inventory render path.**
2. **R1 — архитектурные stalls:** server wait, first-use GPU upload, covered-surface swap, synchronous generators.
3. Повторное измерение.
4. **R2 — resource/streaming lifecycle.**
5. **R3 — main scene / LOD / batching / edges / MSAA.**
6. **R4 — rear camera + maps/HUD/UI.**
7. Повторное измерение и **R5 — решение по версии OpenGL**.
8. **R6 — automated performance guards + real graphical acceptance.**
9. **R7 — удаление transitional/dead render code и финальная документация contracts.**
10. Полный MinGW ready gate + ручной Windows acceptance.

Только после этого снова расширяем мир и продолжаем docking.

---

# 7. Замороженная на время Render Gate система стыковки

## 7.1. Docking target / task

- Hub map больше не строит docking geometry.
- На Hub map выбирается **один конкретный dock** как docking target.
- Два dock одновременно выбрать нельзя.
- Выбор нового dock заменяет предыдущую docking task.
- Закрытие dock info window снимает docking task.
- Нужна отдельная кнопка `СНЯТЬ ЗАДАЧУ / CLEAR DOCKING TASK`.

## 7.2. Manual docking

- `Ctrl + D` — toggle manual docking guidance.
- Используется отдельный **spatial corridor**, а не autopilot temporal trajectory.
- Рамки остаются, но располагаются примерно в **2–3 раза дальше друг от друга**.
- Рамки полупрозрачные.
- Чем дальше рамка от корабля, тем сильнее fade по alpha.
- Небольшие отклонения пилота **не перестраивают corridor**.
- Существенное изменение скорости/манёвренного радиуса либо полный выход из corridor может вызвать replan.
- Corridor желательно разделить на:
  - дальний approach;
  - transition/alignment;
  - final approach.
- Если возможно, пересчитывается только затронутый tail/segment.
- Full rebuild используется только при крупном срыве/смене цели/существенно изменившейся геометрии.
- Manual approach намеренно длиннее и удобнее для пилота, чем autopilot path.
- Мелкий случайный мусор не обязан входить в тяжёлое obstacle planning manual режима; крупные/структурные препятствия учитываются.

## 7.3. Automatic docking

- `Ctrl + Alt + D` — toggle automatic docking.
- Autopilot использует тяжёлую физически параметризованную trajectory.
- Autopilot управляет:
  - main propulsion;
  - attitude;
  - RCS/маневровыми двигателями;
  - acceleration/coast;
  - flip/braking;
  - terminal alignment;
  - final 6-DOF docking execution.
- Текущий obstacle/path/trajectory stack рассматривается как подходящий фундамент autopilot.
- HUD показывает не рамки, а полупрозрачную trajectory line.
- Line затухает по alpha с удалением от корабля.
- На линии/рядом показывается target dock.

## 7.4. Общие правила docking modes

- Manual и Autopilot взаимоисключающие.
- Повторное нажатие активного chord выключает соответствующий режим.
- Переключение на другой docking mode отключает текущий и включает новый.
- Активный mode показывается белым текстом в существующей guidance-status области HUD.
- Ошибки/отсутствие решения остаются отдельными warning/error состояниями и не смешиваются со статусом режима.

**Эти требования не реализуем и не меняем до завершения Render Foundation Gate.**

---

# 8. Критерий завершения Render Foundation Gate

Gate считается закрытым только если одновременно выполнено всё:

- [ ] полный architecture audit не оставил unresolved blocking seam на presentation thread;
- [ ] GPU resource lifetime явный, first-use upload stalls устранены;
- [ ] есть отдельные CPU и GPU timings основных passes;
- [ ] LOD1/proxy реально уменьшают **полную** стоимость рендера;
- [ ] rear camera имеет явный budget и дешёвую representation policy;
- [ ] map/HUD/UI dynamic-buffer strategy bounded и документирована;
- [ ] выбранный OpenGL version/profile осознанно зафиксирован;
- [ ] automated architecture/performance guards защищают новые contracts;
- [ ] real Windows graphical acceptance больше не показывает recurring startup/gameplay freezes известных архитектурных классов;
- [ ] обнаруженные dead/transitional renderer workarounds удалены либо имеют явное обоснование, почему остаются;
- [ ] полный project ready gate зелёный.

**После этого возвращаемся к docking implementation и дальнейшему расширению мира.**
