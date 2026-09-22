# CONTINUE PROMPT — Elite Navigation: Newtonian main-engine-dominant execution

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

When propulsion/control-law ownership changes, also synchronize
`src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`.

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

## Current code candidate

Before documentation-only commits:

```text
421ecb1b2721d54bc0c33737a520100a3c10fac9
```

Target-machine PASS has NOT been established.

## Latest target evidence

```text
NEWTONIAN / EXPERT / STANDARD
START 21.20 M/S
FINISH 21.20 M/S

PLANNER: CACHED ROUTE OK
ROUTE ADDITIONAL CLEARANCE: 17.71 M
TRAJECTORY: RUCKIG OK
CALCULATED MIN/MAX: 20.73 / 21.20 M/S
PROGRAM PHASES COMPLETE: NO
PHYSICAL TERMINAL STATE: MISSED
FINAL POSITION ERROR: 181.42 M
FINAL SPEED: 8.17 M/S
REFERENCE CLOCK HOLD: 40.61 S
MAX BODY/VELOCITY ANGLE: 174.31 DEG
```

Planner direction is reasonable: speed increase widened the detour.

## Root cause

Newtonian attitude authoring treated the complete physical
`manoeuvreThrusterAccel=2.0 m/s^2` as ordinary route propulsion. The Ruckig
curve usually requested ~1.5 m/s^2, so the hull stayed aligned with the travel
reference and RCS supplied the whole material delta-v.

Telemetry examples show main OFF while RCS ~1.5 m/s^2 continuously reduces
speed even though reference speed remains ~20.6 m/s. Main appears only very
late with body/velocity already ~174 deg apart.

This is why Newtonian looked Assisted.

## Current correction

`propulsionReferenceForward(..., law, ...)` now separates doctrine:

- Assisted: may use the full real RCS envelope when deciding whether to stay
  velocity/tangent coupled.
- Newtonian: only the existing 0.35 m/s^2 tiny-correction authority counts for
  attitude authoring.
- Material Newtonian acceleration therefore authors a real hull cant/flip so
  the aft main engine can participate.
- The full RCS hardware limit remains physically available downstream for
  transient recovery and trim.

Navigation main engine remains aft-only.

## Viewer propulsion lamps

Always visible at screen bottom:

```text
МАРШЕВЫЙ
ПЕРЕДНИЙ МАРШЕВЫЙ
МАНЕВРОВЫЙ
```

They are driven from actual physical acceleration channels:
- aft main projection > threshold -> `МАРШЕВЫЙ`;
- main projection < -threshold -> `ПЕРЕДНИЙ МАРШЕВЫЙ`;
- non-zero manoeuvre acceleration -> `МАНЕВРОВЫЙ`.

Current Cobra has no fore main engine. Therefore `ПЕРЕДНИЙ МАРШЕВЫЙ` is
expected to stay dark and acts as a regression detector.

## Regression

New exact fixture:
`NEWTONIAN / EXPERT / STANDARD, 21.20 -> 21.20 m/s`.

`testNewtonianHigherSpeedUsesMainEngineDominantManeuver()` requires:
- material main-engine acceleration within first 8 seconds;
- no negative/fore main acceleration.

Existing Assisted 10->10, Assisted 20.9->20 and Newtonian 26.15->11.75
regressions remain.

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

First run:
`NEWTONIAN / EXPERT / STANDARD, 21.20 -> 21.20 m/s`.

Observe:
1. `МАНЕВРОВЫЙ` may blink/use trim, but must not be the only material
   propulsion for tens of seconds.
2. Hull must visibly lead-rotate/cant for material route delta-v.
3. `МАРШЕВЫЙ` should light during the burn.
4. Hull rotation without thrust must not bend V.
5. `ПЕРЕДНИЙ МАРШЕВЫЙ` must remain dark.

If those are true but path tracking still fails, the next architectural work is
finite lead-rotation time/distance in maneuver authoring: Ruckig point-mass
acceleration currently starts before the physical hull necessarily acquires its
burn attitude.

Do not enable dynamic avoidance.
Do not loosen tracking/terminal tolerances to fake a pass.
