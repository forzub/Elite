# Elite client GPU offload plan

**Baseline decision:** OpenGL 4.3+ is the minimum graphical-client API.  
**Audit status:** full client CPU -> GPU audit completed on 2026-09-14.  
**Implementation status:** **BLOCKED until OpenGL 4.3 Core Profile modernization is accepted.**

Detailed evidence: `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`.  
Active prerequisite plan: `src/render/GL43_MODERNIZATION_PLAN.md`.

## Sequencing rule

Do not implement CPU -> GPU algorithmic offload on top of the temporary Compatibility renderer.

First:

1. accept the 4.3 Compatibility scaffold;
2. remove all fixed-function/compatibility-only rendering from currently working client paths;
3. switch GLAD/context to OpenGL 4.3 Core Profile;
4. build, launch and visually accept the complete client;
5. capture fresh performance baselines;
6. resume this plan.

Modernizing `glBegin` paths into VAO/VBO/shader submission is part of the GL API migration, not an offload wave, even if it improves CPU time incidentally.

## Preserved post-Core priorities

### P0 — System Map textured body geometry

`SystemMapRenderer::addTexturedSystemBodySphere()` currently rebuilds textured sphere vertices on CPU every System Map frame. A large body generates 49,152 vertices/body/frame and re-uploads the dynamic batch.

After Core acceptance, measure the path and replace it with a shared indexed unit sphere plus per-body parameters and vertex-shader transforms if the measured cost confirms the audit. This is not a compute workload.

### P1 — Scene visual traffic culling/LOD, profile-gated

Only after Core acceptance and target-count measurement, evaluate GPU-resident static assembly metadata plus compact per-ship dynamic input, compute visibility/LOD/compaction and direct instanced/indirect rendering. No GPU -> CPU visibility readback.

If target-scale CPU cost is already small, keep it CPU.

### P1/P2 — map primitive geometry

After Core migration, profile remaining repeated orbit/circle/halo/billboard generation. Prefer static parameterized primitives and instancing over compute.

Some of this work may disappear naturally during fixed-function removal; do not duplicate it later.

### P1 adjunct — instance streaming

Evaluate persistent/ring buffers or SSBO instance streams only after measurement. This is transfer/driver optimization, not automatically a compute project.

### P2 — starfield

Current catalog rebuild is threshold-triggered, not per-frame. Keep deferred unless catalog scale or measured rebuild time changes materially.

## Keep CPU by design

- `TrajectoryPredictor`, `LocalGuidancePlanner`, route/path/docking decisions;
- authoritative/shared ship physics and gameplay state transitions;
- `ClientWorldState` replication hydration and CPU prediction state;
- System/Detail/Hub semantic presentation, string/UI and CPU interaction state;
- Hub exact picking unless a future CPU profile proves it is an interaction hitch;
- `GalaxyDatabase` parsing/validation;
- offline/cached load-once asset work.

## Already GPU-driven

Do not duplicate existing GPU work:

- runtime procedural cloud texture generation;
- Milky Way backdrop;
- Hub planet surface;
- planet rings;
- shared `PlanetGlobeMeshRenderer` static sphere path;
- Hub assembly wire meshes.

## Post-Core measurement protocol

For each proposed offload wave record execution frequency, item/pixel/vertex count, CPU wall time, allocation/build cost, upload bytes, GPU time where relevant, synchronization/stalls and target-scale scenario.

The ownership rule remains strict: presentation-only GPU derivatives are fine; authoritative/gameplay answers that require synchronous readback stay CPU by default.