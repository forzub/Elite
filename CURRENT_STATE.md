# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer modernization:** OpenGL 4.3 Core code migration complete; local runtime acceptance pending

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared deterministic/runtime geometry seams. Dual-source model ingress remains accepted and unchanged:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority. Runtime-model consumer migration stays queued behind renderer acceptance.

## OpenGL 4.3 Core status

The client source has crossed the API boundary to **OpenGL 4.3 Core Profile**.

Completed:

- `src/window/Window.cpp` requests `GLFW_OPENGL_CORE_PROFILE` with OpenGL 4.3;
- bundled GLAD 2.0.8 was regenerated for `gl:core=4.3`;
- `tests/architecture_contracts/check_gl43_modernization_boundary.py` requires zero forbidden fixed-function/compatibility API tokens under production `src/`;
- the final machine scan passes with zero offenders;
- `src/render/legacy/CoreGlLegacyBridge.h` preserves remaining legacy presentation semantics through software state + GLSL 4.30 Core + VAO/VBO submission rather than driver Compatibility state;
- the bridge owns software model-view/projection stacks, current color, texcoords and legacy texture-enable semantics where old presentation code still expects them;
- removed `GL_QUADS` submission is converted to Core `GL_TRIANGLES`;
- line/loop/strip/points/triangles/fan/strip submissions use `glDrawArrays`.

The prior incremental work remains accepted:

- GL43-B1 `LocalMapPrimitiveRenderer` immediate-mode replacement — visually accepted;
- GL43-B2a `DetailMapGeometryPass` explicit-color/Core cleanup — visually accepted;
- GL43-B2b `HubMapGeometryPass` explicit-color/Core cleanup — visually accepted.

A branch-wide audit then found 23 additional compatibility-dependent production files outside the original handwritten map list. All were migrated to the Core bridge or already-modern Core paths. The architecture scan, not the old handwritten inventory, is the authority.

## Validation evidence

Automated checks completed:

1. OpenGL 4.3 Core migration workflow: PASS.
   - generated GLAD `gl:core=4.3`;
   - zero forbidden compatibility-only tokens in `src/`;
   - `git diff --check` PASS;
   - migration committed as `37999c5588c6e85ef88a24e0efbf2dfff89b9314`.
2. Windows/MSYS2 MinGW64 Core boundary: PASS.
3. Windows/MSYS2 MinGW64 `g++ -std=c++17` syntax compilation of `CoreGlLegacyBridge.h` against the Core GLAD header: PASS.

A full GitHub MinGW64 `EliteGame` configure could not be used as an independent build gate because `CMakeLists.txt` requires `third_party/webview`, while that directory is not stored in this GitHub repository and is not a submodule. Configure stops before compiling game C++. This is an existing repository/dependency issue, not an observed GL4.3 compile failure.

## Final acceptance still required locally

On the developer machine, where the complete project tree including `third_party/webview` exists:

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

The runtime smoke must cover ordinary flight, cockpit/rear view, Galaxy/System/Detail/Hub maps, close-navigation HUD/labels, radar/PPI, mini-camera, world labels, debug grid when available, and visible Hub/cloud/atmosphere paths.

Until that build + runtime smoke succeeds, the GL4.3 work is **code-complete but not runtime-accepted**.

## After local Core acceptance

Capture fresh CPU/GPU baselines first. Then reactivate `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md` and implement only measured high-value CPU -> GPU transfers.

Do not start algorithmic GPU offload before the local Core acceptance gate.
