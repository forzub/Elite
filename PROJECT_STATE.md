# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / local avoidance performance + trajectory precision fallback  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-LOCAL-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior domains publish only relevant subsets.

Legacy route-wide navigation remains migration code. `RuckigTrajectorySolver` remains downstream local kinematics.

Accepted hybrid ownership:

```text
CPU: static free-space, portals, corridor search, deterministic precision/local work
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, conflict reduction
```

Moving-goal pursuit is specified in `src/world/navigation/PURSUIT_HORIZON.md`; runtime pursuit remains later.

## `NAV-V2-MAP-2` — CLOSED

`NavigationMap` exposes compact candidate products and hides backend/GPU storage. Accepted target-machine evidence includes CPU compact queries below `0.2 ms p95` and GPU 10k heavy-scene totals below `1.7 ms p95`.

Mass/broadphase actor geometry intentionally remains center P/V/A + radius + conservative swept sphere.

## `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final target-machine turn-aware implementation on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero-turn p95    8.4498 ms
open_10k turn-aware p95  12.0072 ms
hub_10k  zero-turn p95    8.4125 ms
hub_10k  turn-aware p95  11.9065 ms
turn portals examined    329,660
```

Static turn search is closed.

## `NAV-V2-LOCAL-1` — behavior accepted / timing pending

Accepted compact-candidate scaling on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

Accepted `LocalAvoidancePlanner` behavior on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
```

The active local performance gate remains:

```text
benchmarks/navigation_local_avoidance/
```

Local CPU budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

## Trajectory precision fallback — prepared candidates, not yet accepted

Architecture authorities:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
```

Prepared chain:

```text
ConflictHold / explicit aperture / docking corridor
        |
        v
BoundedGapCandidateBuilder
    one primary conflict + reduced neighbors
    hard cap 8
    no all-pairs scan
        |
        v
OrientedPassageEvaluator
    O(1) real oriented hull fit
        |
        v
AttitudeReachabilityEvaluator
    O(1) angular timing + longitudinal room
        |
        v
later continuous 6DoF swept-body proof
```

### Oriented passage

`OrientedPassageEvaluator` performs constant-size OBB projection against an oriented passage cross-section. It recovers valid flat-ship/flat-slot cases that the conservative sphere broadphase intentionally rejects.

Passage sources:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

### Bounded emergent gap extraction

Project invariant: **two nearby obstacles may form a usable positive passage between them**.

`BoundedGapCandidateBuilder` examines one already-known primary conflict against already-reduced local neighbors. It does not generate neighbor-neighbor pairs and does not perform global `N x N` discovery.

The builder:

- caps output at 8 deterministic candidates;
- requires same snapshot revision;
- rejects overlapping conservative bounds;
- rejects front/back pairs masquerading as a transverse slot;
- bounds forward distance and centerline offset;
- feeds compact `ObstacleGap` products to `OrientedPassageEvaluator`.

Performance harness:

```text
benchmarks/navigation_trajectory_gap/
```

It measures rejection and top-8 maintenance at 16/64/256/1024 local neighbors. `1024` is deliberate stress.

### Attitude reachability before entry

A geometrically valid gap is not automatically a valid maneuver.

`AttitudeReachabilityEvaluator` checks whether the vehicle can become attitude-ready before reaching the passage entry using declared angular acceleration/rate authority and available longitudinal braking.

Statuses:

```text
AlreadyReady
ReachableCoast
ReachableWithBraking
UnreachableBeforeEntry
```

Pinned reference:

```text
90 deg roll
max angular acceleration = 90 deg/s^2
max angular speed        = 90 deg/s
minimum rest-to-rest time = 2 s
closing speed             = 10 m/s

30 m -> coast reachable
15 m -> braking-assisted reachable at 5 m/s^2
 8 m -> unreachable before entry
```

Current angular velocity is conservatively settled first and consumes extra time/margin.

The evaluator remains a precheck. It does not prove body-axis translation, continuous swept-body clearance, moving-gap persistence or final docking capture.

## Planned continuous 6DoF / docking fidelity

Next precision layer after isolated gates:

```text
oriented hull + attitude / angular state
body-axis or thruster acceleration authority
Elite-assisted versus Newtonian free flight
translation + rotation continuous swept-body proof
head-on / crossing maneuver feasibility
moving/time-varying obstacle gaps
moving/rotating terminal docking
explicit bottom-to-bottom mating-frame orientation
NPC PilotSkillProfile execution
```

Docking remains relative pose-and-motion matching. A rotating port requires matching predicted port pose, tangential velocity and angular motion at capture.

## Next order

1. run local avoidance fan benchmark on the target machine;
2. run passage/gap/reachability architecture + MinGW64 behavior gates;
3. run bounded-gap builder benchmark;
4. accept/fix those candidates from real evidence;
5. implement continuous 6DoF static-gap maneuver proof with body-axis control authority and Elite/Newton semantics;
6. extend the same machinery to moving gaps and moving/rotating docking;
7. pursuit and live game/server integration remain later.

Do not add vehicle velocity/braking/traffic/pursuit state to persistent `NavigationSpace` static cost.
