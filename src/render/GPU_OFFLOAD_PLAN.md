# Elite client GPU offload plan

**Updated:** 2026-09-15  
**Baseline:** OpenGL 4.3 Core **accepted locally**  
**Audit:** `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`  
**Implementation status:** **ACTIVE**

## Sequencing rule

The Core prerequisite is closed. CPU -> GPU work is now allowed, but only where the data flow and measured cost justify it.

Prefer the simplest GPU representation that removes repeated CPU work. Static topology + vertex shader/instancing is preferred over compute when topology is stable. Compute remains reserved for genuinely data-parallel variable-output work where results can stay GPU-resident.

Gameplay authority, navigation/planning, picking answers needed synchronously by CPU, replication and server-compatible simulation remain CPU-owned.

## P0 — System Map textured body geometry

### Previous hot path

`SystemMapRenderer::addTexturedSystemBodySphere()` rebuilt sphere positions and UV payload every System Map frame. A large planet produced:

```text
64 x 128 cells
49,152 vertices/body/frame
~1.69 MiB vertex upload/body/frame
~101 MiB/s/body at 60 Hz
```

This also paid repeated trigonometry, basis transforms, vector writes and dynamic-buffer submission.

### Candidate implemented

Commit: `e9a9cfdef1331f67259d019fd9024054ce30779e`.

The active candidate uses two one-time resident indexed unit spheres:

- 24 x 48 low-resolution mesh;
- 64 x 128 high-resolution mesh.

Per-body dynamic data is reduced to texture, center, radius, prime/north/east basis, color and LOD choice. `textureLongitudeOffsetDeg` and `rotationPhaseRad` are folded into the per-body basis; the vertex shader transforms the resident unit sphere and drawing uses `glDrawElements`.

The per-frame textured-sphere path no longer performs CPU sphere tessellation and no longer performs a full-sphere dynamic vertex upload.

Automated P0 gates PASS:

- `check_system_map_static_sphere.py`;
- GL4.3 Core boundary;
- Windows/MSYS2 MinGW64 syntax compilation of `SystemMapRenderer.cpp`.

**Status:** candidate pending focused local System Map visual acceptance.

## Next after P0 — System Map repeated primitives

Profile and then replace repeated CPU-generated topology such as:

- orbit circles;
- marker/selection rings;
- billboard balls/halos;
- other map circles built with per-frame `sin/cos` loops.

Preferred target:

```text
shared unit circle/ring/disc meshes
    + compact per-instance center/radius/color/basis
    + vertex shader / instancing
```

Do not use compute for geometry that can remain a static parameterized primitive.

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
- Hub assembly wire meshes.

## Measurement protocol

For each proposed wave record execution frequency, item/pixel/vertex count, CPU wall time, allocation/build cost, upload bytes, GPU time where relevant, synchronization/stalls and representative target-scale scenario.

The ownership rule remains strict: presentation-only GPU derivatives are fine; authoritative/gameplay answers that require synchronous readback stay CPU by default.
