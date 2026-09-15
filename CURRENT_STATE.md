# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core accepted locally  
**GPU-P0:** System Map static textured spheres accepted locally  
**GPU-P0.1:** repeated planar System Map circles/orbits accepted locally  
**Navigation:** NAV-RUCKIG-0 accepted; NAV-LIVE-2 active

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared
runtime/deterministic geometry seams.

Dual-source model ingress remains accepted:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority.
Runtime-model consumer migration is queued behind current navigation/performance
work.

## Renderer status

OpenGL 4.3 Core, GPU-P0 resident System Map textured spheres and GPU-P0.1
instanced planar circles/orbits are accepted locally. The renderer wave is
paused.

The current docking-guidance freeze is measured inside client navigation work,
not scene submission. Do not conflate it with the older station-adjacent renderer
freeze.

## Live docking path

The active client path remains:

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner::plan()
    -> world::navigation::TrajectoryGenerator::generate()
    -> GuidanceTunnelBuilder::build()
    -> GuidanceState publication
```

The initial solve and rolling tunnel rebuilds are still synchronous inside the
client update path.

## Measured NAV-LIVE-1 result

Latest runtime evidence:

```text
[DockingPerf]
total_ms      ~= 198.1
snapshot_ms   ~=   0.10
geometric_ms  ~=   0.47
trajectory_ms ~= 106.5
tunnel_ms     ~=  15.7
```

Repeated active-guidance frames show roughly `dock_ms=110..126 ms` and
`tunnel_builds=1`.

Therefore:

- current geometric path search is not the dominant cost in the three-obstacle
  baseline;
- `TrajectoryGenerator` / `SmoothPathOptimizer` is the dominant initial stage;
- rolling `GuidanceTunnelBuilder` still performs expensive work despite the
  bounded reconnect architecture;
- the visible freeze is expected while this work remains on the frame thread.

## NAV-LIVE-2 implementation

### SmoothPathOptimizer

`src/world/navigation/SmoothPathOptimizer.cpp` now:

- replaces the per-evaluation linear knot-span scan with binary search;
- preserves spline/collision/curvature semantics;
- writes detailed optimizer diagnostics to `navigation_perf.log`;
- reports sampling, safety-validation and quality time for every support-level
  candidate.

This is intended to identify whether the next real optimization belongs in
adaptive spline sampling or obstacle validation instead of guessing from one
aggregate `trajectory_ms` value.

### Deterministic stress field

`src/game/scene/GameSceneSetup.cpp` now extends the existing Hub guidance lab.
Instead of only one cube and one cylinder, the scene contains the two original
docking targets plus 16 deterministic stress objects (8 cubes, 8 cylinders).

All stress objects are real Hub-attached `StaticObject`s with fixed positions and
orientations. The existing client planning snapshot converts them through
`NavigationObstacleFactory`, so visual clutter and navigation workload use the
same runtime objects.

Expected obstacle count for the diagnostic route scene is approximately 19:
station + 2 docking targets + 16 stress objects.

## Runtime logging

Detailed navigation performance is now isolated in:

```text
navigation_perf.log
```

The large `[M8E-XPROC]` / `[M8E-STARTUP]` console stream is opt-in runtime tracing
controlled by `ELITE_TRACE_RUNTIME`; disable it for normal navigation profiling.

Existing concise `[DockingPerf]`, `[GuidanceReplan]`, `[FramePerf]` and failure
messages remain on console for correlation with the file trace.

## Navigation architecture baseline

`src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md` remains the authority.
The intended split is:

```text
global coarse route
    -> bounded detailed local motion
    -> per-physics-tick execution
    -> cheap guidance-tunnel presentation
```

Collision/safety fidelity remains non-negotiable. Performance work must reduce
redundant search/sampling or use acceleration structures / scheduling, not weaken
geometry checks.

## Current acceptance gate

Run under MSYS2 MinGW64:

```bash
unset ELITE_TRACE_RUNTIME
rm -f navigation_perf.log

python tests/architecture_contracts/check_live_docking_guidance.py
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Then run `EliteGame`, confirm the stress field visually, calculate one docking
route, fly for several seconds, and inspect `navigation_perf.log` together with
the concise console timing lines.

The next optimization follows measured phase ownership from that file.
