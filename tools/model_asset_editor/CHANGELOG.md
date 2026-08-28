## 0.9.9 — resume latest wizard checkpoint

- Selecting an asset now resumes the highest existing wizard checkpoint before consulting the production `.elmodel` package. A checkpoint is a complete authoring snapshot, so additional meshes and other Geometry-stage edits survive editor restart without requiring `Refresh additional LOD meshes`.
- Resume uses only canonical editor-owned checkpoint paths below the asset workspace; stale persisted checkpoint paths are rebound to the current workspace or cleared when their files were pruned.
- A checkpoint that exists but cannot be loaded is treated as an error. The editor deliberately refuses to silently fall back to an older production package, because doing so would make saved authoring work appear to have vanished.
- Explicit `Reimport Source` still bypasses checkpoint resume and rebuilds from read-only source assembly/OBJ data. Resumed checkpoint data is marked dirty relative to production output until the user explicitly saves the production package.

## 0.9.8 — Geometry finish pass / recursive extras / truthful actions

- Additional OBJ discovery is recursive again across each loaded `LOD<N>` directory tree. Runtime-registered source meshes remain the default set; every other OBJ is an additional mesh. Folder and filename names remain organizational only and do not encode authoring semantics. Refresh now detects byte-for-byte authoring-equivalent mesh content and does not mark GEOMETRY stale merely because an unchanged OBJ was re-read.
- Replacement preview is now explicitly toggleable: clicking the active preview dot again, or `Original`, restores the authored mesh without mutating the asset. `Original` is disabled when no preview is active.
- Geometry now shows the exact ordinary unused-geometry count and list before cleanup. Both cleanup buttons are disabled when the active LOD has nothing deletable; additional replacement meshes remain protected.
- Wizard checkpoint actions now reflect stage state: `Complete stage + checkpoint` is disabled while the current checkpoint is already up to date and re-enables after that stage becomes stale/changed. Restore wording now states precisely that it returns to the exact state recorded at the last checkpoint, discards later edits, and prunes later-stage checkpoints.
- Obvious save actions now expose meaningful disabled states: clean LOD payloads are not offered for redundant save, manifest save is disabled while clean, and Save All is disabled only when the package is complete and nothing is dirty.

## 0.9.7 — stable extra-mesh assignment / world axis labels

- Extra meshes are now discovered only as sibling OBJ files directly inside each loaded `LOD<N>` directory; registered/default OBJ files are excluded. Subfolder and filename naming conventions no longer carry authoring semantics.
- Added persistent opaque authoring ids for additional meshes (`extra.XXXXXX`) and default visual families (`base.XXXXXX`) in `wizard_state.json`. Replacement compatibility is stored `extra -> base visual ids`, never by filename stem or ephemeral `G#`. Existing v0.9.5/v0.9.6 compatibility is migrated when the relevant LOD is loaded.
- Replaced the per-element replacement list with a Geometry-stage master/detail UI: upper table selects one additional mesh, lower table lists each used default geometry once, checkboxes author compatibility, and radio preview temporarily swaps one representative instance before or after compatibility is checked.
- Source reimport no longer auto-pairs additional LOD meshes by matching filenames. Future generated LODs can explicitly reuse the same authoring ids without any naming convention.
- Replaced the corner-only XYZ widget with world-space `+X/-X`, `+Y/-Y`, `+Z/-Z` labels at the ends of the viewport grid/axes.

## 0.9.6 — recursive LOD extra-mesh discovery

- Removed the special meaning of `LOD*/variants/`. Refresh now resolves each real `LOD<N>` source directory and recursively scans all subfolders for OBJ files. Folder names are organizational only.
- Every OBJ declared by the runtime assembly is excluded from the extra-mesh scan even if its GeometryDefinition was later removed by duplicate/instance cleanup, preventing baked source duplicates from being rediscovered as damage variants.
- Extra-mesh identity remains the OBJ filename stem; duplicate extra stems inside one LOD produce an explicit rename diagnostic. Empty scans now report the actual resolved LOD roots that were checked.
- Geometry UI wording now says `Refresh additional LOD meshes`; existing replacement compatibility and hidden variant geometry behavior are unchanged.

## 0.9.5 — flat source variants / explicit replacement compatibility

- Fixed the v0.9.4 MinGW build failure caused by a duplicate `baseGeometryId` declaration in `RuntimeAssemblyImporter.cpp`.
- Source variants now live directly in `LOD*/variants/<variant-id>.obj`; the source directory no longer pretends that one damaged mesh belongs to one precisely named intact module.
- Variant geometry identity is now independent (`source_variant.<variant-id>`). Existing v0.9.4 `<base>.variant.<variant>` IDs remain readable for editor-checkpoint migration.
- Added explicit many-to-many replacement compatibility in the Geometry inspector. Select an intact element and check which source variants may replace its geometry; preview is enabled only for compatible variants.
- Replacement compatibility is authoring metadata persisted in `wizard_state.json`. It intentionally does not decide damage behavior: the later DAMAGE stage will choose among compatible variants from hit region/energy/penetration/accumulated-damage/randomness rules.

## 0.9.4 — source render variants / XYZ orientation

- Source reimport now discovers authored alternate OBJ meshes from `LOD*/variants/<base-mesh-id>/<variant-id>.obj`. The Geometry stage can also refresh those variant files into already-cleaned loaded LODs without rebuilding the intact assembly or losing instance consolidation. Variants enter the LOD-local geometry pool without RenderNodes, so they remain hidden until explicitly previewed or later bound to semantic state.
- Variant geometry identity is deterministic (`<base>.variant.<variant>`), LOD0 is required, LOD1 is optional, and source variants are protected from `Clean unused geometry`.
- The Geometry inspector exposes viewport-only radio preview for variants of the selected intact element. Preview swaps only that element and never changes gameplay state, collision or the asset.
- Added a persistent camera-orientation widget with labelled X/Y/Z axes in the viewport.

## 0.9.3 — metadata-only UI sync / linear checkpoint pruning

- Ordinary editor commands no longer retransmit every render mesh. Full vertex/normal/index/edge payloads are sent only for initial asset load, source reimport, checkpoint restore, LOD load/reload, and other operations that actually replace geometry data.
- Added metadata-only `asset_metadata` synchronization. The browser retains geometry arrays and cached `THREE.BufferGeometry` objects by stable `LOD + geometry id`, then rebuilds only the lightweight scene graph around those cached buffers. `break instance` clones its already-resident mesh locally; edge-mask edits send only the changed edge mask.
- Wizard checkpoints are now linear. Editing/restoring/completing an earlier stage physically removes every later checkpoint directory and resets those stages to `not_started`; only the current stage's previous checkpoint may remain as a rollback point while that stage is stale.

## 0.9.2 — geometry workflow / UI clarity

- Replaced the all-pairs duplicate-card workflow with a reference-based comparison table: choose one reference, check target rows, compare once, and see matches in green and differences in pink. Table selection and viewport selection are synchronized.
- Added one-shot bulk consolidation for checked matching rows. Comparison itself is read-only; only the explicit consolidation command changes geometry bindings. Existing instances of the reference geometry are identified without re-testing.
- Simplified the Geometry stage sidebar: technical render-tree/geometry-pool/storage sections no longer compete with the active task. The selected-element inspector is grouped into Element, Placement, Instance tools, Advanced/manual and Destructive sections. Semantic-state and surface controls no longer appear in the Geometry-stage inspector.
- Inspector actions now have explicit availability rules and contextual hints. Break Instance is disabled for unique geometry, destructive deletion is disabled for parent nodes with children, and manual fit stays collapsed under Advanced.
- Replaced the radial-array prompt chain with a parameter dialog for count, total angle, axis and rotation center.
- Widened the working sidebar and restored the missing dynamic-tooltip binding used by LOD controls.
- Automatic LOD generation remains intentionally deferred; this release is UI/workflow cleanup only.

## 0.9.1 — stable ID preflight / station import repair

- Source import now allocates deterministic semantic Node IDs and qualifies child mesh identity when a module and mesh share the same source name (the Orbital Station `station_solar_panels` case). Legacy v2/v3 migration also repairs empty/duplicate semantic and render-geometry IDs before building v4 RenderNodes.
- Added reusable `ModelAssetBinary::validate` preflight. Wizard checkpoints validate before I/O, while serializer diagnostics now name the duplicate/empty ID and both offending indices.
- The status bar now renders an explicit separator between diagnostic text and the asset path, so copied errors can no longer collapse into strings such as `LOD0D:/...`.

## 0.9.0 — wizard pipeline / capability gate / LOD manager

- Added a top-level asset-processing wizard. Source, LOD review and Geometry are working stages; later stages are visible but locked until their tools are migrated.
- Completing a working stage writes a non-production checkpoint below `build/tools/model_asset_editor/workspaces/<asset>/` and unlocks the next stage.
- Render LOD discovery now uses the union of declared render documents and saved payloads, so an existing LOD can no longer disappear from the UI merely because it is not loaded.
- Geometry duplicate detection is a first-class wizard operation and uses the existing topology-independent `GeometryInstanceFitter`; accepted candidates can be consolidated directly from the stage.
- Added `EDITOR_CAPABILITIES.json` plus architecture checks that require every protected editor capability to have a data-model/backend/UI/test contract. This prevents working tools from silently disappearing during later UI/data-model rewrites.

# Elite Model Asset Editor changelog

## 0.8.2 — reliable settings save / native window close

- Settings Save now enters the visible saving state before any locale/UI refresh and waits for an explicit backend acknowledgement; a timeout reports a missing acknowledgement instead of leaving a dead-looking button.
- The backend acknowledges a successful settings-file write before broadcasting catalog/asset refresh messages, so UI refresh errors cannot hide the save result. Settings requests and successful writes are logged to the editor console with their resolved paths.
- Removed the redundant in-page Quit command/button. The editor closes through the native window close button; the old WebSocket-thread `webview::terminate()` path was unreliable and unnecessary.

## 0.8.1 — source-root defaults / settings acknowledgement

- Source-model root now auto-detects the current project layout (`<project>/assets/models`) instead of defaulting blindly to `<project>/src`; the legacy `src/assets/models` layout remains supported.
- Saving Settings is now an acknowledged operation: the dialog stays open and shows a saving state until C++ confirms the settings file was written, then closes and reports the applied source/compiled paths. Errors keep the dialog open.
- `Defaults` now resolves to the detected source-model root, avoiding a valid-but-wrong `src` directory that causes reimport failures.

## 0.8.0 — semantic states / independent render LOD graphs

- Asset format v4 separates the shared gameplay/semantic assembly from render representation.
- Every render LOD now owns its own RenderNode hierarchy, geometry pool and instancing; no G-index or geometry dependency crosses LOD boundaries. LOD0 may be a detailed assembly while LOD1 is one welded shell and distant LODs may be only a few proxy primitives.
- Added semantic StateVariant records for damaged/breached/destroyed states, including optional transform/pivot, rigid-body and detached overrides. A damaged section may therefore bend or pull away from the intact pose without modifying the base Node.
- Collision volumes and sockets can be state-scoped. Added state-scoped HitRegion, Opening and RepairTarget semantics so a breach can atomically change collision/hit behaviour, expose a traversable/line-of-fire hole and create repair work.
- RenderNodes may bind to a semantic part and a set of active states. Damage-state visualization is independent for every render LOD.
- v2/v3 assets migrate in memory into independent v4 render graphs. Source OBJ/assembly files remain read-only.

## 0.7.0 — shared assembly / active LOD clarity

- Assembly hierarchy and Node -> GeometryDefinition instance links are labelled explicitly as SHARED across every LOD. Creating instances once in the manifest therefore applies automatically to LOD0/LOD1/LOD2/etc.
- GeometryDefinition rows are now semantic/shared IDs only; misleading all-LOD byte estimates were removed from that list.
- Added an Active LOD details section with per-geometry AVAILABLE/MISSING state, instance usage, vertex/triangle/edge counts and per-LOD payload estimate. A missing representation is now visible instead of looking like an uncreated instance.
- Replaced LOD text commands (View/Reload/Save/Unload and Save manifest) with compact icon buttons using the same structured localized tooltips as the main toolbar.
- Narrow sidebar layouts no longer require a horizontal scrollbar for the LOD command row.

## 0.6.0 — localized commands + modal command activity

- Added editor UI localization JSON for the same enabled locales as the game UI: English, Russian, Simplified Chinese, Spanish and Japanese.
- Interface language is selectable in Settings, persists in the editor-local settings JSON, and cycles with `Ctrl+Alt+F12`.
- Toolbar command names/descriptions, LOD I/O controls, settings labels and editor action buttons resolve through localization keys rather than hard-coded language branches.
- Reworked toolbar hints into a structured popup with a prominent uppercase command title and a smaller explanatory description.
- Mutating editor commands now open a true modal busy overlay, not only a status-bar `WORKING` message; the overlay includes a continuously rotating umbrella-style activity indicator and blocks accidental concurrent edits until the command reports completion/error.
- Source OBJ/assembly files remain read-only; reimport only reloads them into editor state.

## 0.5.0 — icon toolbar + persistent editor paths

- Replaced the long top-row text commands with grouped icon buttons and hover hints.
- Normals, hit volumes and sockets are explicit toolbar toggle icons with active-state feedback.
- Added a gear/settings dialog for the source-model root and compiled-model output root.
- Editor path settings persist in `build/tools/model_asset_editor/model_asset_editor.settings.json`; changing them does not move or overwrite existing source/compiled files.
- LOD rows now mark source-backed representations with a `SOURCE` provenance badge and explicitly state that automatic LOD generation is not implemented yet.
- Source-path resolution accepts the project root, `src`, `assets`, or `assets/models` as a practical source-model root.

## 0.4.0 — independently editable LOD package / `.elmodel` v3

- `.elmodel` is a lightweight semantic manifest; heavy geometry lives in one
  `<asset>.lodN.elmesh` file per LOD level.
- LOD files are first-class editor documents with independent `LOADED`,
  `UNLOADED`, `CLEAN` and `DIRTY` state.
- `Load`, `Reload`, `Unload` and `Save` operate on one LOD only. `Save manifest`
  writes semantic metadata only. `Save all` writes only dirty/missing package
  members and does not rewrite clean LOD files.
- v3 opens the manifest plus LOD0 by default; other LOD payloads remain off disk
  until explicitly loaded or an operation genuinely requires all LODs.
- External `.elmesh` entries bind to stable `GeometryDefinition::id`, not the
  transient `G0/G1/...` UI vector index.
- Structural operations that must touch every LOD (basis conversion, breaking
  an instance, deleting geometry definitions) load all LODs first and mark each
  affected payload dirty.
- Existing monolithic `.elmodel` v2 files remain readable for migration. Saving
  writes v3 beside the untouched v2 source binary.
- Editor version and asset-format version are visible in the window/UI.
- Compiled `.elmodel`/`.elmesh` packages are generated artifacts and are kept
  out of Git; editor source, tests and documentation are intended to be tracked.

## 0.2.x — model workbench / native import / instancing

- Native OBJ import retaining topology, materials, normals and UV data.
- Shared GeometryDefinition + Node instancing.
- Collision, sockets, joints and detached rigid-body metadata.
- Baked duplicate -> instance consolidation with LOD0 rigid fitting.
- Operation status/progress feedback and storage diagnostics.
