# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`
**Last target-machine fully accepted Stage-12 baseline:** `daaf038021cdf8b9561db60fdd35e7cefce0b2df`
**Last target-machine checkout actually exercised:** `46f6a37da6775a1d044391f773476df1bb07bc6a`
**Architecture analysis commit:** `92432c842d5ad632b320f90c2ca258b82f17d26b`

## Current decision

The active architecture is being sharpened into two top-level runtime worlds:

```text
PLANNER WORLD
    authoritative world truth + objective + vehicle/pilot/doctrine
        -> topology/corridor
        -> local free-space path
        -> physical maneuver compilation/proof
        -> AcceptedManeuverProgram

AUTOPILOT / FOLLOWER WORLD
    AcceptedManeuverProgram + current state
        -> tracking + bounded recovery
        -> safety monitor / emergency reflex
        -> PilotSkill -> propulsion -> physics
        -> completion/invalidation -> planner wake-up
```

Canonical analysis:
`src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md`.

## What the analysis changed

The current 15/30/45/60/75-degree LocalAvoidance fan should no longer be treated as the target local-planning architecture.

Important distinction:

```text
ray/segment tests may remain as geometric proof primitives
but
ray-fan search should be replaced by direct planning over known world geometry
```

The preferred local solver is a route-aligned configuration-space corridor:
- choose local longitudinal axis along the current route/corridor leg;
- project exact/relevant obstacle occupancy into longitudinal slabs;
- inflate obstacles by hull + clearance + pilot/tracking uncertainty;
- connect free-space components across slabs;
- choose a smooth progress-preserving path;
- then compile that path into a physically feasible, time-parameterized maneuver.

This is a LOCAL/CORRIDOR solver. NavigationSpace topology remains above it for cases requiring branches, backtracking, non-monotonic detours or complex station geometry.

## Current implementation priority

Do **not** immediately delete LocalAvoidance or rewrite the topology layer.

Next code slice remains the execution API boundary because it is required by both the old and new planner:

1. add bounded `AcceptedManeuverProgram`;
2. migrate `TrajectoryFollower` from target-velocity re-solving to sampling the same proved program plus bounded tracking feedback;
3. keep `AcceptedShortSegment` only as transitional compatibility if needed;
4. add deterministic tests that prove planner program == follower feed-forward program.

After that:

5. prototype `RouteAlignedCorridorPlanner` beside the existing ray-fan LocalAvoidance;
6. A/B them on deterministic obstacle fields, slit/tunnel and moving-obstacle fixtures;
7. add physical maneuver compilation for Newtonian/Assisted control;
8. only retire the ray-fan search path after target-machine evidence.

## Follower boundary

Follower may:
- reduce cross-track/velocity/attitude error while preserving forward/program progress;
- recover from small disturbances inside the accepted tracking envelope;
- participate in a bounded imminent-hazard reflex.

Follower must not:
- invent a new ordinary route;
- choose new portals;
- run a hidden LocalAvoidance replacement;
- silently continue an obsolete program after a material emergency deviation.

A material reflex/deviation invalidates the program and wakes the planner.

## Batch/world scaling target

Shared work should be scene-wide:

```text
dynamic snapshot
 -> spatial broadphase
 -> sparse relevant pairs
 -> batch/SIMD kinematic filter
 -> per-agent influence lists
```

Only agents needing a new program enter the planner batch.

Every control tick, all active accepted programs may be sampled in an AutopilotWorld batch.

## Immediate next verification/code work

No target-machine acceptance claim was made by this documentation-only architecture pass.

Next implementation must begin from current `main`, introduce `AcceptedManeuverProgram`, then run:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_foundation_lock.py
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

If NavigationMap/LocalHorizon/LocalAvoidance changes in the later corridor-planner slice, also run their isolated MinGW suites.

## Documentation invariant

After every state-affecting iteration:
- rewrite `CONTINUE_PROMPT.md` from scratch;
- rewrite `CURRENT_TASK.md` to the actual immediate task;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update the active canonical architecture document when its contract changes.

Record actual target-machine evidence separately from documentation/code HEAD.
