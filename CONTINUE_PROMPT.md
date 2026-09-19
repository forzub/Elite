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

## Architecture

Planner:
B0 world -> B1 influence -> B2 objective -> B3 topology -> B4 local corridor ->
B5 physical compiler -> B6 proof -> B7 decision -> B8 accepted program.

Execution:
B9 sample -> B10 bounded tracking -> B11 safety/reflex ->
B12 PilotSkill -> B13 physics.

B14 owns only planner scheduling.

## Accepted evidence

B8/B9/B10 exact-hash target-machine baseline:
`2abd79a6181a322fe15425994ab771942e47bc26`.

B14 isolated gate is also green:
- architecture PASS 0.204 s;
- navigation_runtime 9/9;
- 5000 actors: enqueue 1518 us, dispatch+complete 1258 us, total 2776 us;
- client/server build PASS, production build 23.001 s.

The B14 user output omitted the rev-parse line, so do not fabricate an exact B14
tested hash.

## Current candidate

Live B14 integration candidate before docs:

`382c9d6f8ae347630ccd1a6ae6ec18bd077d086d`.

Stage-12 lab ownership is now:

```text
ReplanPolicy
 -> NavigationPlannerJob
 -> B14 enqueue
 -> bounded dispatch
 -> existing Planner::plan
 -> B14 complete(ticket)
 -> commit only CompletedCurrent
 -> existing AcceptedShortSegment
```

Rules:
- no direct Planner::plan -> authoritative commit;
- world/objective/capability/job revisions cross scheduler boundary;
- capability revision is monotonic and based on real current vehicle authority;
- planner geometry is unchanged;
- AcceptedShortSegment remains live for this slice;
- GameSimulation may measure scheduler/planner wall time; B14 core remains clock-free.

Live diagnostics include enqueue/dispatch/current/stale counts, pending/in-flight
depth and scheduler/planner microseconds.

Headless navigation self-test requires dispatch count == plan count and
CompletedCurrent count == dispatch count, with zero stale completion in this
synchronous fixture.

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

bash tests/navigation_runtime/run_live_scheduler_gate_mingw64.sh
```

The last command writes a persistent log under `build/logs` and MUST print the
exact Windows log path as its final `[LOG] ...` line.

Do not accept live B14 until all four gates are green.

## After green live B14

Migrate the live ACCEPT seam from AcceptedShortSegment to
AcceptedManeuverProgram as a separate target-machine-gated slice. Do not mix
that migration with scheduler changes.

## Every state-affecting iteration

Rewrite this file and CURRENT_TASK; update CURRENT_STATE, PROJECT_STATE and
STAGE12_END_TO_END; update architecture/migration/purity docs when ownership
changes.
