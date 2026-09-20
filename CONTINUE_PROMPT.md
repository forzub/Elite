# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest target attempt

```
d57a22f69c3af1a8c967ce974b895295c77dd21a
```

Architecture PASS. Runtime 18/19. All prior accepted tests remain green.

Only failure:
```
NAVIGATION COMPOSITE PROVING GROUND: FAIL:
composite production planner did not find adjusted dynamic bypass
```

## Root cause

The fixture inserted a dynamic hazard only 35 m ahead after four seconds of the selected physical segment.

Production LocalHorizon safety evaluates current unchanged kinematics as well as the requested bounded corridor. Every adjusted probe starts from the same real current state.

At 35 m, the fixture can already place the hazard inside the unavoidable closest-approach envelope, so every local deflection is correctly rejected as ConflictHold.

This is a late synthetic hazard, not a production planner regression.

## Current unverified code candidate

```
b57d81e42035f9771ae4feecee56899c4fa4f3f7
```

Changes:
- hazard now appears 48 m ahead;
- hazard radius 6 m;
- cross velocity -0.50 m/s;
- test first requires `nominalDynamicConflictsFound > 0`;
- then requires real `AdjustedClear`;
- physical replacement must still maintain >0.5 m conservative dynamic clearance.

New diagnostic:
```
[COMPOSITE-PLAN]
```

It prints planner status, conflict witness, probes, exhaustion, static blocker flag, live P/V, hazard P and selected target.

## Final composite expected chain

```
exact static direct block
 -> production portal detour
 -> physical first segment
 -> B7 Extreme law-specific choice
 -> selected program prefix
 -> dynamic hazard publication
 -> immediate DynamicHazardInvalidated replan
 -> nominal dynamic conflict
 -> production AdjustedClear
 -> replacement from actual live state
 -> narrow portal
 -> final StateCapture
```

## Strict acceptance remains

Both laws:
- one immediate replan;
- zero tracking-envelope exceeded ticks;
- min static clearance >0.5 m;
- min dynamic clearance >0.5 m;
- narrow full-hull half-width <=19 m;
- final P <=1 m;
- final speed <=0.60 m/s;
- final attitude <=4 deg.

Newtonian:
- DriftPass selected;
- max slip >=15 deg.

Assisted:
- PrecisionTransit selected;
- max slip <=8 deg.

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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## If green

Record exact target HEAD, close synthetic maneuver behavior laboratory, and move immediately to real NAV STRESS/game accepted-corridor + trajectory visualization.

## If red

Use `[COMPOSITE-PLAN]` to repair the exact remaining composition seam. Do not weaken physical acceptance thresholds.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
