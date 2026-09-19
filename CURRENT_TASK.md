# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Task

Resolve the remaining DriftTurn terminal-attitude defect from measured angular state, not by timing guesswork.

## Latest verified target state

Exact tested checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

Architecture PASS; runtime 14/15.

The 2.5 s rotate + 1.5 s moving-settle experiment failed:
- expert DriftTurn final attitude error worsened from ~10.325 deg to **16.889 deg**;
- P/V/corridor remain good;
- long arc remains clean.

This proves:
- general angular tracking is healthy;
- simply making the Drift recovery faster and adding a passive settle window is not sufficient.

## Next step

Add DriftTurn recovery diagnostics for:
- actual final angular velocity / yaw rate;
- reference final angular velocity;
- final attitude error;
- peak attitude/angular-velocity tracking error during the recovery.

Use the result to distinguish:
1. residual angular momentum / insufficient terminal damping;
2. reference/controller mismatch during the aggressive recovery profile.

If residual motion is the issue, implement a planner-authored **moving terminal capture**: after nominal recovery, continue an advancing position reference at the terminal velocity while holding final attitude and damping angular rate. Do not use frozen-position StateCapture for a moving exit.

Do not:
- relax the 5 deg exit requirement;
- inflate generic feedback reserve;
- widen corridor;
- move strategy selection into follower.

## Validation commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
```

## Iteration rule

After each code/evidence change, synchronize all state MD files and recreate `CONTINUE_PROMPT.md` from scratch.
