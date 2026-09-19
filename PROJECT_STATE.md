# Elite — PROJECT STATE

**Updated:** 2026-09-20 Europe/Kyiv  
**Repository:** `forzub/Elite`  
**Branch:** `main`

This file is the compact project-level navigation state. Detailed historical
gate chronology remains in `src/game/navigation/STAGE12_END_TO_END.md`.

## Navigation v2 architecture

Canonical execution ownership:

```text
world / objective
  -> topology + local corridor
  -> B5 physical maneuver compiler
  -> B6 continuous capability/geometry proof
  -> B7 maneuver decision
  -> B8 AcceptedManeuverProgram
  -> B9 sampler
  -> B10 bounded tracking
  -> PilotSkillExecutor
  -> propulsion / rigid-body physics
  -> replication
```

Planner and follower are deliberately separate:
- planner chooses route and physical maneuver family;
- follower executes the exact accepted program with bounded feedback;
- follower must not invent another rotate/burn/route strategy;
- safety monitoring may invalidate execution and wake planning but is not a
  hidden route planner.

Navigation calculation uses the sealed NavLocal/System boundary and narrow
static-query API established during Stage 12. Legacy route stacks remain
non-authoritative.

## Established evidence

Already demonstrated in prior target-machine gates:
- B8/B9 program/sampler seam;
- B10 tracking controller;
- event-driven accepted execution;
- PilotSkill separation;
- rigid-body Cobra hull and actuator truth;
- Newtonian vs Assisted low-level control-law difference;
- exact-static safety ownership;
- dynamic candidate broadphase + exact OBB narrow phase;
- moving-gap/static proof infrastructure;
- shared client/server authoritative execution path.

The rigid-body baseline proves that Newtonian aft-main braking requires a real
hull flip while Assisted can use fore/reverse longitudinal main authority.

## Active milestone — maneuver-family quality

Current matrix:
```text
3 pilots
x 2 control laws
x 3 corner families
= 18 deterministic rows
```

Families:
- StopTurnGo;
- RadiusTurn;
- DriftTurn.

Common fixture:
- 90-degree L-corridor;
- 32 m half-width;
- Cobra logical hull 26 x 5 x 22.2 m;
- initial speed 10 m/s;
- common entry/exit/corner-zone geometry.

### Latest tested checkout

```text
ca0c5506a9912bf016e4c52c7545ddd35c66e1fb
```

Evidence:
- architecture PASS;
- navigation_runtime 14/15 PASS;
- ManeuverPhaseGate PASS;
- corner-family matrix FAIL.

What is already strong:
- expert RadiusTurn is accurate, fast and corridor-safe in both laws;
- expert DriftTurn has accurate P/V tracking, correct high slip and zero
  corridor violation;
- PilotSkill levels create materially different execution envelopes.

What remains wrong:
- Newtonian StopTurnGo is under-authored before the braking burn and times out;
- expert DriftTurn exit attitude is ~9.67 deg vs <=5 deg.

### Current corrective candidate

```text
f7e17a1a631157bc4cc8763c226ea73b57adeb23
```

Unverified changes:
- Newtonian StopTurnGo uses 5.0 s lead rotation but preserves the same 5.375 s
  pre-brake travel time and same brake point;
- DriftTurn uses one continuous 4.0 s / 40 m post-arc attitude recovery.

No thresholds, hull dimensions or corridor width were relaxed.

## Production mechanism added during this milestone

`ManeuverPhaseGate` separates:
- `ScheduledMoving`: program-time handoff for genuinely continuous moving
  internal phases;
- `StateCapture`: terminal P/V/attitude/omega capture before handoff;
- explicit bounded timeout rather than silent phase advance.

The isolated mechanism test is green on the latest target machine. Current
failure therefore belongs to reference/maneuver authoring, not to the gate API.

## Immediate roadmap

1. target-machine validate `f7e17a1...`;
2. if green, build mixed-angle 3D route from the validated corner primitives;
3. compare actual route times rather than authored schedule sums;
4. exercise Rational / Freestyle / Extreme doctrine-speed requirements;
5. move candidate families through B6 exact continuous proof and B7 selection;
6. integrate the accepted product into live gameplay/manual guidance.

## Non-negotiable quality rules

- physical failures are not hidden by widening corridors;
- capture timeout is not success;
- follower feedback reserve is not planner authority;
- accepted timing must be physical execution timing;
- exact geometry remains collision truth;
- manual guidance visualizes the same accepted navigation product;
- client/server use the same authoritative planning/execution contracts.

## Documentation protocol

After every state-affecting event synchronize:
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and the active
Stage-12 document.

`CONTINUE_PROMPT.md` is recreated from scratch on every iteration and must
always contain the instruction to recreate itself again on the next iteration.
