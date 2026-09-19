# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

Exact target-tested checkout:

```
213bbfb62ff7dcb8e553c06bdca09d99d2d1fd56
```

Results:
- architecture PASS;
- navigation runtime **16/16**;
- continuous 3D fly-through strict Expert gate PASS.

## Immediate task

Capture the detailed target-machine `FLY3D` diagnostics.

The accepted run did not include verbose output for the new test, so exact Newtonian/Assisted radius/slip/speed/hull comparisons are still missing.

Diagnostics-only runner patch:

```
4cd9c4a14c8f2e4ce033082633766a21fece9331
```

This does not change navigation behavior or acceptance criteria. It only adds verbose execution of:

```
maneuver_fly_through_3d
```

after the normal suite.

## Run

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
echo "===== FLY3D SUMMARY ====="
grep -E '\[FLY3D\]|\[FLY3D-CORNER\]|MANEUVER 3D FLY-THROUGH|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo
echo "===== LOG FILE ====="
echo "$PWD/$OUT"
```

## Metrics to compare

For Expert Newtonian vs Assisted:
- `min_route_speed_mps`;
- `max_center_cross_track_m`;
- `max_hull_required_half_width_m`;
- `max_forward_tracking_error_deg`;
- per-corner:
  - `actual_min_speed_mps`;
  - `max_slip_deg`;
  - `min_observed_turn_radius_m`;
  - `max_hull_required_half_width_m`.

Competent/Rookie remain diagnostic.

## After metrics capture

Record evidence and proceed to speed/doctrine matrix. Do not alter the already accepted fly-through mechanism unless new diagnostics reveal a real defect.

## Iteration rule

After every state/evidence change, update all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
