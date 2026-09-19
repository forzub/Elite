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

## Current candidate

```
76346121516e5b00d14a4e6304621b55791093ab
```

Implementation:
- one coherent 4 s / 40 m moving recovery;
- smooth 90 deg rotation completes in 2.5 s;
- remaining 1.5 s continues at 10 m/s with final yaw held;
- no follower-side hidden replanning;
- no tolerance widening;
- no generic tracking-reserve inflation.

This is the intended mechanism change: give the physical ship a real in-motion settle window before the common exit.

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
