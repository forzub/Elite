# CONTINUE PROMPT — Elite Navigation live stand

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md`, and all relevant files under
`tools/navigation_runtime/` before changing behavior.

After every state-affecting event synchronize those Markdown files and recreate this
`CONTINUE_PROMPT.md` from scratch.

## Mandatory command rule

Whenever compilation produces an executable, always provide a separate exact launch
command from the documented working directory.

## Canonical tool architecture

`tools/navigation_runtime` is a live scenario-driven navigation diagnostic stand.
`scenario.json` is INPUT. `last_calculated_trace.json` is OUTPUT only.

On `РАССЧИТАТЬ` the executable runs in-process:
`NavigationSpace/NavigationMap -> NavigationRuntimePlanner -> maneuver program ->
TrajectoryFollower -> NavigationRuntimeControlBridge/PilotSkillExecutor ->
SharedShipPhysics/DynamicMotionSystem -> in-memory trace -> 3D playback`.

Real UI inputs:
- Assisted / Newtonian;
- Expert / Average / Loser pilot;
- Standard / Extreme flight style;
- sudden obstacle checkbox;
- Calculate button.

## First live-video diagnosis

The first target video showed the Cobra mostly braking with right-panel status
`ДАННЫЕ УСТАРЕЛИ` around simulation time ~41 s.

Root cause was an integration bug in `NavigationScenarioRuntime.cpp`:
`NavigationRuntimePlanner::plan()` received absolute `vehicle.timeSeconds` as
`dynamicResultAgeSeconds`. The freshly queried NavigationMap snapshot therefore
became stale as soon as simulation time exceeded the planner's 0.25 s age limit.

Fix now on main:
- pass `0.0` as snapshot age for the synchronous query->plan call;
- do not regress this back to absolute simulation time.

## HUD rule from video feedback

The right diagnostic panel must NEVER vertically reflow during playback.
All rows use fixed Y slots:
- mode;
- frame;
- time;
- phase;
- planner status;
- orientation error;
- clearance;
- event;
- calculation result;
- explanation;
- legend;
- controls.

Absent values display `-`; they do not remove rows.
`СОБЫТИЕ: ПЕРЕПЛАНИРОВАНИЕ` may change text/color but must never move any other text.
Calculation success/failure remains visible while a partial trace is playing.

## Interpretation of daytime tests

Do not call them fake, but do not overclaim them.
They exercised real production planner/follower/control/physics components and useful
local/composite slices. However the composite scenario was staged through manually
authored phases and AcceptedManeuverPrograms. It did NOT prove a free-running
arbitrary start->world->finish replanning/execution loop.

The live stand exposed that missing end-to-end orchestration gate.

## Critical remaining architecture gap

The live stand currently converts Planner output through its own handcrafted quintic
`makeShortProgram`. That is NOT yet the full production B5/B6/B7/B8 physical
compile/proof/selection/acceptance path.

Therefore a successful live-stand run is not yet final navigation acceptance evidence.
After the immediate rerun, replace the stand-local maneuver adapter with the actual
production physical maneuver chain before further quality judgments.

## Immediate target rerun

Build:
```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
```

Exact executable launch:
```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Check:
- no automatic StaleHold after 0.25 s;
- right panel does not jump;
- calculation result stays visible;
- default no-surprise run makes meaningful progress;
- collect exact target HEAD and visible/runtime behavior before the next change.
