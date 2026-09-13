# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.84
**WebUI architecture:** ~100%

## Immediate goal

Acceptance-test v0.10.84 localization/runtime fixes:

- switch language from the toolbar before selecting any asset;
- verify `Ctrl+Alt+F12` and Settings language remain synchronized;
- verify the viewport axis legend changes language immediately;
- verify connection status says what it is connecting to and is not reset to `connecting` by locale refresh;
- enter SURFACES, press ANALYZE SURFACES, receive the result and continue editing without a JS exception/reconnect loop;
- check the visible LOD/right-panel strings in all five locales.

## Local gate

```bash
python tests/architecture_contracts/run_model_asset_editor_impacted.py --base HEAD^
python tests/architecture_contracts/check_model_asset_editor_localization.py
python tests/architecture_contracts/check_model_asset_editor_surface_runtime.py
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
