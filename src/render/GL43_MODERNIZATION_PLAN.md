# Elite OpenGL 4.3 modernization plan

**Date:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Decision:** finish the renderer migration to OpenGL 4.3 Core Profile before any CPU -> GPU offload wave.

## Goal

Bring every currently working graphical client path onto a modern OpenGL 4.3 rendering foundation while preserving visible behavior. Compatibility Profile is temporary migration scaffolding.

The phase ends only when `EliteGame` runs in **OpenGL 4.3 Core Profile** and no production render path depends on removed fixed-function/compatibility APIs.

This is an API/render-foundation migration, not a compute-offload project.

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
- legacy client arrays (`glVertexPointer`, `glColorPointer`, `glTexCoordPointer`, `glEnableClientState`, `glDisableClientState`).

`GL_TEXTURE_2D` remains valid as a texture target for modern texture storage/binding.

## Migration rules

1. Preserve behavior before redesign.
2. Do not mix compute/SSBO algorithm offload into this phase.
3. Prefer shared primitive/render foundations rather than one bespoke replacement per `glBegin`.
4. Matrices, coordinates and colors become explicit shader inputs.
5. Each pass owns the GL state it changes.
6. CPU interaction/picking/semantic data stays CPU-side.
7. Compatibility debt must shrink monotonically; migrated files receive static guards preventing regression.

## GL43-A — scaffold + machine inventory

### Implemented candidate

`tests/architecture_contracts/check_gl43_modernization_boundary.py` now:

- validates the temporary 4.3 Compatibility context request in `Window.cpp`;
- scans all production C/C++ under `src/`;
- strips comments before matching;
- reports compatibility token counts per file;
- tracks immediate mode, matrix stack, current-color state, fixed texture enable state and legacy client arrays;
- provides a monotonic `NO_IMMEDIATE_MODE_FILES` guard for files whose immediate submission has been retired.

The contract is intentionally incremental. Existing debt is reported rather than globally rejected until the corresponding wave migrates it; the protected set grows as files become clean.

### Confirmed manual debt so far

- `src/render/DebugGrid.cpp`;
- `src/game/system_map/DetailMapBackend.cpp`;
- `src/game/system_map/DetailMapGeometryPass.cpp`;
- `src/game/system_map/DetailMapPlanetPass.cpp`;
- `src/game/system_map/HubMapBackend.cpp`;
- `src/game/system_map/HubMapGeometryPass.cpp`;
- `src/game/system_map/HubMapPlanetPass.cpp`;
- `src/game/system_map/LocalMapAtmosphereRenderer.cpp`;
- `src/game/system_map/MapObjectOverlayRenderer.cpp`;
- transitional current-color dependency in `LocalMapPrimitiveRenderer.cpp`.

The contract's local output, not this handwritten list, is the complete inventory authority.

### Pending acceptance

GL43-A is accepted only after local:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

and full Compatibility-profile visual smoke.

## GL43-B — shared local/screen primitive foundation

### B1 candidate — immediate mode retired in LocalMapPrimitiveRenderer

`LocalMapPrimitiveRenderer` line/cross/circle drawing now uses an internal GLSL 4.30 Core program, VAO and streaming VBO.

The shader receives pixel coordinates and converts them to NDC using `GL_VIEWPORT`, so these primitives no longer depend on fixed-function projection/model-view matrices for their coordinate transform.

`glBegin/glEnd` and immediate `glVertex*` are removed from this file and the architecture contract forbids their return.

To keep B1's blast radius narrow, legacy callers still set fixed-function current color. B1 reads `GL_CURRENT_COLOR` once and forwards that value to the shader as `uColor`.

This is deliberately temporary.

### B2 next — explicit color API

After B1 local acceptance:

- add explicit color arguments to line/cross/circle;
- update every caller;
- remove all dependence on `GL_CURRENT_COLOR` from the primitive renderer;
- mark `LocalMapPrimitiveRenderer.cpp` fully compatibility-clean in the architecture contract.

Do not fold Detail/Hub geometry redesign into B2.

## GL43-C — Detail Map

Migrate:

- `DetailMapBackend` fixed-function projection/background;
- `DetailMapGeometryPass` orbit/grid/marker compatibility paths;
- `DetailMapPlanetPass` sphere-grid, filled disk and projected fallback geometry.

Preserve current map geometry and layering.

## GL43-D — Hub Map and local celestial presentation

Migrate:

- `HubMapBackend` projection/background;
- `HubMapGeometryPass` compatibility fallback;
- `HubMapPlanetPass` fallback body/local-circle immediate paths;
- `LocalMapAtmosphereRenderer` remaining fixed-function soft-band path.

Existing shader-driven planet/cloud/atmosphere paths remain intact unless a narrow Core state fix is required.

## GL43-E — overlays, debug and remaining debt

Migrate:

- `MapObjectOverlayRenderer`;
- `DebugGrid`;
- every remaining offender printed by the architecture scan.

Already modern VAO/VBO/shader paths are not rewritten merely for style.

## GL43-F — Core Profile cutover

Only after the scan is clean:

- regenerate/switch bundled GLAD to **OpenGL 4.3 Core**;
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
- no working feature was removed to satisfy Core;
- no CPU -> GPU algorithmic offload was required.

After this gate, `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md` become active implementation plans again.
