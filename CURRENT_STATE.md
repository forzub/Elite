# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — docking terminal 6DoF candidate (stage 9A)

## Progress

```text
[████████████████░░░░░░░░] 8 / 12 major stages closed

1  NavigationMap / mass dynamic P/V/A               ACCEPTED
2  NavigationSpace / global corridors               ACCEPTED
3  LocalHorizon / LocalAvoidance                     ACCEPTED
4  oriented passage / bounded gaps / attitude        ACCEPTED
5  continuous static passage                         ACCEPTED
6  emergency mitigation + contact severity           ACCEPTED
7  moving gap prediction                             ACCEPTED
8  continuous ship passage through moving gap        ACCEPTED
9  moving/rotating docking 6DoF                      ACTIVE
10 PilotSkillProfile                                 PENDING
11 live game/server/guidance + physics hookup         PENDING
12 end-to-end stress/debug + legacy retirement        PENDING
```

## Closed foundations

```text
NAV-V2-MAP-2       CLOSED / ACCEPTED
NAV-V2-SPACE-1     CLOSED / ACCEPTED
NAV-V2-LOCAL-1     CLOSED / ACCEPTED
```

Reference performance:

```text
NavigationMap CPU compact queries            <0.2 ms p95
NavigationMap GPU 10k heavy scene            <1.7 ms p95
NavigationSpace 10k turn-aware corridor      ~12 ms p95 (<40 ms worker gate)
LocalAvoidance deliberate 1024 x 17 stress   519.9286 us p95
BoundedGap top8_1024                          24.0699 us p95
Continuous static full_precision_batch8       41.3527 us p95 = 0.04135 ms
```

## Accepted trajectory components

```text
BoundedGapCandidateBuilder
OrientedPassageEvaluator
AttitudeReachabilityEvaluator
EmergencyPassageMitigator
ContinuousPassageTrajectoryEvaluator
EmergencyContactSeverityScorer
MovingGapPredictor
MovingPassageTrajectoryEvaluator
```

### Emergency invariant

```text
no collision-free proof != no navigation command
```

Priority remains collision-free maneuver -> stop before contact -> least-severity explicit non-safe mitigation. Exact CCD/TOI/manifold/impulse/ricochet remain physics authority.

### Moving gap — ACCEPTED

Target-machine gate:

```text
bc84940ff23209d9731a3d5332b8af3794182790
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
7/7 navigation_trajectory CTest PASS
```

### Moving continuous passage — ACCEPTED

Target-machine gate:

```text
18799ab2c026b6d6dce3da11f9225eae3a3c5f35
NAVIGATION TRAJECTORY MOVING PASSAGE CONTRACT: PASS
8/8 navigation_trajectory CTest PASS
100% tests passed
```

This closes the dynamic-gap traversal problem as an isolated navigation primitive: one accepted moving-gap prediction plus one concrete ship P/V/attitude/OBB/capability segment receives 33 synchronized samples and 32 conservative between-sample proofs. Exact collision response remains downstream.

## Active candidate — docking terminal 6DoF (stage 9A)

New component:

```text
DockingTerminalEvaluator
```

Authority:

```text
src/world/navigation/DOCKING_TERMINAL_MODEL.md
```

It evaluates one candidate ship terminal state against the predicted future dock-port frame.

Explicit port metadata:

```text
surface semantic
local port position
mating normal
referenceUp / rollReference
```

Current required pairing is explicit:

```text
Bottom(ship) -> Bottom(dock)
```

Capture gates:

```text
relative port position
relative port linear velocity
anti-aligned mating normals
aligned reference-up / roll
relative angular velocity
```

Moving/rotating port kinematics include:

```text
v_port = v_origin + omega x r
```

A 180-degree rolled arrival is rejected even when the two physical face normals are correctly opposed.

Pinned fixtures include stationary capture, translating carrier, rotating offset port, combined moving+rotating future capture, semantic mismatch, 180-degree roll rejection, relative angular-rate mismatch and position-tolerance failure.

## Collision ownership

```text
Navigation / docking trajectory
    prediction / feasibility / capture tolerance evidence

Physics / Collision
    exact broadphase + narrow phase / CCD / TOI / manifold / impulse / ricochet

Docking game state
    authoritative latch / capture transition

Damage / Structural
    actual impact consequences
```

Navigation does not become the collision solver.

## Next after stage 9A acceptance

1. stage 9B: continuous moving/rotating docking corridor + terminal convergence;
2. deterministic `PilotSkillProfile` execution;
3. live `EliteGame` / `EliteServer` / guidance integration and physics hookup;
4. end-to-end stress/debug/performance acceptance;
5. retire legacy route-wide navigation only after v2 owns the stable live path.

## Final completion criterion

Navigation v2 is complete only when the live runtime proves ordinary travel, static/dynamic avoidance, static and moving oriented passage, truthful Elite/Newton authority, least-severity unavoidable-contact commands, recovery from real post-impact state, stationary/moving/rotating docking to the correct mating frame, shared guidance/debug truth and intended NPC scaling without synchronous GPU waits or unbounded precision work.
