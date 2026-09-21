# CURRENT TASK — re-run focused Ruckig gate after curvature-speed fix

**Date:** 2026-09-21  
**Status:** TARGET COMPILE PASS / FOCUSED FUNCTIONAL GATE 7/8 / FIX CANDIDATE UNVERIFIED

## Latest target result

The focused executable compiled and linked successfully.

Result:
```text
7/8 PASS
FAIL: default wall shallow corners stay moving
reason: calculated curve contains a major unnecessary braking dip
```

This is not a compiler failure. It is the exact functional regression we need to fix.

## Root cause

The new dense C1 guide exposed an old invalid speed heuristic.

`buildWaypointVelocities()` used a `blendDistance` derived from local segment length.
On a densely sampled curve, segment length gets smaller even though physical curvature
does not change. Therefore the same curve could receive a lower allowed speed merely by
adding samples.

## Candidate fix

Corner speed now uses true local geometric curvature:
```text
kappa = 2*|AB x BC| / (|AB| |BC| |AC|)
v_max = sqrt(a_lateral / kappa)
```

This is sampling-density invariant.

Ruckig perf lines now also contain:
- `min_speed_mps`;
- `max_speed_mps`.

Commit:
- `4dcebe77580e8abc7a0f9f4a22cec65f3ba985d5`.

## Steady-test correction

The old start/finish magnitudes were 10 m/s, but their directions were +X rather than
the first/last route tangents. That still introduced endpoint steering transients.

Now:
- start velocity + physical forward = first route tangent at 10 m/s;
- terminal velocity + forward = last route tangent at 10 m/s.

Commits:
- `5c408b76b36a86bd6b4fecacc377142d80726796`;
- `3a22a4ae0aca037df4c9b8c76759506ed8a840c6`.

## Viewer revision stamp

Viewer now shows exact Git revision in:
- window title;
- HUD heading.

CMake obtains it from:
`git rev-parse --short=12 HEAD`.

Commits:
- `2c1baf4854f083c2e3e44ed16136c70237dfafcd`;
- `1e84a9cac2f26da8e2802e1b97e8582f869f218d`.

## Immediate target gate

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_guidance -B build/tests/navigation_guidance -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_guidance --target ruckig_route_planner_tests
ctest --test-dir build/tests/navigation_guidance -R ruckig_route_planner -V
```

Expected next evidence:
- either 8/8;
- or the failure text now reports exact `min_speed=<value> m/s`.

Only after focused gate passes:
```bash
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Verify viewer HUD/title revision equals current HEAD short SHA.

## Visual decision

WHITE = retained route  
BLUE = sampled execution guide  
PURPLE = calculated Ruckig reference  
GREEN = actual flight

First check PURPLE:
- no two lateral barrels;
- no obvious braking dips.

If PURPLE is clean and GREEN is not, move downstream to Follower/Pilot/body-thrust.
If PURPLE remains bad, stay in route-to-Ruckig authoring.

Do not enable dynamic avoidance yet.

## Mandatory state protocol

Every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
