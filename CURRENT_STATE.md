# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest target-machine final-composite attempt

Exact tested checkout:

```
d57a22f69c3af1a8c967ce974b895295c77dd21a
```

Results:
- architecture contract PASS;
- build PASS;
- navigation runtime 18/19;
- all previously accepted 18 tests remain PASS;
- only `navigation_composite_proving_ground` failed.

Failure:

```
composite production planner did not find adjusted dynamic bypass
```

## Root cause

The final composite fixture introduced the dynamic hazard only 35 m ahead of the live vehicle while the actor was already moving materially toward the same bounded horizon.

Production `LocalHorizonPlanner` evaluates two independent dynamic safety conditions:
1. unchanged current kinematics over the physical look-ahead;
2. bounded requested corridor.

Every local avoidance probe reuses the same current kinematic state. Therefore, when the hazard is already inside the unavoidable closest-approach envelope, every lateral candidate correctly remains `ConflictHold`.

This is not evidence that `NavigationRuntimePlanner` cannot generate `AdjustedClear`. The fixture triggered invalidation too late.

## Current unverified fix candidate

Code commit:

```
b57d81e42035f9771ae4feecee56899c4fa4f3f7
```

Changes:
- dynamic hazard appears 48 m ahead instead of 35 m;
- hazard radius reduced from 8 m to 6 m;
- lateral hazard velocity reduced from -0.75 to -0.50 m/s;
- nominal route must still report at least one dynamic conflict;
- only then may `AdjustedClear` satisfy the test;
- added `[COMPOSITE-PLAN]` diagnostics with planner status, nominal conflicts, probe count, search exhaustion, static block state, P/V, hazard position and selected target.

The acceptance requirement is not weakened:
- hazard must still invalidate the old accepted program;
- production planner must still see a nominal dynamic conflict;
- production planner must produce a real adjusted safe target;
- replacement execution must clear the hazard physically.

## Current active gate

Final composite end-to-end proving ground remains open.

Expected suite remains **19 tests**.

If the candidate passes:
- synthetic maneuver behavior laboratory closes;
- next primary task moves into real NAV STRESS/game trajectory/corridor visualization and live behavior review.

If it fails:
- `[COMPOSITE-PLAN]` must identify whether the result is ConflictHold, StaticHold, NominalClear or another planner state;
- fix the scenario/mechanism honestly, without weakening physical clearance or terminal criteria.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
