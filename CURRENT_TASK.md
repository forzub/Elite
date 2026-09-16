# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-MAP-2` — isolated CPU/GPU measurement before live integration

## Source-of-truth rule

Read and obey:

```text
REPOSITORY_SOURCE_OF_TRUTH.md
```

The canonical GitHub branch above is the current project baseline. Do not inspect `main` and then explain missing current code as unpublished local work. That exact error occurred on 2026-09-16: `main` was behind/diverged from the active branch, so existing GitHub NavigationMap/library/benchmark code was incorrectly described as `LOCAL / UNPUSHED`.

Local console output supplied by the user is target-machine evidence. It is not proof of a different unseen code version.

## Accepted direction

Navigation v2 replaces the legacy route-wide synchronous architecture rather than optimizing it into the permanent design.

Legacy/migration chain:

```text
GeometricPathPlanner
    -> route-wide TrajectoryGenerator / RuckigRoutePlanner
    -> dense route sampling
    -> route-wide obstacle validation
    -> GuidanceTunnel
```

This chain remains only until v2 owns the live path. `SmoothPathOptimizer` is retired. `GeometricPathPlanner`, route-wide trajectory materialization/validation and `RuckigRoutePlanner` as a whole-route wrapper are not v2 authorities.

The reusable kinematic component is:

```text
RuckigTrajectorySolver
```

It remains a local state-to-state velocity/acceleration/jerk-limited motion primitive after routing/local avoidance selects a temporary target state.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

## Shared NavigationWorld target

Do not give every NPC an independent complete planner or full-scene scan.

```text
AUTHORITATIVE SYSTEM/WORLD STATE
        |
        v
SHIP-CENTERED NAVIGATIONWORLD
    static free-space / clearance / portals
    dynamic actors { P, V, A, bounds, flags, revision }
    shared spatial index
    prediction / swept bounds
    active corridors / local physical horizons
        |
        v
compact relevant/conflict candidates
        |
        +-> mass NPC cheap local steering
        +-> precision docking/repair/special planner
        |
        v
temporary target state
        |
RuckigTrajectorySolver / local motion
        |
flight control
```

Navigation axes are stable system/travel axes; the working origin may rebase with the active ship/domain but does not roll/pitch/yaw with the hull. Hub remains a private local domain and publishes only the subset relevant to the active NavigationWorld.

## NavigationMap block — accepted reference

Implemented under:

```text
src/world/navigation/map/
```

Public ingress:

```text
DynamicWorldUpdate
    sourceRevision
    WorkingFrame
    actors[] { id, P, V, A, radius, flags, motionRevision }
```

Public egress:

```text
queryCorridor()
querySphere()
stats()
```

The block owns system/world -> ship-centered conversion, actor storage, prediction and sparse spatial indexing behind PImpl. Internal cells/backend resources never cross the public API.

Current CPU reference provides:

```text
constant-acceleration endpoint prediction
conservative swept sphere
sparse 3D cell hash
corridor broadphase + conservative segment test
sphere/local broadphase + conservative sphere test
```

Target-machine behavioral evidence already reported:

```text
bash tests/navigation_map/run_mingw64.sh
navigation_map: 1/1 PASS
100% tests passed
```

`EliteNavigationMap` is not yet linked into the live `EliteGame` path; that is intentional until the isolated backend/space gates are accepted.

## Active work now: CPU/GPU benchmark

Preparatory harness defects found and corrected before the run:

1. `tests/architecture_contracts/check_navigation_map_boundary.py` still required `NAV-V2-MAP-1`; it now requires the actual `NAV-V2-MAP-2` state/task gate.
2. `benchmarks/navigation_map/run_mingw64.sh` used `ROOT/work/build/...`, which would create `/d/__elite/work/work/build/...`; it now imports `tests/helpers/build_layout.sh` and uses `${ELITE_TEST_BUILD_ROOT}/navigation_map_benchmark`, matching the canonical build layout.

CPU benchmark:

```text
benchmarks/navigation_map/
```

GPU benchmark:

```text
benchmarks/navigation_gpu/
```

Matched deterministic datasets:

```text
cruise: 1k / 5k / 10k actors, 18 km spawn cube, speed <= 250 m/s, accel <= 8 m/s^2
hub:    1k / 5k / 10k actors,  7 km spawn cube, speed <= 120 m/s, accel <= 6 m/s^2
prediction horizon: 3 s
```

Required measurements:

```text
CPU publication/rebuild median + p95
CPU corridor/local query median + p95
CPU cells visited / actors examined / candidate counts
GPU bin/prediction/corridor median
GPU neighbor/conflict median
GPU total median + p95
CPU command submission cost
candidate/conflict counts
occupied cells / overflow / rejected / out-of-bounds
CPU/GPU memory footprint
GPU readback bytes
```

## Target-machine run sequence

From MSYS2 MinGW64:

```bash
cd /d/__elite/work

git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh

bash benchmarks/navigation_map/run_mingw64.sh
bash benchmarks/navigation_gpu/run_mingw64.sh
```

For a more stable second pass after the first successful run:

```bash
bash benchmarks/navigation_map/run_mingw64.sh --warmup 10 --iterations 100 --horizon 3
bash benchmarks/navigation_gpu/run_mingw64.sh --warmup 10 --iterations 100 --horizon 3
```

Do not run the long pass before the short/default pass proves both harnesses work.

## Backend decision gate

Do not preselect GPU.

- If CPU spatial queries/rebuild cadence fit the budget with async/incremental publication, CPU remains viable.
- If CPU scaling is poor at 5k/10k while GPU prediction/binning/conflict reduction remains within budget, use GPU behind the same API.
- Hybrid is explicitly valid: CPU owns static topology/precision graph search; GPU owns actor prediction, binning and conflict reduction.
- No backend may expose internal actor/cell/GPU buffers through `NavigationMap` API.
- No GPU implementation may synchronously dispatch -> wait -> bulk read back on the frame thread.

Performance design targets:

```text
main-thread navigation CPU       < 0.5 ms typical
                                 < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred
                                 < 2.0 ms heavy-scene target
precision/global route solve     async only
```

## Route mechanism after backend selection

The intended route path is:

```text
cached sparse global corridor through free-space regions/portals
        |
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
execute first part
        |
repeat on receding physical horizon
```

Mass NPCs use a cheaper steering/avoidance consumer; docking, repair and other precision actors may use a more expensive local space-time planner. All consume the same shared NavigationWorld reduction layer.

## Ordered next work

1. **NOW:** run architecture/behavioral gate after the preparatory fixes.
2. **NOW:** run default CPU benchmark.
3. **NOW:** run default GPU benchmark.
4. Record raw outputs/CSV paths and compare matched 1k/5k/10k scenarios.
5. Run longer 100-iteration measurements only after both default runs succeed.
6. Choose CPU/GPU/hybrid dynamic backend from evidence.
7. Start `NAV-V2-SPACE-1`: persistent static free-space/clearance, connectivity/portals, local invalidation, agent-envelope queries.
8. Integrate bounded asynchronous shared NavigationWorld into live runtime.
9. Add mass-NPC avoidance and precision docking/repair planning as separate consumers.
10. Retire/remove obsolete legacy route-wide navigation components once v2 owns the live path; keep the low-level local kinematic solver as appropriate.

Do not spend the next iteration repairing the old route-wide planner as the main architecture.