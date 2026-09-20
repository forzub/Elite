# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest actually tested checkout

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

That checkout passed the architecture contract but runtime behavior did not
execute because a test diagnostic failed to compile on missing `<iomanip>`.
It predates the current B4 replacement and supplies no acceptance evidence for
the new mechanism.

## Current unverified code baseline before mandatory state-sync commits

```
edb4c4106686ce625e1cd5eb99a6d1483cd32854
```

This baseline is the first coherent candidate of the hard-replaced
trajectory-relative visible-horizon local solver.

## Canonical B4 mechanism

Unexpected local obstacle handling is now:

```text
accepted trajectory
    -> physical visible horizon
    -> predict relevant moving occupancy
    -> project occupancy onto plane normal to trajectory
    -> search metric lateral/vertical offsets
    -> search longitudinal bypass stations inside the horizon
    -> prove current -> bypass station
    -> prove bypass station -> merge point on original trajectory
    -> time-coupled dynamic proof of the whole two-segment detour
    -> publish bypass target + merge target
    -> downstream physical maneuver compilation/proof
    -> execute
    -> reacquire original trajectory
```

The solver now searches a **real detour**, not a single off-route endpoint.

New canonical fields include:
- `longitudinalSamples`;
- `selectedBypassForwardDistanceMeters`;
- `routeCandidatesExamined`;
- runtime mirrors `localBypassForwardDistanceMeters` and
  `avoidanceRouteCandidatesExamined`.

## Hard-removed legacy mechanism

The following must remain physically absent:
- 15/30/45/60/75-degree fan;
- angular deflection rings;
- azimuth fan;
- branch continuity hints;
- same-branch ranking;
- branch-switch-required API;
- ordinary Brake-before-changing-side recovery.

Repo-wide symbol audit returned zero matches for the removed production
identifiers.

The architecture checker positively requires the projected two-segment solver
and negatively rejects old fan/branch identifiers.

## Compile/API audit before target run

Found and fixed before target execution:
- helper access incorrectly used `query.horizon.policy` where the helper owns
  a `LocalHorizonPlanner::Query`; corrected to `query.policy`;
- outer solver uses `query.horizon.policy` correctly;
- `NavigationSpace` is movable;
- dynamic exact geometry field is `exactObstacles`;
- static segment result provides `startRegionId`;
- portal-boundary endpoint exception exists as
  `allowEndOnStartRegionBoundary`;
- runtime result fields, avoidance policy fields and live diagnostics all match
  their public headers.

Checker/API/test audit reports no missing required fields or fixtures.

## Focused tests now expected

- nominal clear -> no search;
- crossing moving obstacle -> projected bypass;
- head-on with free space -> bypass without mandatory stop;
- exact static blocker constrains offsets;
- exact static proof covers the **return leg** to the original trajectory;
- obstacle disappears -> direct trajectory reacquisition;
- no fitting offset -> `localBypassExhausted`;
- stale dynamic truth -> fail closed;
- runtime planner publishes metric offset, longitudinal bypass station and merge
  point;
- composite repeats fresh projected bypasses with no branch state.

## Validation status

**UNVERIFIED on target MinGW64.**

No acceptance claim until the user's target-machine gate passes.

## Required next gate

Run architecture + navigation runtime on the exact pulled HEAD. Record the
tested SHA and all visible-horizon/composite failures or metrics.

## Documentation protocol

After every state-affecting event:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
