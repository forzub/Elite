# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** isolated runtime navigation backend  
**Stage:** NAV-RUCKIG-1 — replace live custom smoothing with canonical Ruckig motion

## Decision

The custom live spline/reconnect stack is no longer an optimization target. The
runtime motion layer is being rebuilt around Ruckig.

The ownership boundary is now:

```text
ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner / GeometricPathPlanner   coarse free-space topology
    -> RuckigRoutePlanner                          canonical route motion
    -> RuckigTrajectorySolver                      state-to-state primitive
    -> GuidanceTunnel                              presentation sampler
```

`SmoothPathOptimizer` is forbidden in both live trajectory generation and live
rolling tunnel reconnect. A new architecture contract enforces this.

## Implemented in this wave

### Canonical route motion

`src/game/navigation/RuckigRoutePlanner.h` defines the canonical route-to-motion
seam. `TrajectoryGenerator::generate()` is now a compatibility facade that calls
`RuckigRoutePlanner::plan()`.

The old global B-spline runtime path has been removed from
`TrajectoryGenerator.cpp`. Each accepted coarse-route leg is solved with
`RuckigTrajectorySolver` and swept against canonical navigation obstacles before
it is published.

Current conservative baseline stops at intermediate coarse topology vertices.
That is intentionally less elegant than hidden corner shaving. Continuous
through-waypoint motion will be added only as a bounded Ruckig transition that
passes the same swept collision validation.

### Ruckig rolling reconnect

`GuidanceTunnel.cpp` no longer builds spline/bulge/loop candidate families.
`ReconnectCurrentPose` now uses Ruckig:

- stable terminal: solve live pose/velocity -> bounded rejoin state;
- preserve accepted rejoin velocity;
- collision-check the generated Ruckig leg;
- stitch the untouched accepted trajectory tail;
- keep the real dock as the final tunnel endpoint.

The reconnect horizon remains physical:

```text
v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

If the terminal moved materially, or the bounded join is infeasible, the code
attempts a full Ruckig reconnect through sparse accepted-route supports plus a
final docking-axis alignment point. There is no fallback to the old smoother.

### Test/build boundary

`guidance_tunnel_local_horizon_tests` now links the Ruckig backend directly and
no longer compiles `SmoothPathOptimizer.cpp`.

New architecture contract:

```bash
python tests/architecture_contracts/check_ruckig_live_navigation.py
```

It verifies:

- route trajectory uses Ruckig;
- rolling tunnel reconnect uses Ruckig;
- both retain canonical swept obstacle checks;
- neither live source references `SmoothPathOptimizer`.

## Historical performance baseline

The previous 19-obstacle custom-smoother capture was:

```text
[DockingPerf]
total_ms      ~= 470.3
snapshot_ms   ~=   0.41
geometric_ms  ~=   7.42
trajectory_ms ~= 373.3
tunnel_ms     ~=  15.5
```

The old rolling implementation also produced 39 reconnect attempts × 7 spatial
candidate families and consumed roughly 14.4 seconds of smoother CPU in the
captured short run.

These numbers are now comparison data only. Do not optimize that old algorithm
further.

## Acceptance before motion-quality work

Run under MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
python tests/architecture_contracts/check_ruckig_live_navigation.py
python tests/architecture_contracts/check_live_docking_guidance.py

bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Important: the large `NavigationGuidanceTests.cpp` still contains several
legacy B-spline/SmoothPath assertions. Those tests are being migrated next; if
they fail specifically on old spline expectations, that is a stale-test failure,
not a reason to restore the old runtime backend. Compile/link failures or Ruckig
behavior failures are real blockers and must be fixed.

## Next work

1. Migrate/remove obsolete B-spline tests and delete the last test-only dependency
   on `SmoothPathOptimizer`.
2. Remove `SmoothPathOptimizer.cpp` from `EliteGame` / `EliteServer` production
   source lists once the branch compile proves no remaining runtime consumer.
3. Run the docking stress scene and compare new `[DockingPerf]`,
   `[GuidanceReplan]`, `[FramePerf]` and `[RuckigRoutePerf]` against the historical
   smoother baseline.
4. If motion is too stop-and-go, add **bounded Ruckig through-waypoint blending**:
   never a global spline and never an unchecked corner cut.
5. If synchronous planning remains visibly blocking, move immutable planning to
   a worker with request generation/latest-request-wins publication.

## Acceptance rule

Do not restore the custom global smoother to make a test or visual artifact pass.
Fix the Ruckig motion contract, topology, collision geometry, or stale test as
appropriate.
