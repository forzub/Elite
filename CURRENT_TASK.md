# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

Exact target-tested checkout:

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Architecture PASS; navigation runtime 16/16.

## Current task

Target-test the new **B7 speed/doctrine execution matrix**.

Expected runtime suite size: **17 tests**.

New test:
```
maneuver_speed_doctrine_matrix
```

## What it tests

Six physically generated candidate programs solve the same 180 m objective around one obstacle.

Expected B7 selections:

| Law | Rational | PrecisionRetrieval | Extreme | CombatEscape |
|---|---|---|---|---|
| Newtonian | balanced | precision | newtonian_drift_dash | low_threat_escape |
| Assisted | balanced | precision | fast | low_threat_escape |

Additional hard rule:
- `reckless_shortcut` is faster but has criticalRisk=0.90;
- preferred risk ceiling is 0.20;
- it must not be selected while safer valid candidates exist.

## Execution checks

Every selected program runs through the real accepted execution path:
`AcceptedManeuverProgram -> sampler/follower -> B10 -> PilotSkill -> SharedShipPhysics/DynamicMotionSystem`.

Strict expert checks:
- zero tracking-envelope exceeded ticks;
- positive actual full-hull obstacle clearance >0.25 m;
- final P <=1.5 m;
- final velocity error <=0.75 m/s;
- final forward error <=5 deg;
- Newtonian drift dash produces >=20 deg actual slip.

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
echo "===== SPEED/DOCTRINE SUMMARY ====="
grep -E '\[DOCTRINE\]|MANEUVER SPEED/DOCTRINE|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo
echo "===== LOG FILE ====="
echo "$PWD/$OUT"
```

Upload the complete log.

## Interpretation

If 17/17:
- B7 has its first real select->accept->execute proof;
- compare planned/actual clearance, time, peak speed and slip;
- move to chained transition + negative/physical-limit testing.

If failed:
- diagnose selection error separately from execution error;
- no tolerance weakening.

## Iteration rule

After every state/evidence change, update all project MD files and recreate `CONTINUE_PROMPT.md` from scratch.
