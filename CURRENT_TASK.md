# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** OpenGL 4.3 renderer modernization  
**Stage:** finish GL43-B2b by closing the shared local-map primitive seam

## Active rule

Do **not** start System Map sphere offload, compute culling, starfield compute or other CPU -> GPU optimization work yet.

```text
4.3 Compatibility scaffold
    -> remove compatibility-only rendering
    -> 4.3 Core Profile accepted
    -> measure again
    -> offload only measured hot paths
```

Detailed plan: `src/render/GL43_MODERNIZATION_PLAN.md`.

## Accepted behavior

B1, B2a and Hub geometry cleanup are visually accepted by local runtime smoke. No visible regressions were reported after:

- `LocalMapPrimitiveRenderer` moved line/cross/circle drawing off immediate mode;
- `DetailMapGeometryPass` moved to explicit colors and modern primitive submission;
- `HubMapGeometryPass` moved all fallback box/axis/velocity/grid/screen-marker drawing to the same explicit-color modern primitive path.

`DetailMapGeometryPass.cpp` and `HubMapGeometryPass.cpp` are permanent `NO_COMPATIBILITY_FILES` in the architecture contract.

## Current implementation — close B2b

Three bridge callers still use temporary no-color primitive overloads and therefore keep `GL_CURRENT_COLOR` alive in `LocalMapPrimitiveRenderer`:

1. `DetailMapPlanetPass`;
2. `HubMapBackend`;
3. `HubMapPlanetPass`.

Next code changes:

1. migrate those calls to explicit `glm::vec4` color;
2. delete no-color overloads from `LocalMapPrimitiveRenderer.h/.cpp`;
3. delete `compatibilityCurrentColor()` and the last `GL_CURRENT_COLOR` dependency in that renderer;
4. promote `LocalMapPrimitiveRenderer.cpp` from the immediate-only guard to `NO_COMPATIBILITY_FILES`;
5. build and smoke Detail/Hub again.

Once this passes, **GL43-B is complete**.

## Following waves

### GL43-C — Detail Map

Remove remaining fixed-function code from:

- `DetailMapBackend` projection/background;
- `DetailMapPlanetPass` sphere grid, filled disk and fallback projected geometry.

### GL43-D — Hub/local celestial

Remove remaining fixed-function code from:

- `HubMapBackend` projection/background;
- `HubMapPlanetPass` fallback body/local-circle paths;
- `LocalMapAtmosphereRenderer` soft-band compatibility path.

### GL43-E — remaining debt

Migrate `MapObjectOverlayRenderer`, `DebugGrid` and every remaining offender printed by `check_gl43_modernization_boundary.py`.

### GL43-F — Core cutover

Only after the machine inventory is clean:

- switch bundled GLAD to OpenGL 4.3 Core;
- request `GLFW_OPENGL_CORE_PROFILE`;
- build and launch `EliteGame`;
- smoke flight, cockpit/rear view, Galaxy/System/Detail/Hub and close-navigation HUD.

## Deferred until after Core acceptance

- System Map textured-sphere CPU tessellation removal;
- SceneRenderer compute culling/LOD/compaction;
- instance-stream optimization;
- starfield compute migration;
- other CPU -> GPU algorithmic offload.

The prior audit remains preserved in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md`.

## State discipline

Every accepted GL43 wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/render/GL43_MODERNIZATION_PLAN.md` and `src/game/GAME_RUNTIME_DECOMPOSITION.md`. Runtime model-ingress docs change only when that boundary changes.
