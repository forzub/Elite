# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Task

Validate the corrected DriftTurn flow where route progress and attitude convergence are decoupled from the old x=60 checkpoint.

## Latest verified baseline

Exact tested checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

Architecture PASS; runtime 14/15.

The previous 2.5 s rotate + 1.5 s settle experiment was rejected because it worsened expert DriftTurn final attitude to ~16.889 deg.

## Current unverified candidate

```
24fce76b30d2448a4b94c93ad78dd8d37e5102df
```

### Corrected semantics

The outgoing DriftTurn straight is now a **continuous tracking problem**, not a timed attitude deadline.

After the arc:
- reference position keeps advancing at 10 m/s;
- target body heading is the outgoing corridor heading;
- target angular velocity is zero;
- B10 continuously reduces P/V/attitude/angular-rate error;
- old x=60 is crossed without ending the control phase.

The test route continues to x=120 and records where the ship first captures outgoing attitude:
- forward error <=5 deg;
- angular speed <=0.08 rad/s.

This directly tests the hypothesis that the old failure came from conflating a geometric checkpoint with phase termination.

## Run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

OUT="navigation_test_$(date +%Y%m%d-%H%M%S).txt"

{
    echo "===== TESTED HEAD ====="
    git rev-parse HEAD
    echo
    echo "===== ARCHITECTURE CONTRACT ====="
    TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
    time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
    echo
    echo "===== NAVIGATION RUNTIME ====="
    bash tests/navigation_runtime/run_mingw64.sh
} 2>&1 | tee "$OUT"

echo "$PWD/$OUT"
```

## Acceptance

Need:
- 15/15 runtime;
- expert DriftTurn `outgoing_attitude_captured=1`;
- final attitude <=5 deg;
- final P <=1.5 m;
- final V <=1.0 m/s;
- zero corridor violation;
- long arc stays green.

If this is green, close the corner-family execution defect and move to analysis/design of the next mixed-angle multi-segment 3D corridor test.

## Iteration rule

After every state/evidence change, synchronize all project MD files and recreate `CONTINUE_PROMPT.md` from scratch.
