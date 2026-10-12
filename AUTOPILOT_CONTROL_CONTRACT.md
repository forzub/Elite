# AUTOPILOT_CONTROL_CONTRACT

## Purpose

The route Planner owns route geometry and the accepted speed/attitude reference.
The route follower measures deviation from that accepted route.  A flight-law
autopilot converts the resulting desired/observed vehicle state into ordinary
`ShipControlState` pilot controls.

Production automatic docking must not select engines, inject SYSTEM-frame
acceleration, or call navigation-only propulsion helpers.

## Hard API boundary

The only route-to-pilot data contract is:

`src/game/navigation/autopilot/AutopilotControlApi.h`

Input:

- `AutopilotControlRequest`
- `ShipParams`

Output:

- `AutopilotControlCommand` (ordinary `ShipControlState`)
- read-only `AutopilotControlTelemetry`

Forbidden across this boundary:

- engine ids or slot ids;
- propulsion topology;
- bank health/throttle state;
- concrete Assisted/Newtonian controller state;
- direct actuator allocation;
- RCS/main-engine selection.

`ClientRouteAutopilot` may depend only on the hard API and
`AutopilotLawController`. It must never include a concrete pilot or read its
private identification state.

## Replaceable flight-law modules

The concrete pilots are siblings:

- `AssistedAutopilot`
- `NewtonianAutopilot`

Both implement the same public calls:

- `update(const AutopilotControlRequest&, const ShipParams&)`
- `telemetry(const ShipParams&) const`
- `reset()`

`AutopilotLawController` is the only selection seam. Compile-time parity
guards prevent either concrete pilot from drifting to a different request,
command, or telemetry shape.

Controller state is isolated per law. Running Assisted must not mutate or
initialize Newtonian state, and vice versa.

## Ownership

### Planner

Owns collision-free geometry, corridor/centerline, speed schedule, stopping
points, physically feasible route dynamics, and route replacement.

The Planner does not press controls.

### Route follower

Owns route progress, cross-track/course/speed error, and bounded vehicle-state
guidance.

The follower does not select engines or concrete actuator doctrine.

### AssistedAutopilot

Owns the Assisted conversion from vehicle-state guidance to ordinary pilot
controls. Its measured course/speed response is private implementation state;
the route layer receives only neutral telemetry.

### NewtonianAutopilot

Owns the Newtonian conversion from the same vehicle-state guidance to the same
ordinary pilot-control surface.

Ordinary Newtonian route translation follows these rules:

- meaningful delta-v belongs to the main engine;
- bounded main-nozzle TVC may absorb the part of acceleration-direction error
  that fits inside the installed physical gimbal envelope;
- the hull rotates only the TVC-unserved remainder of the acceleration-angle
  error;
- nozzle travel and nozzle slew are physical limits from `ShipParams`, not
  route/planner tuning constants;
- braking therefore remains turn-and-burn;
- translational RCS must not substitute for main thrust or TVC;
- RCS translation is reserved for explicit precision-stop residuals.

The route/planner layer does not command gimbal axes and does not inspect
`mainGimbal*` controls. The geometric planner is unchanged by TVC.

Newtonian execution authority is flight-law-specific:
`newtonian/NewtonianExecutionAuthority.h` treats the aft main as the owner of
the complete route acceleration vector after hull/TVC orientation. Sustained
route-normal acceleration and follower correction MUST NOT be budgeted from
manoeuvre/RCS authority. RCS remains attitude/precision hardware.

Runtime speed-profile updates own only the longitudinal component of the
reference acceleration. They MUST preserve Newtonian authored path-normal
(curvature/centripetal) feed-forward. A soft SpatialCorridor tracking-envelope
excursion must not erase Newtonian main-vector feed-forward; doing so removes
the very authority required to recover the velocity vector.

The shared TrajectoryGenerator may consume that Newtonian-specific capability
snapshot, but Assisted capability and execution semantics must remain
unchanged. Any future Newtonian-only route optimization must remain behind this
capability/policy boundary rather than modifying Assisted behavior.

### Ship / flight law

`ShipController`, `ShipControlExecutionGateway`, and the physical motion
backend own causal actuator response. The autopilot presses ordinary controls;
it does not allocate installed engines itself.

`mainGimbalRightInput` / `mainGimbalUpInput` are ordinary physical TVC
controls carried in the common `ShipControlState` shape. Assisted leaves them
zero. The Newtonian physical layer rate-limits them and converts actual nozzle
position into the real main-thrust vector.

## STOP semantics

Measured near-zero velocity is never a maneuver-state selector.

An authored STOP is explicit. Large Newtonian stop velocity is removed through
ordinary main-engine/autobrake turn-and-burn. Only the final precision-stop
residual may be removed with ordinary translation RCS.

## Forbidden legacy navigation seam

Both production pilots MUST leave disabled:

- `navigationAccelerationDemandValid`
- `navigationVelocityTargetValid`
- `navigationPrecisionTranslationOnly`

Those fields remain diagnostic/legacy only.

## Isolation invariant

A navigation tick has exactly one concrete pilot owner selected by
`AutopilotLawController`.

No tick may combine Assisted and Newtonian output, blend their private state,
or fall back to direct navigation actuator demands. Failure/recovery must be
explicit.

The `autopilot_law_api` contract test enforces API parity, private state
isolation, Newtonian main-engine ownership, and precision-only RCS translation.
