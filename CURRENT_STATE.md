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
**Navigation:** NAV-RUCKIG-0 isolated spike accepted; active task is NAV-LIVE-0 on the actual runtime docking path

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared deterministic/runtime geometry seams. Dual-source model ingress remains accepted and unchanged:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration remains queued behind current navigation/performance work.

## Renderer status

OpenGL 4.3 Core is accepted locally. GPU-P0 resident System Map textured spheres and GPU-P0.1 instanced repeated planar circles/orbits are accepted locally after visual runtime confirmation. The renderer wave is paused.

The station-adjacent freeze predates renderer modernization and remains explicitly deferred. It must not be conflated with the docking-guidance stall discussed below.

## Ruckig status

Ruckig Community Edition is approved under MIT and pinned to:

```text
release: v0.19.4
commit:  a8db97a4e9c55e5160a3855f739fa3b270df8e4c
```

The exact MIT text is retained at `src/assets/licenses/RUCKIG-MIT.txt`; provenance and redistribution obligations are recorded in `THIRD_PARTY_LICENSES.md`.

NAV-RUCKIG-0 is accepted locally. The MinGW spike passes stationary transfer, orbital-scale local-frame motion, gravity-compensated motion and infeasible-horizon rejection. Integration fixes retained permanently are:

- target-local `_USE_MATH_DEFINES` for Ruckig v0.19.4 under MinGW C++20;
- conservative conversion of Elite scalar acceleration/jerk magnitude limits to Ruckig per-axis limits using `L / sqrt(3)`, followed by authoritative scalar validation.

The branch also contains a production Ruckig-first `LocalGuidancePlanner::predictLeg()` with the legacy shooting predictor retained as fallback and explicit attempt/fallback/timing diagnostics.

## Critical runtime finding

The user's real docking-route test still shows obvious frame stalls and a stale cockpit tunnel after hull-pose changes. This means the Ruckig production A/B candidate is **not accepted as an end-to-end fix**.

Code tracing found why Ruckig did not affect this test: the current `CALCULATE ROUTE` path does not call `LocalGuidancePlanner::plan()`.

The live path is:

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner::plan()
    -> world::navigation::TrajectoryGenerator::generate()
    -> GuidanceTunnelBuilder::build()
    -> GuidanceState publication
```

That chain currently executes synchronously from `SpaceState::update()`. Any expensive solve in it therefore blocks the client frame directly.

## NAV-LIVE-0 — current-pose tunnel correction + real-path timing

Commit:

```text
5f60021cd8339efce7c589a9618864079931a0ff
navigation: refresh live docking tunnel from current pose
```

The manual cockpit tunnel previously kept one immutable generation until a relatively large boundary/prediction/course event. Because attitude was only an explicit fallback at low speed, a moving or drifting ship could visibly change hull pose while the tunnel remained tied to the older generation.

The rolling 4 Hz check now requests a `ReconnectCurrentPose` generation when:

- current lateral offset exceeds 8% of the current tolerance, clamped to 0.75..3.0 m;
- current vertical offset exceeds the analogous threshold;
- current hull attitude differs from the tunnel tangent by more than the existing 4 degree threshold.

A new generation starts from the live hull position/orientation/velocity and reconnects to the accepted physical trajectory. Old gates are not rigidly translated with the ship.

The active path now records timing for the actual expensive stages:

- planning-snapshot build;
- geometric docking path;
- trajectory generation;
- guidance-tunnel generation;
- total request time.

Runtime lines are emitted as `[DockingPerf] ...`, while rolling reconnects include `build_ms` in `[GuidanceReplan] ...`.

Permanent guard:

```text
tests/architecture_contracts/check_live_docking_guidance.py
```

## Current acceptance gate

Run:

```bash
python tests/architecture_contracts/check_live_docking_guidance.py
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Then reproduce the same runtime manoeuvre and capture one `[DockingPerf]` line plus nearby `[GuidanceReplan]` lines.

The next optimization decision must target the measured live stage. If the immutable geometric/trajectory solve dominates, it should leave the frame thread and use latest-request-wins publication. If the rolling tunnel solve dominates, reconnect must become a cheaper bounded local operation and/or asynchronous. Safety fidelity must not be reduced merely to mask a stall.
