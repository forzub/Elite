# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

Exact tested checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

Architecture PASS; navigation runtime 15/15.

## Current task

Target-test the new **continuous 3D fly-through** regression.

Code candidate:
- `5c10c19d7fd2eb4b1fb6aa26b903fd55713b6dcf`
- `c48a92010350cf12f417aa19f23f75487dfb1459`

Expected suite size: **16 tests**.

## Test matrix

PilotSkill:
- Expert — strict;
- Competent — diagnostic;
- Rookie — diagnostic.

Flight laws:
- Newtonian;
- Assisted.

One identical 5-segment 3D route:
- ~35 deg;
- ~60 deg;
- ~90 deg;
- ~120 deg turns.

Nominal fly-through speed: 8 m/s.

No corner may be replaced by a full stop.

## What to inspect

Global `[FLY3D]` rows:
- completed;
- phases;
- final_pos_error_m;
- final_speed_error_mps;
- final_forward_error_deg;
- min_route_speed_mps;
- max_center_cross_track_m;
- max_hull_required_half_width_m;
- max_corridor_violation_m;
- max_forward_tracking_error_deg;
- tracking_envelope_exceeded_ticks.

Per-corner `[FLY3D-CORNER]` rows:
- route_angle_deg;
- planned_peak_accel_mps2;
- planned_min_speed_mps;
- actual_min_speed_mps;
- max_slip_deg;
- min_observed_turn_radius_m;
- max_center_cross_track_m;
- max_hull_required_half_width_m.

## Strict Expert acceptance

Both laws:
- 9/9 phases;
- corridor violation 0;
- tracking-envelope exceeded ticks 0;
- minimum moving speed >=3 m/s;
- final P <=1.5 m;
- final speed error <=0.75 m/s;
- final attitude <=5 deg;
- finite turn radius for every corner;
- planned corner acceleration <=2 m/s2.

## Test commands

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

Upload the complete log after the run.

## Iteration rule

After target evidence arrives, synchronize all state MD files and recreate `CONTINUE_PROMPT.md` from scratch before proceeding.
