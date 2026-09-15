# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** live docking guidance correctness + performance  
**Stage:** NAV-LIVE-1 — bounded rolling reconnect over accepted trajectory

## Why this stage exists

NAV-LIVE-0 established the real runtime path and added timings. The live
`CALCULATE ROUTE` path is:

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner::plan()
    -> world::navigation::TrajectoryGenerator::generate()
    -> GuidanceTunnelBuilder::build()
    -> publish route + manual cockpit tunnel
```

The initial global route/trajectory is built once per docking request. However,
rolling current-pose tunnel correction can call `GuidanceTunnelBuilder` at the
4 Hz replan check. In `ReconnectCurrentPose` mode that builder can generate
multiple smooth candidates and repeatedly sample/collision-check them over the
complete remaining route. A presentation correction can therefore become a
small second planner and stall the frame.

## NAV-LIVE-1 change

The expensive reconnect is now designed as a **bounded local solve** on top of
the immutable accepted trajectory.

For a stable terminal:

1. find current progress on the accepted trajectory;
2. compute a local lookahead from:

```text
v * T_latency
+ v^2 / (2 * a_brake)
+ minimum_turn_radius * imminent_course_change
+ safety_margin
```

3. run `SmoothPathOptimizer` and canonical obstacle validation only from the
   current live pose to that local rejoin point;
4. stitch the untouched accepted trajectory tail after the rejoin;
5. keep the real docking terminal as the final tunnel endpoint.

This preserves safety and topology while making frequent current-pose correction
cost depend mainly on the near physical horizon instead of total route length.

### Moving terminal fallback

If the live dock terminal has materially moved from the accepted trajectory's
terminal pose, NAV-LIVE-1 deliberately keeps the previous full reconnect. We do
**not** deform an unvalidated tail to chase a moving dock. A bounded terminal-tail
solver is a later optimization.

If a bounded local reconnect itself cannot produce a valid smooth/collision-safe
segment, the builder also falls back to the old full reconnect.

## Global planner finding

`GeometricPathPlanner` remains a separate likely hotspot. Its lazy visibility
graph can still test many node-node links, and every segment validation iterates
obstacles. Dense scenes can therefore approach a practical
`nodes^2 * obstacles` workload. Do not rewrite it until `[DockingPerf]` confirms
that `geometric_ms` is the dominant live stage.

## Architecture authority

Read before further navigation work:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

It records the accepted four-layer split:

```text
global route -> bounded local motion -> per-tick execution -> cheap tunnel
```

and the collision/navigation geometry rules plus deterministic test-scene plan.

## Acceptance

Run locally under MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_live_docking_guidance.py
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py

bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Then reproduce the same docking manoeuvre that previously stalled.

Capture:

```text
[DockingPerf] ...
[GuidanceReplan] ... build_ms=...
[FramePerf] ... dock_ms=...
```

Acceptance questions:

1. Do current-pose rolling rebuilds become materially cheaper?
2. Does the cockpit tunnel still start from the live hull pose?
3. Does it still end at the real dock rather than the local-horizon rejoin?
4. Does any materially moved terminal safely use the full reconnect fallback?
5. Which initial request stage now dominates: `geometric_ms`, `trajectory_ms` or
   `tunnel_ms`?

## Next optimization decision

- If `geometric_ms` dominates: optimize/cull/cache global visibility search and
  move immutable global solves off the frame thread with latest-request-wins.
- If `trajectory_ms` dominates: bound/refactor global smoothing and its repeated
  safety sampling; keep accepted route topology immutable.
- If rolling tunnel `build_ms` still dominates: profile the bounded local
  candidate solver and nearest-progress lookup, then add a bounded terminal-tail
  solver for moving targets.
- If none dominates: trace the remaining synchronous phase before changing
  algorithms.

Do not reduce collision/safety fidelity to hide a frame stall.
