# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest target-machine attempt

Exact tested checkout:

```
af9ee9d1694ac0facafaf23b0ec51c3adaf7dbbf
```

Results:
- architecture contract PASS;
- runtime 17/19;
- failures:
  - `navigation_runtime_planner` focused cross-ring regression;
  - `navigation_composite_proving_ground`.

## What is now proven

The forced branch-switch escalation path works.

Composite Newtonian:
- planner reports `branch_switch_required=1`;
- physical Brake recovery is authored;
- recovery duration 4.0 s;
- stopping distance 6.757088 m;
- peak brake FF 1.266954 m/s2;
- planned static clearance 39.154319 m;
- planned dynamic clearance 9.803750 m;
- actual dynamic clearance 9.817623 m;
- tracking exceeded ticks = 0;
- final speed error = 0.009128 m/s.

Therefore the new architecture:
```
branch exhausted
 -> branch-switch escalation
 -> physical recovery
 -> stop
 -> clear old continuity
 -> replan
```
is functioning physically.

## Latest composite failure root cause

After recovery, the planner correctly replans with continuity cleared:
- continuity lateral valid = 0;
- branch switch required = 0;
- new AdjustedClear target is returned.

But `fitAuthorityBoundedReplacement()` still used the old moving-transit rule:
```
minimumPlannedSpeed >= 0.50 m/s
```
including sample t=0.

The successful recovery leaves the craft at ~0.009 m/s by design, so every post-recovery launch candidate necessarily fails this gate before execution.

This is a test-side B5 authoring seam, not a planner or physics failure.

## Current unverified post-recovery authoring fix

Commit:

```
7c87e655788af4e95f9576675185482639fec528
```

Replacement fitter now distinguishes:
- normal moving transit: original `minimum speed >=0.50 m/s` remains unchanged;
- launch from recovery-rest: start below 0.50 m/s is allowed, but the curve must:
  - never reverse progress more than 0.05 m/s;
  - reach 0.50 m/s;
  - never fall below 0.50 m/s after reaching it;
  - still satisfy transverse FF <=1.35 m/s2;
  - still satisfy planned dynamic clearance >=1.50 m.

No ordinary moving-transit criterion was weakened.

## Focused regression fixture correction

Previous blocker placement remained horizon-sensitive.

The test assumed a 600 m primary probe, but `PhysicalManeuverHorizon` adds braking/safety reserve, so the actual ray is slightly longer.

New commit:

```
252f9d81c5fd91363a0e0561e195c1f5ab0d375d
```

The blocker is now placed at an **interior 300 m point** of the primary -Z ray:
```
x = 300*cos(primaryDeflection)
z = -300*sin(primaryDeflection)
```

Since the physical horizon is >300 m, the primary ray must cross it regardless of exact horizon length, while the larger-ring -Z ray is far away at that same X.

New diagnostic:
```
[BRANCH-REGRESSION]
```
prints:
- status;
- selected deflection;
- same-branch safe count;
- branch alignment;
- branch-switch-required;
- selected target.

## Current gate

Expected runtime suite remains **19 tests**.

If focused regression passes:
- cross-ring same-branch semantics are accepted.

If post-recovery transit now executes:
- branch-switch recovery + launch into new branch are physically sequenced correctly.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior laboratory;
- move primary evaluation into actual NAV STRESS/game.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
