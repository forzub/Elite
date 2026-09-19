# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth. Do not incrementally patch stale prompt prose. The newly recreated prompt must contain this same rule again.

## Latest exact verified evidence

Target-machine tested checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

- Stage-12 architecture contract PASS.
- `navigation_runtime` 14/15.
- StopTurnGo expert healthy.
- RadiusTurn healthy.
- Long 180 deg arc healthy across expert/competent/rookie and Newtonian/Assisted.
- Only strict expert failure: DriftTurn terminal attitude.

Latest experiment:
- 4 s / 40 m coherent moving recovery;
- 90 deg yaw completed in 2.5 s;
- 1.5 s continued translation at target yaw.

It failed:
- expert final attitude worsened from ~10.325 deg to **16.889 deg**;
- final P ~0.473 m, V ~0.032 m/s;
- no expert corridor violation;
- expert tracking envelope did not trip.
- competent DriftTurn ~52.744 deg and 193 exceeded ticks;
- rookie DriftTurn ~21.643 deg and 127 exceeded ticks.

The long arc remains clean (expert final attitude 0.052 deg, max in-flight angular error 3.221 deg), so there is no evidence of a general follower angular-tracking defect.

## Current task

Do not tune recovery duration blindly.

Instrument DriftTurn recovery to expose:
- final actual yaw/angular velocity;
- final reference yaw/angular velocity;
- final attitude error;
- peak attitude error;
- peak angular-velocity error during recovery.

Then determine whether the terminal miss is residual angular momentum / damping or a reference/controller transient mismatch.

If residual motion is confirmed, implement a planner-authored **moving terminal capture**:
- continue position reference at terminal velocity (10 m/s);
- hold final attitude;
- command zero terminal angular velocity;
- allow bounded capture time;
- never freeze position while asking for nonzero translational velocity;
- do not let follower choose a new maneuver family.

## Do not weaken

- expert DriftTurn terminal attitude <=5 deg;
- P/V requirements;
- 32 m hull corridor;
- tracking ownership boundaries.

Do not inflate generic feedback reserve merely to pass.

## Validation

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

## Next major stage

Do **not** move to the mixed-angle 3D corridor yet. First close this last currently known strict corner-family execution failure. After a verified 15/15 result, analyze what was closed and proceed to the 3D corridor/speed-doctrine stage.

**Again:** recreate this entire prompt from scratch after every state-affecting iteration.
