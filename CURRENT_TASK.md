# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`
**Canonical architecture:** `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
**Migration audit:** `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`

## Last verified baseline

```text
2abd79a6181a322fe15425994ab771942e47bc26
```

Target-machine evidence:
- Stage-12 architecture contract: PASS;
- `navigation_runtime`: 8/8 PASS;
- `maneuver_tracking_controller`: PASS;
- `EliteGame`: BUILD PASS;
- `EliteServer`: BUILD PASS;
- `build_mingw64.sh`: 44.989 s real;
- architecture contract: 0.186 s real.

B8/B9/B10 are accepted.

## Current candidate — B14 Navigation Work Scheduler

Code/contract baseline before documentation commits:

```text
1ba23241d760b3a57c917189e333e4fbc0365ea4
```

New:
- `src/game/navigation/NavigationWorkScheduler.h`
- `src/game/navigation/NavigationWorkScheduler.cpp`
- `tests/navigation_runtime/NavigationWorkSchedulerTests.cpp`

B14 is a scheduling service, not a planner.

It owns:
- urgent / normal / background work queues;
- actor revision slots;
- pending and in-flight tickets;
- deterministic age promotion;
- bounded planner dispatch slices.

It does not own:
- NavigationMap;
- NavigationSpace;
- NavigationRuntimePlanner;
- any planner callback;
- wall clock / random / file I/O.

### Queue scale contract

```text
enqueue / replace
    -> actor slot
    -> no queue scan in ordinary replacement path

old queue record
    -> lazy tombstone

excess tombstones
    -> amortized compaction
    -> bounded physical queue storage

dispatch
    -> <= 128 jobs
    -> <= deterministic cost-unit budget

planner finishes
    -> complete(ticket)
    -> CompletedCurrent | CompletedStale
```

Important:
- per-actor `jobRevision` is monotonic;
- completed revisions cannot be replayed;
- stale world/objective/capability/route jobs are rejected before expensive work;
- stale in-flight results are rejected before commit;
- capacity pressure reclaims already-stale queue records before rejecting fresh work;
- only one planner job for an actor can be in flight at once.

### Scale fixture

The isolated test queues **5000 synthetic actors** and proves:
- deterministic dispatch;
- no job loss or duplication;
- fixed job/cost slice limits;
- queue, in-flight state and physical records return to zero.

The test prints:

```text
[TIMING] navigation_work_scheduler actors=5000 enqueue_us=... dispatch_complete_us=... total_us=...
```

Timing is diagnostic only; no brittle wall-clock PASS threshold exists.

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
```

Expected:
- architecture contract PASS;
- navigation_runtime **9/9 PASS**;
- new `navigation_work_scheduler` PASS;
- verbose scheduler diagnostic exposes the 5000-actor timing line;
- runtime script prints configure/build/tests/scheduler diagnostic/total timings;
- `EliteGame` and `EliteServer` BUILD PASS.

No persistent log is required for this gate. If a failure needs a log, create it
only for diagnosis and print the exact path as the final line of the test command.

## Next slice after green B14 gate

Do not immediately merge planner implementation changes into B14.

Next step is a separately gated **live scheduler integration slice**:
1. translate current `NavigationExecutionReplanPolicy::Result` into B14 jobs;
2. keep existing planner behavior/geometry unchanged;
3. GameSimulation submits dirty events instead of invoking the planner directly;
4. dispatch a bounded scheduler slice;
5. execute planner only for dispatched jobs;
6. complete ticket and commit result only if still current;
7. add counters/timing for queued, dispatched, stale and completed work;
8. keep `AcceptedShortSegment` live compatibility until this scheduler seam is green.

After the live scheduler seam is accepted, migrate the GameSimulation ACCEPT
product from `AcceptedShortSegment` to `AcceptedManeuverProgram` in its own
target-machine-gated slice.

## Documentation invariant

After every state-affecting iteration:
- rewrite `CONTINUE_PROMPT.md`;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical architecture/migration docs when ownership changes;
- keep verified target-machine baseline separate from unverified HEAD.
