# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — docking stage 9B continuous approach gate

## Progress

```text
[████████████████░░░░░░░░] 8 / 12 major stages closed

1–8  CLOSED / ACCEPTED
9    ACTIVE — moving/rotating docking 6DoF
     9A terminal capture      ACCEPTED
     9B continuous approach   ACTIVE
10   PilotSkillProfile        PENDING
11   live integration         PENDING
12   stress/debug/retirement  PENDING
```

## Newly closed gate — `DockingTerminalEvaluator`

Target-machine run on:

```text
835271539619b7dd02efc54ff54df51d64b49fce
```

passed:

```text
NAVIGATION TRAJECTORY DOCKING TERMINAL CONTRACT: PASS
9/9 navigation_trajectory CTest PASS
100% tests passed
```

Decision: docking terminal 6DoF math is **behavior/architecture accepted**.

Accepted capture gates:

```text
explicit Bottom(ship) -> Bottom(dock)
relative port P
relative port V
mating normals anti-aligned
roll/reference-up aligned
relative omega
```

Rotating port velocity includes `v_origin + omega x r`. A 180-degree rolled ship is rejected even with correct face normals.

The acceptance build reported one harmless unused helper warning (`normalizeOrZero`). It has already been removed; dock-port prediction is now exposed as the shared bounded helper used by 9B.

## Active candidate — `DockingApproachEvaluator`

Public contract:

```text
src/world/navigation/DOCKING_APPROACH_MODEL.md
```

Code/tests:

```text
src/world/navigation/trajectory/DockingApproachEvaluator.h
src/world/navigation/trajectory/DockingApproachEvaluator.cpp
tests/navigation_trajectory/NavigationTrajectoryDockingApproachTests.cpp
tests/architecture_contracts/check_navigation_trajectory_docking_approach.py
```

## Core decision — final docking segment is dock-local

A rotating dock must not be approximated as a static world target plus a rest-to-rest ship attitude.

Stage 9B transforms both ship endpoints into the predicted moving/rotating dock frame. In that relative frame:

```text
corridor is static
relative terminal pose is fixed
relative terminal angular rate = 0
```

Therefore at capture:

```text
omega_ship = omega_dock
```

without a terminal-only hack.

## Continuous corridor geometry

The accepted `ContinuousPassageTrajectoryEvaluator` is reused as the dock-local geometry oracle:

```text
PassageSource::DockingCorridor
33 synchronized relative poses
32 conservative continuous intervals
```

Point samples alone cannot prove the final approach.

## World physical authority

Relative dock-frame convenience does not create thrust. The candidate transforms the relative trajectory back to world space and evaluates inertial acceleration:

```text
v_world = v_port + omega x r + v_relative

a_world = a_port
        + omega x (omega x r)
        + 2 * omega x v_relative
        + a_relative
```

Centripetal and Coriolis terms are therefore real ship acceleration requirements.

Body-axis forward/reverse/lateral/vertical authority receives a conservative between-sample projection margin using a world jerk bound plus body-axis angular motion.

## Angular authority

For dock-local relative orientation change `theta` over `T`:

```text
relative omega peak = 1.5 * theta / T
relative alpha peak = 6.0 * theta / T^2
```

World bounds include dock rotation:

```text
world omega bound = |omega_dock| + relative omega peak
world alpha bound = relative alpha peak
                  + |omega_dock| * relative omega peak
```

## Terminal composition

After corridor and physical-authority success, the generated final world state is submitted to accepted `DockingTerminalEvaluator`.

Result classes:

```text
FeasibleForCapture
CorridorBlocked
LinearAuthorityExceeded
AngularAuthorityExceeded
AssistedSlipExceeded
TerminalNotCapturable
InvalidInput
```

`TerminalNotCapturable` retains the precise 9A reason for diagnostics.

## Pinned behavior

```text
stationary final approach -> FeasibleForCapture
translating dock + co-moving ship -> FeasibleForCapture
moving+rotating dock + dock-local tracking -> FeasibleForCapture + relative omega 0
all 33 point samples fit but continuous bound fails -> CorridorBlocked
insufficient inertial body-axis thrust -> LinearAuthorityExceeded
dock rotation faster than ship angular-rate authority -> AngularAuthorityExceeded
wide corridor + wrong 180-degree roll -> TerminalNotCapturable/RollAlignmentMismatch
wide corridor + terminal position miss -> TerminalNotCapturable/PositionMismatch
sideways approach: Newtonian feasible / tight EliteAssisted slip rejected
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_docking_approach.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Expected suite count:

```text
10/10
```

## Next after green

1. accept/freeze docking stage 9B and close major stage 9;
2. begin deterministic `PilotSkillProfile` execution fixtures;
3. live `EliteGame` / `EliteServer` / guidance + physics/collision hookup;
4. end-to-end stress/debug/performance acceptance;
5. retire legacy navigation only after stable v2 ownership.

## Definition of final success

Navigation v2 is finished when the live runtime demonstrates normal flight, static/dynamic avoidance, oriented static/moving passage, truthful Elite/Newton vehicle authority, least-severity unavoidable collision behavior, replanning from actual post-impact truth, stationary/moving/rotating docking to the correct mating frame, shared guidance/debug truth and intended NPC scaling without planner stalls or unbounded precision work.
