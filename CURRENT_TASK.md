# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** OpenGL 4.3 renderer modernization  
**Stage:** GL43-F local build/runtime acceptance

## Current state

The repository-side API migration is complete:

```text
OpenGL 4.3 Compatibility scaffold
    -> branch-wide compatibility inventory
    -> modern shared primitives
    -> software Core legacy bridge for remaining presentation semantics
    -> bundled GLAD gl:core=4.3
    -> GLFW_OPENGL_CORE_PROFILE
    -> zero forbidden compatibility API tokens in src/
```

Automated static and MinGW64 syntax gates pass. The remaining gate requires the developer's complete local tree because GitHub does not contain `third_party/webview`, which `CMakeLists.txt` requires before `EliteGame` can configure.

## Run now

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Expected architecture-test result:

```text
GL43 CORE MODERNIZATION BOUNDARY: PASS
 - GLFW requests OpenGL 4.3 Core Profile
 - bundled GLAD is generated for gl:core=4.3
 - production src/ has zero forbidden compatibility-only API tokens
 - legacy presentation syntax is translated by the Core GLSL/VAO/VBO bridge
```

## Runtime acceptance checklist

Verify no visible or behavioral regression in:

- ordinary flight;
- cockpit and rear view;
- Galaxy Map;
- System Map;
- Detail Map, including planet grid/orbits/markers;
- Hub Map, including grid, station/module geometry and parent-planet presentation;
- close-navigation HUD and world labels;
- radar and PPI;
- mini-camera;
- debug grid when available;
- visible cloud/atmosphere/Hub backdrop paths.

Startup must create OpenGL 4.3+ Core and still report valid capabilities, including compute/SSBO support.

## Acceptance rule

Any build error or visual regression blocks GL43 acceptance and is fixed before performance work begins.

If local build and smoke pass, mark OpenGL 4.3 Core **accepted**, capture fresh performance baselines, then activate the existing CPU -> GPU plan.

## Deferred until acceptance

Do not yet start:

- System Map textured-sphere CPU tessellation removal;
- SceneRenderer compute visibility/LOD/compaction;
- instance-stream optimization;
- starfield compute migration;
- runtime-model consumer migration;
- other algorithmic CPU -> GPU offload.
