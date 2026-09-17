# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / moving continuous ship passage  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-TRAJECTORY-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`.

```text
CPU: static free-space / portals / corridor / deterministic precision work
GPU: dynamic P/V/A prediction / swept bounds / bins / shared conflict reduction
```

Legacy route-wide navigation remains migration/reference code. `RuckigTrajectorySolver` remains downstream kinematics, not free-space authority.

## Closed stages

```text
NAV-V2-MAP-2     CLOSED / ACCEPTED
NAV-V2-SPACE-1   CLOSED / ACCEPTED
NAV-V2-LOCAL-1   CLOSED / ACCEPTED
```

Reference accepted performance:

```text
NavigationSpace 10k turn-aware search      ~12 ms p95 (<40 ms worker gate)
LocalAvoidance 1024 x 17 stress            519.9286 us p95
BoundedGap top8_1024                        24.0699 us p95
Continuous static full_precision_batch8     41.3527 us p95
```

## `NAV-V2-TRAJECTORY-1` accepted chain

```text
ConflictHold / aperture / docking corridor
    -> BoundedGapCandidateBuilder <= 8
    -> OrientedPassageEvaluator
    -> AttitudeReachabilityEvaluator
    -> ContinuousPassageTrajectoryEvaluator
    -> collision-free command
       OR EmergencyPassageMitigator
            -> EmergencyContactSeverityScorer
```

Accepted emergency invariant:

```text
no collision-free proof != no navigation command
```

Exact runtime contact remains outside navigation:

```text
Physics / Collision
    CCD / TOI / manifold / impulse / ricochet

Damage / Structural
    actual consequence
```

## Moving gap — ACCEPTED

Target-machine gate:

```text
forzub/Elite@bc84940ff23209d9731a3d5332b8af3794182790
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
7/7 trajectory CTest PASS
```

`MovingGapPredictor` receives one already-selected pair and provides 33 time-varying gap/boundary states plus 32 continuous width/alignment proofs. Hidden between-sample gap closure is rejected. Boundary material velocity includes `v + omega x r`.

## Active candidate — moving continuous passage

```text
MovingPassageTrajectoryEvaluator
```

It consumes an accepted `MovingGapPredictor::Result` and one analytic ship segment.

The candidate proves:

```text
33 synchronized ship/gap states
32 continuous moving-passage intervals
moving clear-width lower bound
relative transverse center motion
passage-frame angular motion
relative OBB sweep
truthful body-axis linear/angular authority
Newtonian / EliteAssisted policy distinction
```

The evaluator performs no scene search and does not replace the physics collision solver.

Authority:

```text
src/world/navigation/MOVING_PASSAGE_TRAJECTORY_MODEL.md
```

## Remaining order

1. accept moving continuous ship-passage behavior;
2. moving/rotating docking-frame prediction;
3. terminal 6DoF capture + explicit bottom-to-bottom mating semantics;
4. deterministic NPC `PilotSkillProfile` execution;
5. live `EliteGame` / `EliteServer` / guidance integration;
6. end-to-end performance/debug/stress validation;
7. retire legacy route-wide navigation after v2 owns the stable live path.

## Final system acceptance

Navigation v2 is complete only when the live runtime demonstrates ordinary travel, static/dynamic avoidance, static and moving oriented passage, truthful Elite/Newton authority, least-severity unavoidable-contact behavior, replanning from real post-impact truth, moving/rotating docking, NPC-scale execution and guidance/debug driven by the same accepted navigation state.

No synchronous GPU readback or unbounded all-pairs precision search is allowed on the frame path.
