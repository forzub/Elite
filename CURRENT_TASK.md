# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.86

## Immediate goal

Acceptance-test uniform stage action placement:

- every authoring tab places CHECK at the bottom of the full right-side scroll content;
- BUILD uses the same bottom footer location;
- no duplicated CHECK buttons appear;
- SEMANTICS TREE/GRAPH keeps mode selection at the top but CHECK at the common bottom;
- switching tabs/LODs and dynamic rerenders preserve the footer action and correct stage command.

## Local gate

```bash
python tests/architecture_contracts/check_model_asset_editor_stage_footer.py
python tests/architecture_contracts/run_model_asset_editor_impacted.py --base HEAD^
cmake --build build/tools/model_asset_editor --target EliteAssetEditor
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```
