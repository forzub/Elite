# CONTINUE PROMPT — Elite Navigation: inspect current Ruckig reference before redesign

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST update:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- recreate this `CONTINUE_PROMPT.md` from scratch.

Also synchronize `CONTROL_LAW_MANEUVER_MODEL.md` when ownership changes.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `CONTINUE_PROMPT.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tools/navigation_runtime/NavigationTrace.h/.cpp`
- `src/world/navigation/TrajectoryGenerator.cpp`
- `src/game/navigation/RuckigTrajectorySolver.cpp`
- `src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp`.

## Current code baseline before docs

```text
620b59ebbb6937727c5c3e3a47f78884ae3fd99f
```

Target PASS is not yet established.

## New viewer diagnostics

Execution now shows:
- PINK CROSS = instantaneous `programReferencePosition`, the current
  Ruckig-derived accepted reference sampled by B9/B10;
- VIOLET CROSS = endpoint of the active AcceptedManeuverProgram phase.

Do not falsely call the violet point "Ruckig current target". In multi-point
mode Ruckig performs one scalar path-progress solve to the end of the complete
execution guide; phases are cut afterward.

The global scalar Ruckig target is the end of the whole guide / finish.

## Authority decision

The physical maneuver planner/compiler is the authority for motion:
- geometry;
- tangent/velocity;
- acceleration requirement;
- hull attitude;
- angular reachability;
- aft-main + RCS allocation;
- lead-rotation/braking boundary.

Ruckig is a subordinate timing/state-transition solver called inside this
process.

Follower/autopilot executes the accepted program against real physics. It may
request recompile/replan after a tracking violation but must not invent a
different route to repair bad authoring.

## Important correction

Do NOT assume turns are generated only by manoeuvre thrusters.

For the current Cobra Newtonian mode, a material turn can and often should use
combined:
- hull rotation;
- aft-main thrust projected into the needed world acceleration;
- bounded RCS trim.

The current execution-guide radius formula based primarily on
`manoeuvreThrusterAccel` is therefore insufficient and will be redesigned
after the diagnostic run.

## Current terminal semantics

`tools/navigation_runtime/scenario.json` contains both finish `forward` and
`up`; parser requires both.

Acceptance:
- position <= 5 m;
- speed error <= 1.5 m/s;
- forward/up error <= 0.25 rad.

A non-zero finish speed also sets terminal velocity along `finish.forward`.

Thus the current case is an oriented moving fly-through.

No braking is required by default. At equal start/finish speed, use a broad
free-space arc if physically feasible. Brake only when required by terminal
speed, narrow geometry, curvature/acceleration reachability or lead-rotation
constraints.

## Next target run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Reproduce the higher-speed Newtonian case.

At the first visible miss, inspect:
- pink current reference cross;
- violet phase endpoint;
- actual velocity vector;
- hull nose;
- rear-main / fore-main / manoeuvre indicators.

Determine first whether the accepted reference itself is physically bad or the
follower fails to realize a physically reasonable reference.

Do not add another follower workaround before answering that.
