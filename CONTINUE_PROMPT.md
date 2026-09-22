# CONTINUE PROMPT — Elite Navigation: implement Planner-owned physical ManeuverProgram

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST update:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- recreate this `CONTINUE_PROMPT.md` from scratch.

Keep `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md` synchronized with
ownership changes.

## Canonical terminology

Do not use vague "planner points + follower figures out the rest" language.

The hierarchy is:

```text
Planner
    -> Physical ManeuverProgram
       -> Autopilot/Follower
          -> Ship physics
```

Ruckig is an internal Planner helper.

## ManeuverProgram semantics

A point/state owns instantaneous physical state:

```text
time
position
velocity vector
acceleration vector
3-D body orientation
angular velocity
angular acceleration
```

A segment from state i to i+1 owns the actuator schedule:

```text
duration

rear main:
  enabled
  throttle
  throttle ramp/slew

front main:
  only if actual vehicle has it

manoeuvre/RCS:
  requested force/acceleration vector
  bounded by real hardware

attitude-control profile
```

"Engine runs for N seconds" is a segment property, not a point property.

## Planner ownership

Planner decides:
- obstacle route;
- broad arc / narrow maneuver / brake / flip / burn;
- tangent velocity;
- required acceleration;
- body orientation;
- main + RCS allocation;
- lead rotation;
- braking boundary;
- segment timing.

Planner may call Ruckig to solve bounded transition timing, velocity,
acceleration and jerk.

Ruckig does not own engine selection or maneuver doctrine.

## Autopilot ownership

Autopilot:
- obeys accepted ManeuverProgram;
- actuates real controls;
- closes bounded error;
- watches sudden obstacles;
- may perform immediate safety inhibition/emergency braking;
- requests replanning when nominal program is no longer safe/reachable.

Autopilot must not become a second nominal route planner.

## Existing diagnostics to preserve

- pink cross = current accepted reference point;
- violet cross = active phase endpoint;
- rear-main / fore-main / manoeuvre indicators;
- actual velocity vector and actual hull nose.

## Next implementation

1. Introduce explicit ManeuverState + ManeuverSegment data structure.
2. Adapt current accepted-program generation to populate it.
3. Move propulsion allocation and attitude schedule into Planner compilation.
4. Include throttle slew and finite angular rotation in feasibility.
5. Use Ruckig only inside physically feasible segment construction.
6. Feed Autopilot the resulting physical command program.
7. Add 30 m/s free-space broad-arc regression.

Do not compensate for bad planning by loosening follower tracking.
