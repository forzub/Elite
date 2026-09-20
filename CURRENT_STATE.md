# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

This remains the last exact accepted target-machine baseline. Do not promote a
new baseline without fresh target-machine evidence.

## Latest actually tested checkout

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

Observed:
- Stage-12 architecture contract PASS;
- runtime behavior did **not** execute;
- compilation stopped in `NavigationRuntimePlannerTests.cpp` on a missing
  `<iomanip>` include for diagnostic `std::setprecision`.

Therefore this checkout provides no acceptance evidence for the current local
avoidance mechanism.

## Current unverified repository baseline before mandatory state-doc sync

```
f8cd91d01006d9cba8327ae51efb6705e6df83e0
```

This baseline contains the hard B4 replacement and canonical Stage-12 migration
record. The state-doc synchronization commits come after it and do not change
navigation behavior.

## Current B4 production mechanism

The former angular deflection / azimuth fan and branch-continuity mechanism are
removed from production rather than retained as fallback code.

Removed:
- 15/30/45/60/75-degree angular fan search;
- `primaryDeflectionRadians` / `secondaryDeflectionRadians` /
  `maximumDeflectionRadians`;
- `azimuthSamples`;
- `selectedVisibilityDeflectionRadians`;
- `ordinaryVisibilitySearchExhausted`;
- accepted local branch continuity hints;
- same-branch ranking state;
- `avoidanceBranchSwitchRequired`;
- ordinary Brake-before-branch-switch recovery;
- composite `[COMPOSITE-RECOVERY]` path.

Canonical ordinary unexpected-obstacle path:

```text
accepted route / trajectory
    -> physical visible horizon
    -> predict relevant dynamic occupancy
    -> project swept occupancy onto plane normal to nominal trajectory
    -> search bounded lateral/vertical offsets in meters
    -> exact-static proof
    -> time-coupled dynamic proof
    -> temporary bypass target
    -> merge target on original trajectory
    -> downstream B5/B6 physical maneuver compilation/proof
    -> execute and reacquire nominal trajectory
```

`LocalBypassExhausted` means no safe bounded local offset was demonstrated.
Only then may higher ownership slow further, change waypoint/portal/route, or
select emergency/recovery behavior. Full stop is not the normal side-change
algorithm.

## Hard-removal evidence already established in repository audit

Repository-wide searches returned zero production/test matches for deleted
identifiers including:
- `primaryDeflectionRadians`;
- `secondaryDeflectionRadians`;
- `selectedVisibilityDeflectionRadians`;
- `ordinaryVisibilitySearchExhausted`;
- `localAvoidanceContinuityValid`;
- `preferredDirectionValid`;
- `sameBranchSafeCandidates`;
- `branchSwitchRequired`;
- `avoidanceBranchSwitchRequired`;
- `fitBranchSwitchRecovery`;
- `branchRecoveryPhases`.

The Stage-12 architecture checker now positively requires the projected
visible-horizon API and negatively rejects reintroduction of old fan/branch
identifiers.

## Validation status

**UNVERIFIED on target machine.**

No claim is made yet that the replacement compiles or passes behavior on the
user's MinGW64 target.

Next evidence must establish:
1. exact tested HEAD;
2. Stage-12 architecture PASS;
3. navigation runtime compile and execution;
4. projected-bypass focused regressions;
5. final composite Newtonian + Assisted behavior.

## Next expected gate

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

## Documentation protocol

After every state-affecting project event:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update the active Stage-12 document;
- recreate `CONTINUE_PROMPT.md` **from scratch** from current truth.

Never incrementally patch stale continuation-prompt prose.
