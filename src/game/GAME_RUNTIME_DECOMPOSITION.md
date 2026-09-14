# Game Runtime Decomposition

**Started:** 2026-09-14  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 runtime seams + dual-source model ingress accepted; OpenGL 4.3 Core modernization active

## Purpose

Convert logical runtime boundaries into explicit compile-time libraries and narrow APIs without changing gameplay behavior. Deterministic navigation/simulation-policy layers remain independent from render/UI/client/server/platform effects.

## Accepted runtime seams

`EliteNavigationGeometry` owns deterministic obstacle geometry/path planning. `EliteAssemblyGeometry` owns shared CPU OBJ hydration/assembly caching.

Runtime model ingress is accepted through one canonical `ModelAsset` seam:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. First read-only runtime consumer migration is queued.

## Client rendering modernization boundary

OpenGL modernization is presentation infrastructure and does not change gameplay authority.

Required order:

```text
4.3 Compatibility runtime scaffold
    -> compatibility inventory + incremental retirement
    -> 4.3 Core Profile client accepted
    -> fresh performance baseline
    -> selective CPU/GPU offload
```

Authoritative simulation, route/docking decisions, damage, economy, replication and other gameplay state remain CPU-owned.

Detailed renderer migration: `src/render/GL43_MODERNIZATION_PLAN.md`.

## GL43 current checkpoint

Two visual parity checkpoints are accepted:

- B1: `LocalMapPrimitiveRenderer` moved line/cross/circle submission from immediate mode to GLSL 4.30 + VAO/VBO with no visible change;
- B2a: `DetailMapGeometryPass` moved to explicit-color modern primitives, including orbit rendering, again with no visible change.

The architecture boundary now contains two full no-compatibility zones:

- `DetailMapGeometryPass.cpp`;
- `HubMapGeometryPass.cpp`.

`HubMapGeometryPass` is the current B2b candidate: its fallback box/axis/velocity/grid/screen-marker drawing now uses explicit-color local-map primitives and no longer uses tracked fixed-function/immediate APIs.

Temporary no-color primitive overloads remain for `DetailMapPlanetPass`, `HubMapBackend` and `HubMapPlanetPass`. They still bridge through `GL_CURRENT_COLOR`; the shared primitive seam is not closed until those callers migrate and the bridge is deleted.

## GL43 modernization final acceptance

The phase is complete only when:

- `EliteGame` builds on MinGW64;
- runtime creates OpenGL 4.3+ Core Profile;
- bundled GLAD exposes Core 4.3;
- production client code has no forbidden compatibility-only rendering;
- the architecture contract prevents regressions;
- flight, cockpit/rear view, Galaxy/System/Detail/Hub and close-navigation HUD pass visual smoke;
- no working feature is deleted to achieve Core compatibility.

## CPU -> GPU audit status

`src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` remains the evidence base, but implementation is blocked until Core acceptance.

Preserved later candidates include System Map static-sphere conversion and profile-gated scene culling/LOD. Navigation/guidance, gameplay/shared physics, replication state and CPU interaction semantics remain CPU.

## Planned order

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. Client CPU -> GPU audit — complete.
4. **GL43-A:** compatibility inventory/guard — established.
5. **GL43-B:** shared local/screen primitive foundation — B1 and B2a visually accepted; B2b active, Hub geometry candidate clean.
6. **GL43-C:** remaining Detail Map compatibility removal.
7. **GL43-D:** remaining Hub/local celestial compatibility removal.
8. **GL43-E:** overlays/debug/all remaining inventory debt.
9. **GL43-F:** GLAD/context Core 4.3 cutover + complete visual smoke.
10. Fresh performance baseline, then selective offload.
11. Resume first read-only runtime-model consumer migration.
12. Continue R1+ runtime decomposition.

## Testing policy

Accepted runtime baseline checks remain:

```bash
python tests/architecture_contracts/check_game_runtime_library_boundaries.py
python tests/architecture_contracts/check_game_runtime_shared_geometry_boundary.py
python tests/architecture_contracts/check_runtime_model_asset_ingress.py
python tests/architecture_contracts/check_html_ui_resource_pack_api.py
```

Current GL43 check:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
```

Every GL43 wave additionally requires visual parity smoke appropriate to the changed paths.

## State discipline

Every completed GL43 wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, this file and `src/render/GL43_MODERNIZATION_PLAN.md`. `GPU_OFFLOAD_PLAN.md` changes only when sequencing or offload conclusions change. Runtime model-ingress docs change only when that boundary changes.
