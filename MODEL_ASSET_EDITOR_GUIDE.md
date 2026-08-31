# Elite Model Asset Editor — рабочая инструкция / архитектурный контекст

**Актуально:** 2026-09-01 · explicit heavy-operation boundary / lazy LOD data-plane в 0.10.24
**Редактор:** `Elite Model Asset Editor 0.10.24`
**Asset format:** v4
**Текущий production pipeline:** wizard; реально рабочие стадии `SOURCE`, `LODS`, `GEOMETRY`, `SURFACES`. SOURCE/LODS владеют canonical mesh и render-LOD documents, GEOMETRY — LOD-local geometry/instances/replacements, SURFACES — surface intent и material contract. Следующий незакрытый stage — `SEMANTICS`.

> Этот файл является источником контекста для продолжения работы над Model Asset Editor.
> Старые предположения из эпохи format v2/v3 о единой `Node -> GeometryDefinition -> LOD0/LOD1` структуре больше не применять к v4.

## Heavy-operation boundary / lazy LOD contract (0.10.24)

The editor now has an explicit boundary between **control-plane state** and **geometry data-plane**. Opening a production v4 asset, resuming the latest wizard checkpoint, or restoring a checkpoint reads only `.elmodel` manifest/wizard metadata. No `.elmesh` is decoded at asset-open time. Every LOD stores its current payload provenance (`checkpoint` package when present, otherwise production) and remains `UNLOADED` until an explicit user action needs the mesh.

Wizard tabs are strictly passive. `setWizardStage()` may change panel visibility and selected workflow stage, but it must not call `loadLod`, topology/preflight analysis, `rebuildScene()`, LOD-preview cleanup that rebuilds the scene, or silently switch the viewport to LOD0. Stage navigation is not an authoring/heavy-operation boundary.

The explicit mesh boundaries are commands such as **OPEN LOD IN VIEWPORT**, PREPARE MESHES, ANALYZE, LOD generation/preview/apply, source reimport and other tools whose semantics genuinely require geometry. Internal tool loads are allowed, but they do not automatically publish every resident LOD to the browser.

Browser synchronization is split:

- compact asset/wizard/transform/material/surface/semantic state uses JSON `asset_metadata`;
- one explicitly requested resident LOD uses binary WebSocket frame `ELVPD001`;
- vertex, normal, index, triangle-material, smoothing and edge arrays are never JSON fields;
- metadata refresh must not call `rebuildScene()`. Targeted lightweight updates may adjust transforms/materials only for an already resident viewport LOD. If no LOD payload is resident, geometry, collision and socket viewport groups stay empty; a metadata-only asset/checkpoint open must never draw a “ghost model”. Stale browser geometry caches are dropped on an explicit asset/checkpoint document change and reopened only by a later explicit LOD request.

Surface intent (`ClosedVolume`, `BreachedVolume`, `ThinOneSided`, `ThinTwoSided`) is a geometry-level property. Changing it does not scan triangles, run topology analysis or force LOD siblings into memory. Cross-LOD propagation uses stable authoring visual identity; unloaded siblings record the intent in wizard metadata. When a LOD is explicitly opened, its O(geometry-count) `SurfaceMode` metadata is reconciled in memory. If `Save all` happens first, the same metadata is persisted with a surface-only memory-cursor rewrite that skips vertex/triangle/edge payloads instead of decoding the mesh. `AUTO` clears explicit authoring intent; only explicit ANALYZE owns topology inference.

Derived geometry diagnostics are cached per `LOD + stable geometry id`: counts, bounds/storage estimate, material usage/unassigned count and topology audit. A mesh replacement/preparation/generation/basis conversion invalidates the affected geometry/LOD cache; triangle material assignment invalidates that geometry cache. Pure transforms, surface intent and other metadata do not.

`ModelAssetBinary::loadLod()` keeps asset format v4 unchanged but now reads each `.elmesh` into one contiguous memory buffer and parses through a bounds-checked memory cursor instead of issuing millions of tiny `istream::read()` calls.

Lazy checkpoint save has a specific invariant: if an unopened dirty LOD is still backed by a checkpoint payload, `Save all` promotes that `.elmesh` to production without mesh decode. A byte-identical LOD is copied directly; if only explicit `SurfaceMode` metadata changed, `copyLodWithSurfaceModes()` patches the small geometry metadata bytes while skipping the heavy arrays through the memory cursor. It must never construct a `MeshLod` merely to persist metadata.

The SOURCE panel follows the same contract. `METADATA`/unloaded is a healthy state, not an error. The panel shows manifest-declared `geometryCount`/`renderNodeCount` and per-LOD payload provenance (`checkpoint` or `production`) without decoding the `.elmesh`. The only path from that panel into the geometry data-plane is an explicit per-LOD **OPEN** action or an explicitly labelled heavy source refresh/reimport command.

The storage panel distinguishes two different truths that must never be conflated: **production package files** under the compiled asset path, and the **current workspace payload source** for each LOD. During lazy checkpoint resume, production may legitimately contain only LOD0/LOD1 while LOD2+ are sourced from the checkpoint package. Production byte counts therefore do not mean that the workspace LOD is unavailable; the per-LOD provenance row is authoritative for what an explicit OPEN will read.

---

# 0A. Историческая база 0.10.16 — libigl + Embree production preparation

`0.10.16` завершает эксперимент с самодельной absolute-orientation эвристикой. Продуктом `ПОДГОТОВИТЬ МЕШИ` остаётся **исправленный working MeshLod**, но topology/orientation authority теперь делегирован проверенным geometry-processing библиотекам.

## GEOMETRY workspace contract (0.10.20)

GEOMETRY is a per-RenderLod authoring stage. **Entering the tab itself is passive in 0.10.24:** it preserves the current viewport and does not load/switch/rebuild a LOD. The author explicitly chooses/opens the RenderLod to inspect or edit. G-indices, RenderNodes, instance sharing and cleanup are local to that LOD only.

The stage exposes, in order: the full main/additional geometry browser with single-mesh preview; instance/array authoring (duplicate, break, radial array); rigid duplicate comparison and consolidation; additional/replacement mesh compatibility plus temporary replacement preview; unused ordinary geometry cleanup; and finally the GEOMETRY checkpoint. Additional meshes are protected from ordinary unused-geometry cleanup.

## UI runtime/package contract (0.10.23)

`EliteGame` and `EliteAssetEditor` no longer share a universal `elite_ui.pak`. The game owns `assets/ui/elite_game_ui.pak`; the editor owns `build/tools/model_asset_editor/assets/ui/model_asset_editor_ui.pak`. `HtmlUiServer` never discovers a pack by convention: the executable passes the exact path it owns.

The Model Asset Editor runtime root is the same stable artifact root that owns its executable/workspaces: `build/tools/model_asset_editor`. Its filesystem fallback contains only the editor document and required Three.js modules. The editor build does not depend on the game `copy_assets` tree. Legacy `elite_ui.pak` files are migration debris and are removed by the new pack build commands. A stale game pack therefore cannot shadow a newer editor HTML again.

Wizard stage entry is now deliberately **non-transactional with respect to the viewport**. Manual tab clicks and automatic checkpoint progression both use `setWizardStage`, but that function only changes workflow UI state. It does not clear previews, reset viewport mode, choose LOD0, load `.elmesh`, analyze geometry or call `rebuildScene()`. Viewport representation changes only as the result of a separate explicit viewport/tool action.

## SURFACES workspace contract (0.10.22)

SURFACES is downstream of GEOMETRY and is side-effect free until the author explicitly presses `ANALYZE SURFACES`. Opening or revisiting the tab must not start topology work. One completed analysis remains usable while the upstream SOURCE/LODS/GEOMETRY input is unchanged; upstream authored changes invalidate that cached SURFACES analysis.

The author then chooses a RenderLod and geometry and resolves one of four production surface intents: `ClosedVolume`, `ThinOneSided`, `ThinTwoSided`, `BreachedVolume`. `ClosedVolume`, `BreachedVolume` and `ThinOneSided` render `FrontSide` with back-face culling enabled. `ThinTwoSided` renders `DoubleSide` with back-face culling disabled. The intent is the authority for sidedness; `MaterialDefinition::twoSided` remains only a legacy/binary-compatible field and is not an ordinary SURFACES authoring control.

A default-on `APPLY TO ... ALL LODS` option propagates the chosen intent to the same stable visual family wherever it exists. Matching uses stable base-visual identity for ordinary geometry and stable variant identity for replacement geometry; transient `G#` indices and coincidental per-LOD geometry indices are never cross-LOD identity. The batch writes/publishes metadata once and runs **no** post-change analysis. Loaded sibling geometries receive only the O(1) geometry-level `SurfaceMode` update; unloaded siblings retain their stable-id intent in wizard state and are not decoded. Opening such a LOD reconciles the small geometry metadata in memory; `Save all` can instead persist that metadata with the surface-only v4 payload rewrite. This is useful for station modules whose physical surface class is identical across LODs while still allowing the checkbox to be disabled for deliberately different ship/damage representations.

Material assignment stays per triangle and materials stay in the shared asset material table. Missing/invalid material indices remain an independent SURFACES blocker after surface intent is resolved. The conservative repair still assigns a chosen existing material only to currently unassigned triangles. Material editing covers stable id, base RGBA, emissive color/strength, metallic, roughness and base/emissive texture references. Texture files are references only; import/bake/UV painting is outside this stage.

SURFACES never changes topology, transforms, instance sharing or replacement compatibility. Surface/material edits invalidate SURFACES and later checkpoints only; completed LODS and GEOMETRY remain valid.


## 0A.1 Render contract

Mesh считается подготовленным, когда:

- positions finite, triangle indices читаемы;
- collapsed/zero-area и geometric duplicate triangles удалены;
- positional `1e-4` используется как cleanup/weld candidate, но не склеивает независимые coincident/touching sheets;
- residual non-manifold/non-orientable topology проходит через `libigl::split_nonmanifold`;
- patch winding и absolute front/back определяются `igl::embree::reorient_facets_raycast`;
- настоящие boundary loops/пробоины не закрываются и новые faces не создаются;
- normals пересчитываются **после** окончательного winding;
- UV/material/hard-normal seams сохраняются при rebuild render vertices;
- authoritative `EdgeCanonicalTopology` и bounds перестроены;
- итоговый working mesh проходит дешёвую deterministic проверку topology/winding.

`CanonicalMeshAlgorithmId = canonical_mesh_libigl_embree_v1`. Старые `canonical_mesh_builder_v7` records считаются stale и требуют одного явного PREPARE.

## 0A.2 Production pipeline

```text
resident RAW MeshLod
  ↓
remove collapsed / degenerate / duplicate triangles
  ↓
topology-aware geometric point identity
  ↓
libigl::split_nonmanifold
  ↓
Embree reorient_facets_raycast
  ↓
rebuild normals + hard-normal islands
  ↓
rebuild UV/material-aware render vertices
  ↓
rebuild canonical edges + bounds
  ↓
cheap render-projection stabilization
  ↓
GOOD_ENOUGH or transactional failure
```

The v7 `area-weighted radial envelope`, `OpenOrientationMinConfidence`, open-component flip loop and `radial_score` decisions are removed. `solveOrientation` remains only as a cheap consistency audit for already prepared topology; it is not the production absolute-orientation authority.

## 0A.3 Non-manifold policy

A genuine canonical edge with more than two incident faces is no longer an automatic PREPARE failure. `split_nonmanifold` may duplicate topology vertices to separate orientable manifold sheets. It **must not add triangles**, cap holes or stitch unrelated boundary loops.

The 0.10.11/0.10.14 safeguard remains mandatory: equal coordinates alone do not prove topology identity. Native/source edge adjacency and canonical rebuilt edges decide which coincident render vertices belong to one sheet.

## 0A.4 Three viewport modes

The toolbar exposes:

1. `ИСХОДНИК` — exact resident pre-PREPARE RAW snapshot, `DoubleSide`;
2. `БЕЗ ОТСЕЧЕНИЯ` — prepared mesh, `DoubleSide`;
3. `РАБОЧИЙ` — prepared mesh, `FrontSide`.

RAW snapshots live only in `ModelAssetEditorSession`. They are diagnostic data and are never written into `.elmodel` or `.elmesh`. If no RAW snapshot exists in the current session, SOURCE mode falls back to the resident mesh rather than fabricating source history.

## 0A.5 Repair evidence

Detailed diagnostics remain in:

```text
build/tools/model_asset_editor/workspaces/<asset>/logs/mesh_repair.log
```

For every geometry the log records input cleanup counts, source/canonical non-manifold evidence, `split_topology_vertices`, `raycast_patches`, `raycast_flipped_triangles`, output topology/winding state, render-vertex/edge rebuild counts and exact failure reason. Wizard state schema 6 persists the same libigl/Embree counters beside the canonical fingerprint.

## 0A.6 Runtime boundary

libigl, Eigen and Embree belong only to the offline `EliteAssetEditor` and optional diagnostic spike. `EliteModelAsset`, client/server game runtime and `RuntimeMeshNormalizer` do not link these libraries. Runtime remains tolerant; production authoring remains stricter.

# 0. Изменения 0.9.3: синхронизация UI и контрольные точки

## 0.1 Mesh не гоняется при обычных командах

С 0.10.24 geometry payload **никогда не входит в JSON**. Открытие asset и restore/resume checkpoint публикуют только metadata. Тяжёлый payload конкретного LOD отправляется отдельным binary data-plane только после явного открытия LOD в viewport (или другой явно тяжёлой команды, которой нужен resident mesh).

Обычные authoring-команды используют `asset_metadata` и **не имеют права повторно посылать неизменившийся mesh**. Это относится как минимум к transforms/pivots, geometry binding, instance consolidation, duplicate/radial instances, semantic state metadata, collision, sockets, hit/opening/repair metadata.

Browser сохраняет mesh arrays и `THREE.BufferGeometry` в cache по стабильному ключу `LOD + RenderGeometryDefinition.id`. Metadata refresh не вызывает `rebuildScene()`; он обновляет только лёгкое состояние/overlays либо инвалидирует stale LOD payload, после чего viewport открывается заново явно. После `break instance` новая unique geometry локально клонируется из уже загруженного mesh; изменение edge mask передаёт только изменившийся mask.

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

# 24. Состояние после 0.10.16 acceptance

Real-station acceptance пройден. Зафиксированные проверки:

1. load/restore/reimport остаются non-mutating I/O;
2. `LODS → ПОДГОТОВИТЬ МЕШИ` запускает production libigl/Embree path только по явной команде;
3. station S3 после PREPARE не содержит canonical multi-use edges и winding conflicts;
4. reversed source normals/winding не должны создавать исчезающие FrontSide faces;
5. настоящие boundary loops/пробоины остаются, triangle count не растёт из-за repair;
6. UV/material/hard-normal seams и authored edge metadata сохраняются через editor rebuild;
7. второй PREPARE над уже подготовленной geometry должен дать `changed=0`;
8. при странном результате сначала смотреть `build/tools/model_asset_editor/workspaces/<asset>/logs/mesh_repair.log` и сравнивать `ИСХОДНИК → БЕЗ ОТСЕЧЕНИЯ → РАБОЧИЙ`.

Контрольный spike для `station_Habitat_Module_S3` остаётся диагностическим эталоном метода, но production intermediate vertex count может отличаться из-за topology-aware защиты independent coincident sheets.

План LOD после закрытия canonical repair остаётся:

```text
1. Coplanar Region Collapse
2. Thin Shell Collapse
3. Thin/Small Component Cull
4. Surface Detail Cull
5. general simplifier — только если реально понадобится
```

Каждый LOD строится независимо из canonical LOD0. Далее: `SURFACES → SEMANTICS → PHYSICS → DAMAGE → VALIDATE → BUILD`.

---

# 25. Historical libigl repair spike

This section preserves the spike that justified the 0.10.16 production switch. It is diagnostic history, not a second production pipeline.

The root CMake option:

```text
ELITE_MODEL_ASSET_LIBIGL_SPIKE=ON
```

fetches pinned `libigl v2.6.0` and builds only the isolated target:

```text
model_asset_libigl_spike
```

This spike does **not** change `EliteAssetEditor`, checkpoints or `.elmesh`. It runs:

```text
RAW OBJ
  -> libigl read_triangle_mesh
  -> split_nonmanifold
  -> bfs_orient
  -> diagnostic OBJ
```

Acceptance stage A:

1. libigl + Eigen configure and compile under the project's MSYS2 MinGW64 toolchain;
2. `station_Habitat_Module_S1/S3.obj` complete without crash/assert;
3. output is edge/vertex manifold where libigl can split the source soup into orientable patches;
4. output OBJ opens in Blender and the problematic pocket still physically exists;
5. no production repair code is removed until the real station result is inspected.

The spike intentionally does **not** solve absolute outside for open patches yet. If stage A passes, stage B enables only libigl's Embree module and tests `igl::embree::reorient_facets_raycast` on the same source mesh. If Embree is troublesome under MinGW64, core libigl remains usable and outward-orientation can be evaluated separately.

## 25.1 Stage A result and Stage B contract

Stage A passed on the real MinGW64 workstation for both habitat sources:

```text
RAW vertices=118879 triangles=90162 edge_manifold=yes vertex_manifold=no
SPLIT vertices=118881 triangles=90162 edge_manifold=yes vertex_manifold=yes
PATCHES=16364
DUPLICATED_TOPOLOGY_VERTICES=2
LIBIGL_SPIKE PASS
```

This proves libigl core works and `split_nonmanifold` repairs the actual vertex-manifold defect by duplicating only two topology vertices. It also exposes an important input issue: raw OBJ render vertices still contain UV/normal seam duplication, so `bfs_orient` sees 16364 tiny patches. Stage B therefore does **not** raycast that raw render topology.

Stage B pipeline is deliberately small:

```text
RAW OBJ render vertices
  -> remove_duplicate_vertices(epsilon=1e-4)
  -> remove collapsed triangles
  -> remove combinatorial duplicate triangles
  -> remove unreferenced vertices
  -> split_nonmanifold
  -> reorient_facets_raycast (Embree)
  -> diagnostic OBJ
```

`reorient_facets_raycast` proved to be the better authority for absolute front/back on the real station. The accepted Stage B result is the basis for production 0.10.16, so the previous editor `radial_score` / open-component envelope heuristic is removed from canonical preparation. The editor keeps the validated bounded 0.2-1.0M ray budget instead of libigl's default `100 * face_count`.

---
# 26. Production canonical mesh preparation — 0.10.16

`ПОДГОТОВИТЬ МЕШИ` is the explicit offline authoring boundary for imported OBJ geometry. Production preparation uses pinned `libigl v2.6.0` core plus its Embree module; these dependencies are not linked into the game runtime.

Pipeline:

```text
resident RAW MeshLod
  -> positional 1e-4 cleanup candidates
  -> remove collapsed/degenerate triangles
  -> remove geometric duplicate triangles
  -> topology-aware point identity (do not merge independent touching sheets)
  -> libigl::split_nonmanifold
  -> igl::embree::reorient_facets_raycast
  -> rebuild normals / hard-normal islands
  -> rebuild UV/material-aware render vertices
  -> rebuild canonical edges + bounds
```

`split_nonmanifold` may duplicate topology vertices but never creates triangles. No preparation stage caps boundaries or fills authored openings. Embree ray casting is the authority for absolute patch front/back; radial/centroid/open-component orientation heuristics are not part of production PREPARE.

Viewport diagnostics:

1. `ИСХОДНИК` — the resident pre-PREPARE RAW snapshot, `DoubleSide`;
2. `БЕЗ ОТСЕЧЕНИЯ` — prepared mesh, `DoubleSide`;
3. `РАБОЧИЙ` — the same prepared mesh, `FrontSide`.

The RAW snapshot exists only in the editor session and is never written into `.elmodel` or `.elmesh`. Technical preparation evidence is written to `build/tools/model_asset_editor/workspaces/<asset>/logs/mesh_repair.log` and replaced at the start of each PREPARE run; wizard schema 6 also stores cleanup counts, split topology vertex count, raycast patch count and raycast-flipped triangle count.

Real-station spike reference for `station_Habitat_Module_S3`:

```text
RAW V=118879 T=90162
WELDED V=55163
CLEAN T=90160
SPLIT V=56934 edge_manifold=yes vertex_manifold=yes
RAYCAST_PATCHES=1834
RAYCAST_FLIPPED_TRIANGLES=39122
LIBIGL_SPIKE PASS
```

Production output is not required to have the same intermediate vertex count as the isolated spike because the editor preserves authored topology identity across coincident/touching sheets. Acceptance requires repaired manifold topology, preserved real boundaries, preserved UV/material/hard-edge seams and correct FrontSide appearance in `РАБОЧИЙ`.

---
# 27. LOD gate split and stable editor artifact layout — 0.10.17

0.10.17 separates **technical canonical geometry readiness** from later **SURFACES authoring**.

The LODS stage answers geometric questions only:

```text
Is the resident LOD0 the current PREPARE result?
Are degenerate/duplicate faces gone?
Are winding conflicts gone?
Are closed components no longer inward?
Is the geometry structurally usable for LOD analysis?
```

If yes, `ANALYZE LOD0` is allowed and the LODS checkpoint may be written.

The following are **not** LODS gates:

```text
ClosedVolume
ThinTwoSided
BreachedVolume
surfaceMode reconciliation
```

Those are SURFACES authoring decisions. Preflight may still report them and ask for review, but an unresolved surface class is advisory during LODS and must not paint a technically prepared mesh as a red LOD blocker.

This fixes the invalid dependency that produced:

```text
LODS validation failed: canonical geometry contract is incomplete:
LOD0 G0 needs an explicit target geometry class
```

after a successful canonical PREPARE.

## Stable filesystem contract

Developer-facing Model Asset Editor artifacts use one project-rooted tree and never depend on the process current working directory:

```text
build/tools/model_asset_editor/
    bin/
        EliteAssetEditor.exe
        model_asset_libigl_spike.exe

    workspaces/
        <asset>/
            wizard_state.json
            checkpoint-SOURCE/
            checkpoint-LODS/
            checkpoint-GEOMETRY/
            logs/
                mesh_repair.log
                instance_fit.log

    diagnostics/
        libigl/
            *_libigl_raycast.obj
```

`mesh_repair.log` is truncated at the beginning of every `ПОДГОТОВИТЬ МЕШИ` operation. One file therefore describes exactly one PREPARE run; historical v7/0.10.16 records are no longer mixed together.

Production assets remain separate:

```text
src/assets/compiled/models/<asset>/
    <asset>.elmodel
    <asset>.lod0.elmesh
    <asset>.lod1.elmesh
    ...
```

Source OBJ files remain read-only under the configured source-assets root.

## Station acceptance inherited from 0.10.16

Real production PREPARE on `station_Habitat_Module_S3.obj` produced:

```text
input triangles=90162 degenerate=2
split_topology_vertices=1570
raycast_patches=1937
raycast_flipped_triangles=38982
output triangles=90160
nonmanifold_edges=0
winding_flips=0
winding_conflicts=0
inward_closed=0
```

This is sufficient to resume LOD work. Exact intermediate patch/vertex counts are not required to match the isolated spike because production preserves editor topology identity across coincident/touching sheets.

# 28. Full-asset generated LOD authoring — 0.10.18

LOD generation is an **asset-wide render-document operation**, not a filter applied only to currently visible/default meshes.

For canonical LOD0 the generator must process the complete geometry pool:

- ordinary/main meshes referenced by RenderNodes;
- additional/replacement meshes kept in the LOD-local geometry pool without RenderNodes.

Every generated level is derived independently from canonical LOD0. The first production generator pass is still conservative disconnected-component detail culling; later consolidation/simplification is a separate algorithmic stage.

The LOD panel distinguishes two decisions:

1. **VIEW** — inspect LOD0 or one generated level, optionally isolating one main or replacement mesh;
2. **USE** — select which generated levels become authored RenderLod documents.

After analysis all generated levels are selected by default. APPLY never changes LOD0. For each selected level it either replaces the existing RenderLod slot or creates the missing contiguous slot. Unselected existing slots remain untouched.

APPLY is transactional. All selected candidates are built and validated before any authored LOD is replaced. Generated documents carry `sourceKind=generated` and `generatedFromLod=0`, preserve stable base-visual / source-variant authoring ids, and receive canonical-generation fingerprints for the LODS technical gate.

After APPLY, **ЗАВЕРШИТЬ ЭТАП + КОНТРОЛЬНАЯ ТОЧКА** persists the complete authored LOD set into the LODS checkpoint. GEOMETRY therefore receives one coherent asset containing the selected main and replacement meshes at every retained/generated LOD.
