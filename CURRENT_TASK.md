# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** client CPU -> GPU migration  
**Stage:** GPU-P0.1 — repeated System Map planar circles, final Core rebuild + visual acceptance

## Accepted prerequisite

OpenGL 4.3 Core remains the accepted renderer baseline.

GPU-P0 static textured spheres are also accepted locally after the corrected runtime path restored visible planets and moons while retaining resident indexed sphere geometry.

The station-adjacent freezes predate the renderer migration. They remain deferred and must not be investigated as part of this renderer slice.

## GPU-P0.1 implementation candidate

The repeated-primitive slice is implemented.

Migrated System Map scene consumers:

- primary planet orbit circles;
- asteroid-belt primary orbit plus the three visible belt rings;
- moon orbit circles;
- player position ring;
- selected-body XZ/XY rings;
- selected-hub XZ/XY rings.

The scene no longer routes these repeated planar circles through per-frame CPU `addCircleXZ()` / `addCircleXY()` tessellation.

### New path

`SystemMapGpuCircleBatch` owns lazy resident unit-circle meshes keyed by authored segment count. A mesh is generated once, uploaded with `GL_STATIC_DRAW`, then reused.

Per frame, migrated circles submit only compact instance data:

```text
center.xyz + radius
color.rgba
plane = XZ / XY
```

The GLSL 4.30 vertex stage expands the resident unit circle into map space. Matching topology/plane runs use `glDrawArraysInstanced(GL_LINE_LOOP, ...)`, so the migration does not replace one CPU batch with one draw call per circle.

CPU `sin/cos` for migrated circles therefore occurs only when a new resident segment-count mesh is first created, not every frame.

## Latest local acceptance result

The user ran the three architecture contracts and all reported PASS:

```text
GL43 CORE MODERNIZATION BOUNDARY: PASS
SYSTEM MAP STATIC TEXTURED SPHERE: PASS
SYSTEM MAP GPU CIRCLES: PASS
```

The following `EliteGame` build then exposed one missed Core incompatibility in `src/ui/components/minicamera/UICameraView.cpp`:

```text
glPushAttrib(GL_VIEWPORT_BIT | GL_TRANSFORM_BIT)
glPopAttrib()
```

Those APIs/constants are removed from OpenGL Core and were not covered by the existing static guard.

The branch now fixes that blocker by explicitly saving/restoring the viewport and the software bridge matrix-mode token, while continuing to use the bridge's projection/model-view matrix stacks. The GL43 boundary contract was also tightened to forbid compatibility attribute stacks and their bit tokens so this class of false PASS cannot recur.

## Intentional remaining renderer debt

This slice does **not** claim that every circular-looking primitive in `SystemMapRenderer` is migrated.

Still CPU-owned for now:

- arbitrary dynamic line geometry and `flushLines()`;
- `addOrbitCircle3D()` non-planar/oriented orbit helper;
- billboard/proxy body marker geometry (`addSystemBodyMarker*`, `addBillboardBall`, halos);
- dynamic solid geometry and `flushSolids()` where still used.

`addOrbitCircle3D()` should only be migrated if an active call site/material cost is demonstrated. Do not expand the current slice merely to remove dormant helpers. Billboard/proxy markers are a separate optimization candidate and must not delay the navigation performance investigation unless profiling shows material cost.

## Architecture guard

`tests/architecture_contracts/check_system_map_gpu_circles.py` locks the current GPU-circle boundary:

- resident `GL_STATIC_DRAW` unit-circle topology;
- instanced submission;
- compact center/radius/color instance payload;
- no return of scene-level XZ/XY circles to the old CPU tessellation API;
- no per-frame `sin/cos` circle generation in `SystemMapSceneRenderer`.

`tests/architecture_contracts/check_gl43_modernization_boundary.py` additionally rejects compatibility-only GL attribute-stack APIs and tokens.

## Acceptance now

Pull the latest branch and rerun:

```bash
git fetch origin
git pull --ff-only

python tests/architecture_contracts/check_gl43_modernization_boundary.py
python tests/architecture_contracts/check_system_map_static_sphere.py
python tests/architecture_contracts/check_system_map_gpu_circles.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Visual smoke in System Map:

- planet orbits unchanged;
- asteroid-belt three-ring appearance unchanged;
- moon orbits unchanged;
- player cross + ring unchanged;
- selected-body halo + two rings unchanged;
- selected-hub two rings unchanged;
- no disappearing/flickering or alpha/order regression.

Do **not** mark GPU-P0.1 accepted until the rebuild and runtime smoke pass.

## Immediately after GPU-P0.1 acceptance

Stop renderer work and switch to **NAV-PERF-0 — route/trajectory performance instrumentation**.

First navigation wave is measurement only: identify the exact source of the route/planner freeze before changing algorithms or moving work to GPU. At minimum measure planner wall time, predictor calls, integration steps, gravity evaluations, shooting iterations, safety segment/hazard checks and candidate/detour counts.

The CPU remains the authoritative/reference planner. A later GPU derivative is allowed for large batches of independent candidate trajectories/safety evaluations if profiling justifies it; blindly porting one sequential trajectory integrator to compute is not the target.
