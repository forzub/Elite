# CURRENT TASK — target-validate automatic Stage-2 refresh and angular-control fix

**Date:** 2026-09-22  
**Status:** SOMERSAULT ROOT-CAUSED FROM TELEMETRY / FIX CANDIDATE UNVERIFIED

## What actually controls the ship

Current Stage-2 runtime control chain:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> TrajectoryFollower
 -> ManeuverTrackingController
 -> NavigationSystemControlIntent
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState
 -> SharedShipPhysics / ShipController
 -> DynamicMotionSystem
 -> physical ShipTransform/DynamicMotionState
```

So the viewer is not moving a material point directly.

The remaining architectural caveat is upstream: the accepted reference trajectory and
body attitude are still authored separately rather than by the final B5/B6
body/thrust-coupled physical maneuver compiler/prover.

## Uploaded telemetry diagnosis

The previous telemetry proves the remaining flip is not requested by the main engine.

At the first runaway:
- body nearly matches reference around t ~= 11.57 s while pitch rate is already
  ~+0.75 rad/s;
- reference settles toward the path tangent, but physical pitch rate continues rising to
  ~+1.57 rad/s;
- body then rotates more than 120 degrees away while main engine is zero for much of the
  event.

A later event reaches almost 179 degrees body-vs-reference/velocity with MAIN still zero.

This points to angular-command generation/tracking, not main-thrust vectoring.

## Candidate angular fix

Root cause:
- FreeTransit phase stores at most 16 AcceptedManeuverProgram samples;
- dense attitude angular acceleration was downsampled and then linearly interpolated;
- short alpha pulses were therefore smeared over long intervals;
- final feed-forward + feedback angular demand had no final accepted-capability clamp.

Changes:
- FreeTransit angular acceleration feed-forward = 0;
- reference orientation + reference angular velocity + closed-loop feedback own body
  tracking;
- total angular demand is clamped to accepted max angular acceleration.

Commits:
- 018fd2c08874cb426a9d3fc8340c85ffb4539bad
- e8369c0fa21cc59f0429741540dd4903f7f556e8

## Speed-slider failure fixed

The 5..50 m/s slider exposed a request-validity bug:
- STANDARD trajectory envelope was still max 10 m/s;
- EXTREME was still max 18 m/s;
- terminal slider value above that was rejected as `invalid Ruckig route request`.

Stage-2 `maxSpeedMps` now expands to:
```text
max(style speed, requested start speed, requested finish speed)
```

This makes explicit diagnostic boundary speeds legal. It does not silently change the
static Stage-1 route.

Commit included in:
- 018fd2c08874cb426a9d3fc8340c85ffb4539bad

## Viewer settings now auto-run Stage-2

After the first successful Calculate:
- Assisted/Newtonian change -> automatic Stage-2 refresh;
- pilot change -> automatic Stage-2 refresh;
- Standard/Extreme change -> automatic Stage-2 refresh;
- obstacle toggle -> automatic Stage-2 refresh;
- speed slider -> one automatic Stage-2 refresh on mouse release.

The first Calculate also automatically queues the first Stage-2 execution.

No additional Calculate click is required for execution-only settings.

Commit:
- 7a164a65292a47e41c19a01bdcb3f55b93d0110d

## New command telemetry

Each execution frame/log line now includes:
- ideal_lin_cmd
- ideal_ang_cmd
- exec_lin_cmd
- exec_ang_cmd
then:
- main_a
- rcs_a
- engine_a
- actual pyr_rate / basis

Commits:
- a2ece66294ab0a8aa3c4258257dbcdbe0c72b086
- 4343b7d5289fca95e6989f3e7d21f691723361bc
- 9efe6934aa3f8141604e81e714319126a83afa6a

## Immediate target validation

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Then:
1. click Calculate once;
2. verify Stage-2 starts automatically;
3. change Assisted/Newtonian, pilot, style: each should immediately produce/replay a new
   Stage-2 trace;
4. drag START/FINISH speeds (including >10 in Standard) and release: Stage-2 should
   recalculate without `invalid Ruckig route request`;
5. reproduce the old low-route somersault if it remains;
6. send the new `tools/navigation_runtime/last_execution_telemetry.log`.

Primary acceptance:
- old runaway full somersault should disappear;
- if any rotation remains, new `ideal_ang_cmd` / `exec_ang_cmd` show exactly who
  commands it.

## Mandatory state protocol

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
