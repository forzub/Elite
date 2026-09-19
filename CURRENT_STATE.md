# Elite — CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv  
**Repository:** `forzub/Elite`  
**Branch:** `main`

## Current navigation focus

Navigation v2 is in physical maneuver-quality validation after the B8/B9/B10
execution seam, PilotSkill bridge and rigid-body actuator model were established.

The active question is no longer "can the follower move the ship?". It is:

> Can the planner-side maneuver family be authored so the exact rigid body,
> control law and PilotSkill can execute it inside the proven corridor without
> the follower becoming a hidden second planner?

Current isolated family matrix:
- StopTurnGo;
- RadiusTurn;
- DriftTurn;
- Newtonian and Assisted;
- expert / competent / rookie;
- same Cobra rigid hull and same 32 m half-width L-corridor.

## Latest exact target-machine evidence

Latest tested checkout:

```text
ca0c5506a9912bf016e4c52c7545ddd35c66e1fb
```

Evidence:
- Stage-12 runtime-planner architecture contract: **PASS**;
- navigation_runtime: **14/15 PASS**;
- new `maneuver_phase_gate`: **PASS**;
- only `maneuver_corner_family_matrix`: **FAIL**.

This checkout is **tested but not accepted** because the corner-family matrix is
red.

### Strong rows

Expert RadiusTurn, both laws:
- completed;
- final P error ~0.45 m;
- final V error ~0.027 m/s;
- final attitude error ~1.86 deg;
- minimum corner speed ~7.99 m/s;
- rigid hull ~17.88 m half-width;
- zero 32 m corridor violation;
- zero tracking-envelope exceed ticks.

Expert DriftTurn, both laws:
- completed;
- final P error ~0.68 m;
- final V error ~0.009 m/s;
- 10 m/s retained;
- ~108 deg body/velocity slip;
- rigid hull ~17.37 m half-width;
- zero corridor violation.

Its remaining strict defect is exit attitude:
```text
final_forward_error_deg ~= 9.67
required <= 5.0
```

### Primary failed row

Expert / Newtonian / StopTurnGo:
```text
completed=0
phases=2
capture_timeout_phases=1
max_capture_overrun_s=6.01
final_pos_error_m=77.90
final_velocity_error_mps=11.31
final_forward_error_deg=95.38
max_hull_required_half_width_m=42.61
max_corridor_violation_m=10.61
tracking_envelope_exceeded_ticks=227
```

Newtonian StopTurnGo times out at all three PilotSkill levels.
Rookie Assisted StopTurnGo also times out. Assisted expert succeeds cleanly.

## Root cause from the latest gate

The production `ManeuverPhaseGate` is behaving correctly. It no longer lets a
capture phase silently advance merely because authored time elapsed.

The exposed defect is maneuver authoring:

1. Newtonian StopTurnGo used a scheduled 2.6 s 180-degree flip.
2. The next phase immediately assumed the hull was ready for aft-main braking.
3. With the actual rigid-body angular execution, that reference was too
   aggressive.
4. The brake phase then reached its nominal endpoint with material P/V/attitude
   error.
5. Post-horizon capture correctly zeroed brake feed-forward; B10 was left only
   the reserved 0.55 m/s2 tracking authority.
6. That reserve is for tracking error, not for repairing a badly authored
   planner maneuver, so capture timed out and the ship drifted out of corridor.

Do **not** fix this by:
- widening the 32 m corridor;
- increasing capture timeout blindly;
- increasing B10 feedback reserve so the follower re-plans by force;
- treating timeout as success.

The already-green rigid-body Newtonian baseline used about 5 s for the same
180-degree flip. That is the physically coherent reference for this fixture.

## Current unverified corrective candidate

Code candidate:

```text
f7e17a1a631157bc4cc8763c226ea73b57adeb23
```

Changes in `ManeuverCornerFamilyMatrixTests.cpp`:

### Newtonian StopTurnGo

Keep total pre-brake travel time and the braking point unchanged:

```text
old:
  approach 2.775 s
  flip     2.600 s
  total    5.375 s

new:
  approach 0.375 s
  flip     5.000 s
  total    5.375 s
```

Therefore geometry and braking location are not being relaxed. Only the
planner-side attitude schedule is made physically executable before the
aft-main burn begins.

### DriftTurn

Replace:
```text
3 s moving rotate + 1 s aligned coast
```

with:
```text
4 s continuous moving attitude recovery
```

The post-arc travel remains exactly 4 s / 40 m. This removes an artificial
reference boundary and gives B10 one continuous slower attitude history.

This candidate has **not** been target-machine validated yet.

## Architectural invariants

- Planner/B5-B7 owns physical maneuver family and actuator strategy.
- B8 freezes the exact accepted maneuver program.
- B9 samples it.
- B10 only adds bounded tracking feedback.
- ManeuverPhaseGate owns phase handoff, not maneuver generation.
- ScheduledMoving may advance by authored horizon.
- StateCapture may advance only after real terminal capture.
- A capture timeout is a failed maneuver, not vehicle capability.
- Newtonian main-engine braking may depend on prior hull orientation; the
  planner must author enough lead-rotation time.
- No corridor/tolerance weakening is allowed to hide bad maneuver authoring.

## Next target-machine gate

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected:
- architecture PASS;
- 15/15 runtime tests;
- expert Newtonian StopTurnGo no capture timeout and real near-stop;
- expert DriftTurn final attitude <=5 deg;
- RadiusTurn remains unchanged/green;
- no expert 32 m hull-corridor violation.

If StopTurnGo still fails, inspect phase-specific terminal P/V/attitude/omega
rather than extending timeout.

If Drift still exceeds 5 degrees, the remaining issue is angular reference /
tracking recovery and should be isolated there.

## After this matrix is green

1. compose 3-4 mixed-angle 3D corridor;
2. measure real family route time from common physical gates;
3. add doctrine/speed requirements: Rational / Freestyle / Extreme;
4. feed validated candidates toward B6 continuous proof / B7 selection;
5. later expose the same accepted route as manual guidance, never through a
   second presentation-only planner.

## Documentation invariant

After every state-affecting iteration synchronize:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- active Stage-12 documentation.

Recreate `CONTINUE_PROMPT.md` **from scratch every iteration** from current
truth. The newly recreated prompt must itself repeat this same recreation rule.

Always distinguish:
- latest target-machine tested checkout;
- accepted baseline;
- newer unverified code/documentation HEAD.
