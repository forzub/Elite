# GL43 migration checkpoint — 2026-09-14

Current branch: `chatgpt/mae-v01075-semantic-workflow-motion-v5`.

## Implemented candidate

- Added `tests/architecture_contracts/check_gl43_modernization_boundary.py`.
- The contract scans production C/C++ under `src/` for immediate mode, fixed-function matrix state, current-color state, fixed texture-enable state and legacy client arrays, and prints the current debt inventory.
- `src/game/system_map/LocalMapPrimitiveRenderer.cpp` no longer uses `glBegin/glEnd` or immediate `glVertex*`; line/cross/circle drawing now uses GLSL 4.30 Core + VAO/VBO and explicit pixel-to-NDC conversion from `GL_VIEWPORT`.
- The architecture contract permanently forbids immediate-mode submission from returning to `LocalMapPrimitiveRenderer.cpp`.

## Transitional debt intentionally retained

Existing callers still communicate primitive color through fixed-function current-color state. `LocalMapPrimitiveRenderer` therefore temporarily reads `GL_CURRENT_COLOR` and forwards it to shader uniform `uColor`.

Next subwave GL43-B2 must make color explicit in the primitive API, migrate every caller and remove `GL_CURRENT_COLOR` from this seam. Do not mix B2 with Detail/Hub geometry redesign.

## Expanded confirmed legacy areas

Manual audit has confirmed compatibility debt in at least:

- `src/render/DebugGrid.cpp`;
- `src/game/system_map/DetailMapBackend.cpp`;
- `src/game/system_map/DetailMapGeometryPass.cpp`;
- `src/game/system_map/DetailMapPlanetPass.cpp`;
- `src/game/system_map/HubMapBackend.cpp`;
- `src/game/system_map/HubMapGeometryPass.cpp`;
- `src/game/system_map/HubMapPlanetPass.cpp`;
- `src/game/system_map/LocalMapAtmosphereRenderer.cpp`;
- `src/game/system_map/MapObjectOverlayRenderer.cpp`;
- transitional `GL_CURRENT_COLOR` in `LocalMapPrimitiveRenderer.cpp`.

The machine scan output is authoritative for the complete inventory; this handwritten list is not exhaustive.

## Local acceptance required

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify OpenGL >= 4.3, `compute=1`, `ssbo=1`, then smoke flight, cockpit/rear view, Galaxy/System/Detail/Hub and close-navigation HUD. For this checkpoint specifically verify Detail/Hub lines, circles, crosses and fallback markers preserve position, color and orientation.

This checkpoint is not accepted until those local checks pass.
