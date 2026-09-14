# Elite OpenGL 4.3 modernization plan

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** Core API migration complete; final local runtime acceptance pending

## Goal

Bring every working graphical client path onto an OpenGL 4.3 Core foundation without intentionally changing visible behavior, then establish a new performance baseline before any algorithmic CPU -> GPU offload.

## Final architecture

The production client now targets:

```text
OpenGL 4.3 Core context
    + bundled GLAD 2.0.8 generated for gl:core=4.3
    + explicit modern shader/VAO/VBO paths
    + CoreGlLegacyBridge for remaining legacy presentation semantics
```

`CoreGlLegacyBridge` is not an OpenGL Compatibility context. It translates old presentation operations into software state and Core-profile GPU submission:

- software model-view/projection matrices and stacks;
- software current color and texcoord state;
- software legacy texture-enable flag;
- buffered immediate-style vertices;
- GLSL `#version 430 core`;
- VAO/VBO streaming;
- `glDrawArrays`;
- legacy quads converted to triangles.

This preserves behavior while removing the driver's fixed-function/Compatibility dependency. Later path-specific cleanup may remove bridge usage where worthwhile, but that is not required for the Core API boundary.

## Migration history

### GL43-A — machine inventory

Established `tests/architecture_contracts/check_gl43_modernization_boundary.py` and stopped relying on handwritten legacy lists.

### GL43-B — shared local-map primitives

Accepted visually:

- `LocalMapPrimitiveRenderer` moved off immediate-mode driver calls;
- `DetailMapGeometryPass` became compatibility-clean;
- `HubMapGeometryPass` became compatibility-clean.

### GL43-C/D/E — remaining production compatibility debt

The final branch-wide machine audit found 23 additional files using removed state/submission APIs across maps, flight rendering, scene rendering, HUD, radar/PPI, mini-camera, world labels, debug rendering and celestial presentation.

Rather than introduce separate ad-hoc mini-renderers for every old call site, the remaining presentation syntax was routed through the shared Core bridge. This completed the API migration while preserving the existing algorithms and screen geometry.

### GL43-F — Core cutover

Completed in code:

- `Window.cpp` requests OpenGL 4.3 Core Profile;
- bundled GLAD is `gl:core=4.3`;
- production `src/` contains zero forbidden compatibility-only API tokens according to the permanent architecture test;
- temporary migration workflows were removed after validation.

## Permanent forbidden surface

Production source must not reintroduce driver fixed-function/Compatibility API including:

- `glBegin/glEnd`;
- immediate `glVertex*`, `glColor*`, `glTexCoord*`, `glNormal*`;
- fixed matrix stack calls such as `glMatrixMode`, `glPushMatrix`, `glPopMatrix`, `glLoadIdentity`, `glLoadMatrix*`, `glMultMatrix*`, `glOrtho`;
- `GL_CURRENT_COLOR`, `GL_MODELVIEW`, `GL_PROJECTION`, `GL_MATRIX_MODE` as driver state;
- legacy client arrays;
- fixed-function `glEnable/glDisable/glIsEnabled(GL_TEXTURE_2D)`.

Normal `GL_TEXTURE_2D` use as a texture target remains valid Core OpenGL.

## Automated evidence

PASS:

- final GL43 Core boundary test;
- bundled GLAD Core generation;
- zero compatibility-token inventory;
- `git diff --check` on the migration;
- Windows/MSYS2 MinGW64 execution of the Core boundary test;
- Windows/MSYS2 MinGW64 syntax compilation of `CoreGlLegacyBridge.h` with `g++ -std=c++17` against Core GLAD.

A complete GitHub-hosted `EliteGame` build is not available as a reliable gate because the repository does not contain `third_party/webview` and has no `.gitmodules` entry for it, while `CMakeLists.txt` requires that directory before configuring the client. The developer's local project tree is therefore the authoritative build/runtime gate.

## Final local acceptance

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Accept only if flight, cockpit/rear, Galaxy/System/Detail/Hub maps, close-navigation HUD, radar/PPI, mini-camera, labels, debug rendering and visible celestial/cloud/atmosphere paths remain correct under the Core context.

## After acceptance

1. Record fresh CPU/GPU timings and draw/upload counters.
2. Reactivate `CLIENT_GPU_OFFLOAD_AUDIT.md` and `GPU_OFFLOAD_PLAN.md`.
3. Start with measured high-value candidates, not blanket compute conversion.
4. Keep gameplay authority, interaction/picking semantics, navigation and replication CPU-owned unless a separate architecture decision changes that boundary.
