# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Track:** Client GPU modernization
**Stage:** accept the GL43 runtime baseline, capture measured client CPU baselines, then remove the System Map per-frame CPU sphere rebuild

## Accepted prerequisites

The runtime architecture groundwork is accepted on MinGW64:

- `EliteNavigationGeometry` and `EliteAssemblyGeometry` physical seams are established;
- dual-source runtime model ingress is accepted;
- runtime-ingress architecture contracts pass;
- `EliteRuntimeModelAssets`, `EliteServer` and `EliteGame` build/link;
- `HtmlUiManager -> HtmlUiBridge -> HtmlUiServer` preserves `resourcePackPath`;
- shared `ModelAsset` schema/version remains authoritative for both editor and game.

The first read-only runtime-model consumer migration remains queued and must not be forgotten, but GPU foundation/performance work is the active track by explicit decision.

## Audit checkpoint — complete

The full client CPU -> GPU audit is recorded in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and summarized in `src/render/GPU_OFFLOAD_PLAN.md`.

The audit invalidated the old provisional order:

- CPU weather/cloud generators are not current P0 because the active `ProceduralCloudLayer` path already generates its texture on GPU;
- starfield catalog rebuild is threshold-triggered rather than per-frame;
- `PlanetWireRenderer` generated geometry is initialization/static;
- the strongest concrete live CPU workload is System Map textured-sphere tessellation and dynamic upload.

## Immediate gate 1 — accept GL43 foundation locally

The code candidate exists in commit `19db31eca5aa2c12475d87f2dfa322f794990c6e`. Do not treat it as accepted until the local runtime gate passes.

```bash
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify:

- startup `[OpenGL]` reports actual OpenGL >= 4.3;
- `compute=1`;
- `ssbo=1`;
- logged GPU/vendor/GLSL and compute limits are plausible;
- ordinary flight/system scene renders correctly;
- Hub map renders correctly;
- no regression in active Hub/close-navigation labels;
- general world-signal labels remain intentionally disabled.

OpenGL **4.3 Compatibility Profile** remains transitional. Do not move to Core Profile until legacy fixed-function call sites are removed.

## Immediate gate 2 — measurement baseline before migration

Add/collect timings without changing rendering behavior:

### System Map

Measure separately:

- presentation/frame build;
- orbit/primitive CPU generation;
- textured body sphere CPU generation;
- textured body upload/submission;
- visible textured body count;
- generated vertex count and upload bytes/frame.

### Main scene

Instrument `SceneRenderer::prepareScene()` separately because the current `ScenePerf` phase timer starts inside `renderInternal()` and does not fully represent preparation cost.

Record target-scale cases:

- visual ship count;
- prepared part count;
- proxy/full ship counts;
- `prepareScene()` CPU ms;
- existing `cpuVisualShipsMs`;
- instance matrix upload bytes;
- draw calls.

## First implementation wave after measurement — P0 System Map body sphere

If measurement confirms the code-derived cost, replace `SystemMapRenderer::addTexturedSystemBodySphere()` CPU tessellation with:

```text
one shared indexed unit sphere VBO/EBO
+ per-body center/radius/orientation/rotation/UV parameters
+ vertex shader
```

Do **not** introduce a compute shader for this wave. Static topology means a vertex shader is the simpler and better solution.

Acceptance:

- same body orientation and texture seam;
- same rotation phase and texture longitude offset;
- same culling/depth behavior;
- ring back/body/ring front order unchanged;
- no per-frame CPU sphere vertex-vector rebuild;
- no per-frame full-sphere `GL_DYNAMIC_DRAW` upload;
- before/after CPU and GPU timings documented.

## Conditional P1 — scene visual-traffic GPU culling/LOD

Only promote this to implementation if target-count measurements show material CPU cost.

Candidate design:

- GPU-resident static assembly bounds/local transforms;
- compact per-ship dynamic transform/state upload;
- compute visibility + LOD + visible-instance compaction;
- instanced/indirect rendering consumes GPU result directly;
- **no GPU -> CPU readback**.

If the measured target-scale path is already comfortably below budget, leave it CPU.

## Lower-priority rendering cleanup

- System Map repeated circles/halos/billboards: shared static primitives/instancing after P0;
- Galaxy Map primitive generation: profile only; keep CPU screen points for picking/labels;
- `MeshGPU::drawInstanced`: evaluate persistent/ring-buffer/SSBO streaming if matrix uploads become material;
- starfield compute: only if catalog size/rebuild timing grows enough to justify it;
- HUD/Hub small primitive generation: optimize only from measured counts/timing.

## Keep CPU

Do not move these to GPU under the current architecture:

- `TrajectoryPredictor`, `LocalGuidancePlanner`, route/path/docking decisions;
- authoritative/shared ship physics and gameplay transitions;
- `ClientWorldState` replication hydration/prediction state;
- System/Detail/Hub semantic presentation builders and text/UI logic;
- Hub exact picking unless a future profile first shows an interaction hitch;
- `GalaxyDatabase` parsing/validation;
- offline asset baking and cached load-once OBJ processing.

## Deferred but preserved runtime-asset task

After the current GPU baseline/wave work reaches a stable checkpoint, resume the model-ingress migration:

1. migrate one read-only consumer from direct `AssemblyMeshLibrary` access to `RuntimeModelAssetLibrary::get`;
2. keep that consumer on legacy source and verify parity first;
3. switch only the same object type to `.elmodel`;
4. compare bounds, LOD/render graph, semantic bindings and required runtime metadata;
5. remove the direct legacy dependency only after parity.

## State discipline

Every accepted GPU wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/game/GAME_RUNTIME_DECOMPOSITION.md`, `src/render/GPU_OFFLOAD_PLAN.md` and, when audit facts change, `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` in the same accepted commit.

Runtime-model ingress changes also update `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md`. GPU-only work does not edit that ingress contract unless the asset boundary actually changes.

Chat history is not canonical project state; these repository MD files are.
