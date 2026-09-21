# CONTINUE PROMPT — Elite Navigation: diagnose remaining hull flip with execution telemetry

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read those five files first, then inspect:
- `tools/navigation_runtime/NavigationScenarioRuntime.h/.cpp`;
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`;
- `tools/navigation_runtime/NavigationTrace.h/.cpp`;
- `src/game/navigation/ManeuverProgramSampler.cpp`;
- `src/game/navigation/ManeuverTrackingController.cpp`;
- `src/game/navigation/DynamicMotionSystem.cpp`;
- `src/game/ship/ShipController.cpp`.

## Current verified high-level state

The scalar path-progress architecture fixed the old dense-waypoint Ruckig fan/near-stop.
Latest uploaded perf tail continues to show the default wall reference at exactly
10.0 m/s min/max.

Minimum-cant Newtonian attitude and free-transit speed/progress deadbands improved the
flight substantially.

Remaining user-visible defect:
- around the low/straight part of the route the hull can perform a somersault;
- viewer main-engine indicator is off;
- actual speed can be about 10.1 m/s;
- exact cause is not yet proven.

Do NOT infer body behavior from `navigation_perf.log`; it is trajectory-generator perf
only.

## New evidence channel

Every Stage-2 run now writes:
```text
tools/navigation_runtime/last_execution_telemetry.log
```

Each line contains:
- time, phase, effective control law;
- physical position/speed;
- body forward/up;
- pitch/yaw/roll rates;
- MAIN %, main acceleration vector;
- RCS/manoeuvre acceleration vector;
- total engine acceleration;
- reference speed;
- reference forward/up;
- body-vs-velocity angle;
- actual-vs-reference forward and up angles;
- MAIN_ON/OFF and RCS_ON/OFF sampled transition events.

Trace JSON persists the same physical angular/propulsion data.

Use this log to decide:
1. reference flips -> attitude authoring or program sampling;
2. reference stable, body flips -> angular tracker / physics;
3. main off but RCS on -> translational RCS is active, not main vectoring;
4. both propulsion channels quiet -> flip is purely angular control.

## New viewer controls

Reducer-owned sliders:
- START V 5..50 m/s;
- FINISH V 5..50 m/s.

Runtime settings:
- `startSpeedOverrideMps`;
- `finishSpeedOverrideMps`.

Negative override means authored scenario value.

Stage-1 geometry is retained. Slider changes invalidate only Stage-2 execution.

Start override scales the authored start velocity direction.
Finish override changes terminal Ruckig speed, moving-terminal gate, and final-state
speed validation.

## Relevant commits

- `1932f80e1350a2631332b1092f7615bddc479dfe`
- `0bebf41764177e6614efc10617ddfa034407f38b`
- `62c94992bfbde0983dbcb2d581c038f5939f890c`
- `33854813be1d8299597a7aeb9e4bf40600403f16`
- `72eeb3fde524136aff5edff1e33a683c9cf1e955`
- `e8083bb2f27623e73753bb98a9b6137bfc5928c2`
- `c199d0dbb042066075f1b320799d8e5dfa55b0a7`

## Next target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Reproduce first at 10/10 and send:
`tools/navigation_runtime/last_execution_telemetry.log`.

Then vary boundary speeds independently.

If compile fails, fix actual compile issue first and repeat mandatory MD/prompt update.

Do not enable dynamic avoidance yet.
