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
852e5a71a71625cdfc0c71a6bb89724d2194990e
```

Architecture PASS. Runtime 18/19. Previous 18 gates remain green.

Production dynamic plan:
- `AdjustedClear`;
- nominal conflict witness = 1;
- primary entity = 12060;
- 20 probes;
- no static block.

Authority-bounded first replacement:
- duration 24 s;
- terminal speed 2 m/s;
- peak transverse FF 1.348975 m/s2;
- planned hazard clearance 3.181387 m;
- actual hazard clearance 3.175166 m;
- tracking exceeded = 0;
- terminal P error 0.013099 m;
- terminal V error 0.076580 m/s.

Therefore planner + first physical bypass are healthy.

## Latest root cause

After the successful first local bypass, the test called:

```
Planner::plan(..., emptyDynamic(), ...)
```

while the dynamic hazard continued to exist in physical simulation.

The planner was therefore given a different world from the executor.

The final aggregate dynamic-clearance failure came from this fixture inconsistency, not from the first replacement.

## Current unverified candidate

```
6c0a71d308040c568c109afc4425332021ade730
01a8d69cc635a450d73a49a31b91e5546e0b1828
```

Persistent hazard rules:
- re-publish the hazard at its actual current position after each bypass;
- query NavigationMap again;
- run production NavigationRuntimePlanner again;
- if `AdjustedClear`, execute another authority-bounded local suffix from current live state;
- allow up to four additional continuations;
- resume static portal path only after `NominalClear`.

The original invalidation event remains exactly one. Additional suffix replans are normal short-horizon continuation after segment completion.

New diagnostics:
```
[COMPOSITE-RESUME]
[COMPOSITE-CONTINUATION]
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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## If green

Record exact target checkout + final composite metrics, close synthetic maneuver behavior lab, and move immediately to actual NAV STRESS/game accepted-corridor + physical-trajectory visualization.

## If red

Use persistent-hazard resume diagnostics to repair the exact remaining composition seam. Do not erase hazards or weaken physical criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
