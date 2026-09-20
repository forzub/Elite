# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Evidence:
- architecture PASS;
- navigation runtime 18/18 PASS;
- chained transitions + physical-limit matrix accepted.

## Closed block

The chained/limit laboratory block is complete.

Key evidence:
- Newtonian: 35.11 deg material drift, zero seam resets, zero tracking violations;
- Assisted: max chain slip 1.93 deg, aligned phase 1.14 deg, zero tracking violations;
- both finish at ~2.5 cm P error and ~0.025 m/s;
- full hull remains inside 25 m reference corridor.

Fail-closed contracts accepted:
- turn horizon;
- braking reserve;
- rigid-hull fit;
- control-law compatibility;
- dynamic hazard invalidation.

## Current task

Build the **final composite end-to-end laboratory proving ground**.

This is the last synthetic behavior test before moving the primary quality loop into the game.

## Composite requirements

One scenario, no independent fixture resets, must include:
1. static topology/obstacle detour;
2. narrow/wide passage pressure;
3. speed-up / hard moving turn;
4. doctrine/control-law physical choice;
5. accepted-program execution through B9/B10/PilotSkill/real physics;
6. a dynamic hazard introduced while an accepted program exists;
7. immediate invalidation/replan of the obsolete program;
8. continued progress on the replacement program;
9. exact terminal P/V/attitude capture.

At least Newtonian and Assisted expert runs are strict.

## Required output

Print one compact `[COMPOSITE]` row per law containing:
- selected doctrine candidate/family;
- phase count;
- replan count and invalidation reason;
- minimum rigid-body clearance;
- max slip;
- max tracking error / exceeded ticks;
- terminal P/V/attitude;
- total simulated time.

## Acceptance philosophy

The composite must consume already accepted production components where they exist. Do not duplicate a second planner merely to make the test green.

If a production seam is still intentionally transitional, state that explicitly and keep the test honest about what it proves.

## Exit criterion

If the final composite passes on target:
- laboratory maneuver behavior testing is closed;
- update MDs with exact accepted checkout;
- move next task to in-game NAV STRESS visualization/integration;
- do not invent more synthetic behavior matrices unless a real game defect demands one.

## Iteration rule

After every state/evidence change, synchronize all project MD files and recreate `CONTINUE_PROMPT.md` from scratch.
