# CURRENT TASK — Target-validate normalized APIs, then move physical maneuver proof ahead of Ruckig timing

Date: 2026-09-22

Status: **API/SSOT/CLOCK CLEANUP IMPLEMENTED / TARGET VALIDATION REQUIRED**

Code baseline before documentation commits:

```text
8b7134078fe1e7672e04085254c9f84c29ed1519
```

## Completed architecture cleanup

### Vehicle data

Runtime accepts one generic explicit input:

```text
VehicleDynamicsProfile
    physics: ShipParams
    bodyHalfExtentsMeters
    capabilityRevision
```

Concrete Cobra selection exists only at the viewer/E2E application boundary.
Navigation runtime itself contains no `cobraParams()`.

Vehicle capability and world-map revision are separate domains.

### Common derived limits

All navigation/physics code must derive vehicle limits through common helpers:

```text
ShipDynamics.h
NavigationVehicleProfileAdapters.h
ManeuverCapabilityAdapters.h
```

Do not reintroduce raw local interpretations of:
- maxLinearGs;
- manoeuvreThrusterAccel;
- pitch/yaw/roll limits;
- reverse-main capability;
- vehicle collision size.

### Explicit calculation policy

`ScenarioNavigationPolicy` owns the stand's calculation policy and crosses the
API explicitly through `ScenarioRunSettings`.

No hidden runtime timestep or tracking-loss timeout remains.

### Clock

One authored maneuver has one monotonic clock.

Fixed-capacity AcceptedManeuverProgram objects are storage pages:
- shared acceptedAt time;
- explicit sequenceStartOffsetSeconds;
- page advance is indexing only;
- no phase capture at page boundaries.

### Tracking loss

No infinite reference freeze.

If FreeTransit is outside its proved tracking envelope longer than the explicit
policy timeout, the program becomes invalid:
`PROGRAM_INVALIDATED_TRACKING_LOSS`.

Production ownership after invalidation is:
Autopilot safety response -> Planner re-author from current measured state.

### Attitude

Reference attitude is now angular-acceleration reachable. It no longer jumps from
zero omega directly to max angular rate.

## Target gate now

Run:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh

ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

Then run viewer:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe \
    tools/navigation_runtime/scenario.json
```

First target cases:
1. NEWTONIAN / EXPERT / STANDARD 10 -> 10 m/s
2. same 27.8 -> 26 m/s

Expected diagnostics:
- PROGRAM STORAGE PAGES, not PROGRAM PHASES;
- STORAGE PAGE ADVANCES > 0 for long trajectories;
- REFERENCE CLOCK: MONOTONIC;
- no REFERENCE CLOCK HOLD lines;
- no return-to-stale-point behavior;
- if tracking becomes materially unreachable, explicit
  PROGRAM_INVALIDATED_TRACKING_LOSS.

## Next architecture step after compile/E2E gate

The remaining major RED area is physical maneuver authoring.

Current order is still approximately:

```text
coarse route
 -> geometric execution guide
 -> scalar Ruckig timing
 -> attitude / propulsion compilation
```

Target order:

```text
coarse route
 -> physical maneuver compiler
    geometry / tangents
    hull attitude reachability
    installed main + RCS allocation
    throttle slew
    braking / lead-rotation boundary
    proof against VehicleDynamicsProfile
 -> Ruckig as subordinate timing/state-transition helper
 -> Accepted ManeuverProgram
 -> Autopilot executes explicit ActuatorSegments
```

Do not tune follower gains or tracking envelopes before the target gate.
Do not enable dynamic avoidance yet.
