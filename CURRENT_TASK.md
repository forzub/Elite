# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest tested checkout

```
51e6c41bb94b65e8cc269fb035164a4eb0aa23fd
```

Architecture PASS; runtime 18/19.

The focused explicit-continuity planner regression passed.

Only final composite failed.

## Failure

At a persistent-hazard replan:
```
accepted continuity ~ (0.36,-0.35,+0.87)
selected target direction ~ opposite -Z
```

The physical author then correctly refused to build an impossible no-stop continuation.

## Root cause

Continuity ranking was only applied inside the currently tested deflection ring.

The planner still returned the first ring containing any safe candidate, so:
```
smaller-angle opposite branch
```
could beat:
```
slightly larger-angle accepted branch
```

## Current candidate

Production:
```
e19c1804806ce5f3554f20c7b7d3d5ac19b4911e
c3dcf98b16bd6e45f0dbc949ec926f086aa623a0
```

Regression/diagnostics:
```
06a917f058926a29f41a936dd994be3f8073e7cf
e62328d4af99b6e452e2d07e53cd20e25a103dcf
a2da6453daaa8e00cc1f4661321b9294513057ff
```

## New branch-selection rule

With explicit accepted continuity:
1. scan all safe candidates in all ordinary rings up to maximum deflection;
2. same-branch safe candidates outrank opposite-branch candidates;
3. maximize alignment with accepted direction;
4. smaller deflection breaks equal-alignment ties;
5. azimuth index is deterministic final tie-break.

If no same-branch safe candidate exists, least-opposed safe fallback is allowed.

Without explicit continuity, smallest-safe-ring behavior is unchanged.

## Strong regression

The focused test now blocks preferred -Z only on the first ring while +Z remains safe there.

A larger-ring -Z candidate is safe.

Planner must deliberately take the larger ring and keep the accepted branch.

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

## Exit

If 19/19:
- accept exact target checkout and composite metrics;
- close synthetic behavior lab;
- move to real NAV STRESS/game corridor + physical-trajectory visualization.

If red:
- use selected deflection + continuity diagnostics;
- do not weaken physical or safety criteria.

## Iteration rule

After every state/evidence change, synchronize all MDs and recreate `CONTINUE_PROMPT.md` from scratch.
