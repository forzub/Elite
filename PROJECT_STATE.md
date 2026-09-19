# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Canonical B0-B14 state

### Strong/accepted
- B0 world truth publication / exact geometry;
- B8 AcceptedManeuverProgram;
- B9 sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 propulsion + authoritative physics;
- B14 deterministic scheduler.

### Strong mechanics but incomplete production generalization
- B5 physical compiler: Newtonian ordinary slice exists; Assisted/general family completion remains;
- B6 continuous proof: strong exact-static/moving precision components, not yet one generalized ordinary block;
- B8-B10 ordinary-live migration: lab/runtime path accepted; final compatibility-seam retirement remains integration work.

### Open/transitional
- B1 shared scene-wide influence batching;
- B2 unified NavigationObjective;
- B3 vehicle-aware global edge feasibility;
- B4 route-aligned corridor replacing ray-fan search;
- B7 real doctrine selection in execution chain;
- B11 explicit bounded safety reflex.

## Last accepted physical quality baseline

Exact checkout:
```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Accepted:
- architecture PASS;
- runtime 16/16;
- StopTurnGo / RadiusTurn / DriftTurn;
- long angular tracking;
- rigid-body law-specific braking;
- 3D stop-to-stop corridor;
- continuous multi-corner 3D fly-through.

## Current candidate: B7 speed/doctrine matrix

A new test constructs six different proved-program fixtures for the same objective and feeds their real metrics into `ManeuverDecisionController`.

Doctrine intent:
- Rational -> balanced;
- PrecisionRetrieval -> highest-clearance precision;
- Extreme -> fastest law-compatible;
- CombatEscape -> lowest threat;
- hard critical-risk envelope remains above all doctrines.

Control-law intent:
- Newtonian may select a high-slip drift dash;
- Assisted must filter that Newtonian-only candidate before ranking and choose the next valid fast candidate.

After selection, the chosen AcceptedManeuverProgram is actually executed through follower/PilotSkill/physics and checked for tracking, hull clearance and terminal state.

Expected suite size after registration: 17.

## Test roadmap after B7

1. chained transitions between different maneuver families;
2. negative / physical-limit cases:
   - insufficient turn room;
   - insufficient braking distance;
   - too-narrow hull corridor;
   - incompatible control-law candidate;
   - new obstacle/invalidation;
3. final composite end-to-end proving ground;
4. then move primary evaluation into the game.

## State protocol

After every state-affecting event, synchronize project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
