# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — docking stage 9B continuous approach candidate

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
   9A terminal 6DoF capture                          ACCEPTED
   9B continuous final docking approach              ACTIVE
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
DockingTerminalEvaluator
```

Accepted invariant:

```text
no collision-free proof != no navigation command
```

Exact CCD/TOI/manifold/impulse/ricochet remain physics authority.

## Moving gap — ACCEPTED

```text
bc84940ff23209d9731a3d5332b8af3794182790
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
7/7 navigation_trajectory CTest PASS
```

## Moving continuous passage — ACCEPTED

```text
18799ab2c026b6d6dce3da11f9225eae3a3c5f35
NAVIGATION TRAJECTORY MOVING PASSAGE CONTRACT: PASS
8/8 navigation_trajectory CTest PASS
```

## Docking stage 9A terminal capture — ACCEPTED

Target-machine gate:

```text
835271539619b7dd02efc54ff54df51d64b49fce
NAVIGATION TRAJECTORY DOCKING TERMINAL CONTRACT: PASS
9/9 navigation_trajectory CTest PASS
100% tests passed
```

Accepted terminal semantics:

```text
explicit Bottom(ship) -> Bottom(dock)
relative port position
relative port linear velocity
anti-aligned mating normals
aligned referenceUp / roll
relative angular velocity
v_port = v_origin + omega x r
```

A 180-degree rolled arrival is rejected even when mating normals are correct. Authoritative latch remains game/docking-state ownership; exact contact remains physics ownership.

The one compiler warning observed during the acceptance build (`normalizeOrZero` unused) has been removed while exposing the same dock-port prediction as a shared bounded helper for stage 9B.

## Active candidate — docking stage 9B continuous approach

New component:

```text
DockingApproachEvaluator
```

Authority:

```text
src/world/navigation/DOCKING_APPROACH_MODEL.md
```

Key architecture decision: the final precision segment is solved in the **moving/rotating dock-local frame**.

This gives:

```text
static corridor in dock-local coordinates
relative terminal pose fixed
relative terminal angular rate -> 0
therefore omega_ship(capture) = omega_dock
```

### Continuous geometry

The candidate deliberately reuses the already accepted:

```text
ContinuousPassageTrajectoryEvaluator
```

as a dock-local continuous geometry oracle:

```text
PassageSource::DockingCorridor
33 poses
32 continuous intervals
point samples alone are insufficient
```

Physical capability is not delegated to rotating-frame coordinates.

### Inertial translation authority

Dock-local relative motion is transformed back to world space. With constant dock angular velocity:

```text
v_world = v_port + omega x r + v_relative

a_world = a_port
        + omega x (omega x r)
        + 2 * omega x v_relative
        + a_relative
```

The Coriolis and centripetal terms therefore consume real ship thrust.

Between samples, body-axis acceleration receives a conservative world-jerk/projection bound rather than a sample-only test.

### Angular authority

Relative attitude is rest-to-rest in dock-local space. World bounds include dock rotation:

```text
omega_world_peak <= |omega_dock| + omega_relative_peak
alpha_world_peak <= alpha_relative_peak
                  + |omega_dock| * omega_relative_peak
```

A fast rotating station can therefore be physically undockable for a vehicle whose angular-rate authority is too small.

### Terminal composition

Only after continuous corridor + inertial vehicle authority pass is the final state submitted to accepted `DockingTerminalEvaluator`.

Successful result:

```text
FeasibleForCapture
```

requires both complete final-segment feasibility and terminal `Capturable`.

Pinned fixtures include stationary, translating, moving+rotating co-rotating capture, hidden between-sample corridor failure, inertial thrust failure, dock angular-rate failure, wrong 180-degree roll, terminal position mismatch and Elite/Newton distinction.

## Ownership boundary

```text
Navigation / docking trajectory
    continuous feasibility + terminal capture evidence

Flight control / thruster allocation
    execute accepted intent

Physics / Collision
    exact broadphase/narrow phase / CCD / TOI / manifold / impulse / ricochet

Docking game state
    authoritative latch/capture transition

Damage / Structural
    actual failed-contact consequences
```

## Next after stage 9B acceptance

1. close large docking stage 9;
2. deterministic `PilotSkillProfile` execution;
3. live `EliteGame` / `EliteServer` / guidance + physics hookup;
4. end-to-end stress/debug/performance acceptance;
5. retire legacy route-wide navigation only after v2 owns the stable live path.

## Final completion criterion

Navigation v2 is complete only when the live runtime proves ordinary travel, static/dynamic avoidance, static and moving oriented passage, truthful Elite/Newton authority, least-severity unavoidable-contact commands, recovery from real post-impact state, stationary/moving/rotating docking to the correct mating frame, shared guidance/debug truth and intended NPC scaling without synchronous GPU waits or unbounded precision work.
