# CURRENT TASK — validate physical reference reacquisition and hull damping

**Date:** 2026-09-22  
**Status:** ROOT CAUSES FIXED / TARGET MINWG64 VALIDATION REQUIRED

## Latest reproduced failure

User's high-speed run:
- START 26.15 m/s;
- FINISH 11.75 m/s;
- Newtonian / Expert / Standard;
- Ruckig OK;
- old program timeline completed;
- physical final error 68.49 m;
- max route deviation 62.24 m.

The old system let the time reference outrun the physical ship.

User also observed hull "float"/oscillation while returning to attitude.

## Fix A — attitude damping

B10 default attitude loop was strongly underdamped:
- Kp=2;
- Kd=1.

It is now:
- Kp=2;
- Kd=3.

Default policy regression requires Kd >= 2*sqrt(Kp).

Do not add hidden damping in ShipController's navigation path; B10 owns the requested
angular feedback and physical clamps remain below it.

## Fix B — reference reacquisition

AcceptedManeuverProgram remains immutable.

Runtime owns:
```text
activeProgramReferenceDelaySeconds
```

Follower/B9 reference time:
```text
physical time - reference delay
```

If `follower.trackingErrorExceeded`:
- reference delay advances by dt;
- reference progress pauses;
- phase gate sees the delayed time;
- physical Follower/Pilot/physics keep correcting;
- viewer status = FOLLOWER ВОЗВРАЩАЕТСЯ В КОРИДОР.

When inside envelope again:
- reference time resumes.

Current FreeTransit reacquisition envelope:
- position 8 m;
- linear velocity 4 m/s;
- forward error 0.35 rad;
- angular-rate error 0.8 rad/s.

Longitudinal FreeTransit deadbands are applied before envelope evaluation, so harmless
lead/lag does not pause the clock.

Diagnostics:
- REFERENCE CLOCK HOLD FRAMES
- REFERENCE CLOCK HOLD

## Fix C — completion truth

Diagnostics now distinguish:
- PROGRAM PHASES COMPLETE
- PHYSICAL TERMINAL STATE

Never again interpret elapsed timeline as physical route completion.

## Fix D — root artifacts

All current-run files are in repository root:
- last_route_plan.log
- last_execution.log
- last_execution_telemetry.log
- navigation_perf.log
- last_calculated_trace.json
- last_execution_trace.json

## High-speed E2E

New target regression uses exact failing case:
- 26.15 -> 11.75 m/s;
- Newtonian / Expert / Standard.

Requires successful physical execution and final error <= 5 m.

## Immediate validation

Run:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

If that passes, launch:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Reproduce:
1. Newtonian / Expert / Standard.
2. START about 26.1, FINISH about 11.8.
3. Calculate.
4. Watch for:
   - hull return with no repeated pendulum overshoot;
   - right status `FOLLOWER ВОЗВРАЩАЕТСЯ В КОРИДОР` if actual craft falls behind;
   - physical path bending back toward purple/blue reference instead of allowing the
     reference to escape;
   - no false "complete" while tens of meters away.
5. Send root `last_execution.log` and `last_execution_telemetry.log` if it still fails.

Also re-check Assisted after this damping fix. If Assisted still performs unnecessarily
sharp attitude motion while tracking is otherwise stable, the next slice is Assisted
reference-attitude policy (minimal/smoothed hull rotation), not another generic damping
increase.

## Mandatory state protocol

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
