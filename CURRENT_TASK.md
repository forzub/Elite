# CURRENT TASK — reproduce remaining hull somersault with telemetry and speed sliders

**Date:** 2026-09-21  
**Status:** TELEMETRY + 5..50 M/S START/FINISH SLIDERS IMPLEMENTED / TARGET VALIDATION REQUIRED

## Current observation

Navigation geometry/timing is no longer the obvious problem. Latest uploaded
`navigation_perf.log` tail still reports the default wall reference as a single scalar
Ruckig solve with:
```text
min_speed_mps=10.0000
max_speed_mps=10.0000
```

User nevertheless sees:
- actual speed around 10.1 m/s;
- main-engine indicator off;
- a visible hull somersault near the low/straight route segment.

The perf log cannot diagnose body attitude. We need execution telemetry.

## New telemetry

Generated after every Stage-2 run:
```text
tools/navigation_runtime/last_execution_telemetry.log
```

Per sampled frame it records:
- t, phase, effective law;
- position/speed;
- physical forward/up;
- pitch/yaw/roll rates;
- MAIN %, main acceleration;
- RCS/manoeuvre acceleration;
- combined engine acceleration;
- reference speed;
- reference forward/up;
- body-vs-velocity angle;
- actual-vs-reference forward/up angles;
- MAIN_ON/OFF and RCS_ON/OFF transition events.

Trace JSON also persists the new angular/propulsion fields.

## New speed controls

Viewer reducer state owns two sliders:
- START V: 5..50 m/s;
- FINISH V: 5..50 m/s.

They initialize from scenario.json authored values.

Changing them:
- does not rebuild Stage-1 static route;
- invalidates/restores only Stage-2 execution;
- scales authored start velocity direction to the selected start magnitude;
- applies selected finish magnitude to terminal trajectory/gate/final validation.

## Target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

First reproduce at START=10, FINISH=10 in the mode that shows the somersault.

Then send:
```text
tools/navigation_runtime/last_execution_telemetry.log
```

Interpretation:
- reference forward/up rotates with the flip, MAIN=0 -> attitude authoring/program sampling;
- reference stable but physical forward/up rotates -> angular tracker/physics;
- RCS_ON around the event -> distinguish translational manoeuvre thrust from pure
  attitude torque;
- MAIN_ON proves a real main-thrust vectoring manoeuvre.

After baseline reproduction, vary START/FINISH separately (e.g. 5/10, 10/20, 20/10)
to see whether the flip is tied to boundary speed or independent of it.

## Important caution

The speed sliders are boundary-condition test controls. Very high values may expose
missing interior curvature-speed zoning in the current first scalar path-progress
implementation. Do not hide such failures by clamping the UI back to doctrine speed;
record and fix them if they occur.

## Mandatory state protocol

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
