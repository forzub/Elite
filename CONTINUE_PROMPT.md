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

## Accepted evidence

Revised B5 is green by supplied target-machine evidence:
- navigation_runtime 10/10;
- RCS-feasible request still exposes main-engine B7 option;
- B5 10000 compiles = 30,824 us;
- scheduler 5000 = 2,670 us;
- production build = 23.861 s.

No rev-parse line was supplied; do not invent tested hash.

## Current experiment

Before B6, test actual execution of pre-authored accepted movement programs.

New target:
`maneuver_program_execution_lab`.

Execution is real:
AcceptedManeuverProgram -> B9/B10 -> PilotSkill -> SharedShipPhysics ->
DynamicMotionSystem.

Scenarios:
1. 100 m straight stop-to-stop;
2. 100 m -> stop -> 90-degree yaw -> 100 m;
3. same route inside 5 m half-width corridor.

Print:
- final position error;
- final speed;
- cross-track;
- overshoot;
- corner error;
- corridor violation;
- simulated completion time;
- arrival classification.

First profile has zero PilotSkill reaction delay/latency to isolate autopilot and
physics.

Do not weaken the first thresholds before seeing target-machine metrics.

## Run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected runtime count: 11 tests.

Do not run the long 120 s obstacle live gate in this experiment.

## Interpretation

- straight failure -> fix follower/PilotSkill/physics terminal execution first;
- straight PASS + 90-degree failure -> fix program transition/attitude handling;
- both PASS -> repeat under realistic pilot latency, then proceed to B6;
- later add a non-stop rounded/physical 90-degree corner fixture.

Every state-affecting iteration must synchronize CURRENT_TASK,
CONTINUE_PROMPT, CURRENT_STATE, PROJECT_STATE and STAGE12_END_TO_END.
