# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`  
Branch: `main`  
Local checkout: `D:/__elite/work`

## Mandatory first action

Read the current repository truth before changing code:

1. `CURRENT_TASK.md`
2. `CURRENT_STATE.md`
3. `PROJECT_STATE.md`
4. `src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md`
5. `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`
6. `src/game/navigation/STAGE12_END_TO_END.md`

**Important workflow rule:** after every state-affecting iteration, recreate
this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth.
Do not incrementally patch stale prose. The newly recreated prompt must itself
again contain this same instruction, so a new dialog cannot lose the rule.

Also update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md` and
the active Stage-12 document every iteration. Remove obsolete detail when it no
longer helps continuation.

## Current task

We are validating physical corner-maneuver families for Navigation v2 before
building the mixed-angle 3D route and doctrine/speed matrix.

Matrix:
- StopTurnGo / RadiusTurn / DriftTurn;
- Newtonian / Assisted;
- expert / competent / rookie;
- same rigid Cobra;
- same 90-degree L-corridor;
- same 32 m half-width.

The goal is to validate planner-authored physical maneuver references, not to
make the follower hide bad planning.

## Latest exact target-machine result

Tested checkout:

```text
ca0c5506a9912bf016e4c52c7545ddd35c66e1fb
```

Result:
```text
architecture contract       PASS
navigation_runtime          14/15 PASS
maneuver_phase_gate         PASS
maneuver_corner_family_matrix FAIL
```

### Primary failure

Expert / Newtonian / StopTurnGo:
```text
completed=0
phases=2
capture_timeout_phases=1
max_capture_overrun_s=6.01
final_pos_error_m=77.903552
final_velocity_error_mps=11.314457
final_forward_error_deg=95.378736
min_corner_speed_mps=2.012058
max_hull_required_half_width_m=42.605322
max_corridor_violation_m=10.605322
tracking_envelope_exceeded_ticks=227
```

Newtonian StopTurnGo times out for expert, competent and rookie.
Rookie Assisted StopTurnGo also times out.

### Healthy evidence

Expert RadiusTurn is strong in both laws:
- P ~0.45 m;
- V ~0.027 m/s;
- forward ~1.86 deg;
- speed ~7.99 m/s through corner;
- zero corridor violation;
- zero tracking-envelope exceed ticks.

Expert DriftTurn physically succeeds:
- P ~0.68 m;
- V ~0.009 m/s;
- speed 10 m/s;
- ~108 deg slip;
- zero corridor violation.

But Drift expert exit attitude remains:
```text
~9.67 deg
required <=5 deg
```

## Root cause

`ManeuverPhaseGate` is working as designed. Its isolated regression is green.

The failure is planner/reference authoring:
- Newtonian StopTurnGo scheduled a 2.6 s 180-degree flip;
- aft-main braking started immediately after that scheduled horizon;
- the real rigid body had not cleanly captured the required attitude;
- brake terminal capture then inherited a large physical error;
- terminal brake feed-forward is correctly zero after the nominal endpoint;
- B10 has only 0.55 m/s2 reserved linear feedback, which is intentionally too
  small to repair a wrongly authored multi-m/s maneuver;
- StateCapture correctly times out instead of lying about completion.

Do not solve this by increasing timeout, widening the corridor, weakening expert
thresholds, or letting B10 choose a new maneuver.

The previously green rigid-body Newtonian baseline used about 5 s for the same
180-degree flip.

## Current unverified code candidate

```text
f7e17a1a631157bc4cc8763c226ea73b57adeb23
```

### StopTurnGo correction

Pre-brake timing:

```text
old: approach 2.775 + flip 2.600 = 5.375 s
new: approach 0.375 + flip 5.000 = 5.375 s
```

So the braking point and pre-brake travel are unchanged. Only the Newtonian
lead-rotation is made physically executable before the dependent aft-main burn.

Brake remains:
- 1.25 s nominal;
- StateCapture;
- strict terminal P/V/attitude/omega;
- no timeout relaxation.

### Drift correction

Old:
```text
3 s moving attitude recovery + 1 s coast
```

New:
```text
4 s continuous moving attitude recovery
```

Same 4 s and same 40 m post-arc travel. This removes an artificial phase
boundary while preserving path/time geometry.

This candidate has NOT been target-machine accepted.

## Run next

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

If anything fails, preserve the complete 18 CORNER-MATRIX rows and 9
CORNER-COMPARE rows.

## Existing strict expert acceptance

Do not relax:
- completed=true;
- corridor violation = 0;
- final P <=1.5 m;
- final V <=1.0 m/s;
- final forward <=5 deg;
- StopTurnGo min corner speed <=0.75 m/s;
- RadiusTurn min speed >=5 m/s and drift <=20 deg;
- DriftTurn min speed >=7 m/s and drift >=60 deg.

Additional expected result:
```text
StopTurnGo capture_timeout_phases=0
```

## If the next test still fails

If Newtonian StopTurnGo still times out:
- inspect phase-specific terminal position, velocity, forward and angular
  velocity error at timeout;
- determine whether the remaining defect is the flip reference, brake reference
  or PilotSkill command response;
- do NOT extend the timeout blindly.

If Drift remains >5 deg:
- isolate angular recovery reference/tracking;
- preserve speed, 40 m recovery travel and corridor geometry.

If RadiusTurn regresses:
- treat that as a regression; do not trade its current quality for another
  family's pass.

## Architectural ownership

```text
B4 local corridor
 -> B5 physical maneuver alternatives
 -> B6 prove exact alternatives
 -> B7 choose doctrine-appropriate proved maneuver
 -> B8 freeze AcceptedManeuverProgram
 -> B9 sample
 -> B10 track with bounded feedback
 -> PilotSkill
 -> propulsion / physics
```

Planner owns rotate/burn family and timing. Follower does not.

`ManeuverPhaseGate` only decides whether a compound physical phase may hand
off:
- ScheduledMoving -> nominal horizon;
- StateCapture -> actual terminal capture;
- CaptureTimedOut -> explicit maneuver failure.

## After this gate is green

1. compose 3-4 mixed-angle 3D corridor;
2. measure actual common-gate route time;
3. add Rational / Freestyle / Extreme doctrine-speed matrix;
4. pass candidate maneuvers through B6 continuous geometry/capability proof;
5. let B7 select among proved candidates;
6. later show the same accepted route as manual guidance.

Never create a second presentation-only planner.

## State discipline

Always distinguish:
- exact target-machine tested checkout;
- accepted baseline;
- newer unverified code/doc HEAD.

Again: **recreate this prompt from scratch after every iteration, and make the
new prompt repeat this recreation instruction.**
