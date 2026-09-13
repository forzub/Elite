# Elite — CURRENT STATE

**Updated:** 2026-09-13  
**Authoritative working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Model Asset Editor line:** v0.10.76 candidate

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

The compact visual workflow is the accepted baseline:

- TREE / ASSEMBLY-KINEMATICS and GRAPH / STRUCTURAL LINKS are the primary workflow choices;
- `CHECK` is the final action at the far right;
- long explanatory banners/prose are removed from the normal workspace;
- contextual diagnostics/help are behind compact help controls;
- existing semantic command IDs and behavior are preserved.

Contract:
`tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py`

User verification on 2026-09-13: PASS.

## Application-state control — verified

The editor has an explicit application control layer under `src/assets/webui/model_asset_editor/app/`:

- `actions.js` — application actions;
- `workflow.js` — canonical authoring workflow and pure workflow reducer;
- `reducer.js` — pure application/session/control reducer;
- `store.js` — dispatch / subscribe store;
- `controller.js` — workflow/control command boundary;
- `selectors.js` — application selectors/snapshot;
- `state.js` — compatibility projection onto the current legacy `state` object;
- `bootstrap.js` — dedicated application-state installation boundary;
- `stage_renderers.js` — declarative top-level stage renderer registry.

Canonical top-level workflow:

`SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD`

Legacy scalar writes are projected through `ApplicationController -> action -> reducer -> store`; they no longer own authoritative application-control values.

`EditorViewState` remains authoritative for active LOD, scene/resident LOD, RenderNode/mesh/semantic selection, visibility and isolation. Authored `ModelAsset` data and non-serializable runtime/effect objects stay outside the pure application reducer.

## Application orchestration extraction — v0.10.76

The first physical shell extraction is complete:

- application-state bootstrap was removed from `effects/i18n.js` and moved to `app/bootstrap.js`;
- the implementation of `setWizardStage(...)` and its redraw fan-out moved out of `model_asset_editor.html` into `effects/workflow.js`;
- `renderWizardPanelContents()` no longer owns a top-level `if(stage===...)` routing chain;
- stage rendering now delegates through `app/stage_renderers.js`;
- locked-stage behavior and `EditorViewState` transition invariants remain enforced;
- editor version advanced to `0.10.76`.

Architecture contracts:

- `tests/architecture_contracts/check_model_asset_editor_application_state.py`
- `tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py`
- `tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py`

GitHub Actions verification for this extraction passed all three contracts plus JavaScript syntax checks. A local C++/runtime smoke still belongs to user acceptance because the hosted extraction job did not build `EliteAssetEditor`.

## Remaining whole-editor decomposition

The state authority and top-level workflow orchestration are separated. Remaining major physical owners are:

1. **backend / session / persistence / transport** — WebSocket lifecycle, command dispatch, binary transfer/decode, asset acceptance/merge and save/restore/session effects;
2. **THREE / viewport runtime** — scene construction, geometry cache/materials, overlays, picking, camera/fit and render loop;
3. **EditorViewState physical extraction / remaining view orchestration** — move the correct view-state authority and compatibility adapters out of the HTML shell without changing ownership;
4. **final shell cleanup** — leave `model_asset_editor.html` as static markup/resources plus bootstrap wiring, then close remaining architecture contracts.

At this point the whole-editor separation is approximately **65–70% complete by architectural layers**. The remaining work is infrastructure-heavy rather than feature-domain decomposition.

A defect in one logical layer should invalidate the smallest practical surface instead of the whole editor.
