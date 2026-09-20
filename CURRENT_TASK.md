# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest tested final-composite checkout

```
852e5a71a71625cdfc0c71a6bb89724d2194990e
```

Architecture PASS, runtime 18/19.

The production planner and first authority-bounded replacement both succeeded.

Key evidence:
- `AdjustedClear`;
- nominal conflict witness = 1;
- 20 probes;
- replacement planned clearance 3.181 m;
- actual clearance 3.175 m;
- tracking exceeded = 0;
- final replacement P error 1.31 cm.

## Root cause of latest failure

After that successful bypass, the test resumed planning with `emptyDynamic()`.

The hazard was still physically alive, so later execution measured clearance against an obstacle that the planner had been told no longer existed.

This is invalid world-state synchronization.

## Current candidate

```
6c0a71d308040c568c109afc4425332021ade730
01a8d69cc635a450d73a49a31b91e5546e0b1828
```

Persistent-hazard behavior:
1. publish current hazard position/velocity to NavigationMap;
2. run production NavigationRuntimePlanner;
3. if `AdjustedClear`, author the shortest authority-bounded replacement and execute it;
4. republish hazard at the new current time/state;
5. repeat, bounded to at most four additional continuations;
6. resume the narrow static portal only when planner returns `NominalClear`.

No hazard is silently deleted.

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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Exit

If 19/19:
- record exact target checkout and final composite metrics;
- close synthetic behavior laboratory;
- next task = real NAV STRESS/game corridor + physical trajectory visualization.

If red:
- diagnose from resume/continuation rows;
- do not erase hazards or weaken clearance/tracking criteria.

## Iteration rule

After every state/evidence change, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
