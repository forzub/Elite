# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Canonical architecture

```
Navigation geometry / local corridor
 -> physical maneuver compiler
 -> continuous proof
 -> maneuver decision
 -> AcceptedManeuverProgram
 -> sampler
 -> trajectory follower / tracking
 -> PilotSkill
 -> authoritative propulsion + physics
```

Planner owns large-scale maneuver geometry and physical reference. Follower owns bounded residual tracking, not maneuver invention.

## Current maneuver-quality state

Latest verified target run remains 14/15:
- StopTurnGo expert healthy;
- RadiusTurn healthy;
- long 180 deg angular tracking healthy;
- only strict expert failure remains DriftTurn exit attitude.

The constant-heading experiment proved that B10 cannot execute the entire 90 deg exit turn from tracking reserve.

## Current unverified candidate

```
31a66a3eb462df6b5a60b2da2aca9f018d8aa332
```

The DriftTurn exit now uses a planner-authored moving attitude capture whose boundary conditions are the actual arc-exit yaw/yaw-rate and desired outgoing yaw/zero yaw-rate.

The profile horizon is solved from the same effective angular acceleration/rate envelopes enforced by ShipController, with tracking authority reserved for B10.

This is the first candidate that simultaneously:
- respects actual initial angular state;
- keeps translation continuous;
- avoids arbitrary route-time deadlines;
- preserves planner/follower ownership;
- accounts for the real physical envelope rather than only configured maxima.

## Roadmap

1. Validate moving attitude capture.
2. Reach 15/15.
3. Record accepted baseline and closed defect.
4. Add mixed-angle/multi-segment 3D corridor tests.
5. Extend speed/doctrine coverage.
6. Move toward visible in-game evaluation.

## State protocol

After every state-affecting event, synchronize CURRENT_STATE.md, CURRENT_TASK.md, PROJECT_STATE.md, active Stage-12 documentation, and recreate CONTINUE_PROMPT.md from scratch.
