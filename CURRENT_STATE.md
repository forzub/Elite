# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core **accepted locally**  
**GPU-P0:** System Map static textured spheres **accepted locally**  
**Active renderer work:** repeated System Map primitive modernization / profiling

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared deterministic/runtime geometry seams. Dual-source model ingress remains accepted and unchanged:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration remains queued behind the current renderer/GPU work.

## OpenGL 4.3 Core — ACCEPTED

The local developer build and runtime smoke passed on 2026-09-15. The client runs on the accepted OpenGL 4.3 Core baseline. The station-adjacent freezes predate that migration and remain explicitly deferred; they are not part of the current CPU -> GPU wave.

## GPU-P0 — System Map static textured spheres — ACCEPTED

### Previous hot path

A large textured body rebuilt `64 x 128 x 6 = 49,152` vertices every System Map frame and uploaded about `1.69 MiB/body/frame` before driver overhead.

### Accepted implementation

The accepted path keeps two resident indexed unit spheres:

- low: 24 x 48;
- high: 64 x 128.

The runtime-proven map-body shader ABI remains:

```text
aPos + aUv + aColor + uMVP
```

Per body:

```text
center/radius/basis/phase
    -> bodyModel
    -> bodyMvp = frameMvp * bodyModel
    -> constant color attribute
    -> glDrawElements(static sphere)
```

`textureLongitudeOffsetDeg` and `rotationPhaseRad` are folded once per body into the prime/east basis. The static mesh stays GPU-resident; no per-frame latitude/longitude tessellation and no full-sphere `GL_DYNAMIC_DRAW` upload return.

### Acceptance history

The first static-sphere candidate (`e9a9cfdef1331f67259d019fd9024054ce30779e`) introduced a new per-body shader ABI and was visually rejected because textured planets and moons disappeared while rings, labels and the rest of System Map remained visible.

A standalone OpenGL 4.3 Core raster diagnostic proved the indexed topology, EBO/VAO submission, `GL_BACK + GL_CCW` culling and `glDrawElements` path itself was valid. Correction commit `d9f1db5fdd6e72b68fa59e887bfc88535cb3f279` restored the old shader ABI and moved the body transform into `uMVP` instead of reverting the GPU optimization.

Automated evidence after the correction:

- `tests/architecture_contracts/check_system_map_static_sphere.py` PASS;
- permanent OpenGL 4.3 Core boundary PASS;
- Windows/MSYS2 MinGW64 syntax compilation of the complete corrected `SystemMapRenderer.cpp` PASS.

Local visual acceptance on 2026-09-15: **planets and moons returned**. The disappearance regression is closed and GPU-P0 is accepted. Any later texture-orientation/ring-order defect is a separate regression, not a reason to restore CPU sphere tessellation.

## Active next wave — repeated System Map primitives

Current CPU-generated topology still includes:

- `addCircleXZ()` — per-segment `sin/cos`, transformed vertices, appended into the dynamic line batch;
- `addCircleXY()` — same for XY circles;
- `addOrbitCircle3D()` — per-segment trigonometry plus CPU rotation;
- `addBillboardBall()` — per-segment trigonometry and dynamic triangle generation;
- `flushLines()` / `flushSolids()` — `glBufferData(GL_DYNAMIC_DRAW)` uploads of those rebuilt vertices.

The next implementation must remove repeated topology generation **without replacing one batched draw with hundreds of tiny draw calls**. Preferred shape is shared resident unit primitives + compact per-instance transforms/colors, grouped/instanced where practical.

`SceneRenderer` GPU visibility/LOD remains profile-gated. The known station freeze remains out of scope.