# Elite client GPU offload plan

**Updated:** 2026-09-15  
**Baseline:** OpenGL 4.3 Core **accepted locally**  
**Audit:** `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`  
**Implementation status:** **ACTIVE**

## Sequencing rule

The Core prerequisite is closed. CPU -> GPU work is allowed only where the data flow and measured cost justify it.

Prefer the simplest GPU representation that removes repeated CPU work. Static topology + vertex shader/instancing is preferred over compute when topology is stable. Compute remains reserved for genuinely data-parallel variable-output work where results can stay GPU-resident or can be reduced to a compact result.

Gameplay authority, route/docking decisions, replication and server-compatible simulation remain CPU-owned. This does not prohibit GPU derivative evaluation of large independent candidate sets when the CPU remains the canonical decision maker.

## P0 — System Map textured body geometry — ACCEPTED

### Previous hot path

`SystemMapRenderer::addTexturedSystemBodySphere()` rebuilt sphere positions and UV payload every System Map frame. A large planet produced:

```text
64 x 128 cells
49,152 vertices/body/frame
~1.69 MiB vertex upload/body/frame
~101 MiB/s/body at 60 Hz
```

This also paid repeated trigonometry, basis transforms, vector writes and dynamic-buffer submission.

### Accepted path

Two one-time resident indexed unit spheres are used:

- 24 x 48 low-resolution mesh;
- 64 x 128 high-resolution mesh.

Per-frame body data is reduced to texture, center, radius, prime/north/east basis, color and LOD choice. `textureLongitudeOffsetDeg` and `rotationPhaseRad` are folded into the body basis. The body transform is folded into `bodyMvp = frameMvp * bodyModel` and the runtime-proven `map_body_preview` shader ABI remains unchanged.

The first candidate with a new per-body shader ABI was visually rejected because planets/moons disappeared. The corrected path kept the static mesh optimization, restored the proven shader ABI, and passed Core/static-sphere contracts plus MinGW syntax compilation. Local visual smoke then confirmed that planets and moons returned.

**Status:** accepted locally on 2026-09-15.

## P0.1 — System Map repeated planar circles — ACCEPTANCE CANDIDATE

The first repeated-primitive slice is implemented.

Migrated `SystemMapSceneRenderer` consumers:

- planet primary orbit circles;
- asteroid-belt primary orbit and three belt rings;
- moon orbit circles;
- player ring;
- selected-body XZ/XY rings;
- selected-hub XZ/XY rings.

`SystemMapGpuCircleBatch` caches resident unit circles by authored segment count and uploads topology once with `GL_STATIC_DRAW`. Per-frame payload is center/radius/color plus XY/XZ plane selection; compatible groups use `glDrawArraysInstanced(GL_LINE_LOOP, ...)`.

`tests/architecture_contracts/check_system_map_gpu_circles.py` prevents the scene from silently returning to the old CPU circle API.

**Status:** implementation complete; contracts PASS locally; final clean build/runtime/visual acceptance after Core-tail cleanup is still pending.

Do not migrate dormant circle helpers, billboard/proxy markers or arbitrary line/solid paths merely for code purity. Those remain profile-gated.

## Navigation freeze — explicit Ruckig experiment before GPU work

The route/trajectory calculation can produce a user-visible machine stall. The original next step was NAV-PERF-0 instrumentation. The user explicitly chose to try the MIT-licensed Ruckig Community Edition first as a replacement candidate for the expensive repeated state-to-state shooting solve.

### NAV-RUCKIG-0 — active isolated spike

Pinned upstream:

```text
Ruckig v0.19.4
a8db97a4e9c55e5160a3855f739fa3b270df8e4c
```

The spike is deliberately outside the production build. `RuckigTrajectorySolver` hides Ruckig behind an Elite-only public seam; only the adapter implementation is C++20. `tests/navigation_ruckig` fetches/builds the exact commit with cloud client, examples, upstream tests, benchmark, Python module and shared-library build disabled.

The adapter solves in an accelerating co-moving terminal frame rather than feeding orbital-scale world coordinates directly into Ruckig. It then reconstructs world states, resamples `GravityFieldSystem`, and validates Elite scalar proper-acceleration/proper-jerk limits before returning the normal `TrajectoryPredictionResult` form.

The spike measures a 500-solve wall-time benchmark but has no hard performance threshold yet. First acceptance requires endpoint/limit tests, orbital-scale numerical stability, Earth-like gravity behavior and infeasible-horizon rejection on the developer MinGW toolchain.

### Why this still follows the GPU-offload policy

The current single-trajectory integrator is sequential in time, double-precision-heavy and feeds CPU planner decisions. Replacing repeated shooting iterations with an analytic/online trajectory generator is a more appropriate first experiment than blindly moving the same sequential algorithm to a compute shader.

If Ruckig passes, the next wave is a production A/B path with the old `TrajectoryPredictor` retained as deterministic fallback/reference and `TrajectorySafetyEvaluator` unchanged. Timing and fallback counters then establish whether the route freeze is solved or whether safety/broad-phase work still dominates.

If Ruckig fails or is not materially faster, NAV-PERF instrumentation remains the fallback next step.

### Legitimate future GPU shape

GPU compute remains attractive if the optimized planner later needs many independent candidates or many independent candidate-segment/hazard checks:

```text
CPU authoritative planner
    -> generate N independent candidate control programs
    -> GPU batched trajectory/safety evaluation
    -> compact scores/conflicts
    -> CPU selects/validates the winning candidate
```

This is a derivative/batched accelerator, not a replacement for navigation authority.

## P1 — Scene visual traffic culling/LOD, profile-gated

This remains conditional and behind the route-performance investigation. Before implementation, record representative target-scale values for `prepareScene()` CPU time, visual ship count, prepared part count, proxy/full-model counts, `cpuVisualShipsMs`, instance upload bytes and draw-call count.

Only if target-scale cost is material should the design proceed toward GPU-resident static assembly metadata + compact per-ship dynamic state + compute visibility/LOD/compaction + indirect/instanced rendering. There must be no per-frame GPU -> CPU visibility readback.

The known station-adjacent freeze predates the GL4.3 migration and is not evidence by itself that SceneRenderer compute should be started.

## P1 adjunct — instance streaming

Persistent/ring buffers or SSBO instance streams remain valid transfer/driver optimizations if profiling shows repeated `glBufferData(GL_DYNAMIC_DRAW)` matrix uploads are material. This is not automatically a compute task.

## P2 — starfield

The starfield catalog rebuild is threshold-triggered rather than per-frame. Leave it deferred unless catalog scale or measured rebuild cost changes materially.

## Keep CPU authoritative/reference by design

- route/path/docking decisions and final navigation authority;
- authoritative/shared ship physics and gameplay state transitions;
- `ClientWorldState` replication hydration and CPU prediction state;
- System/Detail/Hub semantic presentation and CPU interaction state;
- Hub exact picking unless profiling later proves it is an interaction hitch;
- `GalaxyDatabase` parsing/validation;
- offline/cached load-once asset work.

The existing `TrajectoryPredictor` remains the CPU reference/fallback while Ruckig is evaluated. Batched GPU candidate/safety evaluation is allowed only after measurements justify it and without synchronous fine-grained readback.

## Already GPU-driven

Do not duplicate existing GPU work:

- runtime procedural cloud texture generation;
- Milky Way backdrop;
- Hub planet surface;
- planet rings;
- shared `PlanetGlobeMeshRenderer` static sphere path;
- Hub assembly wire meshes;
- System Map textured planet/moon spheres;
- current candidate: repeated planar System Map orbit/selection/player/hub rings.

## Measurement protocol

For each proposed wave record execution frequency, item/pixel/vertex count, CPU wall time, allocation/build cost, upload bytes, GPU time where relevant, synchronization/stalls and representative target-scale scenario.

The ownership rule remains strict: presentation-only GPU derivatives are fine; authoritative/gameplay answers stay CPU by default, while coarse batched accelerators may return compact evaluation results when explicitly designed to avoid blocking frame-by-frame GPU readback.
