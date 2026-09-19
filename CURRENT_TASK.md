# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`
**Canonical architecture:** `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
**Migration audit:** `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`
**Last fully accepted Stage-12 target-machine baseline:** `daaf038021cdf8b9561db60fdd35e7cefce0b2df`
**Last target-machine checkout actually exercised:** `46f6a37da6775a1d044391f773476df1bb07bc6a`
**Current unverified B8/B9 code candidate before documentation commits:** `516f63eb7a3b2bbf09bad553aff975d52d7c3e8c`

## Current canonical architecture

Navigation v2 is now fixed into two top-level worlds and explicit blocks B0..B14.

```text
PLANNER WORLD
B0 world truth
 -> B1 shared dynamic influence
 -> B2 objective
 -> B3 topology route
 -> B4 local route-aligned corridor
 -> B5 physical maneuver compiler
 -> B6 continuous proof
 -> B7 maneuver decision
 -> B8 AcceptedManeuverProgram

AUTOPILOT / FOLLOWER WORLD
B9 program sampler
 -> B10 bounded tracking
 -> B11 safety monitor / bounded reflex
 -> B12 PilotSkill
 -> B13 propulsion / physics
 -> completion/invalidation -> B14 planning scheduler
```

Read the canonical block document for each block's owner, question, cadence, inputs, outputs and scaling contract.

## Scale contract

The architecture must support hundreds/thousands of registered units.

Hard rules:
- no dense N x N dynamic work;
- B0/B1 shared scene-wide world work;
- only dirty agents enter planner blocks B3..B8;
- all active controlled actors may run cheap B9/B10/B12/B13 each required control tick;
- planning is queued/budgeted through B14;
- fixed-capacity accepted programs, bounded local candidates and exact proof only for broadphase survivors;
- stale queued jobs are rejected by revision;
- urgent invalidations outrank ordinary/background planning;
- no planner call merely because another physics frame elapsed.

The current runtime lab already uses NavigationExecutionReplanPolicy and only calls NavigationRuntimePlanner when scope != None. The missing production-scale piece is an explicit multi-agent planning queue/scheduler, to be implemented after the execution API boundary is clean.

## Iteration 1 — B8/B9 separation

Added:
- `AcceptedManeuverProgram.h`
- `ManeuverProgramSampler.h/.cpp`
- isolated `ManeuverProgramSamplerTests.cpp`
- production/test CMake wiring.

### B8 AcceptedManeuverProgram

Fixed-capacity value-owned program:
- max 16 control/reference samples;
- P/V/A_ff;
- full body basis;
- omega/alpha_ff;
- validity/revisions;
- terminal tolerances;
- tracking envelope + reserved feedback authority;
- capability/proof witness;
- maneuver family/provenance metadata.

No NavigationMap/NavigationSpace dependency and no per-tick allocation.

### B9 ManeuverProgramSampler

Pure API:

```text
AcceptedManeuverProgram + universeTime
    -> sampled reference/feed-forward state
```

It performs:
- fail-closed validation;
- bounded control-key lookup;
- interpolation of the accepted reference/feed-forward program;
- body-basis re-orthonormalization.

It does NOT:
- inspect obstacles;
- generate a target velocity;
- apply feedback;
- replan.

## Target-machine gate to run now

```bash
cd /d/__elite/work

git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_mingw64.sh
```

Expected new ctest includes:

```text
maneuver_program_sampler
```

Expected output:

```text
MANEUVER PROGRAM SAMPLER TESTS: PASS
 - fixed-capacity AcceptedManeuverProgram
 - exact proved feed-forward survives sampling
 - sampler performs no target-velocity control solve
 - invalid time domains fail closed
```

If the isolated runtime suite is green, also run the production compile gate:

```bash
bash build_mingw64.sh
```

No live behavior is intentionally changed in iteration 1, so the old AcceptedShortSegment runtime seam remains active.

## Next iteration after green gate

B10 migration:
1. add a clean sampled-reference tracking API;
2. make TrajectoryFollower consume AcceptedManeuverProgram/ManeuverProgramSampler;
3. feedback may only use the reserved tracking authority;
4. prove exact A_ff/alpha_ff identity reaches follower output when tracking error is zero;
5. retain old AcceptedShortSegment overload temporarily;
6. only after isolated + live target-machine acceptance migrate GameSimulation packing.

## Documentation invariant

After every state-affecting iteration:
- rewrite `CONTINUE_PROMPT.md` completely;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical block/migration docs when ownership changes.

Do not call documentation HEAD an accepted target-machine baseline.
