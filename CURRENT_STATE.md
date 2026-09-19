# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Latest exact target-machine evidence

Tested checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **14/15 PASS**.
- Only failing target: `maneuver_corner_family_matrix`.
- StopTurnGo expert healthy.
- RadiusTurn healthy.
- Long 180 deg arc healthy for all PilotSkill profiles and both laws.
- Remaining strict failure: DriftTurn terminal attitude.
- The rejected timed recovery experiment (2.5 s rotate + 1.5 s settle) worsened expert DriftTurn to ~16.889 deg while P/V/corridor stayed good.

## Diagnosis

The long arc proves that the follower can continuously reduce angular error while translating. The remaining defect is in the corner-family fixture semantics:

- the old common checkpoint `x=60` was also treated as the end of the DriftTurn control phase;
- `ScheduledMoving` advances at the nominal program end regardless of residual tracking error;
- therefore an arbitrary route checkpoint became an artificial attitude deadline.

The `4 s` value came from geometry (`40 m / 10 m/s`), not from a physical attitude-settling requirement. Using it as an angular completion deadline was the mistake.

## Current unverified candidate

```
24fce76b30d2448a4b94c93ad78dd8d37e5102df
```

Changes:
- removes the timed DriftTurn rotate+settle helper;
- after the drift arc, the accepted reference immediately adopts the outgoing corridor heading and continues translating at 10 m/s;
- B10 continuously reduces attitude/angular-rate error while the position reference continues moving;
- the old `x=60` point is now only a passed checkpoint, not the end of control;
- common outgoing route is extended to `x=120`;
- test records where the moving ship first satisfies:
  - forward error <=5 deg;
  - angular speed <=0.08 rad/s;
- new diagnostics report:
  - `outgoing_attitude_captured`;
  - `outgoing_attitude_capture_x_m`;
  - distance after old x=60 checkpoint at which capture occurs.

StopTurnGo and RadiusTurn also receive the same continued outgoing straight so all expert families finish at the same extended route endpoint.

No production follower gains, capability, corridor width or acceptance thresholds were weakened.

## Acceptance target

On target machine:
- architecture contract PASS;
- runtime 15/15;
- expert DriftTurn remains inside 32 m hull corridor;
- expert DriftTurn retains sustained-speed/material-slip semantics;
- final P <=1.5 m;
- final V <=1.0 m/s;
- final attitude <=5 deg;
- `outgoing_attitude_captured=1`;
- long-arc diagnostic remains green.

## Next commands

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

## Architecture invariants

- Planner owns maneuver choice, trajectory/corridor and proof.
- Follower owns sampling/tracking, bounded feedback and safety monitoring.
- AcceptedManeuverProgram remains the planner/follower contract.
- Follower must not invent an alternate maneuver family.
- Manual guidance must visualize the same accepted route/trajectory.
- Newtonian and Assisted laws remain physically distinct.
- Planner cannot mutate authoritative physics state.

## Documentation protocol

After every state-affecting iteration, synchronize:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- active Stage-12 document

And recreate `CONTINUE_PROMPT.md` **from scratch** from current truth.
