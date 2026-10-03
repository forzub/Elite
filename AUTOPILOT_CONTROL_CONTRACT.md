# AUTOPILOT_CONTROL_CONTRACT

## Purpose

The route Planner owns route geometry and the time/speed/attitude reference.
The Follower measures deviation from that accepted route and requests pilot
corrections. The Autopilot is a virtual pilot: it must use the same ordinary
ship controls as a human pilot.

The production automatic-docking path must not select engines, inject
SYSTEM-frame acceleration, or call navigation-only propulsion helpers.

## Ownership

### Planner

Owns:

- collision-free route geometry;
- accepted corridor/centerline;
- speed schedule and stopping points;
- physically feasible orientation/angular schedule;
- route replacement when the accepted route is invalidated.

The Planner does not press controls.

### Follower

Owns:

- current progress on the accepted route;
- cross-track and speed/attitude error;
- bounded steering/correction intent.

The Follower does not select engines and does not create a second global route.

### Autopilot pilot adapter

`src/game/navigation/autopilot/ShipControlAdapter.h`

Converts guidance into ordinary `ShipControlState` pilot inputs only:

- `pitchInput`, `yawInput`, `rollInput`;
- `targetSpeedRate`;
- `forwardInput`, `strafeInput`, `liftInput`;
- `VelocityAlignmentMode::BrakeToStop` (the ordinary END/autobrake action).

It MUST leave these legacy direct-navigation fields disabled:

- `navigationAccelerationDemandValid`;
- `navigationVelocityTargetValid`;
- `navigationPrecisionTranslationOnly`.

### Ship / flight law

`ShipController`, `LocalFlightControlStateMachine`, and
`DynamicMotionSystem::applyLocalFrameInput` own the causal response of the
vehicle to pilot controls.

They alone know how installed propulsion and control authority produce motion.

Assisted:

- pilot controls desired forward speed with the ordinary +/- control;
- hull nose is the travel direction;
- the ship flight law accelerates/brakes and realigns VREL to the nose.

Newtonian:

- pilot controls primary-main throttle with the ordinary + control;
- velocity remains inertial when thrust is released;
- braking uses the ordinary END/autobrake contract (turn and burn);
- keypad RCS remains available for small positioning corrections.

## STOP semantics

Measured near-zero velocity is never a maneuver state selector.

An authored STOP is explicit. During an authored STOP:

- normal main-engine/autobrake behavior removes large velocity;
- below the precision-stop entry band, ordinary keypad RCS removes the final
  residual velocity;
- values such as 0.045 m/s are treated as residual motion inside the STOP
  operation, not as a new MOVING state.

A new moving segment is explicit and must carry a non-zero movement command;
STOP and START must never be inferred from floating-point equality.

## Production automatic docking invariant

`GameServer.cpp` automatic docking must construct control via
`ShipControlAdapter::make()` and then call `ship->setControlState(...)`.

It must not write direct navigation actuator demand fields or call direct
navigation propulsion functions.

The architecture test `check_automatic_docking.py` enforces this boundary.
