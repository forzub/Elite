# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest failed final-composite gate

Tested checkout:

```
d57a22f69c3af1a8c967ce974b895295c77dd21a
```

Architecture PASS, build PASS, runtime 18/19.

All previous 18 tests remained green.

Only failure:

```
navigation_composite_proving_ground
composite production planner did not find adjusted dynamic bypass
```

## Diagnosis

The fixture published the hazard only 35 m ahead after 4 seconds of the already accepted law-specific program.

Production LocalHorizon checks unchanged current kinematics on every candidate. At that distance the hazard can already sit inside the unavoidable closest-approach envelope, so a bounded lateral target is correctly not authorized.

The dynamic invalidation itself is still correct; the synthetic timing/location of the hazard is too late for a recoverable `AdjustedClear`.

## Current candidate

```
b57d81e42035f9771ae4feecee56899c4fa4f3f7
```

Fixture correction:
- hazard distance: 48 m;
- hazard radius: 6 m;
- hazard lateral velocity: -0.50 m/s.

New hard ordering:
1. `nominalDynamicConflictsFound > 0`;
2. status must be `AdjustedClear`;
3. adjusted target executes and must maintain >0.5 m conservative dynamic clearance.

New diagnostic:

```
[COMPOSITE-PLAN]
```

reports:
- law;
- planner status;
- adjusted flag;
- nominal conflicts;
- primary conflict id;
- probes;
- ordinary-search exhaustion;
- static blocked flag;
- live position/velocity;
- hazard position;
- selected target.

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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Exit

If 19/19:
- accept final composite;
- close synthetic behavior testing;
- move into NAV STRESS/game.

If red:
- diagnose from `[COMPOSITE-PLAN]`;
- do not relax tracking, clearance, hull or terminal requirements.

## Iteration rule

After every state/evidence change, synchronize all MDs and recreate `CONTINUE_PROMPT.md` from scratch.
