# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** isolated runtime navigation backend  
**Stage:** NAV-RUCKIG-1 — canonical Ruckig motion + collision-gated waypoint blending

## Decision

The custom live spline/reconnect stack is retired. Runtime motion is Ruckig.

```text
ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner / GeometricPathPlanner   coarse free-space topology
    -> RuckigRoutePlanner                          canonical route motion
    -> RuckigTrajectorySolver                      state-to-state primitive
    -> GuidanceTunnel                              presentation sampler
```

`SmoothPathOptimizer` is forbidden in live route generation and live rolling
reconnect. Its old B-spline implementation has been deleted from the `.cpp`.
The symbol remains temporarily as a fail-closed migration shim: production calls
return `retired; use the canonical Ruckig navigation backend`. A minimal
non-smoothing compatibility path exists only when the legacy test target defines
`ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT`.

## Implemented

### Route motion

`src/game/navigation/RuckigRoutePlanner.h` defines the canonical route-to-motion
seam. `TrajectoryGenerator::generate()` is only a compatibility facade.

For each internal coarse waypoint the route backend now attempts a bounded
through-waypoint velocity:

1. derive incoming/outgoing directions;
2. reject U-turns and explicit stop constraints;
3. construct a local corner-cut eligibility chord;
4. require that chord to be clear with the full navigation envelope;
5. derive a conservative turn speed from lateral acceleration and available
   blend distance;
6. solve the real adjacent motion with Ruckig;
7. swept-check every generated Ruckig chord against canonical navigation
   obstacles.

If a through-waypoint leg fails either Ruckig or collision validation, only the
adjacent waypoint velocities are relaxed to zero and the route is retried. It
never falls back to a global spline. Thus clear space can remain continuous,
while a geometrically dangerous corner becomes a safe stop point.

Focused coverage:

```text
tests/navigation_guidance/RuckigRoutePlannerTests.cpp
```

It covers a straight route, a clear continuous corner, an obstacle-blocked
corner that must fall back to a stop, and an impossible initial braking state.

### Rolling reconnect

`GuidanceTunnel.cpp` no longer generates spline/bulge/loop candidates.
`ReconnectCurrentPose` uses Ruckig:

- stable terminal: current position/velocity -> bounded accepted rejoin state;
- preserve accepted rejoin velocity;
- swept collision validation;
- immutable accepted tail stitched after the local replacement;
- real dock remains the final endpoint.

The physical horizon remains:

```text
v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

Moved terminal or failed bounded rejoin uses a full Ruckig reconnect through
sparse accepted-route supports and a final docking-axis alignment point. There
is no old-smoother fallback.

## Hard architecture guard

```bash
python tests/architecture_contracts/check_ruckig_live_navigation.py
```

It verifies:

- route trajectory uses Ruckig;
- rolling reconnect uses Ruckig;
- both perform canonical swept obstacle checks;
- live sources contain no `SmoothPathOptimizer` call;
- the retired smoother fails closed in production;
- its temporary compatibility macro exists only in the old navigation test
  target, never in root production CMake.

## Historical baseline

Old custom-smoother 19-obstacle capture:

```text
[DockingPerf]
total_ms      ~= 470.3
snapshot_ms   ~=   0.41
geometric_ms  ~=   7.42
trajectory_ms ~= 373.3
tunnel_ms     ~=  15.5
```

The same capture showed 39 rolling reconnect attempts × 7 curve candidate
families and roughly 14.4 s of smoother CPU. These values are comparison data
only. Do not optimize or restore that algorithm.

## Acceptance order

First validate the new backend without running stale global-spline assertions:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
python tests/architecture_contracts/check_ruckig_live_navigation.py
python tests/architecture_contracts/check_live_docking_guidance.py

bash tests/navigation_guidance/run_ruckig_mingw64.sh
cmake --build build --target EliteGame
```

Only after the focused Ruckig/local-horizon suite and `EliteGame` compile pass,
run the complete legacy aggregate suite:

```bash
bash tests/navigation_guidance/run_mingw64.sh
```

If that full suite fails only in the explicitly old "global B-spline" expectation,
it is a stale-test migration item. Failures in `ruckig_route_planner`,
`guidance_tunnel_local_horizon`, compile/link, collision safety, terminal state or
Ruckig integration are real blockers.

## Next work

1. Run/fix the focused Ruckig and local-horizon suites under MinGW.
2. Migrate the obsolete global-B-spline assertion in the all-in-one suite to
   Ruckig waypoint/blend semantics; then remove the test-only smoother shim and
   delete `SmoothPathOptimizer.{h,cpp}` completely from the tree/build lists.
3. Run the same docking stress scene and compare `[DockingPerf]`,
   `[GuidanceReplan]`, `[FramePerf]` and `[RuckigRoutePerf]` against the historical
   smoother baseline.
4. If initial planning still stalls, move immutable route work off the client
   update thread with generation IDs/latest-request-wins publication.
5. Only after measured need: improve topology search/broadphase or Ruckig
   blending. Never add another global motion smoother.

## Acceptance rule

Do not restore the custom smoother to make a stale test or visual artifact pass.
Fix Ruckig motion, topology, collision geometry, scheduling, or the stale test.
