# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted target-machine checkout

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 17/17 PASS;
- B7 speed/doctrine select->execute gate PASS.

## Accepted maneuver stack

Now demonstrated:
- StopTurnGo;
- RadiusTurn;
- DriftTurn;
- long continuous angular correction;
- rigid-body Newtonian vs Assisted braking;
- non-orthogonal 3D route execution;
- continuous 3D fly-through;
- doctrine-dependent candidate selection;
- law filtering before ranking;
- selected AcceptedManeuverProgram execution through real follower/PilotSkill/physics.

## B7 acceptance

For one common 180 m problem:
- Rational selected balanced;
- PrecisionRetrieval selected highest-clearance precision;
- Extreme selected fastest **law-compatible** program;
- CombatEscape selected lowest-threat program;
- hard critical-risk filtering rejected an even faster reckless shortcut.

The key law divergence is now physically visible:
- Newtonian Extreme -> ~18 s drift dash, ~14.11 m/s peak, ~34.08 deg slip, ~2.62 m actual clearance;
- Assisted Extreme -> ~20 s fast aligned path, ~12.28 m/s peak, ~2.08 deg slip, ~7.59 m clearance.

This closes B7 behavior at lab/runtime level.

Final ordinary-live migration still must route normal production planning through the selected program rather than transitional bypasses.

## B0-B14 status

Strong/accepted behavior:
- B0, B7, B8, B9, B10, B12, B13, B14.

Strong components but still requiring production generalization:
- B5, B6;
- final B7-B10 ordinary-live integration.

Open/transitional architecture:
- B1 shared influence batching;
- B2 unified objective;
- B3 vehicle-aware global topology feasibility;
- B4 route-aligned corridor replacing ray-fan;
- B11 explicit bounded reflex.

## Current laboratory stage

**Chained transitions + negative/physical-limit cases.**

Target evidence must cover:
- physical continuity across different maneuver families;
- no artificial state reset between accepted programs;
- insufficient turn room;
- insufficient braking room;
- rigid hull cannot fit corridor;
- no compatible candidate for current control law;
- program invalidation when new world evidence breaks its proof.

After this matrix:
- run one final composite end-to-end proving ground;
- then move the primary quality loop into the real game scene.

## State protocol

After every state-affecting event, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
