# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / continuous trajectory feasibility  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-TRAJECTORY-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; only the relevant subset is published into a translating working space with stable navigation/travel axes.

Accepted hybrid ownership remains:

```text
CPU: static free-space, portals, corridor search, deterministic precision/local work
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, shared conflict reduction
```

Legacy route-wide navigation remains migration/reference code. `RuckigTrajectorySolver` remains downstream kinematics, not free-space authority.

## Closed stages

### `NAV-V2-MAP-2` — CLOSED

Accepted target-machine evidence includes CPU compact queries below `0.2 ms p95` and GPU 10k heavy-scene totals below `1.7 ms p95`.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final target-machine turn-aware evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero-turn p95    8.4498 ms
open_10k turn-aware p95  12.0072 ms
hub_10k  zero-turn p95    8.4125 ms
hub_10k  turn-aware p95  11.9065 ms
turn portals examined    329,660
```

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Behavior, compact-candidate scaling and multiplied-probe cost are accepted. Fresh target-machine fan evidence on `e0817d157ba5d8c9c329576236310507bda13364`:

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

The `1024 x 17` case is deliberate stress and remains inside the `<1.0 ms normal peak` budget. Keep the deterministic 16-probe fan unchanged.

## `NAV-V2-TRAJECTORY-1` — ACTIVE

Architecture authorities:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
NAVIGATION_WORLD_V2.md
```

Accepted chain:

```text
ConflictHold / explicit aperture / docking corridor
        -> BoundedGapCandidateBuilder (<=8)
        -> OrientedPassageEvaluator
        -> AttitudeReachabilityEvaluator
        -> collision-free continuous candidate OR EmergencyPassageMitigator
```

### Oriented passage / bounded gap / reachability — ACCEPTED

Target-machine C++ behavior on `e0817d...`:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
```

Bounded-gap performance is accepted. Worst deliberate stress:

```text
top8_1024 p95 = 24.0699 us = 0.0241 ms
```

Do not micro-optimize this layer without contrary evidence.

### Emergency passage mitigation — ACCEPTED

Fresh target-machine gate on `b29a03d3d84f4d6575cbbc5166cbd7547b5ce0d8`:

```text
NAVIGATION TRAJECTORY BOUNDED GAP CONTRACT: PASS
NAVIGATION TRAJECTORY EMERGENCY PASSAGE CONTRACT: PASS

navigation_trajectory_passage              PASS
navigation_trajectory_gap                  PASS
navigation_trajectory_reachability         PASS
navigation_trajectory_emergency_passage    PASS

100% tests passed, 0 failed
Total Test time: 0.18 sec
```

Accepted project invariant:

```text
no collision-free proof != no navigation command
```

Priority is:

```text
safe collision-free maneuver
    -> stop before contact if physically possible
    -> otherwise least-severity non-safe contact mitigation
```

`EmergencyMitigatedContact` remains an explicit non-safe control intent: maximum useful braking, gap-center aim, desired travel along the passage axis and best physically reachable hull attitude. Physics/collision owns CCD/TOI/impulse/ricochet; damage owns consequences; navigation resumes from actual post-impact state.

## Continuous static passage verifier — PENDING TARGET-MACHINE GATE

Candidate:

```text
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h/.cpp
```

Detailed contract:

```text
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
```

The candidate verifies one complete static/extruded narrow-passage maneuver:

```text
translation: cubic Hermite, start P/V -> end P/V
attitude: shortest-arc smooth rest-to-rest rotation
```

The fixed partition is `33` poses / `32` intervals, but safety is **not** accepted from point samples alone. Each interval receives conservative continuous geometry bounds:

```text
center curve deviation <= M * dt^2 / 8
rotation sweep inflation <= 2 * R * sin(deltaTheta / 2)
```

This catches a ship whose endpoint poses both fit but whose hull hits the wall while rotating between them.

Physical vehicle gates include:

```text
forward acceleration
reverse/braking acceleration
lateral acceleration
vertical acceleration
angular speed
angular acceleration
```

Hermite acceleration is linear over each interval, so fixed-axis projection extrema are already at the endpoints. Extra continuous body-axis projection margin is added only for rotation of the body frame.

Control semantics are separate:

```text
Newtonian
    inertial velocity and hull attitude may diverge

EliteAssisted
    same truthful physical thrust limits
    plus a controller-policy velocity-to-forward slip bound
```

Result classes:

```text
Feasible
GeometryBlocked
LinearAuthorityExceeded
AngularAuthorityExceeded
AssistedSlipExceeded
InvalidInput
```

Pending target-machine gate:

```bash
python tests/architecture_contracts/check_navigation_trajectory_continuous_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

## Docking continuation

Docking remains the same trajectory domain, as terminal relative 6DoF pose/motion matching against a stationary/moving/rotating port:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
explicit mating frame / top-bottom convention
```

For a rotating port offset `r`:

```text
v_port = v_origin + omega x r
```

`bottom of ship -> bottom of dock` remains explicit. Normal docking goes around if capture is still avoidable; emergency contact semantics apply only when physical impact is actually unavoidable.

## Next order

1. run the continuous static-passage architecture/build/behavior gate;
2. if green, benchmark the fixed 33-sample / 32-interval verifier before optimizing it;
3. add emergency ranking by relative normal contact speed / impact-energy proxy so glancing contact beats normal impact;
4. generalize to moving/time-varying obstacle gaps;
5. reuse the moving-frame machinery for moving/rotating docking;
6. add deterministic NPC `PilotSkillProfile` execution;
7. pursuit and live game/server integration remain later.

Do not add vehicle velocity/braking/traffic/pursuit state to persistent `NavigationSpace` static cost.
