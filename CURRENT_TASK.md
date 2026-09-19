# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`
**Canonical architecture:** `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
**Migration audit:** `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`

## Verified baseline

Target-machine verified checkout:

```text
701881ddae861cd5593e425de91600e048bd417c
```

Evidence supplied from Windows 10 / MSYS2 MinGW64:
- `navigation_runtime`: 7/7 PASS;
- `maneuver_program_sampler`: PASS;
- `EliteGame`: BUILD PASS;
- `EliteServer`: BUILD PASS.

This promotes the B8/B9 API slice to accepted target-machine evidence.

## Current candidate — B10 clean tracking block

Candidate baseline before documentation commits:

```text
66d89b93e3edf0817bb8d88b78e405180887cecc
```

Added:
- `ManeuverTrackingController.h/.cpp`;
- Navigation-v2 overload of `TrajectoryFollower::follow(AcceptedManeuverProgram,...)`;
- `ManeuverTrackingControllerTests.cpp`;
- production/test CMake wiring;
- architecture contract locks for B8/B9/B10;
- configure/build/test phase timing output in `tests/navigation_runtime/run_mingw64.sh`.

Canonical B9/B10 execution path:

```text
AcceptedManeuverProgram
 -> B9 ManeuverProgramSampler
 -> ManeuverReferenceSample
 -> B10 ManeuverTrackingController
 -> A_ff + bounded tracking feedback
 -> alpha_ff + bounded tracking feedback
 -> NavigationLocalControlIntent
```

Hard invariants:
- zero tracking error => feedback exactly zero;
- therefore command equals accepted `A_ff/alpha_ff` exactly;
- feedback clamps to `linearFeedbackReserveMps2` / `angularFeedbackReserveRadPerSec2`;
- tracking envelope violation is reported for replanning;
- B8/B9/B10 have no world/planner query dependency;
- old `AcceptedShortSegment` overload remains for current live compatibility.

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

The runtime script now prints:

```text
[TIMING] navigation_runtime configure_ms=...
[TIMING] navigation_runtime build_ms=...
[TIMING] navigation_runtime tests_ms=...
[TIMING] navigation_runtime total_ms=...
```

No log file is required for this gate. If a later failing gate needs a persistent log, the test command/script must print the exact log path at the end.

Expected ctest count is now 8, including:
- `maneuver_program_sampler`;
- `maneuver_tracking_controller`.

Expected new test output includes:

```text
MANEUVER TRACKING CONTROLLER TESTS: PASS
 - zero error preserves A_ff/alpha_ff exactly
 - tracking feedback is bounded by proved reserve
 - follower composes B9 sampler -> B10 tracker
 - terminal completion uses accepted tolerances
```

## Next slice after green gate

Do **not** jump to B4 yet.

Next clean separation is B14 scheduler API before broad multi-agent rollout:
1. define value-owned `NavigationPlannerJob` + revision identity;
2. urgent / normal / background queues;
3. stale-job rejection before planner work;
4. per-slice job budget;
5. fairness/age promotion;
6. deterministic queue tests with hundreds/thousands of synthetic actors;
7. measure enqueue/dispatch timing but avoid brittle wall-clock pass/fail thresholds.

Then migrate the live GameSimulation ACCEPT boundary from `AcceptedShortSegment` to `AcceptedManeuverProgram` in a separately gated slice.

## Documentation invariant

After every state-affecting iteration:
- rewrite `CONTINUE_PROMPT.md`;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical block/migration docs when ownership changes;
- keep verified target-machine baseline distinct from current documentation HEAD.
