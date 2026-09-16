# GL43 migration checkpoint — 2026-09-15

Branch: `chatgpt/mae-v01075-semantic-workflow-motion-v5`.

## Status

**OpenGL 4.3 Core API migration is code-complete. Local build/runtime acceptance is still required.**

Key migration commit:

`37999c5588c6e85ef88a24e0efbf2dfff89b9314` — `render: complete OpenGL 4.3 Core API migration`

## What changed

- GLFW now requests OpenGL 4.3 Core Profile.
- Bundled GLAD 2.0.8 is generated for `gl:core=4.3`.
- Production `src/` passes a zero-tolerance compatibility-only API scan.
- Remaining old presentation semantics are handled by `src/render/legacy/CoreGlLegacyBridge.h` using software state + GLSL 4.30 Core + VAO/VBO + `glDrawArrays`.
- The bridge converts legacy quads to triangles and preserves legacy matrix/color/texcoord/texture-enable semantics without using driver fixed-function state.

## Audit correction

The original handwritten map-focused inventory was incomplete. The branch-wide CI audit found compatibility debt in 23 additional production files spanning:

- flight/client rendering;
- Detail/Hub map backends and planet passes;
- map overlays and route overlays;
- scene rendering;
- HUD/world labels;
- debug grid;
- radar/PPI;
- mini-camera;
- celestial/cloud presentation.

All discovered production offenders were migrated. The permanent architecture test is now the authority.

## Validation completed

PASS:

- Core GLAD generation;
- final architecture boundary;
- zero forbidden compatibility API tokens in `src/`;
- migration whitespace check;
- Windows/MSYS2 MinGW64 boundary test;
- Windows/MSYS2 MinGW64 syntax compile of `CoreGlLegacyBridge.h` against Core GLAD.

A hosted full `EliteGame` build could not configure because `third_party/webview` is required by `CMakeLists.txt` but is not present in the GitHub repository and is not a submodule. This happens before C++ compilation and is not evidence of a GL4.3 failure.

## Local acceptance command

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Smoke all major render surfaces: flight, cockpit/rear, Galaxy/System/Detail/Hub, HUD/labels, radar/PPI, mini-camera, debug grid and visible celestial/cloud/atmosphere paths.

If that passes, GL4.3 Core becomes accepted and performance/offload work can resume.
