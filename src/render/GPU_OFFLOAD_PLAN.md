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

The first repeated-primitive slice is now implemented.

Migrated `SystemMapSceneRenderer` consumers:

- planet primary orbit circles;
- asteroid-belt primary orbit and three belt rings;
- moon orbit circles;
- player ring;
- selected-body XZ/XY rings;
- selected-hub XZ/XY rings.

### Implementation

`SystemMapGpuCircleBatch` caches resident unit circles by authored segment count. Topology is generated once and uploaded using `GL_STATIC_DRAW`.

Per-frame instance payload is compact:

```text
center.xyz + radius
color.rgba
plane = XZ / XY
```

The GLSL 4.30 vertex shader expands the resident circle. Compatible sequential groups use `glDrawArraysInstanced(GL_LINE_LOOP, ...)` rather than one draw per circle.

This removes repeated per-frame circle `sin/cos`, transformed-circle vertex construction and full-circle dynamic uploads for the migrated scene paths.

`tests/architecture_contracts/check_system_map_gpu_circles.py` prevents the scene from silently returning to the old CPU circle API.

**Status:** implementation complete; local build/runtime/visual acceptance pending.

### Deliberately outside this slice

- arbitrary dynamic lines / `flushLines()`;
- `addOrbitCircle3D()`;
- billboard/proxy body markers and halos;
- `addBillboardBall()` and remaining dynamic solids.

Do not migrate dormant helpers merely for purity. Billboard/proxy marker work is now profile-gated rather than automatically sequenced ahead of navigation performance.

## NAV-PERF-0 — route / trajectory freeze — NEXT AFTER P0.1 ACCEPTANCE

The route/trajectory calculation can produce a user-visible machine stall, so it takes priority immediately after the current circle candidate is accepted.

The first wave is instrumentation only. Capture at least:

- total planner wall time;
- `TrajectoryPredictor` call count;
- integration-step count;
- gravity sample/body-evaluation count;
- `predictLeg()` shooting-correction iterations;
- safety trajectory-segment count;
- obstacle/restricted-volume/scheduled-traffic checks;
- detour/emergency candidate count.

### Working hypothesis

A direct compute-shader port of one current trajectory is not the preferred first move. The integration is sequential in time, uses double-precision state and feeds CPU planner decisions. Algorithmic repetition, fixed integration granularity, safety broad-phase cost and synchronous main-thread execution must be measured first.

Potential CPU-side wins include adaptive integration step, fewer/replaced shooting iterations, spatial broad phase, caching and asynchronous planning with an explicit time/work budget.

### Legitimate future GPU shape

GPU compute becomes attractive if the optimized planner needs to evaluate many independent candidates or many independent candidate-segment/hazard pairs:

```text
CPU authoritative planner
    -> generate N independent candidate control programs
    -> GPU batched trajectory/safety evaluation
    -> compact scores/conflicts
    -> CPU selects/validates the winning candidate
```

The CPU `TrajectoryPredictor` remains the reference implementation. GPU evaluation, if justified by measurements, is a derivative/batched accelerator rather than a replacement for navigation authority.

## P1 — Scene visual traffic culling/LOD, profile-gated

This remains conditional and is now behind NAV-PERF-0. Before implementation, record representative target-scale values for:

- `prepareScene()` CPU time;
- visual ship count;
- prepared part count;
- proxy/full-model counts;
- `cpuVisualShipsMs`;
- instance upload bytes;
- draw-call count.

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

`TrajectoryPredictor` is the CPU reference. Batched GPU candidate/safety evaluation is explicitly allowed only after NAV-PERF profiling demonstrates it is worthwhile and without synchronous fine-grained readback.

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
