# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`
**Canonical architecture:** `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
**Migration audit:** `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`

## Last verified baseline

```text
701881ddae861cd5593e425de91600e048bd417c
```

Verified on target machine:
- navigation_runtime 7/7 PASS;
- maneuver_program_sampler PASS;
- EliteGame BUILD PASS;
- EliteServer BUILD PASS.

B8/B9 is accepted.

## B10 status

First B10 target-machine gate FAILED before tests.

Observed:
- architecture contract false-positive in 0.173 s;
- MinGW/g++ 15.2 rejected `const Policy& policy = {}`;
- production build failed on the same header after 17.209 s;
- therefore B10 is NOT accepted.

Corrective code/contract candidate before documentation commits:

```text
4a3d196c1574e91b747f05d194db7a35ad5c5517
```

Corrections:
- explicit 3-argument + 4-argument B10 overloads;
- same explicit-overload pattern in TrajectoryFollower;
- dependency contract checks actual include/type/query syntax rather than words in comments;
- contract pins the MinGW-safe overload form;
- navigation_runtime timing output now survives configure/build/test failure and prints the failing phase.

No diagnostic log file is required for this rerun.

## Rerun target-machine gate

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

Expected:
- architecture contract PASS;
- navigation_runtime 8/8 PASS;
- `maneuver_tracking_controller` PASS;
- runtime script prints configure/build/tests/total timings even if a phase fails;
- EliteGame + EliteServer build PASS.

## After green B10 gate

Next block is B14 Navigation Work Scheduler API:
- NavigationPlannerJob value type;
- urgent / normal / background queues;
- stale revision rejection;
- bounded jobs per slice;
- fairness/age promotion;
- duplicate suppression;
- deterministic synthetic tests for hundreds/thousands of actors;
- timing diagnostics, no brittle wall-clock correctness threshold.

Only after B14 acceptance migrate live GameSimulation from AcceptedShortSegment to AcceptedManeuverProgram.

## Documentation invariant

After every state-affecting iteration:
- rewrite `CONTINUE_PROMPT.md`;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical architecture/migration docs when ownership changes;
- keep verified target-machine baseline separate from current HEAD.
