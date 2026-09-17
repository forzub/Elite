# Navigation v2 — continuous static passage trajectory model

**Status:** behavior/architecture/performance accepted; static reference frozen  
**Updated:** 2026-09-17 Europe/Kyiv  
**Stage:** `NAV-V2-TRAJECTORY-1`  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/world/navigation/ORIENTED_PASSAGE_MODEL.md`

## Purpose

The earlier trajectory slices answer narrower questions:

```text
OrientedPassageEvaluator
    does the hull fit one passage cross-section at one pose?

AttitudeReachabilityEvaluator
    can a requested entry attitude be reached before the entry plane?

EmergencyPassageMitigator
    if collision-free entry cannot be reached, keep a best-effort command alive
```

The continuous verifier answers:

```text
Can one complete bounded translation + rotation segment be executed
with the declared vehicle authority while the oriented hull stays
inside a static narrow passage for the whole interval?
```

Accepted implementation:

```text
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.cpp
```

## Target-machine behavior acceptance

Accepted on canonical public commit:

```text
574a2e98fd7a75ebf562bbfa476fba367fb888d1
```

Evidence:

```text
NAVIGATION TRAJECTORY CONTINUOUS PASSAGE CONTRACT: PASS

navigation_trajectory_passage               PASS
navigation_trajectory_gap                   PASS
navigation_trajectory_reachability          PASS
navigation_trajectory_emergency_passage     PASS
navigation_trajectory_continuous_passage    PASS

100% tests passed, 0 failed
Total Test time: 0.24 sec
```

## Target-machine performance acceptance

Dedicated benchmark authority:

```text
benchmarks/navigation_trajectory_continuous/RUN_LOG.md
```

Performance run used:

```text
forzub/Elite@6f85436252d36e4b586ab496efe3aea45fb25e79
MSYS2 MinGW64 / g++ 15.2.0 / Release / Ninja
```

Measured p95:

```text
straight_newton                4.0458 us
rolled_newton                  6.5235 us
lateral_newton                 4.1593 us
elite_aligned                  4.1820 us
geometry_blocked_roll          6.5133 us
full_precision_batch8         41.3527 us
```

Primary integration signal:

```text
full_precision_batch8 p95 = 41.3527 us = 0.04135 ms
```

The acceptance rule was `<0.5 ms typical` for the full eight-candidate precision batch. Result: **performance accepted with large margin**.

Decision: freeze the static continuous verifier. Do not micro-optimize it without contrary live-runtime evidence.

## Analytic segment

Center motion uses a cubic Hermite segment from:

```text
start position + start linear velocity
        ->
end position + end linear velocity
```

over a declared duration.

Attitude follows the shortest orientation arc with smooth rest-to-rest timing:

```text
s(u) = 3u^2 - 2u^3
```

This gives exact analytic peak angular requirements for the candidate orientation segment:

```text
omega_peak = 1.5 * angle / T
alpha_peak = 6.0 * angle / T^2
```

The evaluator verifies the segment; it is not a thruster allocator or a replacement flight controller.

## Continuous geometry proof

`kPoseSamples = 33` is a bounded partition, but geometry acceptance is **not** based only on those point poses.

For every interval the verifier computes a conservative continuous clearance bound.

### Center-curve deviation

Cubic-Hermite acceleration is linear inside each interval. For a fixed passage-plane axis and an acceleration bound `M`, deviation of the real center curve from the endpoint chord is bounded by:

```text
centerDeviation <= M * dt^2 / 8
```

This is evaluated independently for passage right and passage up.

### Oriented-hull sweep

For hull rotation `deltaTheta` inside an interval, any hull point moves by at most:

```text
rotationInflation <= 2 * R * sin(deltaTheta / 2)
```

where `R` is a conservative hull rotation radius.

Continuous width/height clearance subtracts:

```text
worst endpoint center offset
+ worst endpoint projected OBB half-extent
+ center-curve deviation bound
+ rotational sweep inflation
```

A segment is geometrically accepted only if the resulting bound stays non-negative across every interval.

This deliberately catches a case where both endpoint poses fit but the hull clips a wall while rotating between them.

## Body-axis linear authority

The Hermite segment defines a required map-space acceleration at every time.

The verifier projects that requirement onto the current body axes and checks declared physical authority separately:

```text
forward acceleration
reverse/braking acceleration
lateral acceleration
vertical acceleration
```

Cubic-Hermite acceleration is linear inside each interval. Therefore, for a **fixed** body axis, the extrema of acceleration projection are already contained by the interval endpoints. Extra continuous projection margin is added only for rotation of the body axis itself:

```text
axisRotationProjectionMargin
    <= maxAccelerationMagnitude * 2 * sin(deltaTheta / 2)
```

This avoids inventing cross-axis thrust demand when only another acceleration component changes while the hull frame is fixed.

Navigation consumes these limits from the authoritative flight/physics capability boundary; it must not invent stronger thrust.

## Angular authority

The analytic orientation segment is rejected when either exact peak requirement exceeds supplied vehicle capability:

```text
AngularAuthorityExceeded
```

The accepted static slice assumes the selected segment starts and ends on the smooth shortest-arc attitude law. Arbitrary nonzero initial angular-rate synthesis remains later trajectory work.

## `Elite` versus `Newton`

Physical thrust limits remain truthful in both modes.

```text
Newtonian
    velocity and hull attitude may diverge
    no artificial velocity-to-nose coupling is imposed

EliteAssisted
    controller may impose a maximum velocity-to-forward slip angle
    this is a policy constraint, not extra thrust
```

## Result classes

```text
Feasible
GeometryBlocked
LinearAuthorityExceeded
AngularAuthorityExceeded
AssistedSlipExceeded
InvalidInput
```

`Feasible` means the entire supplied analytic segment passed the static extruded-passage geometry bound plus declared vehicle-authority gates. It does not yet prove moving-obstacle or moving-dock safety.

## Relationship to emergency contact

A continuous collision-free candidate is preferred.

If no collision-free continuous segment survives and stopping is impossible, `EmergencyPassageMitigator` remains the command fallback. The next bounded layer is now explicitly separated:

```text
EmergencyContactSeverityScorer
```

Authority:

```text
src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md
```

That scorer ranks already-predicted unavoidable contacts by contact-point relative normal speed and impact proxies. It does **not** change this accepted static collision-free verifier and does not own CCD/TOI/contact response.

`EmergencyMitigatedContact` remains explicitly non-safe; actual contact response and damage stay physics/damage authority.

## Behavior fixtures

Target-machine runner:

```text
tests/navigation_trajectory/run_mingw64.sh
```

Fixture executable:

```text
navigation_trajectory_continuous_passage_tests
```

Pinned cases:

```text
straight centered segment
    -> Feasible

both endpoint poses fit but intermediate roll clips the wall
    -> GeometryBlocked

same roll with more clearance
    -> Feasible

lateral Hermite correction with insufficient side thrust
    -> LinearAuthorityExceeded

same correction with sufficient side thrust
    -> Feasible

90 degree / 1 second rotation with insufficient angular speed
    -> AngularAuthorityExceeded

sideways inertial velocity
    Newtonian -> allowed
    EliteAssisted with tight slip policy -> AssistedSlipExceeded

zero duration
    -> InvalidInput
```

Architecture checker:

```text
tests/architecture_contracts/check_navigation_trajectory_continuous_passage.py
```

## Performance invariant

The verifier is bounded and allocation-free in its hot logic:

```text
33 pose samples
32 continuous interval proofs
no scene scan
no obstacle all-pairs work
no NavigationMap/NavigationSpace ownership
```

Measured cost already leaves large headroom. Keep the algorithm unchanged until live composition produces contrary evidence.

## Not yet claimed

This continuous static slice still does not own:

- time-varying obstacle-gap geometry;
- arbitrary nonzero initial angular-rate synthesis inside the segment;
- exact individual-thruster allocation;
- collision response after intentional emergency contact;
- moving/rotating docking terminal capture;
- live `EliteGame` / `EliteServer` integration.

Relative normal contact-severity ranking has moved into its own bounded candidate layer rather than remaining a missing responsibility of this verifier.
