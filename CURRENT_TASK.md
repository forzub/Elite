# Elite — CURRENT TASK

**Updated:** 2026-09-13  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor candidate:** v0.10.76

## Immediate goal

Continue the whole-editor decomposition after the successful application-state and top-level orchestration extraction. The next wave is **backend / session / persistence / transport isolation** from `model_asset_editor.html`, preserving accepted editor behavior.

## Verified baseline

The application state remains authoritative through:

`action -> ApplicationController -> reducer/store -> selector`

Top-level workflow orchestration is now physically separated:

`UI stage request -> workflow effect adapter -> authoritative state transition -> declarative stage renderer registry -> feature renderer`

Canonical workflow remains:

`SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD`

`PHYSICS` is asset-authoring metadata. Game-world runtime simulation remains out of scope.

`EditorViewState` remains authoritative for active LOD, selection, visibility/isolation and loaded/resident LODs.

## v0.10.76 orchestration pass — complete

Completed:

- dedicated `app/bootstrap.js` replaces the temporary application-state bootstrap in i18n;
- `effects/workflow.js` owns stage-transition side effects and redraw fan-out;
- `app/stage_renderers.js` owns declarative top-level stage dispatch;
- the HTML shell no longer implements `setWizardStage(...)`;
- `renderWizardPanelContents()` no longer contains stage `if(stage===...)` dispatch;
- locked-stage behavior and EditorViewState transition invariants are preserved;
- editor version is `0.10.76`.

Contracts executed successfully in the hosted verification job:

```text
MODEL ASSET EDITOR APPLICATION STATE: PASS
MODEL ASSET EDITOR ORCHESTRATION LAYERS: PASS
MODEL ASSET SEMANTICS WORKSPACE LAYOUT: PASS
```

JavaScript syntax checks for bootstrap, stage registry, workflow effects and i18n also passed. Local `EliteAssetEditor` build/runtime smoke is still required for acceptance.

## Next decomposition wave — backend/session/persistence/transport

Trace and separate the remaining shell responsibilities in this order:

1. WebSocket lifecycle / reconnect / binary-vs-JSON dispatch;
2. command transport (`send`) and diagnostic transport;
3. editor binary transfer reader/decoder and transfer bookkeeping;
4. asset metadata/full-payload acceptance and resident LOD merge/reuse logic;
5. SAVE / RESTORE / settings/session persistence effects;
6. backend message routing (`handle`) into explicit handlers/adapters;
7. keep domain calculations and `EditorViewState` ownership out of the transport layer.

Target dependency direction:

```text
UI / feature command
        ↓
application / command boundary
        ↓
transport adapter
        ↓
WebSocket/backend

backend message
        ↓
transport decode/router
        ↓
session / asset effect handler
        ↓
authoritative application/view/domain state
        ↓
render/update effects
```

The transport/session modules must not absorb THREE scene ownership or feature-domain algorithms.

## Verification for next candidate

Keep these green:

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
```

Add architecture contracts for the transport/session split, then run:

```bash
cmake --build build/tools/model_asset_editor --target EliteAssetEditor -j 8
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual smoke should cover connection/open asset, all enabled workflow stages, LOD switching, SAVE/RESTORE, settings/locale, source reload, binary LOD loading, SEMANTICS TREE/GRAPH and PHYSICS editing.

## Remaining decomposition after transport

1. THREE / viewport runtime extraction;
2. EditorViewState physical extraction and remaining view adapters;
3. final HTML-shell cleanup and architecture closure.

Estimated whole-editor separation after v0.10.76: approximately **65–70% by architectural layers**.

## Parallel technical debt

Binary data architecture is logically split but still needs the independent-CMake-translation-unit closure:

- register binary `.cpp` files directly in `EliteModelAsset`;
- remove `.cpp` aggregation from `ModelAssetBinary.cpp`;
- update stale binary aggregate contract;
- build and smoke-test v4 save/load.

This remains separate from the editor application decomposition.
