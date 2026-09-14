# Elite — CURRENT STATE

**Updated:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted; OpenGL 4.3 Core modernization is the active gate

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` are established shared runtime seams. Dual-source runtime model ingress is accepted:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. The first read-only runtime-model consumer migration remains queued but is not the active track.

## Client rendering decision — GL4.3 Core before GPU offload

The graphical client has a transitional OpenGL 4.3 Compatibility context candidate. Compatibility Profile is migration scaffolding only.

Required order:

```text
4.3 Compatibility scaffold
    -> inventory + retire compatibility-only rendering
    -> 4.3 Core Profile accepted
    -> fresh performance baseline
    -> selective CPU -> GPU offload
```

The CPU -> GPU audit remains recorded in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`, but its implementation waves are blocked until Core Profile acceptance.

## GL43-A / GL43-B1 candidate

The first modernization code candidate now does two things:

1. `tests/architecture_contracts/check_gl43_modernization_boundary.py` mechanically scans production C/C++ sources for the compatibility surface and prints the current debt inventory on every run. It also locks the temporary 4.3 Compatibility request and establishes a monotonic migrated-file guard.
2. `src/game/system_map/LocalMapPrimitiveRenderer.cpp` no longer uses `glBegin/glEnd` or immediate `glVertex*`. Line/cross/circle submission now uses a GLSL 4.30 Core shader plus VAO/VBO and explicit pixel-to-NDC conversion from the active viewport.

`LocalMapPrimitiveRenderer` is **not yet Core-clean**: existing callers still communicate color through fixed-function current-color state, so the renderer temporarily reads `GL_CURRENT_COLOR`. The next GL43-B step is to make color explicit in the primitive API and migrate callers, after which `GL_CURRENT_COLOR` is removed from this seam.

This staged split is deliberate: first retire immediate-mode submission without changing every map caller in one patch; then remove the hidden color-state contract.

## Expanded confirmed legacy inventory

The manual audit now confirms compatibility debt in at least:

- `src/render/DebugGrid.cpp`;
- `src/game/system_map/DetailMapBackend.cpp`;
- `src/game/system_map/DetailMapGeometryPass.cpp`;
- `src/game/system_map/DetailMapPlanetPass.cpp`;
- `src/game/system_map/HubMapBackend.cpp`;
- `src/game/system_map/HubMapGeometryPass.cpp`;
- `src/game/system_map/HubMapPlanetPass.cpp`;
- `src/game/system_map/LocalMapAtmosphereRenderer.cpp`;
- `src/game/system_map/MapObjectOverlayRenderer.cpp`;
- transitional `GL_CURRENT_COLOR` in `LocalMapPrimitiveRenderer.cpp`.

The architecture scan is the source of truth for the complete machine inventory when run locally; do not treat this manual list as exhaustive.

Authoritative renderer migration plan: `src/render/GL43_MODERNIZATION_PLAN.md`.

## Acceptance status

This code candidate is **not yet locally accepted**. Required local gate:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Smoke ordinary flight, cockpit/rear view, Galaxy/System/Detail/Hub maps and close-navigation HUD. For this wave specifically verify Detail/Hub screen-space lines, crosses and circles retain position, color and orientation.

## Ownership boundaries unchanged

Render modernization does not move gameplay authority. Authoritative physics, route/path/docking decisions, damage, economy, replication and CPU interaction semantics remain CPU-owned.

`src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` remains accepted and unchanged.
