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
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

User target-machine evidence from `navigation_test_20260920-165856.txt`:
- Stage-12 architecture contract PASS;
- runtime compiled and linked;
- 17/19 runtime tests PASS (89%);
- `navigation_runtime_planner` FAIL;
- `navigation_composite_proving_ground` FAIL.

Therefore the hard-replaced B4 projected visible-horizon solver is **NOT ACCEPTED** yet.

## Current B4 mechanism

```text
accepted trajectory
    -> physical visible horizon
    -> predicted dynamic occupancy
    -> trajectory-normal projection
    -> metric lateral/vertical offset search
    -> longitudinal bypass station search
    -> exact-static proof current -> bypass
    -> exact-static proof bypass -> merge
    -> time-coupled dynamic proof of the complete two-segment detour
    -> temporary bypass target
    -> merge target on the accepted trajectory
    -> physical maneuver compilation/proof
    -> execute and reacquire
```

The removed angular fan / branch-continuity / branch-switch Brake path remains forbidden.

## Failure audit: focused runtime fixture

`testAdjustedVisibilityDoesNotInheritFuturePortalAlignment()` currently asks for
`AdjustedClear` with:
- agent start X = 2.0 m;
- blocker X = 4.5 m, radius = 0.75 m;
- portal staging / merge X = 7.0 m;
- agent radius = 1.0 m;
- horizon safety margin = 0.0 m;
- projection padding = 1.0 m.

The full dynamic proof requires:

```text
1.0 + 0.75 + 0.0 + 1.0 = 2.75 m
```

but both start->blocker and merge->blocker centre distances are only 2.5 m.
`timeCoupledBypassClear()` samples both endpoints and uses the same 2.75 m
requirement. Therefore no two-segment candidate can satisfy the current
contract. The fixture expectation is stale/inconsistent with the new solver.

## Failure audit: final composite

Logged first Newtonian replan:

```text
position      = (138.841366, 52.623525, 0)
hazard        = (185.850434, 42.920559, 0)
merge_target  = (168.222033, 46.559171, 0)
nominal_dynamic_conflicts = 1
projected_obstacles = 1
offset_candidates = 168
route_candidates = 168
localBypassExhausted = 1
nominal_static_blocked = 0
```

Geometry from that row:
- current -> hazard = 48.0 m;
- current -> merge = 30.0 m;
- merge -> hazard at activation = 18.0 m.

Composite required dynamic clearance is:

```text
hull bounding radius 17.275995
+ hazard radius       6.000000
+ safety margin       2.000000
+ projection padding  1.500000
=                     26.775995 m
```

At the final time-coupled sample (`t = 4 s`) the hazard has moved only 2 m in
Y; merge-to-hazard distance is still about 18.51 m, below 26.78 m.
Therefore every candidate forced to return to this merge target must fail the
dynamic proof regardless of its lateral bypass offset.

This explains the `ConflictHold + localBypassExhausted` result directly.

## Important design question exposed by the test

The focused fixture is clearly invalid and must be repaired without weakening
clearance. The composite also exposes a possible real B4 limitation:
`LocalAvoidancePlanner` hard-wires the merge point to the current bounded
nominal target. If the inflated obstacle occupancy extends beyond that point,
the solver has no legal way to remain temporarily off-route and merge later.

Before changing production behavior, distinguish:
1. invalid synthetic fixture geometry;
2. insufficient physical horizon / merge-station selection;
3. need for a multi-horizon off-route continuation with later reacquisition.

## Next evidence gate

1. Add rejection counters to the first failing focused/composite diagnostics
   (`projectionRejected`, `staticRejected`, `dynamicRejected`).
2. Repair the focused portal-attitude fixture geometry so both endpoints are
   outside required clearance while the nominal path is genuinely blocked.
3. Add a deterministic regression that proves whether the current fixed merge
   point itself causes exhaustion.
4. Decide whether B4 needs downstream merge sampling / multi-horizon
   continuation instead of changing safety thresholds.
5. Re-run the exact target-machine architecture + 19-test runtime gate.

## Documentation protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
