# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

## Mandatory workflow rule

After every state-affecting project event — new candidate, target-machine failure/root cause, acceptance, architecture/ownership change, or task change — synchronize:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- directly relevant architecture/migration docs when needed

Then recreate this entire `CONTINUE_PROMPT.md` **from scratch from current truth**.

Never incrementally patch stale continuation-prompt prose.

## Evidence boundary

Last exact accepted target-machine baseline:

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest actually tested checkout:

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

That checkout passed the Stage-12 architecture contract but runtime behavior did
not execute because a test diagnostic failed to compile on missing
`<iomanip>`. It predates the current B4 replacement and gives no behavioral
evidence for it.

Current unverified code baseline before documentation/state-sync commits:

```
edb4c4106686ce625e1cd5eb99a6d1483cd32854
```

The exact HEAD pulled by the user for the next run is the checkout that must be
recorded as tested.

## Current B4 architecture — hard replacement

The old ordinary local-avoidance mechanism is physically removed.

Do not restore:
- angular deflection rings;
- 15/30/45/60/75-degree fan widening;
- azimuth fan search;
- branch-continuity hints;
- same-branch ranking;
- branch-switch-required API;
- ordinary Brake-before-changing-side recovery.

The architecture checker must reject reintroduction of those identifiers.

## Canonical unexpected-obstacle mechanism

```text
accepted trajectory
        |
        v
physical visible horizon
        |
        +-- predict relevant moving obstacle P(t), V(t), A(t)
        +-- retain exact-static route/corridor constraints
        |
        v
plane normal to nominal trajectory tangent
        |
        v
project swept dynamic occupancy
        |
        v
search metric lateral/vertical offsets
        +
search longitudinal bypass stations inside the horizon
        |
        v
candidate detour:
    current -> off-route bypass station
            -> on-route merge target
        |
        +-- projected occupancy rejection
        +-- exact-static proof of outbound leg
        +-- exact-static proof of return/merge leg
        +-- time-coupled dynamic proof of whole detour
        |
        v
AdjustedClear temporary bypass
        |
        v
B5/B6 physical maneuver compilation/proof
        |
        v
B7/B8 ACCEPT
        |
        v
B9/B10 -> PilotSkill -> propulsion/physics
        |
        v
reacquire original trajectory
```

A full stop is not the normal side-change algorithm.

`LocalBypassExhausted` means no safe bounded detour was demonstrated in the
current horizon. Higher ownership may then reduce speed, change waypoint/portal
or topology route, backtrack, or invoke emergency behavior.

## Current LocalAvoidancePlanner contract

Policy includes:
- `lateralGridHalfExtentSamples`
- `minimumLateralStepMeters`
- `lateralStepEnvelopeMultiplier`
- `maximumLateralOffsetMeters`
- `longitudinalSamples`
- `projectionPaddingMeters`
- `trajectorySamples`
- `staticAdditionalClearanceMeters`

Result includes:
- `selectedLateralOffsetMap`
- `selectedLateralOffsetMeters`
- `selectedBypassForwardDistanceMeters`
- `mergeTargetMapMeters`
- `selectedProjectedClearanceMeters`
- `projectedDynamicObstacles`
- `offsetCandidatesExamined`
- `routeCandidatesExamined`
- projection/static/dynamic rejection diagnostics
- `localBypassExhausted`

The same relevant visible-horizon actor set is used for projection and final
time-coupled proof.

## Current NavigationRuntimePlanner products

Runtime mirrors:
- `localBypassLateralOffsetMap`
- `localBypassLateralOffsetMeters`
- `localBypassForwardDistanceMeters`
- `localBypassMergeTargetMapMeters`
- `localBypassProjectedClearanceMeters`
- `avoidanceProjectedDynamicObstacles`
- `avoidanceOffsetCandidatesExamined`
- `avoidanceRouteCandidatesExamined`
- projection/static/dynamic rejection diagnostics
- `localBypassExhausted`

No branch-continuity input exists.

## Pre-target compile/API audit already completed

Before asking for the first target run:
- incorrect helper access `query.horizon.policy` was corrected where helper
  owns `LocalHorizonPlanner::Query`;
- outer solver uses `query.horizon.policy` correctly;
- NavigationSpace move semantics verified;
- exact dynamic field `exactObstacles` verified;
- static `SegmentQueryResult::startRegionId` verified;
- `allowEndOnStartRegionBoundary` verified;
- runtime/planner/test fields cross-checked;
- architecture checker requirements cross-checked against real fixtures;
- repository-wide search returned zero matches for removed fan/branch production identifiers.

This does **not** replace the user's MinGW64 target gate.

## Focused regressions expected

1. nominal clear -> no bypass work;
2. crossing moving obstacle -> projected bypass;
3. head-on obstacle with lateral room -> bypass without mandatory stop;
4. exact static blocker -> offset filtering;
5. late exact-static blocker -> outbound leg may be clear but return leg must also
   be proved;
6. dynamic broadphase sphere must not override clear exact dynamic OBB truth;
7. obstacle disappears -> nominal trajectory reacquisition;
8. no fitting offset -> `localBypassExhausted`;
9. stale dynamic truth -> fail closed;
10. runtime planner publishes lateral offset + longitudinal station + merge
    target;
11. composite replans from fresh world truth without branch state.

## Target-machine validation

Run:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

OUT="navigation_test_$(date +%Y%m%d-%H%M%S).txt"

{
    echo "===== TESTED HEAD ====="
    git rev-parse HEAD

    echo
    echo "===== ARCHITECTURE CONTRACT ====="
    python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

    echo
    echo "===== NAVIGATION RUNTIME ====="
    bash tests/navigation_runtime/run_mingw64.sh
} 2>&1 | tee "$OUT"

echo
echo "===== VISIBLE-HORIZON / COMPOSITE SUMMARY ====="
grep -E 'VISIBLE-HORIZON|LOCAL VISIBLE-HORIZON|\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|NAVIGATION RUNTIME PLANNER TESTS|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## How to handle the result

### Compile failure
Patch the exact type/include/API problem only. Do not add compatibility shims for
the removed fan/branch API.

### Focused behavior failure
Inspect:
- actor working set;
- normal-plane basis;
- metric offset spacing;
- longitudinal bypass station;
- outbound static proof;
- return-leg static proof;
- time-coupled dynamic proof;
- actual fixture free space.

Do not weaken clearance blindly.

### Composite failure
Inspect every:
- `[COMPOSITE-PLAN]`
- `[COMPOSITE-REPLACEMENT]`
- `[COMPOSITE-REPLACEMENT-ACTUAL]`
- `[COMPOSITE-RESUME]`
- `[COMPOSITE-CONTINUATION]`
- final `[COMPOSITE]`

Planner and executor must use synchronized world truth.

### Green gate
Only then:
1. record exact tested HEAD;
2. record exact Newtonian/Assisted metrics;
3. promote only evidence actually proved;
4. synchronize all required MDs;
5. recreate this prompt from scratch;
6. move toward actual NAV STRESS/game visual evaluation instead of indefinite
   unrelated synthetic matrices.

## Non-negotiable ownership

- Planner owns route/corridor, physical reference, proof and accepted program.
- Follower tracks accepted program with bounded residual feedback.
- Follower does not invent a new maneuver.
- Stable execution does not replan every frame.
- Manual guidance consumes the same accepted navigation product.
- Static and dynamic ownership remain separate.
- Removed angular-fan/branch-recovery local logic must remain absent.

## Mandatory workflow rule — repeat

After every state-affecting event, synchronize project state MDs and active
Stage-12, then recreate this entire `CONTINUE_PROMPT.md` **from scratch**.

Never incrementally patch stale continuation-prompt prose.
