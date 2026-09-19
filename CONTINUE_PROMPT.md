# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this same rule.

## Exact accepted target baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 17/17 PASS;
- B7 speed/doctrine select->execute PASS.

## Accepted B7 behavior

Observed:
- Rational -> balanced;
- PrecisionRetrieval -> precision;
- Extreme/Newtonian -> Newtonian drift dash;
- Extreme/Assisted -> common fast aligned path;
- CombatEscape -> low-threat path;
- reckless criticalRisk=0.90 shortcut rejected above doctrine.

Selected programs really executed through follower/B10/PilotSkill/real physics.

## Current unverified candidate

New file:
```
tests/navigation_runtime/ManeuverChainedLimitMatrixTests.cpp
```

CTest:
```
maneuver_chained_limit_matrix
```

Candidate commits:
- `35e81f34605d63ec05876375f7511141730751a3`;
- `f8877e39d1d0a07c89440d2fe3b68708e3f72422`;
- `aff7ba6dc0185e794a5e64ce0051aa16b53c89d5`.

Expected navigation runtime total: **18 tests**.

## Chained execution fixture

For Newtonian and Assisted, one vehicle executes four consecutive programs without any transform/motion reset:

1. FreeTransit:
   - 6 -> 10 m/s.

2. PrecisionTransit:
   - hard moving ~90 degree change.

3. Law-specific:
   - Newtonian: fixed-body DriftPass;
   - Assisted: velocity-aligned PrecisionTransit.

4. PrecisionCapture:
   - moving -> zero terminal velocity;
   - StateCapture gate must wait for real terminal P/V/attitude/omega.

Each new program is authored from the actual final state of the previous phase.

Strict chain acceptance:
- 4/4 phases complete;
- zero tracking-envelope exceeded ticks;
- phase-seam P jump <=1e-9 m;
- V jump <=1e-9 m/s;
- forward jump <=1e-6 deg;
- angular velocity jump <=1e-9 rad/s;
- full Cobra hull <=25 m reference corridor half-width;
- Newtonian law-specific phase actual slip >=20 deg;
- Assisted law-specific phase actual slip <=8 deg;
- final P <=1.0 m;
- final speed <=0.60 m/s;
- final forward error <=4 deg.

## Negative / physical-limit fixture

### 1. Insufficient turn room/horizon
Production `OrdinaryPhysicalManeuverCompiler`:
- 18 m/s;
- major direction change;
- maximumProgramSeconds=0.5.

Must return:
- NoPhysicalCandidate;
- candidateCount=0.

### 2. Insufficient braking distance
Production `NavigationExecutionSafetyProbeBuilder`:
- 20 m/s;
- response reserve 0.5 s;
- braking 2 m/s2;
- only 60 m available.

Required stopping reserve must exceed available room; unsafe commitment is rejected.

### 3. Rigid hull too large
Cobra full perpendicular support radius must exceed a 12 m half-width corridor; centerline-only fit is forbidden.

### 4. No law-compatible maneuver
B7 with Assisted context and only NewtonianOnly candidates must return no valid selection.

### 5. New dynamic hazard
Production `NavigationExecutionReplanPolicy` must return:
- LocalHorizon;
- DynamicHazardInvalidated;
- immediate=true;
- continueAcceptedAutomaticExecution=false.

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
echo "===== CHAIN/LIMIT SUMMARY ====="
grep -E '\[CHAIN\]|\[LIMIT\]|MANEUVER CHAINED/LIMIT|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo
echo "===== LOG FILE ====="
echo "$PWD/$OUT"
```

## Next step

If green:
- accept chained transitions + fail-closed limit block;
- record measured chain/slip/corridor metrics;
- build only one final composite laboratory proving ground;
- after that move primary evaluation into the real game.

If red:
- identify the exact failing contract and repair it;
- never weaken the physical, geometry, seam or invalidation requirements just to obtain green.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
