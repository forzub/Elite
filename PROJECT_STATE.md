# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / continuous trajectory performance  
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
```

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Behavior, compact-candidate scaling and multiplied-probe cost are accepted. Deliberate `1024 x 17` stress remains below the `<1.0 ms normal peak` budget. Keep the deterministic 16-probe fan unchanged.

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
        -> ContinuousPassageTrajectoryEvaluator
        -> collision-free command
           OR EmergencyPassageMitigator when safe motion is already too late
```

### Bounded gap — ACCEPTED

Worst deliberate stress:

```text
top8_1024 p95 = 24.0699 us = 0.0241 ms
```

### Emergency passage mitigation — ACCEPTED

Target-machine gate on `b29a03d3d84f4d6575cbbc5166cbd7547b5ce0d8` passed both architecture contracts and `4/4` trajectory CTest.

Project invariant:

```text
no collision-free proof != no navigation command
```

`EmergencyMitigatedContact` remains explicitly non-safe. Physics/collision owns CCD/TOI/impulse/ricochet; damage owns consequences; navigation resumes from actual post-impact state.

### Continuous static passage — BEHAVIOR ACCEPTED

Fresh target-machine gate on `574a2e98fd7a75ebf562bbfa476fba367fb888d1`:

```text
NAVIGATION TRAJECTORY CONTINUOUS PASSAGE CONTRACT: PASS
5/5 navigation_trajectory CTest PASS
100% tests passed, 0 failed
Total Test time: 0.24 sec
```

The verifier proves one bounded static/extruded passage segment using:

```text
cubic Hermite center translation
shortest-arc smoothstep attitude
33 pose samples
32 conservative continuous interval proofs
```

It does not rely on point samples alone. Each interval bounds center-curve deviation and oriented-hull rotational sweep. Physical feasibility separately checks forward/reverse/lateral/vertical acceleration and angular speed/acceleration.

Control semantics remain distinct:

```text
Newtonian
    inertial velocity and hull attitude may diverge

EliteAssisted
    same truthful physical thrust limits
    plus controller-policy velocity-to-forward slip bound
```

## Continuous verifier performance gate — ACTIVE

Harness:

```text
benchmarks/navigation_trajectory_continuous/
```

Timed full-path scenarios:

```text
straight_newton
rolled_newton
lateral_newton
elite_aligned
geometry_blocked_roll
full_precision_batch8
```

The eight-query batch mirrors the upstream hard `<=8` surviving precision-candidate ceiling.

Decision rule:

```text
batch8 p95 < 0.5 ms  -> performance-accept and freeze verifier
batch8 p95 < 1.0 ms  -> acceptable peak; inspect work scheduling/order
batch8 p95 >= 1.0 ms -> optimize/budget before live integration
```

Do not optimize the continuous math before this target-machine measurement.

## Docking continuation

Docking remains terminal relative 6DoF pose/motion matching against a stationary/moving/rotating port:

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

`bottom of ship -> bottom of dock` remains explicit. Normal docking goes around if safe capture is still possible; emergency contact semantics apply only when physical impact is actually unavoidable.

## Next order

1. run the continuous verifier benchmark contract + target-machine timing;
2. record/accept measured cost before changing the verifier;
3. add emergency ranking by relative normal contact speed / impact-energy proxy so glancing contact beats normal impact;
4. generalize to moving/time-varying obstacle gaps;
5. reuse moving-frame trajectory machinery for moving/rotating docking;
6. add deterministic NPC `PilotSkillProfile` execution;
7. pursuit and live game/server integration remain later.

Do not add vehicle velocity/braking/traffic/pursuit state to persistent `NavigationSpace` static cost.
