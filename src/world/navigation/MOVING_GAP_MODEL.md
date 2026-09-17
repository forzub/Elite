# Navigation v2 — moving / time-varying gap model

**Status:** isolated candidate pending target-machine architecture/build/behavior gate  
**Updated:** 2026-09-17 Europe/Kyiv  
**Stage:** `NAV-V2-TRAJECTORY-1`  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md`

## Purpose

`BoundedGapCandidateBuilder` remains the cheap snapshot-stage pair selector. It examines one already-known primary conflict against already-reduced neighbors and returns at most eight plausible transverse obstacle-gap pairs.

`MovingGapPredictor` solves the next, smaller problem:

```text
Given one already-selected obstacle pair and compact P/V/A state,
does its free-space gap remain open and transverse over one short horizon,
and what moving passage/boundary state exists at each bounded time sample?
```

Implementation:

```text
src/world/navigation/trajectory/MovingGapPredictor.h
src/world/navigation/trajectory/MovingGapPredictor.cpp
```

The predictor does **not** perform pair discovery, world search, all-pairs work, ship trajectory synthesis, CCD/TOI, contact response, or damage.

## Motion contract

Each constraining boundary carries:

```text
snapshot revision
center position
linear velocity
linear acceleration
angular velocity
conservative radius
```

Center motion over the short horizon uses constant acceleration:

```text
p(t) = p0 + v0*t + 0.5*a*t^2
v(t) = v0 + a*t
```

The conservative spherical radius remains the moving-gap geometry proxy. Angular velocity does not change that spherical geometry, but it does contribute to material surface velocity for downstream emergency-contact severity:

```text
v_surface = v_center + omega x r_surface
```

Mixed snapshot revisions fail closed before prediction.

## Fixed bounded horizon partition

The predictor uses:

```text
33 time samples
32 continuous intervals
```

This is fixed bounded work for one already-selected pair. It is not multiplied by scene actor count inside the predictor.

## Gap state per sample

At each sample:

```text
r = center_secondary - center_primary
n = normalize(r)
```

`n` is the separation axis from primary toward secondary.

After adding caller-supplied boundary clearance to both conservative radii:

```text
clear separation = |r| - R_primary_inflated - R_secondary_inflated
```

The moving gap publishes:

```text
gap center
travel direction
separation axis
clear separation
secondary clearance
gap-center velocity
separation rate
```

It also publishes both physical boundary witnesses facing the gap:

```text
surface point
normal toward free space
surface material velocity
```

These boundary witnesses are suitable inputs to a later trajectory/contact-witness composition stage. They are not by themselves a claim that the ship touches the surface at that point.

## Gap-center velocity

For unequal inflated radii the free-space midpoint is not simply the average of obstacle centers when the separation axis rotates.

With:

```text
n_dot = (v_rel - n*dot(v_rel,n)) / |r|
```

the published target velocity is:

```text
v_gap = 0.5*(v_primary + v_secondary)
      + 0.5*(R_primary_inflated - R_secondary_inflated)*n_dot
```

This lets a downstream moving-passage controller target the actual moving center of the gap rather than its stale snapshot position.

## Continuous gap-width proof

The 33 samples are only a partition. A gap is not accepted merely because all sampled widths are positive.

For one interval of duration `dt`, constant-relative-acceleration motion is quadratic. The real relative-position curve differs from the straight chord between the two endpoint relative positions by at most:

```text
center deviation from chord <= |a_rel| * dt^2 / 8
```

The minimum distance from the origin to that chord segment is computed exactly. Therefore a conservative lower bound for center separation over the entire interval is:

```text
d_center_continuous_lower
    = max(0, distance(origin, relative-position chord)
             - |a_rel|*dt^2/8)
```

and the continuous free-gap bound is:

```text
clear_continuous_lower
    = d_center_continuous_lower
      - R_primary_inflated
      - R_secondary_inflated
```

If this bound falls below the configured minimum clear separation, the result is:

```text
GapClosesDuringHorizon
```

This deliberately catches a fast pair that appears open at two adjacent samples but closes and reopens between them.

## Continuous transverse-alignment proof

A side-by-side gap must stay sufficiently transverse to requested travel.

The exact scalar numerator:

```text
dot(r(t), travel)
```

is quadratic under constant relative acceleration. Its maximum absolute value over an interval is obtained from the two endpoints plus any interior stationary point.

Using the continuous center-distance lower bound gives a conservative interval bound:

```text
abs(separationAxis dot travel)
    <= max_abs(dot(r(t),travel)) / d_center_continuous_lower
```

If this exceeds the configured policy threshold, the pair stops being a valid transverse passage and returns:

```text
AlignmentLost
```

Opening wider is not a failure. Closing or becoming longitudinal is.

## Result classes

```text
OpenForHorizon
GapClosesDuringHorizon
AlignmentLost
RevisionMismatch
InvalidInput
```

`OpenForHorizon` means only that the conservative spherical constraining pair remains continuously separated and sufficiently transverse for the whole supplied horizon.

It does **not** mean a particular oriented ship trajectory is safe. The next moving-passage trajectory slice must combine these time-varying gap states with the ship's continuous P/V/attitude/hull sweep and physical authority.

## Relationship to emergency contact severity

The already accepted `EmergencyContactSeverityScorer` consumes predicted contact witnesses with surface velocity.

`MovingGapPredictor` supplies time-indexed boundary surface normals and material velocities, including:

```text
v_surface = v_center + omega x r_surface
```

A later composition stage may combine these boundary data with a predicted ship hull contact point/state to produce scorer witnesses. The moving-gap predictor itself does not invent a collision or contact point.

## Performance / scaling invariant

```text
snapshot broadphase / local reduction
    -> <= 8 static gap pairs
    -> MovingGapPredictor for only plausible pairs
    -> moving continuous ship-passage proof only where needed
```

No frame-path `N x N` moving-gap search is allowed.

The predictor itself is fixed-size work:

```text
33 samples
32 continuous interval bounds
one already-selected obstacle pair
no allocation-sized scene scan
```

A dedicated microbenchmark is not required before behavior acceptance unless target/live composition shows material cost.

## Pinned behavior fixtures

```text
static transverse pair
    -> OpenForHorizon

co-moving pair
    -> gap remains open and gap center carries pair velocity

fast pair open at adjacent samples but closed between them
    -> GapClosesDuringHorizon from continuous chord bound

pair rotates toward requested travel direction
    -> AlignmentLost

rotating spherical boundary
    -> surface material velocity includes omega x r

mixed snapshot revisions
    -> RevisionMismatch before prediction

relative acceleration opening the pair
    -> future gap width and separation rate reflect P/V/A
```

Target-machine runner:

```text
python tests/architecture_contracts/check_navigation_trajectory_moving_gap.py
bash tests/navigation_trajectory/run_mingw64.sh
```
