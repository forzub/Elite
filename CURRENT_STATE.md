# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core **accepted locally**  
**Active renderer work:** CPU -> GPU migration, P0 System Map textured-sphere regression correction

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared deterministic/runtime geometry seams. Dual-source model ingress remains accepted and unchanged:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration remains queued behind the current renderer/GPU work.

## OpenGL 4.3 Core — ACCEPTED

The local developer build and runtime smoke passed on 2026-09-15. The client runs on the accepted OpenGL 4.3 Core baseline. The station-adjacent freezes predate that migration and remain explicitly deferred; they are not part of the current CPU -> GPU wave.

## P0 — System Map static textured spheres

### Original hot path

A large textured body rebuilt `64 x 128 x 6 = 49,152` vertices every System Map frame and uploaded about `1.69 MiB/body/frame` before driver overhead.

### First candidate — visually rejected

Initial implementation commit: `e9a9cfdef1331f67259d019fd9024054ce30779e`.

It introduced resident 24x48 and 64x128 indexed unit spheres and a new vertex-shader ABI with per-body center/radius/basis uniforms. Static contracts and MinGW syntax passed, but the local runtime smoke failed: **textured planets and moons disappeared completely while rings, labels, grid and other System Map presentation remained visible**. P0 was therefore not accepted.

A standalone OpenGL 4.3 Core raster diagnostic reproduced the exact indexed sphere topology, current shader pair, `GL_BACK + GL_CCW` culling, EBO/VAO submission and `glDrawElements`. It passed with:

```text
shader link = PASS
GL error    = GL_NO_ERROR
center pixel= 255,255,255,255
```

That rules out a generic failure of the static sphere topology/index buffer/culling path. The regression is narrowed to integration of the newly introduced shader ABI/state in the full System Map runtime.

### Corrected candidate

Correction commit: `d9f1db5fdd6e72b68fa59e887bfc88535cb3f279`.

The optimization is retained, but the already-proven System Map body shader interface is restored:

```text
resident 24x48 / 64x128 indexed unit spheres
        +
old map_body_preview shader ABI:
    aPos + aUv + aColor + uMVP
        +
per-body bodyModel
bodyMvp = frameMvp * bodyModel
        -> glDrawElements
```

`textureLongitudeOffsetDeg` and `rotationPhaseRad` are still folded once per body into the prime/east basis. The static mesh stays GPU-resident; no per-frame latitude/longitude tessellation and no full-sphere `GL_DYNAMIC_DRAW` upload return.

The per-body color is supplied through the old `aColor` attribute contract as a constant generic vertex attribute, and the mesh EBO is explicitly rebound before indexed drawing to make the ownership/state boundary unambiguous.

## Corrected-candidate automated evidence

PASS:

- `tests/architecture_contracts/check_system_map_static_sphere.py`;
- permanent OpenGL 4.3 Core boundary test;
- Linux hidden OpenGL 4.3 Core raster diagnostic for the original indexed topology;
- Windows/MSYS2 MinGW64 `g++ -std=c++17` syntax compilation of the complete corrected `SystemMapRenderer.cpp` translation unit.

The corrected P0 candidate is therefore pending only the focused local System Map visual smoke.

## Current acceptance boundary

Do **not** diagnose the known station freeze in this wave.

Accept corrected P0 only after confirming:

- planets and moons are visible again;
- sphere winding/visibility is correct;
- texture orientation and seam match the pre-P0 baseline;
- axial orientation, longitude offset and rotation phase remain correct;
- low/high sphere LOD transition remains visually acceptable;
- ring order remains back -> body -> front.

After P0 acceptance, continue with repeated System Map circles/orbits/markers using shared static parameterized primitives. `SceneRenderer` GPU culling/LOD remains profile-gated and is not automatically next.