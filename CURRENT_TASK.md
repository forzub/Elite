# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** client CPU -> GPU migration  
**Stage:** GPU-P0 — System Map static textured sphere acceptance

## Accepted prerequisite

OpenGL 4.3 Core is **accepted locally**. The compatibility-removal track is closed. Do not reopen it without a concrete regression.

The station-adjacent freezes predate the OpenGL migration. They are intentionally deferred and must not be investigated as part of this task.

## P0 implementation candidate

Commit `e9a9cfdef1331f67259d019fd9024054ce30779e` replaces per-frame CPU textured-sphere tessellation in System Map with two resident indexed unit-sphere meshes:

- low: 24 x 48;
- high: 64 x 128.

Per frame the CPU now records only texture + center/radius/body basis/color/LOD selection. The vertex shader applies the body transform and `glDrawElements` submits the resident mesh.

Removed from the hot path:

- nested latitude/longitude tessellation loops;
- tens of thousands of per-body `sin/cos` vertex evaluations;
- per-frame `TexturedVertex` sphere arrays;
- full-sphere `glBufferData(GL_DYNAMIC_DRAW)` uploads.

Automated validation already PASS:

```text
check_gl43_modernization_boundary.py
check_system_map_static_sphere.py
Windows/MSYS2 MinGW64 g++ syntax compile of SystemMapRenderer.cpp
```

## Run now

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_gl43_modernization_boundary.py
python tests/architecture_contracts/check_system_map_static_sphere.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

## P0 visual acceptance

Only a focused System Map smoke is required:

- open a system with textured planets and moons;
- zoom through both normal/small and large-planet presentation ranges;
- verify planets do not disappear or render inside-out;
- verify texture seam and longitude orientation;
- verify axial orientation / visible rotation remains correct;
- verify rings still render in back -> planet -> front order;
- verify no obvious System Map regression outside textured bodies.

Do not use the known station-area freeze as an acceptance criterion for this wave.

## After P0 acceptance

Next preferred wave: System Map repeated primitive modernization/profiling — orbit circles, marker rings, billboard geometry and other topology that is rebuilt from CPU `sin/cos` loops each map frame. Prefer shared static parameterized geometry/instancing over compute.

`SceneRenderer` traffic visibility/LOD compute remains **profile-gated**. Do not start it merely because station-area freezes exist; first collect representative ship/part counts and CPU timing when that wave is intentionally opened.

Keep navigation, picking, gameplay authority, replication and route/docking decisions CPU-owned.
