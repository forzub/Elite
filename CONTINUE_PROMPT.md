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

## Current canon

Navigation is split into two top-level worlds.

```text
PLANNER WORLD
B0 world snapshot
B1 shared sparse influence
B2 objective
B3 topology route
B4 route-aligned local corridor
B5 physical maneuver compiler
B6 continuous proof
B7 maneuver decision
B8 AcceptedManeuverProgram

AUTOPILOT / FOLLOWER WORLD
B9 ManeuverProgramSampler
B10 bounded tracking
B11 safety monitor / bounded reflex
B12 PilotSkill
B13 propulsion / physics
B14 planning scheduler receives completion/invalidation work
```

Planner uses authoritative known world truth. Segment/ray/sweep queries are proof tools, not perception.

Local ordinary planning target is a route-aligned configuration-space corridor, not the 15/30/45/60/75 LocalAvoidance fan. Keep NavigationSpace topology above it.

Follower executes the same program that was proved. It may apply bounded feedback; a material emergency deviation invalidates the program and requests replan.

## Scaling canon

Must support hundreds/thousands of registered units:
- no dense N x N;
- shared scene-wide B0/B1;
- only dirty agents enter B3..B8;
- active controlled actors run cheap fixed-step B9/B10/B12/B13;
- explicit B14 urgent/normal/background planning queue with budget/fairness;
- stale jobs rejected by revisions;
- fixed-capacity hot-path products;
- no plan-every-frame.

## Current iteration

Iteration 1 separates B8/B9 API.

New files:
- `src/game/navigation/AcceptedManeuverProgram.h`
- `src/game/navigation/ManeuverProgramSampler.h`
- `src/game/navigation/ManeuverProgramSampler.cpp`
- `tests/navigation_runtime/ManeuverProgramSamplerTests.cpp`

Current unverified code candidate before docs:
`516f63eb7a3b2bbf09bad553aff975d52d7c3e8c`.

The old `AcceptedShortSegment` live seam intentionally remains active for compatibility.

Run now:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_mingw64.sh
bash build_mingw64.sh
```

Do not claim target-machine acceptance until user supplies the gate result.

Expected new isolated test: `maneuver_program_sampler`.

After green gate, next iteration is B10:
- TrajectoryFollower consumes AcceptedManeuverProgram through ManeuverProgramSampler;
- zero tracking error => output feed-forward must equal accepted A_ff/alpha_ff exactly;
- bounded feedback only inside reserved authority;
- old segment overload remains temporarily;
- then migrate GameSimulation ACCEPT packing in a later live-gated iteration.

Last fully accepted Stage-12 target-machine baseline remains:
`daaf038021cdf8b9561db60fdd35e7cefce0b2df`.

Last target-machine checkout actually exercised before this migration remains:
`46f6a37da6775a1d044391f773476df1bb07bc6a`.

## Every state-affecting iteration

- rewrite this file from scratch;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update canonical architecture/migration docs if ownership changes;
- record actual verified target-machine baselines separately from repo HEAD.
