# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.81
**Architectural layer separation:** ~100%

## Immediate goal

Acceptance-test the v0.10.81 WebUI architecture closure. Do not start another decomposition wave unless runtime smoke exposes a regression or a contract identifies a leaked responsibility.

## Verification

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_editor_transport_layers.py
python tests/architecture_contracts/check_model_asset_editor_session_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_adapters.py
python tests/architecture_contracts/check_model_asset_editor_view_state_layers.py
python tests/architecture_contracts/check_model_asset_editor_shell_architecture.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

## Runtime acceptance

Check startup first, then connect/catalog/settings, asset open/full binary payload, all LOD switches, SOURCE/WORKING viewport, visibility/isolation, edge/normals, SURFACES preview, SEMANTICS TREE/GRAPH/pivot/motion, PHYSICS collisions, sockets/camera preview, SAVE/RESTORE, settings/locale and reconnect.

## If acceptance passes

WebUI architectural decomposition is closed. Future editor work should be feature work or targeted cleanup under the existing boundaries, not another general decomposition campaign.

## Separate next architecture item

The production ModelAsset binary v4 subsystem still has an independent CMake translation-unit closure task: list binary `.cpp` files directly in the target, remove facade `.cpp` aggregation includes, enforce the contract, and build/test v4 save/load. This is separate from the completed WebUI separation metric.
