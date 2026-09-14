# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor baseline:** v0.10.86 accepted
**Model Asset Editor architecture:** closed at the current target boundary
**ModelAsset binary v4 architecture:** independent translation units closed
**Game runtime decomposition:** R0 physical seams established; dual-source runtime model ingress locally accepted; client GPU modernization candidate implemented

## Accepted baseline

The Model Asset Editor decomposition, localization architecture, binary-v4 independent-TU closure and uniform bottom stage footer are accepted as the current editor baseline. New work is now on the game runtime; editor architecture is not to be reopened unless a concrete regression requires it.

## Game runtime decomposition track

The game already has strong logical seams (headless server, client/server transport boundary, deterministic navigation/policy code), but most runtime `.cpp` files are still compiled directly into `EliteGame` and many are duplicated in `EliteServer` source lists. The active decomposition track converts those logical seams into CMake/link-time boundaries without changing gameplay behavior.

Wave R0 established `EliteNavigationGeometry`, owning deterministic obstacle geometry and geometric path planning, and `EliteAssemblyGeometry`, owning shared CPU OBJ hydration/assembly caching. Both `EliteGame` and `EliteServer` consume these libraries instead of recompiling the same implementation files.

Authoritative plan and decomposition rules: `src/game/GAME_RUNTIME_DECOMPOSITION.md`.

## Runtime model asset ingress — ACCEPTED

Game runtime has one canonical transition seam for disk model loading. `EliteRuntimeModelAssets` converges both sources on the shared `elite::model_asset::ModelAsset`:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority shared by editor and game. Runtime code must not duplicate the format version, chunk schema or model structures. The new binary path is never projected back down into legacy `ObjectAssembly`.

Local MinGW acceptance is complete: runtime-ingress architecture contracts pass, `EliteRuntimeModelAssets` builds, `EliteServer` builds/links, the HTML UI resource-pack API contract passes, and `EliteGame` builds/links. The `winsock2.h before windows.h` messages seen during the client build are warnings only and are separate include-order cleanup debt.

Architecture details: `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md`.

## Deferred runtime-asset migration

The ingress is accepted, but production consumers are not yet globally switched. The preserved next asset task is:

1. choose one read-only CPU consumer of `AssemblyMeshLibrary`;
2. migrate it to `RuntimeModelAssetLibrary::get(type)` while keeping `LegacyObjAssembly` as its source;
3. verify behavioral/geometry parity;
4. switch only that object type to `.elmodel`;
5. compare bounds, RenderLods, semantic bindings and required runtime metadata;
6. only then remove that consumer's direct legacy dependency.

## Client GPU modernization — CANDIDATE

Commit `19db31eca5aa2c12475d87f2dfa322f794990c6e` establishes the first GPU-modernization baseline in code. The graphical client now targets **OpenGL 4.3 Compatibility Profile**, bundled GLAD is upgraded for 4.3 compatibility, and `render::gpu::GlRuntimeCapabilities` is the single startup authority for compute/SSBO capability checks and logging. Compatibility Profile is intentional because legacy fixed-function rendering still exists; Core Profile is deferred.

This GPU baseline is **not yet runtime-accepted**. Acceptance requires a local MinGW build + launch of `EliteGame`, confirmation that the startup `[OpenGL]` line reports OpenGL >= 4.3 with `compute=1` and `ssbo=1`, and a visual smoke of the ordinary scene and Hub map.

The GPU ownership rule is strict: client GPU work is for presentation and render-derived data. Authoritative simulation, gameplay decisions, damage, economy, replication and CPU-consumed navigation remain CPU-owned by default. Avoid frame paths that dispatch compute and then synchronously read results back for gameplay decisions.

Initial offload priorities are recorded in `src/render/GPU_OFFLOAD_PLAN.md`: P0 procedural weather/cloud texture generation, P1 starfield derived transforms/filtering/draw preparation, then measured large-instance/frustum culling. General world-signal labels are currently disabled; Hub-map and close-navigation labels remain active.
