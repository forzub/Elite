# CONTINUE PROMPT — Elite Navigation adaptive corner passage validation

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
and **recreate this CONTINUE_PROMPT.md from scratch again**.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `src/world/navigation/TrajectoryGenerator.cpp`
- `tests/navigation_guidance/RuckigRoutePlannerTests.cpp`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `src/game/navigation/OrdinaryPhysicalManeuverCompiler.h/.cpp`
- `src/game/navigation/DynamicMotionSystem.cpp`

## User's latest result

Nominal route shape is now approximately logical, but the ship still stops at both
shallow intermediate points.

The supplied `navigation_perf(6).log` shows the latest four-point / one-obstacle
viewer solves with `blended_waypoints=0`. This confirms the previous Stage-2 reference
still converted both interior points to stops.

User also requested:
- speed numeric readout fixed in the lower-right corner;
- explicit lower-right legend for every ship/vector arrow;
- much more visible actual velocity vector;
- clearer distinction between actual hull nose and the red program/reference arrow;
- treat white polyline as rough route intent and actual execution as a smooth/rounded
  trajectory; slightly lengthening the route is acceptable if needed to create turn
  room.

## Current unverified candidate

### Ruckig corner handling

`d2205f50259fdef05a6515fec3822055891c47d5`
- adaptive corner blend-distance search replaces the single fixed 25% chord test;
- largest safe local blend wins;
- failed Ruckig through-speed is reduced progressively before zero.

`641e6ef6aeb79d4dddcf58ce7601448723f79b9a`
- removed arbitrary 65% max-speed corner cap.

`20aef47942c675ae59b04c288b2ae4b3cb6de5e6`
- tests now require a tighter safe blend to remain moving;
- preserve stop fallback only when even the minimum local blend is blocked;
- default viewer wall shallow corners must both have nonzero through-speed.

### Viewer

`cbdbcc2b2132f0ef29af9a73e3589d25c415a8f5`
`2ad3bf086c153895adefee64fb9f67bcabaa84da`
`f695a55454f5ccc5802c618347dfb400ec4d6bcb`

Legend:
- yellow thick arrow = actual velocity;
- short cyan arrow = actual hull nose;
- red arrow = target/program nose;
- white line = retained geometric route;
- green line = actual flight path;
- current speed is numeric in the fixed lower-right block.

Velocity display scale = 3.5 m per 1 m/s, line width 6 px.

## Next action

Do NOT start dynamic avoidance.

Run the focused Ruckig gate first. If it passes, run the two-stage viewer and inspect
whether the two intermediate points now retain speed.

If either default wall corner still stops:
1. inspect `RETAINED WAYPOINT SPEEDS`;
2. identify whether adaptive blend initialization or later swept Ruckig validation
   reduced it to zero;
3. add a real maneuver-space/corner-radius reserve by moving the execution guide
   farther from the obstacle;
4. prefer a slightly longer path over StopTurnGo.

Do not revive the retired custom SmoothPathOptimizer merely to draw a spline. The
architectural target is:
```text
retained geometric polyline / corridor intent
 -> control-law-aware continuous execution trajectory
 -> capability + swept-geometry proof
 -> AcceptedManeuverProgram
 -> Follower
```

After the stop is gone, continue the deeper Newtonian correction: translation must not
assume future hull attitude. Use yellow actual V vs cyan actual nose vs red target nose
to diagnose that separately.

## Exact target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_guidance -B build/tests/navigation_guidance -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_guidance --target ruckig_route_planner_tests
ctest --test-dir build/tests/navigation_guidance -R ruckig_route_planner -V
```

Then:

```bash
cd /d/__elite/work
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Launch separately:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Return:
- HEAD;
- focused Ruckig output;
- Stage-1/Stage-2 diagnostics;
- `RETAINED WAYPOINT SPEEDS`;
- `MAX BODY/VELOCITY ANGLE`;
- short observations/video for Expert Standard Newtonian and Assisted.

Then update all mandatory MD files and recreate this prompt again.
