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

For `CatalogSourceAuthority::Folder`, OPEN must make every **declared** render LOD resident before the asset is presented for source maintenance. The editor must not recreate the 0.10.57 state where LOD0 was resident while an authored/saved LOD1 remained hidden/unloaded.

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
- schema 13 saves `saveRevision`, `sourceAssetDirectory`, `meshSourceRecords` and `aggregateStageChecks` to both WORKING and production editor state;
- status UI renders `WORKING rN` instead of an I/O filesystem path;
- scan obtains the loaded source folder identity and calls one filesystem inventory;
- scan exact-hashes discovered source files and compares by filename + hash;
- unchanged hash has no import path;
- new source file calls add/import;
- changed hash calls targeted replacement and resets that mesh's stage checks;
- missing source file marks SOURCE failed without deleting the mesh;
- scan body contains no `ModelAssetBinary::`, PREPARE/ANALYZE/repair calls or hidden LOD loading;
- targeted SOURCE reload exists and remains distinct from RELOAD LOD;
- validation-pending styling exists in the common mesh-list paths;
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
