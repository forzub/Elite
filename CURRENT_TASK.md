# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** client CPU -> GPU migration  
**Stage:** GPU-P0.1 — repeated System Map primitive modernization

## Accepted prerequisite

OpenGL 4.3 Core is accepted locally.

GPU-P0 static textured spheres are also accepted locally after the corrected runtime path restored visible planets and moons while retaining resident indexed sphere geometry.

The station-adjacent freezes predate the renderer migration. They remain deferred and must not be investigated as part of this task.

## Accepted GPU-P0 result

The old per-frame textured-sphere hot path is closed:

- no per-frame latitude/longitude tessellation;
- no tens of thousands of sphere vertex `sin/cos` evaluations;
- no per-frame `TexturedVertex` arrays;
- no full-sphere `glBufferData(GL_DYNAMIC_DRAW)` upload;
- resident 24x48 / 64x128 indexed meshes;
- old runtime-proven `map_body_preview` shader ABI retained;
- per-body transform folded into `bodyMvp`;
- local visual smoke confirms planets and moons are visible again.

## Current target

Modernize repeated System Map primitive topology without introducing a CPU/draw-call regression.

Current hot candidates in `SystemMapRenderer.cpp`:

```text
addCircleXZ()
addCircleXY()
addOrbitCircle3D()
addBillboardBall()
flushLines()
flushSolids()
```

Today these functions rebuild circle/disc topology on the CPU every frame using repeated trigonometry, append complete transformed vertices into `m_vertices` / `m_solidVertices`, and upload those arrays through `GL_DYNAMIC_DRAW`.

## Design constraint

Do **not** naively replace the current batched line submission with one draw call per circle.

Preferred direction:

```text
resident unit primitive(s)
    +
compact per-instance center/radius/basis/color
    +
instanced/grouped submission
```

If multiple authored segment counts must remain visually distinct, group by topology/segment count rather than rebuilding vertices every frame. Static topology is preferred over compute.

## First implementation slice

Start with `addCircleXZ()` / `addCircleXY()` because they are structurally simple and widely reused.

Requirements:

- preserve existing center/radius/color/orientation semantics;
- preserve current visible smoothness/segment policy where practical;
- no per-frame `sin/cos` loops for migrated circles;
- no full transformed-circle vertex upload for migrated circles;
- avoid N individual draw calls for N circles when batching/instancing can preserve one/few submissions;
- keep non-circle arbitrary line geometry on the existing dynamic line path until separately migrated;
- add an architecture contract that prevents migrated circle topology from returning to per-frame CPU trigonometry.

Then evaluate `addOrbitCircle3D()` and `addBillboardBall()` separately.

## Acceptance

For the first circle slice:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
python tests/architecture_contracts/check_system_map_static_sphere.py
# new System Map primitive contract
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Visual smoke: System Map orbit circles, asteroid-belt rings, moon orbits, selection rings and player marker circles must remain visually unchanged.

`SceneRenderer` traffic visibility/LOD compute remains profile-gated. Keep navigation, picking, gameplay authority, replication and route/docking decisions CPU-owned.