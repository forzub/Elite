# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — emergency contact severity gate

## Closed gate — continuous verifier performance

Target-machine run on `6f85436252d36e4b586ab496efe3aea45fb25e79`:

```text
NAVIGATION TRAJECTORY CONTINUOUS BENCHMARK CONTRACT: PASS
full_precision_batch8 p95 = 41.3527 us = 0.04135 ms
```

The static `ContinuousPassageTrajectoryEvaluator` is behavior/performance accepted and frozen. Do not optimize its math without contrary live-runtime evidence.

## Active candidate — `EmergencyContactSeverityScorer`

Public contract:

```text
src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md
```

Code:

```text
src/world/navigation/trajectory/EmergencyContactSeverityScorer.h
src/world/navigation/trajectory/EmergencyContactSeverityScorer.cpp
```

Test/architecture gate:

```text
tests/navigation_trajectory/NavigationTrajectoryEmergencyContactSeverityTests.cpp
tests/architecture_contracts/check_navigation_trajectory_emergency_contact_severity.py
```

### Ownership

The scorer does not discover collisions. It consumes already-predicted bounded contact witnesses and ranks at most:

```text
8 emergency candidates
4 contact witnesses per candidate
```

Exact narrow phase / CCD / TOI / manifold / impulse / ricochet remain physics authority.

### Physical contact metric

For each predicted witness:

```text
r = contactPoint - shipCenter
v_ship_contact = v_center + omega x r
v_rel = v_ship_contact - v_surface
v_n = max(0, -dot(v_rel, normalTowardFreeSpace))
```

Therefore hull rotation and moving surfaces affect emergency severity correctly at the witness level.

### Deterministic ranking

```text
1. no predicted contact beats contact
2. lower peak closing normal speed
3. lower summed normal-impact energy proxy
4. lower summed normal momentum proxy
5. more tangential / glancing incidence
6. lower geometry deficit
7. higher useful passage-axis progress
8. stable candidate id / input order
```

Impact severity outranks route progress. A more normal hit must never win merely because it advances farther through the gap.

### Pinned behavior

```text
glancing high-total-speed contact beats harder normal contact
omega x r contributes contact-point velocity
moving surface uses relative contact velocity
equal v_n uses effective-mass energy proxy as tie-break
no-contact candidate always wins
pure normal impact reports pi/2 incidence
zero normal / >8 candidates fail closed
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_emergency_contact_severity.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Expected suite after build:

```text
navigation_trajectory_passage
navigation_trajectory_gap
navigation_trajectory_reachability
navigation_trajectory_emergency_passage
navigation_trajectory_continuous_passage
navigation_trajectory_emergency_contact_severity
```

Send complete output.

## Next after green gate

1. behavior-accept and freeze the bounded severity scorer if the new contract + `6/6` CTest pass;
2. do not benchmark it unless composition/runtime evidence says its fixed `<=8 x <=4` work is material;
3. build time-varying contact/passage witnesses for moving obstacle gaps;
4. use the same relative-motion machinery for moving/rotating docking;
5. add explicit bottom-to-bottom terminal mating constraints;
6. add deterministic `PilotSkillProfile` execution;
7. integrate accepted Navigation v2 into live game/server/guidance;
8. run end-to-end stress/debug acceptance;
9. retire legacy route-wide navigation only after the live v2 path is stable.

## Definition of final success

Navigation v2 is finished only when the live runtime demonstrates normal flight, static/dynamic avoidance, narrow oriented passage, truthful Elite/Newton authority, least-severity unavoidable collision handling, post-impact replanning, moving/rotating docking, shared guidance/debug truth and intended NPC scaling without planner stalls or unbounded precision work.
