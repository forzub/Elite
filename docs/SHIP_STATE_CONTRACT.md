# Ship State Contract

## Purpose

Every materialized ship instance -- player, NPC, bot or diagnostic actor -- owns
one canonical `game::ship::state::ShipState`.

The state is role-neutral. Ownership decides who writes control intent; it must
not select a different vehicle-physics representation.

## Construction

`ShipDescriptor` remains immutable shared definition data. Per-instance
construction selection lives in `ShipState::construction`.

Production spawn binds the descriptor's physical construction once. Diagnostic
flight tests may switch between Assisted/Newtonian I/Y/X by changing this state
without resetting position, velocity or orientation.

No caller may implement six vehicle-specific flight branches. Construction is
data consumed by the capability/allocator layer.

## Actuators

Per-instance actuator availability lives in `ShipState::actuators`.

Normal gameplay projects module health/state into this block at the common ship
control seam. Damage changes thrust/vectoring authority but does not change the
construction family/layout.

Diagnostic tests may take explicit ownership of actuator state. Module
projection must not overwrite that override until it is cleared.

## Control

`ShipControlState` is part of the canonical ShipState. Human input, NPC AI,
autopilot and scripted diagnostics all write the same control surface.

## Kinematics

During this migration, authoritative kinematics remain in `ShipTransform`.
They are deliberately not duplicated inside ShipState. A later migration may
move the storage, but there must never be two independent position/velocity/
orientation truths.

## Runtime capability

The capability path is:

```
ShipState.construction
        +
ShipState.actuators
        |
        v
PhysicalControlSnapshot
        |
        v
ShipCapabilitySnapshot
        |
        v
ShipControlExecutionGateway / autopilot
```

`RuntimeFlightRegime` is a derived diagnostic classification. It must not
become an alternate set of ship-specific flight implementations.
