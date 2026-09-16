# Elite client CPU -> GPU offload audit

**Date:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Audited HEAD:** `a1483ea3542c215239ddc26279951bbc4064f3b2`  
**Scope:** client-only render, scene, galaxy, game/client, system-map/navigation presentation, shared physics used by prediction, and adjacent celestial/HUD code.

## Goal

Find CPU workloads whose cost is both large enough and parallel/render-derived enough to justify GPU execution. This is not a mandate to move CPU code to compute. Server-authoritative simulation, gameplay decisions, branch-heavy planners, string/UI work, one-time asset processing and small calculations remain CPU unless measurements prove a real benefit.

The preferred outcome is often **not compute**. If a CPU path repeatedly rebuilds geometry that is topologically static, a shared static GPU mesh plus vertex shader/instance data is simpler, faster and lower risk than a compute shader.

## Audit rules

For every candidate we consider:

- execution cadence;
- item/pixel/vertex count;
- CPU math/allocation cost;
- CPU -> GPU upload volume;
- whether inputs can stay GPU-resident;
- whether output is consumed only by rendering;
- whether GPU -> CPU readback would be required;
- expected gain, regression risk and migration priority.

A candidate is rejected when GPU synchronization/readback or authority semantics erase the expected gain.

## Executive result

The previous provisional priority list was materially wrong in three places:

1. runtime procedural cloud texture generation is already shader/FBO driven in `ProceduralCloudLayer`; the old CPU weather/cloud generators are not proven runtime hot paths;
2. `GalaxyStarfieldRenderer` does not rebuild the catalog every frame; the active draw path uses a prepared VBO and rebuilds only after a significant observer-position change;
3. `PlanetWireRenderer` builds its sphere/grid geometry at initialization and uploads static buffers.

The strongest concrete live CPU -> GPU candidate found by this audit is instead **System Map textured celestial-body tessellation**.

## Priority matrix

| Priority | Workload | Current CPU work / cadence | GPU fit | Transfer / synchronization | Expected gain | Risk / decision |
| --- | --- | --- | --- | --- | --- | --- |
| **P0** | System Map textured celestial-body sphere generation (`SystemMapRenderer::addTexturedSystemBodySphere`) | Every System Map render frame. Nested latitude/longitude loops generate six vertices per cell, repeatedly evaluate trigonometry/body basis and append dynamic vertex arrays. A large body uses 64 x 128 x 6 = **49,152 vertices per body per frame**. | **Excellent, but use a static unit sphere + vertex shader, not compute.** Topology and UVs are static; only center/radius/orientation/phase/texture differ. | Current path clears CPU vectors and uploads the full batch with `glBufferData(GL_DYNAMIC_DRAW)` each frame. At 36 B per `TexturedVertex`, 49,152 vertices are about 1.69 MiB/body/frame (~101 MiB/s/body at 60 Hz) before driver overhead. Replacement needs only a one-time static sphere plus small per-body uniforms/instance data; no readback. | **High** for System Map CPU submission and upload pressure, especially with multiple textured bodies. | **Medium**: preserve seam/winding, body axes, rotation phase, texture longitude offset, backface culling and ring draw order. Existing `PlanetGlobeMeshRenderer` proves the static 64x128 sphere pattern. First migration after measurements. |
| **P1, profile-gated** | Scene visual-traffic culling / LOD / part compaction (`SceneRenderer`) | Every gameplay frame. `prepareScene` builds ship/part matrices and arrays; render pass performs per-ship and per-part frustum tests, distance/LOD selection, proxy selection and draw grouping. Cost scales with visual ships x parts. | **Good at high counts.** Static bounds/assembly offsets can remain GPU-resident; per-ship transforms/state can feed compute; compute can compact visible instances and indirect draw commands. | Upload per-ship transform/state, keep static part metadata resident, consume compacted buffers directly in drawing. **No CPU readback.** | **High only when counts are large.** May be neutral/negative for small scenes. | **Medium-high.** Two-level hierarchy, proxy/full model parity, mesh grouping, debug statistics and draw policy must remain correct. Do not start until `prepareScene` and `cpuVisualShipsMs` stress measurements justify it. |
| **P1/P2, profile-gated** | System/Galaxy map circles, orbit lines, billboard balls and halos | Every open-map frame. CPU repeatedly runs sin/cos segment loops and pushes line/triangle vertices; System Map also performs several `GL_DYNAMIC_DRAW` uploads per frame. | **Good, normally without compute.** Use static unit circle/ring/quad meshes and vertex shader/instancing. | Small instance/uniform uploads; static topology stays resident; no readback. | **Medium** if many orbits/markers are visible; otherwise small. | **Low-medium.** Preserve exact cartographic layering and selection visuals. P1 for System Map after sphere migration; P2 for Galaxy Map because the current system catalog is modest and CPU screen points are needed for interaction. |
| **P1 adjunct, profile-gated** | Instance-buffer streaming in `MeshGPU::drawInstanced` | Per instanced group/draw, CPU uploads complete `std::vector<mat4>` through `glBufferData(GL_DYNAMIC_DRAW)`. | **Good modernization target**, but this is transfer/driver optimization rather than algorithm offload. Persistent mapped/ring buffers or SSBO instance streams can reduce churn. | CPU -> GPU remains required for dynamic transforms, but avoid repeated allocation/orphan behavior and redundant copies; no readback. | **Medium** under large proxy/instance counts. | **Low-medium.** Measure first; can be implemented independently of compute culling. |
| **P2** | `GalaxyStarfieldRenderer` observer-relative rebuild | Rebuild only when observer moves beyond the configured threshold; active draw path otherwise uses prepared VBO. Current visible real catalog is on the order of a few thousand stars. | Parallel and GPU-friendly if catalog/rebuild frequency grows. | Upload stable star catalog once; shader/compute can derive observer-relative appearance. No readback if draw-only. | **Low today / potentially high at much larger catalogs.** | **Medium.** Defer until measured rebuilds matter. Old P1 assumption of per-frame rebuild was false. |
| **P2/P3** | HUD/navigation primitive tessellation (`WorldLabelRenderer`, map overlay glyphs) | Per visible marker; CPU constructs short ring/line/triangle batches and formats text. | Static glyph meshes/instancing can help large marker counts; compute is unnecessary. | Small instance stream; text/semantic data remains CPU. | **Low at current counts.** | **Low.** Profile only if thousands of markers become active. |
| **P2/P3** | Hub-map dynamic line/screen-circle submission | Hub geometry already sends assembly meshes through GPU resources and has CPU + asynchronous GPU timing. Small circles/lines are still generated on CPU. | Static unit primitives possible; compute not justified by code alone. | Small dynamic stream, no readback. | Unknown; likely low-medium. | **Low.** Use existing `HubMapBackend` perf stats first. |
| **KEEP CPU** | System/Detail/Hub presentation builders and overlays | Per frame/state change; branchy semantic classification, string IDs, hit radii, CPU interaction state, labels and map navigation data. | Poor compute fit because outputs are immediately consumed by CPU UI/picking logic. | GPU round-trip would introduce synchronization/readback. | Negative/low. | Keep CPU; optimize data structures only if profiling shows cost. |
| **KEEP CPU** | Hub exact picking/intersection | Input-driven, CPU broad-phase plus exact mesh/triangle intersection; result must immediately drive interaction. | Technically possible on GPU, architecturally poor for current path. | Synchronous readback would be required. | Likely negative. | Keep CPU. If it becomes a hitch, add CPU acceleration structures/cached bounds first. |
| **KEEP CPU** | `TrajectoryPredictor`, `LocalGuidancePlanner`, route/path planning | Sequential integration, iterative shooting, gravity sampling, constraints, semantic docking policies and branch-heavy validation. Results are CPU-owned planning data. | Poor for the current single/few-candidate workflow. A future large batch evaluator could differ. | GPU result would need CPU readback before route/navigation decisions. | Low/negative today. | Keep CPU by design. |
| **KEEP CPU** | `ClientWorldState` snapshot hydration/interpolation/prediction state | Per replication/presentation update; copies/merges graph state, interpolates modules, rebuilds visibility sets, smooths client render state. | State/branch/string/container heavy; CPU consumers immediately use results. | Readback would dominate and complicate correctness. | Low/negative. | Keep CPU; improve CPU lookup/caching if measured. |
| **KEEP CPU** | shared/authoritative-compatible ship physics (`SharedShipPhysics`) | Control-law state transitions and orientation/ship-controller integration. | Wrong ownership and poor batch economics for current client prediction. | Would require synchronization into CPU gameplay/prediction state. | Negative architecturally. | Keep CPU. Server-authoritative simulation remains CPU. |
| **KEEP CPU** | `GalaxyDatabase` | Startup/file load: JSON I/O, parsing, validation and maps. | Not a render-parallel workload. | N/A. | None. | Keep CPU. |
| **KEEP CPU** | celestial OBJ loading / generated asset loading / texture baking | Load-once cached OBJ parsing or offline generation. | Runtime GPU migration does not improve frame cost. | N/A. | None for runtime. | Keep CPU/offline. |
| **ALREADY GPU** | runtime procedural cloud texture generation (`ProceduralCloudLayer`) | CPU sets parameters; texture storage is GPU-only and generation is shader/FBO driven. | Already where the parallel work belongs. | No CPU pixel upload/readback in the active generator. | N/A. | Do not create a duplicate compute migration until a real CPU call site for old generators is proven. |
| **ALREADY GPU** | Milky Way backdrop, Hub planet surface, planet rings | Shader/fullscreen/mesh GPU passes with small CPU uniform setup. | Already GPU-driven. | Small uniforms only. | N/A. | Optimize only from GPU timing, not CPU-offload policy. |
| **NOT HOT** | `PlanetWireRenderer` generated sphere/grid geometry | Built at initialization and uploaded with static buffers. | GPU generation is possible but unnecessary. | One-time upload. | Negligible frame benefit. | Leave CPU. |
| **STUB** | `src/physics/*` | Current files are empty placeholders. | N/A. | N/A. | None. | No work. |

## P0 detail — System Map textured spheres

### Current path

`SystemMapSceneRenderer` calls `addSystemBodyGeometry()` while the System map is rendered. For a textured body, `SystemMapRenderer::addSystemBodyGeometry()` calls `addTexturedSystemBodySphere()`.

For each latitude/longitude cell the CPU computes four positions using latitude/longitude `sin/cos`, body north/prime/east axes, longitude offset and rotation phase, then appends two triangles (six vertices). `beginTexturedBodies()` clears the per-texture vectors every frame; `flushTexturedBodies()` uploads those vectors with `glBufferData(GL_DYNAMIC_DRAW)`.

Large planets use 64 x 128 cells:

```text
cells                = 64 * 128 = 8,192
vertices             = 8,192 * 6 = 49,152
bodyPoint evaluations= 8,192 * 4 = 32,768
```

Assuming the current `TexturedVertex` payload is 36 bytes (`vec3 + vec2 + vec4`), one large body produces roughly:

```text
49,152 * 36 = 1,769,472 bytes ~= 1.69 MiB per frame
at 60 Hz: ~= 101 MiB/s per large body
```

This estimate excludes vector growth/copy costs, trigonometry and GL driver overhead.

### Target path

Use a single indexed unit sphere resident on the GPU, analogous to `PlanetGlobeMeshRenderer::createSphereMesh()`:

```text
static unit sphere VBO/EBO (one-time)
       +
per-body center/radius/orientation/rotation-phase/UV-offset uniforms or instance data
       -> vertex shader
       -> existing texture sampling / culling / depth order
```

A compute shader is unnecessary. The vertex shader is already the natural parallel execution stage and avoids generating a second dynamic vertex buffer.

### Acceptance requirements

- visual parity for planet/moon texture orientation and seam;
- identical front-face/culling behavior;
- correct rotation phase and `textureLongitudeOffsetDeg`;
- ring back/body/ring front ordering unchanged;
- no per-frame CPU sphere vertex vector generation;
- no full-sphere per-frame `glBufferData` upload;
- CPU and GPU timings recorded before/after.

## P1 detail — visual traffic culling/LOD

`SceneRenderer::prepareScene()` currently constructs prepared visual-ship and part records on CPU. Near ships expand into module/part transforms; render then performs another pass for frustum visibility, distance, LOD and proxy/full-model selection. Proxy ships are already instanced, which is useful groundwork.

The GPU design should preserve the authority boundary:

```text
CPU client presentation snapshot
    -> compact per-ship dynamic transform/state upload
GPU-resident static assembly metadata/bounds
    -> compute visibility + LOD + compaction
    -> indirect/instanced draw buffers
    -> rendering
```

Do **not** read the compacted visibility list back to CPU. CPU picking/game logic keeps its own presentation/semantic state.

Before this becomes an implementation wave, add/record:

- separate `prepareScene()` CPU time (the existing `ScenePerf` total begins inside `renderInternal`, so preparation is not fully represented);
- visual ship count;
- prepared part count;
- proxy/full ship counts;
- `cpuVisualShipsMs`;
- instance matrix upload bytes;
- draw-call count;
- stress cases representative of the intended ~hundreds of combat craft / larger lightweight traffic populations.

If preparation + visual-ship submission stays comfortably below budget at target counts, leave it CPU.

## Map primitive modernization

System Map repeatedly creates orbit circles, belt circles, proxy markers, selection rings and billboard geometry with CPU trigonometry and dynamic uploads. These are better represented by reusable parameterized primitives than by compute:

- unit circle/ring VBO;
- unit billboard quad/disc;
- per-instance center/radius/color/basis;
- optional instancing for repeated primitives.

Keep CPU screen-space projections that are used by labels, picking and overlays. Do not push them to GPU only to read them back.

## De-prioritized previous candidates

### Old weather/cloud generators

`CloudAppearanceTextureGenerator` and `PlanetaryWeatherMapGenerator` are computationally GPU-friendly in isolation, but the current runtime procedural cloud path in `ProceduralCloudLayer` already allocates GPU texture storage without CPU pixels and generates the texture through shader/FBO rendering. Therefore these CPU generators are **not P0** unless a concrete runtime call site and profile proves they are still active.

### Galaxy starfield

The star catalog is already rendered from a prepared VBO. Observer motion only triggers a CPU rebuild beyond a configured displacement threshold. Compute remains a valid future scale path, but there is no evidence that this is a per-frame CPU bottleneck today.

### Planet wire geometry

`PlanetWireRenderer` builds mesh/grid data during initialization and uploads `GL_STATIC_DRAW` buffers. Moving that generation to GPU would save startup work, not frame time.

### Celestial baking/loading

`CelestialTextureBaker` is offline asset generation. `CelestialShapeMeshLibrary` caches loaded OBJ meshes. Neither belongs in the frame offload plan.

## Measurements required before code migration

1. Accept the OpenGL 4.3 Compatibility baseline locally and confirm `compute=1`, `ssbo=1`.
2. Add/collect System Map CPU timings for:
   - presentation/frame build;
   - orbit/primitive CPU generation;
   - textured body tessellation;
   - textured upload/draw submission.
3. Record System Map body counts and generated/uploaded vertex bytes.
4. Instrument `SceneRenderer::prepareScene()` separately from the existing render-phase timer.
5. Run representative scene stress counts and record ship/part/proxy counts, CPU phase times and instance-upload bytes.
6. Only after those measurements implement P0; P1 compute culling remains conditional.

## Non-negotiable ownership boundary

GPU output may be trusted for presentation, culling, LOD choice, procedural visual texture data and render-only compaction. CPU remains authoritative for simulation, damage, collision/gameplay decisions, route/path decisions, docking logic, replication, economy and any state whose answer is consumed synchronously by gameplay.

If a proposal requires `compute dispatch -> barrier -> synchronous readback -> CPU gameplay decision` each frame, reject it by default and redesign the data flow.
