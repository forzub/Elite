## Wave7E SEMANTICS effect extraction — v0.10.73

The full 51-function SEMANTICS effect owner now lives in `src/assets/webui/model_asset_editor/effects/semantics.js`. Pure/deterministic SEMANTICS responsibilities remain in the narrow `semantics/*.js` modules and are imported directly; state/DOM/THREE/backend orchestration is injected through `createSemanticsEffects(...)`. The HTML composition root drops from 263 to 212 named functions.

Tab entry is deliberately view-state neutral. `wizardSemanticsStageSelectionNormalization(...)` supplies a normalized render projection, but opening SEMANTICS does not write that projection back to persistent semantic selection. Explicit clicks, tree selection, binding picks and LOD-switch restoration remain legitimate state mutations. A failed transition diagnostic includes the exact persistent fields that changed.

## SEMANTICS initial-render adapter repair — v0.10.71

Fresh wizard progression may enter SEMANTICS with a selected semantic node. `renderWizardSemanticsStage()` must project that selection through `wizardSemanticsSelectedPanelsModel(...)`, localized `panelText` / `relationText`, and the explicit `semanticSelectedPanels(selected,panelModel,panelText,relationText)` API. The obsolete pre-wave5M call shape is forbidden. Failures at this composition boundary emit an error-only `semantic_stage` diagnostic with selected-node identity before the exception is rethrown.

## Wave7E effect extraction — v0.10.70

`lod_runtime` is now a physical effect module. Its 36 owned functions live in `src/assets/webui/model_asset_editor/effects/lod_runtime.js`. Portable helpers are imported directly; mutable editor/runtime capabilities are injected through `createLodRuntimeEffects(...)`. Before and after every later effect move, run `check_model_asset_dependency_ownership.py`. LOD switch and LOD storage I/O boundaries emit targeted diagnostics so a partial state transition can be reconstructed from `editor_ui.log`.

# Physical module split status — wave7D

All certified portable cores are now physically extracted. PHYSICS is split into stage/node/commands; HIT VOLUMES into list/inspector/commands/render-plan; DAMAGE into stage/state-variants/node/render-selector/semantics; FINAL ASSEMBLY into validation/build/commands. Together with earlier waves, extraction proof now imports 43 portable ES modules + 9 block facades while keeping 259 certified functions / 704 frozen fixtures unchanged.

The remaining large inline code is intentionally effectful adapter/infrastructure code. The next phase is to distribute those adapters by responsibility without forcing them to become PURE, then leave `model_asset_editor.html` as layout/bootstrap/composition only.

# Physical module split status — wave7C

The physical split is active under `MOVE, DON'T REDESIGN`. In addition to the wave7A/wave7B dependency and stage cores, SEMANTICS PURE logic is now physically divided by responsibility under `src/assets/webui/model_asset_editor/semantics/`: `tree.js`, `bindings.js`, `workspace.js`, `motion.js`, `structural.js`, `world_graph.js`, `commands.js`, `preview.js`, and `graph_viewport.js`. `core/shared_forms.js` and `core/axis_mapping.js` hold the pure shared helpers required by those modules; their DOM/state adapters remain in the composition shell.

Do not merge these files back into a generic `semantics.js`. New deterministic SEMANTICS logic belongs in the narrowest responsibility module and communicates through explicit imports. Effectful state/DOM/THREE/backend orchestration remains adapter code until the later adapter split.

Current invariant: 546 named functions, 42 logical owners, 259 certified functions / 704 frozen fixtures, and 31 imported portable ES modules in extraction proof.

# Physical module split status — wave7B

The physical split is active under `MOVE, DON'T REDESIGN`. Real ES modules now contain shared/transform dependency roots plus SOURCE / LODS / GEOMETRY / SURFACES portable cores and the portable `source_maintenance` helpers they depend on. Stage adapters remain in `model_asset_editor.html` and continue to own `state`, DOM and backend effects.

Frozen architecture tests must inspect the complete source bundle (HTML + `model_asset_editor/**/*.js`) when they validate JavaScript implementation tokens/functions. Requiring a portable function to remain physically inline in HTML is no longer a valid contract. `MODULE_OWNERSHIP_CONTRACT.json` is authoritative for physical source ownership.

# Physical module split rule (wave7+)

The editor has passed logical ownership and standalone extraction proof. From wave7 onward, relocation follows **MOVE, DON'T REDESIGN**: move an ownership module to a real ES module, import its declared surface from the composition root/owner, preserve signatures and behavioural oracle, and leave algorithm changes for separate patches. Architecture tests inspect the complete source bundle rather than assuming every function remains inline in `model_asset_editor.html`.

A physical module is not complete until both runtime delivery paths contain it: editor filesystem fallback and `model_asset_editor_ui.pak`. `MODULE_OWNERSHIP_CONTRACT.json` records `physical_source` for extracted modules.

## Dependency ownership gate (v0.10.69)

Physical extraction is allowed only when dependency closure is explicit. For each named function, calls are resolved through logical ownership. Once a function is in a real ES module, every free identifier must resolve to a local/module-local symbol, ES import, explicit effect-factory port, or declared platform global. Aggregate HTML+module test-bundle visibility is not considered runtime reachability. Unresolved calls, hidden cross-file bindings/calls and unresolved external writes fail architecture tests. This gate complements purity; it does not force effect adapters to become PURE.

## Portable-module completion rule (v0.10.66 wave6E)

The editor's logical decomposition is complete when a block can be reconstructed as real ES modules using only `MODULE_OWNERSHIP_CONTRACT.json` / `PORTABLE_BLOCK_CONTRACT.json`, imported without editor globals, and run against the frozen behavioural oracle. This proof is now enforced by `tests/architecture_contracts/check_model_asset_extraction_proof.py`. Internal transitive helper calls are valid; hidden runtime wiring is not. Browser/editor effects (`state`, DOM, backend commands, THREE scene mutation, raycast, prompts/status, timers/storage) remain in owned adapters/infrastructure. The next architectural work is physical file extraction, not further decomposition for purity-count purposes.

# Elite Model Asset Editor — рабочая инструкция / архитектурный контекст

**Актуально:** 2026-09-11 · 0.10.66 portable-block decomposition + complete module ownership through wave6D
**Редактор:** `Elite Model Asset Editor 0.10.66`
**Asset format:** v4
**Текущий production pipeline:** все wizard-стадии имеют portable-core/API boundary. Wave6D дополнительно фиксирует 100% ownership browser-логики: named functions, class methods, top-level bindings, cross-module imports/exports, runtime binding imports и внешние THREE/OrbitControls dependencies. Следующий этап — wave6E standalone extraction proof; после его PASS логическая декомпозиция закрывается и начинается физическое разделение на модули.

> Этот файл является источником контекста для продолжения работы над Model Asset Editor.
> Старые предположения из эпохи format v2/v3 о единой `Node -> GeometryDefinition -> LOD0/LOD1` структуре больше не применять к v4.

## Portable-block rule (0.10.66)

Архитектурная цель — не максимальное число отдельных `PURE` функций, а переносимый функциональный блок с жёстким API. Внутренние функции блока могут вызывать друг друга транзитивно сколько угодно; запрещены скрытые провода наружу. Все данные входят через аргументы/явные imports, все результаты выходят через return. `state`, `editorViewState`, DOM, prompt/confirm, THREE scene mutation, timers/storage/network и backend `send()` принадлежат adapter-слою.

Текущая карта: SOURCE / LODS / GEOMETRY / SURFACES / SEMANTICS — portable; PHYSICS / HIT VOLUMES / DAMAGE / FINAL ASSEMBLY — portable foundation. `MODULE_OWNERSHIP_CONTRACT.json` фиксирует полный owner/import/export/binding map для всего browser editor. Перед физическим разнесением остаётся ровно один логический gate: standalone extraction proof каждого portable блока.

## 0.10.34: два жёстких UX/placement-инварианта

1. **Selection не сбрасывает scroll.** Любой list/table/tree сохраняет текущий viewport при выборе строки, обновлении inspector и metadata rerender в том же asset/stage/LOD контексте. Новый scrollable container обязан подключаться к общему `data-preserve-scroll` контракту.
2. **Instance authoring не имеет права менять placement сам по себе.** Consolidate/duplicate/radial array могут менять geometry sharing или создавать новый RenderNode, но матричный transform должен сериализоваться обратно в тот же XYZ transform без дрейфа. Matrix→Euler encoding использует только extractor, обратный `eulerAngleXYZ`; generic quaternion Euler extraction для asset transform запрещён.

GEOMETRY также имеет явные repair/authoring инструменты для уже существующего экземпляра: относительный `MOVE XYZ` и `MOVE IN RADIAL ARRAY` на заданное число слотов. Эти команды **перемещают существующий RenderNode и не создают копию**.

---

# 0A. Текущее состояние 0.10.16 — libigl + Embree production preparation

`0.10.16` завершает эксперимент с самодельной absolute-orientation эвристикой. Продуктом `ПОДГОТОВИТЬ МЕШИ` остаётся **исправленный working MeshLod**, но topology/orientation authority теперь делегирован проверенным geometry-processing библиотекам.

## SEMANTICS tree/link authoring contract (0.10.37)

SEMANTICS is asset-wide, but the transform structure is now a **forest**, not a mandatory single-root tree. `Node::parentIndex == NoIndex` means the part is authored directly in **ASSET SPACE**. Multiple top-level semantic parts are valid. Parent→child exists only when the child transform must really follow the parent; reparenting preserves the moved subtree root's world pose. The joint stored on a child Node remains the incoming transform-link control (FIXED / ROTATE / DETACH / ROT+DETACH). Structural support and physical detach authority belong to STRUCTURAL GRAPH, not to the transform parent.

The viewport may show transform parent-child lines and explode preview without altering saved transforms. With one transform root it may be used as the preview center; with several ASSET SPACE parts the asset origin is the preview center. RenderNode bindings remain LOD-local and are summarized per LOD for the selected semantic node. SEMANTICS CHECK accepts any acyclic transform forest with valid bindings/payload; it no longer requires a unique semantic root. `FLATTEN STATIC → ASSET SPACE` removes only safe static transform edges while preserving world pose, and `SELECTED → ASSET SPACE` is the explicit manual equivalent.

**0.10.54 transform-forest invariant:** ASSET SPACE is the implicit transform parent and is not a semantic Node. Top-level parts are ordinary semantic parts, not special ROOT objects. The temporary transform audit UI is removed; its conservative rule is encoded in the flatten command: FIXED, non-breakable, no state-transform/detached state, and no inherited dynamic transform. Structural graph, sockets, collisions, bindings and node indices are not rewritten by flatten.

**0.10.55 hotfix invariant:** socket markers are SEMANTICS-only; SOURCE CHANGE SCAN is provenance-only and normally does not page unloaded `.elmesh` payloads into memory; static semantic flatten verifies resident geometry fingerprints before and after so it cannot silently mutate winding/normals/mesh coordinates.

**0.10.38 binding invariant:** GEOMETRY duplicate/circular-copy operations create new visual RenderNodes but never copy semantic identity. New copies start UNBOUND and must be assigned explicitly in SEMANTICS. `VIS` in the tree is the count of active-LOD RenderNodes bound to that semantic node: `0 VIS` is legitimate for a semantic-only grouping root, while an ordinary visible part with `0 VIS` usually means its visual is currently bound elsewhere. A selected semantic node can repair this by entering visual-pick mode and clicking the intended mesh in the 3D viewport. Selection follows desktop tree conventions: click = one, Ctrl = toggle one, Shift = contiguous visible-tree range, Ctrl+Shift = add range.


**0.10.41 semantic interaction invariant:** parent/reorder/joint/frame edits are semantic-only deltas. They must not call full asset metadata serialization or scan render triangles. Collapsing a branch hides the entire descendant subtree; hidden descendants must never reappear through fallback tree enumeration. 3D semantic graph anchors use precomputed geometry metadata bounds transformed by current RenderNode placement, not runtime vertex scans.

**0.10.42 3D semantic graph invariant:** graph layout and exploded visual placement must use one canonical anchor snapshot derived from authored RenderNode transforms plus geometry metadata bounds. Overlay node/link objects are persistent and update in place; slider/motion hot paths must not rebuild/dispose the graph object set or read already-exploded viewport matrices back as layout input. The selected joint gizmo is a separate overlay layer.


**0.10.43/0.10.45 runtime-overlay invariant:** SEMANTICS graph selection/explode/motion uses `semanticDescendantSet` as the only subtree authority. Collision EDIT/PICK authority remains PHYSICS/DAMAGE, but the global Hit Volumes toolbar toggle controls viewport visibility in any loaded 3D stage; visible collision overlays follow semantic preview transforms without intercepting SEMANTICS selection. ROOT nodes have no incoming joint gizmo.

**0.10.44 radial-graph invariant:** semantic explode has one spatial rule: with exactly one semantic root, its canonical visual center (or its semantic frame if it has no visual) is the fixed center; before a single root exists, world origin `(0,0,0)` is the temporary center. Every other logical part moves only along the ray from this center to its canonical visual center, and displacement is monotonic in original radius. Graph markers/links are created hidden and may become visible only after a successful update; uninitialized unit cubes are never a valid preview state.
**0.10.45 semantic-link authoring invariant:** the incoming joint belongs to the parent→child link, not to either mesh. `NodeJoint::pivot` and `axis` remain stored in the child semantic-local frame; the editor must hide that coordinate-system detail behind explicit authoring tools: parent-origin preset, child-visual-center preset, or exact 3D surface pick. Preview speed is editor-only; `defaultRateDegPerSec` is a runtime property. Base semantic Position/Rotation/Pivot are advanced logical-frame data, not mesh-placement controls. `VISUAL REPRESENTATION / LOD BINDINGS` is repair/identity metadata and should stay secondary when all loaded LODs are already bound.

**0.10.45 radial-support invariant:** explode direction remains the ray from the single ROOT center (or world origin before a single root exists) to the logical part visual center, but explode magnitude is based on the farthest transformed visual-bounds point projected along that ray. This keeps inner/nearby details closer while a large outer ring travels farther even when their centers are close.


**0.10.40 lifecycle invariant:** semantic identity and render representation have separate lifecycles, but their relationship is governed by one shared `ModelAssetSemantics` authority. The SEMANTICS tree lists logical/gameplay parts, not meshes; names may initially match LOD0 OBJ names only because SOURCE creates a starter logical identity for each initial LOD0 visual. A logical part with no visual is legal only when it still has a real role (children, socket/collision/state/damage/physics ownership). A leaf with no visual and no role is an ORPHAN and SEMANTICS CHECK must reject it. Deleting a semantic part never deletes geometry: its RenderNodes become UNBOUND and are repaired/reused separately. SOURCE must not create collision/physics payload; that belongs to PHYSICS.

The semantic tree is a real authoring tree: depth is shown by indentation/connectors, branches may collapse, and drag/drop supports BEFORE / INSIDE / AFTER. Reordering siblings is editor-only stable-ID presentation metadata and must never reorder the runtime semantic-node vector or change runtime indices. Ordinary selection must use partial UI refresh rather than rebuilding the whole tree.

ROTATE/DETACH are properties of the incoming parent→child link. Their preview always starts from canonical current RenderNode transforms, applies motion/detach to the whole semantic subtree, and never persists preview transforms. `3D SEMANTIC GRAPH` is also preview-only; its explode anchors are world-space centers of the actual rendered meshes (not RenderNode origins), which is required for folder-authoritative OBJ geometry with baked vertex placement.


## GEOMETRY workspace contract (0.10.36)

GEOMETRY is a per-RenderLod authoring stage, but the UI must expose one coherent workflow rather than the historical order in which tools were added. The active LOD selector is sticky at the top of the right workspace and is paired with `WHOLE MODEL / RECENTLY LOADED / CHANGED`. The recent-work option is disabled when the active LOD has no local maintenance debt.

The visible order is fixed:

1. **СРАВНЕНИЕ / СВЕДЕНИЕ В ЭКЗЕМПЛЯРЫ** — the primary RenderNode list and selection authority. Each row keeps the full RenderNode id plus geometry/source OBJ identity readable; long names wrap to a second line instead of being silently ellipsized. `VIEW` checkboxes isolate one element or a checked group in the viewport; with no checks the whole model is visible. `REF` is a radio reference for rigid duplicate comparison. `CLEAN UNUSED` belongs to this group because unused geometry is a consequence of consolidation.
2. **Замены дополнительными мешами** — source variants and their compatible base visual families. Additional-mesh and source-file names follow the same readable two-line rule.
3. **РЕДАКТИРОВАНИЕ** — exactly one selected RenderNode, chosen either from the table or by viewport click. Exact Position/Rotation/Pivot, relative MOVE XYZ, one circular MOVE/ARRAY dialog, duplicate, break instance, advanced/manual geometry repair and delete all act on this one authority.
4. **CHECK GEOMETRY** — validates the current in-memory stage and unlocks SURFACES; it never saves.
5. **LOD statistics** — informational counts only, placed below CHECK so they do not interrupt the authoring workflow.

The old visible `LOD GEOMETRY / PREVIEW`, separate `INSTANCE / ARRAY AUTHORING`, GEOMETRY-time `Render LOD files`, separate selected-element inspector and detached delete group are not part of the GEOMETRY workflow anymore. Their functionality is either merged into the primary table/editor or belongs to LODS/diagnostics.

Selection/rerender must preserve every list scroll position. This applies equally to row clicks, viewport selection, reference changes, VIEW checkbox changes and backend metadata refreshes.

## UI runtime/package contract (0.10.23)

`EliteGame` and `EliteAssetEditor` no longer share a universal `elite_ui.pak`. The game owns `assets/ui/elite_game_ui.pak`; the editor owns `build/tools/model_asset_editor/assets/ui/model_asset_editor_ui.pak`. `HtmlUiServer` never discovers a pack by convention: the executable passes the exact path it owns.

The Model Asset Editor runtime root is the same stable artifact root that owns its executable/workspaces: `build/tools/model_asset_editor`. Its filesystem fallback contains only the editor document and required Three.js modules. The editor build does not depend on the game `copy_assets` tree. Legacy `elite_ui.pak` files are migration debris and are removed by the new pack build commands. A stale game pack therefore cannot shadow a newer editor HTML again.

Wizard stage entry is also a single transaction. Manual tab clicks and automatic initial-wizard stage progression both use `setWizardStage`; leaving an LOD preview clears its transient state without rebuilding first, and GEOMETRY does not rebuild the whole station merely because its tab was selected. A scene rebuild occurs only when the viewport representation actually changes (for example generated-preview → authored geometry, SURFACES material preview on/off, or a viewport-mode change).

## SURFACES workspace contract (0.10.22)

SURFACES is downstream of GEOMETRY and is side-effect free until the author explicitly presses `ANALYZE SURFACES`. Opening or revisiting the tab must not start topology work. One completed analysis remains usable while the upstream SOURCE/LODS/GEOMETRY input is unchanged; upstream authored changes invalidate that cached SURFACES analysis.

The author then chooses a RenderLod and geometry and resolves one of four production surface intents: `ClosedVolume`, `ThinOneSided`, `ThinTwoSided`, `BreachedVolume`. `ClosedVolume`, `BreachedVolume` and `ThinOneSided` render `FrontSide` with back-face culling enabled. `ThinTwoSided` renders `DoubleSide` with back-face culling disabled. The intent is the authority for sidedness; `MaterialDefinition::twoSided` remains only a legacy/binary-compatible field and is not an ordinary SURFACES authoring control.

A default-on `APPLY TO ... ALL LODS` option propagates the chosen intent to the same stable visual family wherever it exists. Matching uses stable base-visual identity for ordinary geometry and stable variant identity for replacement geometry; transient `G#` indices and coincidental per-LOD geometry indices are never cross-LOD identity. The batch publishes one targeted metadata patch and does **not** run topology analysis. `ANALYZE SURFACES` remains the explicit expensive audit. This is useful for station modules whose physical surface class is identical across LODs while still allowing the checkbox to be disabled for deliberately different ship/damage representations.

Material assignment stays per triangle and explicit materials stay in the shared asset material table, but `Triangle::materialIndex == NoIndex` is now the **valid implicit DEFAULT visual surface**, not a SURFACES error. This matches the planned renderer: most hull triangles need no individual physical/PBR material at all, while sparse explicit material assignments mark special visual roles such as emissive windows, navigation lights or deliberately coloured regions. Only a non-`NoIndex` reference outside the material table is invalid. The existing assign command may replace implicit DEFAULT on a geometry with a chosen explicit material, but the editor never mass-creates a dummy material merely to satisfy validation. Material editing still retains base RGBA/emissive/PBR-compatible fields for binary compatibility and future styles; they are optional appearance data, not a requirement that every triangle be PBR-authored.

SURFACES never changes topology, transforms, instance sharing or replacement compatibility. Surface/material edits invalidate SURFACES and later stage validity only; completed LODS and GEOMETRY remain valid.

Explicit surface-intent changes are metadata-only operations. `ClosedVolume / ThinOneSided / ThinTwoSided / BreachedVolume` must not scan triangles, run PREPARE/preflight, rebuild mesh buffers, resend geometry payloads or rebuild the complete Three.js scene. The backend publishes only the affected geometry metadata and the browser updates sidedness on the resident mesh. `AUTO` may inspect the selected geometry because automatic classification itself requires topology evidence.

## Render-material intent / authoring split (0.10.26–0.10.27)

The production material contract is **semantic/sparse**, not a commitment to a realistic PBR renderer. `NoIndex` means ordinary DEFAULT surface and is sufficient for the bulk of a ship/station in Elite-classic, monochrome/dissolve and anime render styles. Explicit material slots exist only where the renderer needs a visual distinction. Blender/OBJ is the preferred place to partition faces into material groups because face selection, window layout, emissive panels and colour-region layout are authoring tasks; the editor preserves/imports those groups and owns stable runtime ids/properties.

For the anime renderer, source RGB is not the long-term authority. The intended production gate is a small approved palette / stable visual-role mapping: Blender authors which faces belong together, then the editor validates that imported non-emissive colours map to allowed palette roles and may offer an explicit author-approved snap/remap operation. It must not silently recolour geometry during import/CHECK. Emissive surfaces use a separate emissive role/palette. A facade with hundreds or thousands of lit windows should normally be authored in Blender as emissive faces/material groups or an emissive mask/atlas; it must **not** create one real light per window. Point/spot illumination is reserved for semantic light sockets such as a searchlight or a small number of gameplay-relevant lamps. Beacons/sirens combine emissive visual geometry with state/animation/VFX; engine exhaust and explosions remain VFX. Ordinary emissive surfaces do **not** imply bloom/glow: high-contrast colour/emission is the default cheap presentation. Global image softening/blur is a renderer/post-process concern used to move the picture away from a raw 3D-editor look. Haze/glow is reserved for explicit rare environmental/VFX states (for example an anomalous murky field around lost generation ships), not encoded as a normal material requirement.

## Runtime screen-space LOD contract (0.10.27)

The existing generator's `2 px` visibility idea is now an authored/runtime contract rather than a source-unit distance heuristic. For every generated `RenderLod N>0` the editor stores:

`relativeGeometricError = omittedFeatureCharacteristic / completePlacedLod0Characteristic`

The ratio is dimensionless, so a station that is 2.3 Blender units but several kilometres in the game carries exactly the same authored LOD metadata. `LOD0` has error `0`. A manual/legacy LOD with unknown error uses a negative sentinel and is never automatically selected past that boundary until an error is authored. Generated errors must be non-decreasing with coarser LODs.

At runtime the renderer measures the **final world-scaled object's projected characteristic size in pixels** after gameplay scale, camera projection/FOV and viewport size are known. It then evaluates `projectedErrorPx = relativeGeometricError * projectedCharacteristicPixels`. The shared selector chooses the coarsest LOD whose error is safely below the 2-pixel target and uses hysteresis (`1.8 px` to coarsen, `2.2 px` to refine) to prevent LOD chatter. A perspective size/distance helper exists only as a convenience; if the renderer can project the final world-space bounds directly, that projected size is preferred.

The value is stored in a new optional v4 manifest chunk `LERR`. The asset format version stays **v4**, existing `LODS` and `.elmesh` layouts are unchanged, and older readers skip the unknown chunk. The legacy game `ObjectAssembly::lodSwitchDistance` path remains compatibility-only until EliteGame switches from OBJ assemblies to compiled `.elmodel` render LODs; it must not become a competing authority for new v4 assets.


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

For every geometry the log records input cleanup counts, source/canonical non-manifold evidence, `split_topology_vertices`, `raycast_patches`, `raycast_flipped_triangles`, output topology/winding state, render-vertex/edge rebuild counts and exact failure reason. The saved WORKING `editor_state.json` persists the same libigl/Embree counters beside the canonical fingerprint when the user presses SAVE.

## 0A.6 Runtime boundary

libigl, Eigen and Embree belong only to the offline `EliteAssetEditor` and optional diagnostic spike. `EliteModelAsset`, client/server game runtime and `RuntimeMeshNormalizer` do not link these libraries. Runtime remains tolerant; production authoring remains stricter.

# 0. Изменения 0.9.3: синхронизация UI и контрольные точки

## 0.1 Mesh не гоняется при обычных командах

Полный geometry payload (`positions`, `normals`, `indices`, `edges`) передаётся из C++ backend в browser только когда geometry действительно загружается или заменяется:

- первое открытие asset;
- source reimport;
- RESTORE the last saved WORKING ASSET;
- load/reload LOD;
- операция, реально меняющая vertex/index payload.

Обычные authoring-команды используют `asset_metadata` и **не имеют права повторно посылать неизменившийся mesh**. Это относится как минимум к transforms/pivots, geometry binding, instance consolidation, duplicate/radial instances, semantic state metadata, collision, sockets, hit/opening/repair metadata.

Browser сохраняет mesh arrays и `THREE.BufferGeometry` в cache по стабильному ключу `LOD + RenderGeometryDefinition.id`. Metadata refresh может перестроить лёгкий scene graph, но не пересоздаёт неизменившиеся GPU geometry buffers. После `break instance` новая unique geometry локально клонируется из уже загруженного mesh; изменение edge mask передаёт только изменившийся mask.

### 0.1.1 Транспорт geometry с 0.10.24

JSON остаётся только control-plane для маленьких команд и metadata. Bulk geometry (`positions`, `normals`, triangle indices/materials/smoothing groups, authored edges и RAW diagnostic snapshot) передаётся отдельным versioned binary WebSocket frame `ELWIR001`.

Это **не новый viewport contract** и не новый asset format. Backend сначала посылает маленький descriptor (`asset_binary_begin` или `lod_payload_binary_begin`), затем binary geometry. Browser transport-adapter проверяет `LOD index + geometry index + stable geometry id`, подставляет typed arrays в descriptor и только после полной сборки вызывает старый application terminal `asset` / `lod_payload`. Поэтому `acceptAssetState(..., true)`, `state.asset.renderLods[...]` и существующий `rebuildScene()` остаются владельцами поведения отображения.

Если операция изменила только один/несколько уже известных LOD, `asset_binary_begin` может быть transport-delta: неизменившиеся LOD payload не пересылаются, а берутся из уже resident browser state. Это **не delta application-state**: перед вызовом старого `asset` handler adapter обязан собрать полный объект и строго проверить совпадение asset/LOD/geometry identity. При первичной загрузке, reconnect, RESTORE или полном mesh rewrite отправляется self-contained snapshot всех resident LOD.

Binary decoder оставляет bulk arrays typed (`Float32Array` / `Uint32Array` / `Int32Array`). Это требует явного terminal-adapter там, где старый API различает обычный JS `Array` и typed view: в частности `THREE.BufferGeometry.setIndex()` должен получать `THREE.BufferAttribute`, а не голый `Uint32Array`. Этот контракт защищён architecture regression.

Для профилирования reimport backend пишет `[ModelAssetEditor][perf]` / `[ModelAssetEditor][transport]` с временем source import, additional-source import, metadata serialization, binary encode и размером wire payload; browser console отдельно пишет decode/legacy-terminal время. Это позволяет отличить тяжёлый OBJ/topology import от transport/viewport задержки.

`ПОДГОТОВИТЬ МЕШИ` остаётся полностью backend-операцией: browser посылает только маленькую команду, CanonicalMeshBuilder/libigl/Embree работают над resident `RenderLod` в C++ памяти. После PREPARE transport публикует только LOD, где реально изменились mesh bytes; повторный `changed=0` проход синхронизирует только metadata. Session-only RAW snapshot не входит в обычный payload и запрашивается отдельно только при выборе viewport-режима `ИСХОДНИК`. Backend пишет `[ModelAssetEditor][prepare]` отдельно для каждой реально обрабатываемой geometry и общий `compute_ms / changed_lods / publish_ms`.

`.elmodel/.elmesh v4` на диске не меняется. `.elmesh` при чтении теперь забирается одним большим блоком в память и разбирается memory cursor-ом через тот же бинарный layout вместо миллионов мелких `istream::read()`.

### 0.1.2 WORKING ASSET: простой SAVE / RESTORE (0.10.33)

Для каждого asset существует ровно **одно сохранённое рабочее состояние**:

```text
build/tools/model_asset_editor/workspaces/<asset>/working/
    <asset>.elmodel
    <asset>.lod*.elmesh
    editor_state.json
```

Порядок OPEN детерминирован:

```text
OPEN / restart
    → load saved WORKING ASSET, if it exists
    → otherwise load production and create the initial saved WORKING ASSET
    → otherwise import SOURCE and create the initial saved WORKING ASSET
```

После открытия все изменения живут в памяти и поднимают общий `dirty`. Никакого фонового autosave нет. В верхней панели во всех стадиях доступны две глобальные операции:

```text
SAVE
    → enabled only when dirty
    → save the complete coherent WORKING ASSET
    → clear dirty
    → production is untouched

RESTORE
    → enabled only when dirty
    → discard every unsaved in-memory change
    → reload the last saved WORKING ASSET
    → production is untouched
```

Отдельных `SAVE MANIFEST`, `SAVE LOD`, checkpoint/snapshot/rollback history и `wizard_state.json` как второго persistence authority **нет**. Source reimport, maintenance import, PREPARE, LOD generation и metadata authoring сами себя не сохраняют: после любой мутации автор явно выбирает SAVE или RESTORE.

`working/editor_state.json` хранит editor-only identities, SOURCE fingerprints, preparation evidence, maintenance debt и stage validity вместе с тем же saved working revision. Package stamp защищает от применения sidecar к другим `.elmodel/.elmesh` bytes.

### 0.1.3 UX-инвариант: выбор не сбрасывает список (0.10.34)

Для **любого** списка, дерева или таблицы Model Asset Editor действует железное правило: выбор элемента, обновление inspector, локальный backend response или rerender текущей стадии **не имеет права сбрасывать scroll-позицию** и заставлять пользователя снова листать от начала.

Scrollable-контейнеры получают стабильный `data-preserve-scroll` key. Позиция хранится с контекстом asset / stage / LOD; asset-wide списки могут использовать asset scope. `renderWizardPanel()` выполняет DOM rebuild через общий `preserveUiScroll()` transaction: перед заменой DOM сохраняются `scrollTop/scrollLeft`, после rebuild и на следующем animation frame они восстанавливаются.

Новый scrollable list/table/tree нельзя добавлять как голый `overflow:auto`: он обязан подключаться к этому контракту. Исключение — осознанная смена контекста пользователем (другой asset/stage/LOD), где используется отдельная scroll-позиция.

## 0.2 Stage CHECK — только проверка и progression

В конце каждой стадии одна операция `CHECK`:

```text
CHECK stage S
    → run checks owned by S
    → PASS: mark S green in current state and unlock the next stage
    → FAIL: mark S NEEDS FIX and report concrete blockers
    → never save WORKING ASSET
    → never write production
```

Если CHECK изменил stage status, это обычное изменение текущего editor state: глобальный SAVE становится активным и пользователь сам решает, фиксировать ли его. Изменение upstream данных снова делает затронутую стадию и downstream `stale`.

Полный `RELOAD ALL SOURCE MESHES` для уже открытого asset сохраняет историю stage validity через сам reimport и затем применяет обычную invalidation от SOURCE. Поэтому ранее пройденные стадии становятся `STALE`, а не `NOT STARTED`; `NEEDS FIX` не маскируется. Это не отменяет progression gate: после изменения SOURCE его CHECK надо пройти заново, прежде чем продолжать первоначальную последовательность проверок.

`VALIDATE` использует ту же кнопку CHECK, но дополнительно показывает полный production validation report. Отдельной второй кнопки `RUN FULL VALIDATION` нет.

`BUILD` — единственное исключение: это terminal production action. Перед BUILD рабочее состояние должно быть сохранено (`dirty == false`). BUILD повторно проверяет production contract и записывает именно сохранённый WORKING ASSET в production `.elmodel/.elmesh`, после чего обновляет `production_state.json`.

При первом build порядок остаётся `SOURCE → LODS → GEOMETRY → SURFACES → SEMANTICS → PHYSICS → DAMAGE → VALIDATE → BUILD`. После появления production package maintenance editor может открыть нужную стадию напрямую; блокеры определяются current state/debt, а не историей сохранений.

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
- сохранять текущий WORKING ASSET только через глобальный SAVE.

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
4. `SURFACES`

`SEMANTICS / PHYSICS / DAMAGE / VALIDATE / BUILD` входят в общий порядок invalidation/stage validity. Semantic hierarchy/joints/sockets инвалидируют `SEMANTICS`, rigid-body/collision authoring — `PHYSICS`, state variants/render-state selectors/hit regions/openings/repair targets — `DAMAGE`.

## SAVE / RESTORE

В редакторе нет истории snapshots. Глобальный SAVE сохраняет единственный WORKING ASSET; RESTORE выбрасывает несохранённую ветку и возвращает последний SAVE. Stage CHECK не выполняет persistence I/O. Production меняется только через BUILD.

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
Cannot save model asset:
empty/duplicate render node id in LOD0
```

На station ошибка возникла не в пути `station.elmodel` и не в самой операции сохранения.

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

Сохранение было только местом обнаружения.

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
stage CHECK says OK
↓
SAVE/BUILD ModelAssetBinary validation says INVALID
```

Нужен единый reusable validator, которым пользуются:

- importer postflight;
- migration postflight;
- wizard preflight;
- Save binary;
- Save LOD;
- global WORKING SAVE / BUILD;
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

Stage CHECK должен проверять все serializer invariants, относящиеся к уже существующему состоянию, до SAVE/BUILD.

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

## SAVE / CHECK / RESTORE

- CHECK SOURCE/LODS/GEOMETRY ставит зелёный PASS и открывает следующую стадию;
- CHECK ничего не сохраняет;
- SAVE фиксирует полный WORKING ASSET;
- RESTORE возвращает последний SAVE и отбрасывает несохранённые изменения;
- upstream edits делают затронутую стадию и downstream stale.

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

## Test C — station stage checks

Полный imported station должен успешно пройти:

```text
SOURCE CHECK
LODS CHECK
GEOMETRY CHECK
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
- global WORKING SAVE / RESTORE and stage CHECK.

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
5. вызывать preflight в stage CHECK до SAVE/BUILD;
6. улучшить diagnostic: ID + indices + LOD;
7. добавить exact regression для `station_solar_panels`;
8. прогнать ModelAssetBinary tests + architecture contracts;
9. reimport Orbital Station;
10. повторить SOURCE -> LODS -> GEOMETRY CHECK.

Этот список закрыт в `0.9.1`; он сохранён ниже только как история архитектурного решения.

---

# 21. Короткая памятка

Если снова появляется:

```text
Cannot save model asset: ...
```

не считать SAVE/BUILD источником producer-ошибки.

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
- wizard CHECK вызывает preflight без сохранения;
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

- количество positions/instances;
- total angle;
- axis X/Y/Z;
- center: selected pivot / parent origin / custom XYZ.

Круговое размещение использует одно окно и одно поле общего количества. `count = 1` означает MOVE: выбранный RenderNode поворачивается вокруг center на заданный угол без создания копии. `count >= 2` означает ARRAY: выбранный RenderNode остаётся первой позицией 0°, а создаётся ровно `count - 1` новых instances. Для незамкнутой дуги конечная точка включается: 120° / 2 объекта = 0° и 120°; 120° / 4 объекта = 0°, 40°, 80°, 120°. Только точный ±360° считается замкнутым кругом и распределяется без дубля 0°/360°. По умолчанию center = parent origin. Если выбран `selected pivot`, geometry-local pivot сначала переводится в parent coordinates; raw `node.pivot` нельзя напрямую трактовать как orbit center.

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

This spike does **not** change `EliteAssetEditor`, WORKING persistence or `.elmesh`. It runs:

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

The RAW snapshot exists only in the editor session and is never written into `.elmodel` or `.elmesh`. Technical preparation evidence is written to `build/tools/model_asset_editor/workspaces/<asset>/logs/mesh_repair.log` and replaced at the start of each PREPARE run; the saved WORKING `editor_state.json` stores cleanup counts, split topology vertex count, raycast patch count and raycast-flipped triangle count.

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

If yes, `ANALYZE LOD0` and the LODS CHECK are allowed.

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
            working/
                <asset>.elmodel
                <asset>.lod*.elmesh
                editor_state.json
            production_state.json
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

After APPLY the authored LOD set remains dirty in memory. Press global SAVE to persist it. LODS CHECK validates the current state and unlocks GEOMETRY; it does not save anything.


### Fast source-change scan (0.10.56)
`SCAN SOURCE CHANGES` does not reread all authored OBJ bytes. Imported/adopted revisions keep the existing exact fingerprint and an editor-only quick metadata stamp. Normal scans compare quick stamps only. Older saved workspaces that predate quick stamps are shown as `LEGACY BASELINE · REIMPORT ONCE`; reimport only the relevant row(s) to establish the fast baseline.
