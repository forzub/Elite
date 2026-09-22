# CONTINUE PROMPT — Elite Navigation: validate normalized APIs, then implement physical maneuver authoring

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. recreate this `CONTINUE_PROMPT.md` from scratch.

While architecture cleanup remains relevant also synchronize:
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`;
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`.

## Current code baseline before docs

```text
8b7134078fe1e7672e04085254c9f84c29ed1519
```

Target-machine validation has NOT been performed for this cleanup.

## Canonical input ownership

### Vehicle

Navigation receives one generic:

```text
game::navigation::VehicleDynamicsProfile
    ShipParams physics
    bodyHalfExtentsMeters
    capabilityRevision
```

The runtime is vehicle-agnostic.

Viewer/E2E currently choose EliteCobraMk1Descriptor at their application boundary
and call `makeScenarioVehicleParameters(descriptor)`.

Never reintroduce `cobraParams()` inside navigation.

### Derived vehicle limits

Use only:
- `src/game/ship/core/ShipDynamics.h`;
- `src/game/navigation/NavigationVehicleProfileAdapters.h`;
- `src/game/navigation/ManeuverCapabilityAdapters.h`.

Do not locally reinterpret maxLinearGs / RCS / angular rates / reverse thrust.

### Navigation policy

All stand calculation constants now live in
`ScenarioRunSettings::navigation` / `ScenarioNavigationPolicy`.

This includes:
- execution/trace timestep;
- route reserve doctrine;
- RCS trim threshold;
- terminal attitude blend;
- program tolerances;
- tracking envelope/deadbands/reserves;
- tracking-loss invalidation;
- capture/final tolerances.

Public runtime API rejects invalid vehicle/profile policy inputs.

## Clock contract

One physical maneuver -> one monotonic clock.

Fixed-capacity AcceptedManeuverProgram objects are storage pages only.

Every page:
- shares acceptedAtUniverseTimeSeconds;
- has sequenceStartOffsetSeconds;
- is selected transparently by global maneuver time.

Do not use capture/replan semantics at page boundaries.

## Tracking loss

No reference-clock freeze exists.

FreeTransit:
- short tracking error -> bounded correction;
- prolonged error -> `PROGRAM_INVALIDATED_TRACKING_LOSS`;
- production owner should request a new Planner program from measured state.

Never home for tens of seconds to an obsolete sample.

## Attitude authoring

`buildReferenceAttitudes()` now integrates:
- initial omega;
- angular acceleration;
- max angular rate;
- stopping-aware target omega;
- orientation via average omega.

No instantaneous 0 -> max-omega jump is allowed.

## Run target gate next

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh

ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

Then viewer:
```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe \
    tools/navigation_runtime/scenario.json
```

Run Newtonian/Expert/Standard:
1. 10 -> 10 m/s
2. 27.8 -> 26 m/s

Expected architecture diagnostics:
- PROGRAM STORAGE PAGES;
- STORAGE PAGE ADVANCES;
- REFERENCE CLOCK: MONOTONIC;
- no REFERENCE CLOCK HOLD;
- no stop-and-return to a stale reference.

## Next code work after the gate

Move physical maneuver authoring ahead of final timing.

Planner/maneuver compiler must decide and prove:
- smooth geometry / tangent states;
- body attitude schedule;
- angular reachability;
- installed rear/fore main use;
- RCS vector use;
- throttle slew;
- lead-rotation and braking boundaries.

Ruckig is subordinate to this physical plan.

Only after that:
- publish proved ActuatorSegments;
- make Autopilot execute them directly with bounded correction.

Do not tune follower gains to hide planner defects.
Do not enable dynamic avoidance yet.
