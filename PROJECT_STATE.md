# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / moving & rotating docking 6DoF  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-TRAJECTORY-1`

## Progress

```text
[████████████████░░░░░░░░] 8 / 12 major stages closed
```

Closed:

```text
1  NavigationMap / mass dynamic state
2  NavigationSpace / global corridors
3  LocalHorizon / LocalAvoidance
4  oriented passages / bounded gaps / attitude reachability
5  continuous static passage
6  emergency mitigation / contact severity
7  moving gap prediction
8  moving continuous oriented-hull passage
```

Active:

```text
9  moving/rotating docking 6DoF
   9A terminal capture candidate
   9B continuous docking approach after 9A acceptance
```

Remaining after docking:

```text
10 deterministic PilotSkillProfile
11 live EliteGame / EliteServer / guidance + physics hookup
12 end-to-end stress/debug/performance + legacy retirement
```

## Architecture

Navigation v2 uses one shared ship-centered `NavigationWorld`.

```text
CPU: static free-space / portals / cached corridors / deterministic precision work
GPU: mass P/V/A prediction / swept bounds / bins / shared conflict reduction
```

Mass dynamic work is shared. Global corridors are cached/event-driven. Precision trajectory/docking work is bounded and conditional; no `N ships x full pathfinding x every tick` design is allowed.

Legacy route-wide navigation remains migration/reference code. `RuckigTrajectorySolver` remains downstream kinematics rather than free-space authority.

## Accepted trajectory chain

```text
BoundedGapCandidateBuilder <= 8
    -> OrientedPassageEvaluator
    -> AttitudeReachabilityEvaluator
    -> ContinuousPassageTrajectoryEvaluator
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
```

Emergency fallback:

```text
EmergencyPassageMitigator
    -> EmergencyContactSeverityScorer
```

Accepted invariant:

```text
no collision-free proof != no navigation command
```

## Target-machine evidence

```text
Emergency contact severity
    7f1bccd4e8b91c72e4fc5f9e6d1329260790aa8e
    6/6 trajectory CTest PASS

Moving gap
    bc84940ff23209d9731a3d5332b8af3794182790
    NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
    7/7 trajectory CTest PASS

Moving passage
    18799ab2c026b6d6dce3da11f9225eae3a3c5f35
    NAVIGATION TRAJECTORY MOVING PASSAGE CONTRACT: PASS
    8/8 trajectory CTest PASS
```

Accepted reference performance still includes:

```text
NavigationSpace 10k turn-aware search       ~12 ms p95 (<40 ms worker gate)
LocalAvoidance 1024 x 17 stress             519.9286 us p95
BoundedGap top8_1024                         24.0699 us p95
Continuous static full_precision_batch8      41.3527 us p95
```

## Active docking terminal candidate — stage 9A

```text
DockingTerminalEvaluator
src/world/navigation/DOCKING_TERMINAL_MODEL.md
```

Docking is an explicit terminal relative 6DoF problem. Ship and dock interfaces carry:

```text
surface semantic
local port position
mating normal
referenceUp / rollReference
```

Current required semantic pairing:

```text
Bottom(ship) -> Bottom(dock)
```

At candidate capture time the evaluator checks:

```text
relative port position
relative port linear velocity
anti-aligned mating normals
aligned roll/reference-up
relative angular velocity
```

Moving/rotating port kinematics include:

```text
v_port = v_origin + omega x r
```

A 180-degree rolled arrival is rejected despite correct opposing face normals.

## Collision / docking-state boundary

```text
Navigation / docking trajectory
    predicts target frame and proves terminal/corridor feasibility

Physics / Collision
    authoritative broadphase/narrow phase / CCD / TOI / manifold / impulse / ricochet

Docking game state
    authoritative capture/latch transition

Damage / Structural
    consequences of real failed contacts
```

Ordinary docking must abort/go around when capture conditions are not met; destructive contact is never a successful capture.

## Next order

1. target-machine architecture/build/behavior gate for stage 9A (`9/9` expected);
2. stage 9B continuous moving/rotating docking corridor and terminal convergence;
3. deterministic NPC `PilotSkillProfile`;
4. live game/server/guidance + collision-physics integration;
5. end-to-end stress/debug/performance acceptance;
6. retire legacy route-wide navigation only after v2 owns the stable live path.

## Final acceptance

Navigation v2 is complete only when the live runtime demonstrates ordinary travel, static/dynamic avoidance, static and moving oriented passage, truthful Elite/Newton authority, least-severity unavoidable-contact behavior, replanning from real post-impact truth, moving/rotating docking to the correct explicit mating frame, NPC-scale execution, and guidance/debug driven by the same accepted navigation state.

No synchronous GPU readback or unbounded all-pairs precision search is allowed on the frame path.
