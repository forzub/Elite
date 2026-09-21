# CURRENT TASK — inspect the visible calculated Ruckig curve against actual flight

**Date:** 2026-09-21  
**Status:** ROUNDED EXECUTION-GUIDE CANDIDATE / VIEWER REFERENCE OVERLAY ADDED / TARGET UNVERIFIED

## Why this iteration exists

The viewer showed braking and a distorted green flown path on visually simple sections.
We need to stop guessing whether the cause is route-to-trajectory authoring or the
Follower/Pilot execution layer.

Production runtime does not use the retired custom SmoothPathOptimizer. The actual
continuous reference is Ruckig.

## Candidate changes

Stage-2 now derives a separate execution guide from the retained Stage-1 route:
- coarse route is never mutated;
- an interior coarse corner is replaced by entry/exit points;
- desired turn room is derived from speed and lateral acceleration;
- if the desired corner does not fit, local support may be pushed farther outward into
  free space instead of making a tight S-turn / StopTurnGo;
- Ruckig solves the widened guide.

Viewer now shows:
- WHITE = retained coarse route;
- BLUE = execution guide;
- PURPLE = complete calculated Ruckig curve;
- GREEN = actual flown path;
- YELLOW = actual velocity;
- CYAN = real hull nose;
- RED = target nose.

Runtime diagnostics now include:
- EXECUTION GUIDE POINTS;
- CALCULATED MIN SPEED;
- CALCULATED MIN SPEED POS;
- CALCULATED MAX SPEED.

Focused default-wall regression now rejects:
- missing local rounding;
- calculated speed below 7.5 m/s in this steady 10 m/s stand;
- calculated geometric backtracking along +X.

Latest candidate commits:
- 0b4212345b2c38b4f775937060a81ba56cace219
- 939a7058ba58a244282d219e0c298150a72a410f
- 3ee684742afd8ad91307b99c0a1815bd30aea722
- 287ab085275873ffb1f991e09902d31706edb927
- 33e3b228327cfb9c6031ecb09aa67847d16d2cda
- 154d9e1a544dc957769efce127b70ee36dd8b0cc
- 68f4377aabae1fd894be4a882fb30b0cec762d89
- 77f9640eb77267be6a258b0293720e00f089fbe6
- f9b106f8c259c0f7a39ebde11f67c776e1a35c92

## Immediate target gate

Run:
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
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

## Decision after viewer evidence

- PURPLE bad (loop/braking): remain in route-to-Ruckig authoring. Reject/widen the
  execution guide before ACCEPT.
- PURPLE good but GREEN bad: stop changing geometry and fix the Newtonian/Assisted
  Follower + physical body/thrust execution mismatch.

Do not enable dynamic avoidance yet.

## Mandatory project-state protocol

After every state-affecting event update:
- CURRENT_STATE.md
- CURRENT_TASK.md
- PROJECT_STATE.md
- src/game/navigation/STAGE12_END_TO_END.md
- recreate CONTINUE_PROMPT.md from scratch.
