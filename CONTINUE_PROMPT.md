# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth. Do not incrementally patch stale prompt prose. The newly recreated prompt must contain this same rule again.

## Latest exact verified target-machine evidence

Tested checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

Results:
- Stage-12 architecture contract PASS.
- `navigation_runtime` 14/15.
- StopTurnGo expert healthy.
- RadiusTurn healthy.
- Long 180 deg arc healthy for expert/competent/rookie under Newtonian and Assisted.
- Remaining strict failure is DriftTurn exit attitude.

The prior 2.5 s rotate + 1.5 s settle experiment was rejected: expert DriftTurn worsened from ~10.325 deg to ~16.889 deg while P/V/corridor remained good.

## Diagnosis

The follower already continuously reduces:
- position error;
- velocity error;
- attitude error;
- angular-velocity error.

The mistake was test-flow semantics. The old x=60 route checkpoint was also the end of the DriftTurn `ScheduledMoving` phase. Since `ScheduledMoving` advances at nominal program end regardless of residual tracking error, the geometric checkpoint became an artificial attitude deadline.

The old 4 s value came from 40 m remaining distance / 10 m/s speed. It was valid as travel time, not as a physical attitude-completion condition.

## Current unverified candidate

```
24fce76b30d2448a4b94c93ad78dd8d37e5102df
```

Changes in `tests/navigation_runtime/ManeuverCornerFamilyMatrixTests.cpp`:
- remove the timed DriftTurn rotate+settle helper;
- after the drift arc, publish a moving straight reference at 10 m/s with final outgoing heading and zero target angular rate;
- keep B10 continuously converging attitude/angular-rate while translation continues;
- old x=60 is only crossed, not used to terminate control;
- extend common outgoing test corridor to x=120;
- record the first point after x=60 where:
  - forward error <=5 deg;
  - angular speed <=0.08 rad/s;
- print:
  - `outgoing_attitude_captured`;
  - `outgoing_attitude_capture_x_m`;
  - `outgoing_attitude_capture_after_old_exit_m`.

StopTurnGo and RadiusTurn also continue through the same extended outgoing route so all families have a common finite test terminus.

No production follower gain, capability, corridor width or acceptance threshold was weakened.

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
- expert DriftTurn final P <=1.5 m;
- final V <=1.0 m/s;
- final attitude <=5 deg;
- zero 32 m corridor violation;
- sustained-speed/material-slip semantics preserved;
- `outgoing_attitude_captured=1`;
- long arc remains green.

## If green

Do not linger on this gate. Record exact tested HEAD and acceptance evidence, explain the closed defect (checkpoint/phase-end conflation), then analyze and begin the next mixed-angle multi-segment 3D corridor test. Identify implementation risks and acceptance metrics before coding it.

## Architecture ownership

- Planner owns route/corridor, maneuver family, physical reference, proof and accepted trajectory.
- Follower samples/tracks that accepted trajectory and applies bounded feedback/safety monitoring.
- Follower must not invent a different maneuver family.
- AcceptedManeuverProgram is the planner/follower boundary.
- Newtonian/Assisted physical distinctions remain.
- Manual guidance later visualizes the same accepted trajectory.
- Planner never mutates authoritative physics state.

**Again:** recreate this entire prompt from scratch after every state-affecting iteration.
