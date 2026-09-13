# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.80
**Whole-editor separation:** ~96%

## Immediate goal

Verify the v0.10.80 renderer-adapter split, then perform the final WebUI architecture closure: **extract EditorViewState + visibility projection adapters + invariant scheduling and reduce the HTML to composition/bootstrap**.

## Verification

Run:

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_editor_transport_layers.py
python tests/architecture_contracts/check_model_asset_editor_session_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_adapters.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual smoke: open asset, LOD switching, visibility/isolation, edge edit, normals, collision selection/editing, socket selection/camera preview, semantic TREE/GRAPH picking and pivot picking, SOURCE/WORKING viewport, SAVE/RESTORE/reconnect.

## Final WebUI closure

1. move `ProjectedVisibilitySet`, `EditorVisibilityMapAdapter`, `HiddenRenderNodeAdapter`, and `EditorViewState` to a dedicated view-state module;
2. move invariant scheduling and state projection glue out of HTML where practical;
3. keep the application reducer free of THREE/DOM and preserve EditorViewState as viewport state authority;
4. add final shell contract rejecting application/session/transport/viewport implementation functions in `model_asset_editor.html`;
5. reduce the remaining script to imports, composition, small DOM binding and feature UI functions that have not yet justified a standalone domain module.

Expected WebUI separation after closure: **~100%**.

## Parallel binary debt

After WebUI closure, finish the independent-CMake-translation-unit gate for the production v4 binary subsystem.
