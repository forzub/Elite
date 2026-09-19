# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`
**Canonical architecture:** `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
**Migration audit:** `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`

## Accepted evidence

### B8/B9/B10 exact-hash baseline

```text
2abd79a6181a322fe15425994ab771942e47bc26
```

Verified:
- architecture contract PASS;
- navigation_runtime 8/8 PASS;
- maneuver_tracking_controller PASS;
- EliteGame / EliteServer BUILD PASS.

### B14 isolated scheduler gate

Fresh target-machine evidence:
- architecture contract PASS: 0.204 s;
- navigation_runtime 9/9 PASS;
- navigation_work_scheduler PASS;
- 5000 actors:
  - enqueue 1518 us;
  - dispatch + complete 1258 us;
  - total 2776 us;
- EliteGame / EliteServer BUILD PASS;
- production build 23.001 s.

The supplied B14 output did not contain the `git rev-parse HEAD` line, so this
document does not invent an exact tested B14 hash. The observed B14 isolated
gate is accepted; the last explicitly named target-machine hash remains the B10
baseline above.

## Current candidate — live B14 scheduler integration

Code/contract candidate before documentation commits:

```text
382c9d6f8ae347630ccd1a6ae6ec18bd077d086d
```

The deterministic Stage-12 lab now routes replans through:

```text
NavigationExecutionReplanPolicy
    -> NavigationPlannerJob
    -> NavigationWorkScheduler::enqueue
    -> bounded dispatchSlice(maxJobs=1, maxCostUnits=4)
    -> existing NavigationRuntimePlanner::plan
    -> NavigationWorkScheduler::complete(ticket)
    -> commit only on CompletedCurrent
    -> existing AcceptedShortSegment packing
```

No route/local geometry and no AcceptedShortSegment semantics changed.

### Live revision contract

Before dispatch GameSimulation publishes:
- current NavigationMap source/world revision;
- planner goal/objective revision;
- monotonic capability revision derived from current real linear/angular authority;
- monotonic per-actor planner job revision.

A planner result remains local until B14 completion says it is current.

### Live diagnostics

NavigationRuntimeLabObservation now records:
- scheduler enqueue accepted/replaced/duplicate/stale;
- dispatch count;
- current/stale completion counts;
- maximum pending/in-flight depth;
- dispatch total/max microseconds;
- planner total/max microseconds.

The headless self-test requires:
- scheduler dispatch count > 0;
- schedulerDispatchCount == planCount;
- schedulerCompletedCurrentCount == schedulerDispatchCount;
- schedulerCompletedStaleCount == 0 in this synchronous lab;
- max pending == 1;
- max in-flight == 1;
- prior physical navigation / exact-static / replication gates remain green.

## Logged live gate

A dedicated script now stores the complete self-test output:

```bash
bash tests/navigation_runtime/run_live_scheduler_gate_mingw64.sh
```

It always ends with:

```text
[TIMING] navigation_live_scheduler total_ms=... rc=...
[LOG] D:\...\build\logs\navigation_live_scheduler_YYYYMMDD-HHMMSS.log
```

The exact log path is therefore preserved on both PASS and FAIL.

## Run target-machine gate

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh

TIMEFORMAT='[TIMING] build_mingw64 real_s=%R user_s=%U sys_s=%S'
time bash build_mingw64.sh

bash tests/navigation_runtime/run_live_scheduler_gate_mingw64.sh
```

Expected:
- architecture PASS;
- navigation_runtime 9/9 PASS;
- client/server BUILD PASS;
- live scheduler self-test PASS;
- final live diagnostic includes scheduler counters/times;
- the final script line is `[LOG] ...`.

## Next slice after green live B14 gate

Only after this live scheduler seam is accepted:
1. migrate GameSimulation ACCEPT from `AcceptedShortSegment` to
   `AcceptedManeuverProgram`;
2. keep B14 scheduling unchanged;
3. prove that the exact sampled B8/B9/B10 program crosses PilotSkill/physics and
   replication;
4. only then retire the old AcceptedShortSegment live compatibility path.

## Documentation invariant

After every state-affecting iteration:
- rewrite `CONTINUE_PROMPT.md`;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical architecture/migration/purity docs when ownership changes;
- keep exact target-machine hashes separate from inferred/current HEAD.
