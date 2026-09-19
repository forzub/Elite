# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.
Target: Windows 10 / MSYS2 MinGW64 / g++ 15.2 / CMake + Ninja.

## Read first

1. `CURRENT_TASK.md`
2. `CURRENT_STATE.md`
3. `PROJECT_STATE.md`
4. `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
5. `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`
6. `src/game/navigation/STAGE12_END_TO_END.md`
7. `src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md`

## Canon

Planner World:
B0 world snapshot -> B1 shared influence -> B2 objective -> B3 topology route ->
B4 route-aligned local corridor -> B5 physical maneuver compiler ->
B6 continuous proof -> B7 decision -> B8 AcceptedManeuverProgram.

Autopilot/Follower World:
B9 program sampler -> B10 bounded tracking -> B11 safety/reflex ->
B12 PilotSkill -> B13 propulsion/physics.

B14 schedules planner work. Another physics frame alone must never wake the planner.

Scale target:
- hundreds/thousands registered units;
- no dense N x N;
- shared B0/B1;
- only dirty agents enter B3..B8;
- B9/B10/B12/B13 are cheap fixed-step work for active controlled actors;
- planner work is queued/budgeted and stale revisions are dropped.

## Verified baseline

User supplied target-machine evidence for:

```text
701881ddae861cd5593e425de91600e048bd417c
```

Results:
- navigation_runtime 7/7 PASS;
- maneuver_program_sampler PASS;
- EliteGame BUILD PASS;
- EliteServer BUILD PASS.

B8/B9 is accepted.

## Current candidate — B10

Code candidate before docs:

```text
66d89b93e3edf0817bb8d88b78e405180887cecc
```

New:
- `ManeuverTrackingController.h/.cpp`;
- new TrajectoryFollower overload consuming `AcceptedManeuverProgram`;
- `ManeuverTrackingControllerTests.cpp`;
- architecture contract lock for B8/B9/B10;
- navigation_runtime phase timing output.

Canonical execution path:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> ManeuverTrackingController
 -> NavigationLocalControlIntent
```

Rules:
- zero tracking error => exact A_ff / alpha_ff;
- feedback magnitude <= accepted reserve;
- envelope breach is reported, not replanned inside follower;
- B8/B9/B10 must not depend on NavigationMap, NavigationSpace, GameSimulation,
  NavigationRuntimePlanner, queries or unbounded vectors;
- old AcceptedShortSegment overload remains for live compatibility.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh

TIMEFORMAT='[TIMING] build_mingw64 real_s=%R user_s=%U sys_s=%S'
time bash build_mingw64.sh
```

Expected navigation_runtime count: 8 tests.

The runtime script must print configure/build/tests/total milliseconds.

No log is required unless the gate fails. If a persistent log is created in a later diagnostic pass, print its exact path at the end of the command/script.

Do not claim B10 acceptance until the user supplies this target-machine evidence.

## After B10 green

Next implementation slice: B14 Navigation Work Scheduler.
Define clean queue/scheduler API first, then implementation:
- `NavigationPlannerJob` with actor/objective/world/capability revisions;
- urgent / normal / background priority;
- stale-job rejection;
- bounded per-slice job budget;
- fairness/age promotion;
- no duplicate active job for same actor+objective revision;
- deterministic tests with hundreds/thousands of synthetic actors;
- print timing diagnostics, but do not use brittle wall-clock thresholds as correctness criteria.

After B14 is accepted, migrate the live GameSimulation ACCEPT seam from
AcceptedShortSegment to AcceptedManeuverProgram in its own target-machine-gated slice.

## Every state-affecting iteration

- rewrite this file completely;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical architecture/migration docs if ownership changes;
- keep verified target-machine baselines separate from unverified HEAD.
