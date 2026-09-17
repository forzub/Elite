# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — docking terminal 6DoF gate (stage 9A)

## Progress

```text
[████████████████░░░░░░░░] 8 / 12 major stages closed

1–8  CLOSED / ACCEPTED
9    ACTIVE — moving/rotating docking 6DoF
10   PilotSkillProfile
11   live integration + physics hookup
12   end-to-end stress/debug + legacy retirement
```

## Newly closed gate — `MovingPassageTrajectoryEvaluator`

Target-machine run on:

```text
18799ab2c026b6d6dce3da11f9225eae3a3c5f35
```

passed:

```text
NAVIGATION TRAJECTORY MOVING PASSAGE CONTRACT: PASS
8/8 navigation_trajectory CTest PASS
100% tests passed
```

Decision: `MovingPassageTrajectoryEvaluator` is **behavior/architecture accepted**. The isolated navigation stack can now prove one complete oriented ship trajectory through one time-varying moving gap with between-sample geometry and truthful vehicle authority.

Exact runtime collision remains downstream physics authority.

## Active candidate — `DockingTerminalEvaluator`

Public contract:

```text
src/world/navigation/DOCKING_TERMINAL_MODEL.md
```

Code/tests:

```text
src/world/navigation/trajectory/DockingTerminalEvaluator.h
src/world/navigation/trajectory/DockingTerminalEvaluator.cpp
tests/navigation_trajectory/NavigationTrajectoryDockingTerminalTests.cpp
tests/architecture_contracts/check_navigation_trajectory_docking_terminal.py
```

### Stage 9A scope

One supplied candidate ship state at a future capture time is compared with one predicted moving/rotating dock-port frame.

Ship and dock ports carry explicit local metadata:

```text
surface semantic
local port offset
mating normal
referenceUp / rollReference
```

Current required pairing:

```text
Bottom(ship) -> Bottom(dock)
```

No world-up inference is used.

### Moving/rotating dock prediction

Dock origin uses bounded constant-acceleration translation and constant world-space angular velocity for this first terminal slice:

```text
p_origin(t) = p0 + v0*t + 0.5*a*t^2
v_origin(t) = v0 + a*t
```

Port orientation/offset are rotated to capture time and port velocity includes:

```text
v_port = v_origin + omega x r
```

Ship offset-port velocity likewise includes:

```text
v_ship_port = v_ship_center + omega_ship x r_ship
```

### Terminal 6DoF capture gates

```text
relative position
relative linear velocity
mating normals anti-aligned
referenceUp / roll aligned
relative angular velocity
```

A 180-degree rolled arrival is rejected even when face normals are correct.

Result classes:

```text
Capturable
PortSemanticMismatch
PositionMismatch
RelativeLinearVelocityMismatch
MatingNormalMismatch
RollAlignmentMismatch
RelativeAngularVelocityMismatch
InvalidInput
```

### Pinned fixtures

```text
stationary bottom-to-bottom -> Capturable
Top ship port vs required Bottom -> PortSemanticMismatch
translating carrier + matched P/V -> Capturable
translating carrier + unmatched V -> RelativeLinearVelocityMismatch
rotating offset port -> omega x r tangential velocity is required
combined translating + rotating future frame -> Capturable when P/V/attitude/omega all match
correct face normal + 180-degree reversed roll -> RollAlignmentMismatch
correct pose + unmatched omega -> RelativeAngularVelocityMismatch
position outside tolerance -> PositionMismatch
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_docking_terminal.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Expected suite count:

```text
9/9
```

## Next after green

1. accept stage 9A terminal capture math;
2. stage 9B: continuous moving/rotating docking corridor and terminal convergence using the accepted moving-passage machinery;
3. deterministic `PilotSkillProfile` execution;
4. live `EliteGame` / `EliteServer` / guidance + physics/collision hookup;
5. end-to-end stress/debug/performance acceptance;
6. retire legacy route-wide navigation only after v2 owns the stable live path.

## Definition of final success

Navigation v2 is finished when the live runtime demonstrates normal flight, static/dynamic avoidance, oriented static/moving passage, truthful Elite/Newton authority, least-severity unavoidable collision behavior, replanning from actual post-impact truth, stationary/moving/rotating docking to the correct explicit mating frame, shared guidance/debug truth and intended NPC scaling without planner stalls or unbounded precision work.
