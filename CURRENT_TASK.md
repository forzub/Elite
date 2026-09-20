# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest tested checkout

```
0ad327ad63d3e63f8c204b2e59a6f224f80c8fee
```

Architecture PASS; runtime 17/19.

Failures:
- strengthened local branch regression;
- final composite.

## Diagnosis

The cross-ring search was present, but "same branch" was defined by full direction dot product.

Because all progress candidates retain a large forward component, opposite lateral sides could both look positively aligned.

## Current production candidate

```
b8bcaae6c5af366e8cabcefb86fbe104c120b010
caa8da847b0f48ea2d72d2fa8c042c1d599c73b8
88f63b0d62803d73e7f3694d1c4f30bcbc1925d0
9422dddfab64f70f94773ebcacf2167be1daacbe
e9655a4b9f004fe790adbbc287bd55a3f1c269ea
```

Branch continuity is now based on the lateral component relative to nominal forward.

Selection order with explicit continuity:
1. safe same-branch class;
2. smallest safe deflection ring within that class;
3. strongest transverse alignment inside the ring;
4. strongest full-direction continuity;
5. deterministic azimuth index.

Opposite branch is fallback only when no same-branch safe candidate exists.

## Focused regression

Updated fixture:
- 4 azimuth samples;
- accepted branch = -Z;
- exact-static blocker rejects primary-ring -Z only;
- +Z primary remains safe;
- larger-ring -Z remains safe.

Required:
- larger-ring -Z wins;
- transverse branch diagnostics confirm the branch exists and is selected.

## Target commands

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

## Interpretation

If planner regression fails:
- inspect transverse branch calculation/fixture geometry.

If regression passes and composite has `same_branch_safe > 0` but negative selected alignment:
- ranking bug remains.

If composite has `same_branch_safe == 0` at the failing replan:
- branch switch is legitimate;
- next step is explicit brake/recovery before switching branch, not forcing an impossible no-stop transit.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior lab;
- move to real NAV STRESS/game.

## Iteration rule

After every state/evidence change, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
