# Game Runtime Decomposition

**Started:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** Wave R0 candidate — navigation geometry + shared CPU assembly geometry

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

## Wave R0 — seed navigation geometry boundary

Owned implementation files:

- `src/world/navigation/NavigationObstacleGeometry.cpp`
- `src/world/navigation/GeometricPathPlanner.cpp`

Required properties:

- compiled exactly once in `EliteNavigationGeometry`;
- linked by both `EliteGame` and `EliteServer`;
- no render/UI/window/client/server/platform dependencies;
- public behavior unchanged;
- architecture contract protects ownership.

R0 is deliberately small. It proves the method before `SpaceState`, `GameSimulation`, `GameClient` or `GameServer` are touched.

### R0 dependency finding: shared CPU assembly geometry

The first full headless link exposed an existing source-ownership defect: authoritative code calls `AssemblyMeshLibrary`, but `EliteServer` did not compile or link that implementation. Rather than copy client `.cpp` entries into the server, R0 introduces `EliteAssemblyGeometry` for `ObjLoader`, `ObjectAssemblyRegistry` and `AssemblyMeshLibrary`. This is a **STATEFUL SERVICE** boundary, not a PURE library: it owns a synchronized CPU cache and performs asset I/O. It must remain free of OpenGL/UI/client/server dependencies and is shared by both runtime executables. `EliteModelAsset` remains a lower dependency because `ObjLoader` reuses `RuntimeMeshNormalizer`.

## Planned order

1. **R0:** seed `EliteNavigationGeometry` physical boundary.
2. **R1:** dependency audit and expansion toward `EliteNavigationCore`; separate pure/deterministic planners from workspace/presentation/effects.
3. **R2:** extract `EliteSimulationPolicy` from already policy-shaped activation/replication calculations where ownership permits.
4. **R3:** split portable protocol/codecs from transport effects.
5. **R4+:** progressively establish world/ship/simulation/server/client/presentation libraries.
6. **Late wave:** reduce `SpaceState` to a composition/coordinator shell; do not start by carving it blindly.

## Testing policy

Normal development uses impact-based tests selected from changed files/libraries. Each library owns an architecture contract that checks source ownership and forbidden dependencies. Every accepted wave must also build every executable that consumes the changed library. Milestones run the full ready regression suite.

For R0:

```bash
python tests/architecture_contracts/check_game_runtime_library_boundaries.py
cmake --build build --target EliteGame
cmake --build build/headless_server --target EliteServer
```

Relevant navigation regression suites remain authoritative for behavioral changes; R0 itself is intended to be build-only/no-behavior-change.

## State discipline

Architectural state, current intent and next work are repository data. Every completed wave updates:

- `CURRENT_STATE.md` — what is true now;
- `CURRENT_TASK.md` — immediate next acceptance target;
- this file — long-lived decomposition intent, boundaries and wave history.

Chat history is not the canonical project state.
