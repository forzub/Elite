# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core **accepted locally**  
**Active renderer work:** CPU -> GPU migration, P0 System Map textured spheres

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared deterministic/runtime geometry seams. Dual-source model ingress remains accepted and unchanged:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration remains queued behind the current renderer/GPU work.

## OpenGL 4.3 Core — ACCEPTED

The local developer build and runtime smoke passed on 2026-09-15. The client now runs on the accepted OpenGL 4.3 Core baseline:

- GLFW requests OpenGL 4.3 Core Profile;
- bundled GLAD 2.0.8 is generated for `gl:core=4.3`;
- production `src/` has zero forbidden fixed-function/Compatibility API tokens under `check_gl43_modernization_boundary.py`;
- remaining legacy presentation semantics are translated by `CoreGlLegacyBridge` into software state + GLSL 4.30 Core + VAO/VBO submission;
- flight, cockpit/rear, Galaxy/System/Detail/Hub maps, HUD, radar/PPI, mini-camera and related visible paths were locally reported working.

The station-adjacent freezes are **not a GL4.3 regression**: they existed before this renderer transformation. They are explicitly deferred and are not part of the current migration scope.

## Active P0 — System Map static textured spheres

Implementation candidate commit: `e9a9cfdef1331f67259d019fd9024054ce30779e`.

The old System Map path rebuilt and uploaded textured sphere geometry every frame. A large body used 64 x 128 x 6 = 49,152 `TexturedVertex` records, approximately 1.69 MiB/body/frame before driver overhead.

The candidate now uses:

```text
one-time 24x48 indexed unit sphere VBO/EBO
one-time 64x128 indexed unit sphere VBO/EBO
        +
per-body center/radius/basis/color uniforms
        +
vertex shader transform
        -> glDrawElements
```

Per-frame CPU latitude/longitude tessellation and full-sphere `GL_DYNAMIC_DRAW` upload are removed. `textureLongitudeOffsetDeg` and `rotationPhaseRad` are folded into the per-body prime/east basis once per body. The existing 3D depth, backface-culling policy and ring back/body/front ordering remain owned by the System Map path.

## P0 automated evidence

PASS:

- `tests/architecture_contracts/check_system_map_static_sphere.py`;
- permanent GL4.3 Core boundary test;
- migration `git diff --check`;
- Windows/MSYS2 MinGW64 `g++ -std=c++17` syntax compilation of the complete `SystemMapRenderer.cpp` translation unit against the Core GLAD header.

The P0 code is therefore build-syntax/static-contract validated but still requires the developer's short visual System Map smoke before acceptance.

## Current acceptance boundary

Do **not** diagnose the station freeze in this wave.

Accept P0 only after checking textured planets/moons in System Map for:

- correct sphere visibility/winding;
- correct texture orientation and seam;
- correct axial orientation, longitude offset and rotation phase;
- correct low/high sphere LOD appearance while zooming;
- unchanged ring back/body/front ordering.

After P0 acceptance, continue with measured/static-geometry modernization of repeated System Map circles/orbits/markers. `SceneRenderer` GPU culling/LOD remains profile-gated and is not automatically the next step.
