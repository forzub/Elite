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

Planner owns route/corridor, maneuver family, physical reference and proof. Follower tracks the accepted result with bounded feedback/safety response and may not replace maneuver strategy.

## Current verified state

Latest exact target-machine checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

- architecture PASS;
- runtime 14/15;
- StopTurnGo expert healthy;
- RadiusTurn healthy;
- long 180 deg continuous angular tracking healthy;
- remaining failure localized to DriftTurn test-flow termination semantics.

The rejected timed-settle experiment showed that forcing a faster angular schedule to meet the old x=60 deadline worsens the residual instead of solving it.

## Current candidate

```
24fce76b30d2448a4b94c93ad78dd8d37e5102df
```

The corner-family fixture no longer treats x=60 as the DriftTurn control terminus. Drift exits the arc into a moving straight reference at 10 m/s with the final corridor heading and zero target angular rate. Tracking continues beyond the old checkpoint until the finite extended test corridor ends at x=120.

The test records the first moving attitude capture (<=5 deg forward error and <=0.08 rad/s angular speed) and preserves the existing P/V/corridor/speed/slip quality gates.

No production tracking authority or tolerance was relaxed.

## Roadmap

1. Validate candidate and close current corner-family defect.
2. If 15/15, document exactly what was closed.
3. Design/add mixed-angle, multi-segment 3D corridor quality test.
4. Extend speed/doctrine coverage.
5. Proceed toward visible in-game evaluation.

## State protocol

After every state-affecting event, synchronize `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate `CONTINUE_PROMPT.md` from scratch.
