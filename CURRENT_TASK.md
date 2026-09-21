# CURRENT TASK — validate steady 10 m/s corner fly-through, then fix Newtonian body/thrust semantics

**Date:** 2026-09-21  
**Status:** FOCUSED CORNER FIX SHOWS DEFAULT-WALL PASS / MOVING TERMINAL CANDIDATE UNVERIFIED

## Latest target evidence

Focused Ruckig target gate returned 7/8.

PASS:
- straight route uses Ruckig;
- diagonal stopped leg stays on coarse chord;
- clear corner keeps through velocity;
- blocked wide blend shrinks before stop;
- **default wall shallow corners stay moving**;
- initial acceleration is preserved;
- impossible braking is rejected.

The only failure was the synthetic test requiring a tight-corner fixture to force a
full stop. That is not a valid invariant: if a continuous passage is collision-free,
the solver should be allowed to keep moving. The test now checks swept safety instead
of prescribing zero speed.

## User-requested Test 1 change

Make the default Standard visual stand a steady transit:
- start velocity = 10 m/s;
- Standard max speed = 10 m/s;
- finish speed = 10 m/s.

This removes startup and terminal braking from the experiment. The ship should enter,
negotiate both shallow corners, and cross the finish while still moving at 10 m/s.

## Required backend change

Non-zero terminal velocity used to be impossible because:
1. runtime rejected non-zero finish speed;
2. Ruckig route forcibly overwrote final waypoint velocity with zero.

Now `TrajectoryGenerationRequest` has:
- `hasTerminalVelocity`;
- `terminalVelocityMps`.

Runtime authors the exact terminal velocity from finish forward * finish speed.
Ruckig honors it. Point speed constraints remain route speed limits.

Candidate commits:
- c9a8e381531feee4516cafa436b67809fb2d753d
- 9372af939d2406768e530f9e2c02e05389f0df83
- 82dc142e5c06f8be8e6c94aec810f16b8b47302f
- 75f11481979b83706861bbc98f8b6326341a07e0
- d40e4fdc7cb1048d0f45daf2598788684e49affc
- bc5fbaa284663d7a986e2257b8d71f4a64a610ba
- 7f57df3ef4a05d6601d05aa9c94de7121e0d9884

## Immediate gate

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
```

Then launch:
```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Collect:
- HEAD;
- focused Ruckig 8/8 or exact failure;
- Stage-2 `RETAINED WAYPOINT SPEEDS`;
- final speed;
- `MAX BODY/VELOCITY ANGLE`;
- visual relation between yellow actual V, cyan actual nose, red target nose.

## Next architecture step after this gate

Do not move to dynamic avoidance yet.

Once StopTurnGo and start/finish transients are removed from the stand, fix the deeper
Newtonian issue: translational P/V/A must not assume future hull attitude. The accepted
maneuver must be body/thrust-aware before Follower execution and must preserve the
B4 -> B5 -> B6 -> B7 -> B8 -> B9/B10 ownership chain.

## Mandatory state protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
