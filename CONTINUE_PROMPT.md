# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md`, and the code under
`tools/navigation_runtime/` before changing behavior.

After every state-affecting event synchronize those Markdown files and recreate this
`CONTINUE_PROMPT.md` from scratch. Never append stale continuation instructions.

## Mandatory command-output rule

Whenever a build produces an executable, always give the user a separate exact
launch command from the documented working directory.

## Canonical tool architecture

`tools/navigation_runtime` is now a LIVE navigation diagnostic stand, not a passive
trace viewer.

Input is `tools/navigation_runtime/scenario.json`.
The JSON describes the initial scene only:
- start position/velocity/forward/up;
- optional forced ship route points (default empty);
- final position;
- optional final forward and up constraints;
- final speed requirement;
- standard/extreme cruise speeds;
- static obstacles: sphere / box / capsule;
- moving obstacles using either velocity vector OR route_points + route_speed_mps;
- optional sudden obstacle, including `spawn_relative_to_ship_fru`.

On the real `РАССЧИТАТЬ` button the executable runs in-process:

`scenario.json -> NavigationSpace / NavigationMap -> NavigationRuntimePlanner ->
AcceptedManeuverProgram -> TrajectoryFollower -> NavigationRuntimeControlBridge /
PilotSkillExecutor -> SharedShipPhysics / DynamicMotionSystem -> in-memory trace -> 3D`.

`last_calculated_trace.json` is only an OUTPUT for diagnostics. It is not the source
of the live calculation.

## Real top controls

Control law:
- Assisted;
- Newtonian.

Pilot:
- Expert;
- Average;
- Loser.

Flight style:
- Standard;
- Extreme.

Sudden-obstacle checkbox:
- unchecked: sudden obstacle is never published;
- checked: sudden obstacle is absent from the initial map and becomes authoritative
  only when `activation_time_s` is reached DURING simulation;
- therefore initial routing has no foreknowledge of the surprise obstacle;
- normal receding-horizon replanning reacts after activation.

These controls are real simulation inputs, not cosmetic replay toggles.

Pilot selection changes actual PilotSkillExecutor execution profile:
reaction delay, perception/decision rate, command latency, response, slew and
deterministic command error.

Flight style changes real cruise speed and planner horizon/cost/aggressiveness.
Hard collision geometry and vehicle authority are not disabled for Extreme.

Control law changes real LocalFlightControlLaw and maneuver attitude semantics.

## Route semantics

The default `scenario.json` has empty `ship_route_points`, so Planner must calculate
the route around the static JSON obstacle.

The rendered white route is generated from the live sequence of Planner
`selectedTarget` decisions after each replan. Adjusted targets are recorded as
turn/bypass markers. Green path is actual simulated ship motion.

## Existing diagnostics retained

- ordinary decorated Windows window starts maximized; no exclusive fullscreen;
- Russian HUD;
- playback and frame slider;
- static JSON obstacles rendered in 3D;
- actual ship forward/right/up preserved;
- actual nose and AcceptedManeuverProgram reference nose rendered separately;
- angular orientation error displayed;
- translucent maneuver-program tracking corridor;
- Cobra horizon inset: plane perpendicular to actual ship longitudinal axis;
- moving hazard and predicted envelope tunnel projected into Cobra right/up plane;
- calculated trace saved to `last_calculated_trace.json` for inspection.

## Current validation state

The live scenario runtime implementation is committed but NOT YET compiled/run on the
target MinGW64 machine. Do not claim it accepted.

Immediate target gate:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
```

Exact executable launch command:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Check first:
- compile/link;
- window opens maximized;
- top three selector groups + sudden obstacle + `РАССЧИТАТЬ` are visible/clickable;
- no preloaded old trajectory;
- pressing `РАССЧИТАТЬ` performs a new live calculation;
- default planner routes around the wall without forced ship waypoints;
- unchecked/checked sudden obstacle behavior differs correctly;
- selected pilot/mode/style materially changes the simulation;
- final forward/up/speed contract is respected;
- route/path/corridor/horizon render.

Any target compile/runtime failure becomes the next state-affecting event. Record exact
HEAD, observed error, root cause and patch before proceeding.
