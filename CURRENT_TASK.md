# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.82
**WebUI architecture:** ~100%

## Immediate goal

Acceptance-test the localization architecture closure. In particular verify language switching before any asset is selected, all five locales, the permanent status-bar shortcut hint, settings persistence, static shell text, contextual `?` help, LODS/SURFACES/SEMANTICS/PHYSICS/DAMAGE screens, and backend status messages.

## Normal iteration gate

```bash
python tests/architecture_contracts/run_model_asset_editor_impacted.py --base HEAD^
```

The runner selects relevant contracts from changed files. It is an iteration accelerator, not a substitute for the full release gate.

## Full gate for v0.10.82

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

## Separate next architecture item

After localization acceptance, the remaining known architecture debt is the production ModelAsset binary v4 CMake translation-unit closure.
