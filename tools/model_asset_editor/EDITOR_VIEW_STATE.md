# Elite Model Asset Editor — UI state lifecycle trace

Baseline traced: `src(20260907-201123).zip` (2026-09-07 20:11:23 UTC library snapshot). This document records the pre-fix ownership graph and the post-fix authority boundary.

## 1. Pre-fix lifecycle and competing owners

### Active LOD / scene

The browser kept `state.activeLod` as a scalar, but it was written from several independent paths:

1. explicit LOD buttons (`switchGeometryInventoryLod`, `switchGeometryLod`, `switchSurfaceLod`, `switchSemanticLod`, LOD file VIEW controls);
2. `setWizardStage`, which reset several stages to LOD0 and rebuilt/refit the scene;
3. `acceptAssetState`, which selected a loaded LOD during refresh;
4. asynchronous `lod_payload`, which could overwrite the active LOD merely because a payload arrived.

There was no explicit `sceneLod` owner. `rebuildScene()` implicitly rendered whatever `activeRenderLod()` returned at that instant, so table state and the already-built scene could describe different LODs after asynchronous traffic or a tab reset.

### Loaded versus resident LOD

The backend has two materially different states:

- `ensureLodLoaded()` can make a LOD native-session loaded without publishing its geometry payload to the browser;
- `sendLodPayload()` / binary LOD transfer makes geometry browser-resident.

The WebUI only tested `lod.loaded`, so it conflated these states. A loaded LOD could therefore be treated as scene-ready before the browser had geometry bytes.

### Selection

Selection was split between `selectedRenderNode`, SOURCE `geometryInventorySelectedId`, SURFACES `surfaceGeometrySelection`, semantic `selectedNode`/`semanticSelectedNodes`, and stage-specific local selection variables. SOURCE/LOD/GEOMETRY mostly converged through `selectRenderNode()`, but tab resets and LOD handlers also cleared these fields directly. Rendering SURFACES could auto-select its first geometry, meaning merely entering a tab was capable of changing selection.

### Visibility / isolation

`updateVisibility()` combined several independently mutable filters:

- semantic `hidden`;
- semantic `isolated`;
- `hiddenRenderNodes`;
- SOURCE `geometryInventoryVisibleByLod`;
- LOD `lodPreflightVisibleByLod`;
- GEOMETRY `geometryStageVisibleByLod` plus `geometryCompareChecked`;
- old geometry mesh/workset filtering;
- temporary LOD-generator preview filtering.

The first six represented the same persistent user question — which mesh is visible — but lived in different containers. A tab could therefore show a checkbox state from one map while the Three.js mesh was gated by another map.

### Camera

Several stage switches called `fitView()`. Thus tab navigation was not navigation-neutral: it could move the camera even when the operator only changed the authoring stage.

## 2. Root cause

The defect was architectural, not a single stale render call. The browser had no unique owner for the persistent view. Stage code both rendered UI and mutated navigation state, asynchronous transport could become an LOD authority, and loaded/resident state was collapsed into one bit. Every later synchronization patch therefore had another writer capable of undoing it.

## 3. Post-fix authority

`EditorViewState` owns:

- `activeLod` and `sceneLod`;
- explicit `pendingActiveLod`;
- selected RenderNode stable ID/index, selected mesh ID and selected semantic node;
- one typed `visibilityByLod` map (`rn:<id>` / `geo:<id>` tokens);
- per-LOD semantic hidden and isolation state;
- `loadedLods` and `residentLods`.

SOURCE/LOD/GEOMETRY historical map names are adapters/projections onto this state so their accepted UI code does not acquire a second store. `Mesh.visible` uses `EditorViewState` as the only persistent mesh-visibility authority; LOD-generator filtering remains a temporary preview-only tool filter.

`setWizardStage()` captures the persistent view snapshot, changes only the stage/tool presentation, rebuilds stage-specific scene material/gizmos without fitting the camera, then verifies the snapshot is unchanged. `sceneLod != activeLod` throws an invariant error and writes a diagnostic instead of silently repairing state.

An incoming LOD payload becomes resident cache. It may change `activeLod` only when it matches `pendingActiveLod`, which was created by an explicit operator LOD action.

## 4. Shared post-GEOMETRY mesh navigation

SURFACES, SEMANTICS, PHYSICS, DAMAGE, VALIDATE and BUILD use one mesh navigation surface with LOD buttons, SHOW/HIDE ALL, per-row visibility/isolation behavior, RenderNode table↔3D selection and stage-check coloring. Stage-specific editors remain in their existing branches.

## 5. Diagnostics

The WebUI records:

- `window.error` JavaScript exceptions;
- `unhandledrejection`;
- WebSocket receive/dispatch errors;
- UI command send/dispatch errors;
- `EditorViewState` invariant failures.

Records include timestamp, stage, active/scene LOD and selected mesh/RenderNode. The C++ session appends JSONL to `<workingFilesRoot>/<asset>/logs/editor_ui.log` (`wizardLogPath("editor_ui.log")`).
