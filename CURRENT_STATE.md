# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Latest verified evidence

Latest target-machine run:
- Stage-12 architecture contract: PASS;
- navigation_runtime: 14/15 PASS;
- only failing target: maneuver_corner_family_matrix;
- StopTurnGo expert healthy;
- RadiusTurn healthy;
- long 180 deg arc healthy;
- DriftTurn outgoing attitude still failed under the raw constant-heading experiment.

That experiment proved B10 tracking reserve cannot be used as the primary 90 deg maneuver generator.

## Current unverified candidate

Code candidate:

```
31a66a3eb462df6b5a60b2da2aca9f018d8aa332
```

The DriftTurn exit is now authored as a planner-side moving attitude capture.

### What changed

At the end of the drift arc the capture starts from the **actual** vehicle state:
- actual body yaw;
- actual yaw rate left by the arc;
- actual outgoing translational state.

The target is:
- outgoing corridor yaw;
- terminal yaw rate = 0;
- continued translation at 10 m/s.

A quintic yaw profile is built with:
- initial yaw = actual yaw;
- initial yaw rate = actual yaw rate;
- initial angular acceleration = 0;
- final yaw = outgoing corridor yaw;
- final yaw rate = 0;
- final angular acceleration = 0.

The profile duration is not fixed. It is solved from physical angular limits.

### Physical limits used

The candidate mirrors the real ShipController envelopes:
- effective angular acceleration =
  min(configured angularAccel, maxGs * g / turnRadius);
- effective yaw-rate limit =
  min(configured maxYawRate, sqrt(maxGs * g / turnRadius)).

For Cobra this is stricter than the raw 3 rad/s2 / 2.5 rad/s configured values.

B10 reserve is explicitly left unused by feed-forward:
- feed-forward angular acceleration limit =
  effective physical angular acceleration - 0.35 rad/s2 tracking reserve;
- an additional 5% execution/proof margin is applied.

### Why this is different from rejected attempts

Rejected attempts either:
1. forced a fixed-time yaw schedule unrelated to actual arc exit state, or
2. removed the maneuver reference entirely and asked B10 to execute a 90 deg step.

The new candidate preserves planner ownership:
- planner authors the large angular transition;
- B10 only corrects residual error.

### New diagnostics

Corner rows now also report:
- attitude_capture_program_s;
- attitude_capture_start_yaw_rate_radps;
- attitude_capture_peak_ff_yaw_rate_radps;
- attitude_capture_peak_ff_yaw_accel_radps2;
- outgoing_attitude_captured;
- outgoing_attitude_capture_x_m.

## Acceptance target

Need:
- architecture PASS;
- runtime 15/15;
- expert DriftTurn final attitude <=5 deg;
- final P <=1.5 m;
- final V <=1.0 m/s;
- zero corridor violation;
- outgoing attitude capture;
- long arc remains green.

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

echo
echo "===== LOG FILE ====="
echo "$PWD/$OUT"
```

## Documentation protocol

After every state-affecting iteration, synchronize:
- CURRENT_STATE.md
- CURRENT_TASK.md
- PROJECT_STATE.md
- active Stage-12 document

And recreate CONTINUE_PROMPT.md from scratch.
