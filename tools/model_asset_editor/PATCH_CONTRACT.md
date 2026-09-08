# Elite Model Asset Editor — patch contract

This file is a regression contract for changes to `tools/model_asset_editor` and its WebUI. A patch that changes one of these boundaries must update the matching tests and this document deliberately; it must not silently restore an older path as a fallback.

## 1. Persistence authority: exactly one WORKING save

- The editor has one mutable persisted resume state per asset below the configured `workingFilesRoot`:
  - `<workingFilesRoot>/<asset>/working/<asset>.elmodel`;
  - its `.lodN.elmesh` payloads;
  - `<workingFilesRoot>/<asset>/working/editor_state.json`.
- There is no automatic checkpoint/snapshot/save-history branch. `SAVE` overwrites the current WORKING state. `RESTORE` discards unsaved edits and returns to that one saved WORKING state. `BUILD` writes production only from a saved WORKING revision.
- `CHECK`/validation is separate from persistence. A successful or failed CHECK must not save, overwrite, advance or synthesize a WORKING revision.
- Every successful WORKING save increments one monotonic `saveRevision` and writes `savedAtUtc`. The revision belongs to the save content, not to an ephemeral editor session.
- The WebUI status/toolbar must expose the loaded revision as `WORKING rN`. Do not replace it with a filesystem path. Paths are diagnostics/tooltips/progress information only.
- `workingFilesRoot` is an explicit editor setting. Changing it changes where subsequent WORKING loads/saves occur; it does not search old roots or silently migrate/copy old saves.

## 2. Editor-only state that must accompany WORKING and production

`working/editor_state.json` and the production editor sidecar carry authoring state that is intentionally not part of the runtime `.elmodel` semantic contract.

They must persist at least:

- `schemaVersion`;
- `editorVersion`;
- `saveRevision`;
- `savedAtUtc`;
- `sourceAssetDirectory` (the asset-level folder identity used for SOURCE geometry);
- global `stages` validity;
- `aggregateStageChecks` for every editor stage;
- `meshSourceRecords` for every source-backed mesh.

A `meshSourceRecord` is keyed by LOD + stable geometry id and contains:

- `sourceFileName`;
- `sourcePath`;
- exact `sourceHash` for the accepted SOURCE revision;
- `stageChecks` for every editor stage, each `passed`, `failed`, or `not_checked`.

A SOURCE replacement/new import clears all stage checks only for the affected mesh. Unchanged meshes retain their prior evidence. A successful stage CHECK certifies the current resident SOURCE meshes for that stage. A failed CHECK must not erase already-passed evidence for unaffected meshes; records still awaiting that stage become failed.

A source-backed mesh is considered validation-pending until **all** editor stages, including terminal VALIDATE/BUILD, are `passed` for its current SOURCE revision.

## 3. OPEN lifecycle and LOD residency

Authority order remains explicit:

1. If WORKING exists, ordinary OPEN resumes WORKING.
2. Otherwise, if production exists, production becomes the initial WORKING basis.
3. Otherwise, or on explicit whole-asset SOURCE reimport, import SOURCE.

OPEN/RESTORE must make every **declared** render LOD resident before the asset is presented to the viewport, regardless of SOURCE authority. The editor must never recreate a state where LOD0 is resident while a declared LOD1+ remains hidden/unloaded merely because no tab has requested it yet. Per-LOD edit operations may mutate only their target LOD; that isolation must not be implemented by making residency lazy.

Initial folder SOURCE import discovers every contiguous `LOD0`, `LOD1`, `LOD2`, ... directory and all ordinary OBJ files directly in those LOD directories. `variants/**/*.obj` are additional/replacement meshes and are discovered separately.

If a new authored LOD folder appears after a WORKING save, `SCAN SOURCE CHANGES` may add the new resident LOD document; ordinary OPEN does not silently mutate a saved WORKING package merely because SOURCE changed.

Runtime-registry assets that have no folder geometry authority may retain their compatibility behavior. A runtime descriptor is allowed to bootstrap semantic identity, but must never act as a geometry allow-list when an authored Folder SOURCE exists.

## 4. Geometry authority vs semantic bootstrap

`CatalogSourceAuthority` answers **where geometry is authoritative**. `CatalogBootstrapMode` answers **how initial semantic identity may be bootstrapped**. They are different axes.

For canonical Cobra when its authored folder exists:

- geometry authority = `Folder`;
- semantic bootstrap may = `RuntimeAssembly`;
- catalog label must say `[SOURCE]`, not `[runtime]`;
- source scan/reload must route to the folder, not return through a runtime-registry gate.

A runtime assembly descriptor may seed module/semantic identity. It must not suppress an ordinary OBJ that exists in the selected Folder SOURCE but is absent from the C++ registry descriptor.

## 5. RELOAD LOD is not RELOAD FROM SOURCE

- `LOAD LOD` / `RELOAD LOD` operate on the saved binary package: WORKING if it exists, otherwise production. They read `.elmesh`; they never read OBJ/MTL.
- `RELOAD FROM SOURCE` is a separate per-mesh operation. It imports exactly that linked SOURCE mesh, replaces only that geometry payload, preserves stable render/semantic/physics identity where still valid, clears that mesh's stage evidence, and leaves the asset dirty until SAVE.
- Generated meshes without a SOURCE link do not get SOURCE reload.
- Source reload/scan must not silently mutate sockets, collisions, joints, semantic ownership or unrelated meshes.

## 6. Folder SOURCE path is exact

For `CatalogSourceAuthority::Folder`:

- the selected asset folder is persisted as `sourceAssetDirectory`;
- scan/reload first use that saved folder identity; catalog discovery is bootstrap only;
- linked SOURCE files must resolve inside that selected asset root;
- modern Folder SOURCE maintenance must not use the legacy multi-candidate resolver as a fallback;
- a wrong/escaped/missing path is an error, not a reason to try a same-named file elsewhere.

Legacy runtime-registry assets may keep their resolver only until they are migrated to Folder geometry authority.

## 7. SCAN SOURCE CHANGES = exact-hash synchronization

`SCAN SOURCE CHANGES` is no longer a passive metadata report. It is the bounded SOURCE synchronization operation requested by the editor contract.

### Inventory phase

- Resolve the `sourceAssetDirectory` recorded by the loaded save (fall back to catalog folder identity only for legacy state that predates this field).
- Enumerate each authored LOD directory once and the `variants` subtree once.
- Discover ordinary OBJ files directly in each LOD directory and replacement/additional OBJ files below `variants`.
- Do not read `.elmesh` and do not change LOD residency to perform the scan; Folder OPEN must already have declared LODs resident.

### Comparison authority

Each discovered source mesh receives an exact content hash. Matching is by:

`LOD index + ordinary/variant class + case-folded source filename`.

Behavior:

- filename exists in WORKING and hash is equal -> **do nothing** to mesh payload, stable identity, authoring checks or selection;
- filename does not exist in WORKING -> import it as a **new** mesh and clear all stage checks for that new mesh;
- filename exists but hash differs -> import SOURCE, replace the existing geometry payload under the same stable geometry identity, and clear all stage checks for that mesh;
- tracked filename is absent from SOURCE -> keep the WORKING mesh, mark its SOURCE check failed, and report `missing_source`; never silently delete user geometry;
- ambiguous duplicate filenames in the same LOD/class are an error; do not guess.

A newly discovered authored LOD may be created as a resident source LOD. Generated geometry at the same LOD level must not be blended with the authored source authority.

### Forbidden work

The comparison loop must not call:

- `ModelAssetBinary::load*` or read `.elmesh`;
- PREPARE, ANALYZE, canonicalization or repair;
- topology/weld repair passes;
- `ensureAllLodsLoaded()` / `ensureLodLoaded()` as a way to make the scan work.

`importObjNative` is legal **only after comparison says a file is new or changed**, because applying that source revision necessarily requires parsing the replacement geometry. Unchanged hashes must never be imported.

Do not emit one WebSocket/UI progress redraw per OBJ. Start/end hash progress and changed/new import work are sufficient.

## 8. Mesh validation visualization

Every WebUI mesh list that presents source-backed render geometry must use the same validation-pending state.

If any required stage check for the mesh is not `passed`, the row must use the shared red-brown/red-tinted `meshValidationPending` styling. This must coexist with selection styling rather than hiding selection.

The rule applies to mesh inventories/geometry lists, geometry compare/replacement lists, LOD maintenance/preflight lists, surface mesh tables, structural mesh tables and other lists whose rows represent a render mesh. Pure semantic-node lists are not mesh lists and do not inherit this color merely because a bound render mesh is pending.

## 9. Selection and scroll are not import collateral

A targeted per-mesh SOURCE reload or hash-driven replacement must preserve user navigation state where stable identities remain valid:

- active LOD;
- selected RenderNode / corresponding semantic selection;
- geometry filter/selection;
- side/list scroll positions governed by the existing preserve-scroll helpers.

A newly added mesh must not unexpectedly steal selection. A whole-asset destructive reimport may reset identities only when the new source inventory makes the old identities invalid.

Do not enlarge click targets or replace exact mesh picking with broad hit volumes while touching these flows.

## 10. SAVE vs CHECK / wizard stages

- Completing/checking a stage changes validation evidence only; it does not imply persistence.
- SAVE and CHECK remain different buttons, backend commands and contracts.
- SOURCE/SURFACES metadata edits must remain bounded and must not trigger a whole-model PREPARE/rebuild merely to persist metadata.
- BUILD is the explicit production write and requires the current WORKING state to be saved first.

## 11. SEMANTICS-only sockets

- Socket markers and socket editing UI are visible only in SEMANTICS.
- Leaving SEMANTICS clears socket viewport markers; global toolbar state must not leak them into SOURCE/LODS/GEOMETRY/SURFACES or later stages.
- Source scan/reload does not create, delete, move or reinterpret sockets.

## 12. No fallback accretion

When an authority path is wrong, remove/reject the wrong path. Do not add another fallback that leaves the ambiguous path reachable.

In particular:

- a damaged existing WORKING package does not borrow missing `.elmesh` from production;
- SOURCE reload does not fall back to WORKING `.elmesh`;
- RELOAD LOD does not fall back to SOURCE OBJ;
- canonical Folder-authoritative Cobra does not fall back to runtime-registry geometry maintenance;
- a changed source hash is not "accepted" without importing the changed geometry.

## 13. Minimum regression checks for source/persistence patches

At minimum, tests must prove:

- canonical Cobra becomes Folder geometry authority when its folder exists while retaining runtime semantic bootstrap;
- Folder OPEN loads every declared LOD;
- `workingFilesRoot` is persisted/read by settings and used by `wizardWorkspacePath()`;
- schema 15 saves `saveRevision`, `sourceAssetDirectory`, `meshSourceRecords`, `sourceMissing`, `aggregateStageChecks` and `physicalScaleGraph` to both WORKING and production editor state;
- status UI renders `WORKING rN` instead of an I/O filesystem path;
- scan obtains the loaded source folder identity and calls one filesystem inventory;
- scan exact-hashes discovered source files and compares by filename + hash;
- unchanged hash has no import path;
- new source file calls add/import;
- changed hash calls targeted replacement and resets that mesh's stage checks;
- missing source file sets persisted `sourceMissing` without deleting the mesh or destroying its previous stage evidence;
- explicit deletion confirmation is required before missing SOURCE geometry/render instances and geometry-owned editor metadata are removed;
- scan body contains no `ModelAssetBinary::`, PREPARE/ANALYZE/repair calls or hidden LOD loading;
- targeted SOURCE reload exists and remains distinct from RELOAD LOD;
- current-stage mesh styling exists in the common mesh-list paths: passed = green/dark-green, not-checked/failed = red/dark-brown, missing SOURCE = yellow/dark-red;
- SAVE remains one mutable WORKING save and CHECK remains persistence-read-only.

## 14. SOURCE geometry inventory controls — viewport-only active-LOD mesh browser

The `Active LOD geometry` inventory shown on SOURCE is a navigation/view tool, not another authoring or geometry authority. **SOURCE active-LOD mesh browser is viewport-only.**

- It must expose compact `LOD0 ... LODN` buttons for every render LOD currently declared by the loaded asset. Switching here changes only the active render representation; an unloaded payload may use the normal `load_lod` / `request_lod_payload` path, but this control must never call RELOAD LOD, SOURCE reimport or SAVE.
- Mesh rows and the 3D viewport share the same RenderNode selection authority. Clicking a used geometry row resolves a representative RenderNode and goes through `selectRenderNode`; viewport selection must update the inventory highlight without rewriting geometry, source provenance or validation evidence.
- Per-mesh checkboxes are editor-only visibility state, scoped independently per LOD. The initial implicit state is ALL visible; the **first checkbox interaction isolates that mesh** (it stays checked and the others become unchecked). After an explicit visibility set exists, each checkbox toggles that geometry normally. `ALL` restores the implicit complete-LOD view and `NONE` hides/unchecks it.
- Hiding a geometry must hide its render mesh and selected-mesh overlays such as edge/normal overlays; it must not hide transform parents merely because those parent groups carry no geometry of their own.
- SOURCE inventory visibility affects only the SOURCE-stage viewport. GEOMETRY, SURFACES, SEMANTICS, PHYSICS, DAMAGE and later stages must not inherit these hidden-mesh filters.
- Visibility is keyed by stable geometry id, is not serialized to WORKING/production, does not set the asset dirty flag, and is cleared when another asset is selected.
- Existing per-mesh `↻ SOURCE` remains a separate data mutation. Row selection, checkboxes and LOD view buttons must never trigger SOURCE reload, SAVE, CHECK, PREPARE or scan.
- Folder authority in the catalog is displayed from `sourceAuthority == folder`; `[SOURCE]` / `[RUNTIME]` labels are derived from `CatalogSourceAuthority` in the catalog payload/UI, never hard-coded into individual asset display names.
- Existing `meshValidationPending` red-brown styling remains visible together with selected-row highlight and visibility state.


## 15. Per-mesh stage certification and two-phase SOURCE deletion

The persisted mesh SOURCE graph is the visual and behavioral authority for per-mesh validation state.

- A successful **CHECK** of the current wizard stage sets that stage to `passed` for every current source-backed mesh and immediately republishes asset metadata. CHECK does not SAVE. The visible mesh lists for that stage must turn those meshes green on a dark green-black background without waiting for another OPEN or SAVE.
- A failed CHECK must not erase previously passed evidence for unaffected meshes. New/replaced SOURCE meshes have all stage checks reset before validation, so they remain red/dark-brown until rechecked while unchanged previously-certified meshes remain green.
- SOURCE scan compares the saved source-folder inventory by filename/hash. Equal hashes leave geometry and stage evidence byte/logically untouched. New/replaced meshes reset only their own stage graph.
- If a previously tracked SOURCE filename is absent, SCAN **must not delete it**. It sets `MeshSourceRecord::sourceMissing = true`; the resident WORKING mesh remains present and is displayed as yellow text on a dark red background with a `DELETED` marker and explicit `CONFIRM` action. This state remains persisted (schema 14 introduced it; schema 15 retains it).
- If the exact source file reappears before confirmation, a later SCAN clears `sourceMissing`. If its hash is unchanged, its previous stage evidence remains; if its hash changed, normal targeted replacement occurs and that mesh's checks reset.
- `confirm_source_mesh_deletion` is the only SOURCE-maintenance path allowed to physically remove a mesh because its file disappeared. It must recheck that the SOURCE file is still absent before mutation.
- Confirmation removes the render geometry and every RenderNode instance that owns it, plus geometry-owned source/preparation/orientation/topology/raw-snapshot/variant-maintenance records. If a removed RenderNode was a transform parent, surviving render children must be reparented while preserving world placement.
- Do **not** blindly delete semantic/gameplay nodes, sockets, physics graph nodes or cross-LOD semantic structure merely because one render mesh disappeared. Render-node bindings disappear with the removed RenderNode; LOD-independent semantics require their own explicit authoring decision.
- Confirmed deletion marks WORKING dirty and invalidates dependent wizard evidence from SOURCE onward. It is still not persistent until the user presses SAVE.
- Missing/deleted UI state has higher visual priority than passed/not-checked stage state. Selection and viewport visibility controls may overlay it but must not hide the deletion warning.


## 16. SOURCE change panel is an unresolved-work queue

The `SOURCE CHANGES` panel is not a historical scan log. It represents source events that still require operator action.

- `ADDED` / `REPLACED` rows remain visible after SCAN while their imported mesh is not yet certified for the SOURCE stage.
- After a successful SOURCE **CHECK**, once that mesh has `stageChecks.source == passed`, its matching `ADDED` / `REPLACED` row must be removed from the transient scan result. The accepted row is folded into the current/unchanged count; stale `new` / `replaced` counters must not remain visible.
- CHECK must consume only rows whose resident mesh identity exists and is actually certified for that stage. Do not hide a row merely because a button was pressed.
- `missing_source`, hash/import failures, ambiguous identity and other unresolved rows are never consumed by CHECK. Missing SOURCE still requires explicit `CONFIRM` before destructive removal.
- This acknowledgement is UI/session state only. CHECK still does not SAVE and does not rewrite source files. A later SCAN reconstructs truth from the persisted mesh source graph plus the current SOURCE filesystem.


## 17. Physical scale boundary — authoring space in WORKING, meters only at BUILD

Physical size is an asset-wide interpretation contract, **not a destructive editor transform**.

- SOURCE OBJ coordinates are imported exactly as authored. WORKING geometry, RenderNode transforms, semantic positions/pivots, collisions, hit regions, openings, repair points and socket transforms remain in the same shared authoring coordinate space for the lifetime of the editor session and saved WORKING package.
- One `PhysicalSizeProfile::sourceToMeters` coefficient is authoritative for the complete asset and every LOD. Individual meshes may expose `scalePolicy = asset` for diagnostics but must never own independent physical scale coefficients.
- Manual calibration records `axis`, measured `sourceExtent`, requested `targetMeters` and `sourceToMeters = targetMeters / sourceExtent`. Setting or changing calibration must not mutate vertices, bounds, RenderNode transforms, hit/collision volumes, GPU geometry, source hashes or source provenance.
- Runtime descriptor `LogicalDimensions` are read-only game-link context (`gameDimensionsMeters`). They may prefill/suggest a target but must never auto-enable calibration or auto-resize SOURCE/WORKING. If there is no game link, the editor states that explicitly.
- `SCAN SOURCE CHANGES`, per-mesh `↻ SOURCE`, source add/replace, confirmed-delete resurrection and whole-source discovery all operate in raw authoring coordinates. No source-maintenance path may apply `sourceToMeters` to imported mesh bytes.
- BUILD is the normal conversion boundary. It clones the saved/in-memory authoring asset, applies the one uniform coefficient to authored distance/geometry fields on the **temporary production copy**, marks that package `PhysicalGeometrySpace::Meters`, and saves it. WORKING remains byte/logically in authoring space. Runtime consumers of that compiled metric package must not apply the coefficient again.
- A metric production package adopted as a new editor WORKING copy must be converted back to authoring coordinates before editing. All LOD payloads must be resident for that inverse conversion so later LOD loads cannot mix coordinate spaces.
- Geometry-derived physical calculations must convert authoring dimensions through `sourceToMeters` before computing SI quantities. Auto mass/inertia therefore uses meter collision dimensions. `RigidBodyProperties::centerOfMass` remains an authored position in WORKING; `massKg`, inertia (`kg*m²`), density (`kg/m³`), break force (`N`), break torque (`Nm`) and fields already explicitly stored in meters such as `LightProperties::rangeMeters` are physical values and are **not** multiplied by BUILD scale.
- Hit/collision volumes cannot be visually rescaled independently of render geometry in WORKING. They stay in authoring space with the mesh; the same BUILD copy conversion scales both once. A calibration UI operation therefore never requires a GPU geometry refresh.
- Pre-0.10.62 enabled `SIZE` chunks are `LegacyUnknown` because the old destructive resize did not persist enough information to recover the original authoring coordinate space safely. Incremental SOURCE scan/reload must be blocked in that state. Explicit `RELOAD ALL SOURCE MESHES` is the migration boundary: restore complete raw SOURCE geometry, derive a new coefficient from the retained target size, then require explicit SAVE. Never guess an inverse legacy scale.
- WORKING/production editor sidecar schema 15 mirrors the scale graph for diagnostics, but the `.elmodel` `SIZE` chunk remains the asset-level scale authority.

Regression tests for scaling patches must prove the absence of destructive WORKING resize and the presence of BUILD-copy conversion. Do not reintroduce `scaleModelAssetUniform(m_asset, ...)`, AUTO-after-source-import, per-mesh scale ownership, or source-reimport scaling as a convenience fallback.


## 18. Frozen editor shell and SOURCE tab — v0.10.62 acceptance baseline

As of Model Asset Editor **v0.10.62**, the overall nine-stage wizard structure and the first **SOURCE** stage are accepted and frozen. Subsequent feature patches must treat this area as protected infrastructure, not convenient UI/code to refactor while working elsewhere.

The frozen editor workflow order is exactly:

`SOURCE → LOD → GEOMETRY → SURFACES → SEMANTICS → PHYSICS → DAMAGE → VALIDATE → BUILD`.

SOURCE remains the first/default stage. Its accepted surface includes the source summary/inventory, authoring→meters physical scale contract, unresolved SOURCE-change queue, advanced whole-SOURCE reimport controls, stage CHECK, active-LOD geometry browser, LOD0…LODN viewport selector, ALL/NONE/per-mesh visibility, mesh↔viewport selection synchronization, per-mesh `↻ SOURCE`, validation colors and confirmed missing-SOURCE deletion.

**Freeze rule:** patches whose stated task is outside editor structure/SOURCE must not modify this protected surface. No cleanup, renaming, restyling, helper extraction, fallback addition, control relocation or opportunistic refactor is allowed there merely because nearby code is being edited.

An exception is allowed only when the requested bug/feature directly requires changing SOURCE or the editor shell. Such a patch must:

1. state the exceptional SOURCE/structure reason explicitly;
2. preserve every unrelated accepted SOURCE contract;
3. update this section and CHANGELOG with the intentional delta;
4. deliberately update the SOURCE lock fingerprint only after review;
5. run both the general editor architecture test and the dedicated SOURCE freeze test.

Enforcement lives in `tests/architecture_contracts/model_asset_source_tab_lock.py`. The general `check_model_asset_editor.py` invokes it automatically, and `check_model_asset_source_tab_frozen.py` provides a focused diagnostic. The lock checks the exact wizard order/default stage, SOURCE-only side-panel ownership and the accepted SOURCE implementation fingerprint. A digest mismatch is a **test failure by design**, not an instruction to mechanically regenerate the hash.

## 19. LOD workspace — per-mesh PREPARE evidence, viewport filtering, read-only ANALYZE

The LOD workspace is per-mesh. It is not a second SOURCE workset selector and it must not reintroduce the old `CHANGES / WHOLE MODEL` maintenance mode switch.

- The LOD table is the persistent operator surface for the resident render documents. Rows use the existing per-mesh graph as visual authority: `stageChecks.lods == passed` is green on dark green-black; source-backed `not_checked` / `failed` is red on dark brown. Selection outline must not replace those colors.
- `PREPARE MESHES` is the certification boundary for the LOD stage. A source-backed mesh becomes `lods=passed` when its resident canonical payload is current/successfully prepared. A preparation failure records `lods=failed` for that mesh. This metadata is mutable WORKING state and is persisted only by the ordinary SAVE.
- Global LOD PREPARE is pending-only: already `lods=passed` meshes are skipped before canonical fingerprint/canonicalization work. SOURCE add/replace already resets the affected mesh stage graph, so only new/changed meshes return to the red pending set. Unchanged certified meshes stay green and are not prepared again.
- Non-SOURCE/generated diagnostic geometry may still use the existing canonical preparation record path; the per-mesh SOURCE graph must not disable PREPARE merely because such a geometry has no `MeshSourceRecord`.
- LODS begins with the same explicit `ACTIVE RENDER LOD` selector pattern as GEOMETRY. The selector is a separate semantic block and routes through the canonical `EditorViewState` LOD switch. Both the pre-ANALYZE inventory and post-ANALYZE mesh table are scoped to that active LOD; a table row must never change LOD implicitly.
- The LOD table and 3D viewport share canonical RenderNode selection. Row click selects the representative RenderNode; 3D click highlights and scrolls the matching table row inside the active LOD.
- LOD visibility checkboxes are editor-only and independent per LOD. Initial state is all visible; the first checkbox interaction isolates that mesh. Compact `SHOW ALL / HIDE ALL` controls restore all or hide all. `SHOW ALL`, `HIDE ALL`, orientation action and the visible counter stay together on one action line below the selected-mesh identity. These controls never mutate geometry, source provenance, stage checks, SAVE state or other wizard stages.
- The old LOD `WORKING SET`, `CHANGES`, `WHOLE MODEL`, `HIDE SELECTED` and `SHOW SELECTED` mode/control surface is retired. The table has more vertical space instead of being constrained to the former short workset panel.
- **ANALYZE remains read-only.** `analyze_model_preflight` continues to call the established `analyzeModelPreflight()` audit directly. It must not route through PREPARE/canonicalization, set `lods` stage evidence, or mutate resident geometry as a side effect of the LOD workspace cleanup. The optional LOD generator analysis (`analyze_lod_requirements`) is likewise kept as its existing separate path.
- The accepted SOURCE freeze from section 18 remains untouched. LOD patches must pass the SOURCE fingerprint guard.

Regression protection is provided by `tests/architecture_contracts/check_model_asset_lod_workspace.py` in addition to the general editor and frozen-SOURCE tests.

## 20. GEOMETRY workspace — per-mesh CHECK evidence and viewport filtering

The GEOMETRY workspace is per-mesh and LOD-local. It must not reintroduce the retired `WHOLE MODEL / RECENTLY LOADED` workset selector. The existing duplicate comparison/consolidation, additional replacement assignment and geometry editing tools remain the functional core; this UI pass only changes operator navigation, stage evidence and viewport filtering around them.

- Every source-backed geometry row uses `stageChecks.geometry` as its certification authority. `passed` is green text on a dark green-black row; `not_checked` / `failed` is red text on a dark brown row. Comparison state (`REFERENCE`, `MATCH`, `DIFFERENT`, `INSTANCE`) may add icons/labels but must not overwrite the stage color.
- A successful GEOMETRY CHECK transitions only mesh records that are not already `geometry=passed`. Already-certified source meshes remain green and their evidence is not rewritten. SOURCE add/replace resets the affected mesh graph, so only new/changed meshes return to red and need GEOMETRY CHECK again. The global structural safety validation may still inspect the render graph; the per-mesh certification workset is what changes.
- The main table is vertically expanded. Row click selects the canonical RenderNode in the viewport; a 3D mesh click selects and scrolls the corresponding row. No secondary selection authority may be introduced.
- Per-row checkboxes are viewport-only visibility controls, independent per LOD. Default is all visible; the first checkbox interaction isolates that mesh. Compact `SHOW ALL / HIDE ALL` actions restore all or hide all. These controls never mutate geometry, source provenance, validation graph, SAVE state or runtime data.
- The old GEOMETRY workset/scope controls are retired. Do not bring back `geometryScopeSelector`, `data-geometry-scope`, `RECENTLY LOADED / CHANGED` or a second `WHOLE MODEL` mode as a maintenance fallback.
- The existing duplicate comparison/consolidation route (`scan_render_duplicates` / `consolidate_render_duplicates`) and editing operations are not redesigned by this patch. Future changes to those algorithms require a separate task and tests.
- The frozen SOURCE contract from section 18 remains untouched, and LOD PREPARE/ANALYZE behavior from section 19 remains unchanged.

Regression protection is provided by `tests/architecture_contracts/check_model_asset_geometry_workspace.py` together with the general editor, LOD workspace and frozen-SOURCE tests.

## 21. Authoritative EditorViewState / tab-neutral viewport lifecycle — 0.10.64 architecture hotfix

`EditorViewState` is the sole persistent browser view-state authority for the editor. Stage tabs may own stage-specific tool state, previews and authored operations, but they must not own another copy of viewport navigation state.

- `EditorViewState` owns `activeLod`, `sceneLod`, the pending explicit LOD target, selected RenderNode/mesh/semantic node identity, per-LOD mesh visibility, per-LOD semantic hidden/isolation state, and the loaded/resident LOD sets. Historical names such as `geometryInventoryVisibleByLod`, `lodPreflightVisibleByLod`, `geometryStageVisibleByLod` and `hiddenRenderNodes` are compatibility projections over that one state, never independent stores.
- A stage transition is view-state neutral. Switching SOURCE/LOD/GEOMETRY/SURFACES/SEMANTICS/PHYSICS/DAMAGE/VALIDATE/BUILD must not change active LOD, selected mesh/RenderNode, per-LOD visibility/isolation or camera transform/target. Stage-specific preview objects may be rebuilt, but the persistent view snapshot before and after the transition must compare equal.
- While an asset is open, `sceneLod == activeLod` is an invariant. A mismatch is a state error, is written to diagnostics and must not be silently repaired by another fallback path. A scene with render-node geometry also requires that LOD to be resident.
- `loadedLods` and `residentLods` are distinct. `loaded` means the native editor session owns the LOD document; `resident` means the browser has the geometry payload needed to build the scene. An async `lod_payload` only activates a LOD when it satisfies the explicit `pendingActiveLod`; unsolicited/cache payloads must never steal the active viewport.
- Physical mesh visibility is evaluated from `EditorViewState` once. SOURCE, LOD and GEOMETRY visibility controls project geometry IDs or RenderNode indices into the same typed per-LOD visibility set. The LOD generator may still apply its temporary preview-only mesh filter, because that filter is a stage tool and is not persistent editor view state.
- The accepted SOURCE branch/functions remain frozen. The exceptional architecture change required by this bug is below that protected surface: the SOURCE visibility/selection APIs now project onto the shared state. No SOURCE control, wording, layout or protected implementation fingerprint is changed.
- SEMANTICS, PHYSICS, DAMAGE, VALIDATE and BUILD expose the shared mesh navigation panel. SURFACES intentionally owns a richer combined mesh/surface table instead of duplicating the same active LOD in two lists. All of these views still route LOD, primary selection and visibility through `EditorViewState`; stage-specific authoring state is separate.
- The retired `WORKING SET / WHOLE MODEL / RECENTLY LOADED / CHANGES` maintenance selector must not return as a second view-state authority or fallback.
- Browser diagnostics capture JavaScript exceptions, `unhandledrejection`, WebSocket dispatch/receive errors, UI command dispatch errors and state invariant failures. Every record carries client timestamp, stage, active LOD, scene LOD and selected mesh/RenderNode. The backend appends JSONL to `wizardLogPath("editor_ui.log")`, i.e. the selected asset's `logs/editor_ui.log` beside its WORKING/intermediate workspace.

Regression protection is provided by `tests/architecture_contracts/check_model_asset_editor_view_state.py` together with the general editor, LOD/GEOMETRY workspace and frozen-SOURCE tests.

## 22. Frozen SOURCE / LODS / GEOMETRY acceptance baseline — v0.10.64

After the final navigation polish, the first three authoring tabs are accepted as a stable editor surface and are frozen together.

- **SOURCE** retains its existing v0.10.62 protected fingerprint and all section 18 contracts. This final polish does not alter the protected SOURCE implementation surface.
- **LODS** is frozen with the explicit `ACTIVE RENDER LOD` block, active-LOD-only mesh table, per-mesh PREPARE evidence, compact one-line `SHOW ALL / HIDE ALL / FLIP / visible-count` controls, table↔3D selection and read-only ANALYZE boundary.
- **GEOMETRY** is frozen with the matching active-LOD navigation pattern, per-mesh stage evidence/visibility, duplicate comparison/consolidation tools, existing geometry editing tools and canonical table↔3D RenderNode selection.
- A 3D pick in SOURCE/LODS/GEOMETRY must move keyboard/UI focus into the right-side mesh table and scroll the selected row into view. This is navigation only; it must not create another selection authority or mutate asset data.
- The viewport selection highlight is deliberately a saturated emerald replacement (`0x00a84f`) with a bright green emissive cue (`0x00ff70`). Selection must replace the displayed mesh colour while active rather than merely tinting/lerping the authored colour; pale blue/white meshes must remain unmistakably selected. This palette is shared across editor stages; changing it is allowed only as an explicit UX decision and must keep the SOURCE/LODS/GEOMETRY layout/function fingerprints intact.
- Later SURFACES/SEMANTICS/PHYSICS/DAMAGE/VALIDATE/BUILD work must not opportunistically reorganize, rename, restyle or refactor the protected SOURCE/LODS/GEOMETRY surfaces. Changes require an explicit user-approved reopening of the affected accepted tab and an intentional lock fingerprint update.

Enforcement lives in `tests/architecture_contracts/model_asset_core_tabs_lock.py` and `check_model_asset_core_tabs_frozen.py`. The general `check_model_asset_editor.py` invokes the combined lock automatically. LODS and GEOMETRY use protected stage/function/CSS fingerprints; shared selection/focus behavior is token-guarded so later stages can evolve shared code without silently weakening the accepted first-three-tab contract.


## 23. SURFACES workspace — combined mesh table / multi-selection

SURFACES is geometry-authoritative: topology/surface intent and material ownership belong to the active LOD geometry definition, not to a duplicate RenderNode workset. The accepted SURFACES navigation contract is therefore one combined table.

- Do not show the generic `ACTIVE LOD MESHES` panel on SURFACES. The SURFACES table combines active-LOD mesh navigation, visibility, per-mesh certification and surface intent in one geometry row. A geometry used by multiple RenderNodes remains one row; the row reports instance usage and its visibility checkbox controls every render instance of that geometry without creating another persistence authority.
- Row color has exactly one certification authority: `stageChecks.surfaces`. `passed` is green/dark-green; `failed` (and missing source evidence) is red-brown; `not_checked` is neutral dark slate. A failed SURFACES stage must record results per geometry, so one bad mesh cannot paint every other row red. Topology review/mismatch/material problems may change the icon, text or tooltip, but must not introduce a yellow/review row color that obscures certification state.
- SURFACES supports a stage-local geometry multi-selection because one surface intent is commonly authored for several meshes at once. Plain click replaces the group, Ctrl/Cmd adds or toggles one row, Shift fills the contiguous range from the anchor, and Ctrl/Cmd+Shift adds that range. This set is an authoring workset only; the primary mesh/RenderNode remains the single canonical `EditorViewState` selection used by the viewport and cross-tab navigation.
- Changing `Surface intent` applies to every geometry in the current SURFACES multi-selection. Material editing and implicit-default material assignment remain primary-geometry operations; multi-selection must not silently copy material definitions between geometries.
- Table selection must not force-scroll the right panel to the surface-type/material editor. Local rerenders preserve the current side/table scroll. A 3D viewport pick is different: it intentionally collapses SURFACES multi-selection to that picked geometry, sets the canonical primary RenderNode, focuses the right-side combined table and scrolls the corresponding row into view.
- Visibility remains editor-only and per LOD through the shared `EditorViewState.visibilityByLod`; SHOW ALL/HIDE ALL and row checkboxes never mutate surface metadata, stage evidence or SAVE state.
- Viewport selection uses the shared green palette. On SURFACES, authored material colors are restored first and then the selected geometry/geometries receive a strong green tint plus emissive cue; the effect must remain visible even when the authored material has zero emissive strength.
- SOURCE, LODS and GEOMETRY remain frozen. SURFACES work must pass `check_model_asset_core_tabs_frozen.py` without changing their protected fingerprints.

Regression protection is provided by `tests/architecture_contracts/check_model_asset_surfaces_workspace.py` together with the authoritative EditorViewState and frozen-core-tabs tests.

## 24. Per-LOD SOURCE basis authority / reload consistency — 0.10.64

Coordinate-basis conversion is owned by a render LOD, not by the asset as one irreversible global switch. A visual LOD must never become a mixture of raw Blender coordinates and game coordinates after SOURCE maintenance.

- Every render LOD has one persisted SOURCE-basis state. `game_current` means its resident payload needs no Blender→game transform; a preset such as `blender_model` means raw SOURCE OBJ data for that LOD must be transformed by that preset before it is admitted to the resident WORKING document.
- The toolbar axis operation always carries an explicit `lodIndex` and may load/modify only that visual LOD. It must not call `ensureAllLodsLoaded()` or transform another render document as a side effect.
- SOURCE replace, add, replacement-variant import and broad variant refresh are raw-import boundaries. Immediately after OBJ decode and before comparing/storing the mesh, the target LOD's configured basis is reapplied. This rule prevents old SOURCE reloads from silently reintroducing Blender Z-up coordinates into a converted Y-up LOD.
- Same-asset full SOURCE reimport preserves the existing per-LOD basis profile and reapplies it to the freshly imported visual documents before they become authoritative WORKING geometry. A full reimport may still replace other authored WORKING content according to its existing contract; it must not lose coordinate-basis state.
- Generated LODs inherit the basis state of the source LOD from which they were generated.
- Legacy pre-schema-16 snapshots with an asset-wide canonicalized `sourceBasis` are migrated as the initial per-LOD basis profile. Because historical individual SOURCE reloads may already have produced a physically mixed LOD, the marker alone is not proof that every resident mesh is correctly transformed.
- `reimport_lod_source_basis` is the recovery path for that legacy corruption. It stages every source-backed canonical geometry of exactly one selected LOD from its exact SOURCE file, reapplies that LOD's configured basis, and commits only after every staged import succeeds. Geometry ids, RenderNode ids/placement and persistent instance-family aliases are preserved. PREPARE/topology/raw snapshots and downstream mesh stage evidence are invalidated because the resident payload changed.
- Semantic/collision authoring is one shared SOURCE frame rather than one copy per render LOD. LOD0 owns the explicit shared-frame conversion: source/bootstrap collision volumes, hit regions, semantic transforms, sockets, openings, repair targets and structural damage proxies rotate with LOD0 exactly once. Converting or rebuilding LOD1+ must never rotate this shared frame again.
- A recovery rebuild of an already converted LOD transforms only newly decoded visual meshes. It must not transform the shared SOURCE/hit frame again.

Regression protection is provided by `tests/architecture_contracts/check_model_asset_per_lod_basis.py` together with the SOURCE maintenance, physical-scale, frozen-core-tabs and EditorViewState contracts.

### Explicit direct SOURCE→GAME axis mapping and fixed game frame

The canonical game frame is not user-configurable: `RIGHT = +X`, `UP = +Y`, `NOSE/FORWARD = -Z`; therefore the editor ground plane is `XZ` and the vertical axis is `Y`. Axis authoring configures how a SOURCE LOD is interpreted, not what the game coordinate system means.

- The user-facing active-LOD mapping is a direct three-row SOURCE→GAME permutation: `SOURCE +X → GAME ±axis`, `SOURCE +Y → GAME ±axis`, `SOURCE +Z → GAME ±axis`. Each GAME axis family X/Y/Z must be used exactly once. The opposite SOURCE direction follows automatically with the opposite GAME sign; the user never has to translate the operation into RIGHT/UP/NOSE semantics.
- The UI labels every GAME destination semantically (`+X RIGHT`, `-X LEFT`, `+Y UP`, `-Y DOWN`, `-Z NOSE`, `+Z TAIL`) and shows the full positive+negative result before APPLY. A signed permutation with negative determinant is explicitly identified as a mirror/handedness flip rather than an ordinary rotation.
- Named Blender compatibility is displayed in the same direct notation: `X→X, Y→Z, Z→Y`. No-remap is `X→X, Y→Y, Z→Z`. The backend may continue storing the equivalent semantic `axis:<right>,<up>,<nose>` key; UI conversion to that internal representation is not exposed as an authoring task.
- Custom mappings persist in the same per-LOD basis authority as `axis:<right>,<up>,<nose>`. They are reapplied on every SOURCE reload/add/replace/reimport boundary for that LOD.
- Changing an already-configured mapping is an explicit remap, not an incremental blind rotation: source-backed geometry is rebuilt from immutable SOURCE under the requested mapping, source-less geometry and RenderNode placement follow the old→new mapping delta, and other LOD documents are untouched.
- LOD0 owns the shared SOURCE semantic/collision/hit frame for coordinate mapping purposes. Remapping LOD0 applies the old→new delta to that frame; remapping LOD1+ never rotates the same shared volumes again.
- The viewport must show the fixed semantic game-frame labels plus the active LOD's SOURCE mapping so that geometry orientation can be judged without guessing which raw axis is intended as ship nose/up/right.

Regression protection is also provided by `tests/architecture_contracts/check_model_asset_axis_mapping.py`.


## 25. Shared SOURCE hit/collision primitive frame — schema 17

The shared LOD0 SOURCE frame contains both hierarchical semantic coordinate frames and leaf physical primitives. These two categories must not use the same rotation formula.

- Semantic nodes, state transform overrides, sockets and repair frames are coordinate systems. A basis remap uses conjugation (`B * R * B^-1`) so their local right-handed frame remains a coordinate frame.
- Collision volumes, hit regions, openings and structural damage proxies are centered leaf primitives. Their generated local primitive geometry is not itself a child coordinate system, so the physical primitive orientation follows the frame change by left multiplication. Signed-permutation reflections are absorbed through the primitive's local sign symmetry so the serialized Euler orientation remains a proper rotation.
- Editor state schema 17 persists `sharedSourceFrameTransformVersion=2`. Schema-16 and older states that already have a non-`game_current` shared basis are treated as legacy v1 leaf encoding. The next explicit LOD0 axis APPLY first reconstructs the source primitive orientation from the v1 conjugated representation, converts it to v2, then applies the requested old→new frame delta. Reapplying the same LOD0 mapping is therefore an intentional repair operation and must not be optimized away.
- The migration changes only primitive leaf orientation; current authored positions/sizes and the semantic hierarchy are retained. After migration, further LOD0 mappings are delta-based and non-cumulative. LOD1+ never touch this shared frame.

Regression protection is provided by `tests/architecture_contracts/check_model_asset_shared_hit_frame.py`.

## Axis rotation UI contract

- Axis editing is expressed to the user as rotations around fixed GAME/world X/Y/Z axes, not as SOURCE semantic-axis remapping.
- Each axis exposes +90°, -90°, and 180° buttons.
- Dialog button presses do not mutate the asset; APPLY composes the pending orientation and rebuilds only the active LOD from immutable SOURCE.
- Existing per-LOD basis persistence, eager all-LOD residency, and LOD0 shared hit-frame ownership remain unchanged.
