# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

## Mandatory workflow rule

After **every state-affecting project event** — new code candidate, failed
target-machine gate/root cause, acceptance, architecture/ownership change, or
change of current task/next step — synchronize:

- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- directly relevant architecture/migration docs when needed

Then recreate this entire `CONTINUE_PROMPT.md` **from scratch from current
truth**.

Do **not** incrementally patch stale continuation-prompt prose.

Every recreated continuation prompt must repeat this recreate-from-scratch rule.

## Evidence boundary

### Last exact accepted target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

Do not promote another accepted baseline without fresh target-machine evidence.

### Latest actually tested checkout

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

Observed:
- Stage-12 architecture contract PASS;
- runtime behavior did **not** execute;
- compilation stopped in `NavigationRuntimePlannerTests.cpp` because a
  diagnostic used `std::setprecision` without `<iomanip>`.

This tested checkout predates the current B4 hard replacement and therefore
provides no behavioral evidence for it.

### Current replacement code/doc baseline before mandatory state synchronization

```
f8cd91d01006d9cba8327ae51efb6705e6df83e0
```

The mandatory state-sync commits come after this baseline and do not alter
navigation behavior.

The current branch HEAD after pulling is the exact checkout that must be
recorded by the next target-machine run.

## Current architecture — B4 hard replacement

The former ordinary local-avoidance mechanism is **removed**, not deprecated and
not retained as fallback.

Deleted production/test semantics include:
- angular deflection rings;
- 15/30/45/60/75-degree fan widening;
- azimuth fan search;
- `primaryDeflectionRadians`;
- `secondaryDeflectionRadians`;
- `maximumDeflectionRadians`;
- `azimuthSamples`;
- `selectedVisibilityDeflectionRadians`;
- `ordinaryVisibilitySearchExhausted`;
- accepted local branch-continuity hints;
- same-branch ranking state;
- `avoidanceBranchSwitchRequired`;
- ordinary Brake-before-branch-switch recovery;
- composite `[COMPOSITE-RECOVERY]` path.

Do not restore any of these to make a failing test green.

## Canonical ordinary unexpected-obstacle flow

```text
accepted route / trajectory
        |
        v
physical visible horizon
        |
        +-- predict relevant dynamic obstacle P(t), V(t), A(t)
        +-- retain exact-static corridor constraints
        |
        v
plane normal to nominal trajectory tangent
        |
        v
project predicted swept occupancy
        |
        v
search bounded lateral/vertical offsets in METERS
        |
        +-- projected occupancy rejection
        +-- exact-static segment proof
        +-- time-coupled dynamic proof
        |
        v
temporary bypass target
        +
merge target on original trajectory
        |
        v
B5 physical maneuver compilation
        |
        v
B6 continuous capability/geometry proof
        |
        v
B7/B8 decision + ACCEPT
        |
        v
B9/B10 -> PilotSkill -> propulsion/physics
        |
        v
reacquire original trajectory
```

`LocalBypassExhausted` means only that no safe bounded local offset was
demonstrated in the current physical horizon.

Higher ownership may then:
- reduce speed further;
- select another local waypoint/portal;
- backtrack;
- rebuild topology route;
- invoke emergency/contact-expected behavior.

A full stop is **not** the normal algorithm for selecting another side of an
unexpected obstacle.

## Current implementation ownership

### LocalAvoidancePlanner

Current policy/result concepts:
- `lateralGridHalfExtentSamples`
- `minimumLateralStepMeters`
- `lateralStepEnvelopeMultiplier`
- `maximumLateralOffsetMeters`
- `projectionPaddingMeters`
- `trajectorySamples`
- `selectedLateralOffsetMeters`
- `selectedLateralOffsetMap`
- `mergeTargetMapMeters`
- `selectedProjectedClearanceMeters`
- `projectedDynamicObstacles`
- `offsetCandidatesExamined`
- `projectionRejected`
- `staticRejected`
- `dynamicRejected`
- `localBypassExhausted`

The same relevant visible-horizon actor set must be used by projected occupancy
and final time-coupled bypass proof.

### NavigationRuntimePlanner

Current local-bypass outputs include:
- `localBypassLateralOffsetMap`
- `localBypassLateralOffsetMeters`
- `localBypassMergeTargetMapMeters`
- `localBypassProjectedClearanceMeters`
- projected/offset/static/dynamic rejection diagnostics
- `localBypassExhausted`

No branch-continuity input exists.

### Live NAV STRESS / server evidence

Live diagnostics use visible-horizon terminology and **meters of lateral
offset**, not angular deflection.

The live path must consume the same production planner; no visual-only or
fallback planner is allowed.

## Architecture lock

`tests/architecture_contracts/check_navigation_stage12_runtime_planner.py`
must continue to:
- require the projected visible-horizon API/implementation/tests;
- require live metric-offset integration;
- reject reintroduction of legacy fan/branch identifiers.

A future longitudinal multi-slab route-aligned corridor may extend B4 for
complex known geometry, but it must extend the **same owner** rather than create
a second planner or restore the old angular fan.

## Focused behavior expected from the next target gate

The replacement must prove:

1. clear nominal route -> `NominalClear`, no offset search;
2. crossing moving obstacle -> projected normal-plane bypass;
3. head-on obstacle with genuine lateral room -> bypass without mandatory stop;
4. exact static blocker -> offsets filtered by exact geometry;
5. dynamic broadphase sphere cannot override clear exact dynamic OBB truth;
6. obstacle disappears -> immediate return to nominal trajectory;
7. no fitting local offset -> `localBypassExhausted` / fail closed;
8. stale dynamic truth -> fail closed before offset search;
9. runtime planner publishes bypass offset + merge target;
10. persistent-hazard composite replans from fresh world truth without any
    branch continuity/recovery state.

## Validation command

Run on the user's target machine:

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

## How to interpret the next result

### If compilation fails

Patch the exact compile/API defect only. Do not reintroduce old fan/branch
fields as compatibility shims.

Likely classes of first-run issues:
- type ordering / missing include;
- renamed live diagnostic mismatch;
- architecture checker marker drift;
- test fixture assumptions from the removed API.

After patch:
- synchronize all required MD files;
- recreate this prompt from scratch.

### If a focused projected-bypass regression fails

Inspect:
- projected actor working set;
- normal-plane basis;
- offset spacing / envelope inflation;
- exact-static segment result;
- time-coupled dynamic result;
- whether the fixture actually contains physical free space.

Do not weaken clearance blindly and do not restore angular fan behavior.

### If the composite fails

Inspect each:
- `[COMPOSITE-PLAN]`
- first `[COMPOSITE-REPLACEMENT]`
- `[COMPOSITE-REPLACEMENT-ACTUAL]`
- every `[COMPOSITE-RESUME]`
- every `[COMPOSITE-CONTINUATION]`
- final `[COMPOSITE]`

Important invariant:
planner world truth and executor world truth must stay synchronized across every
bounded continuation.

### If the full gate is green

Only then:
1. record the exact tested checkout;
2. record exact Newtonian and Assisted metrics;
3. promote the replacement to accepted evidence as justified by the gate;
4. synchronize all required project MDs;
5. recreate this prompt from scratch;
6. decide whether the synthetic maneuver behavior lab is closed enough to move
   into actual NAV STRESS/game visual evaluation.

## Next phase after synthetic acceptance

The user's intended next phase is actual game inspection, not indefinite
synthetic-matrix expansion:

- render the accepted route/corridor/tunnel;
- render the accepted physical time trajectory;
- run the real NPC/autopilot in the actual scene;
- visually compare Newtonian and Assisted behavior;
- then vary PilotSkill/control-law profiles as needed.

Do not invent additional synthetic matrices unless live/game behavior reveals a
specific defect that needs a focused regression.

## Non-negotiable ownership rules

- Planner owns route/corridor, maneuver family, physical reference, proof and
  accepted program.
- Follower tracks the accepted program and applies bounded residual feedback.
- Follower does not invent a new maneuver because actuators are weak.
- Stable automatic execution does not replan every frame.
- Replan occurs on meaningful invalidation/completion.
- Navigation does not give up.
- Manual guidance consumes the same accepted navigation product.
- Static world truth remains static ownership; moving actors remain dynamic
  ownership.
- The old angular-fan / branch-recovery local mechanism must remain physically
  absent.

## Mandatory workflow rule — repeat

After every state-affecting project event, synchronize the project state MDs and
active Stage-12 document, then recreate this entire
`CONTINUE_PROMPT.md` **from scratch**.

Never incrementally patch stale continuation-prompt prose.
