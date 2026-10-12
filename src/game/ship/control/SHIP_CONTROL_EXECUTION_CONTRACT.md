# Ship Control Execution Contract

Status: migration seam established 2026-10-12.

## Purpose

`ShipControlExecutionGateway` is the single execution boundary between
`ShipControlState` and the ship's physical response.

Control producers remain above this boundary:

- human input / `PlayerInputMapper`;
- client `PredictivePilot` / autopilot;
- NPC pilot logic.

Transport also remains above it. The network sends numbered
`ShipControlState` samples. It does not send authoritative position,
velocity, force, torque, engine thrust or actuator state.

## Current execution path

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
        +--> SharedShipPhysics          (attitude/control-state backend)
        |
        +--> DynamicMotionSystem        (translation backend)
        |
        v
authoritative motion
```

`GameSimulation` owns scheduling, activation cadence and reference-frame
orchestration. It must not inspect `ShipControlState` to choose Assisted,
Newtonian, RCS, precision-navigation or acceleration-demand execution.

## Future physical-plant insertion

The future component ship model belongs behind this seam:

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
   +-- individual thrusters
   +-- propellant
   +-- reactor / power buses
   +-- thermal / cooling
   +-- damage / degradation
        |
        v
ActualWrench
        |
        v
rigid-body motion
```

Replacing the current backend must not require changes to:

- keyboard mapping;
- autopilot/follower output contract;
- `ClientMessage` / transport;
- `GameServer::receiveClientMessage`;
- control-tick acknowledgement/reconciliation;
- `GameSimulation` control-law branching (there must be none);
- client prediction call sites.

## Phase split

Server activation deliberately separates:

1. control evaluation;
2. orientation propagation;
3. translation control evaluation;
4. translation propagation;
5. slower ship systems.

The gateway therefore exposes phase methods instead of a single mandatory
monolithic step. This preserves coarse/decimated control cadence while allowing
kinematics to remain fixed-step.

## Capability input

For the migration step, the gateway consumes the existing damage-aware
`ShipParams`/effective physics profile. This preserves current behavior.

A later component model may replace that input internally with a
`ShipCapabilitySnapshot`/physical-plant state. Callers must not depend on the
backend representation.

## Ownership rules

1. Only the gateway interprets `ShipControlState` for translation execution
   path selection.
2. `GameSimulation`, client prediction and presentation must not call
   `DynamicMotionSystem` control-entry methods directly.
3. `ShipCore` must not call `SharedShipPhysics` or
   `DynamicMotionSystem` directly for pilot-control execution.
4. Client prediction and server authority use the same gateway API.
5. The gateway has no transport, UI, renderer, planner or session dependency.
6. Server remains authoritative even after a detailed client-side plant
   predictor is introduced.

Architecture guard:
`tests/architecture_contracts/check_ship_control_execution_gateway.py`.

Runtime regression:
`ship_control_execution_gateway`.
