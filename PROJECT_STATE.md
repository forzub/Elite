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

Planner owns route/corridor, maneuver family, physical reference and proof. Follower tracks the accepted result with bounded feedback and must not replace maneuver strategy.

## Current maneuver-quality state

Latest target evidence:
- architecture PASS;
- runtime 14/15;
- StopTurnGo expert healthy;
- RadiusTurn healthy;
- long 180 deg continuous angular tracking healthy;
- DriftTurn exit attitude remains the only strict expert corner-family failure.

The latest experiment decoupled x=60 from phase termination and extended the outgoing reference to x=120. DriftTurn still did not converge:
- expert final attitude ~48.287 deg;
- 439 tracking-envelope exceeded ticks;
- no outgoing attitude capture.

This experiment closes an important misconception: the follower's bounded tracking controller cannot be substituted for planner-authored large-angle maneuver dynamics.

## Current diagnosis

The remaining defect is in DriftTurn exit trajectory authoring.

A 90 deg transition must be represented as a physically feasible angular reference/feed-forward program. B10's `angularFeedbackReserveRadPerSec2` is only residual authority and is intentionally too small to own the maneuver.

The next implementation should create a moving attitude-capture primitive that computes its angular profile/horizon from current state and vehicle capability while translation continues along the outgoing route.

## Roadmap

1. Implement and validate moving attitude capture for DriftTurn exit.
2. Reach 15/15 corner-family/runtime gate.
3. Record the closed defect and accepted evidence.
4. Add mixed-angle/multi-segment 3D corridor quality tests.
5. Extend speed/doctrine coverage.
6. Proceed toward visible in-game evaluation.

## State protocol

After every state-affecting event, synchronize `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, active Stage-12 documentation, and recreate `CONTINUE_PROMPT.md` from scratch.
