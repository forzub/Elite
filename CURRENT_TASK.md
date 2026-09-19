# Elite — CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv
**Branch:** main

## Last exact target-machine baseline

```text
519313ba430b73b557622436ed7df9a5a9a832cf
```

Evidence:
- architecture contract PASS;
- navigation_runtime 13/14 PASS;
- only `maneuver_corner_family_matrix` failed.

Primary failure:
expert/Newtonian/StopTurnGo advanced through flip/brake/rotate by authored time
before real waypoint / near-zero-speed / attitude capture.

Do not weaken the corridor or expert quality thresholds.

## Mechanism implemented in this iteration

New production component:

```text
src/game/navigation/ManeuverPhaseGate.h
src/game/navigation/ManeuverPhaseGate.cpp
```

Purpose: own compound maneuver phase handoff semantics.

Modes:

### ScheduledMoving
For continuous moving internal reference phases.
- before nominal horizon: Continue;
- at/after nominal horizon: Advance.

### StateCapture
For waypoint/stop/attitude capture phases.
- after nominal horizon, keep following the terminal sample;
- Advance only when `TrajectoryFollower::Complete`;
- bounded overrun;
- if not captured by timeout: `CaptureTimedOut`;
- never silently advance a failed capture.

The gate does not plan, sample, track, mutate physics, or mutate the accepted
program.

## Isolated regression

New:
`tests/navigation_runtime/ManeuverPhaseGateTests.cpp`.

Pins:
- scheduled moving does not advance early;
- scheduled moving advances at nominal end;
- state capture holds after nominal end while follower is Following;
- state capture advances only on Follower::Complete;
- capture timeout is explicit;
- invalid follower state fails closed.

Shared runtime and isolated runtime CMake both compile the production source.

Expected navigation_runtime test count:

```text
15
```

## Corner fixture migration

`ManeuverCornerFamilyMatrixTests.cpp` now uses production
`ManeuverPhaseGate`.

### StopTurnGo

StateCapture:
- braking / waypoint capture;
- in-place outgoing attitude capture.

ScheduledMoving:
- approach/coast;
- moving Newtonian flip phase;
- outgoing acceleration;
- outgoing cruise.

Braking capture terminal feed-forward is explicitly zero, so after the nominal
braking horizon the follower converges on terminal P/V instead of continuing to
command the braking acceleration.

### RadiusTurn

Internal phases remain ScheduledMoving.
External quality still requires common exit gate + final P/V/attitude envelope.

### DriftTurn

Internal phases remain ScheduledMoving.

Exit recovery profile changed while preserving the same post-arc travel/time:
- old: 2 s rotate + 2 s coast;
- new: 3 s rotate + 1 s coast.

Goal: reduce the previously observed expert ~14.38 deg exit-attitude error
without changing total 4 s recovery horizon or 40 m outgoing travel.

## New diagnostics

Every CORNER-MATRIX row additionally prints:

```text
capture_timeout_phases
max_capture_overrun_s
```

This distinguishes:
- genuine capture requiring extra time;
- explicit capture timeout;
- ordinary scheduled moving phases.

## Static audit result

Static source/contract audit: PASS for:
- phase-gate API;
- phase-gate implementation;
- isolated tests;
- root shared-runtime CMake;
- isolated runtime CMake;
- corner StateCapture wiring;
- zero terminal braking feed-forward;
- capture diagnostics;
- 3s+1s drift recovery;
- architecture checker wiring.

This is NOT target-machine acceptance.

## Run now

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
- 15 CTests;
- new `maneuver_phase_gate` PASS;
- full 18-row CORNER-MATRIX output if corner fixture still fails;
- 9 CORNER-COMPARE rows.

Capture exact HEAD and all failure/metric lines.

## What to inspect first in result

### StopTurnGo expert/Newtonian
Target improvements:
- `capture_timeout_phases=0`;
- minimum corner speed should now approach the real stop threshold;
- P error should collapse from previous 27.26 m;
- center cross-track should collapse from previous 21.03 m;
- hull half-width should return inside 32 m;
- total time is now allowed to differ from Assisted because capture overrun is
  real execution time.

### Drift expert
Need final forward error <=5 deg while retaining:
- high slip;
- high speed;
- zero corridor violation.

If StopTurnGo now times out:
- do not extend timeout blindly;
- inspect terminal P/V/attitude and authority;
- determine whether reference/capture target is physically inconsistent.

If Drift remains >5 deg:
- inspect recovery angular tracking, not corridor width.

## Acceptance order

1. ManeuverPhaseGate isolated test green.
2. Expert StopTurnGo captures correctly.
3. Expert RadiusTurn remains green.
4. Expert DriftTurn exits <=5 deg.
5. Timing is based on actual capture, not authored phase sum.
6. Then compose 3-4 segment mixed-angle 3D corridor.
7. After that add doctrine/speed matrix: Rational / Freestyle / Extreme.
8. Then move physically validated candidates toward B6 proof / B7 choice.

## Do not

- do not widen the 32 m corridor to hide maneuver defects;
- do not weaken expert thresholds;
- do not treat a capture timeout as success;
- do not run the old 120 s obstacle live gate yet.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update architecture/migration/purity docs when contracts change.

Always distinguish target-tested baseline from newer unverified HEAD.
