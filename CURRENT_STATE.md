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
**Navigation:** NAV-RUCKIG-0 isolated spike accepted; NAV-LIVE-1 active

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared
runtime/deterministic geometry seams.

Dual-source model ingress remains accepted:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` is the single schema/version authority.
Runtime-model consumer migration remains queued behind current
navigation/performance work.

## Renderer status

OpenGL 4.3 Core is accepted locally. GPU-P0 resident System Map textured spheres
and GPU-P0.1 instanced repeated planar circles/orbits are accepted after visual
runtime confirmation. The renderer wave is paused.

The old station-adjacent renderer freeze predates renderer modernization and is
still a separate deferred issue. Do not conflate it with docking guidance stalls.

## Ruckig status

Ruckig Community Edition v0.19.4 / commit
`a8db97a4e9c55e5160a3855f739fa3b270df8e4c` is accepted under MIT; license text
is retained in `src/assets/licenses/RUCKIG-MIT.txt` and provenance in
`THIRD_PARTY_LICENSES.md`.

NAV-RUCKIG-0 is accepted locally. The production Ruckig-first
`LocalGuidancePlanner::predictLeg()` remains useful, with legacy shooting fallback
and diagnostics, but it is **not** the live `CALCULATE ROUTE` docking path.

## Live docking path

The active client path is:

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner::plan()
    -> world::navigation::TrajectoryGenerator::generate()
    -> GuidanceTunnelBuilder::build()
    -> GuidanceState publication
```

The initial solve still runs synchronously inside `SpaceState::update()`, so a
slow snapshot/geometric/trajectory/tunnel stage can block a frame.

NAV-LIVE-0 already added:

- live current-pose tunnel refresh at the existing 4 Hz check;
- `[DockingPerf] total/snapshot/geometric/trajectory/tunnel` timing;
- `[GuidanceReplan] ... build_ms=...` timing;
- `tests/architecture_contracts/check_live_docking_guidance.py`.

## NAV-LIVE-1

Code now targets the main repeated-work defect in rolling guidance: a current
pose correction must not re-smooth the complete remaining route.

For a stable terminal the reconnect horizon is derived from solve latency,
braking distance, turn demand and safety margin. Only the near reconnect is
smoothed/collision-validated. The remainder is the previously accepted immutable
trajectory and is stitched back on unchanged, so the HUD still reaches the true
docking terminal.

Material terminal motion or an infeasible bounded reconnect uses the previous
full-safe reconnect fallback.

Dedicated behavior coverage is added as
`tests/navigation_guidance/GuidanceTunnelLocalHorizonTests.cpp` and is included
by the existing `tests/navigation_guidance/run_mingw64.sh` CTest run.

## Navigation architecture baseline

`src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md` is now the architecture
authority for this work. It fixes the intended split:

```text
global coarse route
    -> bounded detailed local motion
    -> per-physics-tick execution
    -> cheap guidance-tunnel presentation
```

It also records:

- the physical horizon formula;
- adaptive/swept collision-validation requirement;
- the distinction between render, collision, hit and navigation geometry;
- the requirement that navigation geometry preserve real holes/passages;
- deterministic obstacle/corridor/moving-object/cargo test scenes;
- the current `GeometricPathPlanner` visibility-graph complexity risk.

## Current acceptance gate

Run:

```bash
python tests/architecture_contracts/check_live_docking_guidance.py
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Then reproduce the same runtime manoeuvre and compare `[GuidanceReplan] build_ms`
plus `[DockingPerf]` against the NAV-LIVE-0 baseline.

The next optimization must follow measured ownership of the remaining stall.
Safety/collision fidelity is not negotiable for performance.
