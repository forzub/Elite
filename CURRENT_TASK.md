# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.83
**WebUI architecture:** ~100%

## Immediate goal

Acceptance-test localization completeness across all five locales. Verify switching before any asset is selected, permanent `Ctrl+Alt+F12` hint, settings persistence, static shell, contextual `?` help, SOURCE/LODS/GEOMETRY/SURFACES/SEMANTICS/PHYSICS/DAMAGE, axis legend, storage panel and representative backend status messages.

English fallback is valid only when the requested locale has no translation. A non-English catalog field populated with copied English prose is a defect.

## Normal iteration gate

```bash
python tests/architecture_contracts/run_model_asset_editor_impacted.py --base HEAD^
```

## Full gate for v0.10.83

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_editor_transport_layers.py
python tests/architecture_contracts/check_model_asset_editor_session_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_adapters.py
python tests/architecture_contracts/check_model_asset_editor_view_state_layers.py
python tests/architecture_contracts/check_model_asset_editor_shell_architecture.py
python tests/architecture_contracts/check_model_asset_editor_localization.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

## NEXT TASK AFTER LOCALIZATION

**Finish binary v4 translation-unit architecture.**

Required closure:

1. list the binary subsystem `.cpp` files directly in the `EliteModelAsset` CMake target;
2. remove `.cpp` aggregation includes from the facade;
3. add/strengthen the architecture contract so `.cpp` includes cannot return;
4. build and test production v4 save/load compatibility.
