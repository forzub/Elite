# Game Runtime Decomposition

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 runtime seams + dual-source model ingress + OpenGL 4.3 Core accepted; active work is canonical Ruckig navigation cutover

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
    -> RuckigRoutePlanner                          constrained motion
    -> RuckigTrajectorySolver                      local state-to-state primitive
    -> swept NavigationObstacleGeometry validation safety
    -> GuidanceTunnel                              presentation sampling
    -> future trajectory follower                  execution
```

`TrajectoryGenerator` is temporarily only the old public compatibility facade
for `RuckigRoutePlanner`. The custom global B-spline implementation has been
removed from that live path.

`GuidanceTunnel` also no longer owns a custom spline reconnect planner. A rolling
live-pose correction uses `RuckigTrajectorySolver`, preferably over a bounded
physical horizon, then stitches the immutable accepted tail. A materially moved
terminal or infeasible bounded join uses a full Ruckig reconnect rather than the
old smoother.

Hard contract:

```bash
python tests/architecture_contracts/check_ruckig_live_navigation.py
```

It rejects `SmoothPathOptimizer` in both live motion sources.

## Planned order from here

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. Client CPU -> GPU audit — complete.
4. OpenGL 4.3 Core + GPU-P0/P0.1 — accepted; renderer wave paused.
5. NAV-RUCKIG-0 isolated Ruckig state-to-state solver — accepted earlier.
6. NAV-RUCKIG-1 live route + rolling reconnect cutover — implementation active,
   local MinGW acceptance pending.
7. Migrate stale B-spline tests and remove the remaining legacy smoother from
   production targets/source when no runtime consumer remains.
8. Measure the deterministic/stress docking scenes using Ruckig diagnostics.
9. Add bounded collision-safe Ruckig through-waypoint blending if motion quality
   requires it.
10. Move immutable heavy planning off the frame thread with generation IDs /
    latest-request-wins if measured stalls remain.
11. Resume renderer/runtime-model decomposition work.

## Testing policy

Renderer contract:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
```

Navigation contracts:

```bash
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
python tests/architecture_contracts/check_ruckig_live_navigation.py
python tests/architecture_contracts/check_live_docking_guidance.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Future navigation work must not restore the old global smoother merely to satisfy
a stale test or make a corner look nicer. Motion quality improvements belong in
bounded Ruckig transitions plus canonical collision validation.
