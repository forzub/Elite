# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

Read first:
1. CURRENT_TASK.md
2. CURRENT_STATE.md
3. PROJECT_STATE.md
4. src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md
5. src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md
6. src/game/navigation/STAGE12_END_TO_END.md
7. src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md

## Accepted baseline

Fresh supplied target-machine evidence:
- architecture PASS;
- navigation_runtime 13/13 PASS;
- rigid-body Cobra corridor PASS;
- production build PASS 84.197 s.

Rigid model is accepted:
- Cobra 26 x 5 x 22.2 m;
- Newtonian braking performs ~180 deg flip and aft-main burn;
- Assisted stays nose-forward and uses fore/reverse main;
- expert Newtonian required 17.275892 m half-width;
- expert Assisted required 13.238202 m.

No rev-parse line was supplied, so no exact tested hash is invented.

## Current experiment

New target:
`maneuver_corner_family_matrix`.

Code/architecture candidate before state docs:
`8f1397a919009d4ed5fdc84412506ba681b3059c`.

Expected navigation_runtime count: 14.

One common 90-degree rigid-hull corridor:
- start/incoming gate (0,0,60);
- mathematical vertex (0,0,0);
- exit gate (60,0,0);
- initial speed 10 m/s;
- final target speed 10 m/s;
- corridor half-width 32 m;
- corner-zone timing gates at +/-35 m.

Three families:
1. StopTurnGo
   - true near-stop;
   - outgoing attitude capture;
   - depart.
2. RadiusTurn
   - R=35 m;
   - nominal 8 m/s;
   - coordinated/tangent body;
   - RCS centripetal acceleration.
3. DriftTurn
   - R=20 m;
   - nominal 10 m/s;
   - deliberate large body/velocity slip;
   - main thrust bends velocity.

Matrix:
3 pilots x 2 laws x 3 families = 18 rows.

Passage rule:
internal phases hand off by program schedule;
corner passage is common entry gate -> common exit gate;
touching the route vertex alone is not completion.

Output:
- 18 CORNER-MATRIX rows;
- 9 CORNER-COMPARE rows comparing Newtonian vs Assisted total/corner time and
  hull half-width for identical pilot/family.

First pass does not hard-code a fastest family.

Strict expert semantic checks:
- all rows cross exit and fit 32 m rigid corridor;
- final P <=1.5 m, V <=1 m/s, forward <=5 deg;
- stop-turn reaches <=0.75 m/s;
- radius keeps >=5 m/s and <=20 deg slip;
- drift keeps >=7 m/s and reaches >=60 deg slip.

## Run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Capture exact HEAD, all CORNER-MATRIX and CORNER-COMPARE rows, plus failures and
timings.

Do not run the 120 s obstacle live gate yet.

After evidence:
- if drift works, accept the primitive and build the requested 3-4 segment 3D
  mixed-angle corridor from these same families;
- compare total route time and hull clearance by law/pilot/family;
- then integrate proved families toward B6/B7.

Every state-affecting iteration must synchronize CURRENT_TASK,
CONTINUE_PROMPT, CURRENT_STATE, PROJECT_STATE and STAGE12_END_TO_END.
