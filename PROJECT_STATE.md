# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Evidence boundary

Accepted baseline:
```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest actually tested checkout:
```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

Current unverified two-segment visible-horizon baseline before state-doc sync:
```
edb4c4106686ce625e1cd5eb99a6d1483cd32854
```

## B4 current architecture

B4 ordinary unexpected-obstacle handling is now one trajectory-relative owner:

`LocalHorizonPlanner + LocalAvoidancePlanner`.

The local solver:
- uses the accepted trajectory as reference;
- predicts moving occupancy over the physical horizon;
- projects occupancy into the normal plane;
- searches offsets in meters, not angles;
- samples several longitudinal bypass stations;
- proves both outbound and return-to-route exact-static segments;
- time-checks the whole two-segment detour against relevant moving actors;
- publishes a temporary bypass target and an explicit merge point on the
  original trajectory.

This is the sole ordinary local path.

A future longitudinal multi-slab corridor may extend the same B4 owner for
complex known geometry. It must not become a second planner.

## Removed path

The angular ray fan / branch-continuity / branch-switch-recovery mechanism is
historical only and must remain absent.

The architecture checker enforces this negatively.

## Other block status

Accepted/strong:
- B0 snapshot ownership;
- B7 maneuver decision mechanics;
- B8 accepted program;
- B9 sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 propulsion/physics;
- B14 scheduler mechanics.

Still incomplete/transitional:
- B1 shared influence builder;
- B2 unified objective;
- B3 coarse vehicle-aware topology feasibility;
- B5 general production maneuver compiler;
- B6 generalized continuous proof;
- B11 explicit safety/reflex monitor;
- ordinary-live migration of accepted-program execution.

## Current milestone

First target-machine validation of the hard B4 replacement.

Do not call the new solver accepted until:
- architecture passes;
- runtime compiles;
- focused projected-bypass tests execute;
- composite behavior supplies acceptable evidence.

## State protocol

After every state/evidence change synchronize all project MDs, active Stage-12,
and recreate `CONTINUE_PROMPT.md` from scratch.
