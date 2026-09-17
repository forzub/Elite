# Navigation v2 — terminal docking 6DoF model

**Status:** isolated candidate pending target-machine architecture/build/behavior gate  
**Updated:** 2026-09-17 Europe/Kyiv  
**Stage:** `NAV-V2-TRAJECTORY-1` / docking 9A  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/world/navigation/MOVING_PASSAGE_TRAJECTORY_MODEL.md`

## Purpose

After static and moving oriented-passage feasibility, docking adds a terminal relative-pose contract.

`DockingTerminalEvaluator` answers one narrow question:

```text
At one candidate future capture time, do the ship docking interface and the
predicted moving/rotating dock interface satisfy the complete terminal capture
tolerances?
```

Implementation:

```text
src/world/navigation/trajectory/DockingTerminalEvaluator.h
src/world/navigation/trajectory/DockingTerminalEvaluator.cpp
```

This is docking stage **9A**. It does not yet synthesize the entire approach trajectory; stage 9B will compose the accepted moving-passage machinery with this terminal target.

## Docking is not center-point arrival

A successful capture requires all of:

```text
relative port position
relative port linear velocity
mating-face orientation
roll/up orientation
relative angular velocity
```

A ship that reaches the correct point but is moving too fast, spinning relative to the dock, facing the wrong way, or rolled upside-down is not captured.

## Explicit interface frames

Both ship and dock expose body-local docking-port metadata:

```text
surface semantic
local position
matingNormal
referenceUp / rollReference
```

The mating normal points outward from the physical mating face. `referenceUp` lies in the mating plane and fixes roll.

No world-up heuristic is used.

### Explicit bottom-to-bottom semantics

The current required mating contract is explicitly:

```text
ship surface = Bottom
dock surface = Bottom
```

At capture:

```text
ship mating normal = -dock mating normal
ship referenceUp   =  dock referenceUp
```

Therefore a 180-degree rolled ship can have the correct mating-face normal and still fail capture because its reference-up axis is reversed.

The surface semantic is part of the contract rather than inferred from geometry. Future top/side ports can use the same evaluator with different metadata.

## Ship port kinematics

The candidate ship state is supplied at capture time:

```text
body pose
center linear velocity
body angular velocity
```

For body-local port offset `r_ship` transformed to map space:

```text
p_ship_port = p_ship_center + r_ship
v_ship_port = v_ship_center + omega_ship x r_ship
```

This prevents an offset docking interface on a rotating ship from being treated as if it moved with center velocity only.

## Moving / rotating dock prediction

The dock provides a reference-origin state:

```text
origin position / orientation
origin linear velocity
origin linear acceleration
constant world-space angular velocity over the bounded prediction
```

The origin translation is predicted as:

```text
p_origin(t) = p0 + v0*t + 0.5*a*t^2
v_origin(t) = v0 + a*t
```

The dock orientation and local port offset are rotated by the constant angular velocity over the candidate capture interval.

For predicted map-space port offset `r_port(t)`:

```text
p_port = p_origin + r_port
v_port = v_origin + omega x r_port
```

The `omega x r` term is mandatory. A rotating station-rim port has tangential velocity even when the station origin is stationary.

A moving + rotating dock therefore has one combined predicted terminal frame rather than separate translation and rotation hacks.

## Terminal error diagnostics

The evaluator publishes:

```text
positionError
axialPositionError
lateralPositionError
relativeLinearSpeed
relativeNormalSpeed
relativeTangentialSpeed
matingNormalAlignmentError
rollAlignmentError
relativeAngularSpeed
```

The current first capture policy gates total position error and total relative linear speed. The axial/tangential diagnostics are exposed so stage 9B can use more detailed approach/capture policies without changing frame ownership.

## Capture statuses

```text
Capturable
PortSemanticMismatch
PositionMismatch
RelativeLinearVelocityMismatch
MatingNormalMismatch
RollAlignmentMismatch
RelativeAngularVelocityMismatch
InvalidInput
```

The failure ordering is deterministic. `Capturable` means the supplied candidate terminal state is inside every configured capture tolerance; it does not itself latch the ship or modify physics state.

## Collision / latch ownership

The terminal evaluator is navigation/control feasibility, not collision physics.

```text
Navigation / docking trajectory
    predict target frame
    converge terminal P/V/attitude/omega
    decide whether capture tolerances are satisfied

Physics / Collision
    exact contact / CCD / TOI / manifold
    physical motion until capture authority transfers

Docking / game state
    authoritative latch/capture transition

Damage / Structural
    consequences of an actual failed-impact event
```

Ordinary docking must go around / abort when capture cannot be maintained. `Capturable` is not a license to treat a destructive impact as successful docking.

## Pinned behavior fixtures

```text
stationary bottom-to-bottom ports
    -> Capturable

explicit ship Top vs required Bottom
    -> PortSemanticMismatch

translating carrier at future capture time
    matching ship P/V -> Capturable
    same position but unmatched velocity -> RelativeLinearVelocityMismatch

rotating offset port
    v_port includes omega x r
    ship matching tangential velocity + omega -> Capturable

combined translating + rotating future dock
    predicted position/orientation/linear velocity/angular rate all matched
    -> Capturable

correct bottom-face normal but 180-degree reversed reference-up
    -> RollAlignmentMismatch

correct pose but unmatched angular rate
    -> RelativeAngularVelocityMismatch

position outside capture tolerance
    -> PositionMismatch
```

## Next after acceptance

Stage 9B will build a bounded docking approach evaluator using the already accepted moving-passage machinery:

```text
coarse rendezvous / moving intercept
    -> docking corridor acquisition
    -> continuous oriented-hull corridor proof
    -> terminal relative P/V/attitude/omega convergence
    -> DockingTerminalEvaluator
    -> capture / latch request
```

The port frame is the target; no second navigation world or separate docking physics model is introduced.
