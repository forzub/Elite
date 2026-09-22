# CURRENT TASK — Validate Planner actuator program before switching Autopilot execution

Date: 2026-09-22

Status: **FIRST MANEUVER-PROGRAM SLICE IMPLEMENTED / TARGET VALIDATION REQUIRED**

Code baseline before documentation commits:

```text
7b60f875193334e20bc5d168d65df351b746754d
```

## What is implemented

`AcceptedManeuverProgram` now has explicit physical command intervals.

### State

Existing `ReferenceSample` remains the instantaneous target state:

```text
time
position
velocity
linear acceleration
body basis/orientation
angular velocity
angular acceleration
```

### Segment

New `ActuatorSegment[i -> i+1]` owns:

```text
duration

rear main:
    enabled
    throttle start/end

front main:
    enabled
    throttle start/end

manoeuvre/RCS:
    acceleration vector start/end

propulsionFeasible
```

For current Cobra, fore-main is always OFF.

## Current compiler behavior

For each reference sample:

```text
requested acceleration
    -> project onto planned hull forward
    -> positive forward component = rear-main acceleration
    -> remainder = manoeuvre/RCS
    -> clamp RCS to real manoeuvreThrusterAccel
    -> mark infeasible if required authority exceeded
```

This is the first explicit propulsion schedule. It does not yet solve the
higher-level broad-arc/lead-rotation problem; it exposes whether the existing
trajectory can even be represented as a physical engine program.

## Observability

Viewer now shows:

```text
ПЛАН SEG N: MAIN xx% | FRONT 0% | RCS x.x M/S2 | FEASIBLE/SATURATED
```

The existing lamps still show ACTUAL physical engines.

Telemetry now places planned and actual propulsion beside each other:

```text
plan_seg
plan_main_pct
plan_front_pct
plan_rcs
plan_feasible

main_pct
main_a
rcs_a
engine_a
```

Pink cross = current sampled reference.
Violet cross = active program endpoint.

## Important limitation of this slice

Autopilot does NOT yet literally apply the new actuator schedule.

Execution is still the old net-acceleration path. The run prints:

```text
AUTOPILOT ACTUATOR EXECUTION: OBSERVE-ONLY MIGRATION
```

This is intentional for one gate: first inspect whether Planner is producing a
sensible engine schedule. If Planner already says SATURATED or commands a
nonsensical main/RCS sequence, do not wire that bad program into physics.

## Run next

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Use the same higher-speed Newtonian case first.

At the first bend, compare:
- pink reference point;
- planned MAIN/RCS line;
- actual engine lamps;
- actual velocity vector;
- actual hull nose.

Questions for the next iteration:
1. Does Planner mark the program FEASIBLE or SATURATED?
2. Does planned rear-main throttle rise BEFORE the bend where material delta-v
   is needed?
3. Does planned RCS stay trim-sized or does it still carry the whole maneuver?
4. Is the pink reference itself already too sharp / too late?

If the planned program is sane, next iteration switches Autopilot from inferred
net acceleration to direct execution of `ActuatorSegment` plus bounded
tracking correction.

Do not tune follower envelopes before this gate.
