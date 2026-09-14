# Elite — CURRENT STATE

**Updated:** 2026-09-15  
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

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration is queued behind renderer modernization.

## Client rendering decision — GL4.3 Core before GPU offload

The graphical client still uses OpenGL 4.3 Compatibility as temporary migration scaffolding. Required order remains:

```text
4.3 Compatibility scaffold
    -> retire all compatibility-only rendering
    -> 4.3 Core Profile accepted
    -> fresh performance baseline
    -> selective CPU -> GPU offload
```

## Accepted GL43 checkpoints

### B1 — accepted

`LocalMapPrimitiveRenderer` line/cross/circle submission moved from immediate mode to GLSL 4.30 + VAO/VBO with no visible regression in local smoke.

### B2a — accepted

`DetailMapGeometryPass` moved to explicit-color modern primitives, including orbit rendering. Hub/player orbits, far-side attenuation, volume edges and small-body markers remained visually unchanged.

### B2b Hub geometry — accepted

`HubMapGeometryPass.cpp` is now fully compatibility-clean and its local smoke is accepted. The user reports Hub Map still looks as before after fallback box/axis/velocity/grid/screen-marker drawing moved to explicit-color modern primitives.

The architecture guard therefore treats both of these files as permanent no-compatibility zones:

- `src/game/system_map/DetailMapGeometryPass.cpp`;
- `src/game/system_map/HubMapGeometryPass.cpp`.

## Active seam — finish GL43-B2b

`LocalMapPrimitiveRenderer` itself is not yet fully Core-clean because temporary no-color overloads still bridge three callers through `GL_CURRENT_COLOR`:

- `DetailMapPlanetPass`;
- `HubMapBackend`;
- `HubMapPlanetPass`.

The immediate next step is to migrate those calls to explicit `glm::vec4` color, delete the no-color overloads and `compatibilityCurrentColor()`, and move `LocalMapPrimitiveRenderer.cpp` into the full no-compatibility guard.

At that point GL43-B is complete.

## Remaining GL43 waves after B

- **GL43-C:** remaining Detail Map fixed-function code in `DetailMapBackend` and `DetailMapPlanetPass`;
- **GL43-D:** remaining Hub/local celestial code in `HubMapBackend`, `HubMapPlanetPass`, `LocalMapAtmosphereRenderer`;
- **GL43-E:** `MapObjectOverlayRenderer`, `DebugGrid`, and every remaining machine-inventory offender;
- **GL43-F:** switch bundled GLAD/context to OpenGL 4.3 Core Profile and run complete visual acceptance.

CPU -> GPU optimization remains blocked until GL43-F passes.

## Ownership boundaries unchanged

Authoritative physics, route/path/docking decisions, damage, economy, replication and CPU interaction semantics remain CPU-owned.

`src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` remains accepted and unchanged.
