# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this same rule.

## Accepted exact target baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Evidence:
- Stage-12 architecture PASS;
- navigation_runtime 17/17 PASS;
- B7 speed/doctrine select->execute accepted.

## Current stage

Chained transitions + negative/physical-limit matrix.

CTest:
```
maneuver_chained_limit_matrix
```

Expected total after successful build: 18 tests.

## Latest target-machine result

Tested checkout:
```
4753451be23f913d3e20d2ca11c112f113980434
```

Architecture passed, but build failed before tests.

Exact defect:
```
ManeuverChainedLimitMatrixTests.cpp::captureState()
```
used `glm::dvec3 * float` for ShipTransform pitch/yaw/roll rates.

This is a test-harness type mismatch, not a navigation behavior failure.

## Current unverified fix

```
1e0d555a504e6913ee417b4b628e7d062081a0c0
```

The fix explicitly casts:
- pitchRate;
- yawRate;
- rollRate

to double before reconstructing map-space angular velocity.

No production navigation behavior changed.

## Intended chained gate

One vehicle, no resets:

```
FreeTransit
 -> hard moving PrecisionTransit
 -> Newtonian DriftPass / Assisted aligned PrecisionTransit
 -> PrecisionCapture with StateCapture
```

Strict seam requirements:
- P jump <=1e-9 m;
- V jump <=1e-9 m/s;
- forward jump <=1e-6 deg;
- omega jump <=1e-9 rad/s.

Execution requirements:
- 4/4 phases;
- zero tracking-envelope exceed ticks;
- full Cobra hull within 25 m reference half-width;
- Newtonian law-specific slip >=20 deg;
- Assisted law-specific slip <=8 deg;
- final P <=1.0 m;
- final speed <=0.60 m/s;
- final attitude <=4 deg.

## Negative contracts

- insufficient turn horizon -> B5 NoPhysicalCandidate;
- insufficient braking distance -> stopping reserve exceeds available room;
- full Cobra hull cannot fit 12 m half-width corridor;
- Assisted cannot select all-NewtonianOnly B7 population;
- new dynamic hazard -> immediate LocalHorizon replan and old accepted program stops being authoritative.

## Validation command

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

## Next action

If build/test fails again:
- diagnose the first real failure;
- do not weaken physical or seam criteria just to pass.

If 18/18:
- accept chained transitions + negative/limit block;
- build the single final composite laboratory proving ground.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
