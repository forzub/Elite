# Game Runtime Decomposition

**Started:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 physical seams established; dual-source runtime model ingress accepted; client GPU modernization active

## Purpose

Convert the game's existing logical subsystem boundaries into explicit compile-time libraries and narrow APIs without changing gameplay. The objective is smaller change blast-radius, impact-based testing and a dependency graph that makes architectural violations visible at compile/link time.

This track is independent from the existing server/client authority migration and map-decomposition progress recorded in `ARCHITECTURE_STATUS.md`. It changes ownership/build boundaries, not gameplay authority.

## Function classification

Runtime code is classified by behavior rather than by folder name:

1. **PURE** — output depends only on explicit input; no mutation or hidden effects.
2. **DETERMINISTIC STATE TRANSITION** — mutates only explicitly supplied state and reads all time/policy/input through parameters.
3. **STATEFUL SERVICE** — intentionally owns history/cache/state behind a narrow API; no unrelated I/O or presentation effects.
4. **EFFECT / ORCHESTRATION** — transport, threads, filesystem, clocks, OpenGL, UI, process lifecycle and top-level composition.

Not every function should be PURE. The architectural target is a pure/deterministic core surrounded by explicit stateful services and a thin effect shell.

## Dependency direction

```text
executables / composition roots
        |
        +--> client/server orchestration
        |         |
        |         +--> simulation/runtime services
        |                    |
        +--> presentation    +--> deterministic policy/core
        |         |                       |
        +--> render/UI                    +--> foundation/value types

transport effects --> protocol/codecs --> DTO/value types
```

Lower layers must never depend on higher layers. In particular, deterministic navigation/simulation-policy libraries may not include render, UI, window, WebView, client orchestration or server orchestration headers.

## Target library map

The exact source membership is discovered incrementally. Candidate domains are:

- `EliteFoundation` — IDs, coordinates, value types and cross-domain primitives;
- `EliteNavigationGeometry` / later `EliteNavigationCore` — obstacle geometry, path planning, trajectories, guidance mathematics;
- `EliteSimulationPolicy` — activation/cadence and other deterministic server policies;
- `EliteProtocol` — wire schema/codecs and replication merge primitives;
- `EliteTransport` — TCP/loopback/process transport effects;
- `EliteWorldRuntime` — CPU-only world/object/celestial runtime;
- `EliteShipRuntime` — ship core/equipment/damage/physics state transitions;
- `EliteSimulation` — authoritative `GameSimulation` orchestration over lower runtime libraries;
- `EliteServerRuntime` — sessions, publication, server runner/runtime/worker;
- `EliteClientRuntime` — `GameClient`, prediction/synchronization/client-world services;
- `ElitePresentationModel` — renderer-independent presentation builders/interpolators;
- `EliteRender` / `EliteUI` — OpenGL and user-interface effects.

This is a destination map, not permission to create all targets immediately. A library is created only after its dependency direction is traced and acyclic.

## Wave R0 — accepted physical seams

`EliteNavigationGeometry` owns:

- `src/world/navigation/NavigationObstacleGeometry.cpp`
- `src/world/navigation/GeometricPathPlanner.cpp`

Both `EliteGame` and `EliteServer` link the library; neither executable compiles these implementations directly. The library is protected against render/UI/window/client/server/platform dependencies.

The first full headless link also exposed an existing source-ownership defect around `AssemblyMeshLibrary`. R0 therefore established `EliteAssemblyGeometry` for `ObjLoader`, `ObjectAssemblyRegistry` and `AssemblyMeshLibrary`. This is a **STATEFUL SERVICE** boundary, not PURE: it owns synchronized CPU asset cache/I/O and remains free of OpenGL/UI/client/server dependencies. Both runtime executables share it.

## Runtime model-ingress transition — accepted seam

Disk model loading now has one canonical CPU ingress. `EliteRuntimeModelAssets` sits above `EliteModelAsset` and `EliteAssemblyGeometry`:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` is the only schema/version authority. Legacy data is adapted upward into the new schema; compiled `ModelAsset` is never degraded back into `ObjectAssembly`.

The transition seam is locally accepted on MinGW64: ingress contracts pass and `EliteRuntimeModelAssets`, `EliteServer` and `EliteGame` build/link. Production object types remain on legacy input by default until explicitly switched.

Next model-ingress work is consumer migration, not another loader: migrate one read-only `AssemblyMeshLibrary` consumer to `RuntimeModelAssetLibrary`, verify parity on the legacy backend, then switch only that object type to compiled binary.

Detailed ingress policy: `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md`.

## Client GPU modernization boundary

Client GPU modernization is a presentation/runtime-efficiency track, not a relocation of game authority. OpenGL 4.3+ is the new graphical-client baseline candidate so compute shaders and SSBOs can absorb sufficiently parallel presentation workloads.

The current implementation uses **OpenGL 4.3 Compatibility Profile** because legacy fixed-function presentation calls still exist. Core Profile is a later dependency-cleanup gate. `render::gpu::GlRuntimeCapabilities` is the startup capability authority; bundled GLAD must expose the same 4.3 compatibility API rather than hiding compute entry points behind an older loader.

GPU-derived results should normally remain GPU-resident through rendering. A design that requires `dispatch -> barrier -> synchronous readback -> gameplay decision` in the frame path is presumed architecturally wrong until proven otherwise by measurement.

Authoritative simulation, route decisions, damage, economy, replication and other gameplay state remain CPU-owned by default. Presentation-only derivatives may use GPU compute independently of authoritative CPU state.

Initial priorities:

- P0 — `PlanetaryWeatherMapGenerator`, `CloudAppearanceTextureGenerator`;
- P1 — `GalaxyStarfieldRenderer` derived transforms/filtering/draw preparation;
- P1/P2 — large instance/frustum visibility only after profiling;
- P2 — procedural celestial mesh/detail and `PlanetWireRenderer` only if rebuild cadence makes compute worthwhile.

General world-signal labels are disabled for now; Hub-map and close-navigation label paths remain active. `src/render/GPU_OFFLOAD_PLAN.md` is the authoritative GPU workload/migration plan.

The GL43 baseline implementation exists in commit `19db31eca5aa2c12475d87f2dfa322f794990c6e`, but remains a **candidate until local build/launch runtime acceptance** confirms actual >=4.3 context, compute/SSBO support and visual parity.

## Planned order

1. **R0:** seed `EliteNavigationGeometry` and shared CPU assembly geometry — accepted.
2. **GPU foundation interleave:** establish/accept GL43 capability baseline, then offload measured presentation hot paths.
3. **Runtime model ingress consumer migration:** first read-only consumer, legacy parity, then one-type `.elmodel` A/B switch.
4. **R1:** dependency audit and expansion toward `EliteNavigationCore`; separate pure/deterministic planners from workspace/presentation/effects.
5. **R2:** extract `EliteSimulationPolicy` from already policy-shaped activation/replication calculations where ownership permits.
6. **R3:** split portable protocol/codecs from transport effects.
7. **R4+:** progressively establish world/ship/simulation/server/client/presentation libraries.
8. **Late wave:** reduce `SpaceState` to a composition/coordinator shell; do not start by carving it blindly.

## Testing policy

Normal development uses impact-based tests selected from changed files/libraries. Each library owns an architecture contract that checks source ownership and forbidden dependencies. Every accepted wave must also build every executable that consumes the changed library. Milestones run the full ready regression suite.

For the accepted R0/runtime-ingress baseline:

```bash
python tests/architecture_contracts/check_game_runtime_library_boundaries.py
python tests/architecture_contracts/check_game_runtime_shared_geometry_boundary.py
python tests/architecture_contracts/check_runtime_model_asset_ingress.py
python tests/architecture_contracts/check_html_ui_resource_pack_api.py
cmake --build build --target EliteGame
cmake --build build/headless_server --target EliteServer
```

GPU waves additionally require runtime capability logging, CPU/GPU timing and visual/parity checks appropriate to the workload.

## State discipline

Architectural state, current intent and next work are repository data. Every completed wave updates:

- `CURRENT_STATE.md` — what is true now;
- `CURRENT_TASK.md` — immediate next acceptance target;
- this file — long-lived decomposition intent, boundaries and wave history;
- `src/render/GPU_OFFLOAD_PLAN.md` for GPU work;
- `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` for model-ingress work.

Chat history is not the canonical project state.
