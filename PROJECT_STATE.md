# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Canonical architecture

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

Planner owns physical maneuver/reference generation. Follower owns bounded residual tracking.

## Accepted quality baseline

Exact target-machine checkout:

```
213bbfb62ff7dcb8e553c06bdca09d99d2d1fd56
```

Accepted:
- Stage-12 architecture PASS;
- navigation runtime 16/16;
- StopTurnGo / RadiusTurn / DriftTurn strict expert gates;
- long continuous angular tracking;
- rigid-body braking/corridor behavior;
- non-orthogonal stop-to-stop 3D corridor;
- continuous multi-corner 3D fly-through.

## Newly closed capability

The planner/follower/physics stack can execute one continuous 5-segment 3D route through ~35/60/90/120 degree turns while:
- retaining nonzero fly-through speed;
- preserving strict expert tracking;
- keeping full Cobra OBB inside a 32 m half-width corridor;
- completing with bounded P/V/attitude error.

This is stronger than the earlier 3D corridor matrix because the vehicle does not stop and rotate at every waypoint.

## Remaining diagnostic work

The accepted target log did not contain verbose `FLY3D` rows because the runner did not yet include the new test in its diagnostic reruns.

Runner-only patch:

```
4cd9c4a14c8f2e4ce033082633766a21fece9331
```

adds verbose FLY3D output without altering navigation behavior.

Need one target diagnostic run to quantify whether Assisted actually requires more radius/speed loss than Newtonian on the same accepted geometry.

## Roadmap

1. Capture detailed Newtonian/Assisted FLY3D metrics.
2. Record measured turn-envelope comparison.
3. Add speed/doctrine matrix.
4. Reconcile doctrine terminology:
   - Rational;
   - Precision;
   - Extreme;
   - CombatEscape;
   - compare with older “Freestyle” wording.
5. Proceed toward visible in-game evaluation.

## State protocol

After each state-affecting event, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
