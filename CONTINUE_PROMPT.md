# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

## Read first

1. CURRENT_TASK.md
2. CURRENT_STATE.md
3. PROJECT_STATE.md
4. src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md
5. src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md
6. src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md
7. src/game/navigation/STAGE12_END_TO_END.md

## Last exact target-machine baseline

`519313ba430b73b557622436ed7df9a5a9a832cf`.

At that checkout:
- architecture PASS;
- navigation_runtime 13/14 PASS;
- only maneuver_corner_family_matrix failed.

Failure:
expert/Newtonian/StopTurnGo advanced internal phases by authored time instead of
real state capture:
- P error 27.257718 m;
- min corner speed 2.933115 m/s;
- center cross-track 21.026823 m;
- hull half-width 34.330940 m;
- 32 m corridor violation 2.330940 m;
- 40 tracking-envelope exceed ticks.

RadiusTurn was already strong.
Drift physically worked but expert exit attitude remained ~14.38 deg.

## New unverified mechanism

Production:
- src/game/navigation/ManeuverPhaseGate.h
- src/game/navigation/ManeuverPhaseGate.cpp

Semantics:
- ScheduledMoving -> advance at nominal reference horizon;
- StateCapture -> continue following terminal sample until
  TrajectoryFollower::Complete;
- bounded capture overrun;
- CaptureTimedOut instead of silent phase advance.

New isolated test:
- tests/navigation_runtime/ManeuverPhaseGateTests.cpp

Expected runtime CTest count: 15.

## Corner fixture now uses production gate

StopTurnGo:
- brake/waypoint capture = StateCapture;
- outgoing in-place attitude capture = StateCapture;
- moving approach/flip/outgoing phases = ScheduledMoving.

Brake terminal acceleration feed-forward is zeroed for capture convergence.

RadiusTurn:
- moving phases ScheduledMoving.

DriftTurn:
- moving phases ScheduledMoving;
- recovery changed from 2 s rotate + 2 s coast to 3 s rotate + 1 s coast;
- same total 4 s and same 40 m post-arc travel.

New row diagnostics:
- capture_timeout_phases;
- max_capture_overrun_s.

Static audit passed, but NO target-machine evidence exists for this new HEAD.

## Run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected:
- architecture PASS;
- 15 tests;
- maneuver_phase_gate PASS.

Inspect all CORNER-MATRIX/CORNER-COMPARE rows.

Desired expert outcomes:
- StopTurnGo capture_timeout_phases=0 and genuine near-stop;
- no 32 m hull-corridor violation;
- RadiusTurn remains green;
- Drift retains high-slip/high-speed character and final attitude <=5 deg.

If StopTurnGo times out, do not increase timeout blindly. Inspect which terminal
component P/V/attitude/omega prevents capture.

If Drift still misses attitude, fix recovery reference/control timing without
widening corridor or weakening threshold.

Only after this gate:
- construct 3-4 segment mixed-angle 3D corridor;
- compare actual Newtonian vs Assisted route time;
- compare PilotSkill;
- add Rational/Freestyle/Extreme doctrine-speed requirements;
- proceed toward B6/B7.

## Workflow invariant

Work directly in repo.
After every state-affecting event:
- rewrite CURRENT_TASK.md;
- recreate CONTINUE_PROMPT.md from the true current state;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update STAGE12_END_TO_END.md;
- update canonical architecture/migration docs when contracts change.

Never call a newer candidate accepted without target-machine evidence.
