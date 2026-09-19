# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth. Do not incrementally patch stale prompt prose. The newly recreated prompt must contain this same rule again.

## Latest exact verified target-machine evidence

Tested checkout:

```
d659416b9b1ddb2356c37eff315f9d13b70bafaa
```

Results:
- Stage-12 architecture contract PASS.
- `navigation_runtime` 14/15.
- Expert StopTurnGo is healthy.
- RadiusTurn is healthy.
- Only strict expert failure is DriftTurn terminal attitude:
  - Newtonian 10.324757 deg;
  - Assisted 10.324757 deg;
  - required <=5 deg.
- Drift final P/V and corridor are already good.

## Long-arc diagnostic conclusion

The added 180 deg arc (R=80 m, v=10 m/s, ~251.33 m, ~25.13 s) completed cleanly for every PilotSkill and both laws.

Expert:
- final P error 0.332 m;
- final V error 0.036 m/s;
- final attitude error 0.052 deg;
- max in-flight forward/tangent error 3.221 deg;
- zero tracking-envelope exceed ticks.

Competent final attitude error is 0.048 deg; rookie 0.859 deg. All complete.

This proves the ship **can rotate accurately while translating** and the general B9/B10 angular-tracking chain is not the source of the DriftTurn miss.

## Current task

Fix the DriftTurn recovery/reference authoring.

The current recovery asks for a moving 90 deg attitude change over its last 4 s / 40 m, but the accepted reference ends while the physical ship is still ~10.3 deg short.

Implement a coherent moving recovery that:
- preserves the intended 10 m/s translational exit;
- reaches the target yaw before the final endpoint;
- holds target yaw for a short in-motion settle interval;
- remains one planner-authored AcceptedManeuverProgram/recovery reference;
- does not ask the follower to invent a new maneuver;
- does not widen the 5 deg terminal gate, corridor or generic tracking reserve.

A suitable first implementation is a coast+rotate+settle trajectory: use part of the final 4 s to complete the smooth yaw change, then continue the same translational motion at target yaw for the remaining fraction.

## Validation commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Acceptance:
- expert DriftTurn final attitude <=5 deg;
- existing expert P <=1.5 m, V <=1.0 m/s and 32 m hull corridor remain;
- Drift retains sustained speed/slip semantics;
- long arc remains green.

## Architecture ownership

- Planner owns route/corridor, maneuver family, physical reference, continuous proof and accepted trajectory.
- Follower samples/tracks the accepted trajectory, applies bounded feedback and safety monitoring.
- Follower must not invent an alternate maneuver strategy.
- AcceptedManeuverProgram is the planner/follower boundary.
- Newtonian/Assisted physical distinctions remain real.
- Manual guidance later visualizes the same accepted route/trajectory.
- Planner never mutates authoritative physics state.

## After this gate

Once DriftTurn and long arc are green, proceed to mixed-angle multi-segment 3D corridor quality, then speed/doctrine coverage.

**Again:** recreate this entire prompt from scratch after every state-affecting iteration.
