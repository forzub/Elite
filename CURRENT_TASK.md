# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.78
**Whole-editor separation:** ~83%

## Immediate goal

Verify the v0.10.78 session/persistence split, then move to the largest remaining boundary: **THREE / viewport runtime isolation**.

## v0.10.78 session/persistence pass

Expected complete boundaries:

- `handle(msg)` imperative type chain is removed from `model_asset_editor.html`;
- backend JSON dispatch is declarative (`type -> handler`);
- asset metadata/full-payload merge + acceptance is in a session effect adapter;
- WORKING SAVE acknowledgement/bookkeeping is in a persistence adapter;
- settings load/save/error acknowledgement is in a persistence adapter;
- transport remains independent of DOM/THREE/session semantics;
- feature-specific semantic/surface patch effects remain feature effects rather than being absorbed into transport.

## Verification

Run:

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_editor_transport_layers.py
python tests/architecture_contracts/check_model_asset_editor_session_layers.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor -j 8
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual smoke should cover connect/catalog/settings, open asset, full binary payload, LOD load/reload/switch, stage checks, SAVE/RESTORE, settings/locale, SEMANTICS TREE/GRAPH and PHYSICS editing.

## Next decomposition wave — THREE / viewport

Trace and separate in controlled passes:

1. scene/bootstrap lifecycle (`initScene`, renderer/camera/controls ownership);
2. geometry-cache construction/disposal and mesh creation;
3. `rebuildScene` orchestration and RenderNode materialization;
4. visibility / collision / socket / semantic overlay rebuilds;
5. picking and camera/fit/gizmo effects;
6. leave domain calculations in feature modules and keep transport/session independent of THREE.

Expected progress after a successful first viewport extraction: approximately **89–91%**.

## Final closure after viewport

- physically extract `EditorViewState` and remaining view adapters;
- shrink `model_asset_editor.html` toward static markup + imports + bootstrap;
- add final architecture contract rejecting application/session/transport/THREE implementations in the shell.

## Parallel binary debt

After WebUI architecture closure, finish the independent-CMake-translation-unit gate for the production v4 binary subsystem.
