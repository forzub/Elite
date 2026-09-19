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

Planner owns route, corridor, maneuver family, physical reference and proof. Follower tracks the accepted result and may apply bounded feedback/safety response but may not replace the maneuver strategy.

Newtonian and Assisted laws are separate physical families. Manual guidance will expose the same accepted trajectory/corridor.

## Established Stage-12 state

The live Navigation v2 path, exact HitVolume static geometry, moving-passage composition, typed frame transforms, replication truth and planner/follower seams are already covered by accepted architecture/runtime gates. Legacy route stacks are non-authoritative.

## Current maneuver-quality milestone

Latest exact target-machine checkout:

```
be4686f4dcba419f451813b1ddc246088c145e48
```

Result:
- architecture PASS;
- navigation runtime 14/15;
- StopTurnGo expert defect fixed;
- RadiusTurn healthy;
- remaining strict expert failure: DriftTurn final attitude ~10.325 deg vs <=5 deg requirement.

Current unverified code candidate:

```
5c16bedc25c422f2c79ae5def14396839ef4ee7c
```

It adds a 180 deg, R=80 m, 10 m/s long-arc diagnostic (~251 m) to determine whether the attitude miss is general B9/B10 angular tracking or local DriftTurn recovery authoring.

No production tolerance, corridor width or physical capability was weakened.

## Roadmap after this split

1. Fix the identified angular mechanism without hiding it behind tolerance.
2. Re-run corner-family + long-arc gates.
3. Add mixed-angle / multi-segment 3D corridor quality tests.
4. Extend speed-doctrine coverage (Rational / Precision / Extreme / CombatEscape/Freestyle-equivalent behavior as the contract requires).
5. Move the proven planner/follower behavior into visible game evaluation.

## State protocol

After every state-affecting event, synchronize `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate `CONTINUE_PROMPT.md` from scratch.
