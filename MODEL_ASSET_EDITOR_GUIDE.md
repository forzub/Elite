# Elite Model Asset Editor — рабочая инструкция / архитектурный контекст

**Актуально:** 2026-08-28 · canonical mesh contract завершён в 0.10.8
**Редактор:** `Elite Model Asset Editor 0.10.8`
**Asset format:** v4
**Текущий production pipeline:** wizard; реально рабочие стадии `SOURCE`, `LODS`, `GEOMETRY`. В `LODS` перед необязательным генератором теперь обязателен этап подготовки мешей к каноническому render-контракту. Последующие стадии пока остаются capability slots.

> Этот файл является источником контекста для продолжения работы над Model Asset Editor.
> Старые предположения из эпохи format v2/v3 о единой `Node -> GeometryDefinition -> LOD0/LOD1` структуре больше не применять к v4.

---

# 0A. Текущее состояние 0.10.8 — Canonical Mesh Builder

`0.10.7` считать переходной реализацией. Начиная с `0.10.8`, Model Preflight в стадии `LODS` имеет реальный контракт **mesh preparation**, а не набор диагностических safe-fix операций.

Главный поток:

```text
RAW / imported MeshLod
    ↓
positional weld 1e-4 → geometric points
    ↓
index / finite-position validation
    ↓
remove collapsed + duplicate geometric triangles
    ↓
build canonical edge topology
    ↓
solve consistent winding
    ↓
closed shells outward
    ↓
normal islands (polygon / smoothing group / 25° crease)
    ↓
rebuild render vertices
    ↓
rebuild triangles + MeshLod.edges
    ↓
classify geometry contract
    ↓
CANONICAL ASSET GEOMETRY
```

## 0A.1 Geometric point != render vertex

`positional weld = 1e-4` задаёт только **геометрическую идентичность**. Он не означает, что совпавшие OBJ/GPU vertices надо физически схлопнуть в одну вершину.

Одна geometric point может иметь несколько render vertices из-за:

- UV seam;
- material/render split;
- hard normal;
- других render-only атрибутов.

Canonical builder сохраняет существующие UV/material splits. Hard normals не копируются из грязного OBJ: они реконструируются как отдельные **normal islands** по source polygon, smoothing group и автоматическому crease `25°`. Поэтому shared render vertex при необходимости разделяется, а UV seam при сглаженной поверхности остаётся двумя GPU vertices с одинаковой вычисленной normal.

Это обязательный контракт для дальнейших `LOD`, `SURFACES` и `DAMAGE`. Нельзя снова вводить алгоритм «одна welded point → одна normal».

## 0A.2 Что именно меняет подготовка

Canonical builder:

1. проверяет finite positions и triangle indices;
2. строит geometric points через quantized weld `1e-4`;
3. удаляет triangles, схлопнувшиеся после weld или имеющие практически нулевую площадь;
4. удаляет duplicate geometric triangles независимо от их winding, сохраняя coincident face с другим material как отдельную authored поверхность;
5. согласует winding на однозначной two-face adjacency;
6. разворачивает целиком замкнутую inside-out оболочку наружу;
7. **никогда не заделывает boundary loops**;
8. полностью перестраивает render vertices/normals;
9. полностью перестраивает `MeshLod.edges`, включая актуальные `triangleA/triangleB` и seam/crease/boundary flags;
10. сохраняет authored edge render-mask metadata по геометрическим концам ребра, где это возможно.

После операции старые authored normals/winding больше не считаются authoritative. Source OBJ остаётся read-only input.

## 0A.3 Geometry contracts

Каждый подготовленный mesh должен закончить стадию одним из контрактов:

- `ClosedVolume` — закрытый manifold volume, normals наружу, `SurfaceMode::Closed`;
- `ThinTwoSided` — поверхность без толщины, штатные boundary edges, `SurfaceMode::ThinTwoSided`;
- `BreachedVolume` — намеренно открытый объём; boundary пробоины сохраняется, `SurfaceMode::Closed`;
- `Invalid` — только реальная структурная поломка, которую canonical builder не имеет права угадывать/чинить.

`Invalid` включает как минимум:

- triangle index вне массива render vertices;
- non-finite position;
- настоящий source edge, использованный более чем двумя faces и помеченный importer как `EdgeNonManifold`;
- topology, которую невозможно согласованно ориентировать;
- отсутствие usable triangles после cleanup.

Важно: `canonical multi-use` после positional weld **не равен автоматически source non-manifold**. Он может появиться у двух независимых coincident/touching shells и остаётся warning. Настоящий source non-manifold importer фиксирует отдельно.

## 0A.4 Автоклассификация

После preparation:

- однозначный closed mesh автоматически принимает `ClosedVolume`;
- уверенная тонкая открытая геометрия (`ThinTwoSided`, confidence >= 0.90) принимается автоматически;
- неоднозначная open geometry требует **одного** решения пользователя: `ThinTwoSided` или `BreachedVolume`;
- `Invalid` не маскируется ручным выбором класса — сначала нужна реальная правка geometry/source.

Boundary у `ThinTwoSided` и `BreachedVolume` — штатная геометрия, а не ошибка.

## 0A.5 Persistent preparation state

`wizard_state.json` schema `5` хранит для каждого `LOD + geometryId` запись подготовки:

- algorithm id: `canonical_mesh_builder_v2`;
- source render-vertex / triangle counts;
- canonical geometric-point count;
- output render-vertex / triangle counts;
- removed degenerate / duplicate triangles;
- normal-island count;
- rebuilt-edge count;
- fingerprint итогового mesh payload.

Preparation считается действительной только если fingerprint текущего `MeshLod` совпадает с записью. Любое последующее структурное изменение mesh payload/topology автоматически делает запись stale и снова блокирует LOD Generator до повторной подготовки. Изменение только authoring `edge.renderMask` не инвалидирует preparation. Поэтому факт «mesh когда-то анализировали» больше не подменяет факт «этот payload канонически подготовлен».

## 0A.6 Preflight UI

Целевой смысл таблицы:

```text
mesh
    source:       render V / T
    canonical:    geometric P · render V / T
    cleanup:      removed degenerate / duplicate
    topology:     boundary / canonical multi-use / source non-manifold
    type:         Closed / ThinTwoSided / Breached / Invalid
    state:        READY / PREPARE / SET TYPE / FIX MESH
```

Основные и дополнительные meshes показываются отдельными группами. Кнопка `ПОДГОТОВИТЬ МЕШИ` активна, пока хотя бы у одного загруженного mesh нет актуальной preparation record.

Режим `КАК В ИГРЕ` остаётся **read-only сравнением** со старым runtime OBJ-loader: positional weld `1e-4`, runtime-style normals, two-sided rendering. Он не является canonical builder и ничего не пишет в asset.

## 0A.7 Gate для LOD Generator

LOD Generator можно запускать только если используемая базовая геометрия `LOD0`:

- имеет актуальную preparation record;
- не `Invalid`;
- после canonicalization не содержит cleanup/orientation leftovers;
- имеет разрешённый geometry contract;
- имеет `SurfaceMode`, соответствующий этому контракту.

**Не добавлять новые LOD algorithms, пока этот gate не подтверждён на реальной station/ship geometry.**

Поведенческие regression contracts canonical builder:

```text
closed cube
    → remains closed / outward / rebuilt edges

single plate
    → remains open; hole/boundary is not filled

breached cube
    → authored opening survives

dirty UV-seam quad
    → degenerate + duplicate removed
    → UV render splits survive

hard edge
    → one geometric point may own multiple render vertices/normals

broken indices / real source non-manifold
    → Invalid
```

Правильный рабочий порядок:

```text
SOURCE
  → load complete authoring set
LODS / MESH PREPARATION
  → ANALYZE
  → PREPARE MESHES
  → classify only ambiguous open meshes
  → verify READY для всех загруженных meshes
  → checkpoint (LODS не завершится при незаконченной canonical preparation)
LOD GENERATOR (optional, downstream)
GEOMETRY
  → instances / replacements / placement
```


---


# 0. Изменения 0.9.3: синхронизация UI и контрольные точки

## 0.1 Mesh не гоняется при обычных командах

Полный geometry payload (`positions`, `normals`, `indices`, `edges`) передаётся из C++ backend в browser только когда geometry действительно загружается или заменяется:

- первое открытие asset;
- source reimport;
- restore checkpoint;
- load/reload LOD;
- операция, реально меняющая vertex/index payload.

Обычные authoring-команды используют `asset_metadata` и **не имеют права повторно посылать неизменившийся mesh**. Это относится как минимум к transforms/pivots, geometry binding, instance consolidation, duplicate/radial instances, semantic state metadata, collision, sockets, hit/opening/repair metadata.

Browser сохраняет mesh arrays и `THREE.BufferGeometry` в cache по стабильному ключу `LOD + RenderGeometryDefinition.id`. Metadata refresh может перестроить лёгкий scene graph, но не пересоздаёт неизменившиеся GPU geometry buffers. После `break instance` новая unique geometry локально клонируется из уже загруженного mesh; изменение edge mask передаёт только изменившийся mask.

## 0.2 Wizard checkpoints — линейная история

Checkpoint не является независимой веткой. Если изменили, восстановили или повторно завершили более ранний stage, все более поздние checkpoints относятся к старой версии asset и должны быть удалены физически.

Правило:

```text
SOURCE complete
LODS complete
GEOMETRY complete

изменили LODS

SOURCE complete
LODS stale       # старый LODS checkpoint можно восстановить
GEOMETRY not_started + checkpoint directory removed
```

После `RESTORE LODS` сам LODS снова `complete`, а всё после него удалено. После `COMPLETE SOURCE + CHECKPOINT` все старые LODS/GEOMETRY/... checkpoints также удаляются.

---

# 1. Главная архитектура v4

Asset разделён на два независимых слоя.

## 1.1 Semantic / gameplay layer — общий для всех LOD

Живёт в `.elmodel`.

Содержит, в частности:

- semantic `Node` hierarchy;
- module identity;
- base transforms / pivots / joints;
- rigid-body metadata;
- `StateVariant`;
- collision volumes;
- sockets / lights / VFX anchors;
- hit regions;
- openings;
- repair targets;
- materials;
- descriptors доступных render LOD.

Этот слой отвечает за смысл объекта.

**Render LOD не имеет права владеть gameplay state.**

Повреждение, отрыв, collision, repair, sockets и прочая семантика не должны зависеть от того, какой render LOD сейчас показан.

## 1.2 Render layer — независимый документ на каждый LOD

Каждый `RenderLod` имеет собственные:

- `RenderNode` hierarchy;
- `RenderGeometryDefinition` pool;
- local geometry indices;
- instances;
- transforms;
- vertices / triangles / normals / UV;
- material / polygon / smoothing metadata;
- topology;
- Technical / Elite edge masks;
- optional bindings к semantic node/state.

**LOD0, LOD1, LOD2 и дальше не обязаны иметь одинаковую topology, число узлов, число mesh, vertex count или структуру сборки.**

Пример:

```text
semantic asset: station
    ├─ station_core
    ├─ habitat_ring
    └─ solar_array

LOD0:
    detailed multi-part assembly

LOD1:
    one welded station shell

LOD2:
    two coarse proxy primitives
```

Это нормальная и желательная архитектура v4.

---

# 2. Формат файлов

Для asset `station` production package выглядит концептуально так:

```text
src/assets/compiled/models/station/
    station.elmodel
    station.lod0.elmesh
    station.lod1.elmesh
    station.lod2.elmesh
    ...
```

## `station.elmodel`

Semantic manifest + lightweight render-LOD descriptors.

## `station.lodN.elmesh`

Полный независимый render document конкретного LOD.

Нельзя считать `G0`, `G1`, ... глобальными идентификаторами между LOD.

`geometryIndex` всегда LOD-local.

---

# 3. Source policy

Source OBJ / assembly registry являются **read-only input**.

Редактор может:

- читать source;
- reimport;
- canonicalize в памяти;
- строить semantic/render asset;
- писать `.elmodel` / `.elmesh`;
- писать wizard checkpoints.

Редактор не должен менять исходные OBJ/assembly в процессе authoring.

`Reimport source` означает: выбросить текущую in-memory authored версию и заново построить её из source.

---

# 4. Wizard 0.9

Порядок стадий:

```text
SOURCE
LODS
GEOMETRY
SURFACES
SEMANTICS
PHYSICS
DAMAGE
VALIDATE
BUILD
```

На текущем этапе реально реализованы:

1. `SOURCE`
2. `LODS`
3. `GEOMETRY`

Остальные стадии видимы как будущий pipeline contract и не должны имитировать готовый функционал.

## Checkpoints

Завершённая рабочая стадия создаёт **non-production checkpoint** под:

```text
build/tools/model_asset_editor/workspaces/<asset>/
```

Checkpoint нужен для:

- восстановления законченного этапа;
- защиты от последующей порчи состояния;
- маркировки поздних стадий как stale после возврата назад.

Checkpoint не является production asset.

---

# 5. Критический контракт ID

Это hard invariant. Его нужно проверять **до любой записи**, а не надеяться, что serializer поймает ошибку.

## 5.1 Semantic Node IDs

Для `asset.nodes`:

- `Node::id` не пустой;
- `Node::id` уникален во всём semantic graph;
- ID стабилен при повторном reimport одного и того же source;
- module ID и mesh ID нельзя слепо использовать как Node ID, если они могут совпасть.

## 5.2 RenderNode IDs

Внутри **каждого отдельного RenderLod**:

- `RenderNode::id` не пустой;
- `RenderNode::id` уникален в пределах этого LOD.

Одинаковый render ID в разных LOD допустим, потому что LOD documents независимы.

## 5.3 RenderGeometryDefinition IDs

Внутри каждого `RenderLod`:

- geometry ID не пустой;
- geometry ID уникален в пределах этого LOD.

## 5.4 Другие semantic IDs

State / collision / socket / hit-region / opening / repair IDs также должны быть стабильными и однозначными в своей области действия.

---

# 6. Ошибка 2026-08-27: duplicate RenderNode ID в LOD0

Симптом:

```text
Cannot write wizard checkpoint:
empty/duplicate render node id in LOD0
```

На station ошибка возникла не в checkpoint и не в пути `station.elmodel`.

## Реальная причина

В source assembly есть:

```text
module id = station_solar_panels
mesh id   = station_solar_panels
```

Старый importer делал:

```cpp
moduleNode.id = module.moduleId;
meshNode.id   = part.meshId;
```

Получались два semantic nodes:

```text
station_solar_panels
station_solar_panels
```

Дальше v4 migration/build render LOD копировал:

```cpp
render.id = semantic.id;
```

и создавал в LOD0 два одинаковых `RenderNode::id`.

Serializer v4 затем корректно запрещал запись.

## Вывод

Checkpoint был только местом обнаружения.

**Ошибка — в producer path: source importer / legacy-to-v4 migration не обеспечили ID invariants.**

---

# 7. Как исправлять этот класс ошибок

Нельзя чинить только конкретный `station.elmodel` ручным rename.

Нужно исправить генератор состояния.

## 7.1 Importer

При создании semantic node использовать детерминированный unique-ID policy.

Рекомендуемая логика:

```text
module node:
    preferred = module.moduleId

mesh child:
    preferred = part.meshId

если preferred пустой:
    build deterministic fallback from module + role

если preferred уже занят:
    first fallback = <moduleId>.<partMeshId or mesh>

если и он занят:
    append deterministic numeric suffix
```

Для текущего station ожидаемый результат, например:

```text
station_solar_panels          // semantic module
station_solar_panels.mesh     // child mesh semantic node
```

или другой стабильный parent-qualified вариант.

Не использовать случайные GUID для authored source identity.

## 7.2 Legacy -> v4 migration

`buildIndependentRenderLodsFromLegacy()` не должен предполагать, что legacy semantic IDs идеальны.

Перед созданием RenderNode нужно:

1. проверить semantic IDs;
2. при поддерживаемой legacy migration — детерминированно нормализовать старые дубли;
3. затем строить RenderLod;
4. отдельно проверять RenderNode IDs в каждом LOD.

Если автоматический repair небезопасен — fail early с точным diagnostic.

## 7.3 Editor commands

Операции:

- duplicate render instance;
- radial array;
- break instance;
- generated proxy nodes;

обязаны использовать общий helper вида:

```text
uniqueRenderNodeId(lod, preferred)
```

Никакой UI command не должен самостоятельно конструировать ID без общей проверки.

---

# 8. Validator должен быть единым

Нельзя иметь ситуацию:

```text
wizard stage says OK
↓
checkpoint ModelAssetBinary::save says INVALID
```

Нужен единый reusable validator, которым пользуются:

- importer postflight;
- migration postflight;
- wizard preflight;
- Save binary;
- Save LOD;
- checkpoint write;
- tests.

Минимум:

```text
validateSemanticIds(asset)
validateRenderLodIds(lod)
validateRenderBindings(asset, lod)
validateStateReferences(asset)
```

---

# 9. Требование к diagnostics

Сообщение:

```text
empty/duplicate render node id in LOD0
```

слишком бедное.

Нужно выдавать минимум:

```text
LOD0 duplicate RenderNode id 'station_solar_panels':
node[2] and node[7]
producer/source hint: semanticNodeIndex=...
```

Для пустого ID:

```text
LOD0 RenderNode node[7] has empty id
```

То же правило относится к geometry IDs и semantic Node IDs.

**Diagnostic обязан называть offending ID и индексы.**

---

# 10. Wizard preflight

Перед `ModelAssetBinary::save(checkpoint...)` стадия должна проверять все serializer invariants, относящиеся к уже существующему состоянию.

Для `LODS` минимум:

- renderLods не пуст;
- level values корректны;
- каждый loaded LOD имеет nodes/geometries;
- RenderNode IDs non-empty/unique;
- RenderGeometry IDs non-empty/unique;
- parent indices valid;
- geometry bindings valid;
- semantic bindings valid;
- saved payload descriptors не противоречат текущему asset.

Для `GEOMETRY` дополнительно:

- geometry bindings valid;
- no dangling geometry;
- duplicate-consolidation result internally consistent;
- local instance transforms valid;
- bounds finite.

Serializer должен оставаться последней линией защиты, но не первым местом, где пользователь узнаёт о проблеме.

---

# 11. Geometry instancing

В v4 instancing существует **внутри конкретного RenderLod**.

Если LOD0 содержит три одинаковых habitat section:

```text
LOD0 RenderNode A -> G4
LOD0 RenderNode B -> G4
LOD0 RenderNode C -> G4
```

это настоящий LOD-local instance sharing.

LOD1 может вообще не содержать G4 и может быть одним welded shell.

Нельзя автоматически переносить geometry identity с LOD0 в LOD1 по vector index.

## Duplicate fitting

Canonical identity для поиска baked rigid duplicates сейчас практически определяется по LOD0/source geometry.

После consolidation:

- target RenderNode использует reference geometry внутри того же LOD;
- transform компенсирует rigid difference;
- unused geometry удаляется только после проверки.

---

# 12. LOD architecture — не возвращаться к старой модели

Запрещённое предположение:

```text
LOD1 = обязательно decimated версия каждой GeometryDefinition из LOD0
```

Правильное:

```text
LOD1 = независимое визуальное представление semantic asset
```

Допустимо:

- LOD0 — 40 частей;
- LOD1 — 3 части;
- LOD2 — 1 shell;
- topology полностью другая;
- material boundaries упрощены;
- semantic state binding coarse или отсутствует там, где это допустимо визуально.

Gameplay semantics при этом остаются общими.

---

# 13. Damage / semantic state contract

Semantic state не принадлежит render LOD.

Пример:

```text
semantic node habitat_b:
    intact
    breached
```

LOD0:

```text
habitat_b.intact.render
habitat_b.breached.render
```

LOD1:

```text
station_shell_intact
station_shell_breached
```

LOD2 может использовать один coarse proxy, если damage визуально сообщается другим способом.

State transition должен атомарно менять:

- active render presentation;
- collision;
- hit regions;
- openings;
- sockets/VFX;
- repair targets;
- transform/physics overrides.

LOD switch сам по себе gameplay state не меняет.

---

# 14. Source basis / coordinates

Compiled runtime data считается canonical.

Basis conversion — одноразовая authoring operation.

Для уже существующих игровых assembly, импортированных как `game_current`, повторно нажимать Blender conversion нельзя.

Для Blender-oriented input:

```text
Blender:
    +X right
    +Z up
    -Y forward

Game canonical:
    +X right
    +Y up
    -Z forward
```

Преобразовываться должны согласованно:

- geometry;
- normals;
- winding;
- semantic transforms;
- render transforms;
- pivots;
- joints;
- collision;
- sockets;
- inertia.

---

# 15. Orbital Station — acceptance asset

До batch conversion остального каталога станция остаётся stress/acceptance asset.

На ней обязаны проходить:

## Source

- source registry читается;
- LOD0/LOD1 source paths находятся;
- optional broken LOD не уничтожает весь asset без полезной diagnostics;
- source не изменяется.

## IDs

- semantic Node IDs уникальны;
- render Node IDs уникальны в каждом LOD;
- render geometry IDs уникальны в каждом LOD;
- случай `moduleId == meshId` покрыт тестом.

## Geometry

- baked rigid duplicates обнаруживаются;
- consolidation создаёт настоящий instance;
- transform сохраняет положение;
- unused geometry удаляется только после consolidation;
- другой LOD не меняется.

## LOD

- LOD0 и LOD1 могут иметь разную структуру;
- Save LOD0 не переписывает LOD1;
- unload/load LOD сохраняет authored state.

## Checkpoints

- SOURCE checkpoint пишется;
- LODS checkpoint пишется;
- GEOMETRY checkpoint пишется;
- restore возвращает состояние;
- поздние стадии становятся stale после restore ранней.

## Round trip

```text
load -> edit -> save -> close -> reopen
```

не меняет authored structure.

---

# 16. Обязательные regression tests после ошибки 2026-08-27

Добавить тесты, которые запрещают повторение проблемы.

## Test A — importer node identity

Source:

```text
moduleId = station_solar_panels
meshId   = station_solar_panels
```

После import:

```text
all asset.nodes[i].id are non-empty and unique
```

## Test B — v4 migration

После legacy -> independent render LOD:

```text
all lod.nodes[i].id are non-empty and unique per LOD
```

## Test C — station checkpoint

Полный imported station должен успешно пройти:

```text
SOURCE checkpoint
LODS checkpoint
GEOMETRY checkpoint
```

без ручного rename.

## Test D — diagnostics

Искусственно создать duplicate RenderNode ID.

Validator должен вернуть текст, содержащий:

- LOD number;
- duplicate ID;
- both node indices.

## Test E — producer repair

Проверять не только serializer rejection.

Нужен тест, доказывающий, что **normal station import больше не создаёт invalid asset**.

---

# 17. Debugging playbook для будущей сессии

Если пользователь приносит ошибку Model Asset Editor:

## Шаг 1 — классифицировать слой

Определить, кто выдал ошибку:

```text
source import
wizard stage validation
editor command
ModelAssetBinary save
LOD payload save/load
runtime
```

Не делать вывод по последнему видимому пути файла.

## Шаг 2 — найти exact invariant

Искать exact error string в коде.

Если строка находится в serializer — это означает, что upstream допустил invalid in-memory state.

## Шаг 3 — проверить producer

Для invalid state проследить:

```text
source registry
    ↓
RuntimeAssemblyImporter
    ↓
legacy/v4 migration
    ↓
editor commands
    ↓
wizard
    ↓
serializer
```

Исправлять первое место, где invariant нарушается.

## Шаг 4 — не латать конкретный compiled asset

Ручная правка `station.elmodel` допустима только как диагностика.

Production fix должен чинить:

```text
producer + validator + regression test
```

## Шаг 5 — проверить соседние сущности

Если найден конфликт ID, проверить аналогичный контракт у:

- semantic nodes;
- render nodes;
- render geometries;
- state IDs;
- collision IDs;
- sockets;
- openings;
- repair targets.

## Шаг 6 — записать проблему сюда

После архитектурного изменения обновить этот файл:

- версия;
- текущий pipeline;
- hard invariants;
- известные ограничения;
- acceptance tests;
- следующий шаг.

---

# 18. Capability gate

Рабочая возможность редактора считается сохранённой только если есть четыре слоя:

1. data model;
2. backend command/implementation;
3. visible UI entry point;
4. regression test.

`EDITOR_CAPABILITIES.json` должен защищать как минимум:

- geometry instance fit;
- independent render LODs;
- semantic damage states;
- source reimport read-only;
- wizard checkpoints.

Если новая UI/data-model переделка оставила C++ функцию, но убрала доступ к ней или тест — capability считается потерянной.

---

# 19. Что сейчас считается известными ограничениями

На baseline 0.9:

- wizard позднее `GEOMETRY` ещё не мигрирован полностью;
- full validation report требует усиления;
- diagnostics ID invariants недостаточно подробны;
- legacy/import identity normalization требует системного фикса;
- arbitrary LOD generation/proxy authoring ещё не финализирован;
- transform gizmos / motion preview / material authoring не следует считать завершёнными только потому, что часть backend уже существует;
- game runtime migration на `.elmodel` нельзя ускорять, пока editor pipeline и validation не закрыты acceptance tests.

---

# 20. Исторический фикс после 2026-08-27 — закрыт в 0.9.1

Приоритет:

1. добавить общий deterministic semantic Node ID allocator;
2. применить его в `RuntimeAssemblyImporter`;
3. защитить legacy -> v4 migration от duplicate/empty IDs;
4. вынести render-ID validation в reusable preflight;
5. вызывать preflight до wizard checkpoint;
6. улучшить diagnostic: ID + indices + LOD;
7. добавить exact regression для `station_solar_panels`;
8. прогнать ModelAssetBinary tests + architecture contracts;
9. reimport Orbital Station;
10. повторить SOURCE -> LODS -> GEOMETRY checkpoints.

Этот список закрыт в `0.9.1`; он сохранён ниже только как история архитектурного решения.

---

# 21. Короткая памятка

Если снова появляется:

```text
Cannot write wizard checkpoint: ...
```

не считать checkpoint источником ошибки.

Сначала спросить:

```text
Какое invalid state уже лежало в m_asset,
которое serializer отказался сохранять?
```

Для ошибки:

```text
empty/duplicate render node id in LODN
```

проверять в таком порядке:

```text
semantic Node IDs
    ↓
legacy/v4 RenderNode creation
    ↓
editor-generated IDs
    ↓
LOD-local uniqueness
```

**Правило проекта:** invalid authored state должен быть невозможен или обнаруживаться максимально близко к месту его создания, а не спустя несколько стадий на Save.


---

# 22. Результат фикса 0.9.1

Ошибка `empty/duplicate render node id in LOD0` закрыта системно:

- importer использует deterministic stable-ID allocation;
- при `moduleId == meshId` child получает `<module>.mesh`;
- legacy v2/v3 migration нормализует пустые/дублирующиеся semantic IDs до создания RenderNode;
- `ModelAssetBinary::validate` доступен как reusable preflight;
- wizard вызывает preflight до записи checkpoint;
- diagnostics называют ID и оба индекса;
- status bar имеет явный разделитель между ошибкой и путём.

При повторной подобной ошибке сначала проверять producer identity policy и reusable validator, а не файловый overwrite.


---

# 23. UI/workflow baseline 0.9.2

После 0.9.1 следующий приоритет — **не добавлять новые authoring capabilities, а сделать уже существующие операции понятными и безопасными**.

## 23.1 GEOMETRY: reference-first comparison

Обычный workflow поиска одинаковой baked geometry:

1. выбрать один render element как **Reference**;
2. отметить checkbox только у элементов, которые нужно сравнить с ним;
3. нажать **COMPARE SELECTED**;
4. сравнение ничего не изменяет в asset;
5. совпавшие строки становятся зелёными (`MATCH`), несовпавшие — розовыми (`DIFFERENT`);
6. уже использующие geometry эталона строки отмечаются как `INSTANCE`;
7. одной командой **MAKE MATCHES INSTANCES** перевести все выбранные совпадения на geometry эталона.

Не использовать pairwise UX вида `G2-G3`, `G2-G4`, `G3-G4` как основной рабочий режим. Backend compatibility scan может существовать, но production UI должен оставаться reference-first.

Таблица и viewport обязаны быть синхронизированы в обе стороны:

```text
click table row -> select/highlight viewport object
click viewport object -> select/highlight table row
```

`G#` — только LOD-local индекс для отображения. Постоянные asset-связи не строить на `G#`.

## 23.2 GEOMETRY inspector: минимальный и stage-aware

На стадии `GEOMETRY` правый inspector показывает только то, что относится к геометрии активного LOD:

- идентичность выбранного элемента и его geometry;
- source/statistics/usage count;
- placement: Position / Rotation / Pivot;
- instance tools;
- collapsed Advanced/manual tools;
- отдельно destructive actions.

На этой стадии **не показывать** semantic-state controls и damage/state editing. `Surface` относится к стадии `SURFACES`.

Кнопки группировать по назначению. Не оставлять плоский набор несвязанных команд.

Все context-sensitive команды должны быть `disabled`, если операция неприменима. Например:

- `Duplicate instance` — только при наличии geometry;
- `Break instance` — только если geometry реально shared;
- `Radial array` — только для элемента с geometry;
- `Fit as instance` — только после выбора reference;
- `Delete element` — блокировать, если удаление нарушит hierarchy.

Каждая видимая команда должна иметь понятный tooltip, объясняющий **что именно изменится**, а не только внутреннее имя backend-команды.

Термины UI ориентировать на действие пользователя:

```text
Apply placement
```

вместо неясного:

```text
Transform render node
```

и аналогично для geometry assignment/state operations.

## 23.3 Radial array

`Radial array` остаётся полезным authoring tool для станций и повторяющихся модулей, но не должен работать через цепочку `prompt()`.

Параметры задаются в одном modal dialog:

- количество instances, включая выбранный;
- total angle;
- axis X/Y/Z;
- center: selected pivot / origin / custom XYZ.

UI явно сообщает, что копии используют общую geometry, а выбранная axis задаёт ось вращения; плоскость массива ей перпендикулярна.

## 23.4 Sidebar

Правая рабочая панель должна иметь достаточно места для XYZ, IDs и таблиц. Для desktop baseline использовать ориентир порядка 460–580 px вместо старой узкой колонки ~330 px.

Приоритет: меньше одновременно видимых секций, а не больше. Wizard stage определяет, какие панели вообще относятся к текущей работе.

## 23.5 LOD generator

Автоматический `Generate LOD1 from LOD0` **намеренно отложен**. Не начинать его до стабилизации editor workflow, damage/state variants и понятного LOD authoring UI.

Текущий UI cleanup не меняет v4 boundary: каждый RenderLod остаётся независимым render document, semantic/gameplay graph остаётся shared.


---

# 24. Состояние после 0.10.8 и следующий шаг

Canonical geometry contract считается **реализованным на уровне редактора и покрытым поведенческими regression tests в source tree**. `LOD Generator` в 0.10.8 намеренно не развивается.

Перед возобновлением LOD-pass'ов провести acceptance на реальных моделях:

1. reimport Orbital Station из source;
2. `ANALYZE → ПОДГОТОВИТЬ МЕШИ`;
3. проверить несколько больших `ClosedVolume`;
4. проверить отдельную `ThinTwoSided` plate;
5. проверить authored `BreachedVolume` с удалёнными полигонами;
6. визуально подтвердить сценарий **detached plate → настоящая дыра → видимые внутренности**;
7. сохранить/перезапустить editor и убедиться, что preparation records восстанавливаются и `READY` не теряется;
8. изменить mesh payload и убедиться, что fingerprint делает preparation stale;
9. только после этого продолжать LOD Generator.

План дальнейших LOD-pass'ов остаётся:

```text
1. Coplanar Region Collapse
2. Thin Shell Collapse
   thin box → ThinTwoSided sheet
3. Thin/Small Component Cull
4. Surface Detail Cull
5. general simplifier — только если действительно понадобится
```

Каждый LOD строится **независимо из canonical LOD0**, а не цепочкой `LOD0 → LOD1 → LOD2`.

После стабилизации LOD Generator pipeline продолжается: `SURFACES → SEMANTICS → PHYSICS → DAMAGE → VALIDATE → BUILD`.
