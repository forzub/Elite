# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest target-machine attempt

Tested checkout:
```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

Architecture contract PASS.

Runtime behavior was not tested because compilation failed in:
```
tests/navigation_runtime/NavigationRuntimePlannerTests.cpp
```

Error:
```
std::setprecision is not a member of std
```

Root cause:
- `[BRANCH-REGRESSION]` diagnostic added `std::setprecision`;
- source lacked `#include <iomanip>`.

Compile-only fix:
```
cfa56734020b41875354262302b9be51684413be
```

No navigation semantics or thresholds changed.

## Current behavior problem

We are proving a physically correct local-branch transition:

```
accepted moving branch
 -> branch becomes unavailable
 -> avoidanceBranchSwitchRequired
 -> Brake recovery
 -> near-zero speed
 -> old continuity cleared
 -> fresh production replan
 -> bounded launch into newly safe branch
```

Already demonstrated before this build-only failure:
- branch-switch escalation;
- real Brake execution;
- >9.8 m dynamic clearance;
- zero tracking-envelope violations;
- ~0.009 m/s final speed error.

Current unverified candidate also contains:
- horizon-independent focused branch regression blocker;
- post-recovery launch support from near-zero speed.

## Validation

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
echo "===== FINAL COMPOSITE SUMMARY ====="
grep -E '\[BRANCH-REGRESSION\]|\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE-RECOVERY\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|NAVIGATION RUNTIME PLANNER TESTS|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Interpretation

If build succeeds, only then evaluate mechanism behavior.

If focused regression fails:
- use `[BRANCH-REGRESSION]` diagnostics.

If post-recovery launch fails:
- instrument fitter rejection reasons before changing thresholds.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior lab;
- move to real NAV STRESS/game.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
