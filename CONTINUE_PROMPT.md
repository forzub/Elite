# CONTINUE PROMPT — Elite Navigation: physical reacquisition + damped attitude

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `src/game/navigation/ManeuverTrackingController.cpp/.h`
- `src/game/navigation/ManeuverPhaseGate.cpp/.h`
- `src/game/navigation/TrajectoryFollower.cpp/.h`
- `tests/navigation_runtime/ManeuverTrackingControllerTests.cpp`
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`.

## Latest target evidence

User ran Newtonian / Expert / Standard around:
- START 26.15 m/s;
- FINISH 11.75 m/s.

Old diagnostics:
- Ruckig OK;
- program phases completed;
- final physical error 68.49 m;
- max route deviation 62.24 m;
- max follower error 68.49 m.

Viewer showed the reference/path continuing while the physical ship failed to return.

Telemetry also showed visible hull attitude overshoot/oscillation.

## Root cause 1 — B10 angular loop was underdamped

Old default:
```text
Kp attitude = 2
Kd angular velocity = 1
```

Critical Kd for Kp=2 is about 2.83.

Current:
```text
Kp = 2
Kd = 3
```

Do not move damping responsibility into a hidden ShipController navigation term.
B10 owns angular feedback; lower physics still clamps angular authority.

Regression:
default Kd >= 2*sqrt(Kp).

## Root cause 2 — program reference outran physical execution

Do NOT mutate AcceptedManeuverProgram after acceptance.

Current runtime owns:
```text
activeProgramReferenceDelaySeconds
```

Sampling:
```text
programReferenceTime = vehicle.timeSeconds - delay
```

When `follower.trackingErrorExceeded`:
- delay += dt;
- B9/Follower reference progress pauses;
- gate uses the same delayed reference time;
- physical control continues trying to reacquire;
- no timed phase handoff while actual craft is outside envelope.

When tracking recovers, program reference time resumes.

Viewer status:
`FOLLOWER ВОЗВРАЩАЕТСЯ В КОРИДОР`.

Diagnostics:
- REFERENCE CLOCK HOLD FRAMES
- REFERENCE CLOCK HOLD

Current reacquisition envelope:
- position 8 m;
- velocity 4 m/s;
- forward angle 0.35 rad;
- angular velocity 0.8 rad/s.

FreeTransit longitudinal deadbands are applied BEFORE envelope evaluation.

## Completion semantics

Diagnostics now say:
- PROGRAM PHASES COMPLETE
- PHYSICAL TERMINAL STATE

Never use wall-clock program completion as proof of physical arrival.

## Actuator semantics

A body turn with MAIN=0 is not automatically wrong.

Angular torque (attitude/RCS) and main linear thrust are separate. Telemetry witnesses:
- `exec_ang_cmd` = attitude acceleration request;
- `main_pct` = main linear engine.

The bug to eliminate is unnecessary or oscillatory attitude motion, not the fact that a
ship can rotate without firing the main engine.

## Assisted

Requested/effective selector state remains separate:
- selected law = next Calculate input;
- effective law = displayed runtime trace.

Do not undo that.

After current damping/reacquisition target gate is green, if Assisted still turns hull
more sharply than needed, change Assisted reference-attitude authoring specifically.
Do not fake it in the viewer and do not couple FlightStyle to control law.

## Logs and traces

Everything current-run is now in repository root:
- last_route_plan.log
- last_execution.log
- last_execution_telemetry.log
- navigation_perf.log
- last_calculated_trace.json
- last_execution_trace.json

## High-speed regression

`NavigationScenarioRuntimeE2ETests.cpp` now reproduces:
- Newtonian / Expert / Standard;
- 26.15 -> 11.75 m/s.

It requires physical success and final position error <= 5 m.

## Target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Do not claim PASS without target evidence. Do not enable dynamic avoidance yet.
