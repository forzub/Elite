# Elite — CURRENT STATE

**Updated:** 2026-09-13  
**Authoritative working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Model Asset Editor line:** v0.10.75 candidate

This is the short hand-off state for the active branch. `CURRENT_STATE.md` + `CURRENT_TASK.md` are authoritative for the next iteration; `PROJECT_STATE.md` remains the longer historical journal.

## Accepted / frozen baseline

- SOURCE / LODS / GEOMETRY / SURFACES remain accepted unless a regression forces a targeted repair.
- The cleaned SEMANTICS workspace is accepted by the user.
- MODEL ROOT is the implicit identity root; it is not a serialized semantic node.
- semantic transform parentage and structural support/detach are different graphs and must not be conflated.
- semantic parts are asset-wide; every LOD owns independent visual RenderNodes/bindings.
- PHYSICS is part of Model Asset authoring: collision / rigid-body / physical metadata belong to the asset. Runtime physics of a concrete world entity belongs to the game and is outside this editor architecture pass.
- production model binary remains **v4**.
- Model Asset Binary v5 is still a **draft target**, not the production format.

## Binary subsystem

The former `ModelAssetBinary.cpp` monolith is logically split under `src/model_asset/binary/` into facade, controller, validation, storage, manifest I/O, LOD I/O, MeshLod codec, FourCC registry, domain codecs and bounded wire primitives.

Dependency direction:

`facade -> controller -> validation/storage/I/O -> codecs/registry -> wire`

### Remaining binary isolation gate

The root CMake target still uses the temporary composition translation unit. Final binary closure still requires:

1. list every `src/model_asset/binary/*.cpp` and `binary/chunks/*.cpp` directly in `EliteModelAsset`;
2. remove every `.cpp` include from `ModelAssetBinary.cpp`;
3. make the architecture contract reject `.cpp` aggregation;
4. rebuild and smoke-test v4 save/load.

This is technical closure, not a reason to continue redesigning authored object data.

## SEMANTICS workspace — accepted

The compact visual workflow is now the accepted baseline:

- TREE / ASSEMBLY-KINEMATICS and GRAPH / STRUCTURAL LINKS are arranged as the primary workflow choices;
- `CHECK` is the final action at the far right;
- long explanatory banners/prose are removed from the normal workspace;
- contextual diagnostics/help are behind compact help controls;
- legacy cleanup is a secondary compact action;
- 3D preview controls are condensed;
- existing semantic command IDs and behavior were preserved.

Contract:
`tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py`

User verification on 2026-09-13:

```text
MODEL ASSET SEMANTICS WORKSPACE LAYOUT: PASS
```

## Application-state control pass — verified

The editor now has an explicit application control layer under:

`src/assets/webui/model_asset_editor/app/`

- `actions.js` — application actions;
- `workflow.js` — canonical authoring workflow and pure workflow reducer;
- `reducer.js` — pure application/session/control reducer;
- `store.js` — dispatch / subscribe store;
- `controller.js` — workflow/control command boundary;
- `selectors.js` — application selectors/snapshot;
- `state.js` — compatibility projection onto the current legacy `state` object.

Canonical top-level workflow:

`SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD`

The current shell still contains expressions such as `state.wizardStage = ...`, but `wizardStage` is now an accessor backed by `ApplicationController -> action -> reducer -> store`. The same projection is installed for session control flags and the main scalar feature-control/mode/selection values. Those legacy writes therefore no longer own the authoritative control value.

`EditorViewState` remains authoritative for active LOD, scene/resident LOD, RenderNode/mesh/semantic selection, visibility and isolation. The application snapshot composes it instead of duplicating that state.

Authored `ModelAsset` data and non-serializable runtime/effect objects (THREE scenes/groups, geometry caches, timers, WebSocket/backend transport, filesystem) intentionally stay outside the application reducer.

Architecture document:
`src/assets/webui/model_asset_editor/APP_STATE_ARCHITECTURE.md`

Contract:
`tests/architecture_contracts/check_model_asset_editor_application_state.py`

User verification on 2026-09-13:

```text
MODEL ASSET EDITOR APPLICATION STATE: PASS
 - canonical workflow is reducer/store/controller driven
 - PHYSICS is part of the asset-authoring workflow
 - legacy state writes project into authoritative application state
 - EditorViewState remains the viewport/view-state authority
 - reducer/controller layers are free of DOM/THREE/RPC effects
```

The WebUI incremental build also regenerated the UI pack/fallback successfully before these contract runs. A full manual runtime smoke is still useful as the application shell is physically extracted.

### Temporary bootstrap bridge

`effects/i18n.js` currently invokes `installApplicationState(state)` because its factory is the first stable effect bootstrap after the legacy state object is created. This is explicitly temporary. Once the HTML shell is extracted, installation must move to the dedicated application bootstrap.

## Remaining whole-editor decomposition

State authority is verified; physical orchestration is the next target:

1. move application bootstrap out of the i18n migration hook;
2. move stage transition/re-render side effects out of `model_asset_editor.html` into application/effect adapters;
3. replace the remaining stage-view `if` dispatch with a declarative renderer registry;
4. split backend/session, persistence and THREE viewport orchestration into dedicated adapters;
5. continue shrinking `model_asset_editor.html` toward static shell + bootstrap only.

A defect in one logical layer should invalidate the smallest practical surface instead of the whole editor.
