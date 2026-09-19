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
7. `src/game/navigation/NAVIGATION_PURITY_CONTRACT.md`

## Canon

Planner world:
B0 world snapshot -> B1 shared influence -> B2 objective -> B3 topology ->
B4 local route-aligned corridor -> B5 physical maneuver compiler ->
B6 continuous proof -> B7 decision -> B8 AcceptedManeuverProgram.

Execution world:
B9 sampler -> B10 bounded tracking -> B11 safety/reflex ->
B12 PilotSkill -> B13 propulsion/physics.

B14 decides **when** planner blocks get CPU. It is not a planner.

## Verified baseline

Target-machine verified:

```text
2abd79a6181a322fe15425994ab771942e47bc26
```

Evidence:
- architecture contract PASS;
- navigation_runtime 8/8 PASS;
- maneuver_tracking_controller PASS;
- EliteGame / EliteServer BUILD PASS;
- build_mingw64 real 44.989 s;
- architecture contract real 0.186 s.

B8/B9/B10 accepted.

## Current B14 candidate

Code/contract baseline before documentation commits:

```text
1ba23241d760b3a57c917189e333e4fbc0365ea4
```

New:
- `NavigationPlannerJob`;
- `NavigationWorkScheduler`;
- `NavigationWorkSchedulerTests.cpp`.

B14 rules:
- urgent / normal / background queues;
- deterministic age promotion;
- bounded dispatch <= 128 jobs and cost-unit budget;
- one pending actor slot;
- monotonic per-actor job revision;
- duplicate suppression;
- stale rejection before dispatch;
- in-flight ticket retained until `complete(ticket)`;
- stale worker result rejected before commit;
- ordinary replacement does not scan queue;
- lazy tombstones are compacted amortized;
- capacity pressure reclaims already-stale records;
- scheduler owns no planner/world owner/wall clock.

Scale gate:
- 5000 synthetic actors;
- deterministic drain;
- no loss/duplication;
- pending/in-flight/queuedRecords all return to zero;
- timing is printed but not used as a correctness threshold.

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

Expected:
- architecture PASS;
- navigation_runtime 9/9 PASS;
- navigation_work_scheduler PASS;
- verbose diagnostic prints
  `[TIMING] navigation_work_scheduler actors=5000 ...`;
- runtime script prints configure/build/tests/scheduler diagnostic/total;
- client + server build PASS.

No persistent log is required unless a failure needs diagnosis. If a log is
created, print its exact path as the last line of the command/script.

Do not accept B14 without target-machine evidence.

## After B14 PASS

Next slice is **live scheduler integration**, not B4/B5 yet:
- map current NavigationExecutionReplanPolicy result into scheduler jobs;
- GameSimulation submits dirty work instead of directly planning;
- dispatch bounded jobs;
- execute existing planner unchanged for dispatched jobs;
- complete ticket and commit only if current;
- add live queue/stale/dispatch timing counters;
- preserve AcceptedShortSegment until this seam is independently green.

After live B14 integration acceptance, migrate live ACCEPT to
AcceptedManeuverProgram in a separate slice.

## Every state-affecting iteration

- rewrite this file completely;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical architecture/migration/purity docs when ownership changes;
- keep verified target-machine baseline distinct from unverified HEAD.
