# Elite — CURRENT TASK

**Updated:** 2026-09-13  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`

## Immediate goal

Continue the whole-editor decomposition now that the first authoritative application-state pass is verified. The next wave is physical extraction of orchestration from `model_asset_editor.html` without changing accepted behavior.

## Verified baseline

The following contracts were run by the user on 2026-09-13 and PASS:

```text
MODEL ASSET EDITOR APPLICATION STATE: PASS
 - canonical workflow is reducer/store/controller driven
 - PHYSICS is part of the asset-authoring workflow
 - legacy state writes project into authoritative application state
 - EditorViewState remains the viewport/view-state authority
 - reducer/controller layers are free of DOM/THREE/RPC effects

MODEL ASSET SEMANTICS WORKSPACE LAYOUT: PASS
 - TREE and GRAPH workspaces use the compact workflow bar
 - CHECK is the final workflow action
 - explanatory banners are removed from the primary workspace
 - existing semantic control IDs and bindings are preserved
```

Canonical workflow remains:

`SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD`

`PHYSICS` is asset-authoring metadata. Game-world runtime simulation remains out of scope.

## Current architecture invariant

Authoritative application control state is owned by:

`action -> ApplicationController -> reducer/store -> selector`

`EditorViewState` remains authoritative for active LOD, view selection, visibility/isolation and loaded/resident LODs.

Authored ModelAsset data, THREE objects, geometry caches, backend handles, filesystem I/O and timers must stay outside the pure application reducer/controller.

## Next decomposition wave

Do these in order:

1. create a dedicated application bootstrap and remove application-state installation from the temporary i18n hook;
2. extract top-level stage transition/re-render side effects from `model_asset_editor.html` into an application/effect adapter;
3. replace `renderWizardPanelContents()` stage `if(stage===...)` dispatch with a declarative stage renderer registry;
4. preserve locked-stage behavior and the existing `EditorViewState` invariants during every stage transition;
5. then separate backend/session/persistence effects;
6. then separate THREE viewport orchestration;
7. keep shrinking `model_asset_editor.html` toward static markup + bootstrap imports.

### Existing shell debt to remove

The HTML shell still owns:

- `setWizardStage(...)` transition orchestration;
- `renderWizardPanelContents()` stage `if` chain;
- stage-specific redraw fan-out after a transition;
- part of backend/session/persistence orchestration;
- part of THREE scene orchestration.

The state itself is no longer owned by those functions; this wave removes the remaining imperative shell routing.

## Verification for the next candidate

Keep these contracts green:

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
```

Add/extend an architecture contract so the next candidate rejects:

- application-state bootstrap from `effects/i18n.js`;
- stage-render `if(stage===...)` chains in the HTML shell;
- new DOM/THREE/RPC dependencies in reducer/controller modules.

Then run:

```bash
cmake --build build/tools/model_asset_editor --target EliteAssetEditor -j 8
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual smoke should walk all enabled workflow stages forward/backward, verify locked tabs, LOD/selection preservation, SEMANTICS TREE/GRAPH, PHYSICS editing, locale switching and save/reload.

## Parallel technical debt

Binary data architecture is logically split but still needs the independent-CMake-translation-unit closure:

- register binary `.cpp` files directly in `EliteModelAsset`;
- remove `.cpp` aggregation from `ModelAssetBinary.cpp`;
- update stale binary aggregate contract;
- build and smoke-test v4 save/load.

This remains separate from the editor application-state/orchestration pass.
