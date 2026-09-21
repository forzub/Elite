# CONTINUE PROMPT — Elite Navigation: visible Ruckig curve vs actual flight

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

After every state-affecting event:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- **recreate this CONTINUE_PROMPT.md from scratch again**.

Read first:
- CURRENT_STATE.md
- CURRENT_TASK.md
- PROJECT_STATE.md
- src/game/navigation/STAGE12_END_TO_END.md
- src/world/navigation/TrajectoryGenerator.h/.cpp
- tools/navigation_runtime/NavigationTrace.h/.cpp
- tools/navigation_runtime/NavigationScenarioRuntime.cpp
- tools/navigation_runtime/NavigationRuntimeViewer.cpp
- tests/navigation_guidance/RuckigRoutePlannerTests.cpp
- src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md

## Critical correction

Do not call the current production reference a SmoothPathOptimizer spline.
`SmoothPathOptimizer` is retired in production. Aggregate `SmoothPathPerf` log lines
are legacy/test compatibility output. Current runtime motion is Ruckig.

## Current candidate

The four-point Stage-1 route remains immutable:
```text
(0,0,0) -> (110,-27,0) -> (190,-27,0) -> (300,0,0)
```

Stage-2 now creates a separate rounded execution guide:
- local entry/exit points replace an exact coarse corner constraint;
- turn-room estimate uses speed and lateral acceleration;
- if the useful corner cannot fit, local support can be moved outward into free space;
- Ruckig solves that guide.

The full products are now visible:
- WHITE = retained Stage-1 route;
- BLUE = local execution guide;
- PURPLE = complete calculated Ruckig reference;
- GREEN = actual flown path;
- YELLOW arrow = actual V;
- CYAN short arrow = physical hull nose;
- RED arrow = target/program nose.

Runtime also reports:
- EXECUTION GUIDE POINTS
- CALCULATED MIN SPEED
- CALCULATED MIN SPEED POS
- CALCULATED MAX SPEED

Focused default-wall test now rejects a calculated reference that:
- does not create a rounded guide;
- drops below 7.5 m/s in this steady 10 m/s experiment;
- geometrically backtracks along +X.

Latest candidate commits:
0b4212345b2c38b4f775937060a81ba56cace219
939a7058ba58a244282d219e0c298150a72a410f
3ee684742afd8ad91307b99c0a1815bd30aea722
287ab085275873ffb1f991e09902d31706edb927
33e3b228327cfb9c6031ecb09aa67847d16d2cda
154d9e1a544dc957769efce127b70ee36dd8b0cc
68f4377aabae1fd894be4a882fb30b0cec762d89
77f9640eb77267be6a258b0293720e00f089fbe6
f9b106f8c259c0f7a39ebde11f67c776e1a35c92

Do not call target PASS until the user's MinGW64 output proves it.

## Exact next commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_guidance -B build/tests/navigation_guidance -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_guidance --target ruckig_route_planner_tests
ctest --test-dir build/tests/navigation_guidance -R ruckig_route_planner -V

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

## Interpret next viewer

If PURPLE itself has a loop or large speed dip:
- continue changing execution-guide/Ruckig authoring;
- widen support farther rather than tightening the curve or stopping.

If PURPLE is smooth at about 10 m/s but GREEN brakes or loops:
- geometry is no longer the main fault;
- fix Follower / Pilot / Newtonian body-thrust execution;
- do not hide it with more route changes.

Do not start dynamic avoidance yet.
