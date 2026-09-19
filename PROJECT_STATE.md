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

## Current verified maneuver-quality milestone

Latest exact target-machine checkout:

```
d659416b9b1ddb2356c37eff315f9d13b70bafaa
```

Result:
- architecture PASS;
- navigation runtime 14/15;
- StopTurnGo expert defect fixed;
- RadiusTurn healthy;
- long 180 deg arc healthy across expert/competent/rookie and both laws;
- only strict expert failure: DriftTurn final attitude ~10.325 deg vs <=5 deg.

The long arc materially narrows the diagnosis: B9/B10 continuous angular tracking is healthy. Expert tracks ~251 m / 25.1 s of curved flight with only 3.221 deg maximum in-flight angular error and 0.052 deg final error.

Therefore the remaining defect is local DriftTurn recovery/reference authoring.

## Current unverified candidate

```
76346121516e5b00d14a4e6304621b55791093ab
```

DriftTurn now uses one coherent 4 s / 40 m recovery reference with 2.5 s smooth rotation followed by 1.5 s of continued 10 m/s translation at the final yaw. This adds the needed in-motion attitude settle without moving maneuver strategy into the follower.

Do not:
- widen the 5 deg requirement;
- widen corridor;
- increase generic feedback reserve merely to force green;
- add follower-side strategy selection.

## Roadmap after DriftTurn is green

1. Re-run corner-family + long-arc gates.
2. Add mixed-angle / multi-segment 3D corridor quality tests.
3. Extend speed/doctrine coverage.
4. Move the proven planner/follower behavior into visible game evaluation.

## State protocol

After every state-affecting event, synchronize `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate `CONTINUE_PROMPT.md` from scratch.
