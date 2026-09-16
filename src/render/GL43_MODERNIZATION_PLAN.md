# Elite OpenGL 4.3 modernization plan

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** **COMPLETE / ACCEPTED LOCALLY**

## Goal — complete

Every working graphical client path now runs on an OpenGL 4.3 Core foundation without a driver Compatibility context. The API migration is closed; current renderer work has moved to selective CPU -> GPU optimization.

## Accepted architecture

```text
OpenGL 4.3 Core context
    + bundled GLAD 2.0.8 generated for gl:core=4.3
    + explicit shader/VAO/VBO paths
    + CoreGlLegacyBridge for residual legacy presentation semantics
```

`CoreGlLegacyBridge` is software compatibility inside the engine, not an OpenGL Compatibility profile. It maps residual old presentation operations onto software matrix/color/texcoord state and GLSL 4.30 Core submission. Production code no longer depends on removed driver fixed-function APIs.

## Migration history

### GL43-A — machine inventory

`tests/architecture_contracts/check_gl43_modernization_boundary.py` became the authoritative repository-wide compatibility scan.

### GL43-B — local-map primitive foundation

Visually accepted during migration:

- `LocalMapPrimitiveRenderer` immediate-mode removal;
- `DetailMapGeometryPass` Core cleanup;
- `HubMapGeometryPass` Core cleanup.

### GL43-C/D/E — remaining production compatibility debt

The branch-wide scan found 23 additional compatibility-dependent production files across maps, flight/scene rendering, HUD, radar/PPI, mini-camera, world labels, debug rendering and celestial presentation. These paths were moved to explicit Core rendering or routed through the shared Core bridge while preserving behavior.

### GL43-F — Core cutover

Completed:

- `Window.cpp` requests OpenGL 4.3 Core Profile;
- bundled GLAD is `gl:core=4.3`;
- production `src/` contains zero forbidden Compatibility/fixed-function API tokens under the permanent test;
- migration and temporary validation workflows were removed after use.

Core API migration commit: `37999c5588c6e85ef88a24e0efbf2dfff89b9314`.

## Validation evidence

Automated PASS:

- GL4.3 Core boundary test;
- bundled Core GLAD generation;
- zero compatibility-token inventory;
- migration `git diff --check`;
- Windows/MSYS2 MinGW64 execution of the Core boundary test;
- Windows/MSYS2 MinGW64 syntax compilation of `CoreGlLegacyBridge.h` against Core GLAD.

Local developer acceptance on 2026-09-15:

- `EliteGame` built and launched;
- user reported all migrated visible/runtime paths working.

The station-adjacent freezes were present before this transformation and are not classified as a GL4.3 regression. Their diagnosis is deferred by explicit project decision.

## Permanent forbidden surface

Production source must not reintroduce driver fixed-function/Compatibility API including:

- `glBegin/glEnd`;
- immediate `glVertex*`, `glColor*`, `glTexCoord*`, `glNormal*`;
- fixed matrix stack calls;
- driver `GL_CURRENT_COLOR`, `GL_MODELVIEW`, `GL_PROJECTION`, `GL_MATRIX_MODE` state;
- legacy client arrays;
- fixed-function `glEnable/glDisable/glIsEnabled(GL_TEXTURE_2D)`.

Normal Core texture-target use of `GL_TEXTURE_2D` remains valid.

## Handoff

The active plan is now `src/render/GPU_OFFLOAD_PLAN.md`.

The first post-Core candidate is the System Map static textured sphere migration. Future work must preserve the authority boundary: rendering derivatives may move to GPU, while gameplay, navigation/planning, replication and synchronous CPU interaction answers remain CPU-owned unless separately redesigned.
