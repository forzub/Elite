# CONTINUE PROMPT — Elite Navigation: Assisted runaway target validation

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `src/game/navigation/ManeuverTrackingController.cpp/.h`
- `src/game/navigation/ManeuverProgramSampler.cpp/.h`
- `src/game/navigation/TrajectoryFollower.cpp/.h`
- `src/game/navigation/ManeuverPhaseGate.cpp/.h`
- `tests/navigation_runtime/ManeuverTrackingControllerTests.cpp`
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`.

## Current code candidate

Before the documentation-only synchronization commits:

```text
13ef6bd731ef6d8e78c75c72bf7a59f524b30bcb
```

Target-machine validation is pending. Never claim PASS before it is supplied.

## Latest target failure

```text
ASSISTED / EXPERT / STANDARD
START 10.00 M/S
FINISH 10.00 M/S

TRAJECTORY: RUCKIG OK
CALCULATED MAX SPEED: 11.71 M/S
PROGRAM PHASES COMPLETE: NO
PHYSICAL TERMINAL STATE: MISSED
FINAL POSITION ERROR: 2411.14 M
FINAL SPEED: 100.64 M/S
REFERENCE CLOCK HOLD: 47.70 S
MAX BODY/VELOCITY ANGLE: 179.99 DEG
COARSE STATIC CONTACT: YES
```

The speed planner did **not** request 100 m/s. This was physical execution
runaway.

## Root cause A — held moving reference kept derivatives alive

Reference progress pause froze a moving trajectory sample but continued to
apply that sample's feed-forward / rate derivatives indefinitely. B10 recovery
reserve could not cancel them.

Current B10:
- normal in-envelope tracking uses accepted feed-forward;
- out-of-envelope reacquisition uses zero linear/angular feed-forward;
- recovery damps actual angular rate toward zero;
- feedback remains bounded by the proved reserve;
- accepted moving reference resumes when the physical craft re-enters.

Regression:
`testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives()`.

## Root cause B — dense angular velocity was copied into sparse FreeTransit

FreeTransit is <=16 samples. Copying an instantaneous dense attitude
`angularVelocity` into a sparse program caused B9 to smear short rate peaks
across long intervals. The visible sparse basis and the commanded omega were
therefore inconsistent.

Current authoring:
- keep sparse basis as authority;
- re-derive interior omega from neighboring sparse basis/time samples;
- omega first/last = 0;
- alpha_ff = 0;
- clamp sparse omega to physical angular-rate capability.

## Viewer

Normal F / ВПИСАТЬ fit ignores the full physical frame history. A runaway
physical trace can no longer shrink the authored route/obstacles to a postage
stamp.

## Exact regressions

1. Assisted / Expert / Standard 10 -> 10:
   `testAssistedLowSpeedDoesNotRunAwayDuringReferenceHold()`
   - physical max speed <= 25 m/s;
   - execution success;
   - final position <= 5 m;
   - final speed 10 +/- 1.5 m/s.

2. Newtonian / Expert / Standard 26.15 -> 11.75:
   existing high-speed reacquisition regression.

## Run next

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

First inspect exactly:
`ASSISTED / EXPERT / STANDARD, 10.00 -> 10.00 m/s`.

Required:
- no runaway acceleration;
- no persistent tumbling;
- hold/reacquisition converges;
- phases complete physically;
- final position <= 5 m;
- no static contact;
- camera fit remains useful.

Do not enable dynamic avoidance.
Do not change route geometry or loosen acceptance tolerances to hide an
execution failure.
If the test still fails, inspect telemetry at the **first**
`follower_reacquiring` transition and compare:
`ref_forward`, body forward, body/velocity angle, ideal/exec linear demand,
ideal/exec angular demand, and reference-clock hold onset.
