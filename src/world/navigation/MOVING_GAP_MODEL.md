# Navigation v2 — moving / time-varying gap model

**Status:** behavior/architecture ACCEPTED  
**Updated:** 2026-09-17 Europe/Kyiv  
**Stage:** `NAV-V2-TRAJECTORY-1`  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md`

## Accepted target-machine gate

Canonical acceptance commit:

```text
bc84940ff23209d9731a3d5332b8af3794182790
```

Evidence:

```text
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
navigation_trajectory_passage                       PASS
navigation_trajectory_gap                           PASS
navigation_trajectory_reachability                  PASS
navigation_trajectory_emergency_passage             PASS
navigation_trajectory_continuous_passage            PASS
navigation_trajectory_emergency_contact_severity    PASS
navigation_trajectory_moving_gap                    PASS

100% tests passed, 0 failed out of 7
```

Decision: **freeze `MovingGapPredictor` behavior** unless later composition/live evidence exposes a defect. No dedicated microbenchmark is required yet because work is fixed to one already-selected pair and the accepted precision candidate ceiling remains `<=8`.

## Purpose

`BoundedGapCandidateBuilder` remains the cheap snapshot-stage pair selector. It examines one already-known primary conflict against already-reduced neighbors and returns at most eight plausible transverse obstacle-gap pairs.

`MovingGapPredictor` receives exactly one such pair and answers:

```text
does the gap remain continuously open and sufficiently transverse
through one short receding horizon?
```

Implementation:

```text
src/world/navigation/trajectory/MovingGapPredictor.h
src/world/navigation/trajectory/MovingGapPredictor.cpp
```

It does **not** discover pairs, scan the world, synthesize the ship trajectory, perform CCD/TOI, calculate contact response, or own damage.

## Boundary motion

Each constraining boundary supplies:

```text
snapshot revision
center P / V / A
angular velocity
conservative radius
```

Center prediction is constant acceleration:

```text
p(t) = p0 + v0*t + 0.5*a*t^2
v(t) = v0 + a*t
```

Mixed snapshot revisions fail closed before prediction.

Angular velocity does not change the conservative spherical gap boundary, but it does affect material surface velocity:

```text
v_surface = v_center + omega x r_surface
```

That output is intended for downstream emergency-contact prediction/severity composition.

## Fixed bounded work

```text
33 time samples
32 continuous intervals
one already-selected obstacle pair
```

Per sample the predictor publishes:

```text
gap center
gap-center velocity
travel direction
separation axis
clear separation
separation rate
secondary clearance

for both facing boundaries:
    center / velocity
    physical surface point
    normal toward free space
    material surface velocity
```

## Continuous width proof

Samples are only a partition.

For one interval under constant relative acceleration, the true relative-position curve differs from its endpoint chord by at most:

```text
|a_rel| * dt^2 / 8
```

The exact minimum origin-to-chord distance minus that deviation gives a conservative lower bound on center separation. After inflated radii are subtracted, the result is a continuous lower bound on free gap width.

Therefore:

```text
sample i      open
between       closed
sample i+1    open
```

still returns:

```text
GapClosesDuringHorizon
```

## Continuous transverse-alignment proof

A side-by-side gap must remain transverse to requested travel.

The scalar numerator:

```text
dot(r(t), travel)
```

is quadratic. Its exact maximum absolute value over an interval comes from endpoints plus any interior stationary point. Dividing by the conservative center-distance lower bound yields a safe bound for:

```text
abs(separationAxis dot travel)
```

Exceeding policy returns:

```text
AlignmentLost
```

## Result classes

```text
OpenForHorizon
GapClosesDuringHorizon
AlignmentLost
RevisionMismatch
InvalidInput
```

`OpenForHorizon` proves only that the conservative constraining pair remains continuously suitable as a moving gap. It does **not** prove that a specific oriented ship trajectory fits.

That next responsibility is now owned by:

```text
src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.h/.cpp
src/world/navigation/MOVING_PASSAGE_TRAJECTORY_MODEL.md
```

## Accepted fixtures

```text
static transverse pair
    -> OpenForHorizon

co-moving pair
    -> translated gap center + preserved width

hidden between-sample closure
    -> GapClosesDuringHorizon

pair rotates toward travel
    -> AlignmentLost

rotating spherical boundary
    -> surface material velocity includes omega x r

mixed revisions
    -> RevisionMismatch before prediction

relative acceleration
    -> future width and separation rate reflect P/V/A
```

## Scaling invariant

```text
shared dynamic NavigationWorld / local reduction
    -> bounded static gap pairs <= 8
    -> MovingGapPredictor only for plausible pairs
    -> MovingPassageTrajectoryEvaluator only where precision proof is needed
```

No frame-path `N x N` moving-gap search is allowed.
