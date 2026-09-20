# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Evidence:
- Stage-12 architecture PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Final synthetic behavior candidate

Source:
```
tests/navigation_runtime/NavigationCompositeProvingGroundTests.cpp
```

CTest:
```
navigation_composite_proving_ground
```

Expected full suite: **19 tests**.

Candidate commits:
- `ecef36520a0957300b70f1469fcd47d209dcb7e0`;
- `9a74a03845003a302db38ec2102c5d688e7c214b`;
- `0eaf1b5faddef336d4110d9b306f609cef5b0a8c`;
- `753d6027b76c99498132a0e4f8ecf533cf7cbcbe`.

## Scenario

For each expert law:

1. Production exact-static query proves direct start -> final is blocked by `composite_static_wall`.
2. Production `NavigationSpace/NavigationRuntimePlanner` selects a two-portal topology detour.
3. First physical AcceptedManeuverProgram executes through B9/B10 -> PilotSkill -> real physics.
4. B7 `Extreme` chooses:
   - Newtonian: faster NewtonianOnly DriftPass;
   - Assisted: common aligned PrecisionTransit after law filtering.
5. Only a prefix executes.
6. A new dynamic actor is published ahead through NavigationMap.
7. Production `NavigationExecutionReplanPolicy` must invalidate old execution immediately with `DynamicHazardInvalidated`.
8. Production `NavigationRuntimePlanner` must report nominal dynamic conflict and produce `AdjustedClear`.
9. Replacement physical program is authored from actual live state; no reset.
10. Static topology resumes through second constrained portal.
11. Final PrecisionCapture uses StateCapture.

## Strict acceptance

Both laws:
- one replan;
- old program not continued;
- zero tracking-envelope exceed;
- min static clearance >0.5 m;
- min dynamic clearance >0.5 m;
- narrow passage full-hull half-width <=19 m;
- final P <=1 m;
- final speed <=0.60 m/s;
- final forward <=4 deg.

Newtonian:
- selected family DriftPass;
- max slip >=15 deg.

Assisted:
- selected family PrecisionTransit;
- max slip <=8 deg.

Output:
```
[COMPOSITE] law=... doctrine=extreme selected_family=... replans=... min_static_clearance_m=... min_dynamic_clearance_m=... max_hull_half_width_m=... max_slip_deg=... tracking_exceeded_ticks=... final_pos_error_m=... final_speed_mps=... final_forward_error_deg=...
```

## Honesty boundary

The composite uses production topology, dynamic working set, runtime planner, B7, replan policy, B8-B10, PilotSkill and real physics.

General physical time-program authoring remains test-side because production B5 Assisted/general-family compiler is not yet fully migrated.

A green composite closes synthetic behavior testing; it does not falsely claim B1-B6/B11 migration complete.

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
grep -E '\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## If green

- record exact target-machine checkout and composite metrics;
- mark synthetic maneuver behavior lab complete;
- next task becomes in-game NAV STRESS accepted-corridor + physical-trajectory visualization and live behavior evaluation.

Do not add another synthetic matrix unless an actual game defect requires a focused regression.

## If red

Fix the first real composition seam; do not weaken criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
