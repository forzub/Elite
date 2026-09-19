# Elite — CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv
**Branch:** `main`

## Accepted rigid-body baseline

Fresh supplied target-machine evidence:
- architecture contract PASS;
- navigation_runtime 13/13 PASS;
- maneuver_rigid_body_corridor PASS;
- production build PASS, 84.197 s.

Accepted physical observations:
- expert Newtonian:
  - final error 0.346002 m;
  - final speed 0.399834 m/s;
  - final forward 179.996705 deg from route;
  - max hull half-width 17.275892 m;
  - max flip 179.999972 deg;
  - aft-main brake 8.325947 m/s2;
  - fore-main brake 0;
- expert Assisted:
  - final error 0.346002 m;
  - final speed 0.399834 m/s;
  - final forward 0 deg;
  - max hull half-width 13.238202 m;
  - no flip;
  - fore-main brake 8.326298 m/s2.

This confirms the rigid-body/actuator model and the wider Newtonian flip
envelope.

The supplied log contains no `git rev-parse HEAD` line, so do not invent an
exact tested checkout hash.

## Current task — corner maneuver families and timing

A mathematical route vertex does not have one universal "passed" condition.

New target:

```text
maneuver_corner_family_matrix
```

Current code/architecture candidate before state-document commits:

```text
8f1397a919009d4ed5fdc84412506ba681b3059c
```

Expected navigation_runtime CTest count:

```text
14
```

## Common corridor

First isolate one canonical 90-degree corner.

```text
incoming gate
(0,0,60)
   |
   | 60 m
   v
vertex (0,0,0)
   +----------------> outgoing gate (60,0,0)
                  60 m
```

Common conditions:
- initial velocity = 10 m/s toward the vertex;
- final target velocity = 10 m/s along outgoing leg;
- initial body forward = incoming direction;
- final body forward = outgoing direction;
- rigid Cobra Mk1 hull 26 x 5 x 22.2 m;
- common corridor half-width = 32 m;
- corner timing entry gate = 35 m before vertex;
- corner timing exit gate = 35 m after vertex.

All families therefore have the same physical start, exit gate and hull
clearance test.

## Family 1 — StopTurnGo

Meaning:

```text
approach
 -> capture vertex
 -> speed approximately zero
 -> acquire outgoing attitude
 -> accelerate out
```

"Corner passed" is NOT touching the vertex.

For this family the semantic requirement is:
- near-zero speed is actually achieved inside the corner zone;
- outgoing gate is later crossed;
- final P/V/attitude is inside the common exit envelope.

Newtonian is allowed/expected to use its physical flip/aft-main strategy where
needed.
Assisted may use fore/reverse longitudinal thrust.

## Family 2 — RadiusTurn

Meaning:

```text
approach
 -> reduce only as much speed as physical lateral authority requires
 -> continuous proved arc
 -> exit without stopping
```

Current isolated fixture:
- radius = 35 m;
- nominal arc speed = 8 m/s;
- body stays approximately tangent to velocity;
- centripetal acceleration is within the 2 m/s2 RCS envelope.

Semantic quality:
- minimum corner-zone speed >= 5 m/s;
- maximum body/velocity slip <= 20 deg;
- common exit gate crossed.

## Family 3 — DriftTurn

Meaning:

```text
approach at speed
 -> rotate hull away from velocity
 -> use physical thrust to bend V
 -> retain material translation through corner
 -> recover outgoing attitude
```

Current isolated fixture:
- radius = 20 m;
- nominal speed = 10 m/s;
- hull is deliberately near 90 deg to velocity during the powered arc;
- aft-main thrust supplies most centripetal acceleration.

Semantic quality:
- minimum corner-zone speed >= 7 m/s;
- body/velocity slip reaches >=60 deg;
- common exit gate crossed.

This answers the concrete question: can the current follower/PilotSkill/physics
actually execute a drift corner without cheating on thrust direction or hull
geometry?

## Passage semantics

Internal test phases are pieces of one conceptual accepted maneuver.

They hand off by scheduled program time. We explicitly do NOT use
`Follower::Complete` for a moving internal waypoint because that is terminal
capture semantics.

Externally meaningful corner passage:

```text
entry-gate crossing
        ->
rigid-body maneuver inside common corridor
        ->
exit-gate crossing with valid exit state
```

Touching the mathematical vertex alone never counts.

## Matrix

Pilots:

```text
expert
competent  (current production NPC baseline)
rookie
```

Laws:

```text
Newtonian
Assisted / aircraft-like
```

Families:

```text
StopTurnGo
RadiusTurn
DriftTurn
```

Total:

```text
3 pilots x 2 laws x 3 families = 18 rows
```

## Metrics

Every `[CORNER-MATRIX]` row prints:
- valid/completed;
- phase count;
- total corridor time;
- corner-zone time;
- final position error;
- final velocity error;
- final forward/attitude error;
- minimum corner speed;
- maximum drift/slip angle;
- center cross-track;
- rigid-hull required half-width;
- common 32 m corridor violation;
- aft-main peak;
- fore-main peak;
- RCS peak;
- tracking-envelope exceeded ticks.

Additionally nine `[CORNER-COMPARE]` rows print, for the same pilot and family:
- Newtonian total time;
- Assisted total time;
- Assisted - Newtonian time delta;
- Newtonian corner-zone time;
- Assisted corner-zone time;
- Newtonian required half-width;
- Assisted required half-width.

No winner/ranking is hard-coded. First target-machine run establishes the actual
timing/clearance result.

## First-pass strict expert checks

All six expert law/family rows must:
- remain valid;
- cross the common exit gate;
- stay inside common 32 m rigid-hull corridor;
- finish within 1.5 m position error;
- finish within 1.0 m/s velocity error;
- finish within 5 deg outgoing attitude error.

Family semantics:
- StopTurnGo: corner-zone speed <=0.75 m/s at some point;
- RadiusTurn: min corner speed >=5 m/s, slip <=20 deg;
- DriftTurn: min corner speed >=7 m/s, slip >=60 deg.

Competent/rookie remain diagnostic on the first run.

## Why timing may or may not differ at one 90-degree corner

Do not force a difference.

At a symmetric 90-degree corner some Newtonian and Assisted maneuvers can have
similar passage time even though attitude/thrust histories and required corridor
width differ.

After this primitive is measured, compose the same accepted families into the
requested 3-4 segment 3D corridor with mixed turn angles. Mixed angles are where
Newtonian reorientation history can create a larger timing difference:
a Newtonian ship may arrive at a vertex tail-forward after braking, while an
Assisted ship may remain nose-forward.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Do not run the old 120 s obstacle live gate yet.

Capture:
- exact HEAD;
- all 18 `[CORNER-MATRIX]` rows;
- all 9 `[CORNER-COMPARE]` rows;
- any FAIL/warning;
- timing summary.

## Next after target-machine evidence

If expert drift executes:
- accept drift as an executable physical corner primitive;
- compare actual stop/radius/drift time and hull width;
- compose all three families into a 3-4 segment 3D corridor;
- then feed those physically distinct candidates toward B6 proof/B7 selection.

If drift fails:
- do not loosen criteria;
- use P/V/attitude/actuator/clearance output to identify whether the failure is
  angular timing, main-thrust direction, tracking reserve or corridor geometry.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update canonical architecture/migration/purity docs when ownership/contracts
  change.
