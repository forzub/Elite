# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth. Do not incrementally patch stale prompt prose. The recreated prompt must contain this rule again.

## Latest verified evidence

Latest target-machine run:
- architecture contract PASS;
- navigation_runtime 14/15;
- StopTurnGo expert healthy;
- RadiusTurn healthy;
- long 180 deg arc healthy;
- only strict expert failure remains DriftTurn exit attitude.

The raw constant-heading experiment failed badly because it asked B10 to execute the entire 90 deg turn from bounded tracking reserve.

## Current unverified code candidate

```
31a66a3eb462df6b5a60b2da2aca9f018d8aa332
```

## New DriftTurn exit mechanism

A planner-authored **moving attitude capture** replaces the failed raw heading step.

At the end of the drift arc it reads the actual physical state:
- actual yaw;
- actual yaw rate;
- current position;
- continued 10 m/s outgoing velocity.

It solves a quintic angular boundary-value trajectory:
- theta(0) = actual yaw;
- omega(0) = actual yaw rate;
- alpha(0) = 0;
- theta(T) = outgoing corridor yaw;
- omega(T) = 0;
- alpha(T) = 0.

T is not a fixed 4 s deadline. It is the shortest duration that fits the effective physical angular envelopes.

Effective limits mirror ShipController:
- angular accel <= min(configured angularAccel, maxGs*g/turnRadius);
- yaw rate <= min(configured maxYawRate, sqrt(maxGs*g/turnRadius)).

The profile reserves B10 correction authority:
- feed-forward acceleration stays below effective acceleration minus the 0.35 rad/s2 tracking reserve;
- an extra 5% execution margin is used.

Position reference continues moving during the entire capture.

## New diagnostics

Inspect:
- attitude_capture_program_s
- attitude_capture_start_yaw_rate_radps
- attitude_capture_peak_ff_yaw_rate_radps
- attitude_capture_peak_ff_yaw_accel_radps2
- outgoing_attitude_captured
- outgoing_attitude_capture_x_m
- tracking_envelope_exceeded_ticks
- final_forward_error_deg

## Validation commands

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

echo
echo "===== LOG FILE ====="
echo "$PWD/$OUT"
```

## Acceptance

Require:
- architecture PASS;
- runtime 15/15;
- expert DriftTurn final attitude <=5 deg;
- P <=1.5 m;
- V <=1.0 m/s;
- zero corridor violation;
- moving attitude capture observed;
- long arc remains green.

## If green

Record exact tested HEAD and acceptance evidence, close the DriftTurn exit-authoring defect, analyze the next mixed-angle multi-segment 3D corridor stage, identify implementation/test risks, and proceed directly.

**Again:** recreate this prompt from scratch after every state-affecting iteration.
