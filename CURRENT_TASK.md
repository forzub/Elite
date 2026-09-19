# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

Exact target-tested checkout:

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Architecture PASS; navigation runtime 17/17; B7 doctrine select->execute accepted.

## Current task

Target-test:

```
maneuver_chained_limit_matrix
```

Expected runtime total: **18 tests**.

## Part A — chained execution

Both flight laws run four consecutive programs without state reset:

```
moving transit
 -> hard 90-degree continuous turn
 -> law-specific maneuver
 -> precision StateCapture
```

Newtonian:
- phase 3 = high-slip DriftPass.

Assisted:
- phase 3 = velocity-aligned PrecisionTransit.

Strict checks:
- 4/4 phases;
- zero tracking-envelope exceed ticks;
- no P/V/attitude/omega reset at seams;
- full Cobra hull <=25 m reference half-width;
- Newtonian phase-3 slip >=20 deg;
- Assisted phase-3 slip <=8 deg;
- terminal P <=1.0 m;
- terminal speed <=0.60 m/s;
- terminal attitude <=4 deg.

## Part B — negative/limit cases

1. B5 turn horizon too short:
   - expect NoPhysicalCandidate.

2. braking reserve > available distance:
   - expect pre-ACCEPT rejection.

3. Cobra rigid hull > corridor:
   - expect geometry rejection.

4. Assisted + only NewtonianOnly B7 candidates:
   - expect no valid selection.

5. new dynamic hazard invalidates accepted execution:
   - immediate LocalHorizon replan;
   - old program must not continue.

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

If 18/18:
- accept chained transition continuity;
- accept the five fail-closed limit contracts;
- record actual Newtonian/Assisted chain metrics;
- move to the final composite laboratory proving ground.

If failed:
- identify whether failure is:
  - phase seam/reference construction;
  - tracking/physics execution;
  - rigid-body corridor;
  - B5 physical rejection;
  - braking reserve;
  - B7 law filter;
  - execution invalidation.
- do not weaken the acceptance criteria.

## Iteration rule

After every state/evidence change, update all project MD files and recreate `CONTINUE_PROMPT.md` from scratch.
