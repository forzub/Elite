# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` performance gate active; first trajectory precision-geometry candidate prepared in parallel

## Accepted preconditions

`LocalHorizonPlanner` behavior and compact-candidate scaling remain accepted. Target-machine reference on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

`LocalAvoidancePlanner` behavior is accepted from target-machine commit `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
Total Test time: 0.06 sec
```

Accepted local behavior remains:

```text
nominal Clear
    -> zero avoidance probes

nominal ConflictHold
    -> deterministic 15 deg x 8 + 30 deg x 8 fan
    -> same-region / same-publication static proof
    -> dynamic recheck through LocalHorizonPlanner
    -> first proven target = AdjustedClear
    -> otherwise fail closed
```

## Active gate A — multiplied avoidance probe cost

Dedicated harness:

```text
benchmarks/navigation_local_avoidance/
```

Pinned classes:

```text
nominal_clear_64
    0 probes / 1 horizon evaluation

early_adjust_64
    first probe accepted

all_static_rejected_64
    16 static rejections

all_dynamic_rejected_16/64/256/1024
    16 statically valid probes
    16 failed dynamic rechecks
    17 horizon evaluations
```

`1024` is deliberate stress. Current local CPU budgets remain:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Do not optimize the fan until target-machine evidence exists.

## Candidate B — oriented passage / emergent obstacle gap

User requirement added: two nearby objects may themselves form a usable **slot**. If ordinary avoidance cannot prove a side-step because speed is high and remaining distance is short, the system must not automatically conclude that collision is unavoidable. It may test the free space between nearby boundaries as a positive passage candidate.

Architecture authority:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
```

First isolated code candidate:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h
src/world/navigation/trajectory/OrientedPassageEvaluator.cpp
```

Ownership/performance rule:

```text
cheap broadphase / accepted local avoidance
        |
        +-- normal safe path -> done
        |
        +-- ConflictHold / explicit aperture / docking corridor
                |
                v
        bounded gap candidate builder
        primary conflict + nearby relevant boundaries only
        NO unbounded N x N obstacle-pair scan
        initial design target <= 4-8 passage candidates
                |
                v
        OrientedPassageEvaluator
        constant-size OBB projection math, O(1) per candidate
                |
                v
        only plausible passages -> later full 6DoF swept trajectory proof
```

Current geometry candidate deliberately does not own scene discovery, dynamics, NavigationMap, NavigationSpace, GLM, OpenGL or rendering.

Pinned behavior fixtures:

```text
flat hull + flat slot + correct attitude
    -> Fits
    -> conservative sphere rejects

same hull rolled 90 degrees
    -> rejected

two nearby obstacles -> ObstacleGap
    wide attitude -> rejected
    rolled thin attitude -> Fits

correct attitude but excessive lateral offset
    -> rejected

degenerate passage frame
    -> InvalidInput / fail closed
```

Architecture checker:

```text
tests/architecture_contracts/check_navigation_trajectory_passage.py
```

Behavior runner:

```text
tests/navigation_trajectory/run_mingw64.sh
```

This first candidate proves only oriented cross-section fit. It does **not** yet claim that the ship has enough time/rotation/thrust authority to reach that pose, or that a moving gap remains open for the complete crossing. Those are the next continuous 6DoF feasibility slices.

## Docking remains part of the same precision layer

Docking is terminal 6DoF pose matching against a possibly moving/rotating port. The precision passage model also applies to narrow docking tunnels.

Required terminal contract includes:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
explicit mating frame / top-bottom convention
```

For rotating ports, target-point velocity includes:

```text
v_port = v_origin + omega x r
```

`bottom of ship -> bottom of dock` remains an explicit mating-frame rule; a 180-degree rolled center-point arrival is invalid.

## RUN NOW

Fast-forward first:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD
```

Then run both pending gates:

```bash
python tests/architecture_contracts/check_navigation_local_avoidance_benchmark.py
bash benchmarks/navigation_local_avoidance/run_mingw64.sh

python tests/architecture_contracts/check_navigation_trajectory_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Send the complete output.

## Decision after results

1. accept or adjust the 16-probe fan only from measured timing;
2. if oriented-passage architecture + behavior are green, keep its O(1) fit evaluator and build the **bounded gap-candidate extractor** next;
3. candidate extraction must avoid all-pairs work and should start from the primary conflict plus local adjacency/spatial-bin/static-boundary evidence;
4. then add rotation-time/thrust-aware continuous 6DoF passage feasibility for head-on/crossing and narrow gaps;
5. extend the same pose/sweep machinery into moving/rotating docking;
6. live `EliteGame` / `EliteServer` and pursuit-specific integration remain later.
