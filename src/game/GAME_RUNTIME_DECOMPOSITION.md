# Game Runtime Decomposition

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 runtime seams + dual-source model ingress accepted; OpenGL 4.3 Core code migration complete, local runtime acceptance pending

## Purpose

Keep deterministic gameplay/runtime boundaries explicit while modernizing presentation infrastructure independently. Renderer changes do not move gameplay authority.

## Accepted runtime seams

`EliteNavigationGeometry` owns deterministic obstacle/path geometry. `EliteAssemblyGeometry` owns shared CPU assembly geometry.

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority.

## Renderer boundary

The graphical client source now targets OpenGL 4.3 Core:

- Core GLFW profile request;
- bundled GLAD `gl:core=4.3`;
- zero forbidden fixed-function/Compatibility tokens under production `src/`;
- old presentation semantics translated by `CoreGlLegacyBridge` into software state and Core shader/VAO/VBO submission.

Incremental Detail/Hub geometry migrations were visually accepted before the branch-wide cutover. The remaining compatibility debt across flight, maps, scene, HUD, radar/PPI, mini-camera, labels and debug/celestial presentation was then migrated through the common Core bridge.

This is still presentation infrastructure. Authoritative simulation, ship physics, route/docking decisions, damage, economy, replication, interaction/picking semantics and navigation remain CPU-owned.

## Acceptance status

Repository-side Core migration gates pass, including Windows/MSYS2 MinGW64 syntax validation of the bridge. Full local `EliteGame` build/runtime acceptance remains mandatory because the GitHub checkout lacks the required `third_party/webview` tree and cannot configure the complete graphical target.

Required local gate:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

## Planned order from here

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. Client CPU -> GPU audit — complete.
4. OpenGL 4.3 Core API migration — code-complete.
5. **Local Core build + complete visual runtime smoke — active gate.**
6. Capture fresh CPU/GPU baseline.
7. Resume selective GPU-offload priorities from `CLIENT_GPU_OFFLOAD_AUDIT.md` / `GPU_OFFLOAD_PLAN.md`.
8. Resume first read-only runtime-model consumer migration.
9. Continue R1+ runtime decomposition.

## Testing policy

Existing runtime architecture tests remain applicable. The permanent renderer contract is:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
```

Every future renderer change must keep that contract clean and must not reintroduce driver fixed-function/Compatibility OpenGL.
