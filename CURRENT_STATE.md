# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Target evidence:
- Stage-12 architecture contract: **PASS**;
- navigation_runtime: **18/18 PASS**;
- chained transition + physical-limit matrix: **PASS**.

## Newly accepted: chained transitions + fail-closed physical limits

Both laws complete the same four-phase chain with no synthetic state reset:

```
FreeTransit
 -> hard moving turn
 -> Newtonian DriftPass / Assisted aligned PrecisionTransit
 -> PrecisionCapture
```

### Newtonian measured chain

- phases: 4/4;
- seam P/V/forward/omega jumps: 0;
- maximum hull half-width: 17.419551 m inside 25 m;
- maximum slip: 35.114714 deg;
- final P error: 0.024611 m;
- final speed: 0.024960 m/s;
- final forward error: 0.029227 deg;
- tracking-envelope exceeded ticks: 0;
- total: 48.04 s.

### Assisted measured chain

- phases: 4/4;
- seam P/V/forward/omega jumps: 0;
- maximum hull half-width: 17.419551 m inside 25 m;
- maximum slip over full chain: 1.934322 deg;
- aligned phase max slip: 1.139399 deg;
- final P error: 0.024611 m;
- final speed: 0.024960 m/s;
- final forward error: 0.035921 deg;
- tracking-envelope exceeded ticks: 0;
- total: 48.04 s.

The parallel-transport/Bishop-frame correction eliminated the artificial full-attitude discontinuity without changing tracking gains or tolerances.

## Accepted negative / physical-limit contracts

1. Insufficient turn horizon:
   - 18 m/s;
   - 0.5 s available;
   - B5 returns NoPhysicalCandidate.

2. Insufficient braking distance:
   - required stopping reserve 110 m;
   - available 60 m;
   - unsafe commitment rejected.

3. Rigid hull corridor:
   - Cobra required half-width 13.238202 m;
   - available 12 m;
   - rejected.

4. Law incompatibility:
   - Assisted context with only NewtonianOnly candidates;
   - B7 returns no valid selection.

5. New dynamic hazard:
   - immediate LocalHorizon replan;
   - obsolete accepted program may not continue.

## Laboratory status

The following isolated/compound behavior blocks are now accepted:
- corner families;
- long moving attitude;
- full rigid-body corridor;
- 3D stop-to-stop route;
- continuous 3D fly-through;
- speed/doctrine selection + real execution;
- chained cross-family state continuity;
- physical-limit rejection/invalidation.

## Remaining laboratory gate

Only one behavior gate remains:

**final composite end-to-end proving ground**

It must combine, in one scenario:
- real static topology/obstacles;
- a dynamic hazard;
- wide and narrow passage pressure;
- speed change;
- doctrine/control-law-dependent physical choice;
- accepted-program execution through B9/B10/PilotSkill/physics;
- mid-run invalidation/replan;
- exact terminal capture.

This final lab must not be treated as proof that every production migration seam B1-B6/B11 is complete. Its purpose is to end synthetic maneuver behavior testing and move primary evaluation into the real game scene.

## After final composite acceptance

Stop adding synthetic maneuver-quality tests by default.

Primary loop becomes:
- run NAV STRESS / real game;
- visualize accepted corridor/trajectory;
- inspect NPC/manual guidance behavior;
- only add a focused regression when an actual defect is found.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 document;
- recreate `CONTINUE_PROMPT.md` from scratch.
