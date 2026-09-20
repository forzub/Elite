# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md` and the relevant viewer/test code.
After every state-affecting event synchronize those Markdown files and recreate this
`CONTINUE_PROMPT.md` from scratch. Never merely append stale continuation instructions.

## Mandatory developer workflow

Whenever compilation produces an executable, always give the user a separate exact
launch command from the documented working directory.

## Current viewer architecture

`tools/navigation_runtime/` is a deterministic replay viewer.
The real composite test executes Planner -> AcceptedManeuverProgram -> Follower ->
runtime control bridge -> SharedShipPhysics first, then writes JSON.
The viewer does NOT run planner/follower live while replaying JSON.

Latest target-verified viewer before the current source changes compiled and opened.
The current source changes below are NOT target-verified yet.

## Trace/viewer accuracy changes now on main

- trace schema v2 records actual ship forward/right/up; roll is no longer reconstructed
  from forward only;
- trace v2 records the sampled AcceptedManeuverProgram reference position and full basis;
- trace v2 records hazard velocity and planner look-ahead;
- playback interpolates between ~0.10 s trace samples, removing the apparent ~10 Hz
  stutter that looked like CPU overload;
- ordinary decorated GLFW window starts maximized to Windows work area; it is not
  exclusive/borderless fullscreen;
- viewer HUD/title are Russian with internal Cyrillic bitmap glyphs;
- frame slider is visible and scrubbable;
- actual nose and program-reference nose are rendered separately;
- HUD reports angular error between actual and accepted-program forward;
- translucent blue tube renders AcceptedManeuverProgram reference path plus
  `tracking.positionErrorMeters`. Do NOT call this a Planner volumetric corridor:
  `NavigationRuntimePlanner::Result` does not publish one;
- bottom-left `ГОРИЗОНТ КОБРЫ` inset is the right/up plane perpendicular to actual ship
  forward and projects the moving hazard plus its predicted envelope tunnel over the
  planner look-ahead.

## Newtonian correction

The composite test had been authoring post-hazard replacement/continuation programs as
`OrientationMode::VelocityAligned` even when law was Newtonian. This made Newtonian
look Assisted despite using the Newtonian control law.

Now:
- Newtonian local bypass/reacquisition replacement programs preserve current rigid-body
  attitude using `FixedStart`;
- the un-oriented portal-102 transit also preserves body attitude for Newtonian;
- Assisted retains velocity-aligned attitude;
- explicit precision/oriented attitude requirements may still rotate the body when the
  maneuver contract actually demands it.

## What remains pending

Do NOT add cosmetic replay controls that pretend to change physics.
The requested selectors still need a reusable live scenario runner:
- Assisted / Newtonian;
- pilot: expert / average / loser;
- flight doctrine: standard / extreme;
- obstacle checkbox: unchecked must run a genuinely obstacle-free simulation, checked
  must run the moving-hazard scenario.

The live scenario should also support the existing Cobra-horizon projection and moving
obstacle prediction tunnel from authoritative planner inputs.

## Immediate target validation

From repository root:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_runtime --target navigation_composite_proving_ground_tests
./build/tests/navigation_runtime/navigation_composite_proving_ground_tests.exe || true

cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/last_trace_newtonian.json
```

Check: Russian UI, maximized ordinary window, smooth interpolation, actual/program
attitude distinction, translucent program tracking corridor, Cobra-horizon inset, and
Newtonian body no longer being continuously velocity-aligned.

Known navigation issues remain separate:
- planner fixture currently reaches `fixture must produce a safe adjusted target`;
- composite previously performed two safe B4 adjusted segments and then lost dynamic
  clearance after returning to the long portal-102 leg. Do not weaken safety limits to
  green this test.
