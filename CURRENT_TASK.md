# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` performance gate active; trajectory precision candidates prepared in parallel and pending target-machine acceptance

## Accepted preconditions

`LocalHorizonPlanner` behavior and compact-candidate scaling remain accepted. Target-machine reference on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

`LocalAvoidancePlanner` behavior is accepted from `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
```

Accepted local behavior remains:

```text
nominal Clear
    -> zero probes

nominal ConflictHold
    -> deterministic 15 deg x 8 + 30 deg x 8 fan
    -> same-region / same-publication static proof
    -> compact dynamic recheck
    -> first proven target = AdjustedClear
    -> otherwise fail closed
```

## Gate A — multiplied avoidance probe cost

Harness:

```text
benchmarks/navigation_local_avoidance/
```

Pinned classes:

```text
nominal_clear_64
early_adjust_64
all_static_rejected_64
all_dynamic_rejected_16/64/256/1024
```

`1024` is deliberate stress. Current local CPU budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Do not optimize the fan until target-machine timing exists.

## Gate B — oriented passage geometry

Architecture:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
```

Code:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h/.cpp
```

Pinned semantics:

```text
flat hull + flat slot + correct attitude
    -> Fits while conservative sphere rejects

same hull in wrong roll
    -> rejected

two-obstacle ObstacleGap
    wide attitude -> rejected
    thin rolled attitude -> Fits

off-center / invalid frame
    -> fail closed
```

## Gate C — bounded emergent gap extraction

Code:

```text
src/world/navigation/trajectory/BoundedGapCandidateBuilder.h/.cpp
```

User requirement: when ordinary avoidance cannot clear two nearby objects because speed is high and remaining distance is short, the free space between those objects may itself be treated as a **slot**.

Required ownership/performance contract:

```text
one already-known primary conflict
    x already-reduced local neighbors
    -> hard cap 8 deterministic gap candidates
```

Explicitly forbidden:

```text
unbounded all-pairs / neighbor-neighbor / global N x N scan
```

Current builder rejects mixed snapshot revisions, overlapping conservative bounds, front/back pairs masquerading as slots, and gaps outside the bounded route window.

Behavior fixtures include direct `BoundedGapCandidateBuilder -> OrientedPassageEvaluator` composition.

Dedicated architecture checker:

```text
tests/architecture_contracts/check_navigation_trajectory_gap.py
```

Dedicated performance harness:

```text
benchmarks/navigation_trajectory_gap/
```

Scenarios:

```text
reject_16/64/256/1024
top8_16/64/256/1024
```

The `top8` path stresses deterministic maintenance of the best 8 candidates. `1024` neighbors is an artificial ceiling, not normal runtime expectation.

## Gate D — attitude reachability before gap entry

Code:

```text
src/world/navigation/trajectory/AttitudeReachabilityEvaluator.h/.cpp
```

A passage is not accepted merely because the hull fits geometrically. The ship must have enough longitudinal room to become attitude-ready before crossing the entry plane.

Inputs:

```text
current attitude
required passage attitude
current angular velocity
max angular acceleration
max angular speed
distance to entry
closing speed
available longitudinal braking
```

Result classes:

```text
AlreadyReady
ReachableCoast
ReachableWithBraking
UnreachableBeforeEntry
```

Pinned 90 degree-roll reference:

```text
max angular acceleration = 90 deg/s^2
max angular speed        = 90 deg/s
rest-to-rest 90 deg roll = 2 s
closing speed            = 10 m/s

30 m available -> ReachableCoast
15 m available -> ReachableWithBraking at 5 m/s^2
 8 m available -> UnreachableBeforeEntry
```

Current angular velocity is conservatively settled first and consumes additional time/orientation margin.

This is a constant-size precheck, not a duplicate flight controller or a complete swept-body proof.

## Docking continuation

Docking remains part of the same eventual 6DoF solver:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
explicit mating frame / top-bottom convention
```

For rotating ports:

```text
v_port = v_origin + omega x r
```

`bottom of ship -> bottom of dock` remains explicit; a 180 degree rolled/upside-down capture is invalid.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_local_avoidance_benchmark.py
bash benchmarks/navigation_local_avoidance/run_mingw64.sh

python tests/architecture_contracts/check_navigation_trajectory_passage.py
python tests/architecture_contracts/check_navigation_trajectory_gap.py
python tests/architecture_contracts/check_navigation_trajectory_reachability.py
bash tests/navigation_trajectory/run_mingw64.sh

python tests/architecture_contracts/check_navigation_trajectory_gap_benchmark.py
bash benchmarks/navigation_trajectory_gap/run_mingw64.sh
```

Send the complete output.

## Decision after results

1. close or adjust the 16-probe fan from measured timing only;
2. accept/fix oriented-passage, bounded-gap and attitude-reachability candidates from real MinGW64 evidence;
3. if the bounded-gap benchmark is cheap, freeze its hard-cap fallback and do not micro-optimize it;
4. next implement continuous 6DoF swept-body feasibility: translation + rotation through a static gap, including body-axis thrust/control authority and Elite/Newton semantics;
5. then extend the same time-varying pose/sweep machinery to moving obstacle gaps and moving/rotating docking;
6. live `EliteGame` / `EliteServer` and pursuit-specific integration remain later.
