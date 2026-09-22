# CURRENT TASK — Fix stale E2E contract, then physical maneuver timing

Date: 2026-09-22

Status: **TARGET BUILD PASS / STAGE-2 PHYSICAL AUTHORING FAIL**

Target-tested baseline:

```text
5cc0b668665f0adc11b160bad3bc2af314cdfe4d
```

## Target evidence

Passed:
- architecture contract;
- Stage-1 nominal route;
- follower corridor test;
- viewer/runtime build.

Failed:
- `navigation_runtime_pipeline`.

High-speed Newtonian result:
```text
trajectory samples:             743
storage pages:                   51
actuator segments:              742
infeasible actuator segments:    42
storage page advances:            1
tracking invalidated at:       0.51 s
final error:                 287.19 m
reference/velocity angle:     175.84 deg
body/velocity angle:            0.94 deg
reference clock:              MONOTONIC
```

## Immediate test defect

`testHighSpeedRunReacquiresInsteadOfOutrunningReference()` still requires:

```text
REFERENCE CLOCK HOLD:
```

That mechanism was intentionally removed.

Update regressions to require:
- `REFERENCE CLOCK: MONOTONIC`;
- no hold diagnostics;
- explicit invalidation when the accepted program is materially unreachable.

Do not make invalidation itself a success condition for normal navigation.

## Real production defect

The accepted translational trajectory is generated before full attitude/thrust
feasibility is known.

Current sequence:

```text
route
 -> guide
 -> scalar Ruckig trajectory
 -> attitude author
 -> actuator fit
```

This allows Ruckig to demand braking/turn acceleration at a time when the hull
cannot yet point installed main thrust in the required direction.

The angular stream is now individually reachable, but the translational stream
does not wait for it. Result: actuator infeasibility and tracking invalidation.

## Required next implementation

1. Remove stale REFERENCE CLOCK HOLD assertions from E2E tests.
2. Make actuator infeasibility a hard authoring failure, not observe-only data.
3. Introduce physical maneuver compilation before final timing:
   - tangent/geometry state;
   - required force vector;
   - hull attitude schedule;
   - angular reachability;
   - installed rear/fore main;
   - RCS allocation;
   - throttle slew;
   - lead-rotation / braking boundary.
4. Use Ruckig only after these constraints are known.
5. Keep 0.50 s tracking invalidation as a safety guard; do not widen it to mask the planner defect.

Dynamic avoidance remains disabled.
