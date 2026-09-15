# Game Runtime Decomposition

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 runtime seams + dual-source model ingress + OpenGL 4.3 Core accepted; active work is canonical Ruckig navigation acceptance

## Purpose

Keep deterministic gameplay/runtime boundaries explicit while renderer,
asset-ingress and navigation infrastructure evolve independently. Presentation
never owns simulation or navigation authority.

## Accepted runtime seams

`EliteNavigationGeometry` owns deterministic obstacle/path geometry.
`EliteAssemblyGeometry` owns shared CPU assembly geometry.

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority.

## Renderer boundary

The graphical client targets OpenGL 4.3 Core. The Core cutover, GPU-P0 System Map
spheres and GPU-P0.1 instanced circles/orbits are accepted locally. Renderer work
is paused while navigation is stabilized.

Authoritative simulation, physics, navigation, damage, economy and replication
remain CPU-owned unless a later explicit architecture decision changes that.

## Navigation decomposition

Authoritative design:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

Current runtime ownership:

```text
planning snapshot
    -> GeometricPathPlanner / DockingPathPlanner   topology
    -> RuckigRoutePlanner                          constrained route motion
    -> RuckigTrajectorySolver                      local state-to-state primitive
    -> swept NavigationObstacleGeometry validation safety
    -> GuidanceTunnel                              presentation sampling
    -> future trajectory follower                  execution
```

`TrajectoryGenerator` is temporarily the old compatibility facade for
`RuckigRoutePlanner`.

Clear internal topology vertices may retain a conservative non-zero through
velocity when a local corner-cut eligibility chord is free and the actual
adjacent Ruckig motion passes swept validation. Any failed blended leg relaxes
only adjacent waypoint velocities to zero and retries; no global smoother is
invoked.

`GuidanceTunnel` uses `RuckigTrajectorySolver` for rolling live-pose correction,
preferably over a bounded physical horizon, then stitches the immutable accepted
tail. Material terminal movement or an infeasible bounded join uses a full Ruckig
reconnect.

## Retired custom smoother

The old B-spline implementation has been removed from
`SmoothPathOptimizer.cpp`. Production behavior is now fail-closed. A minimal
non-smoothing compatibility branch exists only for the old all-in-one navigation
test target under `ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT`; root production CMake
does not enable it.

Hard contract:

```bash
python tests/architecture_contracts/check_ruckig_live_navigation.py
```

Focused runtime-motion coverage:

```text
tests/navigation_guidance/RuckigRoutePlannerTests.cpp
tests/navigation_guidance/GuidanceTunnelLocalHorizonTests.cpp
```

The final obsolete global-B-spline assertion in `NavigationGuidanceTests.cpp`
still needs migration before the compatibility API/source can be removed from
the build lists entirely.

## Planned order from here

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. Client CPU -> GPU audit — complete.
4. OpenGL 4.3 Core + GPU-P0/P0.1 — accepted; renderer wave paused.
5. NAV-RUCKIG-0 isolated state-to-state solver — accepted earlier.
6. NAV-RUCKIG-1 live route/reconnect cutover + waypoint blending — implemented,
   local MinGW acceptance pending.
7. Migrate stale spline test; remove the compatibility smoother API/source and
   CMake entries completely.
8. Measure deterministic/stress docking scenes with `[RuckigRoutePerf]`,
   `[DockingPerf]` and `[GuidanceReplan]`.
9. Move immutable heavy planning off the frame thread with generation IDs /
   latest-request-wins if measured stalls remain.
10. Resume renderer/runtime-model decomposition work.

## Testing policy

Renderer contract:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
```

Navigation acceptance:

```bash
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
python tests/architecture_contracts/check_ruckig_live_navigation.py
python tests/architecture_contracts/check_live_docking_guidance.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

A failure confined to the named stale global-B-spline assertion is a test
migration issue. Focused Ruckig/local-horizon, collision, terminal-state,
compile/link or integration failures are blockers.
