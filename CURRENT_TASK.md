# Elite — CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv  
**Branch:** `main`

## Task

Validate the corrected physical authoring of the 90-degree corner-family matrix.

Latest tested checkout:
```text
ca0c5506a9912bf016e4c52c7545ddd35c66e1fb
```

Result:
- architecture PASS;
- navigation_runtime 14/15 PASS;
- ManeuverPhaseGate PASS;
- only maneuver_corner_family_matrix FAIL.

Current unverified code candidate:
```text
f7e17a1a631157bc4cc8763c226ea73b57adeb23
```

## What was wrong

The gate itself is correct. It exposed a planner/reference defect.

Newtonian StopTurnGo scheduled only 2.6 s for a 180-degree flip and then began
aft-main braking. The actual rigid body had not completed the attitude state
needed by that dependent burn. StateCapture then correctly refused to pretend
the brake waypoint/stop was captured.

Zero terminal brake feed-forward plus only 0.55 m/s2 reserved B10 feedback
cannot repair a multi-m/s maneuver-authoring miss. Increasing timeout would only
let the bad reference drift farther.

Secondary defect: expert DriftTurn completes P/V/corridor correctly but exits at
~9.67 deg instead of <=5 deg.

## Fix now in main

### StopTurnGo Newtonian
- pre-brake total remains 5.375 s;
- approach: 2.775 -> 0.375 s;
- flip: 2.6 -> 5.0 s;
- brake location remains unchanged;
- brake duration remains 1.25 s;
- StateCapture remains strict.

This follows the already-green rigid-body flip timing instead of asking the
follower to compensate for an under-timed planner maneuver.

### DriftTurn
- old recovery: 3 s rotate + 1 s coast;
- new recovery: one continuous 4 s moving rotate;
- same 40 m outgoing travel;
- same total recovery time.

## Run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Return the complete CORNER-MATRIX and CORNER-COMPARE output if anything fails.

## Acceptance

Expert rows must satisfy the existing thresholds without relaxation:
- all modes complete;
- no 32 m corridor violation;
- final P <=1.5 m;
- final V <=1.0 m/s;
- final attitude <=5 deg;
- StopTurnGo minimum corner speed <=0.75 m/s;
- RadiusTurn minimum speed >=5 m/s and slip <=20 deg;
- DriftTurn minimum speed >=7 m/s and slip >=60 deg.

Additional StopTurnGo check:
```text
capture_timeout_phases=0
```

RadiusTurn must not regress.

Competent/rookie remain diagnostic in this pass; their metrics are used to
characterize PilotSkill execution reserve, not to weaken expert quality.

## If the gate still fails

StopTurnGo timeout:
- do not extend timeout;
- print/inspect terminal P/V/forward/omega at the timed-out phase;
- identify whether the remaining miss is brake reference, flip capture, or
  PilotSkill command dynamics.

Drift attitude >5 deg:
- isolate angular recovery reference/control;
- keep speed, travel distance and corridor unchanged.

## Next only after green

1. mixed-angle 3-4 segment 3D corridor;
2. real route-time comparison;
3. Rational / Freestyle / Extreme speed-doctrine matrix;
4. B6 proof and B7 selection integration.

## Iteration rule

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.

Do not promote an untested code candidate to accepted.
