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

The graphical client uses a transitional OpenGL 4.3 Compatibility context. Compatibility Profile is migration scaffolding only.

Required order:

```text
4.3 Compatibility scaffold
    -> inventory + retire compatibility-only rendering
    -> 4.3 Core Profile accepted
    -> fresh performance baseline
    -> selective CPU -> GPU offload
```

The CPU -> GPU audit remains recorded in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`, but its implementation waves are blocked until Core Profile acceptance.

## Accepted GL43 behavior checkpoints

### B1 — accepted

The user reported no visible change after `LocalMapPrimitiveRenderer` line/cross/circle submission moved from immediate mode to GLSL 4.30 + VAO/VBO.

### B2a — accepted

The user again reported Detail Map behavior visually unchanged after `DetailMapGeometryPass` moved to explicit-color primitives and retired its own fixed-function color/current-color/immediate orbit submission.

Accepted preserved behavior includes hub/player orbits, hidden-half attenuation, volume edges and small-body markers.

## Current candidate — B2b / Hub geometry cleanup

`HubMapGeometryPass.cpp` has now crossed the full compatibility-clean boundary:

- no `glColor*`;
- no `glBegin/glEnd`;
- no immediate `glVertex*`;
- fallback box/axis/velocity/grid rendering passes explicit `glm::vec4` colors to `LocalMapPrimitiveRenderer`;
- fallback screen marker circle/cross uses the same modern primitive path;
- the architecture contract permanently forbids compatibility-only API from returning to this file.

This does not redesign Hub geometry; it only replaces the submission/state mechanism.

`LocalMapPrimitiveRenderer` itself is **not yet fully Core-clean** because temporary no-color overloads still support unmigrated callers in `DetailMapPlanetPass`, `HubMapBackend` and `HubMapPlanetPass` via `GL_CURRENT_COLOR`.

## Remaining confirmed legacy areas

Compatibility debt still includes at least:

- `src/render/DebugGrid.cpp`;
- `src/game/system_map/DetailMapBackend.cpp`;
- `src/game/system_map/DetailMapPlanetPass.cpp`;
- `src/game/system_map/HubMapBackend.cpp`;
- `src/game/system_map/HubMapPlanetPass.cpp`;
- `src/game/system_map/LocalMapAtmosphereRenderer.cpp`;
- `src/game/system_map/MapObjectOverlayRenderer.cpp`;
- transitional compatibility overloads in `LocalMapPrimitiveRenderer.cpp`.

`DetailMapGeometryPass.cpp` and `HubMapGeometryPass.cpp` are now static no-compatibility zones.

The architecture scan output is the machine authority; this list is only a readable checkpoint.

Authoritative renderer migration plan: `src/render/GL43_MODERNIZATION_PLAN.md`.

## Current validation gate

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Primary visual target is Hub Map geometry: station/module fallback boxes, axis lines, adaptive grid, screen circles/crosses and velocity marker lines must remain visually unchanged.

## Ownership boundaries unchanged

Render modernization does not move gameplay authority. Authoritative physics, route/path/docking decisions, damage, economy, replication and CPU interaction semantics remain CPU-owned.

`src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` remains accepted and unchanged.
