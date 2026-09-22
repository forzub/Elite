# CURRENT TASK — Introduce a real physical ManeuverProgram between Planner and Autopilot

Date: 2026-09-22

Status: **ARCHITECTURE CONTRACT ACCEPTED / IMPLEMENTATION NEXT**

Baseline before documentation commits:

```text
d3a566fb262512fa3fdbe1dc391db219abc46a4c
```

## Canonical ownership

```text
PLANNER
    owns route + physical maneuver
    may call Ruckig internally
        |
        v
PHYSICAL MANEUVER PROGRAM
        |
        v
AUTOPILOT / FOLLOWER
    executes it
    watches sudden hazards
        |
        v
SHIP PHYSICS
```

## Program representation

Do not treat a waypoint as if it simultaneously means position, an engine
command and a duration.

Use two concepts.

### ManeuverState

At instant t:

```text
t
position
velocity
acceleration
orientation quaternion/basis
angular velocity
angular acceleration
```

### ManeuverSegment

From state i to state i+1:

```text
duration

rear/aft main:
    enabled
    throttle command
    throttle slew/ramp

front main:
    enabled
    throttle command
    only when vehicle really has that engine

manoeuvre/RCS:
    commanded force/acceleration vector
    bounded by hardware

attitude:
    target/profile for the interval
```

Dense sampling is allowed, but these semantics must remain explicit.

## Ruckig ownership

Ruckig is subordinate to Planner.

It may solve jerk-limited transition timing or a feasible state transition.

It must NOT decide:
- engine allocation;
- body orientation doctrine;
- obstacle route;
- braking policy;
- whether to use a broad arc or a stop/flip/burn maneuver.

## Autopilot ownership

Autopilot:
- tracks the accepted program;
- actuates the actual engines and attitude controls;
- closes bounded tracking error;
- watches sudden obstacles;
- may safety-inhibit/emergency-brake;
- requests replanning if the accepted program becomes unsafe/unreachable.

It does not silently replace the nominal trajectory.

## Immediate implementation order

1. Introduce an explicit maneuver-program data model with State + Segment.
2. Make current trajectory/reference generation populate it rather than hiding
   propulsion decisions downstream.
3. Put real engine allocation into Planner/maneuver compilation.
4. Integrate throttle slew and angular rotation time into planned segment
   feasibility.
5. Move Ruckig calls inside those segment/state solves.
6. Make Follower consume the resulting propulsion/attitude program.
7. Preserve the new pink current-reference and violet phase-endpoint diagnostics.
8. Add a 30 m/s free-space turn regression where Planner may choose a broad
   constant-speed arc instead of braking.

Do not add another follower-side workaround before this boundary exists.
