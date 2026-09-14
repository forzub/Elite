# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor baseline:** v0.10.86 accepted
**Model Asset Editor architecture:** closed at the current target boundary
**ModelAsset binary v4 architecture:** independent translation units closed
**Game runtime decomposition:** R0 physical seams established; dual-source runtime model ingress locally accepted; client GPU audit complete; GL43 runtime acceptance still pending

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

## Client GPU modernization — AUDIT COMPLETE, BASELINE CANDIDATE

Commit `19db31eca5aa2c12475d87f2dfa322f794990c6e` establishes the first GPU-modernization baseline in code. The graphical client targets **OpenGL 4.3 Compatibility Profile**, bundled GLAD exposes the 4.3 compatibility API, and `render::gpu::GlRuntimeCapabilities` is the startup authority for compute/SSBO capability checks and logging. Compatibility Profile remains intentional while legacy fixed-function rendering exists.

The GPU baseline is **not yet runtime-accepted**. Acceptance still requires a local MinGW build + launch of `EliteGame`, startup `[OpenGL] >= 4.3`, `compute=1`, `ssbo=1`, plausible limits, and visual smoke of the ordinary scene and Hub map.

A full client CPU -> GPU audit is now recorded in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`. It corrected the earlier provisional shortlist:

- **P0:** System Map textured celestial bodies currently rebuild latitude/longitude sphere vertices on CPU every map frame and re-upload them. Large bodies generate 49,152 vertices/body/frame. Target is a shared static indexed unit sphere + vertex shader/per-body parameters, **not compute**.
- **P1, profile-gated:** `SceneRenderer` visual-traffic prepare/cull/LOD/compaction may justify SSBO/compute/indirect drawing at high ship/part counts. `prepareScene()` needs its own timing before any migration.
- **P1/P2:** repeated System/Galaxy map circles/halos/billboards are candidates for static parameterized primitives/instancing; usually no compute shader is needed.
- **P2:** starfield compute is deferred because the catalog rebuild is threshold-triggered, not per-frame.
- runtime procedural cloud texture generation is already shader/FBO driven; the old CPU weather/cloud generators are no longer assumed to be runtime P0;
- `PlanetWireRenderer` geometry is initialization-time/static, and celestial texture baking is offline.

The ownership rule remains strict: client GPU work is for presentation and render-derived data. Authoritative simulation, gameplay decisions, damage, economy, replication and CPU-consumed navigation stay CPU-owned by default. Avoid frame paths that dispatch compute and then synchronously read results back for gameplay decisions.

Detailed priorities and measurement gates: `src/render/GPU_OFFLOAD_PLAN.md`.

## Runtime ingress contract remains unchanged

The GPU audit does not alter `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md`. Dual-source model ingress is still the accepted asset boundary and its deferred consumer migration remains preserved.
