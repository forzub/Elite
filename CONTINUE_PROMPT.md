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
- Long 180 deg angular-tracking diagnostic is healthy across expert/competent/rookie and both laws.
- Only strict expert failure is DriftTurn terminal attitude: ~10.325 deg vs <=5 deg.
- Drift final P/V/corridor are already good.

The long arc proves the ship can continuously rotate accurately while translating. Therefore the remaining defect is DriftTurn recovery/reference authoring, not general follower angular tracking.

## Current unverified candidate

```
76346121516e5b00d14a4e6304621b55791093ab
```

Change in `tests/navigation_runtime/ManeuverCornerFamilyMatrixTests.cpp`:
- keep the final DriftTurn recovery as one coherent 4 s / 40 m accepted moving reference;
- complete the smooth 90 deg yaw recovery in 2.5 s;
- continue translating at 10 m/s for the remaining 1.5 s while holding final exit yaw;
- no follower-side strategy change;
- no tolerance/corridor/feedback widening.

Purpose: provide a physical in-motion attitude-settle window before the common exit.

## Target-machine validation

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
- existing expert final P <=1.5 m;
- final V <=1.0 m/s;
- 32 m hull corridor remains clean;
- Drift retains sustained-speed/material-slip semantics;
- long arc remains green.

## Current known follower status

No general follower angular-tracking defect is currently known: the long arc is clean. The remaining known failure is planner-authored DriftTurn recovery timing/reference. Passing this gate will close the last **currently known strict expert corner-family execution defect**, but it will not prove the entire follower bug-free for all future 3D/speed/doctrine cases.

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
