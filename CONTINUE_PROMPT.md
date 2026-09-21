# CONTINUE PROMPT — Elite Navigation: speed-aware route + explicit Calculate state

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read those files first, plus:
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp/.h`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tools/navigation_runtime/scenario.json`
- `tools/navigation_runtime/README.md`
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`
- `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`.

## Non-negotiable current semantics

### FlightStyle

Never introduce Standard/Extreme nominal speeds.

FlightStyle answers only the clearance/risk tradeoff:
- STANDARD -> more safety/maneuver room from static obstacles;
- EXTREME -> tighter pass / less clearance when useful tactically.

There must be no:
- standard_speed_mps / extreme_speed_mps;
- standardSpeedMps / extremeSpeedMps;
- styleSpeedMps.

The current test stand speed is explicitly requested by START/FINISH settings.

### Speed is route-affecting

START/FINISH speed changes must invalidate Stage-1 because higher inertia changes
required maneuver room.

Current coarse implementation:
```text
planningSpeed = max(startSpeed, finishSpeed)
inertialLead = planningSpeed * characteristicTurnTime(real ship angular limits)
additionalClearance =
    authoredClearance +
    inertialLead * styleReserveFactor
```

Current style factors:
- STANDARD 1.0
- EXTREME 0.35

This is coarse Stage-1 maneuver reserve, not final B6 swept-volume proof.

Expected visible behavior:
- higher speed generally moves white route support points farther from the obstacle;
- same speed EXTREME is closer than STANDARD;
- topology can remain the same even when point coordinates move.

### Calculate button contract

Settings do NOT auto-run calculations.

Dirty state:
- speed/style => Stage-1 route + Stage-2 dirty;
- pilot/control law/dynamic toggle => Stage-2 dirty only.

`РАССЧИТАТЬ` has exactly one meaning:
- run all calculations required by current dirty state;
- if Stage-1 dirty, rebuild route first;
- then execute Stage-2;
- show concise on-screen result log.

After the attempt:
- button disabled and visually dim;
- label `РАСЧЕТ ГОТОВ`;
- same inputs cannot trigger another solve;
- changing an invalidating input marks result stale and re-enables button.

Do NOT restore:
- automatic Stage-2 refresh on selector clicks;
- second Execute button/action;
- `ЗАПУСТИТЬ ПОЛЁТ` workflow.

Playback controls only playback.

## Current somersault candidate

Previous telemetry proved the main engine was not commanding the old full hull flip.
The angular loop kept winding up after reference attitude had settled.

Current candidate fixes still require target validation:
- FreeTransit sparse angular-acceleration feed-forward = zero;
- total follower angular acceleration demand clamped to physical capability;
- telemetry logs ideal/pilot-executed linear/angular commands and physical main/RCS
  allocation.

Do not undo those fixes while working on route/UI semantics.

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

Validate:
1. initial Calculate enabled;
2. Calculate once at Standard 10/10;
3. short log visible; button disabled;
4. change to 40/40 -> button enabled/stale;
5. Calculate -> white route farther from obstacle;
6. switch 40/40 Standard to Extreme -> Calculate -> route closer;
7. pilot/control-law change re-enables Calculate but does not require Stage-1 route
   geometry rebuild;
8. viewer REV matches target HEAD/build.

If compile/test fails, fix the actual problem and repeat mandatory MD/prompt protocol.

Do not enable dynamic avoidance yet.
