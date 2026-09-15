# Game Runtime Decomposition

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 runtime seams + dual-source model ingress accepted; OpenGL 4.3 Core accepted locally; active work is live navigation decomposition/performance

## Purpose

Keep deterministic gameplay/runtime boundaries explicit while modernizing
presentation and navigation infrastructure independently. Renderer changes do
not move gameplay authority.

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

The graphical client targets OpenGL 4.3 Core:

- Core GLFW profile request;
- bundled GLAD `gl:core=4.3`;
- zero forbidden fixed-function/Compatibility tokens under production `src/`;
- old presentation semantics translated by `CoreGlLegacyBridge` into software
  state and Core shader/VAO/VBO submission.

The branch-wide Core cutover and local visual runtime gate are accepted. GPU-P0
System Map spheres and GPU-P0.1 instanced circles/orbits are also accepted.

This remains presentation infrastructure. Authoritative simulation, ship
physics, route/docking decisions, damage, economy, replication,
interaction/picking semantics and navigation remain CPU-owned unless an explicit
architecture decision says otherwise.

## Navigation decomposition

The client CPU -> GPU audit is complete, but the active performance problem is
currently CPU navigation rather than renderer submission.

The accepted navigation design is documented in:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

The key runtime split is:

```text
global coarse route
    -> bounded local detailed motion
    -> per-tick execution/control
    -> cheap guidance presentation
```

The live docking request still has a synchronous initial solve inside
`SpaceState::update()`. NAV-LIVE-1 first removes unnecessary whole-route work from
rolling tunnel reconnects; later work will move immutable expensive global solves
off the frame thread when live timings identify the dominant stage.

## Planned order from here

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. Client CPU -> GPU audit — complete.
4. OpenGL 4.3 Core migration + local visual acceptance — accepted.
5. GPU-P0/P0.1 System Map work — accepted; renderer wave paused.
6. NAV-LIVE-1 bounded rolling docking reconnect — active acceptance gate.
7. Use `[DockingPerf]` to select the next global navigation optimization.
8. Move immutable expensive navigation solves off the frame thread with
   latest-request-wins where measured cost justifies it.
9. Resume selective GPU-offload priorities from
   `CLIENT_GPU_OFFLOAD_AUDIT.md` / `GPU_OFFLOAD_PLAN.md`.
10. Resume first read-only runtime-model consumer migration.
11. Continue R1+ runtime decomposition.

## Testing policy

Renderer contract:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
```

Navigation acceptance currently includes:

```bash
python tests/architecture_contracts/check_live_docking_guidance.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
```

Future renderer changes must not reintroduce Compatibility OpenGL. Future
navigation optimizations must not reduce canonical collision/safety fidelity.
