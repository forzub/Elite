# Elite client GPU offload plan

**Updated:** 2026-09-15  
**Baseline:** OpenGL 4.3 Core **accepted locally**  
**Audit:** `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`  
**Implementation status:** **ACTIVE**

## Sequencing rule

The Core prerequisite is closed. CPU -> GPU work is allowed only where the data flow and measured cost justify it.

Prefer the simplest GPU representation that removes repeated CPU work. Static topology + vertex shader/instancing is preferred over compute when topology is stable. Compute remains reserved for genuinely data-parallel variable-output work where results can stay GPU-resident.

Gameplay authority, navigation/planning, picking answers needed synchronously by CPU, replication and server-compatible simulation remain CPU-owned.

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

## P0.1 — System Map repeated primitives — ACTIVE

Current CPU-generated topology still includes:

- `addCircleXZ()`;
- `addCircleXY()`;
- `addOrbitCircle3D()`;
- `addBillboardBall()`;
- dynamic `flushLines()` / `flushSolids()` uploads of the generated vertices.

The simple circle paths currently evaluate `sin/cos` per segment every frame, transform every point on the CPU, append complete line vertices, then upload the full dynamic batch. The orbit and billboard paths do the same with additional transforms/triangle generation.

Preferred target:

```text
shared resident unit circle/ring/disc topology
    + compact per-instance center/radius/color/basis
    + grouped/instanced submission
```

Important constraint: do not trade CPU tessellation for a draw-call explosion. The existing line path batches many circles and arbitrary lines into one upload/draw, so a replacement must preserve batching economics. Where authored segment counts differ, group instances by topology/segment count rather than rebuilding geometry every frame.

### First slice

Migrate `addCircleXZ()` and `addCircleXY()` first. Keep arbitrary lines on the existing dynamic line path. Preserve current visual semantics for:

- planetary/orbit circles;
- asteroid belt rings;
- moon orbits;
- player marker circles;
- selected-body and selected-hub rings.

Add an architecture contract preventing the migrated circle path from returning to per-frame CPU trigonometric topology generation.

After that, evaluate `addOrbitCircle3D()` and `addBillboardBall()` separately.

## P1 — Scene visual traffic culling/LOD, profile-gated

This remains conditional. Before implementation, record representative target-scale values for:

- `prepareScene()` CPU time;
- visual ship count;
- prepared part count;
- proxy/full-model counts;
- `cpuVisualShipsMs`;
- instance upload bytes;
- draw-call count.

Only if target-scale cost is material should the design proceed toward GPU-resident static assembly metadata + compact per-ship dynamic state + compute visibility/LOD/compaction + indirect/instanced rendering. There must be no per-frame GPU -> CPU visibility readback.

The known station-adjacent freeze predates the GL4.3 migration and is explicitly deferred. It is **not** evidence by itself that SceneRenderer compute should be started.

## P1 adjunct — instance streaming

Persistent/ring buffers or SSBO instance streams remain valid transfer/driver optimizations if profiling shows repeated `glBufferData(GL_DYNAMIC_DRAW)` matrix uploads are material. This is not automatically a compute task.

## P2 — starfield

The starfield catalog rebuild is threshold-triggered rather than per-frame. Leave it deferred unless catalog scale or measured rebuild cost changes materially.

## Keep CPU by design

- `TrajectoryPredictor`, `LocalGuidancePlanner`, route/path/docking decisions;
- authoritative/shared ship physics and gameplay state transitions;
- `ClientWorldState` replication hydration and CPU prediction state;
- System/Detail/Hub semantic presentation and CPU interaction state;
- Hub exact picking unless profiling later proves it is an interaction hitch;
- `GalaxyDatabase` parsing/validation;
- offline/cached load-once asset work.

## Already GPU-driven

Do not duplicate existing GPU work:

- runtime procedural cloud texture generation;
- Milky Way backdrop;
- Hub planet surface;
- planet rings;
- shared `PlanetGlobeMeshRenderer` static sphere path;
- Hub assembly wire meshes;
- System Map textured planet/moon spheres.

## Measurement protocol

For each proposed wave record execution frequency, item/pixel/vertex count, CPU wall time, allocation/build cost, upload bytes, GPU time where relevant, synchronization/stalls and representative target-scale scenario.

The ownership rule remains strict: presentation-only GPU derivatives are fine; authoritative/gameplay answers that require synchronous readback stay CPU by default.