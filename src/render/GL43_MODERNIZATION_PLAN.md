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

`tests/architecture_contracts/check_gl43_modernization_boundary.py`:

- validates the temporary 4.3 Compatibility context request in `Window.cpp`;
- scans all production C/C++ under `src/`;
- strips comments before matching;
- reports compatibility token counts per file;
- tracks immediate mode, matrix stack, current-color state, fixed texture enable state and legacy client arrays;
- provides monotonic per-file guards as migration advances.

The contract is incremental: existing debt is reported rather than globally rejected until its wave migrates it.

## GL43-B — shared local/screen primitive foundation

### B1 — visually accepted

`LocalMapPrimitiveRenderer` line/cross/circle drawing now uses GLSL 4.30 Core, VAO/VBO and `glDrawArrays`.

The shader receives pixel coordinates and converts them to NDC using `GL_VIEWPORT`; `glBegin/glEnd` and immediate `glVertex*` are gone from this file and statically forbidden from returning.

Local visual smoke reported no visible change after this replacement, which is the required behavioral result.

### B2a — explicit color API + Detail geometry candidate

The primitive API now exposes explicit `glm::vec4` color overloads for line/cross/circle.

`DetailMapGeometryPass` has already moved fully onto them. Its legacy orbit path was also folded onto the shared primitive renderer without changing the orbit algorithm:

- no fixed-function `glColor*`;
- no `GL_CURRENT_COLOR`;
- no `glBegin/glEnd`;
- no immediate `glVertex*`;
- same 192-segment orbit policy at current call sites;
- same hidden-side alpha attenuation (`0.16x`).

The architecture contract now treats `DetailMapGeometryPass.cpp` as fully compatibility-clean.

Temporary no-color primitive overloads remain for unmigrated Hub/planet callers and still use `GL_CURRENT_COLOR`. Therefore `LocalMapPrimitiveRenderer.cpp` itself is not yet fully Core-clean.

### B2b — close the primitive seam

After B2a local validation:

- migrate all remaining local-map primitive callers to explicit color;
- delete the no-color overloads;
- delete `compatibilityCurrentColor()` and the last `GL_CURRENT_COLOR` use from the primitive renderer;
- upgrade its static guard from no-immediate-mode to no-compatibility-at-all;
- smoke Detail/Hub again.

## GL43-C — Detail Map

Migrate remaining Detail compatibility debt:

- `DetailMapBackend` fixed-function projection/background;
- `DetailMapPlanetPass` sphere-grid, filled disk and projected fallback geometry.

`DetailMapGeometryPass` is already compatibility-clean as of B2a and should not be reopened except for a regression.

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

## Current validation

For B2a:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Primary visual target is Detail Map geometry: volume edges, hub/player orbits, hidden-half attenuation, small-body circles/crosses, colors and orientation.

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
