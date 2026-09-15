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
**Navigation:** NAV-RUCKIG-0 isolated spike **ACCEPTED locally**; NAV-RUCKIG-1 production A/B integration is active

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

The previous `LocalGuidancePlanner` generated every state-to-state leg through the sequential `TrajectoryPredictor` and could repeat a full prediction up to six times for shooting correction. Docking, detour and emergency branches multiply those leg solves. Safety evaluation is a separate CPU stage and remains authoritative.

A direct compute-shader port of one sequential trajectory is not the current direction. The active experiment is to replace normal state-to-state leg generation with a cheap tested trajectory generator while retaining the legacy path as reference/fallback.

## NAV-RUCKIG-0 — ACCEPTED local spike

Ruckig Community Edition is approved under MIT and pinned to:

```text
release: v0.19.4
commit:  a8db97a4e9c55e5160a3855f739fa3b270df8e4c
```

The exact upstream MIT text is retained at `src/assets/licenses/RUCKIG-MIT.txt`; provenance and redistribution obligations are recorded in `THIRD_PARTY_LICENSES.md`.

The isolated MinGW spike now passes locally end-to-end. During bring-up it exposed and fixed two integration issues:

1. upstream v0.19.4 uses `M_PI` under strict C++20, so MinGW receives target-local `_USE_MATH_DEFINES`;
2. Elite defines proper acceleration/jerk limits as Euclidean vector magnitudes while Ruckig accepts independent per-axis bounds, so scalar limit `L` is conservatively mapped to `L / sqrt(3)` per axis and then revalidated against the authoritative scalar envelope.

The accepted spike covers stationary transfer, orbital-scale coordinates, gravity-compensated co-moving motion, infeasible-horizon rejection and a 500-solve benchmark. The user confirmed the complete test run passed; the numeric benchmark line was not copied into chat, so no speed ratio is claimed here yet.

## NAV-RUCKIG-1 — production A/B candidate

The branch now contains the first production integration candidate.

### Build boundary

`cmake/EliteRuckigNavigation.cmake` owns the shared pinned dependency setup:

- exact reviewed commit;
- Community Edition only;
- cloud client/examples/upstream tests/benchmark/Python/shared build disabled;
- generic upstream cache options are restored after dependency configuration so Ruckig cannot silently alter unrelated Elite build policy;
- MinGW `M_PI` compatibility remains target-local;
- upstream Ruckig and `EliteNavigationRuckig` compile as private C++20 targets while `EliteGame` / `EliteServer` remain C++17.

Both client and server link `EliteNavigationRuckig`. The navigation-guidance regression project uses the same production seam.

### Planner boundary

`LocalGuidancePlanner::predictLeg()` is now Ruckig-first:

```text
state-to-state leg request
    -> RuckigTrajectorySolver
    -> if accepted: use candidate
    -> if rejected/fails: legacy six-iteration shooting predictor
    -> TrajectorySafetyEvaluator unchanged
```

No obstacle, restricted-volume, scheduled-traffic, docking-policy or authority logic moved into Ruckig.

Per-plan `LocalGuidanceBackendDiagnostics` now records:

- Ruckig leg attempts;
- Ruckig leg successes;
- Ruckig fallbacks;
- actual legacy `TrajectoryPredictor` call count;
- accumulated Ruckig solve microseconds;
- accumulated legacy-fallback microseconds;
- last Ruckig failure message.

This is explicitly an A/B acceptance stage. The legacy predictor is not deleted.

### Permanent guard

`tests/architecture_contracts/check_ruckig_navigation_integration.py` protects:

- the exact upstream pin and Community-only build boundary;
- C++20 isolation;
- MinGW portability shim;
- client/server/guidance-test linkage;
- Ruckig-first ordering;
- retained legacy fallback;
- exposed attempt/fallback/timing diagnostics;
- runtime license registry status.

## Current acceptance gate

Local validation is now required for NAV-RUCKIG-1:

```bash
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
bash tests/navigation_ruckig/run_mingw64.sh
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

If those pass, run the game and reproduce the route calculation that previously stalled the machine. The next decision must be based on runtime wall time plus Ruckig/fallback counters, not on the isolated microbenchmark alone.
