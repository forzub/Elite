# Ship capability boundary

Status: foundation contract, 2026-10-12.

## One rule

Code above the ship physical plant asks **what the ship can do now**.
It does not ask which I/Y/X topology, engine slot or damaged actuator produced
that capability.

The dependency direction is:

```text
module damage/state
        |
        v
Assisted/Newtonian I/Y/X physical model
        |
        v
ShipCapabilitySnapshot
        |
        +--> control / gateway
        +--> autopilot
        +--> planner
        +--> client prediction
```

## Public capability surface

`ShipCapabilitySnapshot` exposes only motion capability:

- positive/negative longitudinal acceleration;
- positive/negative strafe/lift acceleration;
- positive/negative pitch/yaw/roll acceleration;
- positive/negative angular-rate limits;
- overall control-health state:
  `Normal / Degraded / Survival / Uncontrollable`.

The current backend also receives `effectivePhysics` as a temporary
compatibility projection. This field exists only to migrate the accepted
`SharedShipPhysics + DynamicMotionSystem` stack without changing navigation
and networking at the same time.

## Damage

Damage changes actuator slot state first.

Current integrated rule is intentionally simple:

```text
module attached and health > 0:
    thrust01    = health / maxHealth
    vectoring01 = health / maxHealth

module disabled/destroyed/detached/hanging-for-control:
    thrust01    = 0
    vectoring01 = 0
```

The physical I/Y/X model converts those slot values into ship capability.

A failed or impossible control direction is a legal degraded state. It is not
an exception, assertion failure or process-fatal condition.

## Healthy model interchange

A healthy ship descriptor selects one fixed physical model. Player input,
autopilot output and network `ShipControlState` do not change when the model
is replaced.

The matrix acceptance test therefore flies the same authored corridor with:

```text
Assisted-I
Assisted-Y
Assisted-X
Newtonian-I
Newtonian-Y
Newtonian-X
```

using the same nominal Cobra kinematic ratings and the native control law of
each physical family.

## Boundaries retained

- tactical speed cap remains control policy, not a physical maximum speed;
- route speed limits may be lower because of curvature/braking capability;
- reactor, power, thermal, fuel and RCS working-fluid accounting are not added
  in this milestone;
- jump/cruise drive remains a separate mechanic;
- navigation geometry and the visual Cobra model are not changed.
