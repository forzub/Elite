# Navigation v2 — continuous static passage trajectory model

**Status:** isolated candidate pending target-machine gate  
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

The next question is continuous:

```text
Can one complete bounded translation + rotation segment be executed
with the declared vehicle authority while the oriented hull stays
inside a static narrow passage for the whole interval?
```

Candidate code:

```text
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.cpp
```

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

Continuous width/height clearance therefore subtracts:

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

Cubic-Hermite acceleration is linear inside each interval. Therefore, for a **fixed** body axis, the extrema of acceleration projection are already contained by the interval endpoints. The continuous verifier adds extra projection margin only for rotation of the body axis itself:

```text
axisRotationProjectionMargin
    <= maxAccelerationMagnitude * 2 * sin(deltaTheta / 2)
```

This avoids inventing cross-axis thrust demand when, for example, only lateral acceleration changes while the hull frame is fixed. A geometrically clear trajectory can still fail as `LinearAuthorityExceeded` when the actual required body-axis authority is too large.

Navigation consumes these limits from the authoritative flight/physics capability boundary; it must not invent stronger thrust.

## Angular authority

The analytic orientation segment is rejected when either exact peak requirement exceeds the supplied vehicle capability:

```text
AngularAuthorityExceeded
```

The current first slice assumes the selected segment starts and ends on the smooth shortest-arc attitude law. Full arbitrary initial angular-rate trajectory synthesis remains later; the prior reachability/emergency layers already account conservatively for existing angular motion before selecting a passage pose.

## `Elite` versus `Newton`

Physical thrust limits remain truthful in both modes.

```text
Newtonian
    velocity and hull attitude may diverge
    no artificial velocity-to-nose coupling is imposed

EliteAssisted
    the controller may impose a maximum velocity-to-forward slip angle
    this is an assisted-control policy constraint, not extra thrust
```

The first isolated evaluator therefore distinguishes control semantics without duplicating the authoritative flight controller.

## Result classes

```text
Feasible
GeometryBlocked
LinearAuthorityExceeded
AngularAuthorityExceeded
AssistedSlipExceeded
InvalidInput
```

`Feasible` means the entire supplied analytic segment has passed the static extruded-passage geometry bound plus declared vehicle-authority gates. It does not yet prove moving-obstacle or moving-dock safety.

## Relationship to emergency contact

A continuous collision-free candidate is preferred.

If no collision-free continuous segment survives and stopping is impossible, `EmergencyPassageMitigator` remains the fallback command source. The later emergency continuous solver will additionally rank expected contact by relative normal contact speed / impact-energy proxy so a glancing hit or ricochet is preferred over a normal impact.

`EmergencyMitigatedContact` remains explicitly non-safe; actual contact response and damage stay physics/damage authority.

## Behavior fixtures

Target-machine runner:

```text
tests/navigation_trajectory/run_mingw64.sh
```

New fixture executable:

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

## Performance rule

The verifier is bounded and allocation-free in its hot logic:

```text
33 pose samples
32 continuous interval proofs
no scene scan
no obstacle all-pairs work
no NavigationMap/NavigationSpace ownership
```

Do not benchmark-optimize it before behavior acceptance on the target machine. After the behavior gate is green, add a dedicated microbenchmark before integrating the verifier into live NPC/game control.

## Not yet claimed

This first continuous slice still does not own:

- time-varying obstacle-gap geometry;
- arbitrary nonzero initial angular-rate synthesis inside the segment;
- exact individual-thruster allocation;
- collision response after an intentional emergency contact;
- relative normal impact-energy ranking;
- moving/rotating docking terminal capture;
- live `EliteGame` / `EliteServer` integration.

Those remain later `NAV-V2-TRAJECTORY-1` slices.
