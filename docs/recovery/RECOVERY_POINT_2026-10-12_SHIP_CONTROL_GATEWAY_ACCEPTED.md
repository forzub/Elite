# Recovery point — Ship control execution gateway accepted

Date: 2026-10-12

Status: ACCEPTED / restore-safe.

This recovery point freezes the ship-control architecture immediately before
the component-level ShipPhysicalPlant work begins.

## Accepted behavior

The refactor is intentionally behavior-neutral for a healthy ship.

Human input, client autopilot and NPC control still produce ordinary
`ShipControlState`. The server remains authoritative. Client prediction uses
the same execution seam as server authority.

Visual acceptance after the gateway refactor:

- guidance tunnel geometry is good;
- ship motion through the tunnel is good;
- no observed control regression.

Accepted navigation stress run:

- file: `elite_navigation_stress(20261012-113500).log`
- scenario: `hot-replan-blue-transit-v2`
- requests: 12
- failures: 0
- planner retries: 0
- emergency stops: 0
- BLUE entered: 1
- BLUE exited: 1
- status: COMPLETE
- max speed: 404.578984 m/s
- max angular speed observed: 1.347511 rad/s
- safety course corrections: 1
- planning safety overlap samples: 48

The corridor stress is now a regression benchmark for future ship-physics work.
Navigation geometry/optimizer tuning is not part of the next phase unless a
future regression proves that navigation itself is responsible.

## Accepted execution boundary

```text
Human / Autopilot / NPC
        |
        v
ShipControlState
        |
        +---------------- client prediction ----------------+
        |                                                   |
        v                                                   v
network                                             ShipControlExecutionGateway
        |                                                   |
        v                                                   v
GameServer                                         current physics backend
        |
        v
Ship / ShipCore
        |
        v
ShipControlExecutionGateway
        |
        +--> SharedShipPhysics
        +--> DynamicMotionSystem
        |
        v
authoritative motion
```

`ShipControlExecutionGateway` is the only public execution seam between
`ShipControlState` and the physical response of the ship.

## Ownership rules

1. `GameSimulation` owns cadence, activation and reference-frame orchestration.
   It does not interpret `ShipControlState` to choose Assisted, Newtonian,
   precision-navigation, RCS or acceleration-demand execution.
2. `ShipCore` crosses the gateway for control execution. It does not directly
   invoke the current low-level control backends.
3. Fixed client prediction and fractional presentation prediction use the same
   gateway API.
4. Network transport remains command-based: the client sends numbered
   `ShipControlState` samples, never authoritative position, velocity,
   force, torque or engine output.
5. Current `SharedShipPhysics` and `DynamicMotionSystem` behavior is private
   behind the gateway and may later be replaced without changing the upper
   control/network contracts.
6. The gateway has no UI, renderer, network-session, planner or world
   orchestration dependency.
7. Server authority remains mandatory even if the future component plant is
   predicted in full on the client.

## Architecture guards accepted

The following guards passed together before this recovery point:

```text
check_ship_control_execution_gateway.py
check_local_flight_control.py
check_hub_tactical_prediction.py
check_local_predicted_presentation.py
check_travel_frame_ownership.py
check_navigation_live_runtime_control.py
```

These guards were also updated to the current navigation execution
architecture:

- `ClientRouteAutopilot`;
- `sampleContinuousReference()`;
- `ManeuverTrackingController::track()`;
- `sampleRouteCurveAtProgress()`;
- `PredictivePilot::make()`;
- `HullAttitudeControl::evaluate()`;
- learned authority / `updateIdentification()`.

Retired `RouteFollower::follow()`, `TrajectoryFollower`,
`choosePredictiveAxisInput()` and obsolete direct-control expectations must
not be restored merely to satisfy old guards.

## Next phase: ShipPhysicalPlant

The next implementation phase belongs behind the accepted gateway:

```text
ShipControlState
        |
        v
ShipControlExecutionGateway
        |
        v
FlightControlComputer
        |
        v
DesiredWrench
        |
        v
ActuatorAllocator
        |
        v
ShipPhysicalPlant
   +-- frame / mass / center of mass / inertia
   +-- individual main engines
   +-- individual RCS / manoeuvring thrusters
   +-- propellant
   +-- reactor / power distribution
   +-- cooling / thermal nodes
   +-- damage / degradation
        |
        v
ActualWrench
        |
        v
rigid-body motion
```

The first component-model milestone must preserve the healthy ship's present
flight behavior. The existing corridor stress is the acceptance benchmark.

Do not introduce fuel, power or thermal complexity until the real thruster
layout + allocator can reproduce the current healthy ship control envelope.

## Recovery intent

This branch is the fallback point if component-ship development destabilizes
control, prediction, networking or navigation execution.

The restore branch created from this commit should be treated as immutable.
