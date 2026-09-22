# CONTINUE PROMPT — Elite Navigation: validate explicit Planner actuator program

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. recreate this `CONTINUE_PROMPT.md` from scratch.

Keep `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md` synchronized when the
Planner/Autopilot contract changes.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `src/game/navigation/AcceptedManeuverProgram.h`
- `src/game/navigation/ManeuverProgramSampler.cpp/.h`
- `src/game/navigation/TrajectoryFollower.cpp/.h`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationTrace.cpp/.h`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`.

## Current code candidate before docs

```text
7b60f875193334e20bc5d168d65df351b746754d
```

Target-machine PASS has NOT been established.

## Canonical ownership

```text
Planner
    -> physical ManeuverProgram
       -> Autopilot/Follower
          -> ship physics
```

Ruckig is an internal Planner helper.

## Implemented in this slice

`AcceptedManeuverProgram` now contains:
- state/reference samples;
- explicit `ActuatorSegment` intervals.

Each actuator interval contains:
- duration;
- rear-main enable + throttle start/end;
- explicit fore-main enable + throttle start/end;
- manoeuvre/RCS acceleration start/end;
- propulsion-feasibility witness.

Current Cobra has no fore main; Planner keeps that channel OFF.

Current Stage-12 compiler decomposes each reference acceleration:
- positive component along planned hull forward -> rear main;
- residual vector -> RCS;
- RCS is bounded by real `manoeuvreThrusterAccel`;
- authority excess marks the segment SATURATED / infeasible.

B9 samples these commands directly.
Follower exposes them without re-solving engine choice.

## New diagnostics

Viewer line:
```text
ПЛАН SEG N: MAIN xx% | FRONT 0% | RCS x.x M/S2 | FEASIBLE/SATURATED
```

Existing engine lamps are ACTUAL physics.

Telemetry adds:
```text
plan_seg
plan_main_pct
plan_front_pct
plan_rcs
plan_feasible
```
beside actual main/RCS telemetry.

Pink cross remains the instantaneous accepted reference.
Violet cross remains the active phase endpoint.

## Important current limitation

The explicit actuator schedule is still OBSERVE-ONLY in Stage-12 execution.

Diagnostics say:
`AUTOPILOT ACTUATOR EXECUTION: OBSERVE-ONLY MIGRATION`.

Do not hide this. The next run must first determine whether Planner's explicit
engine schedule is physically sane.

## Run next

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

First use the same higher-speed Newtonian case.

At first bend inspect:
- pink reference;
- `ПЛАН SEG` line;
- FEASIBLE/SATURATED;
- actual engine lamps;
- velocity vector;
- hull nose.

If Planner schedule is sane:
next iteration wires sampled actuator intervals into Autopilot/physics and keeps
bounded tracking feedback separate.

If Planner schedule is saturated or late:
fix physical maneuver authoring / broad-arc / lead-rotation first, not Follower.
