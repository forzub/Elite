# Navigation v2 — continuous moving/rotating docking approach

**Status:** stage 9B behavior/architecture ACCEPTED; docking stage 9 CLOSED  
**Updated:** 2026-09-18 Europe/Kyiv  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/world/navigation/DOCKING_TERMINAL_MODEL.md`

## Purpose

Stage 9A proved the terminal 6DoF capture contract. Stage 9B verifies the **complete final precision docking segment** that leads into that terminal state.

Implementation:

```text
src/world/navigation/trajectory/DockingApproachEvaluator.h
src/world/navigation/trajectory/DockingApproachEvaluator.cpp
```

The evaluator answers:

```text
Can this concrete ship, with this hull and physical authority, continuously
remain inside the moving/rotating docking corridor and finish inside the
already accepted terminal capture tolerances?
```

It is still a bounded candidate verifier, not a global docking route search or a flight-controller replacement.

## Why docking is solved in the dock frame

A rotating dock cannot be treated as a static world-space target followed by a rest-to-rest hull rotation. At capture a ship attached to a rotating port must already share the port's angular velocity.

Therefore the final precision segment is expressed in the moving/rotating dock-port frame.

In that frame:

```text
docking corridor = static oriented passage
relative terminal pose = fixed target
relative terminal velocity = near zero
relative terminal angular rate = zero
```

The ship's relative attitude uses the same shortest-arc smooth rest-to-rest interpolation already accepted for continuous passage.

When transformed back to world space, relative angular rate zero at capture gives:

```text
omega_ship(capture) = omega_dock
```

This is the required rotating-dock behavior rather than a special terminal patch.

## Dock corridor frame

At each time `t`, stage 9A shared prediction supplies the world dock-port state:

```text
port position
port linear velocity
port angular velocity
mating normal
referenceUp
```

The docking passage basis is:

```text
forward = -matingNormal      # travel toward the mating face
up      = referenceUp
right   = up x forward
```

The basis is proper right-handed and contains no global world-up inference.

The corridor is an extruded rectangular cross-section:

```text
halfWidth
halfHeight
```

Terminal face/capture semantics remain owned by `DockingTerminalEvaluator`; the corridor verifier does not reinterpret bottom/top/side mating rules.

## Endpoint transformation into dock-local coordinates

For ship-center offset from the port:

```text
r = p_ship - p_port
```

the derivative in the rotating dock frame is:

```text
v_relative_world = v_ship - v_port - omega_dock x r
```

Both endpoint position and relative velocity are transformed into the corresponding dock frame.

The candidate relative translation is one cubic Hermite segment between those endpoint P/V states.

Ship body orientation is likewise transformed into dock-local coordinates at both endpoints and follows the shortest relative rotation arc.

## Reuse of accepted continuous geometry

The dock-local corridor is static, so stage 9B deliberately reuses:

```text
ContinuousPassageTrajectoryEvaluator
```

as the continuous geometry oracle.

The delegated query uses:

```text
PassageSource::DockingCorridor
33 dock-local ship poses
32 conservative between-sample intervals
static dock-local corridor cross-section
```

Only geometry is delegated. Huge internal capability limits and Newtonian policy are supplied to that internal geometry query so it cannot pretend that rotating-frame accelerations are physical ship authority.

The accepted verifier still supplies the authoritative between-sample hull-clearance proof. Point samples alone are insufficient.

## World / inertial kinematics

Physical authority is checked separately after transforming the relative curve back to world space.

For dock-relative position `r`, velocity `v_rel`, acceleration `a_rel`, constant dock angular velocity `omega`, and port acceleration `a_port`:

```text
v_world = v_port
        + omega x r
        + v_rel
```

and:

```text
a_world = a_port
        + omega x (omega x r)
        + 2 * omega x v_rel
        + a_rel
```

where rotated dock-local vectors are implied in the world terms.

The `2 * omega x v_rel` Coriolis term and centripetal terms are real inertial acceleration requirements. They are not free acceleration granted by the navigation frame.

For a dock-local port offset `r_port` and constant dock angular velocity:

```text
a_port = a_origin + omega x (omega x r_port)
```

## Continuous body-axis thrust proof

Checking world acceleration only at 33 samples would be insufficient. Stage 9B adds a conservative interval projection margin.

The cubic dock-local Hermite curve has constant relative jerk `j_rel`. With constant dock angular velocity, a conservative world jerk bound is:

```text
|j_world| <=
      |omega|^3 * (|r_port| + |r_relative|)
    + 3 * |omega|^2 * |v_relative|
    + 3 * |omega| * |a_relative|
    + |j_relative|
```

For each interval, body-axis acceleration projection may change because both acceleration and the body axes move. The projection margin therefore bounds:

```text
jerk contribution
    +
world acceleration magnitude * body angular-speed bound
```

over half an interval.

This margin is added to the endpoint forward/reverse/lateral/vertical projections before comparing them with ship capability.

The purpose is fail-closed physical authority between samples, not a discrete thrust check.

## Angular authority in a rotating dock frame

Relative attitude follows smoothstep timing. For relative orientation change `theta` over duration `T`:

```text
relative omega peak = 1.5 * theta / T
relative alpha peak = 6.0 * theta / T^2
```

With constant dock angular velocity:

```text
world omega peak bound
    = |omega_dock| + relative omega peak
```

and a conservative angular-acceleration bound is:

```text
world alpha peak bound
    = relative alpha peak
    + |omega_dock| * relative omega peak
```

Therefore merely co-rotating with a fast station consumes real angular-rate capability even when the ship has zero relative attitude change.

## Elite / Newton semantics

World physical authority remains the same for both modes.

```text
Newtonian
    world velocity may diverge from hull forward

EliteAssisted
    same physical acceleration proof
    + supplied world velocity-to-forward slip policy
```

Dock-local convenience never creates extra thruster authority.

## Terminal composition

After the complete corridor and physical-authority proof, the final generated world state is passed to the already accepted:

```text
DockingTerminalEvaluator
```

The terminal query uses:

```text
final ship pose
final ship center velocity
ship angular velocity = dock angular velocity
explicit ship/dock local port frames
capture time
accepted terminal tolerances
```

Success therefore requires both:

```text
continuous approach feasible
AND
terminal 6DoF state Capturable
```

A wide corridor cannot bypass wrong bottom/top semantics, 180-degree roll rejection, relative velocity mismatch, or terminal position error.

## Result classes

```text
FeasibleForCapture
CorridorBlocked
LinearAuthorityExceeded
AngularAuthorityExceeded
AssistedSlipExceeded
TerminalNotCapturable
InvalidInput
```

`TerminalNotCapturable` preserves the precise stage 9A terminal result for diagnostics.

## Pinned fixtures

```text
stationary dock final approach
    -> continuous corridor + terminal capture

translating dock
    ship follows same relative trajectory
    -> FeasibleForCapture

moving + rotating dock
    ship follows dock-local trajectory
    -> relative attitude change zero
    -> world angular-speed requirement includes dock omega
    -> terminal relative angular rate zero

all 33 corridor samples fit but continuous interval bound fails
    -> CorridorBlocked

insufficient inertial body-axis thrust
    -> LinearAuthorityExceeded

fast rotating dock exceeds ship angular-rate capability
    -> AngularAuthorityExceeded

wide corridor + 180-degree wrong terminal roll
    -> TerminalNotCapturable / RollAlignmentMismatch

wide corridor + terminal position outside tolerance
    -> TerminalNotCapturable / PositionMismatch

sideways bottom docking
    Newtonian -> feasible
    tight EliteAssisted slip policy -> AssistedSlipExceeded
```

## Regression correction after first target-machine gate

The first 9B behavior run correctly refused to satisfy the intended `CorridorBlocked` assertion because the original fixture was not actually blocked. With zero endpoint displacement and equal 10 m/s lateral endpoint velocities over 1 second, the cubic-Hermite center excursion is:

```text
largest 33-sample offset = 0.9613037109 m
exact continuous maximum = 0.9622504486 m
```

The original corridor/hull combination allowed `0.975 m` of center travel, so the full curve genuinely fit. The regression now allows `0.9618 m` instead:

```text
sample clearance            = +0.0004962891 m
exact continuous clearance  = -0.0004504486 m
interval conservative bound approximately -0.00408135 m
```

This preserves the required test semantics: every discrete pose fits, while the existing between-sample proof must fail closed. No production geometry tolerance was relaxed.

## Scope boundary

Stage 9B starts after coarse rendezvous/corridor acquisition has reduced docking to one bounded final precision segment.

It does not own:

```text
global route search
port selection
traffic scheduling
coarse pursuit/intercept
thruster allocation
exact collision CCD/TOI/contact response
authoritative latch state transition
```

Those remain separate owners.

## Collision and latch ownership

```text
Navigation / docking trajectory
    continuous corridor + physical feasibility
    terminal capture evidence

Flight control / thruster allocation
    execute accepted commands

Physics / Collision
    exact CCD / TOI / contact manifold / impulse

Docking game state
    authoritative latch / capture transition

Damage / Structural
    consequences of real failed contact
```

Normal docking aborts or goes around when a safe/capturable final segment cannot be proven. A destructive collision is never relabeled as docking success.

## Target-machine acceptance

Accepted on:

```text
c90a66d6c64bdf3acc037208a000b1955d40e6c3
NAVIGATION TRAJECTORY DOCKING APPROACH CONTRACT: PASS
10/10 navigation_trajectory CTest PASS
100% tests passed
```

The repaired hidden-between-sample regression passed without changing the production continuous geometry evaluator. Stage 9A terminal capture and stage 9B continuous approach are therefore both accepted, and the large moving/rotating docking mathematics stage is closed.

## Next after acceptance

Stage 9B passed its target-machine gate. The large docking mathematics stage is closed. The active Navigation v2 block is deterministic `PilotSkillProfile` execution, followed by live game/server/guidance + physics hookup and end-to-end stress/debug acceptance.
