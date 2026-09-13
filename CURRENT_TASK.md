# Elite — CURRENT TASK

**Updated:** 2026-09-13  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`

## Immediate goal

Verify the first whole-application state-control pass, then continue physical extraction of orchestration from `model_asset_editor.html` without changing accepted editor behavior.

## Current candidate — application state authority

The active candidate introduces:

- explicit application actions;
- canonical workflow state machine;
- pure application reducer;
- application store;
- application controller;
- selectors/snapshot;
- compatibility projection over the current legacy `state` object.

Canonical workflow:

`SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD`

`PHYSICS` remains asset-authoring metadata. Game-world runtime simulation is out of scope.

### Required invariant

UI/feature/effect code may request or assign a control change through the compatibility surface, but the authoritative value must be owned by:

`action -> ApplicationController -> reducer/store -> selector`

DOM, THREE, backend `send`, filesystem and timers must not enter the reducer/controller layers.

`EditorViewState` remains the authority for active LOD, view selection, visibility/isolation and loaded/resident LODs.

## Verification now

Run from the repository root:

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor -j 8
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual smoke:

- open an asset and walk SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD where enabled;
- move backward between stages;
- verify locked-stage behavior is unchanged;
- switch LODs and verify selection/visibility preservation;
- verify dirty/busy UI still changes normally during commands;
- switch locale;
- switch SEMANTICS TREE/GRAPH and use CHECK;
- edit PHYSICS controls and return to SEMANTICS;
- save/reload the asset.

## Next decomposition wave after this smoke

1. create a dedicated application bootstrap and remove state installation from the temporary i18n hook;
2. extract the body of top-level stage transition/render orchestration from `model_asset_editor.html` into application/effect adapters;
3. replace the stage-view `if(stage===...)` chain with a declarative stage renderer registry;
4. separate backend/session/persistence effects;
5. separate THREE viewport orchestration;
6. keep shrinking `model_asset_editor.html` toward static markup + bootstrap imports.

Do **not** move authored ModelAsset data, THREE objects, geometry caches or backend handles into the application reducer merely to make everything look centralized.

## Parallel technical debt

Binary data architecture is logically split but still needs the independent-CMake-translation-unit closure:

- register binary `.cpp` files directly in `EliteModelAsset`;
- remove `.cpp` aggregation from `ModelAssetBinary.cpp`;
- update stale binary aggregate contract;
- build and smoke-test v4 save/load.

This remains separate from the current editor application-state pass.
