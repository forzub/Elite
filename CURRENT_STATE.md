# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor baseline:** v0.10.86 accepted
**Model Asset Editor architecture:** closed at the current target boundary
**ModelAsset binary v4 architecture:** independent translation units closed
**Game runtime decomposition:** R0 physical seams established; dual-source runtime model ingress accepted; client GPU audit complete; GL4.3 Core modernization is now the active gate

## Accepted baseline

The Model Asset Editor decomposition, localization architecture, binary-v4 independent-TU closure and uniform bottom stage footer are accepted as the current editor baseline. New work is on game runtime/render architecture; editor architecture is not to be reopened without a concrete regression.

`EliteNavigationGeometry` and `EliteAssemblyGeometry` are established shared runtime seams. Dual-source runtime model ingress is accepted:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. The first read-only runtime-model consumer migration remains queued but is not the current active track.

## Client rendering decision — GL4.3 first

The graphical client already has an OpenGL 4.3 Compatibility baseline candidate in commit `19db31eca5aa2c12475d87f2dfa322f794990c6e`. The full CPU -> GPU audit is recorded in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`.

The implementation order has now changed by explicit project decision:

1. locally accept the existing OpenGL 4.3 Compatibility scaffold;
2. migrate **all currently working client rendering paths** away from fixed-function/compatibility-only OpenGL;
3. switch the client and bundled GLAD to **OpenGL 4.3 Core Profile**;
4. build/launch and visually accept all current render modes;
5. only then resume performance measurements and CPU -> GPU offload waves.

Compatibility Profile is therefore temporary scaffolding, not the final rendering baseline.

Confirmed live legacy areas already include `LocalMapPrimitiveRenderer`, `DetailMapGeometryPass`, `DetailMapPlanetPass`, `HubMapBackend`, `HubMapGeometryPass` fallback paths and `MapObjectOverlayRenderer`. A complete repository inventory is still required before Core cutover.

The authoritative migration plan is `src/render/GL43_MODERNIZATION_PLAN.md`.

## GPU offload — audited but blocked on Core acceptance

The audit conclusions remain valid but implementation is intentionally deferred. The strongest later candidate is still System Map textured-body CPU tessellation; visual-traffic GPU culling/LOD remains profile-gated; starfield compute remains lower priority; route/guidance/physics/gameplay authority stay CPU.

No compute or other algorithmic offload is required during GL4.3 modernization. Shader/VBO/VAO replacements for legacy drawing are part of the API migration even if they incidentally reduce CPU overhead.

Detailed later priorities: `src/render/GPU_OFFLOAD_PLAN.md`.

## Runtime ingress contract remains unchanged

`src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` remains accepted and unchanged by render modernization.