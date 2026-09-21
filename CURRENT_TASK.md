# CURRENT TASK — target-validate scalar Ruckig path progress and visible viewer revision

**Date:** 2026-09-21  
**Status:** DENSE 3-D RUCKIG WAYPOINT CHAIN RETIRED / SCALAR PATH-PROGRESS CANDIDATE UNVERIFIED

## Latest verified target evidence

Focused test binary compiled and linked.

Result:
```text
7/8 PASS
FAIL default wall shallow corners stay moving
min_speed=0.009 m/s
```

Viewer also shows a purple fan around the rounded node.

The latest performance log shows the cause:
```text
guide_points=18
legs=74
ruckig_ok=17
min_speed_mps=0.0094
```

Earlier dense candidates reached 36/52 guide points and hundreds/thousands of Ruckig
leg attempts. This is not acceptable.

## Internet / upstream verification

Official Ruckig guidance:
- basic/community use is state-to-state online trajectory generation;
- local intermediate-waypoint solving is a Pro feature;
- waypoint calculation is substantially harder;
- use as few waypoints as possible;
- filter close waypoint lists and prefer waypoints far apart.

Therefore our implementation was wrong: we were turning dense geometric samples into
many independent 3-D Ruckig target states.

## New architecture candidate

```text
coarse route
 -> rounded geometric guide p(s)
 -> one-dimensional jerk-limited Ruckig progress s(t)
 -> map progress back to path p(s(t))
 -> swept geometry validation
```

Ruckig remains:
- true 2-point path: existing 3-D state-to-state solve;
- curved / multi-point route: scalar progress only.

New API:
- `RuckigProgressRequest`;
- `RuckigProgressSample`;
- `RuckigProgressResult`;
- `RuckigTrajectorySolver::solveProgress()`.

Key commits:
- 2e815ef993a31acd40596f170c67a124f9337bf9
- a50de59958887efbe50e2cdacee8c67623b2ebb8
- 9f166f4286dd03197705b0cd9e4ba63e33d000bc
- 3c58f4c3719d712e524e86e5f030820486af3102
- c6f7b160878a3582362574691a12c96fd3ff3623
- b76c4a52d8c32421d6fffe1098de9ad1c6a95dfb
- b9e7ce64182ce761edb45b4751d680950ab33ff5
- 3843194d999e302ca32171de3d49f3f21987de64

## Viewer revision requirement

Viewer must visibly show:
```text
NAV REV <12-char git sha>
```
inside the OpenGL HUD, independent of the native title/right panel.

Per-sample execution-guide crosses are removed; BLUE remains the geometric guide line.

Commit:
- 1199a0c04ee7cf3a6938bef0ca9c3bb55d7bd4c8

## Immediate target gate

First build focused tests only:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_guidance -B build/tests/navigation_guidance -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_guidance --target ruckig_route_planner_tests
ctest --test-dir build/tests/navigation_guidance -R ruckig_route_planner -V
```

If compile fails, fix compile first.
If the default wall functional test still fails, use the exact reported min speed and
latest `RuckigRoutePerf` line. Do not restore dense 3-D waypoint chaining.

Only after focused gate passes:

```bash
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Viewer acceptance:
- NAV REV visible;
- BLUE guide has reasonable geometry without sample-cross clutter;
- PURPLE calculated path has no fan/barrels;
- steady wall route does not nearly stop;
- GREEN vs PURPLE then determines whether the next defect is downstream Follower/Pilot.

## Important limitation of this first scalar candidate

For curved routes it currently uses a conservative path-wide speed cap derived from:
- vehicle max speed;
- speed-limit ranges;
- positive point limits;
- worst local curvature.

This is intentionally conservative and correct. Later optimization can split the path
into a small number of meaningful speed zones. Never use one 3-D Ruckig target per
geometric sample again.

## Mandatory state protocol

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
