# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`
**Canonical architecture:** `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
**Migration audit:** `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`

## Last fully verified migration baseline

```text
701881ddae861cd5593e425de91600e048bd417c
```

Target-machine evidence:
- navigation_runtime 7/7 PASS;
- maneuver_program_sampler PASS;
- EliteGame BUILD PASS;
- EliteServer BUILD PASS.

B8/B9 is accepted.

## B10 corrective rerun status

The first B10 gate failed on:
- a false-positive architecture grep;
- MinGW/g++ 15.2 rejecting `const Policy& policy = {}`.

Corrective code/contract baseline before docs:

```text
4a3d196c1574e91b747f05d194db7a35ad5c5517
```

Fixes are on main:
- explicit overloads instead of braced default-reference arguments;
- precise dependency contract;
- MinGW-safe overload form pinned by architecture contract;
- runtime gate timing prints on both PASS and FAIL.

Fresh target-machine evidence now confirms:
- EliteGame BUILD PASS;
- EliteServer BUILD PASS;
- `build_mingw64.sh` real time: **44.989 s**.

This closes the production compile/link defect.

## What is still missing before B10 acceptance

The supplied output did not include:
- architecture-contract PASS;
- navigation_runtime 8/8 PASS;
- exact `git rev-parse HEAD` from the target machine.

Therefore B10 remains target-machine pending.

Run only the missing gate pieces now:

```bash
cd /d/__elite/work

git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected:
- architecture contract PASS;
- navigation_runtime 8/8 PASS;
- `maneuver_tracking_controller` PASS;
- timing lines for configure/build/tests/total.

No production rebuild is required again unless one of those two gates exposes a new code change.

## After full B10 green

Next block is B14 Navigation Work Scheduler API:
- NavigationPlannerJob value type;
- urgent / normal / background queues;
- stale revision rejection;
- bounded jobs per slice;
- fairness/age promotion;
- duplicate suppression;
- deterministic tests for hundreds/thousands of synthetic actors;
- timing diagnostics, without brittle wall-clock pass/fail thresholds.

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
