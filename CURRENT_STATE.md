# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core accepted locally; final clean rebuild after tail cleanup still pending  
**GPU-P0:** System Map static textured spheres accepted locally  
**GPU-P0.1:** repeated planar System Map circles — contracts PASS; final build/runtime acceptance still pending  
**Active navigation experiment:** NAV-RUCKIG-0 — isolated Ruckig state-to-state trajectory spike

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared deterministic/runtime geometry seams. Dual-source model ingress remains accepted and unchanged:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration remains queued behind current navigation/performance work.

## OpenGL 4.3 Core / System Map GPU work

The OpenGL 4.3 Core runtime baseline was accepted locally. Later clean builds exposed compatibility-profile tails outside the new System Map code; those tails were removed and the GL43 architecture guard strengthened.

GPU-P0 static textured spheres are accepted. GPU-P0.1 repeated planar circles remain an acceptance candidate: the three architecture contracts passed locally, but the final clean `EliteGame` rebuild/runtime visual smoke after the Core-tail cleanup has not yet been reported. Do **not** mark GPU-P0.1 accepted merely because navigation work has started.

The station-adjacent freezes predate renderer modernization and remain explicitly deferred.

## Navigation performance problem

The current `LocalGuidancePlanner` can perform repeated full numerical predictions. `predictLeg()` uses shooting correction with up to six `TrajectoryPredictor` runs; docking, detour and emergency branches can multiply that work. The predictor itself integrates sequentially with small time steps and repeated gravity samples. Safety evaluation is a separate CPU stage and remains authoritative.

A direct compute-shader port of one sequential trajectory is still not the preferred first response. The user explicitly chose to try Ruckig as a replacement candidate for the expensive state-to-state leg generation.

## NAV-RUCKIG-0 — isolated spike

Ruckig Community Edition is approved under MIT and pinned for the spike to:

```text
release: v0.19.4
commit:  a8db97a4e9c55e5160a3855f739fa3b270df8e4c
```

The exact upstream MIT text is retained at `src/assets/licenses/RUCKIG-MIT.txt`; provenance and redistribution obligations are recorded in `THIRD_PARTY_LICENSES.md`.

### Isolation boundary

The first wave deliberately does **not** modify the production `LocalGuidancePlanner` or main `EliteGame` CMake graph.

Added:

- `src/game/navigation/RuckigTrajectorySolver.h` — Elite-only public seam; no Ruckig headers leak through it;
- `src/game/navigation/RuckigTrajectorySolver.cpp` — private Ruckig implementation;
- `tests/navigation_ruckig/` — isolated FetchContent build and executable tests;
- `tests/architecture_contracts/check_ruckig_navigation_spike.py` — permanent spike-boundary/provenance guard.

Ruckig v0.19.4 requires C++20. Only the isolated adapter target is compiled as C++20; Elite remains C++17 outside that private target.

The spike disables upstream cloud/client and nonessential build surfaces:

- `BUILD_CLOUD_CLIENT=OFF`;
- examples OFF;
- upstream tests OFF;
- benchmark target OFF;
- Python module OFF;
- shared library OFF.

A MinGW-only `_USE_MATH_DEFINES` definition is applied to the upstream `ruckig` target because v0.19.4 uses `M_PI` in strict C++20 mode. This remains target-local and does not alter the rest of Elite.

### Adapter model

The adapter does not ask Ruckig to solve directly in orbital-scale world coordinates. It constructs an accelerating co-moving terminal frame:

1. sample Elite gravity at the actor and target;
2. choose the mean as a local reference-frame acceleration;
3. express initial/terminal state in that frame;
4. let Ruckig generate a synchronized jerk-limited 3-DOF relative trajectory for the requested leg duration;
5. reconstruct world-space states;
6. resample Elite gravity along the candidate;
7. validate the actual scalar proper-acceleration and proper-jerk envelope;
8. return the normal `TrajectoryPredictionResult` shape for later planner integration.

Elite's motion envelope is a scalar Euclidean vector limit. Ruckig's acceleration/jerk constraints are per-axis. The first executable run exposed this mismatch: the stationary single-axis case passed, while the orbital-scale multi-axis case exceeded the scalar Elite jerk envelope because each Ruckig axis had been given the full scalar limit.

The adapter now conservatively maps scalar limit `L` to an axis-aligned Ruckig box with half-width `L / sqrt(3)`, which is inscribed in the Elite spherical envelope. The existing gravity-frame allowance remains additive per axis, and final world-space scalar validation remains authoritative. Failure diagnostics now print observed max versus limit.

Ruckig still does not own obstacle/traffic safety, route policy, docking semantics, ship authority or execution.

### Spike acceptance tests

`tests/navigation_ruckig/RuckigTrajectorySolverTests.cpp` covers:

- 100 m local state-to-state transfer;
- orbital-scale world coordinates with a small local manoeuvre;
- Earth-like gravity with the accelerating co-moving frame;
- rejection of an infeasible short horizon;
- a non-gating 500-solve wall-time benchmark that prints average microseconds/solve.

Local MinGW results so far:

- architecture contract: PASS;
- Ruckig build/link: PASS after target-local `M_PI` compatibility fix;
- stationary local transfer: PASS;
- orbital-scale case: initially rejected by scalar-vs-per-axis jerk mismatch; adapter correction is now committed and requires rerun.

## Next decision after NAV-RUCKIG-0 local result

If the isolated tests pass and solve time is materially lower than the current shooting predictor, the next wave is production A/B integration:

- build Ruckig behind a private C++20 navigation library in the main graph;
- make state-to-state leg generation Ruckig-first while retaining the existing CPU predictor as deterministic fallback/reference;
- preserve `TrajectorySafetyEvaluator` unchanged;
- add counters comparing Ruckig solve/fallback counts and wall time;
- only then reproduce the route freeze in the real game.

If the spike fails numerically or is not materially faster, do not force it into production; use the measured failure mode to choose the next planner optimization.
