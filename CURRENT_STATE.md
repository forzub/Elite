# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted exact target-machine baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 17/17 PASS;
- B7 speed/doctrine select->execute matrix PASS;
- all prior maneuver/corridor/fly-through regressions remain green.

## Accepted B7 result

Doctrine selection is now physically demonstrated, not only unit-tested:
- Rational -> balanced;
- PrecisionRetrieval -> precision;
- Extreme/Newtonian -> Newtonian-only drift dash;
- Extreme/Assisted -> common-law fast path;
- CombatEscape -> low-threat path;
- faster reckless shortcut with criticalRisk=0.90 rejected above doctrine.

Every selected AcceptedManeuverProgram executed through follower/B10/PilotSkill/real physics with zero tracking-envelope violations.

## Current unverified candidate: chained transitions + physical limits

New test:

```
tests/navigation_runtime/ManeuverChainedLimitMatrixTests.cpp
```

CTest:

```
maneuver_chained_limit_matrix
```

Expected runtime suite size: **18 tests**.

Candidate commits:
- `35e81f34605d63ec05876375f7511141730751a3` — chained/limit test source;
- `f8877e39d1d0a07c89440d2fe3b68708e3f72422` — CMake registration;
- `aff7ba6dc0185e794a5e64ce0051aa16b53c89d5` — verbose runner diagnostic.

## Chained execution design

Both Newtonian and Assisted execute four consecutive accepted phases **without resetting vehicle state**:

1. moving transit:
   - 6 -> 10 m/s;
   - straight progress.

2. hard continuous turn:
   - approximately 90 degree velocity-direction change;
   - moving throughout.

3. law-specific family:
   - Newtonian: fixed-body high-slip DriftPass;
   - Assisted: velocity-aligned PrecisionTransit.

4. precision braking/capture:
   - terminal velocity -> 0;
   - StateCapture semantics;
   - time expiry may not fake success.

Every next program is authored from the actual previous P/V/body basis/angular velocity. The test measures phase-seam discontinuity and fails if a reset/jump is injected.

Strict chain checks:
- 4/4 phases complete;
- zero tracking-envelope exceed ticks;
- seam P jump <= 1e-9 m;
- seam V jump <= 1e-9 m/s;
- seam attitude jump <= 1e-6 deg;
- seam angular-velocity jump <= 1e-9 rad/s;
- full Cobra hull remains inside a 25 m reference corridor;
- Newtonian law-specific phase must produce >=20 deg slip;
- Assisted law-specific phase must stay <=8 deg slip;
- final capture P <=1.0 m, speed <=0.60 m/s, attitude <=4 deg.

## Negative / physical-limit cases

The same test also verifies fail-closed behavior with existing production components.

### Insufficient turn horizon
Uses `OrdinaryPhysicalManeuverCompiler`:
- 18 m/s state;
- ~90+ degree required delta-v;
- only 0.5 s / ~9 m local maneuver horizon.

Expected:
- `NoPhysicalCandidate`;
- candidateCount=0;
- impossible turn never reaches ACCEPT.

### Insufficient braking distance
Uses `NavigationExecutionSafetyProbeBuilder::buildStoppingReserve`:
- 20 m/s;
- 0.5 s control-response reserve;
- 2 m/s2 braking;
- only 60 m available.

Expected:
- required stopping reserve exceeds available distance;
- maneuver rejected before unsafe commitment.

### Rigid hull too large
Full Cobra perpendicular support radius is compared to a 12 m half-width corridor.

Expected:
- corridor rejected;
- centerline fit cannot override rigid-body occupancy.

### No law-compatible candidate
Uses B7 `ManeuverDecisionController`:
- Assisted context;
- candidate population is NewtonianOnly.

Expected:
- invalid/no selection;
- incompatible family cannot leak through doctrine ranking.

### New dynamic hazard
Uses `NavigationExecutionReplanPolicy`.

Expected:
- immediate LocalHorizon replan;
- reason DynamicHazardInvalidated;
- obsolete accepted program is not allowed to continue.

## Current status

This candidate is **not accepted** until exact target-machine evidence is returned.

If green:
- chained state handoff + fail-closed physical-limit block closes;
- only one final composite laboratory proving ground remains before primary visual/game evaluation.

If red:
- separate chain continuity/tracking failure from negative-case contract failure;
- fix the mechanism, not the criteria.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 document;
- recreate `CONTINUE_PROMPT.md` **from scratch**.
