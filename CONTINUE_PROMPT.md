# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md` and relevant production/tests.
After every state-affecting event synchronize those files and recreate this
`CONTINUE_PROMPT.md` from scratch.

## Current architecture

B4 is receding-horizon local avoidance. It proves/executes the next safe short
segment. There is no mandatory fixed-distance return to the nominal line.
If no physically executable safe segment exists, navigation remains active and
commands braking/replanning. Legacy angular fan/branch mechanisms stay forbidden.

## Latest target evidence before viewer work

The 18:24 target run built successfully and passed 17/19 runtime tests.
B4 found and physically executed two safe adjusted segments; the composite later
lost dynamic clearance after returning to a long scripted topology leg.
The planner fixture also has a separate route-context fixture problem.

## 3D viewer now implemented

Location: `tools/navigation_runtime/`.

Core files:
- `NavigationTrace.h/.cpp`;
- `NavigationRuntimeViewer.cpp`;
- standalone `CMakeLists.txt`;
- `run_mingw64.sh`;
- `README.md`.

`NavigationCompositeProvingGroundTests.cpp` now emits
`tools/navigation_runtime/last_trace_<law>.json` through an RAII writer.
The trace survives later test assertion failure.

Viewer shows:
- route polyline;
- route turn/portal markers;
- actual ship trajectory;
- oriented rectangular ship box using Cobra half extents;
- explicit nose arrow;
- moving hazard trajectory;
- hazard radius, hull collision envelope and planner safety envelope;
- selected bypass target, reacquisition reference, portal target;
- all replan events;
- time/phase/status/clearance in title.

Controls: RMB orbit, MMB pan, wheel zoom, F fit, Space play/pause,
`[`/`]` frame step, R next replan, Esc close.

## Validation status

Viewer/trace code is committed but UNVERIFIED on the target MinGW64 machine.
Do not claim acceptance before target compilation/run.

Run:
```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_mingw64.sh || true
bash tools/navigation_runtime/run_mingw64.sh
```

Expected known runtime failures may still exist; the first command is also used
to produce the trace. Inspect compile errors separately from navigation behavior.

After visual inspection, fix monitored topology-resume behavior without lowering
safety radii, padding, acceleration limits or tracking tolerances.
