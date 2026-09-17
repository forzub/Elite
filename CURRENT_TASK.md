# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — moving continuous passage gate

## Newly closed gate — `MovingGapPredictor`

Target-machine run on:

```text
bc84940ff23209d9731a3d5332b8af3794182790
```

passed:

```text
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
7/7 navigation_trajectory CTest PASS
100% tests passed
```

Decision: `MovingGapPredictor` is **behavior/architecture accepted** and frozen unless downstream composition exposes a defect.

It remains one-pair bounded work:

```text
33 samples
32 continuous intervals
no pair discovery
no world scan
```

## Active candidate — `MovingPassageTrajectoryEvaluator`

Public contract:

```text
src/world/navigation/MOVING_PASSAGE_TRAJECTORY_MODEL.md
```

Code/tests:

```text
src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.h
src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.cpp
tests/navigation_trajectory/NavigationTrajectoryMovingPassageTests.cpp
tests/architecture_contracts/check_navigation_trajectory_moving_passage.py
```

### Input composition

```text
accepted MovingGapPredictor::Result
    +
ship cubic-Hermite P/V segment
    +
shortest-arc smooth attitude segment
    +
OBB hull proxy
    +
linear/angular capability
    +
Elite/Newton control policy
```

No pair search or duplicate NavigationWorld prediction is performed.

### Continuous proof

The evaluator synchronizes:

```text
33 ship poses <-> 33 moving-gap states
32 ship intervals <-> 32 moving-gap intervals
```

Sample fit alone is insufficient. Every interval conservatively includes:

```text
moving gap-width lower bound
relative transverse ship/gap-center displacement
passage-axis/frame rotation
relative OBB orientation sweep
body-axis acceleration projection bounds
```

A fixture explicitly requires:

```text
all 33 sampled ship poses fit
but interval proof is negative
    -> GeometryBlocked
```

### Collision ownership

This layer is predictive navigation, not the runtime collision engine.

```text
Navigation
    feasibility / expected-contact evidence / maneuver choice

Physics / Collision
    exact broadphase + narrow phase
    CCD / TOI
    contact manifold
    impulse / friction / restitution / ricochet

Damage / Structural
    damage / breach / detach
```

### Pinned behavior

```text
static gap + straight centered ship
    -> Feasible

moving gap + ship tracks gap-center velocity
    -> Feasible

moving gap + ship fails to follow it
    -> GeometryBlocked

all point samples fit but continuous bound fails
    -> GeometryBlocked

upstream gap not continuously open
    -> GapUnavailable before ship work

insufficient lateral thrust
    -> LinearAuthorityExceeded

sideways travel
    Newtonian -> Feasible
    EliteAssisted tight slip -> AssistedSlipExceeded
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_moving_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Expected suite count after build:

```text
8/8
```

## Next after green

1. accept/freeze moving continuous passage behavior;
2. build moving/rotating docking-frame prediction using the same relative-motion machinery;
3. add terminal 6DoF capture: relative position, linear velocity, attitude and angular rate;
4. enforce explicit bottom-of-ship -> bottom-of-dock mating frame semantics;
5. add deterministic `PilotSkillProfile` execution;
6. integrate accepted Navigation v2 into live game/server/guidance;
7. run end-to-end stress/debug/performance acceptance;
8. retire legacy navigation only after v2 owns the stable live path.

## Definition of final success

Navigation v2 is finished when the live runtime demonstrates normal flight, static/dynamic avoidance, oriented static/moving passage, truthful Elite/Newton vehicle authority, least-severity unavoidable collision behavior, post-impact replanning, stationary/moving/rotating docking, one shared guidance/debug truth and intended NPC scaling without planner stalls or unbounded precision work.
