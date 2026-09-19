# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted target baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Accepted:
- Stage-12 architecture PASS;
- navigation runtime 17/17;
- B7 speed/doctrine selection + real selected-program execution;
- previous maneuver, rigid-body corridor and 3D fly-through quality gates.

## Original B0-B14 status

Strong/accepted behavior:
- B0 world truth;
- B7 decision semantics/select->execute;
- B8 AcceptedManeuverProgram;
- B9 sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 real propulsion/physics;
- B14 scheduler.

Strong components but production generalization/integration remains:
- B5 full Assisted/general-family compiler coverage;
- B6 generalized ordinary proof ownership;
- final ordinary-live B7-B10 migration.

Open/transitional architecture:
- B1 scene-wide sparse influence batching;
- B2 unified NavigationObjective;
- B3 vehicle/control-law-aware global edge feasibility;
- B4 route-aligned corridor replacing visibility ray-fan;
- B11 explicit bounded safety-reflex API.

## Current test candidate

`maneuver_chained_limit_matrix`

Purpose:
1. prove physical continuity across different maneuver families without resetting actual state;
2. prove impossible/invalid execution fails closed before or during execution.

Chained route:
- accelerate moving transit;
- hard continuous turn;
- Newtonian drift or Assisted aligned transit;
- precision braking/capture.

Negative contracts:
- insufficient turn horizon -> B5 NoPhysicalCandidate;
- insufficient braking distance -> stopping reserve exceeds room;
- too-narrow rigid hull corridor -> reject;
- no law-compatible B7 candidate -> no selection;
- newly invalidated dynamic safety -> immediate local replan, old program stops being authoritative.

Expected suite size: 18.

## Remaining laboratory roadmap

If this matrix passes:
1. one final composite end-to-end proving ground combining clutter, moving hazards, narrow/wide passages, speed changes, law/doctrine choice, invalidation and exact terminal capture;
2. then stop expanding synthetic behavior tests and move primary quality evaluation into the game.

Architecture cleanup B1-B4/B11 and final production migration remain implementation work, but are no longer reasons to endlessly extend the maneuver laboratory.

## State protocol

After each state-affecting event, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
