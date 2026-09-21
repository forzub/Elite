# CURRENT TASK — validate sampled C1 corner guide removes purple Ruckig barrels

**Date:** 2026-09-21  
**Status:** PURPLE REFERENCE DEFECT ISOLATED / SAMPLED-CURVE CANDIDATE UNVERIFIED

## Confirmed defect

Viewer evidence shows two visible lateral barrels directly in the PURPLE calculated
Ruckig reference, at the two rounded-corner regions.

Therefore:
- this defect is upstream of Follower/Pilot;
- do not change body/thrust execution yet to solve these two barrels.

Latest perf evidence before the fix:
```text
coarse_points=4
guide_points=6
rounded_corners=2
expanded_corners=0
blended_waypoints=4
samples=2300
valid=1
```

The six-point guide was too sparse. A long Ruckig leg with non-collinear endpoint
velocities can bow laterally between the endpoints.

## Candidate fix

Each corner now becomes a densely sampled quadratic C1 curve:
- entry tangent = incoming leg;
- exit tangent = outgoing leg;
- quadratic control = local widened corner;
- ~2.5 m sample spacing;
- 4..24 segments per corner;
- the sampled curve is collision checked.

Also removed the artificial 0.15 floor from the small-angle bend factor. With a dense
smooth curve, tiny local heading changes must not be interpreted as tight turns.

Commits:
- 3dccb8a36c8e8023083b21f317b944a8097fd6a0
- 517904389019fbf94ddbbabbe3248cdb7b6fab20
- a056973c674194b109b8f37646f6d7a9e461c5b9

## Focused acceptance

Default steady 10 m/s wall regression now requires:
- execution guide is rounded;
- calculated min speed >= 7.5 m/s;
- no +X backtracking;
- max PURPLE-reference distance from BLUE guide <= 1.5 m.

## Immediate target commands

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

## What to inspect in viewer

WHITE = coarse route  
BLUE = dense execution guide  
PURPLE = calculated Ruckig reference  
GREEN = actual flight

First question only:
- did the two visible PURPLE barrels disappear?

If PURPLE is now clean but GREEN still brakes/deviates, then move downstream to
Follower/Pilot/body-thrust semantics.

If PURPLE still barrels, keep working in Ruckig guide discretization / continuous
reference generation. Do not hide it downstream.

## Mandatory state protocol

After every state-affecting event:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
