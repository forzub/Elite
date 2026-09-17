# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — moving continuous passage candidate

## Closed foundations

```text
NAV-V2-MAP-2       CLOSED / ACCEPTED
NAV-V2-SPACE-1     CLOSED / ACCEPTED
NAV-V2-LOCAL-1     CLOSED / ACCEPTED
```

Accepted reference performance remains:

```text
NavigationMap CPU compact queries            <0.2 ms p95
NavigationMap GPU 10k heavy scene            <1.7 ms p95
NavigationSpace 10k turn-aware corridor      ~12 ms p95 (<40 ms worker gate)
LocalAvoidance deliberate 1024 x 17 stress   519.9286 us p95
```

## `NAV-V2-TRAJECTORY-1` accepted components

```text
BoundedGapCandidateBuilder
OrientedPassageEvaluator
AttitudeReachabilityEvaluator
EmergencyPassageMitigator
ContinuousPassageTrajectoryEvaluator
EmergencyContactSeverityScorer
MovingGapPredictor
```

### Static precision chain

Bounded gap extraction is accepted at:

```text
top8_1024 p95 = 24.0699 us
```

Continuous static passage is behavior/performance accepted and frozen:

```text
full_precision_batch8 p95 = 41.3527 us = 0.04135 ms
```

### Emergency behavior

Accepted invariant:

```text
no collision-free proof != no navigation command
```

Priority:

```text
collision-free maneuver
    -> stop before contact if possible
    -> otherwise least-severity non-safe contact mitigation
```

`EmergencyContactSeverityScorer` was accepted on `7f1bccd4e8b91c72e4fc5f9e6d1329260790aa8e` with architecture contract PASS and `6/6` trajectory CTest PASS. It remains bounded to `<=8 candidates x <=4 witnesses` and ranks relative contact-point normal motion including `omega x r` and moving-surface velocity. Exact CCD/TOI/manifold/impulse/ricochet remain physics authority.

### Moving gap prediction — ACCEPTED

Target-machine gate on:

```text
bc84940ff23209d9731a3d5332b8af3794182790
```

Evidence:

```text
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
7/7 navigation_trajectory CTest PASS
100% tests passed
```

`MovingGapPredictor` is therefore accepted/frozen as the bounded pair predictor.

It receives one already-selected obstacle pair, uses compact P/V/A + angular velocity + conservative radius, and publishes 33 synchronized time states plus 32 continuous gap-width/alignment proofs. Hidden between-sample closure is rejected. Boundary surface material velocity includes `v + omega x r`.

## Active candidate — moving continuous ship passage

New component:

```text
MovingPassageTrajectoryEvaluator
```

Authority:

```text
src/world/navigation/MOVING_PASSAGE_TRAJECTORY_MODEL.md
```

It consumes one accepted `MovingGapPredictor::Result` plus one analytic ship segment and verifies:

```text
33 synchronized ship/gap poses
32 continuous intervals
moving gap width
transverse relative ship/gap-center motion
passage-frame rotation
oriented hull sweep
body-axis linear authority
angular authority
Newtonian vs EliteAssisted semantics
```

Important ownership boundary:

```text
Navigation
    predictive collision-free proof / risk witness

Physics / Collision
    exact broadphase/narrow phase / CCD / TOI / manifold / impulse / ricochet

Damage / Structural
    actual consequences
```

Navigation does not become the authoritative collision solver.

## Remaining before live integration

1. accept `MovingPassageTrajectoryEvaluator`;
2. moving/rotating docking frame prediction + terminal 6DoF capture constraints;
3. explicit bottom-to-bottom mating semantics;
4. deterministic `PilotSkillProfile` execution;
5. live `EliteGame` / `EliteServer` / guidance integration;
6. end-to-end stress/debug/performance acceptance;
7. retire legacy route-wide navigation only after v2 owns the stable live path.

## Final completion criterion

Navigation v2 is complete only when the live runtime proves ordinary travel, static/dynamic avoidance, static and moving oriented passage, truthful Elite/Newton authority, least-severity unavoidable-contact commands, recovery from real post-impact state, stationary/moving/rotating docking, shared guidance/debug truth and intended NPC scaling without synchronous GPU waits or unbounded precision work.
