# Elite OpenGL 4.3 modernization plan

**Date:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Decision:** finish the renderer migration to OpenGL 4.3 Core Profile before any CPU -> GPU offload wave.

## Goal

Bring every currently working graphical client path onto a modern OpenGL 4.3 rendering foundation while preserving visible behavior. The compatibility context is only a temporary migration scaffold.

The phase ends only when `EliteGame` runs in **OpenGL 4.3 Core Profile** and no production render path depends on removed fixed-function/compatibility APIs.

This phase is an API/render-foundation migration, not a performance-offload project. Do not mix in compute culling, starfield compute, System Map sphere offload or other algorithmic GPU migrations until Core Profile acceptance is complete.

## Why this order

A large part of the client is already shader/VBO/VAO based, but several live map/overlay/fallback paths still depend on compatibility-only state. Optimizing those paths before eliminating the legacy API would create two moving targets and force later rework.

Modernizing first gives one stable GPU contract for all later work:

```text
OpenGL 4.3 Compatibility baseline
        -> remove fixed-function dependencies
        -> OpenGL 4.3 Core Profile accepted
        -> capture performance baselines
        -> CPU/GPU offload only where measurements justify it
```

## Confirmed legacy areas

This list is confirmed from the current branch but is not yet the complete repository-wide inventory.

- `src/game/system_map/LocalMapPrimitiveRenderer.cpp` — immediate-mode `glBegin/glEnd`, `glVertex2d`.
- `src/game/system_map/DetailMapGeometryPass.cpp` — fixed-function color state, `GL_CURRENT_COLOR`, immediate-mode orbit/line rendering through local-map primitives.
- `src/game/system_map/DetailMapPlanetPass.cpp` — immediate-mode sphere-grid/disk/fallback projected geometry and fixed-function color state.
- `src/game/system_map/HubMapBackend.cpp` — fixed-function projection/model-view matrix stack, `glOrtho`, immediate-mode background quad.
- `src/game/system_map/HubMapGeometryPass.cpp` — compatibility fallback primitives still use fixed-function color/immediate mode.
- `src/game/system_map/MapObjectOverlayRenderer.cpp` — fixed-function 2D projection/model-view setup and immediate-mode overlay glyphs.

Other render/HUD/debug/cockpit paths must be audited before declaring the inventory complete. Existing VAO/VBO/shader code such as `RenderCockpitBitmapPass` is already Core-compatible in principle and should not be rewritten merely for stylistic uniformity.

## Forbidden compatibility surface at final gate

Production client code must not rely on these classes of API after migration:

- `glBegin`, `glEnd`;
- `glVertex*`, `glColor*`, `glTexCoord*`, `glNormal*` immediate-mode submission;
- `glMatrixMode`, `glPushMatrix`, `glPopMatrix`, `glLoadIdentity`, `glLoadMatrix*`, `glMultMatrix*`, `glOrtho` fixed-function matrix stack;
- `GL_CURRENT_COLOR`, `GL_MODELVIEW`, `GL_PROJECTION`, `GL_MATRIX_MODE` fixed-function state;
- `glEnable/glDisable(GL_TEXTURE_2D)` fixed-function texture enable state;
- legacy client arrays such as `glVertexPointer`, `glColorPointer`, `glTexCoordPointer`, `glEnableClientState` if any remain.

Normal modern uses of `GL_TEXTURE_2D` as a texture target for `glBindTexture`, storage and sampling remain valid.

## Migration rules

1. **Preserve behavior first.** Each wave must render the same scene/map/UI before any unrelated visual redesign.
2. **No algorithmic offload during this phase.** Replacing immediate mode with batched VBO/VAO/shaders is allowed; moving CPU algorithms to compute is deferred.
3. **Prefer shared primitives.** Do not replace every `glBegin` with a bespoke mini-renderer. Reuse or establish narrow modern primitive submission paths for screen-space lines/triangles/circles and local-map geometry.
4. **Explicit matrices and color.** Projection/view/model matrices and color become shader uniforms/vertex attributes; no hidden global matrix/color state.
5. **Explicit GL state ownership.** Each pass owns and restores only the states it actually changes. Avoid dependence on compatibility defaults.
6. **Keep CPU interaction data on CPU.** Picking, label anchors, semantic overlays and navigation state remain CPU-side even when their drawing becomes shader based.

## Work waves

### GL43-A — runtime Compatibility acceptance and inventory

- build and launch `EliteGame` with the existing 4.3 Compatibility request;
- verify `[OpenGL] >= 4.3`, `compute=1`, `ssbo=1`;
- smoke ordinary flight, cockpit/rear view, Galaxy/System/Detail/Hub maps and close-navigation HUD;
- perform a repository-wide scan for compatibility-only calls and record every production call site;
- add an architecture/static contract that can eventually fail on forbidden calls.

This gate proves the migration scaffold before changing live render paths.

### GL43-B — shared local/screen primitive foundation

Migrate the lowest shared primitive seams first, especially `LocalMapPrimitiveRenderer`, to VAO/VBO/shader submission. Provide explicit color and projection inputs instead of reading `GL_CURRENT_COLOR` or matrix-stack state.

The goal is to let several maps lose compatibility dependencies through one implementation change rather than by duplicating replacements.

### GL43-C — Detail Map

Migrate:

- `DetailMapGeometryPass` orbit/grid/marker compatibility calls;
- `DetailMapPlanetPass` grid, filled-disk and projected fallback geometry;
- any Detail-specific fixed-function state setup.

Do not redesign the map or change its geometry policy during this wave.

### GL43-D — Hub Map

Migrate:

- `HubMapBackend` fixed-function 2D background/projection setup;
- `HubMapGeometryPass` remaining compatibility fallback primitives;
- any other Hub fixed-function call sites found by the full inventory.

Existing GPU geometry, ring, cloud and planet-surface passes stay intact unless Core compatibility requires a narrow state fix.

### GL43-E — object overlays / HUD / remaining client render code

Migrate `MapObjectOverlayRenderer` and every remaining production fixed-function call site in render, scene, HUD, debug and cockpit code. Shader-based code that already satisfies Core 4.3 is left alone.

### GL43-F — Core Profile cutover

Only after the static scan is clean:

- regenerate/switch bundled GLAD from 4.3 Compatibility to **OpenGL 4.3 Core**;
- request `GLFW_OPENGL_CORE_PROFILE`;
- build `EliteGame`;
- launch and verify the context is Core 4.3+;
- run full visual smoke for all currently working render modes;
- keep the architecture contract permanently preventing reintroduction of fixed-function APIs.

## Final acceptance

The GL43 modernization phase is accepted only when all of the following are true:

- `EliteGame` builds on MinGW64;
- runtime creates an OpenGL 4.3+ **Core Profile** context;
- startup capability logging remains valid;
- repository architecture/static scan reports no forbidden compatibility-only production calls;
- ordinary flight, cockpit/rear view, Galaxy Map, System Map, Detail Map and Hub Map render correctly;
- close-navigation HUD/labels work as before;
- no current working feature was intentionally dropped to make Core Profile pass;
- no CPU->GPU algorithmic offload is required for acceptance.

After this gate, `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md` become active implementation plans again.