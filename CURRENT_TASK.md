# CURRENT TASK — inspect steady 10 m/s fly-through in viewer, then fix Newtonian body/thrust semantics

**Date:** 2026-09-21  
**Status:** CORNER SPEED FIX CONFIRMED IN TARGET OUTPUT / FINAL-CAPTURE BUG FIXED / VIEWER-FIRST

## Latest target evidence

The current default retained route is:
```text
(0,0,0)
 -> (110,-27,0)
 -> (190,-27,0)
 -> (300,0,0)
length 306.53 m
```

Target output confirms:
- start speed = 10 m/s;
- `RETAINED WAYPOINT SPEEDS: P1=10.00, P2=10.00 M/S`;
- no coarse static contact;
- Ruckig generated the trajectory.

The headless run then failed with:
- `FINAL_CAPTURE_TIMEOUT`;
- final speed 1.01 m/s;
- final position error 32.94 m;
- max body/velocity angle 178.94 deg.

## Root cause of that failure

The final phase was always `StateCapture`.

That is wrong for a moving finish. A 10 m/s terminal is a fly-through boundary, not a
parking target. Continuing to capture one fixed endpoint after the trajectory horizon
artificially brakes the ship and eventually times out.

Fix:
- `9b1251e8601172ecd83ba4f656871f92f4669fb4`:
  final phase is `ScheduledMoving` whenever authored finish speed is non-zero.

## Viewer-first workflow

Per user request, current maneuver quality is evaluated in the interactive viewer.

`run_stage1_mingw64.sh` now:
- runs static/architecture checks;
- builds the viewer;
- does **not** auto-run the headless Stage-2 E2E.

Commit:
- `06513569f7861ac8b6e3104994ec72ffd0d891d1`.

The headless E2E remains available separately and will be restored as a required
regression after moving-terminal semantics and body/thrust authoring stabilize.

## Immediate next action

Run:
```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then inspect the entire maneuver in:
```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Watch:
- yellow = actual velocity vector;
- cyan = actual physical hull nose;
- red = program/Follower target nose;
- white = retained geometric route;
- green = actual path.

Expected stand behavior:
- start already at 10 m/s;
- no stop at either shallow corner;
- no artificial braking at finish;
- cross finish at ~10 m/s.

## After visual confirmation

The next real mechanism task is Newtonian body/thrust-aware maneuver authoring.

The target output already showed `MAX BODY/VELOCITY ANGLE: 178.94 DEG`, so even after
removing StopTurnGo there is still a serious semantics issue to inspect. Translation
must not behave as if future hull attitude already exists.

Preserve:
B4 geometry -> B5 physical maneuver -> B6 continuous proof -> B7 doctrine ->
B8 AcceptedManeuverProgram -> B9/B10 Follower/Pilot execution.

Do not start dynamic obstacle avoidance yet.

## Mandatory state protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
