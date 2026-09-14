# Elite OpenGL 4.3 modernization plan

**Date:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Decision:** finish the renderer migration to OpenGL 4.3 Core Profile before any CPU -> GPU offload wave.

## Goal

Bring every currently working graphical client path onto a modern OpenGL 4.3 rendering foundation while preserving visible behavior. Compatibility Profile is temporary migration scaffolding.

The phase ends only when `EliteGame` runs in **OpenGL 4.3 Core Profile** and no production render path depends on removed fixed-function/compatibility APIs.

## Required order

```text
OpenGL 4.3 Compatibility scaffold
        -> machine inventory of compatibility debt
        -> remove fixed-function dependencies wave by wave
        -> OpenGL 4.3 Core Profile accepted
        -> capture fresh performance baselines
        -> CPU/GPU offload only where measurements justify it
```

## Forbidden compatibility surface at final gate

Production client code must not rely on:

- `glBegin`, `glEnd`;
- immediate `glVertex*`, `glColor*`, `glTexCoord*`, `glNormal*`;
- `glMatrixMode`, `glPushMatrix`, `glPopMatrix`, `glLoadIdentity`, `glLoadMatrix*`, `glMultMatrix*`, `glOrtho`;
- `GL_CURRENT_COLOR`, `GL_MODELVIEW`, `GL_PROJECTION`, `GL_MATRIX_MODE`;
- `glEnable/glDisable(GL_TEXTURE_2D)` as fixed-function texture state;
- legacy client arrays.

## Migration rules

1. Preserve behavior before redesign.
2. Do not mix compute/SSBO algorithm offload into this phase.
3. Prefer shared primitive/render foundations instead of bespoke replacements.
4. Matrices, coordinates and colors become explicit shader inputs.
5. Each pass owns the GL state it changes.
6. CPU interaction/picking/semantic data stays CPU-side.
7. Compatibility debt shrinks monotonically and migrated files receive static guards.

## GL43-A — scaffold + machine inventory

`tests/architecture_contracts/check_gl43_modernization_boundary.py` scans production C/C++ for compatibility-only OpenGL, reports current debt and protects migrated files against regression.

## GL43-B — shared local/screen primitive foundation

### B1 — accepted

`LocalMapPrimitiveRenderer` line/cross/circle drawing uses GLSL 4.30 Core, VAO/VBO and `glDrawArrays`. Local smoke showed no visible change.

### B2a — accepted

`DetailMapGeometryPass` is fully compatibility-clean and uses explicit-color primitives. Orbit rendering preserves segmentation and hidden-side `0.16x` alpha behavior, with batched line submission. Local smoke showed no visible change.

### B2b Hub geometry — accepted

`HubMapGeometryPass` is fully compatibility-clean. Fallback boxes, axes, velocity lines, screen markers, adaptive grid and grid axes use explicit-color modern primitives. Local smoke again showed no visible regression.

### B2b remaining — close the primitive seam

Temporary no-color primitive overloads still serve three callers through `GL_CURRENT_COLOR`:

- `DetailMapPlanetPass`;
- `HubMapBackend`;
- `HubMapPlanetPass`.

To complete GL43-B:

- migrate those calls to explicit color;
- delete the no-color overloads;
- delete `compatibilityCurrentColor()`;
- make `LocalMapPrimitiveRenderer.cpp` fully `NO_COMPATIBILITY_FILES` protected;
- smoke Detail/Hub again.

## GL43-C — Detail Map

Migrate remaining Detail compatibility debt:

- `DetailMapBackend` fixed-function projection/background;
- `DetailMapPlanetPass` sphere-grid, filled disk and projected fallback geometry.

`DetailMapGeometryPass` is accepted and should not be reopened except for a regression.

## GL43-D — Hub Map and local celestial presentation

Migrate remaining Hub/local celestial debt:

- `HubMapBackend` projection/background;
- `HubMapPlanetPass` fallback body/local-circle immediate paths;
- `LocalMapAtmosphereRenderer` remaining fixed-function soft-band path.

`HubMapGeometryPass` is accepted and should not be reopened except for a regression.

## GL43-E — overlays, debug and remaining debt

Migrate `MapObjectOverlayRenderer`, `DebugGrid` and every remaining offender printed by the architecture scan.

## GL43-F — Core Profile cutover

Only after the scan is clean:

- switch bundled GLAD to **OpenGL 4.3 Core**;
- request `GLFW_OPENGL_CORE_PROFILE`;
- build `EliteGame`;
- launch and confirm Core 4.3+;
- smoke ordinary flight, cockpit/rear view, Galaxy/System/Detail/Hub maps and close-navigation HUD;
- keep the static contract permanently.

## Final acceptance

The GL43 modernization phase is accepted only when:

- `EliteGame` builds on MinGW64;
- runtime creates an OpenGL 4.3+ Core Profile context;
- startup capability logging remains valid;
- compatibility scan is clean;
- all current visual modes pass smoke;
- no working feature was removed to satisfy Core.

After this gate, `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md` become active implementation plans again.
