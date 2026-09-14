# Elite — CURRENT TASK

**Updated:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** OpenGL 4.3 renderer modernization  
**Stage:** validate GL43-A inventory guard + GL43-B1 immediate-mode retirement, then remove hidden current-color state from local-map primitives

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

## Current candidate — GL43-A + GL43-B1

### Architecture scan

Run:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
```

The contract:

- verifies the temporary GLFW context remains OpenGL 4.3 Compatibility during migration;
- scans production C/C++ sources for immediate mode, fixed-function matrix state, current-color state, fixed texture enable state and legacy client arrays;
- prints every current offender and token count;
- permanently forbids `LocalMapPrimitiveRenderer.cpp` from returning to immediate-mode submission.

The printed inventory is the mechanical baseline for subsequent waves.

### LocalMapPrimitiveRenderer B1

`LocalMapPrimitiveRenderer` now submits line/cross/circle geometry with:

```text
pixel-space CPU vertices
    -> streaming VBO + VAO
    -> GLSL 4.30 Core vertex shader
    -> explicit pixel -> NDC transform from GL_VIEWPORT
    -> uniform color
    -> glDrawArrays
```

No `glBegin/glEnd` or immediate `glVertex*` remains in this file.

For behavioral safety, B1 still reads the caller's legacy `GL_CURRENT_COLOR` and forwards it to the shader uniform. This is transitional debt, not the final API.

## Required local validation now

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify startup still reports OpenGL >= 4.3 with `compute=1`, `ssbo=1`.

Visual smoke:

- ordinary flight;
- cockpit and rear view;
- Galaxy Map;
- System Map;
- Detail Map;
- Hub Map;
- close-navigation HUD;
- specifically Detail/Hub lines, circles, crosses, grid markers and fallback markers.

Any color/position/orientation regression blocks acceptance.

## Next implementation — GL43-B2

After B1 local acceptance:

1. extend `LocalMapPrimitiveRenderer` line/cross/circle API with explicit `glm::vec4 color`;
2. update every caller instead of preceding calls with `glColor*`;
3. remove `compatibilityCurrentColor()` and `GL_CURRENT_COLOR`;
4. tighten `check_gl43_modernization_boundary.py` so the whole file is compatibility-clean;
5. build + smoke again.

Do not combine B2 with Detail planet disk/grid migration. Keep the seam small.

## Following waves

After B2:

- GL43-C: `DetailMapBackend`, `DetailMapGeometryPass`, `DetailMapPlanetPass`;
- GL43-D: `HubMapBackend`, `HubMapGeometryPass`, `HubMapPlanetPass`, `LocalMapAtmosphereRenderer`;
- GL43-E: `MapObjectOverlayRenderer`, `DebugGrid` and every remaining inventory offender;
- GL43-F: GLAD Core 4.3 + `GLFW_OPENGL_CORE_PROFILE`, complete build/runtime/visual acceptance.

## Deferred until after Core acceptance

- System Map textured-sphere CPU tessellation removal;
- SceneRenderer compute culling/LOD/compaction;
- instance-stream optimization;
- starfield compute migration;
- other CPU -> GPU algorithmic offload.

The prior audit remains preserved in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md`.

## State discipline

Every accepted GL43 wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/render/GL43_MODERNIZATION_PLAN.md` and `src/game/GAME_RUNTIME_DECOMPOSITION.md`. Runtime model-ingress docs change only when that boundary changes.
