# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest tested checkout

```
69f8ca4dbcb44df5340b94f45640bcb7d6e6ed1a
```

Architecture PASS, runtime 17/19.

Failures:
- focused side-continuity regression;
- final composite.

## What the last run proved

The velocity-only tie-break is insufficient.

Repeated local replans can still change avoidance branch because instantaneous velocity is not the same thing as the direction/branch of the currently accepted bounded segment.

The focused regression also had a fixture bug: generic static region Z bounds were only +/-10 m, so intended +/-Z alternatives were not actually statically legal.

## Current architecture fix

Execution owns accepted-segment continuity.

New runtime planner input:
```
localAvoidanceContinuityValid
localAvoidanceContinuityDirectionMap
```

This is forwarded to:
```
LocalAvoidancePlanner::Query::preferredDirection*
```

Selection rule inside the smallest safe ring:
1. accepted-segment continuity direction;
2. otherwise current velocity direction;
3. otherwise nominal forward;
4. deterministic azimuth index for exact ties.

No hidden planner memory.

## Current candidate commits

Production:
```
71b80c4529e1bc776e2a2dbf209059a2ccff44f9
4bde26ee2fbb531116f960d089d488067f1bfd6d
76022a5199422dd80ef4caff611539b53c39e731
40d7b9852bc6f265ae02ecc0bf9d7b4e002d5b96
```

Tests/composite:
```
bd31307fbda3d512a579f521efc1664199b1de46
09bc81e03cab6c251b50678585161cc73e814ddb
```

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

If focused regression fails:
- debug explicit continuity forwarding/scoring.

If regression passes but composite still side-switches:
- inspect whether direction-only continuity needs promotion to an explicit accepted branch/plane identifier.

If 19/19:
- accept final composite;
- close synthetic behavior lab;
- next task = real NAV STRESS/game accepted-corridor + physical-trajectory visualization.

## Iteration rule

After every state/evidence change, synchronize all MDs and recreate `CONTINUE_PROMPT.md` from scratch.
