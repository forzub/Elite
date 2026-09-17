# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — continuous static passage feasibility candidate

## Closed foundations

### `NAV-V2-MAP-2` — CLOSED

Accepted target-machine evidence includes CPU compact candidate queries below `0.2 ms p95` and GPU 10k heavy-scene totals below `1.7 ms p95`.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final accepted turn-aware evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k zero p95     8.4125 ms
hub_10k turn p95    11.9065 ms
turn portals examined 329,660
```

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Fresh multiplied-probe target-machine evidence on `e0817d157ba5d8c9c329576236310507bda13364`:

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

The `1024 x 17` case is deliberate stress and remains inside the `<1.0 ms normal peak` budget. The deterministic 16-probe fan remains unchanged.

## `NAV-V2-TRAJECTORY-1` — active precision/control layer

Architecture authorities:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
NAVIGATION_WORLD_V2.md
```

Accepted isolated chain so far:

```text
ConflictHold / explicit aperture / docking corridor
        |
        v
BoundedGapCandidateBuilder
        |
        v
OrientedPassageEvaluator
        |
        v
AttitudeReachabilityEvaluator
        |
        +-- collision-free entry attitude reachable
        |      -> continuous passage proof candidate
        |
        +-- not reachable
               v
EmergencyPassageMitigator
```

### Oriented passage / gap / reachability — ACCEPTED

Target-machine behavior suite on `e0817d...`:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
```

Bounded gap performance is accepted. Worst measured stress:

```text
top8_1024 p95 = 24.0699 us = 0.0241 ms
```

Do not micro-optimize this builder without contrary runtime evidence.

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

Accepted emergency semantic:

```text
safe route unavailable
    != navigation disabled
```

Priority remains:

```text
safe collision-free maneuver
    -> stop before contact if physically possible
    -> otherwise keep navigation/control active and minimize the unavoidable hit
```

`EmergencyMitigatedContact` is an explicitly non-safe but valid control intent:

```text
maximum useful braking
+ aim toward gap center
+ desired travel along passage axis
+ best physically reachable hull attitude
+ contact / ricochet expected
```

Actual contact impulse, ricochet, damage, detach and post-impact truth remain physics/damage authority.

## New continuous passage candidate — PENDING TARGET-MACHINE GATE

New code:

```text
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.cpp
```

New contract:

```text
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
```

Purpose: verify one complete bounded static-passage maneuver rather than only entry pose.

Candidate analytic motion:

```text
center: cubic Hermite
    start P/V -> end P/V

attitude: shortest-arc smooth rotation
    s(u) = 3u^2 - 2u^3
```

Continuous geometry is not accepted from point samples alone. The fixed `33` pose partition is supplemented on every interval with conservative bounds for:

```text
center-curve deviation <= M * dt^2 / 8
oriented-hull rotational inflation <= 2 * R * sin(deltaTheta / 2)
```

Therefore both endpoint poses may fit while an intermediate rotating hull still causes `GeometryBlocked`.

Vehicle capability gates:

```text
forward acceleration
reverse / braking acceleration
lateral acceleration
vertical acceleration
angular acceleration
angular speed
```

Required map-space acceleration is projected into body axes and conservatively bounded between samples.

Control semantics:

```text
Newtonian
    velocity and hull attitude may diverge

EliteAssisted
    same physical thrust limits
    plus controller-policy velocity-to-forward slip-angle bound
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

This first continuous slice is static/extruded-passage only. Moving gaps, arbitrary angular-rate synthesis, impact-energy ranking and moving/rotating docking remain later.

## Docking remains part of the same trajectory layer

Docking remains terminal 6DoF pose/motion matching against a possibly moving/rotating port:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
explicit mating frame / top-bottom convention
```

`bottom of ship -> bottom of dock` remains explicit. A rotating port includes:

```text
v_port = v_origin + omega x r
```

## Immediate next step

Target-machine gate for the new continuous candidate:

```bash
python tests/architecture_contracts/check_navigation_trajectory_continuous_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

If green: benchmark the continuous verifier, then add emergency ranking by relative normal contact speed / impact-energy proxy and extend the same time-varying pose machinery toward moving gaps/docking.
