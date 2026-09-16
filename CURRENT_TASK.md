# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — first static-space CPU reference candidate pending target-machine gate

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Previous stage — CLOSED

`NAV-V2-MAP-2` is accepted. Target-machine measurement selected **hybrid** ownership:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local search

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

Accepted 100-iteration 10k evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

GPU 1k correctness matched the independent CPU reference; all measured GPU scenarios had `overflow=0`, `out_of_bounds=0`, `valid=1`, fixed 32-byte readback.

## First `NAV-V2-SPACE-1` candidate now on `main`

Current candidate HEAD after the static-space files/tests were added:

```text
966d7269b824dd51a6bc5c56e535e0cce553a2bd
```

New isolated block:

```text
src/world/navigation/space/
    NavigationSpace.h
    NavigationSpace.cpp
    CMakeLists.txt
    README.md
```

Standalone test surface:

```text
tests/navigation_space/
    NavigationSpaceContractTests.cpp
    CMakeLists.txt
    run_mingw64.sh

tests/architecture_contracts/check_navigation_space_boundary.py
```

### Reference representation

The first CPU reference models navigable **free-space regions + explicit portals**, not a giant dense system voxel field.

Public concepts:

```text
AgentEnvelope
RegionInput
PortalInput
StaticSpaceUpdate
LocalPatch
queryPoint
queryCorridor
invalidateBounds
stats
```

Public header is intended to remain independent of GLM/OpenGL/GLFW/render/game state and hides storage/search structures behind PImpl.

### Reference behavior

- free-space point admission uses agent radius + additional clearance;
- region bounds and authored clearance cap both constrain admission;
- portal clearance rejects oversized agents;
- corridor search is deterministic BFS over stable region/portal IDs;
- disconnected regions fail closed;
- `invalidateBounds()` invalidates only intersecting free-space regions plus touching portals;
- invalidated topology fails closed immediately;
- `applyLocalPatch()` transactionally replaces/removes affected regions/portals without whole-world replacement;
- a rejected patch must not partially mutate state.

Current CPU reference limitations are intentional:

```text
AABB free-space regions
linear point-location/invalidation scan
unweighted BFS corridor search
no static-space benchmark yet
no live runtime integration yet
```

These are internal/reference limitations, not public API commitments.

## RUN NOW

Sync and run only the new static-space gate:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
```

Do not rerun CPU/GPU `NAV-V2-MAP-2` benchmarks; that gate is closed.

## If this gate passes

Next slice remains inside `NAV-V2-SPACE-1`:

1. add a deterministic static-space benchmark for region lookup, corridor query, invalidation and local patch;
2. test scale with large open-space region sets plus denser Hub/interior region sets;
3. use measured behavior to decide whether the CPU reference needs a sparse hierarchy/BVH/spatial hash for point-location and invalidation;
4. add costed corridor search only after the topology/reference boundary is accepted;
5. keep all of this isolated from `EliteGame` / `EliteServer` until the static-space gate is complete.

## Not yet

Do not yet:

- wire NavigationSpace into live runtime;
- repair the old route-wide planner;
- implement final mass-NPC steering;
- implement final precision docking/repair search;
- implement Shift+F12 debug rendering;
- delete migration navigation code.

## Performance contract

```text
main-thread navigation CPU       < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred, < 2.0 ms heavy-scene target
precision/global route solve     asynchronous only
```

Static-space rebuild/invalidation may be worker-side; ordinary point/clearance/corridor queries must remain bounded and cheap enough for shared NavigationWorld use.
