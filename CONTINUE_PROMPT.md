# CONTINUE PROMPT — Elite Navigation viewer-first steady fly-through

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
and **recreate this CONTINUE_PROMPT.md from scratch again**.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/world/navigation/TrajectoryGenerator.h/.cpp`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tools/navigation_runtime/scenario.json`
- `src/game/navigation/ManeuverPhaseGate.h/.cpp`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `src/game/navigation/OrdinaryPhysicalManeuverCompiler.h/.cpp`

## Latest target evidence

Default route:
`(0,0,0) -> (110,-27,0) -> (190,-27,0) -> (300,0,0)`,
length 306.53 m.

The target headless output proved the important corner result:
`RETAINED WAYPOINT SPEEDS: P1=10.00, P2=10.00 M/S`.

So the former shallow-corner StopTurnGo defect is no longer present in the authored
Ruckig waypoint speeds.

That headless run still failed:
- FINAL_CAPTURE_TIMEOUT;
- final speed 1.01 m/s;
- final position error 32.94 m;
- MAX BODY/VELOCITY ANGLE 178.94 deg.

## Diagnosis

The last program always used `ManeuverPhaseGate::StateCapture`. That is correct only
for a stopped/parking terminal.

The current stand has finish speed 10 m/s, so its terminal is a fly-through boundary.
Holding/capturing the final point after the nominal moving trajectory ended caused the
artificial terminal braking and timeout.

Fix committed:
- `9b1251e8601172ecd83ba4f656871f92f4669fb4`:
  moving terminal -> final phase uses `ScheduledMoving`.

## User-requested workflow

All current maneuver behavior must be watched in the viewer.

Commit:
- `06513569f7861ac8b6e3104994ec72ffd0d891d1`:
  `run_stage1_mingw64.sh` no longer auto-runs the headless Stage-2 E2E.

The script still runs static/architecture gates and builds the viewer.

Do not re-add the headless Stage-2 test to this workflow while current maneuver
behavior is being iterated visually. The separate E2E test may remain in the project for
later regression use.

## Exact next commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then:
```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Inspect Expert / Standard / Newtonian first.

Legend:
- yellow thick arrow = actual velocity;
- cyan short arrow = physical hull nose;
- red arrow = program target nose;
- white line = retained route;
- green line = actual flown trajectory.

Expected:
- starts at 10 m/s;
- both shallow corners remain moving;
- finish is crossed at ~10 m/s;
- no final capture/braking.

## Then

If the fly-through looks correct at corners/final boundary, move directly to the deeper
Newtonian issue. The previous 178.94-degree body/velocity separation is a serious clue.

Determine whether yellow actual velocity changes before real cyan attitude/RCS/main
authority can physically support it. Fix that through B5/B6 physical maneuver authoring,
not viewer tricks or boosted RCS.

Do not start dynamic avoidance yet.

After every state-affecting event, synchronize all mandatory MD files and recreate this
prompt from scratch again.
