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
18f93e15f3baa5218d459b289ae89beb170f1c54
```

Architecture PASS. Runtime 18/19. Previous 18 gates remain green.

Production composite planner evidence:
```
[COMPOSITE-PLAN]
law=newtonian
status=adjusted_clear
adjusted=1
nominal_dynamic_conflicts=1
primary_conflict=12060
probes=20
ordinary_exhausted=0
nominal_static_blocked=0
position=(138.841366,52.623525,0)
velocity=(9.263267,3.401145,0)
hazard=(185.850434,42.920559,0)
selected_target=(159.856528,40.750781,17.815749)
```

The planner is now doing exactly what the final lab requires.

Failure:
```
composite replacement did not clear dynamic hazard
```

## Root cause

The test authored a 6-second quintic from the live P/V state to the adjusted target with 8 m/s terminal speed.

That analytic curve requires roughly:
- 5.61 m/s2 peak total acceleration;
- 5.03 m/s2 peak transverse acceleration.

The Cobra fixture only has 2.0 m/s2 lateral/vertical manoeuvre authority and needs B10 reserve.

Therefore the target was safe but the test-side B5-like time parameterization was physically invalid.

## Current unverified candidate

```
7444c5930586300d6cac48bd4b2fa63b27e96bd6
```

Replacement authoring now searches for the shortest physical candidate:
- duration 8..32 s;
- terminal speed 4 then 2 m/s;
- 768 dense analytic samples.

Required before ACCEPT:
- peak transverse FF <=1.35 m/s2;
- minimum speed >=0.50 m/s;
- planned conservative dynamic clearance >=1.50 m.

Then real execution requires:
- zero tracking-envelope exceeded ticks;
- actual conservative dynamic clearance >0.5 m.

Diagnostics:
```
[COMPOSITE-REPLACEMENT]
[COMPOSITE-REPLACEMENT-ACTUAL]
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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## If green

Record exact target checkout and composite metrics, close synthetic maneuver behavior laboratory, then move immediately to real NAV STRESS/game accepted-corridor + trajectory visualization.

## If red

Compare planned vs actual replacement metrics and repair the exact remaining seam. Do not weaken physical criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
