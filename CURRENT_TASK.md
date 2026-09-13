# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.85
**WebUI architecture:** ~100%
**Binary architecture:** independent-TU closure candidate

## Immediate goal

Acceptance-test production binary v4 translation-unit closure:

1. architecture contract rejects every `.cpp` include in the binary subsystem;
2. `EliteModelAsset` compiles all binary implementation files as independent CMake TUs;
3. standalone `model_asset_tests` compiles the same implementation set;
4. v4 save/load and legacy-compatibility binary tests still pass;
5. local MinGW `EliteAssetEditor` build links against the split library without unresolved/duplicate symbols.

## Local gate

```bash
python tests/architecture_contracts/check_model_asset_binary_layers.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor
./tests/model_asset/run_mingw64.sh
```

## After acceptance

Binary v4 architecture is formally closed. Do not expand the v5 draft unless a separate v5 implementation task is explicitly started.
