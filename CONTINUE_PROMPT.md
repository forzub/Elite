# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. The recreated prompt must contain this same rule.

## Accepted target baseline

Exact tested checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation_runtime 15/15;
- strict expert StopTurnGo / RadiusTurn / DriftTurn healthy;
- long 180 deg angular tracking healthy.

## Current unverified candidate

New test code:
- `5c10c19d7fd2eb4b1fb6aa26b903fd55713b6dcf`
- CMake registration: `c48a92010350cf12f417aa19f23f75487dfb1459`

New CTest:

```
maneuver_fly_through_3d
```

Expected navigation runtime total: **16 tests**.

## Purpose

Close the next quality gap: the existing 3D corridor matrix is non-orthogonal, but it is stop-to-stop with in-place rotations. The new test exercises a genuinely continuous 3D fly-through.

## Route and participants

Same 5-segment route for:
- Expert / Competent / Rookie;
- Newtonian / Assisted.

Turn angles are approximately:
- 35 deg;
- 60 deg;
- 90 deg;
- 120 deg.

Nominal speed:
- 8 m/s.

Corner cut:
- 45 m on incoming and outgoing legs.

Cobra Mk1 rigid hull:
- half extents 13.0 / 2.5 / 11.1 m.

Corridor:
- 32 m half-width measured using all eight hull corners.

## Reference construction

Every corner is a C2 quintic 3D transition:
- position continuous;
- velocity continuous;
- 8 m/s at endpoints;
- zero translational acceleration at seams;
- body forward follows velocity tangent;
- angular velocity and angular acceleration feed-forward are derived from sampled orientation changes.

The reference may lose speed inside hard turns, but expert must never fall below 3 m/s; therefore a hidden StopTurnGo fails.

The sampled planned corner acceleration must remain <=2 m/s2.

## Strict expert gate

Both Newtonian and Assisted must:
- complete 9/9 moving phases;
- keep full hull inside 32 m half-width;
- have zero tracking-envelope exceed ticks;
- stay >=3 m/s;
- finish P <=1.5 m;
- finish speed error <=0.75 m/s relative to 8 m/s;
- finish attitude <=5 deg;
- produce finite observed turn radius in all four corners.

Competent/Rookie are diagnostic on the first pass.

## Diagnostics

Global:
- `[FLY3D]`

Per corner:
- `[FLY3D-CORNER]`

Compare laws using:
- actual minimum speed;
- max slip;
- minimum observed radius;
- hull required half-width;
- forward tracking error;
- tracking-envelope exceed count.

Do not assume Assisted must be worse; measure it.

## Validation commands

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

## Response to next target log

First determine whether the new target is 16/16.

If green:
- record exact tested HEAD;
- analyze Newtonian vs Assisted corner radius/speed/slip/hull metrics;
- determine what capability was actually closed;
- update all MDs and recreate this prompt;
- proceed to speed/doctrine stage.

If red:
- identify the first physical/reference failure from FLY3D metrics;
- fix mechanism rather than weakening strict criteria;
- synchronize MD state before another candidate.

**Again: recreate this entire prompt from scratch after every state-affecting iteration.**
