# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-MAP-2` — CPU measured; corrected GPU benchmark rerun is the active task

## Source-of-truth rule

`main` is the only game-development baseline. Read and obey:

```text
REPOSITORY_SOURCE_OF_TRUTH.md
```

Do not continue feature work on `chatgpt/*`, rescue, staging or other parallel branches.

## Accepted architecture

Navigation v2 replaces the legacy route-wide synchronous architecture rather than optimizing it into the permanent design.

Legacy/migration chain:

```text
GeometricPathPlanner
    -> route-wide TrajectoryGenerator / RuckigRoutePlanner
    -> dense route sampling
    -> route-wide obstacle validation
    -> GuidanceTunnel
```

The reusable low-level kinematic component is `RuckigTrajectorySolver`; it remains a local state-to-state velocity/acceleration/jerk-limited primitive after routing/local avoidance selects a temporary target state.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
NAVIGATION_WORLD_V2.md
```

## NavigationMap reference block

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

The public API remains backend-neutral. Internal actor storage, prediction, spatial cells and future GPU resources stay private.

## Accepted target-machine evidence

Architecture and behavioral gates on `main`:

```text
NAVIGATION MAP BOUNDARY CONTRACT: PASS
navigation_map: 1/1 PASS
100% tests passed, 0 failed
```

CPU benchmark default run:

```text
horizon=3 s
warmup=5
iterations=30
```

Measured CPU results:

```text
scenario actors  rebuild med/p95 ms   corridor med/p95 ms   sphere med/p95 ms
cruise   1000    0.3090 / 0.3567     0.0169 / 0.0204       0.0102 / 0.0131
cruise   5000    1.3982 / 1.4902     0.0307 / 0.0499       0.0169 / 0.0307
cruise  10000    2.7388 / 2.9632     0.1352 / 0.2089       0.0329 / 0.1573
hub      1000    0.3103 / 0.3209     0.0082 / 0.0098       0.0071 / 0.0089
hub      5000    1.2864 / 2.0914     0.0231 / 0.0639       0.0262 / 0.0791
hub     10000    2.1889 / 2.9984     0.1381 / 0.2198       0.0496 / 0.1172
```

CPU interpretation:

- query cost is already well inside the current main-thread design budget in these scenarios;
- whole-snapshot rebuild is the CPU cost center and reaches ~2.2-2.7 ms median / ~3 ms p95 at 10k;
- CPU remains viable if rebuild is asynchronous, incremental or lower cadence;
- do not select a backend until GPU numbers exist;
- CPU and GPU harnesses measure different total work: the GPU prototype also performs all-agent neighbor/conflict reduction, so do not compare a single CPU total with a single GPU total as if they were equivalent.

CPU CSV:

```text
D:\__elite\work\navigation_map_cpu_benchmark.csv
```

## GPU harness failure found and fixed

The first GPU attempt configured successfully but failed during compilation:

```text
fatal error: glad/gl.h: No such file or directory
```

Cause: `benchmarks/navigation_gpu/CMakeLists.txt` exposed `${ELITE_ROOT}/glad`, but current GLAD 2 layout requires `${ELITE_ROOT}/glad/include`.

Corrected on `main`:

```text
benchmarks/navigation_gpu/CMakeLists.txt
```

A second stale contract was corrected at the same time:

```text
tests/architecture_contracts/check_navigation_gpu_benchmark.py
```

It now requires `NAV-V2-MAP-2` instead of obsolete `NAV-V2-GPU-0`, pins `glad/include`, and rejects the obsolete include root.

GPU performance is still **UNMEASURED**. A compile failure is not a GPU performance result.

## Active target-machine run

Do **not** rerun the already-passed CPU benchmark just because the GPU harness changed.

Run only:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_gpu_benchmark.py
bash benchmarks/navigation_gpu/run_mingw64.sh
```

If the corrected default GPU run succeeds, capture the full output including:

```text
gpu_bin_ms
gpu_neighbor_ms
gpu_total_ms
gpu_p95_ms
cpu_submit_ms
pairs / neighbor_checks
corridor count
occupied cells
overflow
out_of_bounds
max sweep
memory MiB
readback bytes
reference_ok for 1k cases
```

Only after that default run succeeds, run the longer measurement:

```bash
bash benchmarks/navigation_gpu/run_mingw64.sh --warmup 10 --iterations 100 --horizon 3
```

The CPU long run is optional and should be repeated only if we need tighter CPU distribution estimates after seeing GPU results.

## Backend decision gate

Do not preselect GPU.

- CPU query ownership remains viable from the measured numbers.
- GPU is attractive for P/V/A prediction, spatial binning and all-agent conflict reduction if its measured compute/submission/transfer costs stay inside budget.
- Hybrid remains explicitly valid: CPU static topology/precision graph search + GPU dynamic reduction.
- No GPU implementation may synchronously dispatch -> wait -> bulk read back on the frame thread.
- No backend may leak internal cells/actor buffers/GPU resources through the `NavigationMap` API.

Performance design targets:

```text
main-thread navigation CPU       < 0.5 ms typical
                                 < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred
                                 < 2.0 ms heavy-scene target
precision/global route solve     async only
```

## After the backend decision

Next stage is `NAV-V2-SPACE-1`:

```text
persistent static free-space / clearance
connectivity / portals
local invalidation
agent-envelope queries
cached sparse global corridor
```

Then integrate the bounded asynchronous shared NavigationWorld into the live runtime, add mass-NPC avoidance and precision docking/repair consumers, and only then retire obsolete route-wide legacy navigation.
