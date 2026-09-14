# Elite — CURRENT TASK

**Updated:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** OpenGL 4.3 renderer modernization  
**Stage:** GL43-B explicit-color migration; Detail geometry accepted, Hub geometry candidate is now compatibility-clean

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

B1 and B2a are visually accepted by local smoke. The user reports no visible changes after:

- `LocalMapPrimitiveRenderer` moved line/cross/circle drawing off immediate mode;
- `DetailMapGeometryPass` moved to explicit colors and modern primitive submission.

## Current candidate — HubMapGeometryPass cleanup

`HubMapGeometryPass.cpp` no longer uses any compatibility-only API tracked by the GL43 architecture contract.

Its fallback rendering now uses explicit-color `LocalMapPrimitiveRenderer` calls for:

- box edges;
- X/Y/Z axes;
- velocity line;
- screen circle/cross markers;
- adaptive grid and grid axes.

The modern `HubMapGeometryRenderer` path remains unchanged. Only the old fallback submission/state mechanism was replaced.

The architecture guard now places both:

- `DetailMapGeometryPass.cpp`;
- `HubMapGeometryPass.cpp`;

inside `NO_COMPATIBILITY_FILES`.

## Required local validation now

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Primary smoke target is Hub Map:

- adaptive grid;
- hub axes;
- fallback module/station boxes if visible;
- screen circles/crosses;
- velocity marker lines;
- unchanged colors, position and orientation.

Any regression blocks acceptance.

## Remaining B2b work

After Hub geometry validation:

1. migrate remaining no-color primitive callers in `DetailMapPlanetPass`, `HubMapBackend` and `HubMapPlanetPass`;
2. delete the temporary no-color overloads from `LocalMapPrimitiveRenderer.h/.cpp`;
3. delete `compatibilityCurrentColor()` and the last `GL_CURRENT_COLOR` dependency from that renderer;
4. move `LocalMapPrimitiveRenderer.cpp` into `NO_COMPATIBILITY_FILES`;
5. smoke Detail/Hub again.

At that point the shared local-map primitive seam is fully closed.

## Following waves

- GL43-C: `DetailMapBackend` and remaining `DetailMapPlanetPass` fixed-function paths;
- GL43-D: `HubMapBackend`, `HubMapPlanetPass`, `LocalMapAtmosphereRenderer`;
- GL43-E: `MapObjectOverlayRenderer`, `DebugGrid` and every remaining inventory offender;
- GL43-F: bundled GLAD Core 4.3 + `GLFW_OPENGL_CORE_PROFILE`, full build/runtime/visual acceptance.

## Deferred until after Core acceptance

- System Map textured-sphere CPU tessellation removal;
- SceneRenderer compute culling/LOD/compaction;
- instance-stream optimization;
- starfield compute migration;
- other CPU -> GPU algorithmic offload.

The prior audit remains preserved in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md`.

## State discipline

Every accepted GL43 wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/render/GL43_MODERNIZATION_PLAN.md` and `src/game/GAME_RUNTIME_DECOMPOSITION.md`. Runtime model-ingress docs change only when that boundary changes.
