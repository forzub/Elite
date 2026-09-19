# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted target-machine baseline

Exact tested checkout:

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 16/16 PASS;
- continuous 3D fly-through accepted with verbose Newtonian/Assisted/PilotSkill metrics.

## Canonical block status from the original B0-B14 architecture

### Accepted / strong evidence

- **B0 Navigation World Snapshot** — authoritative static/dynamic publication and exact HitVolume usage are proven in Stage 12.
- **B8 Maneuver Acceptance / Program Store** — `AcceptedManeuverProgram` exists and is exercised by runtime maneuver tests.
- **B9 Maneuver Program Sampler** — explicit sampler exists and is tested.
- **B10 Bounded Tracking Controller** — explicit tracking controller exists and is tested through real physics.
- **B12 Pilot Skill Executor** — accepted component and exercised in all runtime maneuver tests.
- **B13 Propulsion Allocator + Physics** — authoritative `DynamicMotionSystem / SharedShipPhysics` accepted.
- **B14 Navigation Work Scheduler** — isolated deterministic scheduler gate is accepted and scale diagnostics remain green.

### Mechanically proven but production block still incomplete/generalization needed

- **B5 Physical Maneuver Compiler** — maneuver mechanics are strongly tested, but the ordinary production compiler still has incomplete Assisted/general-family coverage.
- **B6 Continuous Maneuver Prover** — exact-static / moving-passage proof components are strong, but proof ownership is not yet one fully generalized ordinary B6 block.
- **B8-B10 live replacement** — lab/runtime execution path is accepted, but final retirement of every transitional ordinary-live compatibility seam still belongs to final integration.

### Still open / transitional

- **B1 Shared Dynamic Broadphase / Influence Builder** — NavigationMap spatial hash works, but scene-wide sparse influence batching still needs completion.
- **B2 Navigation Objective** — semantic objective remains fragmented across goal/task types.
- **B3 Global / Topology Route Planner** — NavigationSpace base is accepted; vehicle/control-law-aware edge feasibility is incomplete.
- **B4 Local Route-Aligned Corridor Planner** — current LocalAvoidance ray-fan search is transitional; route-aligned corridor replacement/A-B proof remains.
- **B7 Maneuver Decision** — controller exists and unit tests pass, but the original architecture explicitly notes that ordinary live execution bypasses it. This is the current active gap.
- **B11 Execution Safety Monitor / Bounded Reflex** — replan/monitoring exists, explicit bounded-reflex API remains missing.

## Current unverified candidate: B7 speed/doctrine execution matrix

New test:

```
tests/navigation_runtime/ManeuverSpeedDoctrineMatrixTests.cpp
```

CTest:

```
maneuver_speed_doctrine_matrix
```

Expected navigation runtime suite size: **17 tests**.

Candidate code commits:
- `5294be208f6c48d29d2d1e00cb45b8d7c99045d8` — initial matrix;
- `3a51f6726196889b09a3782d8279971630536192` — fixture hardening;
- `ed3228fd3d8f30b6954a99b14b59076e9bcda2b7` — CMake registration;
- `83d41454c0cec924f8839356028579d3fe1a0ff6` — verbose runner diagnostics.

## B7 test design

All candidates solve the same 180 m local objective around one spherical obstacle.

Common initial state:
- position (0,0,0);
- velocity +X at 6 m/s;
- Cobra rigid hull;
- expert PilotSkill;
- real follower -> PilotSkill -> propulsion/physics execution.

Candidate set:
1. `precision` — largest offset/clearance, slow final speed;
2. `balanced` — balanced clearance/time/reserve;
3. `fast` — faster, tighter common-law path;
4. `newtonian_drift_dash` — fastest preferred Newtonian-only high-slip path;
5. `low_threat_escape` — lower threat exposure;
6. `reckless_shortcut` — fastest raw shortcut but criticalRisk=0.90.

The reckless shortcut is intentionally physically describable but outside the preferred critical-risk envelope. It must never win while a preferred-risk candidate exists.

Expected deterministic B7 choices:
- Rational -> balanced;
- PrecisionRetrieval -> precision;
- Extreme Newtonian -> newtonian_drift_dash;
- Extreme Assisted -> fast (Newtonian-only candidate filtered before ranking);
- CombatEscape -> low_threat_escape;
- reckless_shortcut -> rejected above doctrine by critical-risk policy.

Each selected `AcceptedManeuverProgram` is then executed through:
`TrajectoryFollower -> B10 -> PilotSkill -> SharedShipPhysics/DynamicMotionSystem`.

Strict execution checks:
- tracking-envelope exceeded ticks = 0;
- actual full-hull obstacle clearance > 0.25 m;
- final P <= 1.5 m;
- final V error <= 0.75 m/s;
- final forward error <= 5 deg;
- Newtonian drift dash must produce material slip >=20 deg.

## Next action

Run exact target-machine architecture + navigation runtime gate.

If green:
- accept B7 doctrine selection + selected-program execution at lab/runtime level;
- record actual time/clearance/speed/slip differences;
- next testing block becomes chained transitions + negative/limit cases, then final end-to-end proving ground.

If red:
- fix the first physical/selection defect;
- do not weaken doctrine, risk, clearance or tracking requirements merely to obtain green.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` **from scratch**.
