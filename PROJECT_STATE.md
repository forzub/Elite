# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / trajectory precision + emergency impact mitigation  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-TRAJECTORY-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior domains publish only relevant subsets.

Legacy route-wide navigation remains migration code. `RuckigTrajectorySolver` remains downstream local kinematics.

Accepted hybrid ownership:

```text
CPU: static free-space, portals, corridor search, deterministic precision/local work
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, conflict reduction
```

Moving-goal pursuit remains later.

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

## `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Behavior and compact-candidate scaling were already accepted. Fresh multiplied-probe target-machine evidence on `e0817d157ba5d8c9c329576236310507bda13364`:

```text
scenario                     p95_us
nominal_clear_64               1.8333
early_adjust_64                4.2795
all_static_rejected_64         4.2655
all_dynamic_rejected_16       12.9500
all_dynamic_rejected_64       35.8900
all_dynamic_rejected_256     128.9708
all_dynamic_rejected_1024    519.9286
```

The `1024 x 17` case is deliberate stress and still fits the `<1.0 ms normal peak` local budget. The deterministic 16-probe fan is retained unchanged.

Authority:

```text
benchmarks/navigation_local_avoidance/RUN_LOG.md
```

## `NAV-V2-TRAJECTORY-1` — active isolated precision stage

Architecture authorities:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
NAVIGATION_WORLD_V2.md
```

Current chain:

```text
ConflictHold / explicit aperture / docking corridor
        |
        v
BoundedGapCandidateBuilder
    primary conflict x reduced neighbors
    hard cap 8
    no all-pairs scan
        |
        v
OrientedPassageEvaluator
    oriented OBB entry fit
        |
        v
AttitudeReachabilityEvaluator
    collision-free requested attitude reachable in time?
        |
        +-- yes -> later continuous safe 6DoF proof
        |
        +-- no
              v
EmergencyPassageMitigator
    brake / center / passage-axis intent
    best reachable hull attitude
    stop if possible
    otherwise explicit mitigated contact/ricochet intent
```

### Target-machine trajectory evidence

On `e0817d...`:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
Total Test time: 0.12 sec
```

The one bounded-gap architecture failure was only a stale exact Markdown marker; runtime behavior was green. The checker has been repaired on current `main`.

### Bounded gap performance — ACCEPTED

Target-machine p95:

```text
reject_16       0.2459 us
reject_64       0.7550 us
reject_256      4.6499 us
reject_1024    11.6730 us

top8_16         0.7883 us
top8_64         1.8260 us
top8_256        6.3609 us
top8_1024      24.0699 us
```

Even the deliberate `top8_1024` stress case is only `0.0241 ms p95`. Gap extraction is performance-accepted.

Authority:

```text
benchmarks/navigation_trajectory_gap/RUN_LOG.md
```

## Emergency collision semantic

Project invariant:

```text
no collision-free route != no navigation command
```

If a collision-free passage cannot be achieved in time:

1. stop before contact when physically possible;
2. otherwise keep maximum useful braking;
3. aim at the gap center;
4. bias travel along the passage axis to reduce side-normal incidence;
5. rotate to the best physically reachable hull attitude;
6. allow explicit contact/ricochet as a least-severity emergency outcome.

`EmergencyMitigatedContact` is never equivalent to `Clear`. Physics/collision/damage remain authoritative for the actual impact and post-contact state.

Prepared isolated implementation:

```text
src/world/navigation/trajectory/EmergencyPassageMitigator.h/.cpp
tests/navigation_trajectory/NavigationTrajectoryEmergencyPassageTests.cpp
tests/architecture_contracts/check_navigation_trajectory_emergency_passage.py
```

The first version uses a fixed 17-sample reachable orientation arc and ranks by passage clearance/geometric deficit. Continuous 6DoF must later add translational reachability and relative normal contact-speed / impact-energy scoring.

## Planned continuous 6DoF / docking fidelity

Next precision layer:

```text
authoritative body-axis linear thrust/braking authority
Elite-assisted versus Newtonian free flight
position + velocity + attitude + angular-state propagation
continuous swept oriented-body proof
safe head-on / crossing / narrow-gap maneuvers
emergency contact ranking by relative normal speed / impact proxy
moving/time-varying obstacle gaps
moving/rotating terminal docking
explicit bottom-to-bottom mating-frame orientation
NPC PilotSkillProfile execution
```

Docking remains relative pose-and-motion matching. A rotating port requires matching predicted port pose, tangential velocity and angular motion at capture. Normal docking should go around rather than intentionally collide when avoidance remains possible.

## Next order

1. run repaired bounded-gap architecture checker + new emergency-passage architecture/behavior gate;
2. if green, implement continuous bounded 6DoF static-gap feasibility with explicit `Elite`/`Newton` authority;
3. add emergency impact ranking by predicted relative normal speed/energy rather than geometry alone;
4. extend the same time-varying pose/sweep machinery to moving gaps and moving/rotating docking;
5. add deterministic NPC execution skill;
6. pursuit and live game/server integration remain later.

Do not add vehicle velocity/braking/traffic/pursuit state to persistent `NavigationSpace` static cost.
