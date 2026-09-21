# CONTINUE PROMPT — Elite Navigation: scalar Ruckig path progress

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read those five files first. Then inspect:
- `src/game/navigation/RuckigTrajectorySolver.h/.cpp`;
- `src/game/navigation/RuckigRoutePlanner.h`;
- `src/world/navigation/TrajectoryGenerator.h/.cpp`;
- `tests/navigation_guidance/RuckigRoutePlannerTests.cpp`;
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`;
- `tools/navigation_runtime/CMakeLists.txt`;
- `tools/navigation_runtime/scenario.json`.

## Latest target evidence

Target MinGW64 focused binary compiled and linked.

7/8 tests passed.

Failure:
```text
default wall calculated curve contains a major unnecessary braking dip:
min_speed=0.009 m/s
```

Viewer showed a purple fan near the rounded node.

Latest perf evidence from that generation:
```text
guide_points=18
legs=74
ruckig_ok=17
min_speed_mps=0.0094
```

Earlier dense versions reached 36/52 guide points and hundreds/thousands of Ruckig leg
attempts.

## Root cause, externally verified

Official Ruckig documentation says:
- core/community Ruckig is state-to-state online trajectory generation;
- full local intermediate waypoint calculation is a Pro feature;
- waypoint planning is significantly harder;
- use as few waypoints as possible;
- filter waypoint lists and prefer waypoints far apart.

So do NOT try to fix the fan by adding more Ruckig target points. That was the misuse.

## Current candidate architecture

```text
Stage-1 coarse route
 -> local rounded geometry p(s)
 -> scalar jerk-limited Ruckig progress s(t)
 -> trajectory p(s(t))
 -> collision validation
```

Ruckig roles:
- true single leg: existing 3-D state-to-state solve;
- curved/multi-point path: new `solveProgress()` 1-D Ruckig solve.

Dense geometry samples are never independent Ruckig target states.

Key code commits:
- `2e815ef993a31acd40596f170c67a124f9337bf9`
- `a50de59958887efbe50e2cdacee8c67623b2ebb8`
- `9f166f4286dd03197705b0cd9e4ba63e33d000bc`
- `3c58f4c3719d712e524e86e5f030820486af3102`
- `c6f7b160878a3582362574691a12c96fd3ff3623`
- `b76c4a52d8c32421d6fffe1098de9ad1c6a95dfb`
- `b9e7ce64182ce761edb45b4751d680950ab33ff5`
- `3843194d999e302ca32171de3d49f3f21987de64`

## Viewer revision control

Viewer must show an in-window badge:
`NAV REV <sha>`.

It already also has revision in title/right HUD, but the new badge is specifically to
survive cropped screenshots.

Commit:
- `1199a0c04ee7cf3a6938bef0ca9c3bb55d7bd4c8`.

Guide sample crosses are hidden; BLUE line remains.

## Exact next commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_guidance -B build/tests/navigation_guidance -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_guidance --target ruckig_route_planner_tests
ctest --test-dir build/tests/navigation_guidance -R ruckig_route_planner -V
```

Do not open viewer until focused gate builds and runs.

If focused gate is 8/8:
```bash
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Check:
- visible NAV REV matches build revision;
- no purple fan/barrels;
- no near-zero speed in steady 10 m/s wall test.

If PURPLE becomes correct and GREEN remains wrong, next task is Follower/Pilot/body-thrust.
If PURPLE remains wrong, stay in path geometry / scalar timing.

## Current limitation

Curved paths currently use one conservative path-wide speed cap based on worst curvature
and authored limits. That is acceptable for the current correctness gate. Later replace
it with a small number of meaningful speed zones if needed. Never return to one Ruckig
3-D solve per dense geometric sample.

Do not enable dynamic avoidance yet.
