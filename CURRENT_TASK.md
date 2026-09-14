# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Track:** Runtime model asset ingress migration
**Stage:** prepare dual-source disk loading before production binary cutover

## Immediate goal

Accept the new runtime model-ingress seam without changing production object behavior:

- one shared `ModelAsset` schema/version for editor and game;
- `CompiledModelAssetReader` reads `.elmodel` only through shared `ModelAssetBinary`;
- legacy OBJ continues through `AssemblyMeshLibrary`;
- `LegacyAssemblyModelAdapter` lifts old data into `ModelAsset`;
- `RuntimeModelAssetLibrary` is the only source selector/cache per `ObjectType`;
- default remains legacy until explicit `useBinary(...)`;
- build `EliteGame` and `EliteServer` with the new library;
- run the runtime ingress architecture contract and compiled-reader smoke.

## After acceptance

Migrate one read-only runtime consumer from direct `AssemblyMeshLibrary` access to `RuntimeModelAssetLibrary::get`. Verify identical behavior on the legacy source first. Then switch only that object type to `.elmodel` and compare bounds, LOD/render graph and semantic bindings. Do not globally replace OBJ loading in one step.

## State discipline

Every transition step updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/game/GAME_RUNTIME_DECOMPOSITION.md` and `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` in the same accepted commit.
