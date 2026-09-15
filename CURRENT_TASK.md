# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** live docking guidance correctness + performance  
**Stage:** NAV-LIVE-0 — current-pose tunnel refresh and active-path timing

## Runtime evidence

The isolated NAV-RUCKIG-0 spike remains accepted locally. The production game test, however, showed two clear problems:

- visible stalls/freezes remain during docking-route/guidance work;
- the cockpit guidance tunnel can stay attached to an obsolete solution while the hull has materially changed pose.

A runtime video supplied by the user is the acceptance evidence for this failure. NAV-RUCKIG-1 is therefore **not accepted as an end-to-end solution**.

## Critical architecture finding

The live `CALCULATE ROUTE` docking path currently does **not** go through `LocalGuidancePlanner::plan()` and therefore does not exercise the new Ruckig-first `predictLeg()` path.

The active game path is:

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner::plan()
    -> world::navigation::TrajectoryGenerator::generate()
    -> GuidanceTunnelBuilder::build()
    -> publish route + manual cockpit tunnel
```

All of this currently executes synchronously inside `SpaceState::update()`. Consequently, a slow geometric/trajectory/tunnel solve can block the frame directly. The previous Ruckig A/B work remains useful for `LocalGuidancePlanner`, but it cannot by itself remove a freeze from this different live path.

## NAV-LIVE-0 patch

Commit `5f60021cd8339efce7c589a9618864079931a0ff` changes the live path in two ways.

### 1. Rolling tunnel follows the current hull pose

The old manual-tunnel policy intentionally kept one fixed generation until a relatively large deviation, predicted exit, course change or low-speed attitude event. In a wide docking corridor that allowed the ship to move several metres, or rotate while drifting at speed, without rebuilding the tunnel.

At the existing 4 Hz replan check the tunnel now requests a current-pose reconnect when any of these are true:

- lateral displacement from the current tunnel exceeds 8% of the lateral tolerance, clamped to 0.75..3.0 m;
- vertical displacement exceeds the analogous threshold;
- hull attitude differs from the current tunnel tangent by more than the existing 4 degree course threshold.

A replacement generation still starts from the actual current hull position/orientation/velocity and reconnects to the accepted physical trajectory. We do **not** rigidly drag an old tunnel with the ship.

### 2. Time the path that actually freezes

The live route now reports:

```text
[DockingPerf] request=...
 total_ms=...
 snapshot_ms=...
 geometric_ms=...
 trajectory_ms=...
 tunnel_ms=...
```

Every rolling tunnel rebuild also reports `build_ms` in `[GuidanceReplan]`.

This separates four materially different costs instead of attributing the freeze to Ruckig without evidence.

## Acceptance now

Run:

```bash
git fetch origin
git pull --ff-only

python tests/architecture_contracts/check_live_docking_guidance.py
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py

bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Then reproduce the same manoeuvre from the video.

Acceptance questions:

1. Does the near cockpit tunnel rebuild from the changed hull pose instead of remaining stale?
2. Which live stage owns the stall according to `[DockingPerf]`?
3. Are rolling stalls correlated with `[GuidanceReplan] ... build_ms=...`?

Paste the `[DockingPerf]` line plus a few `[GuidanceReplan]` lines from one bad run. Do not infer the bottleneck from the isolated Ruckig benchmark.

## Next move after timings

If `trajectory_ms` or `geometric_ms` dominates, move the immutable route solve off the frame thread and make completion latest-request-wins. If rolling `tunnel build_ms` dominates, the current-pose reconnect must become a cheaper bounded local operation and/or asynchronous. If neither dominates, trace the remaining synchronous phase before changing algorithms.

Do not reduce collision/safety fidelity merely to hide a frame stall.

## Renderer status

OpenGL 4.3 Core, GPU-P0 static System Map spheres and GPU-P0.1 instanced planar System Map circles/orbits are accepted locally. The older station-adjacent renderer freeze remains explicitly out of scope for this navigation task.
