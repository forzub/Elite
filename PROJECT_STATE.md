# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
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
   9A terminal 6DoF capture       ACCEPTED
   9B continuous final approach   ACTIVE
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

Docking accepted terminal primitive:

```text
DockingTerminalEvaluator
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

Docking terminal 6DoF
    835271539619b7dd02efc54ff54df51d64b49fce
    NAVIGATION TRAJECTORY DOCKING TERMINAL CONTRACT: PASS
    9/9 trajectory CTest PASS
```

Accepted reference performance still includes:

```text
NavigationSpace 10k turn-aware search       ~12 ms p95 (<40 ms worker gate)
LocalAvoidance 1024 x 17 stress             519.9286 us p95
BoundedGap top8_1024                         24.0699 us p95
Continuous static full_precision_batch8      41.3527 us p95
```

## Docking stage 9A — ACCEPTED

`DockingTerminalEvaluator` makes capture an explicit relative interface-frame problem rather than center-point arrival.

Port metadata:

```text
surface semantic
local port offset
mating normal
referenceUp / rollReference
```

Current required semantics:

```text
Bottom(ship) -> Bottom(dock)
```

Capture requires relative P/V/attitude/angular rate within tolerance. Rotating offset-port velocity includes `v_origin + omega x r`. A 180-degree rolled approach is rejected even with correct mating normals.

## Active docking stage 9B — continuous final approach

```text
DockingApproachEvaluator
src/world/navigation/DOCKING_APPROACH_MODEL.md
```

The final precision docking segment is solved in the moving/rotating **dock-local frame**.

This is a key physical decision:

```text
relative terminal attitude rate -> 0
therefore world omega_ship(capture) = omega_dock
```

### Continuous corridor

The accepted `ContinuousPassageTrajectoryEvaluator` is reused as a dock-local geometry oracle:

```text
PassageSource::DockingCorridor
33 samples
32 conservative continuous intervals
```

### Inertial physical authority

Physical capability is checked after transforming relative motion back to world space:

```text
v_world = v_port + omega x r + v_relative

a_world = a_port
        + omega x (omega x r)
        + 2 * omega x v_relative
        + a_relative
```

Thus centripetal and Coriolis acceleration consume real vehicle authority.

Between samples, forward/reverse/lateral/vertical thrust projection receives a conservative jerk + body-axis-rotation margin.

### Angular authority

```text
world omega peak bound
    = |omega_dock| + relative omega peak

world alpha peak bound
    = relative alpha peak
    + |omega_dock| * relative omega peak
```

A dock may therefore rotate too quickly for a specific vehicle to capture safely.

### Terminal composition

Only after continuous corridor and physical authority pass does the candidate call the accepted `DockingTerminalEvaluator`.

Successful result:

```text
FeasibleForCapture
```

means both final-segment feasibility and terminal 6DoF capture are proven under the declared model.

## Stage 9B first gate status

The first target-machine run is **not accepted**. Build succeeded and the existing 1–9 tests stayed green, but the new 10th behavior test failed because its supposed between-sample collision fixture did not actually leave the corridor. Exact analysis showed the true Hermite peak was `0.9622504486 m` while the fixture allowed `0.975 m` of center travel.

The fixture now allows `0.9618 m`: all 33 samples still fit (`+0.0004962891 m` minimum sampled clearance), while the true curve is outside (`-0.0004504486 m`) and the existing conservative interval proof is expected to reject it. The documentation gate was also hardened against capitalization-only Markdown differences. A fresh 10/10 target-machine rerun is required before closing stage 9.

## Collision / docking-state boundary

```text
Navigation / docking trajectory
    predicts target frame and proves corridor/terminal feasibility

Flight control / thruster allocation
    executes accepted intent

Physics / Collision
    authoritative broadphase/narrow phase / CCD / TOI / manifold / impulse / ricochet

Docking game state
    authoritative capture/latch transition

Damage / Structural
    consequences of real failed contacts
```

Ordinary docking must abort/go around when capture conditions are not met; destructive contact is never a successful capture.

## Next order

1. target-machine architecture/build/behavior gate for stage 9B (`10/10` expected);
2. close major docking stage 9;
3. deterministic NPC `PilotSkillProfile`;
4. live game/server/guidance + collision-physics integration;
5. end-to-end stress/debug/performance acceptance;
6. retire legacy route-wide navigation only after v2 owns the stable live path.

## Final acceptance

Navigation v2 is complete only when the live runtime demonstrates ordinary travel, static/dynamic avoidance, static and moving oriented passage, truthful Elite/Newton authority, least-severity unavoidable-contact behavior, replanning from real post-impact truth, moving/rotating docking to the correct explicit mating frame, NPC-scale execution, and guidance/debug driven by the same accepted navigation state.

No synchronous GPU readback or unbounded all-pairs precision search is allowed on the frame path.
