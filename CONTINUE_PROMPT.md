# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new truth. Do not incrementally patch this prompt. Every recreated version must repeat this same rule.

## Last accepted target baseline

Exact tested checkout:

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 16/16 PASS;
- continuous 3D fly-through accepted with verbose metrics.

## Original B0-B14 architecture status

Strong/accepted:
- B0 authoritative world publication;
- B8 AcceptedManeuverProgram;
- B9 sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 real propulsion/physics;
- B14 scheduler.

Strong mechanics but incomplete production generalization:
- B5 ordinary physical compiler still lacks full Assisted/general-family production coverage;
- B6 proof is strong in exact-static/moving precision cases but not fully generalized as one ordinary block;
- B8-B10 still require final ordinary-live compatibility seam retirement.

Open/transitional:
- B1 shared influence batching;
- B2 unified objective;
- B3 vehicle/control-law-aware global edge feasibility;
- B4 route-aligned local corridor instead of ray-fan;
- B7 doctrine selection in the actual selected-program execution chain;
- B11 explicit bounded reflex API.

## Current unverified candidate

New source:
```
tests/navigation_runtime/ManeuverSpeedDoctrineMatrixTests.cpp
```

CTest:
```
maneuver_speed_doctrine_matrix
```

Candidate commits:
- `5294be208f6c48d29d2d1e00cb45b8d7c99045d8`;
- `3a51f6726196889b09a3782d8279971630536192`;
- `ed3228fd3d8f30b6954a99b14b59076e9bcda2b7`;
- `83d41454c0cec924f8839356028579d3fe1a0ff6`.

Expected runtime total: **17 tests**.

## B7 fixture

All candidates share:
- same 180 m objective;
- same initial state: +X at 6 m/s;
- one obstacle;
- Cobra full rigid hull;
- expert PilotSkill;
- real B9/B10/PilotSkill/physics execution after selection.

Candidate alternatives:
1. precision — largest clearance, slow;
2. balanced — best general reserve;
3. fast — common-law faster tighter path;
4. newtonian_drift_dash — NewtonianOnly, faster, fixed-forward high-slip path;
5. low_threat_escape — lowest threat exposure;
6. reckless_shortcut — fastest raw path but criticalRisk=0.90.

Preferred critical-risk ceiling is 0.20.

Expected selection:
- Rational -> balanced;
- PrecisionRetrieval -> precision;
- Extreme Newtonian -> newtonian_drift_dash;
- Extreme Assisted -> fast;
- CombatEscape -> low_threat_escape;
- reckless shortcut never selected while preferred-risk alternatives exist.

## Strict execution gate

For every selected program:
- zero tracking-envelope exceeded ticks;
- actual full-hull obstacle clearance >0.25 m;
- final P <=1.5 m;
- final velocity error <=0.75 m/s;
- final forward error <=5 deg.

Newtonian drift dash additionally:
- actual max slip >=20 deg.

The test prints `[DOCTRINE]` rows with:
- law;
- doctrine;
- selected candidate;
- planned time;
- planned clearance;
- planned peak speed/acceleration/slip;
- actual minimum clearance;
- actual peak speed/slip;
- final P/V/attitude;
- tracking exceeded ticks.

## Target validation

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

## Next action from target result

If green:
- record exact tested HEAD and metrics;
- accept first B7 select->program->execution gate;
- move to chained transitions + negative/physical-limit cases.

If red:
- separate B7 selection failure from physical execution failure;
- fix the mechanism;
- never weaken risk, clearance, tracking or terminal criteria merely to pass.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
