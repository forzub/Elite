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

## GL43-A / GL43-B1 status

The first migration seam is now visually accepted by local runtime smoke: the user reports no visible change after the `LocalMapPrimitiveRenderer` immediate-mode replacement.

Implemented foundation:

1. `tests/architecture_contracts/check_gl43_modernization_boundary.py` mechanically scans production C/C++ for compatibility-only OpenGL and prints the current debt inventory.
2. `src/game/system_map/LocalMapPrimitiveRenderer.cpp` no longer submits line/cross/circle geometry through `glBegin/glEnd` or immediate `glVertex*`; it uses GLSL 4.30 + VAO/VBO + pixel-to-NDC conversion.
3. Immediate-mode submission is statically forbidden from returning to that seam.

The visual B1 contract is therefore preserved: modernization changed the API path, not the rendered result.

## GL43-B2a candidate — explicit color + Detail geometry cleanup

The primitive API now has explicit-color overloads:

```text
drawLocalMapLine(..., glm::vec4 color)
drawLocalMapCross(..., glm::vec4 color)
drawLocalMapCircle(..., glm::vec4 color)
```

`DetailMapGeometryPass` has been migrated to them completely:

- fixed-function `glColor*` state removed from the pass;
- `GL_CURRENT_COLOR` removed from orbit rendering;
- its own `glBegin(GL_LINES)` orbit path removed;
- orbit visibility/far-side alpha behavior is preserved (`0.16x` alpha on the hidden half);
- the architecture guard now forbids any compatibility-only API from returning to `DetailMapGeometryPass.cpp`.

`LocalMapPrimitiveRenderer` is **not yet fully Core-clean** because temporary no-color overloads still bridge unmigrated Hub/planet callers through `GL_CURRENT_COLOR`. Those overloads are transitional and must disappear after the remaining callers move to explicit color.

## Remaining confirmed legacy areas

Compatibility debt still includes at least:

- `src/render/DebugGrid.cpp`;
- `src/game/system_map/DetailMapBackend.cpp`;
- `src/game/system_map/DetailMapPlanetPass.cpp`;
- `src/game/system_map/HubMapBackend.cpp`;
- `src/game/system_map/HubMapGeometryPass.cpp`;
- `src/game/system_map/HubMapPlanetPass.cpp`;
- `src/game/system_map/LocalMapAtmosphereRenderer.cpp`;
- `src/game/system_map/MapObjectOverlayRenderer.cpp`;
- transitional compatibility overloads in `LocalMapPrimitiveRenderer.cpp`.

The architecture scan output is the machine authority; this list is only a readable checkpoint.

Authoritative renderer migration plan: `src/render/GL43_MODERNIZATION_PLAN.md`.

## Current validation gate

For GL43-B2a:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Primary visual target is Detail Map: volume edges, hub/player orbits, small-body circles/crosses and their colors must remain unchanged.

## Ownership boundaries unchanged

Render modernization does not move gameplay authority. Authoritative physics, route/path/docking decisions, damage, economy, replication and CPU interaction semantics remain CPU-owned.

`src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` remains accepted and unchanged.
