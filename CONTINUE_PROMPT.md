# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth. Do not incrementally patch stale prompt prose. The newly recreated prompt must contain this rule again.

## Current exact verified evidence

Latest target-machine tested checkout:

```
be4686f4dcba419f451813b1ddc246088c145e48
```

Results:
- Stage-12 architecture contract PASS.
- `navigation_runtime` 14/15.
- Only failing test: `maneuver_corner_family_matrix`.
- Expert Newtonian StopTurnGo is now fixed and completes cleanly.
- RadiusTurn remains healthy.
- Remaining strict expert failure is DriftTurn common-exit attitude: about 10.325 deg instead of <=5 deg.
- Drift P/V and corridor are otherwise good.
- Rookie StopTurnGo timeouts remain diagnostic and are not the current strict failure.

The 5 deg rule is a **terminal exit-attitude requirement**. The ship is allowed and expected to correct attitude while translating.

## Current unverified candidate

```
5c16bedc25c422f2c79ae5def14396839ef4ee7c
```

Change:
- add a long 180 deg curved-flight probe to `tests/navigation_runtime/ManeuverCornerFamilyMatrixTests.cpp`;
- R=80 m, v=10 m/s, arc length ~251.33 m, duration ~25.13 s;
- run expert/competent/rookie under Newtonian and Assisted;
- report maximum centerline error, rigid-hull corridor demand, maximum in-flight forward/tangent error, tracking-envelope exceed ticks, and final P/V/attitude errors;
- keep existing corner/Drift behavior and thresholds unchanged.

Expert long-arc gates:
- same 32 m hull half-width corridor;
- final P <=1.5 m;
- final V <=1.0 m/s;
- final attitude <=5 deg;
- max in-flight forward/tangent error <=10 deg.

Purpose:
- clean long arc + bad Drift exit => DriftTurn recovery/reference defect;
- bad long arc too => general sampler/follower angular-tracking defect.

Do not widen tolerances or tracking reserve to make the test green before this split is understood.

## Target-machine commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

When the result arrives:
1. record the exact tested HEAD;
2. inspect all `[LONG-ARC]` rows and expert corner rows;
3. decide whether angular error is general or Drift-specific;
4. fix the mechanism, not the threshold;
5. update state MDs;
6. recreate this prompt from scratch again.

## Architecture ownership that must not regress

- Planner: route/corridor, maneuver family, physical reference, continuous proof, accepted trajectory.
- Follower: sample/track accepted trajectory, bounded feedback, monitoring/reflex.
- Follower must not invent an alternate maneuver strategy.
- AcceptedManeuverProgram is the planner/follower boundary.
- Newtonian/Assisted distinctions remain physical, not cosmetic.
- Manual guidance later visualizes the same accepted route/trajectory.
- Navigation typed working-frame boundaries remain sealed.
- Planner never mutates authoritative physics state.

## Next after this gate

After angular behavior is understood and corner + long-arc quality are green, proceed to a mixed-angle multi-segment 3D corridor, then speed/doctrine quality coverage.

**Again:** recreate this entire prompt from scratch after every state-affecting iteration.
