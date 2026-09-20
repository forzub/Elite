# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation runtime 18/18;
- chained transitions + physical-limit block.

## Current final laboratory candidate

`navigation_composite_proving_ground`

Expected suite size: 19.

The scenario composes already accepted pieces in one uninterrupted run:
- exact static geometry blocks direct route;
- production static topology selects portal detour;
- B7 selects law-specific physical family;
- B8-B10/PilotSkill/physics execute it;
- dynamic hazard appears while program is active;
- production replan policy invalidates it;
- NavigationMap + NavigationRuntimePlanner produce a local bypass;
- replacement starts from actual current P/V/q/omega;
- ship then crosses a constrained portal and performs exact terminal capture.

## Final composite acceptance target

Newtonian:
- DriftPass selected under Extreme;
- >=15 deg material drift.

Assisted:
- aligned PrecisionTransit selected;
- <=8 deg max slip.

Both:
- one immediate DynamicHazardInvalidated replan;
- positive static/dynamic full-hull conservative clearance;
- <=19 m hull half-width through narrow passage;
- zero tracking-envelope exceed;
- strict terminal P/V/attitude.

## What a green result means

A green result ends synthetic maneuver **behavior** testing.

It demonstrates the composed navigation behavior is coherent enough to move the main quality loop into the game.

It does **not** close remaining architecture/migration work:
- B1 shared influence batching;
- B2 unified objective;
- B3 vehicle-aware topology feasibility;
- B4 route-aligned corridor replacement;
- full B5 Assisted/general physical compiler;
- generalized B6 proof ownership;
- explicit B11 bounded reflex;
- final ordinary-live B7-B10 seam retirement.

## Next after green

Actual NAV STRESS/game:
- accepted route/corridor visualization;
- accepted time-trajectory/tunnel visualization;
- NPC/autopilot execution;
- visual and gameplay evaluation.

Synthetic tests become regression tools, not the primary development loop.

## State protocol

After every state-affecting event, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
