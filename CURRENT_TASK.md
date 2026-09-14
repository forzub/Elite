# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Track:** Runtime model asset ingress migration
**Stage:** finish local client acceptance, then migrate first read-only consumer

## Immediate goal

Close acceptance of the dual-source runtime model-ingress seam without changing production object behavior:

- one shared `ModelAsset` schema/version for editor and game;
- `CompiledModelAssetReader` reads `.elmodel` only through shared `ModelAssetBinary`;
- legacy OBJ continues through `AssemblyMeshLibrary`;
- `LegacyAssemblyModelAdapter` lifts old data into `ModelAsset`;
- `RuntimeModelAssetLibrary` is the only source selector/cache per `ObjectType`;
- default remains legacy until explicit `useBinary(...)`;
- MinGW architecture contracts: PASS;
- MinGW `EliteRuntimeModelAssets`: PASS;
- MinGW `EliteServer`: PASS;
- hosted compiled-reader disk smoke: PASS;
- fix and verify the UI resource-pack API drift exposed by the MinGW `EliteGame` build;
- rerun `cmake --build build --target EliteGame` as the final local acceptance gate.

The UI API fix must preserve the executable-owned resource-pack path through `Application -> HtmlUiManager -> HtmlUiBridge -> HtmlUiServer`; do not remove the third argument from `Application.cpp`.

## After acceptance

Migrate one read-only runtime consumer from direct `AssemblyMeshLibrary` access to `RuntimeModelAssetLibrary::get`. Verify identical behavior on the legacy source first. Then switch only that object type to `.elmodel` and compare bounds, LOD/render graph and semantic bindings. Do not globally replace OBJ loading in one step.

## State discipline

Every transition step updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/game/GAME_RUNTIME_DECOMPOSITION.md` and `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` in the same accepted commit.
