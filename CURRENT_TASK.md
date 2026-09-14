# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Track:** Client GPU modernization
**Stage:** establish OpenGL 4.3+ compute-capable baseline, then migrate measured client hot paths

## Accepted prerequisite

Dual-source runtime model ingress is locally accepted on MinGW64: architecture contracts pass, `EliteRuntimeModelAssets`, `EliteServer` and `EliteGame` all build/link. The first read-only consumer migration remains queued, but GPU foundation work is now the active track by explicit decision.

## Immediate goal — GL43 foundation

- request OpenGL **4.3 Compatibility Profile** from GLFW;
- regenerate bundled GLAD as `gl:compatibility=4.3` with no optional extensions;
- centralize runtime capability validation in `render::gpu::GlRuntimeCapabilities`;
- require compute-shader and SSBO entry points before client renderer startup;
- log actual OpenGL/GLSL/vendor/renderer and compute resource limits;
- keep existing GLSL/render behavior unchanged during the baseline jump;
- disable general world-signal labels without disabling Hub-map/close-navigation labels;
- local MinGW gate: build and launch `EliteGame`, confirm the `[OpenGL]` line reports >=4.3, `compute=1`, `ssbo=1`, and visually smoke the ordinary scene + Hub map.

Compatibility profile is transitional. Do **not** switch to Core Profile until legacy fixed-function call sites are removed.

## GPU offload waves after GL43 acceptance

P0: move `PlanetaryWeatherMapGenerator` and `CloudAppearanceTextureGenerator` to compute while retaining CPU reference/parity tests. P1: move starfield derived transforms/filtering/draw preparation away from repeated CPU VBO rebuilds; then profile instance/frustum workloads for SSBO culling/indirect draw. Other algorithms move only when measurements show enough work and no synchronous readback dependency.

Authoritative simulation, gameplay decisions and CPU-consumed navigation results remain CPU by default. GPU visualization derivatives are allowed as separate presentation products.

Detailed policy: `src/render/GPU_OFFLOAD_PLAN.md`.

## Deferred but preserved runtime-asset task

Migrate one read-only runtime consumer from direct `AssemblyMeshLibrary` access to `RuntimeModelAssetLibrary::get`; verify legacy parity first, then switch only that object type to `.elmodel`.

## State discipline

Every accepted GPU wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/game/GAME_RUNTIME_DECOMPOSITION.md` and `src/render/GPU_OFFLOAD_PLAN.md` in the same accepted commit.
