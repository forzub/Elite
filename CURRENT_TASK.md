# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Architecture PASS; navigation runtime 17/17; B7 select->execute accepted.

## Latest failed gate

Tested checkout:
```
4753451be23f913d3e20d2ca11c112f113980434
```

Outcome:
- architecture PASS;
- build failed before tests.

Root cause:
`ManeuverChainedLimitMatrixTests.cpp::captureState()` multiplied `glm::dvec3` by `float` pitch/yaw/roll rates.

Fix candidate:
```
1e0d555a504e6913ee417b4b628e7d062081a0c0
```

Only the test harness changed; navigation behavior is untouched.

## Current task

Rerun:
```
maneuver_chained_limit_matrix
```

Expected total: **18 tests**.

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
    TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
    time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

    echo
    echo "===== NAVIGATION RUNTIME ====="
    bash tests/navigation_runtime/run_mingw64.sh
} 2>&1 | tee "$OUT"

echo
echo "===== CHAIN/LIMIT SUMMARY ====="
grep -E '\[CHAIN\]|\[LIMIT\]|MANEUVER CHAINED/LIMIT|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo
echo "===== LOG FILE ====="
echo "$PWD/$OUT"
```

Upload the complete log.

## Interpretation

If build succeeds but a runtime gate fails:
- separate chain/reference defect from real physics/follower defect;
- separate negative-contract defect from execution defect;
- fix the mechanism or fixture truthfully;
- do not widen tolerances merely to obtain green.

If 18/18:
- accept chained transitions + fail-closed physical-limit block;
- move to the final composite laboratory proving ground.

## Iteration rule

After every state/evidence change, update all project MD files and recreate `CONTINUE_PROMPT.md` from scratch.
