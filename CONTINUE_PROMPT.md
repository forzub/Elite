# CONTINUE PROMPT — Elite Navigation: physical control chain + somersault fix

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read those files first, then inspect:
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tools/navigation_runtime/NavigationTrace.h/.cpp`
- `src/game/navigation/AcceptedManeuverProgram.h`
- `src/game/navigation/ManeuverProgramSampler.cpp`
- `src/game/navigation/TrajectoryFollower.cpp`
- `src/game/navigation/ManeuverTrackingController.cpp`
- `src/game/navigation/NavigationRuntimeControlBridge.cpp`
- `src/game/shared/SharedShipPhysics.cpp`
- `src/game/ship/ShipController.cpp`
- `src/game/navigation/DynamicMotionSystem.cpp`.

## Actual control-chain contract

The runtime ship is physically controlled, not directly animated:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> TrajectoryFollower
 -> ManeuverTrackingController
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState navigation demands
 -> SharedShipPhysics / ShipController
 -> DynamicMotionSystem main+RCS allocation
 -> ShipTransform/DynamicMotionState
```

Be explicit that a remaining architectural gap still exists upstream: trajectory and
attitude are currently authored as a reference program rather than by the final fully
body/thrust-coupled B5/B6 physical maneuver compiler/prover.

## Telemetry-proven somersault diagnosis

User supplied `last_execution_telemetry.log`.

Important evidence:
- around t=11.57s actual forward nearly equals reference forward while pitch rate is
  already about +0.75 rad/s;
- thereafter reference stabilizes but pitch rate continues increasing/saturating around
  +1.57 rad/s;
- physical body rotates more than 120 degrees from reference while MAIN is zero;
- later body reaches almost 179 degrees from velocity/reference with MAIN still zero;
- main thrust begins only after body is already nearly inverted.

Therefore do not blame main-engine vectoring. The angular-command loop was winding the
body up.

## Root cause and current fix candidate

`AcceptedManeuverProgram::kMaxSamples = 16`.

The previous FreeTransit program:
- differentiated dense attitude into angular velocity/acceleration;
- downsampled to <=16 samples;
- linearly interpolated angular acceleration feed-forward;
- added that feed-forward to controller feedback without final capability clamp.

This can smear a short angular-acceleration impulse across a large time span.

Current candidate:
- FreeTransit publishes ZERO angular-acceleration feed-forward;
- reference basis + reference angular velocity + feedback drive attitude;
- total angular acceleration demand is clamped to accepted physical capability.

Commits:
- `018fd2c08874cb426a9d3fc8340c85ffb4539bad`
- `e8369c0fa21cc59f0429741540dd4903f7f556e8`

## Speed slider validity fix

The prior `invalid Ruckig route request` after changing START/FINISH was because the
explicit terminal velocity could exceed the nominal style `maxSpeedMps` (10 Standard,
18 Extreme).

Now Stage-2 max speed envelope is:
```text
max(style speed, requested start speed, requested finish speed)
```

This is an execution-only test override; static route remains retained.

## Automatic Stage-2 refresh

After one successful Calculate:
- control law click auto-executes Stage-2;
- pilot click auto-executes Stage-2;
- style click auto-executes Stage-2;
- obstacle toggle auto-executes Stage-2;
- START/FINISH slider release auto-executes Stage-2.

First successful Calculate itself also queues the first Stage-2 execution.

Commit:
- `7a164a65292a47e41c19a01bdcb3f55b93d0110d`

Do not reintroduce a requirement to press Calculate for execution-only settings.

## New control telemetry

Trace/log now records every sampled layer:
- `ideal_lin_cmd`
- `ideal_ang_cmd`
- `exec_lin_cmd`
- `exec_ang_cmd`
- then physical `main_a`, `rcs_a`, `engine_a`, `pyr_rate`, body/reference basis.

Commits:
- `a2ece66294ab0a8aa3c4258257dbcdbe0c72b086`
- `4343b7d5289fca95e6989f3e7d21f691723361bc`
- `9efe6934aa3f8141604e81e714319126a83afa6a`

This is now the authoritative way to answer "who pulled what lever".

## Next target run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Click Calculate once, then change execution settings without Calculate.

Check:
- automatic fresh Stage-2 trace each time;
- speeds >10 Standard / >18 Extreme no longer fail merely at request validation;
- old body somersault disappears or is materially reduced;
- NAV REV matches build.

If rotation remains, inspect new telemetry command fields before changing more geometry.

Do not enable dynamic avoidance yet.
