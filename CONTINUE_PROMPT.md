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
0ad327ad63d3e63f8c204b2e59a6f224f80c8fee
```

Architecture PASS. Runtime 17/19.

Failures:
- strengthened planner continuity regression;
- final composite.

The explicit continuity hint is reaching production, but branch classification was wrong.

## Root cause

Using full direction dot product to define "same avoidance branch" is invalid because all ordinary rays contain a strong forward component.

Opposite lateral bypasses can therefore both produce positive full-direction dot.

## Current unverified production fix

Commits:
```
b8bcaae6c5af366e8cabcefb86fbe104c120b010
caa8da847b0f48ea2d72d2fa8c042c1d599c73b8
88f63b0d62803d73e7f3694d1c4f30bcbc1925d0
9422dddfab64f70f94773ebcacf2167be1daacbe
e9655a4b9f004fe790adbbc287bd55a3f1c269ea
```

Branch classification:
```
accepted_lateral =
    accepted_direction -
    nominal_forward * dot(accepted_direction, nominal_forward)

candidate_lateral =
    candidate_direction -
    nominal_forward * dot(candidate_direction, nominal_forward)
```

If both lateral components are meaningful:
```
branch_alignment =
    dot(normalize(candidate_lateral),
        normalize(accepted_lateral))
```

Positive alignment = same branch.

## Ranking

With explicit branch continuity:
1. safe same-branch class;
2. smallest safe deflection ring in that class;
3. highest transverse branch alignment within that ring;
4. highest full-direction continuity;
5. deterministic azimuth index.

If no same-branch safe candidate exists, use the safest least-opposed fallback.

Without explicit branch continuity, legacy minimum-safe-ring behavior remains.

## New diagnostics

Runtime planner now surfaces:
```
avoidanceContinuityHintUsed
avoidanceContinuityLateralValid
avoidanceSameBranchSafeCandidates
avoidanceSelectedBranchAlignment
```

Composite resume rows print these values.

## Focused regression

Commit:
```
0e344f3a3b2a5e887cbb474ce9ce650b68851716
```

The regression now uses 4 azimuth samples so first-ring preferred -Z is one deterministic ray.

Exact-static blocker rejects that ray only.

Larger-ring -Z remains clear.

Assertions require:
- explicit lateral continuity active;
- at least one same-branch safe candidate;
- selected branch alignment >0.5;
- selected deflection > primary ring;
- selected target remains -Z.

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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|NAVIGATION RUNTIME PLANNER TESTS|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Next interpretation

If regression passes and composite shows:
```
same_branch_safe > 0
selected_branch_alignment > 0
```
then branch continuity is functioning.

If composite failure shows:
```
same_branch_safe == 0
```
then switching branch is physically required; do not force continuity. Add explicit recovery/brake/stop-turn-go before the new branch.

If 19/19:
- accept final composite;
- close synthetic behavior testing;
- move into real NAV STRESS/game.

Do not weaken physical or safety thresholds.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
