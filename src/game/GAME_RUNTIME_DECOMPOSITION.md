# Game Runtime Decomposition

**Started:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 physical seams established; dual-source runtime model ingress accepted; client GPU audit complete; OpenGL 4.3 Core modernization active

## Purpose

Convert existing logical subsystem boundaries into explicit compile-time libraries and narrow APIs without changing gameplay behavior. The objective is smaller change blast-radius, impact-based testing and dependency direction that is visible at compile/link time.

Runtime code remains classified as PURE, deterministic state transition, stateful service or effect/orchestration. Deterministic navigation/simulation-policy layers may not acquire render/UI/client/server/platform dependencies.

## Accepted runtime seams

`EliteNavigationGeometry` owns deterministic obstacle geometry/path planning. `EliteAssemblyGeometry` owns shared CPU OBJ hydration/assembly caching. Both are shared by client and server rather than recompiled independently.

Runtime model ingress is accepted through one canonical `ModelAsset` seam:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. The first read-only consumer migration remains queued.

## Client rendering modernization boundary

OpenGL modernization is presentation infrastructure and does not change gameplay authority.

The current code candidate requests OpenGL 4.3 Compatibility Profile so the application can remain runnable while legacy presentation code is replaced. That profile is now explicitly temporary. The first rendering milestone is **OpenGL 4.3 Core Profile**, not compute offload.

The required order is:

```text
4.3 Compatibility runtime scaffold
    -> remove fixed-function/compatibility-only presentation
    -> 4.3 Core Profile client accepted
    -> performance baseline
    -> selective CPU/GPU offload
```

Authoritative simulation, route/docking decisions, damage, economy, replication and other gameplay state remain CPU-owned. A GPU design requiring synchronous render-GPU readback for gameplay is presumed wrong unless a future architecture explicitly changes that boundary.

Detailed renderer migration: `src/render/GL43_MODERNIZATION_PLAN.md`.

## GL43 modernization acceptance

The phase is complete only when:

- `EliteGame` builds on MinGW64;
- the runtime creates an OpenGL 4.3+ Core Profile context;
- bundled GLAD exposes the Core 4.3 API;
- production client code no longer depends on fixed-function/immediate-mode/matrix-stack compatibility calls;
- an architecture/static contract prevents reintroduction of those APIs;
- ordinary flight, cockpit/rear view, Galaxy/System/Detail/Hub maps and close-navigation HUD all pass visual smoke;
- no working feature is deleted merely to achieve Core compatibility.

Shader/VBO/VAO replacement of legacy primitives is allowed and expected during this phase. Compute/SSBO algorithmic offload is not required for acceptance.

## CPU -> GPU audit status

The completed audit in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` remains authoritative evidence, but implementation is blocked until the Core renderer gate passes.

Preserved later order:

- P0 — System Map textured-body CPU tessellation -> static indexed sphere + vertex shader, subject to measurement;
- P1, profile-gated — visual traffic culling/LOD/compaction;
- P1/P2 — repeated map primitives where Core migration has not already solved the problem;
- P2 — starfield only if scale/rebuild timing justifies it.

Navigation/guidance, client snapshot state, gameplay/shared physics, database parsing and CPU interaction semantics stay CPU by design.

## Planned order

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. Client CPU -> GPU audit — complete.
4. **GL43-A:** accept Compatibility scaffold and inventory all compatibility-only production calls.
5. **GL43-B..E:** migrate all currently working render paths to explicit shader/VAO/VBO/state ownership.
6. **GL43-F:** switch GLAD/context to OpenGL 4.3 Core Profile and pass complete visual smoke.
7. Capture fresh CPU/GPU performance baselines.
8. Resume selective offload from `GPU_OFFLOAD_PLAN.md`.
9. Resume first read-only runtime-model consumer migration.
10. Continue R1+ runtime library decomposition.

## Testing policy

Normal development uses impact-based architecture/regression tests plus every affected executable build. GL43 waves additionally require visual parity smoke at each meaningful checkpoint and a final static forbidden-API gate.

Accepted runtime baseline checks remain:

```bash
python tests/architecture_contracts/check_game_runtime_library_boundaries.py
python tests/architecture_contracts/check_game_runtime_shared_geometry_boundary.py
python tests/architecture_contracts/check_runtime_model_asset_ingress.py
python tests/architecture_contracts/check_html_ui_resource_pack_api.py
cmake --build build --target EliteGame
cmake --build build/headless_server --target EliteServer
```

The GL43 track must add its own compatibility-call architecture contract before final Core acceptance.

## State discipline

Every completed GL43 wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, this file and `src/render/GL43_MODERNIZATION_PLAN.md`. `GPU_OFFLOAD_PLAN.md` is updated when sequencing or offload conclusions change. `src/game/assets/RUNTIME_MODEL_ASSET_INGRESS.md` changes only when the asset boundary itself changes.

Chat history is not canonical project state.