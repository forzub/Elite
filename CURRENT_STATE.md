# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-MAP-2` — ship-centered NavigationMap CPU reference accepted; CPU benchmark measured; GPU measurement pending rerun after harness fix

## Repository source of truth — mandatory

The only canonical game-development branch is:

```text
main
```

Permanent rule:

```text
REPOSITORY_SOURCE_OF_TRUTH.md
```

Long-lived parallel game-development branches are prohibited. Temporary rescue refs may be used only to preserve/recover divergent history, must not receive continued feature work, and must be merged into `main` and removed after reconciliation.

On 2026-09-16 three divergent histories (`main`, `chatgpt/mae-v01075-semantic-workflow-motion-v5`, and the user's local line published as `rescue/local-97500`) were audited and reconciled into `main` with a three-parent merge. Both former development lines are now ancestors of `main`; current code/state must no longer be read from either of them.

The earlier conclusion that NavigationMap/library/benchmark work was `LOCAL / UNPUSHED` was wrong. The code already existed on GitHub on the divergent development line. The corrective rule is to keep `main` authoritative and inspect its exact HEAD before coding/reporting state.

Local console output supplied by the user is target-machine evidence. It is not proof of a separate unseen code version unless the user explicitly says there are unpublished local changes.

## Stable baseline outside navigation

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains shared runtime/editor schema authority. Renderer feature work remains paused while Navigation v2 is established.

## Navigation architecture reset

The legacy route-wide synchronous chain is migration code, not the new foundation:

```text
GeometricPathPlanner
    -> route-wide trajectory materialization / RuckigRoutePlanner
    -> dense route sampling and route-wide obstacle validation
    -> GuidanceTunnel
```

`SmoothPathOptimizer` is retired. `GeometricPathPlanner`, the route-wide trajectory wrapper/sampling path and current guidance plumbing remain only while the live path is migrated; they are not Navigation v2 architecture authority.

The reusable low-level kinematic component is `RuckigTrajectorySolver`: state-to-state local motion under velocity/acceleration/jerk limits after routing/local avoidance has selected a target state. Ruckig is not free-space/path-search authority.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
NAVIGATION_WORLD_V2.md
```

## Accepted coordinate/runtime domains

### Authoritative system/world state

Long-lived simulation, replication, celestial and persistent-object state remains in precise system/world coordinates. This is storage/simulation authority.

### Active ship-centered NavigationWorld

Ordinary flight/navigation work is performed in a ship-centered working domain:

- origin may translate/rebase with the active ship/domain;
- navigation axes remain stable system/travel axes;
- axes do **not** follow hull roll/pitch/yaw;
- nearby static navigation geometry, dynamic actors, predicted swept bounds and active route corridors live in this working domain;
- hull-local coordinates remain flight/control detail;
- render/player-relative coordinates remain presentation-only.

### Hub-local space

Hub owns its local geometry, docking ports and scheduled local bots. Only the subset that can affect the active ship is transformed/published into the ship NavigationWorld. The same rule applies to carriers, capital ships, settlements and interiors.

## Shared NavigationWorld ownership

The v2 target is one shared data-oriented NavigationWorld for the active domain, not one full-world planner per actor.

```text
AUTHORITATIVE SYSTEM/WORLD STATE
            |
            v
ship-centered NavigationWorld
    + static free-space / clearance
    + dynamic actor table { P, V, A, bounds, flags, revisions }
    + spatial index
    + prediction / swept bounds
    + active corridors / local horizons
            |
            v
compact relevant/conflict candidates
      |                         |
      v                         v
mass NPC steering       precision docking/repair planner
      |                         |
      +------------+------------+
                   v
            temporary target state
                   |
        RuckigTrajectorySolver / local motion
                   |
             flight control
```

Background traffic uses cheap shared corridor + local avoidance. Precision actors use more expensive space-time/local planning only after shared spatial reduction.

## NavigationMap ownership boundary

Implemented block:

```text
src/world/navigation/map/
    NavigationMap.h
    NavigationMap.cpp
    CMakeLists.txt
    README.md
```

The block is built as its own `EliteNavigationMap` static library for standalone testing/benchmarking. It is intentionally not yet linked into the live `EliteGame` / `EliteServer` runtime path.

Public ingress is an owned `DynamicWorldUpdate` by value containing working frame, source revision and actor P/V/A/radius/flags/revision data.

The block transforms and owns resulting state internally. Actor tables, prediction caches, sparse cells and future CPU/GPU backend resources do not cross the boundary. `NavigationMap.h` deliberately has no GLM/OpenGL/GLFW/scene/game-state dependency and uses PImpl.

Public egress:

```text
queryCorridor()
querySphere()
stats()
```

The CPU reference backend implements constant-acceleration endpoint prediction, conservative swept spheres and a sparse 3D cell hash. It is the deterministic behavior oracle for future CPU/GPU/hybrid backends.

User target-machine evidence reported 2026-09-16:

```text
python tests/architecture_contracts/check_navigation_map_boundary.py
NAVIGATION MAP BOUNDARY CONTRACT: PASS

bash tests/navigation_map/run_mingw64.sh
navigation_map: 1/1 PASS
100% tests passed, 0 failed
```

This is behavioral evidence for the reference block.

## `NAV-V2-MAP-2` CPU measurement — target-machine evidence

Default benchmark run:

```text
horizon_s=3
warmup=5
iterations=30
```

Measured results:

```text
scenario actors  rebuild med/p95 ms   corridor med/p95 ms   sphere med/p95 ms
cruise   1000    0.3090 / 0.3567     0.0169 / 0.0204       0.0102 / 0.0131
cruise   5000    1.3982 / 1.4902     0.0307 / 0.0499       0.0169 / 0.0307
cruise  10000    2.7388 / 2.9632     0.1352 / 0.2089       0.0329 / 0.1573
hub      1000    0.3103 / 0.3209     0.0082 / 0.0098       0.0071 / 0.0089
hub      5000    1.2864 / 2.0914     0.0231 / 0.0639       0.0262 / 0.0791
hub     10000    2.1889 / 2.9984     0.1381 / 0.2198       0.0496 / 0.1172
```

10k diagnostic counts:

```text
cruise: corridor candidates=63, examined=239; sphere candidates=43, examined=204; occupied cells=8310
hub:    corridor candidates=150, examined=815; sphere candidates=192, examined=1382; occupied cells=1719
out_of_bounds=0 and rejected=0 for all measured cases
```

CPU interpretation at this gate:

- corridor/sphere query cost is comfortably below the current `<0.5 ms typical / <1 ms peak` main-thread design target in the measured 1k/5k/10k scenarios;
- full snapshot rebuild is the CPU cost center and reaches roughly `2.2–2.7 ms median`, ~`3 ms p95` at 10k;
- this does **not** reject CPU query ownership: rebuild can be asynchronous/incremental or performed at a lower cadence;
- backend selection remains open until the GPU measurement exists because the GPU prototype performs additional all-agent neighbor/conflict work not represented by these two CPU query calls.

CSV produced by the user's run:

```text
D:\__elite\work\navigation_map_cpu_benchmark.csv
```

## GPU benchmark status — build harness fixed, measurement pending

The first target-machine GPU benchmark attempt did **not** produce performance numbers. CMake configured, but compilation failed because the harness exposed the obsolete include directory:

```text
-I D:/__elite/work/glad
fatal error: glad/gl.h: No such file or directory
```

Current GLAD 2.0.8 layout is:

```text
glad/include/glad/gl.h
glad/src/gl.c
```

`benchmarks/navigation_gpu/CMakeLists.txt` was corrected on `main` to use `${ELITE_ROOT}/glad/include`.

A second stale harness contract was found at the same time: `tests/architecture_contracts/check_navigation_gpu_benchmark.py` still required obsolete project stage `NAV-V2-GPU-0`. It now requires the active `NAV-V2-MAP-2` gate and explicitly rejects the old GLAD include root.

GPU performance remains **UNMEASURED** until the corrected benchmark compiles and runs on the target machine.

## GPU feasibility state

Strong GPU candidates are:

```text
P/V/A actor prediction
conservative future swept bounds
spatial binning/hash
corridor/local-horizon relevance filtering
all-agent neighbor/conflict candidate generation
```

Dynamic P/V/A stays actor-owned; do not build a dense velocity/acceleration voxel field. Single-agent A*/Theta*/SIPP-style graph search is not the first GPU migration target. Precision/global search remains an asynchronous CPU-worker candidate until batched evidence justifies otherwise. A hybrid backend is explicitly valid.

## GPU scheduling invariant

No production navigation path may synchronously wait for compute completion or perform bulk blocking GPU readback on the frame thread.

```text
frame N:   submit dynamic NavigationWorld work
frame N+1: consume last completed bounded result; submit next work
```

Use double/triple buffering. Compute/data age is part of the local physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

## Route architecture target

Navigation v2 separates global intent from local physical work.

```text
cached sparse global corridor through free-space regions/portals
        |
        v
NavigationMap corridor/local-horizon query
        |
predicted conflicts for returned actors only
        |
local route / velocity correction
        |
temporary target state
        |
RuckigTrajectorySolver
        |
execute first part and repeat on a receding horizon
```

A global route is not materialized into thousands of dense samples to the final destination. Global route validity is event/revision driven, while local avoidance runs on a bounded physical horizon.

## Navigation / collision / damage boundary

```text
Navigation
    conservative envelopes / free-space / predicted conflicts

Physics / Collision
    broadphase candidates -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local navigation invalidation
```

Render, collision, hit/damage and navigation geometry intentionally differ. A breach is navigable only when clearance admits the requesting agent envelope.

## Performance targets

```text
main-thread navigation CPU       < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred, < 2.0 ms heavy-scene target
full/precision route solve       asynchronous; never a frame-thread blocker
```

These are design targets, not cross-machine assertions. Rendering competes for the same GPU.

## Active decision gate / next step

Current stage: `NAV-V2-MAP-2`.

1. sync local checkout to current `main` containing the GPU harness corrections;
2. run `python tests/architecture_contracts/check_navigation_gpu_benchmark.py`;
3. run `bash benchmarks/navigation_gpu/run_mingw64.sh`;
4. record GPU timing, scaling, memory, conflict counts, overflow/out-of-bounds, CPU submission and readback data;
5. compare against the already recorded CPU measurement, while keeping the different CPU/GPU workloads explicit;
6. run longer 100-iteration measurements only after the corrected GPU default run succeeds;
7. choose CPU/GPU/hybrid dynamic backend without changing `NavigationMap` public API;
8. then start `NAV-V2-SPACE-1`: static free-space, clearance, connectivity/portals, local invalidation and agent-envelope queries;
9. integrate bounded asynchronous live NavigationWorld;
10. add mass-NPC avoidance and precision docking/repair consumers;
11. remove obsolete route-wide legacy navigation once v2 owns the live path.

Do not rerun the already-passed CPU benchmark merely because the GPU harness was fixed. Do not repair the old route-wide planner as the primary solution. Do not reintroduce heavy periodic synchronous route solves on the frame thread.