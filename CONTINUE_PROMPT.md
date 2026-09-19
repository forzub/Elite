# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. The recreated prompt must contain this same rule.

## Accepted target baseline

Exact tested checkout:

```
213bbfb62ff7dcb8e553c06bdca09d99d2d1fd56
```

Evidence:
- Stage-12 architecture contract PASS.
- navigation_runtime **16/16 PASS**.
- new `maneuver_fly_through_3d` PASS.
- total main runtime test time ~0.43 s.

This accepts the continuous 3D fly-through gate.

## What is now proved

Strict Expert Newtonian and Assisted both passed the same continuous 5-segment 3D route with ~35/60/90/120 degree turns.

Embedded strict requirements include:
- 9/9 moving phases;
- no hidden StopTurnGo (speed >=3 m/s);
- complete Cobra hull inside 32 m half-width;
- zero tracking-envelope exceeded ticks;
- final P <=1.5 m;
- final speed error <=0.75 m/s relative to 8 m/s;
- final forward error <=5 deg;
- finite measured turn radius in every corner;
- planned corner acceleration <=2 m/s2.

Therefore those conditions are accepted for both laws.

## Missing target evidence

The old runner did not execute `maneuver_fly_through_3d` verbosely after the all-tests pass. The target log therefore lacks `[FLY3D]` and `[FLY3D-CORNER]` numeric rows.

Do not invent Newtonian-vs-Assisted differences from the PASS result alone.

## Current unverified runner patch

```
4cd9c4a14c8f2e4ce033082633766a21fece9331
```

This patch changes diagnostics only:
- adds verbose `ctest -R maneuver_fly_through_3d -V`;
- adds fly-through diagnostic timing.

No navigation behavior, test acceptance, physics, planner or follower logic changed.

## Immediate task

Run the target suite once more and inspect detailed FLY3D metrics, especially Expert Newtonian vs Assisted:
- min route speed;
- per-corner min speed;
- slip;
- observed turn radius;
- hull required half-width;
- forward tracking error.

Validation commands:

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

## Next stage after metrics

If metrics are physically coherent, record them and proceed directly to speed/doctrine coverage.

Do not modify the accepted fly-through mechanism merely because Newtonian and Assisted differ; law-specific differences are expected and should be characterized.

**Again: recreate this entire prompt from scratch after every state-affecting iteration.**
