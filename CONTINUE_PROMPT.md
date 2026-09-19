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

## Accepted state

Revised B5 is green by supplied target-machine evidence:
- navigation_runtime 10/10;
- main-engine alternative exists even when RCS trim is feasible;
- B5 10,000 compiles = 30,824 us;
- scheduler 5000 jobs = 2,670 us;
- production build = 23.861 s.

No rev-parse line was supplied; do not invent the tested hash.

## Current experiment

Current execution-lab candidate before docs:
`78fd1e356138f94f6e6b8990053d80fdc419eb4d`.

The experiment bypasses planner/world search and tests:

```text
AcceptedManeuverProgram
 -> B9/B10
 -> PilotSkill
 -> SharedShipPhysics
 -> DynamicMotionSystem propulsion + translation
```

Scenarios:
1. 100 m straight, stop-to-stop;
2. 100 m -> stop -> yaw 90 deg -> 100 m;
3. same path within 5 m half-width corridor.

Metrics:
- arrival class;
- final position error;
- final speed;
- cross-track;
- overshoot;
- corner error;
- corridor violation;
- simulated completion time.

First profile is expert/zero-latency to isolate autopilot and physics.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected CTest count: 11, including
`maneuver_program_execution_lab`.

The lab may fail. Do not change thresholds before reading the movement metrics.

Do not run the long 120 s obstacle live gate yet.

## After results

- straight fail -> fix execution stack first;
- straight pass / corner fail -> fix program transition or attitude tracking;
- both pass -> repeat with realistic pilot latency, then B6;
- later add a continuous/non-stop 90-degree path.

Every state-affecting iteration must synchronize CURRENT_TASK,
CONTINUE_PROMPT, CURRENT_STATE, PROJECT_STATE and STAGE12_END_TO_END.
