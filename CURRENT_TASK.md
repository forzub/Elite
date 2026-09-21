# CURRENT TASK — validate reducer-synced control law, speed corridor, and actual main-engine indication

**Date:** 2026-09-21  
**Status:** IMPLEMENTED / TARGET VIEWER VALIDATION REQUIRED

## User-observed issues being addressed

1. After switching to EXTREME, the UI could still show ASSISTED while motion looked
   Newtonian.
2. Free transit was over-controlling exact 10.0 m/s; a harmless 10.1 m/s deviation could
   provoke corrective thrust/attitude.
3. Viewer did not clearly distinguish a physically useful main-engine vectoring turn
   from an attitude rotation with no aft-main thrust.

## Implemented state architecture

Viewer is now reducer-driven:
```text
UI click/runtime observation
 -> ViewerAction
 -> reduceViewerState(AppState)
 -> buttons/HUD are projections of AppState
```

Runtime frames publish the effective law. Playback feeds it back into the same store.
If runtime is NEWTONIAN, the NEWTONIAN button must become selected even if the previous
user request was ASSISTED.

Diagnostics:
- CONTROL LAW REQUESTED
- CONTROL LAW EFFECTIVE
- CONTROL LAW SWITCHES

## Implemented free-transit corridor

Follower no longer treats the route as an exact longitudinal timetable.

Current `FreeTransit` deadbands:
- STANDARD: speed +/-0.5 m/s, progress +/-12 m;
- EXTREME: speed +/-1.0 m/s, progress +/-16 m.

Cross-track control remains active. Precision maneuver families are not loosened.

Viewer displays the active speed corridor.

## Implemented physical main-engine indicator

Trace frame records actual positive aft-main throttle.

Viewer:
- orange filled rear face = actual aft main engine producing thrust;
- intensity follows throttle;
- HUD shows MAIN: N%;
- RCS alone does not light the rear face.

## Latest source HEAD before mandatory doc sync

`a60a215fde65505826b9b96c70edf511991da91f`

## Target validation

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Run at least:
1. ASSISTED + STANDARD
2. ASSISTED + EXTREME
3. NEWTONIAN + STANDARD
4. NEWTONIAN + EXTREME

Check each:
- NAV REV visible;
- selected control-law button matches actual/effective runtime law;
- REQUESTED/EFFECTIVE/SWITCHES diagnostics are sensible;
- speed corridor shown;
- ~0.1 m/s harmless speed error does not provoke longitudinal correction;
- orange rear face appears only when main engine is truly active;
- MAIN:% agrees with rear-face indication.

If EXTREME appears Newtonian while diagnostics say EFFECTIVE=ASSISTED and SWITCHES=0,
then the visual similarity is in Assisted execution behavior, not a hidden law switch,
and must be debugged there.

## Mandatory state protocol

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
