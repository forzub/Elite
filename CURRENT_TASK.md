# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Track:** Client GPU modernization
**Stage:** locally accept the OpenGL 4.3 baseline, then migrate measured client hot paths

## Accepted prerequisites

The runtime architecture groundwork is accepted on MinGW64:

- `EliteNavigationGeometry` and `EliteAssemblyGeometry` physical seams are established;
- dual-source runtime model ingress is accepted;
- runtime-ingress architecture contracts pass;
- `EliteRuntimeModelAssets`, `EliteServer` and `EliteGame` build/link;
- `HtmlUiManager -> HtmlUiBridge -> HtmlUiServer` preserves `resourcePackPath`;
- shared `ModelAsset` schema/version remains authoritative for both editor and game.

The first read-only runtime-model consumer migration remains queued and must not be forgotten, but GPU foundation work is the active track by explicit decision.

## Immediate acceptance target — GL43 foundation

The code candidate is already implemented in commit `19db31eca5aa2c12475d87f2dfa322f794990c6e`. Do not treat it as accepted until the local runtime gate passes.

Required local gate:

```bash
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Then verify:

- the startup `[OpenGL]` line reports actual OpenGL >= 4.3;
- `compute=1`;
- `ssbo=1`;
- logged GPU/vendor/GLSL and compute limits are plausible;
- ordinary flight/system scene renders correctly;
- Hub map renders correctly;
- no regression in active Hub/close-navigation labels;
- general world-signal labels remain intentionally disabled.

OpenGL **4.3 Compatibility Profile** is transitional. Do **not** switch to Core Profile until remaining fixed-function call sites are removed.

## GPU offload waves after GL43 acceptance

P0: migrate `PlanetaryWeatherMapGenerator` and `CloudAppearanceTextureGenerator` to compute while retaining CPU reference/parity paths during validation.

P1: migrate `GalaxyStarfieldRenderer` derived observer-relative transforms, filtering, visibility/appearance preparation and repeated dynamic buffer preparation toward persistent GPU-resident data. After measurement, evaluate SSBO culling/indirect draw for sufficiently large instance/frustum workloads.

P2: profile procedural celestial mesh/detail generation and `PlanetWireRenderer` CPU geometry before deciding whether compute migration is better than caching/precomputation.

Do not move authoritative route/path decisions, authoritative physics or gameplay state transitions to the client GPU simply because they are expensive. GPU visualization derivatives are allowed when they remain presentation-only and avoid synchronous CPU readback.

Detailed policy: `src/render/GPU_OFFLOAD_PLAN.md`.

## Deferred but preserved runtime-asset task

After the current GPU baseline/wave work reaches a stable checkpoint, resume the model-ingress migration:

1. migrate one read-only consumer from direct `AssemblyMeshLibrary` access to `RuntimeModelAssetLibrary::get`;
2. keep that consumer on legacy source and verify parity first;
3. switch only the same object type to `.elmodel`;
4. compare bounds, LOD/render graph, semantic bindings and required runtime metadata;
5. remove the direct legacy dependency only after parity.

## State discipline

Every accepted GPU wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/game/GAME_RUNTIME_DECOMPOSITION.md` and `src/render/GPU_OFFLOAD_PLAN.md` in the same accepted commit. Runtime-model ingress changes also update `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md`.

Chat history is not canonical project state; these repository MD files are.
