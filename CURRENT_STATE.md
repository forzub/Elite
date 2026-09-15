# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core **accepted locally; final clean rebuild pending after tail cleanup**  
**GPU-P0:** System Map static textured spheres **accepted locally**  
**GPU-P0.1:** System Map repeated planar circles **contracts PASS; build/runtime acceptance pending**

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared deterministic/runtime geometry seams. Dual-source model ingress remains accepted and unchanged:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration remains queued behind the current renderer/navigation-performance work.

## OpenGL 4.3 Core — accepted baseline, tail cleanup closed in source

The local developer runtime smoke passed on 2026-09-15 for the Core migration baseline. The station-adjacent freezes predate that migration and remain explicitly deferred from renderer modernization.

Subsequent clean rebuilds exposed compatibility tails that the first static inventory had missed. They have now been removed in source:

- `UICameraView::renderToTexture()` no longer uses `glPushAttrib/glPopAttrib` or compatibility attrib-bit tokens; viewport and software matrix-mode state are restored explicitly;
- `HubBackdropCloudRenderer` no longer uses fixed-function texture environment (`glGetTexEnviv`, `glTexEnvi`, `GL_TEXTURE_ENV`, `GL_MODULATE`). `CoreGlLegacyBridge` already implements the required MODULATE semantics explicitly in GLSL as texture * vertex color;
- `DetailMapPlanetPass` no longer queries/disables/restores `GL_ALPHA_TEST`. The affected shape-model path is intentionally opaque, so Core blending state is sufficient;
- `GlRuntimeCapabilities` now requires and reports `GL_CONTEXT_CORE_PROFILE_BIT` / `coreProfile`, not the obsolete compatibility profile.

The GL43 architecture guard is now materially stronger:

- fixed-function matrix/color/texture/client-array/attrib-stack APIs remain forbidden;
- texture-environment and alpha-test symbols are explicitly forbidden;
- every raw production `glXxx()` call and `GL_*` token is mechanically checked against the bundled OpenGL 4.3 Core GLAD header (with the local helper `glString` explicitly excluded);
- the runtime capability gate is asserted to require Core profile.

The one-shot CI migration passed all architecture contracts and `git diff --check`. A fresh local MinGW `EliteGame` rebuild is still required before declaring the cleanup fully accepted.

## GPU-P0 — System Map static textured spheres — ACCEPTED

The accepted path keeps resident indexed 24x48 and 64x128 unit spheres. Per-body transform is folded into `bodyMvp`, the runtime-proven `map_body_preview` shader ABI remains intact, and the old per-frame latitude/longitude tessellation plus full-sphere dynamic upload are gone.

The first candidate that introduced a new shader ABI was visually rejected because planets/moons disappeared. The corrected path retained the GPU optimization while restoring the proven shader contract. Static-sphere/Core contracts and MinGW syntax compilation passed; local visual smoke confirmed planets and moons returned.

## GPU-P0.1 — repeated planar System Map circles — CANDIDATE

The current branch contains a resident/instanced circle path through `SystemMapGpuCircleBatch`.

### Migrated scene primitives

- primary planet orbit circles;
- asteroid-belt orbit and three belt rings;
- moon orbit circles;
- player ring;
- selected-body XZ/XY rings;
- selected-hub XZ/XY rings.

### Data path

For each authored segment count, a unit circle is generated lazily once and uploaded with `GL_STATIC_DRAW`. Per frame, migrated circles submit only center/radius/color plus XY/XZ plane selection. GLSL 4.30 expands the unit circle and `glDrawArraysInstanced(GL_LINE_LOOP, ...)` submits compatible groups.

This removes per-frame `sin/cos` and complete transformed-circle vertex uploads from the migrated `SystemMapSceneRenderer` paths without creating one draw call per circle.

`tests/architecture_contracts/check_system_map_gpu_circles.py` protects this boundary.

### Local evidence so far

The user ran all three contracts locally and they reported PASS. The first clean build exposed `UICameraView` compatibility attrib-stack calls; the next build exposed fixed-function texture environment in `HubBackdropCloudRenderer` and `GL_ALPHA_TEST` in `DetailMapPlanetPass`. All of those tails plus the stale compatibility-profile runtime gate are now removed in source and covered by the stronger Core contract.

### Acceptance still required

Pull the latest branch and run:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
python tests/architecture_contracts/check_system_map_static_sphere.py
python tests/architecture_contracts/check_system_map_gpu_circles.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Visual parity must cover planet/moon orbits, asteroid-belt rings, player ring, selected-body rings and selected-hub rings.

## Explicit remaining renderer debt

The old generic `addCircleXZ()` / `addCircleXY()` helpers remain for unmigrated callers, but `SystemMapSceneRenderer` no longer uses them for the migrated planar rings.

Still CPU-generated:

- arbitrary line geometry / `flushLines()`;
- non-planar `addOrbitCircle3D()` helper;
- billboard/proxy body markers, halos and `addBillboardBall()`;
- remaining dynamic solid primitives.

These are not automatically the next task. Dormant helpers should not be migrated merely for code purity, and small marker paths should be profile-gated.

## Next track after GPU-P0.1 acceptance — NAV-PERF-0

The next priority is the route/trajectory calculation that can stall the machine. Start with instrumentation, not a compute-shader port.

Measure at least:

- total planner wall time;
- number of predictor calls;
- integration-step count;
- gravity-field/body evaluations;
- shooting-correction iterations;
- safety trajectory segments;
- obstacle/restricted-volume/traffic checks;
- detour/emergency candidate counts.

The current single-trajectory integrator is sequential and double-precision-heavy, so it remains the CPU reference initially. If profiling later shows large independent candidate sets or safety-test matrices dominate, those batches are legitimate GPU-compute candidates while CPU planning/authority stays canonical.

`SceneRenderer` traffic visibility/LOD compute remains profile-gated and is no longer ahead of NAV-PERF-0 in priority.
