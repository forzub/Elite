# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Task

Fix the remaining DriftTurn exit-attitude error by correcting the maneuver reference, not by weakening acceptance or follower limits.

## Latest verified target state

Exact tested checkout:

```
d659416b9b1ddb2356c37eff315f9d13b70bafaa
```

Stage-12 architecture PASS; runtime 14/15.

### What the diagnostic proved

The 180 deg long arc proves the ship can continuously correct attitude while translating:
- expert final attitude error: 0.052 deg;
- expert max in-flight forward/tangent error: 3.221 deg;
- competent final attitude error: 0.048 deg;
- rookie final attitude error: 0.859 deg;
- all long-arc rows completed with zero tracking-envelope exceed ticks.

So the remaining ~10.325 deg expert DriftTurn exit error is **DriftTurn-specific reference/recovery authoring**, not a general B9/B10 angular-tracking defect.

## Required mechanism change

Keep DriftTurn translational behavior and common exit contract intact, but author the accepted recovery so the ship has time to finish the turn before the terminal gate.

Preferred shape:
- one coherent moving recovery program;
- reach target exit yaw before the end of translational travel;
- keep final yaw commanded for a short moving settle interval;
- preserve 10 m/s exit motion and corridor;
- no follower-side hidden replanning;
- no tolerance widening;
- no generic tracking-reserve inflation.

A natural first implementation is a coast+rotate+settle reference that reaches the final yaw before the program endpoint and continues the same translation while holding target yaw.

## Validation

Rerun:
```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
```

Success criterion remains expert DriftTurn final attitude <=5 deg with existing P/V/corridor requirements, while long-arc behavior must stay green.

## Iteration rule

After the code/result changes state, synchronize all project MD files and recreate `CONTINUE_PROMPT.md` from scratch.
