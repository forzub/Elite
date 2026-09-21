# CONTINUE PROMPT — Elite Navigation: physical hull/thrust coupling gate

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Also keep `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md` synchronized
when propulsion/control-law ownership changes.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `src/game/navigation/DynamicMotionSystem.cpp`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `src/game/navigation/ManeuverTrackingController.cpp/.h`
- `tests/navigation_runtime/NavigationRuntimeControlTests.cpp`
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`.

## Current candidate

Code baseline before documentation commits:

```text
b833eddb7bd04b5c025b2be0fd8334c33f8824e6
```

Target-machine PASS is not yet established.

## Latest evidence

The previous exponential runaway is fixed.

New target case:

```text
ASSISTED / EXPERT / STANDARD
START 20.90 M/S
FINISH 20.00 M/S

TRAJECTORY RUCKIG OK
CALCULATED MAX SPEED 20.90 M/S
PROGRAM PHASES COMPLETE NO
PHYSICAL TERMINAL STATE MISSED
FINAL ERROR 175.20 M
FINAL SPEED 7.45 M/S
REFERENCE HOLD 40.70 S
MAX BODY/VELOCITY ANGLE 180 DEG
```

## Proven root cause

Old Assisted navigation allocation had a symmetric longitudinal main channel.
A negative acceleration demand could create **fore/nose main thrust** without
rotating the hull.

Telemetry showed:
- hull/reference nearly identical;
- main_pct displayed 0;
- physical `main_a` pointed opposite hull forward;
- speed fell through zero;
- velocity reversed while hull attitude stayed unchanged;
- body/velocity angle approached 180 deg.

This violated the existing architecture statement that Assisted is not
permission to invent thrust.

## Current correction

`DynamicMotionSystem::applySystemAccelerationDemand`
- aft main only for BOTH Assisted and Newtonian;
- reverse demand can use only bounded real RCS before hull rotation.

`buildReferenceAttitudes`
- Assisted now also uses propulsion-aware attitude authoring;
- if RCS alone cannot supply the requested acceleration, reference hull
  cants/flips toward the acceleration enough for aft main participation.

Invariant:
```text
nonzero main engine => main_a dot hull_forward >= 0
```

## Regression gate

Exact new E2E:
`ASSISTED / EXPERT / STANDARD, 20.90 -> 20.00 m/s`.

It requires:
- physical execution success;
- no hidden reverse main thrust;
- max physical speed <= 35 m/s;
- final position <= 5 m;
- final speed 20 +/- 1.5 m/s.

Existing regressions remain:
- Assisted 10 -> 10 no runaway;
- Newtonian 26.15 -> 11.75 reacquisition.

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

Test first:
`ASSISTED / EXPERT / STANDARD, 20.90 -> 20.00 m/s`.

Observe physical causality:
- if hull has not rotated and RCS is insufficient, strong braking must NOT
  occur;
- rotating hull alone must not alter velocity;
- main/RCS thrust must be visible whenever velocity materially changes;
- a loop must result from integrated forces, not from an independent visual
  curve.

If this still fails, inspect whether the Ruckig point-mass program asks for
main-engine-dominant acceleration before finite hull lead-rotation can be
completed. The next fix would then belong in maneuver timing/authoring, not in
the propulsion allocator or viewer.

Do not enable dynamic avoidance.
Do not loosen tolerances to hide the failure.
