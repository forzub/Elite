# CONTINUE PROMPT — Elite Navigation: remove purple Ruckig barrels

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

After every state-affecting event:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
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

## Current confirmed evidence

The viewer now proves the two "barrels" are in the PURPLE calculated Ruckig reference
itself, not merely in the GREEN actual path.

Pre-fix perf:
coarse_points=4, guide_points=6, rounded_corners=2, expanded_corners=0,
blended_waypoints=4, samples=2300, valid=1.

Diagnosis:
- one coarse corner had become only one entry point + one exit point;
- Ruckig solved a relatively long leg between them;
- endpoint velocity directions were not collinear with that chord;
- therefore a lateral polynomial bow was mathematically legal and visibly appeared.

## Candidate mechanism fix

Each rounded corner is now a densely sampled quadratic C1 curve:
- control point = widened local corner;
- tangent at entry matches incoming route;
- tangent at exit matches outgoing route;
- sample spacing about 2.5 m, 4..24 segments;
- sampled curve collision checked.

The old artificial bend-factor floor 0.15 was removed. Tiny direction changes on a dense
curve now use their real `sin(angle/2)` with only a 1e-4 numerical floor.

Latest candidate commits:
- 3dccb8a36c8e8023083b21f317b944a8097fd6a0
- 517904389019fbf94ddbbabbe3248cdb7b6fab20
- a056973c674194b109b8f37646f6d7a9e461c5b9

Do not call target PASS until user MinGW64 evidence proves it.

## New regression

In the default steady 10 m/s wall case:
- max calculated Ruckig-reference deviation from execution guide <= 1.5 m;
- min calculated speed >= 7.5 m/s;
- no +X backtracking;
- collision-free.

## Next commands

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

Inspect WHITE / BLUE / PURPLE / GREEN.

Decision:
- PURPLE clean, GREEN bad -> next layer is Follower/Pilot/body-thrust execution.
- PURPLE still bad -> stay in continuous reference generation and densification.

Do not enable dynamic avoidance yet.
