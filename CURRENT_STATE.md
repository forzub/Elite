# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core accepted locally  
**GPU-P0/P0.1:** accepted locally  
**Navigation:** NAV-RUCKIG-0 isolated solver accepted earlier; NAV-RUCKIG-1 live cutover active

## Accepted non-navigation baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared
deterministic/runtime geometry seams.

Dual-source model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority.
OpenGL 4.3 Core and the first System Map GPU migration waves remain accepted.
Renderer work is paused while navigation is stabilized.

## Live navigation architecture

The current docking path is:

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner::plan()
    -> TrajectoryGenerator::generate() compatibility facade
    -> game::navigation::RuckigRoutePlanner::plan()
    -> RuckigTrajectorySolver::solve() per local leg
    -> exact swept navigation-obstacle validation
    -> GuidanceState publication
```

Manual rolling guidance is now:

```text
accepted Ruckig route trajectory
    -> GuidanceTunnelBuilder::build(TrajectoryBackbone)   initial presentation

material live-pose change
    -> GuidanceTunnelBuilder::build(ReconnectCurrentPose)
    -> bounded Ruckig current-state -> accepted-state rejoin
    -> swept collision validation
    -> accepted immutable tail
```

There is no `SmoothPathOptimizer` call in either live trajectory generation or
live rolling reconnect.

## Ruckig route baseline

`src/game/navigation/RuckigRoutePlanner.h` is the canonical route-to-trajectory
API. The implementation currently lives behind the old
`world::navigation::TrajectoryGenerator` compatibility facade while call sites
are migrated.

The baseline is deliberately conservative:

- coarse obstacle topology remains owned by `GeometricPathPlanner` /
  `DockingPathPlanner`;
- each consecutive coarse leg is solved by Ruckig;
- intermediate topology vertices currently terminate with zero velocity;
- generated motion is swept against canonical navigation obstacles;
- target-obstacle ingress is allowed only after authored ingress progress;
- the exact terminal pose/orientation remains a separate contract.

This may produce stop-and-go behavior at coarse vertices. That is accepted for
this cutover gate. Continuous through-waypoint motion will be implemented as a
bounded Ruckig transition only after collision validation proves the blend safe.

## Ruckig rolling reconnect

The old reconnect generated spline/bulge/loop candidate families. That code has
been removed from `GuidanceTunnel.cpp`.

Stable terminal:

```text
lookahead = latency + braking + turning + safety margin
live position/velocity -> Ruckig -> accepted local rejoin state
accepted tail reused unchanged
```

The rejoin keeps the accepted trajectory velocity rather than fabricating a
visual-only curve. The final tunnel gate remains the real accepted docking
terminal.

Moved terminal or failed bounded join:

- a full Ruckig reconnect is attempted through sparse accepted-route supports;
- a final docking-axis alignment support is inserted;
- every Ruckig leg remains collision checked;
- there is no fallback to the custom spline smoother.

## Historical custom-smoother baseline

The previous 19-obstacle measurements remain useful only as comparison data:

```text
[DockingPerf]
total_ms      ~= 470.3
snapshot_ms   ~=   0.41
geometric_ms  ~=   7.42
trajectory_ms ~= 373.3
tunnel_ms     ~=  15.5
```

The same old capture showed 39 rolling reconnect attempts × 7 curve candidate
families and roughly 14.4 seconds of smoother CPU in a short run. Do not optimize
or restore that path.

## Contracts/tests

New hard architecture guard:

```bash
python tests/architecture_contracts/check_ruckig_live_navigation.py
```

It requires Ruckig in both live motion paths, requires canonical swept obstacle
validation and rejects `SmoothPathOptimizer` references there.

`guidance_tunnel_local_horizon_tests` now links `EliteNavigationRuckig` and no
longer compiles `SmoothPathOptimizer.cpp`.

The large legacy `NavigationGuidanceTests.cpp` still contains old explicit
B-spline/SmoothPath expectations. Those tests are the next migration item. A
failure limited to those stale assertions must not cause the removed runtime
backend to be restored.

## Remaining cleanup

- migrate/delete the obsolete B-spline tests;
- remove `SmoothPathOptimizer.cpp` from production `EliteGame`/`EliteServer`
  source lists after confirming no remaining runtime consumer;
- ideally move the full `RuckigRoutePlanner` implementation into its own
  translation unit after the compatibility seam is stable;
- measure the new docking stress scene;
- add collision-safe bounded through-waypoint Ruckig blending if needed;
- move heavy immutable planning off the client update thread if it still causes
  visible stalls.

## Current local acceptance

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

No claim of compile/runtime acceptance is made until the MinGW build and focused
navigation tests are run locally.
