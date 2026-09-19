# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Canonical execution architecture

```
Navigation geometry / corridor
 -> physical maneuver compiler
 -> continuous proof
 -> maneuver decision
 -> AcceptedManeuverProgram
 -> sampler
 -> trajectory follower / bounded tracking
 -> PilotSkill
 -> authoritative propulsion / physics
```

Planner owns the physical reference and maneuver. Follower owns bounded residual tracking, not maneuver invention.

## Accepted baseline

Exact target-machine checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

Evidence:
- architecture PASS;
- navigation runtime 15/15;
- strict expert StopTurnGo, RadiusTurn, DriftTurn healthy;
- long-arc angular tracking healthy.

The accepted DriftTurn fix established that large attitude transitions must be planner-authored from real angular state and capability.

## Current unverified stage

Continuous multi-corner 3D fly-through.

Candidate:
- new `ManeuverFlyThrough3dTests.cpp`;
- registered `maneuver_fly_through_3d` CTest;
- expected runtime suite size 16.

### Geometry

One 5-segment 3D route with ~35/60/90/120 degree turns.

### Motion

- nominal speed 8 m/s;
- no stop at corner;
- C2 quintic moving corner references;
- body attitude follows route tangent;
- angular feed-forward derived from orientation evolution.

### Rigid-body proof

Cobra OBB hull corners are measured against a 32 m route corridor.

The test reports per-corner observed physical radius, speed loss, slip and hull envelope so Newtonian/Assisted behavior can be compared from evidence.

## Current question

Can the accepted planner/follower/physics stack execute chained 3D turns without collapsing to stop-turn-go and without leaving the rigid-body corridor?

No assumption is made that Newtonian or Assisted must perform better. The first run establishes the measured difference.

## Roadmap after this gate

If fly-through is healthy:
1. characterize Newtonian vs Assisted continuous-turn envelopes;
2. add speed/doctrine matrix;
3. reconcile doctrine terminology (Rational / Precision / Extreme / CombatEscape vs older Freestyle references);
4. proceed toward visible in-game evaluation.

## State protocol

After every state-affecting event, update project MD context and recreate `CONTINUE_PROMPT.md` from scratch.
