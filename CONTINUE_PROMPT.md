# CONTINUE PROMPT — Elite Navigation: physically compile the maneuver before Ruckig timing

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. recreate this `CONTINUE_PROMPT.md` from scratch.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `src/world/navigation/TrajectoryGenerator.cpp/.h`
- `src/game/navigation/RuckigTrajectorySolver.cpp/.h`
- `src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp/.h`
- `src/game/navigation/DynamicMotionSystem.cpp`
- Cobra descriptor / ShipParams definitions.

## Latest architectural finding

Higher-speed corner overshoot cannot be fixed by follower tuning alone.

Actual current multi-point chain:

```text
coarse route
 -> local Bezier execution guide p(s)
 -> ONE scalar Ruckig progress solve s(t)
 -> map tangent/curvature acceleration
 -> author attitude afterwards
 -> follower tries to execute
```

Ruckig is therefore not currently solving each route waypoint as a full 3-D
ship state. It does not know hull rotation, aft-main pointing, combined
main/RCS allocation, or lead-rotation braking distance.

## Cobra data findings

Runtime harness currently uses duplicated hard-coded `cobraParams()`, not the
authoritative descriptor.

Inspected duplicated values:
- pitch/yaw 2.5 rad/s
- roll 3.0 rad/s
- angularAccel 3.0 rad/s^2
- manoeuvreThrusterAccel 2.0 m/s^2
- maxLinearGs 7.5
- turnRadius 20 m
- maxCombatSpeed 500 m/s.

The trajectory profile maps `maxLinearGs*9.80665 ~= 73.55 m/s^2` into both
forward and braking acceleration. Treat this as suspicious: it is an envelope,
not an engine-allocation model.

## Required next architecture

The planner/trajectory layer must compile a physically feasible maneuver before
final Ruckig timing.

For a material corner:
1. know incoming position/velocity/hull attitude/angular rate;
2. choose a broad free-space arc/transition geometry appropriate to speed;
3. compute needed acceleration vector along it;
4. solve whether RCS alone is trim or hull must rotate for aft main;
5. include finite angular acceleration/rate in lead-rotation time;
6. derive where turning/braking must start;
7. only then time the feasible primitive with Ruckig or equivalent state solver.

At 30 m/s, if free space exists, the planner should produce a broad smooth arc,
not cling to a sharp polyline corner and hope follower fixes it.

## Immediate coding order

1. Replace hard-coded `cobraParams()` with authoritative Cobra profile access.
2. Introduce explicit propulsion capability separate from max pilot/load G.
3. Add maneuver-feasibility / lead-distance computation.
4. Make execution-guide corner radius/transition length use that feasibility.
5. Add exact 30 m/s free-space broad-arc regression.
6. Then evaluate whether scalar Ruckig progress remains sufficient or whether
   selected primitives need explicit state-to-state Ruckig calls.

Do not add another follower recovery workaround for this issue.
