# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Architecture PASS; navigation runtime 18/18; chained/limit block accepted.

## Current task

Target-test the **final composite end-to-end proving ground**:

```
navigation_composite_proving_ground
```

Expected navigation runtime total: **19 tests**.

## What the final composite does

One uninterrupted vehicle scenario per law:

```
exact-static direct route blocked
 -> production NavigationSpace portal detour
 -> execute first physical segment
 -> B7 Extreme law-specific family selection
 -> execute accepted program prefix
 -> new dynamic actor appears
 -> ReplanPolicy invalidates old accepted program
 -> production NavigationRuntimePlanner finds AdjustedClear
 -> replacement program starts from actual live state
 -> narrow second portal
 -> final PrecisionCapture / StateCapture
```

## Strict expected behavior

Newtonian:
- B7 selected family = DriftPass;
- material drift >=15 deg.

Assisted:
- B7 selected family = PrecisionTransit;
- max slip <=8 deg.

Both:
- one DynamicHazardInvalidated LocalHorizon replan;
- old accepted program stops immediately;
- production planner reports dynamic conflict and AdjustedClear;
- zero tracking-envelope exceeded ticks;
- static conservative full-hull clearance >0.5 m;
- dynamic conservative full-hull clearance >0.5 m;
- narrow full-hull half-width <=19 m;
- final P <=1 m;
- final speed <=0.60 m/s;
- final attitude <=4 deg.

## Production/test-side boundary

Production components used:
- NavigationSpace;
- NavigationMap;
- NavigationRuntimePlanner;
- B7 ManeuverDecisionController;
- NavigationExecutionReplanPolicy;
- B8/B9/B10;
- PilotSkill;
- SharedShipPhysics/DynamicMotionSystem.

Test-side only:
- general physical time-program authoring helper.

That limitation is intentional and must stay explicit until production B5 Assisted/general-family compiler migration is complete.

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
grep -E '\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo
echo "===== LOG FILE ====="
echo "$PWD/$OUT"
```

Upload the complete log.

## Exit condition

If 19/19 and composite PASS:
- record exact target-tested checkout and metrics;
- declare synthetic maneuver behavior laboratory complete;
- next task: integrate/visualize accepted corridor + trajectory in actual NAV STRESS/game and inspect real behavior.

If red:
- fix the first actual composite seam;
- do not weaken physical, clearance, tracking or terminal criteria merely to obtain green.

## Iteration rule

After every state/evidence change, synchronize all MD files and recreate `CONTINUE_PROMPT.md` from scratch.
