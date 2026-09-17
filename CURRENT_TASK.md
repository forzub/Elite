# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` — same-region avoidance behavior accepted; dedicated avoidance performance gate active

## Accepted preconditions

`LocalHorizonPlanner` behavior and compact-candidate scaling remain accepted. Target-machine reference on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

The one-pass compact-candidate loop is not a bottleneck.

## `LocalAvoidancePlanner` behavior — ACCEPTED

Fresh target-machine gate on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
Total Test time: 0.06 sec
```

Accepted behavior:

```text
nominal Clear
    -> NominalClear / zero avoidance probes

nominal ConflictHold
    -> public NavigationSpace start query
    -> deterministic 15 deg x 8 + 30 deg x 8 fan
    -> same-region / same static-publication proof
    -> dynamic recheck through accepted LocalHorizonPlanner
    -> first proven target = AdjustedClear / PassThrough
    -> otherwise ConflictHold
```

Current-kinematics head-on/crossing remains fail-closed by design. Portal-crossing adjusted targets are not claimed yet.

## Active gate — multiplied avoidance probe cost

Dedicated harness:

```text
benchmarks/navigation_local_avoidance/
    CMakeLists.txt
    main.cpp
    README.md
    RUN_LOG.md
    run_mingw64.sh

tests/architecture_contracts/
    check_navigation_local_avoidance_benchmark.py
```

Measured behavior classes:

```text
nominal_clear_64
    0 probes / 1 horizon evaluation / 0 static point queries

early_adjust_64
    first lateral probe accepted
    1 probe / 2 horizon evaluations / 2 static point queries

all_static_rejected_64
    all 16 probes rejected by NavigationSpace
    1 horizon evaluation / 17 static point queries

all_dynamic_rejected_16/64/256/1024
    all 16 probes statically valid
    all 16 dynamic rechecks fail
    17 horizon evaluations / 17 static point queries
```

`1024` is deliberate stress, not an expected normal local-neighbor count.

The benchmark reports median/p95 plus probes, static/dynamic rejections, horizon-evaluation count, estimated compact-candidate visits and static point-query count. Scenario construction and static-space publication are outside the timed region.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_local_avoidance_benchmark.py
bash benchmarks/navigation_local_avoidance/run_mingw64.sh
```

Send the complete output.

## Decision after measurement

Use target-machine evidence against the existing local CPU budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Decision order:

1. if nominal/early paths are comfortably cheap and even the deliberate worst cases fit the budget, keep the deterministic 16-probe fan unchanged;
2. if only large worst-case candidate counts are expensive, introduce evidence-based probe ordering and/or a bounded dynamic-recheck budget rather than optimizing the already-cheap one-pass horizon loop;
3. after this performance gate, begin the trajectory-aware vehicle/control/docking layer in `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`;
4. that next layer must cover oriented hulls, Elite/Newton reachability, head-on/crossing maneuver feasibility, oriented narrow passages such as a flat ship through a flat slot, and terminal 6DoF docking against stationary/moving/rotating docks;
5. docking must match predicted port pose and motion, including relative linear/angular velocity and explicit ship/dock top-bottom mating orientation (`bottom of ship -> bottom of dock`), rather than accepting an upside-down center-point arrival;
6. do not wire live `EliteGame` / `EliteServer` or pursuit-specific intercept logic before these gates are closed.
