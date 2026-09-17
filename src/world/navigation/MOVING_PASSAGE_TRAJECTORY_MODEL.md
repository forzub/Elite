# Navigation v2 — moving passage trajectory model

**Status:** isolated candidate pending target-machine architecture/build/behavior gate  
**Updated:** 2026-09-17 Europe/Kyiv  
**Stage:** `NAV-V2-TRAJECTORY-1`  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/MOVING_GAP_MODEL.md`, `src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md`

## Purpose

The accepted `MovingGapPredictor` answers whether one already-selected obstacle pair remains an open transverse gap over a short horizon. It does not prove that a particular ship can fly through it.

`MovingPassageTrajectoryEvaluator` composes:

```text
one accepted MovingGapPredictor::Result
        +
one analytic ship P/V trajectory
        +
one analytic ship attitude trajectory
        +
ship OBB proxy and physical authority
```

and answers:

```text
Can this particular ship continuously traverse this particular moving gap
for the whole bounded horizon without leaving the time-varying passage?
```

Implementation:

```text
src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.h
src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.cpp
```

This evaluator is **not a collision solver**.

Navigation owns predictive feasibility and conservative risk evidence. The authoritative runtime contact path remains:

```text
Physics / Collision
    broadphase / narrow phase
    CCD / TOI / contact manifold / impulse / ricochet

Damage / Structural
    material consequence / breach / detach / destruction
```

Navigation never fabricates the post-impact state.

## Bounded synchronized partition

The evaluator consumes the accepted moving-gap partition directly:

```text
33 synchronized ship/gap samples
32 continuous intervals
```

It does not rerun pair discovery or scan the world. The moving gap is already one member of the upstream bounded `<=8` precision candidate set.

The ship center follows the same cubic-Hermite reference used by the accepted static continuous verifier:

```text
start position + velocity
    -> cubic Hermite over T
end position + velocity
```

Ship attitude follows the same shortest-arc smooth rest-to-rest law:

```text
s(u) = 3u^2 - 2u^3
```

so the exact whole-segment angular requirements remain:

```text
omega_peak = 1.5 * angle / T
alpha_peak = 6.0 * angle / T^2
```

## Sample geometry

At each synchronized time sample, the moving-gap state is converted into an oriented obstacle passage:

```text
passage center      = predicted moving gap center
passage right       = projected separation axis
passage forward     = requested travel direction
passage up          = right-handed transverse complement
passage half-width  = 0.5 * predicted clear separation
passage half-height = 0.5 * caller-owned secondary clearance
```

The actual ship OBB pose is checked with `OrientedPassageEvaluator`.

A discrete sample failure immediately returns:

```text
GeometryBlocked
```

but discrete success alone is not enough.

## Continuous moving-width proof

For every interval the evaluator reconstructs the same constant-relative-acceleration lower bound used by `MovingGapPredictor`:

```text
relative obstacle center motion
    -> exact minimum distance to endpoint chord
    -> subtract |a_rel| * dt^2 / 8
    -> subtract inflated boundary radii
```

This gives a conservative interval lower bound on moving clear separation and therefore passage half-width.

A moving gap that closes between two apparently valid samples cannot be accepted.

## Continuous relative-center proof

The ship must also track the passage laterally while the pair itself translates.

The proof works in the plane transverse to the fixed travel direction, so normal forward progress through the extruded passage does not create false lateral inflation.

For each half interval it bounds:

```text
relative transverse displacement
    <= |v_rel_transverse| * dt/2
       + 0.5 * |a_rel_transverse|max * (dt/2)^2
```

where relative motion is measured against the midpoint motion of the two constraining boundaries.

The interval lateral/vertical offset bound therefore includes:

```text
worst endpoint offset
+ relative transverse center-motion bound
+ passage-axis rotation projection inflation
```

This is specifically a **between-sample** proof, not a sample interpolation assumption.

## Passage-frame rotation

The separation axis may rotate as the constraining obstacles move.

The evaluator bounds its continuous angular sweep from:

```text
endpoint separation-axis angle
+ constant-acceleration chord-deviation perturbation
```

The projected passage right/up frame may rotate more than the raw separation vector. Under the precision contract that the accepted gap remains sufficiently transverse (`abs(separationAxis dot travel) <= 0.5` by default), the evaluator uses an additional conservative projection amplification.

If the moving-gap result was produced with a looser alignment policy than the trajectory verifier accepts, the result is:

```text
GapUnavailable
```

rather than an optimistic trajectory claim.

## Continuous OBB sweep

Both the hull and the moving passage frame can rotate during the same interval.

The relative orientation sweep is conservatively bounded by the sum of:

```text
ship body interval rotation
+ passage-frame interval rotation bound
```

For conservative hull rotation radius `R`, the support/sweep inflation is:

```text
orientationSweepInflation <= 2 * R * sin(deltaRelativeAngle / 2)
```

The continuous width/height clearance bound subtracts:

```text
relative center-offset bound
+ worst endpoint projected OBB extent
+ relative orientation sweep inflation
```

from the continuously available moving half-width / half-height.

If the resulting interval clearance goes negative:

```text
GeometryBlocked
```

This catches a ship that fits all 33 poses but cannot be conservatively proven safe between two adjacent poses.

## Vehicle authority

The moving-passage evaluator preserves the accepted **body-axis physical authority** contract.

It checks:

```text
forward acceleration
reverse / braking acceleration
lateral acceleration
vertical acceleration
angular speed
angular acceleration
```

Hermite acceleration is linear over an interval. Fixed-axis projection extrema therefore occur at interval endpoints; body rotation adds only the same conservative projection margin used by the accepted static verifier.

The evaluator does not allocate thrusters and does not invent stronger authority than supplied by flight/physics.

## Newtonian versus assisted control

The accepted distinction remains explicit:

```text
Newtonian
    inertial velocity may differ arbitrarily from hull forward

EliteAssisted
    same physical thrust limits
    plus caller-supplied maximum velocity-to-forward slip policy
```

A moving gap does not weaken or bypass either control model.

## Result classes

```text
Feasible
GapUnavailable
GeometryBlocked
LinearAuthorityExceeded
AngularAuthorityExceeded
AssistedSlipExceeded
InvalidInput
```

`Feasible` means only that this bounded analytic ship segment is conservatively proven inside the supplied time-varying gap and within declared vehicle authority.

It is not an assertion that the physics engine can skip collision detection.

## Collision boundary

The expected live ownership is:

```text
NavigationWorld / local reduction
    -> bounded candidate gaps
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
        collision-free proof if possible
        predicted risk/contact witness later if not

Physics / Collision
    exact runtime CCD and contact response

Damage / Structural
    actual consequence
```

This separation lets navigation plan before contact while keeping one authoritative collision mechanism for the game.

## Pinned behavior fixtures

```text
static gap + straight centered ship
    -> Feasible

translating gap + ship matching gap-center velocity
    -> Feasible

translating gap + ship that does not follow it
    -> GeometryBlocked

all 33 sampled poses fit but conservative interval bound fails
    -> GeometryBlocked from between-sample proof

upstream moving gap closes during horizon
    -> GapUnavailable before ship work

lateral maneuver exceeds declared side thrust
    -> LinearAuthorityExceeded

sideways inertial travel
    Newtonian -> Feasible
    EliteAssisted with tight slip policy -> AssistedSlipExceeded
```

Target-machine gate:

```text
python tests/architecture_contracts/check_navigation_trajectory_moving_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Expected suite count after this candidate is registered:

```text
8/8
```

## Next after acceptance

The same relative-frame machinery becomes the basis for moving/rotating docking:

```text
moving obstacle pair frame
    -> moving passage frame

moving/rotating docking port
    -> terminal 6DoF mating frame
```

Docking adds explicit terminal relative position, velocity, attitude, angular-rate and bottom-to-bottom mating constraints rather than inventing a separate navigation system.
