# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest target result

Tested:
```
af9ee9d1694ac0facafaf23b0ec51c3adaf7dbbf
```

Architecture PASS. Runtime 17/19.

## Branch-switch recovery is working

Composite evidence:
```
branch_switch_required=1
recovery duration=4.0 s
stopping distance=6.757088 m
peak brake FF=1.266954 m/s2
planned static clearance=39.154319 m
planned dynamic clearance=9.803750 m
actual dynamic clearance=9.817623 m
tracking exceeded=0
final speed error=0.009128 m/s
```

The old branch is then cleared and production planner replans from the recovered state.

## Remaining composite failure

Fresh post-recovery AdjustedClear is produced, but the test-side replacement author rejects it.

Root cause:
```
minimumPlannedSpeed >= 0.50 m/s
```
was applied at t=0 even after intentional recovery to ~0.009 m/s.

## Current unverified authoring fix

```
7c87e655788af4e95f9576675185482639fec528
```

For normal moving transits:
- unchanged: minimum planned speed >=0.50 m/s.

For post-recovery launch:
- start below 0.50 is legal;
- no reverse progress below -0.05 m/s;
- must reach >=0.50 m/s;
- must never drop below 0.50 after reaching it;
- transverse FF <=1.35 m/s2;
- dynamic planned clearance >=1.50 m.

## Focused regression fixture fix

```
252f9d81c5fd91363a0e0561e195c1f5ab0d375d
```

Old blocker depended on a guessed 600 m horizon endpoint.

New blocker lies at:
```
300 * primary-ray direction
```
inside the primary -Z segment.

Therefore exact physical-horizon endpoint changes cannot make the blocker miss the primary ray.

New diagnostic:
```
[BRANCH-REGRESSION]
```

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

## Next interpretation

If focused branch regression is green, the branch semantics are settled.

If post-recovery launch is green, recovery -> fresh branch execution is settled.

If launch still fails, instrument the fitter rejection reasons before changing thresholds.

If 19/19:
- record exact target checkout + final metrics;
- close synthetic maneuver behavior laboratory;
- move immediately to real NAV STRESS/game accepted-corridor + trajectory visualization.

Do not weaken physical or safety criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
