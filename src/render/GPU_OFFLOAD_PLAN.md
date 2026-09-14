# Elite client GPU modernization plan

**Baseline decision:** OpenGL 4.3+ is the minimum graphical-client API.  
**Implementation status:** GL43 Compatibility baseline candidate implemented in `19db31eca5aa2c12475d87f2dfa322f794990c6e`; local runtime acceptance pending.  
**Audit status:** full client CPU -> GPU audit completed on 2026-09-14 against branch HEAD `a1483ea3542c215239ddc26279951bbc4064f3b2`.

Detailed evidence and workload matrix: `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md`.

## Transitional profile

The client currently requests **OpenGL 4.3 Compatibility Profile**, not Core Profile. This is intentional: legacy presentation code still contains fixed-function calls such as `glBegin`, `glColor*` and `GL_TEXTURE_2D`. Compatibility 4.3 gives the project compute shaders, shader-storage buffers and modern synchronization now, while allowing legacy passes to be removed incrementally. Moving to Core Profile is a later cleanup gate after those calls are gone.

The bundled GLAD loader must expose `gl:compatibility=4.3`. `render::gpu::GlRuntimeCapabilities` remains the single runtime capability gate.

## Ownership rule

GPU offload is for **client presentation and render-derived data**. Authoritative gameplay/simulation remains CPU-owned and deterministic unless a future design explicitly introduces a server-side accelerator path. Do not move authoritative navigation, damage, economy, replication or ship-state decisions into a client GPU merely because they are expensive.

A workload is a GPU candidate when it has enough parallel work, stable/resident inputs and a render-side consumer. Avoid `dispatch -> barrier -> synchronous readback -> CPU decision` in frame paths.

## Corrected priorities after full audit

### P0 — System Map textured body geometry

Replace per-frame CPU tessellation in `SystemMapRenderer::addTexturedSystemBodySphere()` with one shared indexed unit-sphere GPU mesh plus per-body uniforms/instance data and vertex-shader transforms.

This is deliberately **not a compute shader migration**. The topology is static; compute would only manufacture a dynamic buffer that the vertex shader can avoid entirely.

Why P0:

- large body path generates 49,152 vertices/body/frame on CPU;
- 32,768 body-point evaluations/body/frame include trigonometry and basis math;
- CPU vectors are cleared/rebuilt every frame;
- batches are re-uploaded with `GL_DYNAMIC_DRAW` every frame;
- roughly 1.69 MiB/body/frame is uploaded if `TexturedVertex` is 36 B;
- replacement requires only static geometry plus tiny dynamic parameters and no readback.

Acceptance: visual parity, no per-frame sphere vertex generation/upload, before/after CPU + GPU timing.

### P1 — Scene visual traffic culling/LOD, only if measurements justify it

Candidate: GPU-resident static assembly bounds/metadata plus compact per-ship dynamic state, compute visibility/LOD/instance compaction and direct instanced/indirect draw consumption.

Gate before implementation:

- instrument `SceneRenderer::prepareScene()` separately;
- profile target ship/part counts;
- use existing `ScenePerf` phase timings;
- record proxy/full counts and instance upload bytes.

If target-scale scenes are already comfortably inside CPU budget, keep the path CPU. No migration for ideological purity.

### P1/P2 — map primitive geometry, preferably static/instanced rather than compute

CPU-generated orbit circles, belt circles, halos, billboard balls and selection rings can become reusable unit primitives with per-instance parameters. System Map is the higher-value target; Galaxy Map is lower priority because the current catalog is modest and CPU screen points remain necessary for picking/labels.

### P1 adjunct — instance-buffer streaming

`MeshGPU::drawInstanced` currently uploads full model-matrix arrays with `glBufferData(GL_DYNAMIC_DRAW)` per group. Persistent mapped/ring buffers or SSBO instance streams may reduce CPU/driver/upload churn. This is a transfer optimization, not a reason to introduce compute by itself.

### P2 — starfield only if scale or rebuild timing changes

`GalaxyStarfieldRenderer` is not a per-frame CPU catalog rebuild. It renders a prepared VBO and rebuilds only after observer movement passes a threshold. GPU catalog/compute preparation remains a valid future scale path, but is not current P1 work without measurement.

### P2/P3 — HUD and Hub primitive cleanup

Small screen circles/rings/lines can use static glyph primitives if marker counts become large. Hub geometry already has GPU mesh submission and CPU/GPU timing. Use those measurements before further migration.

## Removed from the old priority list

### CPU weather/cloud generators are not current P0

The active runtime `ProceduralCloudLayer` already creates GPU-only texture storage and generates procedural cloud textures through shader/FBO passes. `PlanetaryWeatherMapGenerator` and `CloudAppearanceTextureGenerator` are GPU-friendly algorithms, but they are not a migration target until a real active runtime call site is demonstrated and measured.

### `PlanetWireRenderer` is not a frame hot path

Its generated sphere/grid geometry is built at initialization and uploaded as static buffers. GPU generation would mostly optimize startup, not frame CPU.

### Offline/generated celestial asset work is not a runtime offload wave

`CelestialTextureBaker` is offline. Cached OBJ/generated-asset loading should stay CPU unless startup profiling creates a separate asset-loading project.

## Keep CPU by design

- `TrajectoryPredictor`, `LocalGuidancePlanner`, route/path decisions and docking logic;
- authoritative/shared ship physics and gameplay state transitions;
- `ClientWorldState` replication hydration and CPU prediction state;
- System/Detail/Hub presentation semantics, string/UI data and CPU interaction state;
- Hub exact picking unless a future profile first proves it is an input hitch;
- `GalaxyDatabase` parsing/validation;
- text formatting and normal UI/label semantics.

For Hub picking, prefer a CPU BVH/cached acceleration structure before any GPU picking path that requires synchronous readback.

## Already GPU-driven

Do not duplicate work already on the correct side of the boundary:

- runtime procedural cloud texture generation;
- Milky Way backdrop;
- Hub planet surface shader;
- planet ring shader;
- shared `PlanetGlobeMeshRenderer` static sphere path;
- Hub assembly wire meshes through GPU geometry resources.

## Current acceptance gate

Before the first migration is accepted locally:

```bash
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify startup reports OpenGL >=4.3, `compute=1`, `ssbo=1`, credible compute limits, and visually smoke ordinary gameplay rendering and Hub map rendering.

Then capture a CPU baseline before changing the P0 path.

## Measurement protocol

For every proposed wave record:

- execution frequency;
- item/pixel/vertex count;
- CPU wall time;
- allocation/vector-build time where separable;
- bytes uploaded per frame/update;
- GPU execution time when relevant;
- stalls/synchronization points;
- target-scale scenario.

For P0 specifically add System Map timings around presentation/frame build, primitive generation, textured-sphere generation and textured upload/submission.

For scene compute-culling evaluation, add a timer around `prepareScene()` because existing `ScenePerf` timings begin later inside `renderInternal()`.

## Migration protocol

For each accepted GPU wave:

1. measure the existing CPU path;
2. keep a reference/parity route until visual/semantic parity is established;
3. upload stable input once and keep it resident;
4. keep dynamic transfer compact;
5. avoid synchronous readback;
6. consume GPU-derived output directly in rendering;
7. measure CPU frame time, GPU time and transfer stalls after migration;
8. add an architecture/regression contract preventing silent return to the old per-frame rebuild.

## Current label policy

`game::runtime::WorldSignalLabelsEnabled` is false. General world-signal label feed/draw remains gated off. Hub-map labels and explicit close-navigation HUD markers remain active. `WorldLabelRenderer` remains because navigation markers share it.
