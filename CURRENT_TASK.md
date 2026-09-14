# Elite — CURRENT TASK

**Updated:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** OpenGL 4.3 renderer modernization  
**Stage:** GL43-B explicit-color migration; Detail geometry is the first compatibility-clean consumer

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

## Accepted B1 behavior

The user completed visual smoke after the first `LocalMapPrimitiveRenderer` migration and reported no visible changes. That is the required visual outcome: line/cross/circle rendering moved off immediate mode while preserving appearance.

`LocalMapPrimitiveRenderer` now uses GLSL 4.30 + VAO/VBO + `glDrawArrays`; `glBegin/glEnd` and immediate `glVertex*` are permanently forbidden from returning there.

## Current candidate — GL43-B2a

The primitive API now exposes explicit-color forms for line/cross/circle.

`DetailMapGeometryPass` has been migrated completely to those forms. It no longer uses:

- `glColor*` fixed-function color state;
- `GL_CURRENT_COLOR`;
- `glBegin/glEnd`;
- immediate `glVertex*`.

Its orbit segmentation and hidden-half alpha policy are unchanged. The architecture contract now treats `DetailMapGeometryPass.cpp` as fully compatibility-clean.

Temporary no-color primitive overloads remain only to preserve unmigrated Hub/planet callers. They still bridge through `GL_CURRENT_COLOR`; they are not final API.

## Required local validation now

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Primary smoke target for B2a is Detail Map:

- spatial-volume box edges;
- hub orbits;
- player orbit;
- far-side orbit attenuation;
- small celestial-body circles and crosses;
- unchanged colors/positions/orientation.

Any regression blocks acceptance.

## Next implementation — GL43-B2b

After B2a validation:

1. migrate remaining `drawLocalMapLine/Cross/Circle` callers in `DetailMapPlanetPass`, `HubMapBackend`, `HubMapGeometryPass` and `HubMapPlanetPass` to explicit color;
2. delete the temporary no-color overloads;
3. delete `compatibilityCurrentColor()` and the last `GL_CURRENT_COLOR` use from `LocalMapPrimitiveRenderer`;
4. strengthen the static contract so `LocalMapPrimitiveRenderer.cpp` is fully compatibility-clean;
5. build + smoke Detail/Hub again.

After that the shared local-map primitive seam itself is closed.

## Following waves

- GL43-C: `DetailMapBackend` and remaining `DetailMapPlanetPass` fixed-function paths;
- GL43-D: `HubMapBackend`, `HubMapGeometryPass`, `HubMapPlanetPass`, `LocalMapAtmosphereRenderer`;
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
