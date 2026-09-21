# CURRENT TASK — validate continuous shallow-corner passage, then fix Newtonian maneuver semantics

**Date:** 2026-09-21  
**Status:** ADAPTIVE THROUGH-CORNER CANDIDATE COMMITTED / TARGET VALIDATION REQUIRED

## Current evidence

The nearest-face nominal route is now visually/logically acceptable, but the user's
latest run still stops at both shallow intermediate points.

The supplied performance log confirms the previous candidate still ended with
`blended_waypoints=0` for the four-point / one-obstacle viewer route. Therefore the
stops are authored upstream by Ruckig waypoint handling, not caused by Follower losing
an otherwise continuous reference.

## Candidate now committed

### Corner execution

`d2205f50259fdef05a6515fec3822055891c47d5`
- replace the single fixed 25%-leg corner-cut safety test with adaptive blend-distance
  search;
- keep the widest collision-clear local blend;
- if an actual Ruckig leg fails, reduce through-speed progressively before zeroing it.

`641e6ef6aeb79d4dddcf58ce7601448723f79b9a`
- remove the arbitrary 65% max-speed corner cap;
- physical curvature/lateral authority and real speed constraints now limit speed.

`20aef47942c675ae59b04c288b2ae4b3cb6de5e6`
- test wide-blend-blocked/tight-blend-safe -> keep moving;
- test truly blocked minimum blend -> stop allowed;
- test default wall shallow corners -> both must retain nonzero speed.

The white retained route remains a coarse geometric/topological polyline. It is not a
literal instruction to stop and pivot at each vertex. The intended Stage-2 output is a
continuous Ruckig execution trajectory through that intent.

If this is still too tight, add an execution/maneuver reserve that moves the corner
support farther from the obstacle. A slightly longer route is preferable to a fake stop.

### Viewer

Latest viewer commits:
- `cbdbcc2b2132f0ef29af9a73e3589d25c415a8f5`
- `2ad3bf086c153895adefee64fb9f67bcabaa84da`
- `f695a55454f5ccc5802c618347dfb400ec4d6bcb`

Lower-right fixed block now shows:
- numeric current speed;
- yellow thick arrow = actual velocity vector;
- short cyan arrow = actual hull nose;
- red arrow = program/Follower target nose;
- white = retained geometric route;
- green = actual flown path.

Velocity arrow scale is 3.5 m of display length per 1 m/s and width 6 px.

## Important Newtonian issue still open

Do not confuse fixing StopTurnGo with completing Newtonian maneuver authoring.

Current translation is still generated before final body/thrust attitude semantics.
After the shallow-corner stop is removed, inspect:
- actual yellow velocity vector;
- actual cyan hull nose;
- red program target nose.

For Expert/Newtonian, a material course change must ultimately come from a
body/thrust-aware proved maneuver:
- preserve useful inertial velocity;
- lead body rotation as needed;
- use bounded RCS for trim;
- use main-engine thrust only when actual hull attitude makes it available;
- never let an arbitrary translational reference assume the future body attitude.

That remains a B5/B6 integration task after this gate.

## Target-machine gate

First run the focused Ruckig corner tests:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_guidance -B build/tests/navigation_guidance -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_guidance --target ruckig_route_planner_tests
ctest --test-dir build/tests/navigation_guidance -R ruckig_route_planner -V
```

Then run the retained-route/viewer gate:

```bash
cd /d/__elite/work
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Launch separately:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Collect:
- HEAD;
- focused Ruckig result;
- route points/length;
- `RETAINED WAYPOINT SPEEDS`;
- `MAX BODY/VELOCITY ANGLE`;
- video or observation of yellow V vs cyan actual nose vs red target nose for
  Expert/Standard/Newtonian and Assisted.

## Mandatory state protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
