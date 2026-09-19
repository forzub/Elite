# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`

## Fresh accepted execution evidence

Supplied target-machine run:
- architecture contract PASS;
- navigation_runtime 12/12 PASS;
- maneuver_corridor_matrix PASS;
- maneuver_program_execution_lab PASS;
- ordinary_physical_maneuver_compiler PASS;
- navigation_work_scheduler PASS.

### 4-leg 3D matrix observations

Expert:
- Newtonian: 4/4 legs, 3/3 rotations, final error 0.014967 m,
  final speed 0.096233 m/s, zero 5 m corridor violation;
- Assisted/aircraft-like: same measured calm-route result.

Production-competent:
- both laws: 4/4 legs, 3/3 rotations;
- final error 0.111386 m;
- final speed 0.078158 m/s;
- zero 5 m corridor violation.

Rookie:
- both laws: only first leg completes;
- no attitude transition completes;
- zero corridor violation before failure;
- maximum forward-angle error 19.115652 degrees.

The calm corridor route does not reach a state where Newtonian and Assisted
propulsion semantics diverge, so identical metrics are expected and are not by
themselves sufficient evidence that both laws are materially exercised.

## Control-law naming

Two local flight laws:

```text
Newtonian
Assisted  == aircraft-like / "самолётный"
```

For navigation acceleration execution:
- Newtonian: ordinary main propulsion remains in controlled-speed envelope,
  while physical RCS can keep accumulating inertial delta-v;
- Assisted: combined controlled propulsion is limited by the controlled-speed
  envelope.

Planner-side ordinary B5 is still Newtonian-only. Full Assisted physical
maneuver compilation is not yet implemented.

## Current candidate — explicit law stress

Code/contract candidate before documentation commits:

```text
88ad3a8921239cf2865c32c0c8711b7514094aa1
```

The existing `maneuver_corridor_matrix` target now also runs a dedicated
control-law seam before the 6-row corridor matrix.

Law-stress setup:
- expert PilotSkill;
- hull remains fixed;
- lateral RCS demand = 2.0 m/s2;
- controlled-speed envelope = 10 m/s;
- duration = 8 s;
- same acceleration command in both laws.

Expected physical result:

```text
Newtonian
  -> RCS continues accumulating delta-v
  -> final speed > 12 m/s

Assisted / aircraft-like
  -> combined controlled motion clips at envelope
  -> final speed <= 10.05 m/s

difference > 2 m/s
```

The test fails if Newtonian and Assisted remain physically indistinguishable.

Expected diagnostic rows:

```text
[LAW-STRESS] law=newtonian final_speed_mps=... max_speed_mps=... final_x_m=...
[LAW-STRESS] law=assisted final_speed_mps=... max_speed_mps=... final_x_m=...
```

The six existing `[CORRIDOR-MATRIX]` rows still run afterward.

The obsolete unused `finite(vec3)` warning in the matrix test was removed.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected CTest count remains **12** because law-stress is inside
`maneuver_corridor_matrix`.

Capture:
- both `[LAW-STRESS]` lines;
- all six `[CORRIDOR-MATRIX]` lines.

## Interpretation

If law-stress passes:
- the same accepted-program execution stack genuinely exercises two distinct
  flight laws;
- calm-corridor equality is simply because both are far inside their common
  physical envelope.

If law-stress fails:
- investigate DynamicMotionSystem/control-law selection before using this
  matrix as two-mode evidence.

After execution-law seam is accepted:
- quantify rookie attitude timing;
- then proceed toward B6;
- separately implement Assisted/aircraft-like planner-side B5 before claiming
  full planner parity between modes.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update canonical architecture/migration/purity docs when ownership changes.
