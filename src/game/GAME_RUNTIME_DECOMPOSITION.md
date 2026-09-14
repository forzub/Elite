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

## GL43-A / GL43-B1 checkpoint

The current candidate establishes the first enforceable migration boundary:

- `check_gl43_modernization_boundary.py` scans all production C/C++ for compatibility-only OpenGL and prints the debt inventory;
- the contract locks the temporary 4.3 Compatibility scaffold while migration is incomplete;
- `LocalMapPrimitiveRenderer.cpp` retires immediate-mode submission in favor of GLSL 4.30 + VAO/VBO;
- the contract permanently forbids `glBegin/glEnd` and immediate `glVertex*/glColor*` from returning to that migrated submission seam.

`LocalMapPrimitiveRenderer` still reads `GL_CURRENT_COLOR` as a transitional bridge for unchanged callers. B2 removes that hidden state by making color explicit.

This checkpoint is a code candidate until local architecture test, `EliteGame` build and visual smoke pass.

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
4. **GL43-A:** inventory/guard + local Compatibility acceptance — candidate, local validation pending.
5. **GL43-B:** shared local/screen primitive foundation; B1 immediate-mode retirement candidate, B2 explicit color next.
6. **GL43-C:** Detail Map compatibility removal.
7. **GL43-D:** Hub/local celestial compatibility removal.
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

GL43 candidate check:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
```

Every GL43 wave additionally requires visual parity smoke appropriate to the changed paths.

## State discipline

Every completed GL43 wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, this file and `src/render/GL43_MODERNIZATION_PLAN.md`. `GPU_OFFLOAD_PLAN.md` changes only when sequencing or offload conclusions change. Runtime model-ingress docs change only when that boundary changes.
