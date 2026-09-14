# GL43 migration checkpoint — 2026-09-14

Current branch: `chatgpt/mae-v01075-semantic-workflow-motion-v5`.

## Accepted visual checkpoint — B1

The user reports no visible change after the first `LocalMapPrimitiveRenderer` migration. The immediate-mode line/cross/circle path has therefore preserved behavior in local runtime smoke.

Implemented B1 foundation:

- `tests/architecture_contracts/check_gl43_modernization_boundary.py` scans production C/C++ under `src/` for compatibility-only OpenGL and prints the current debt inventory.
- `LocalMapPrimitiveRenderer.cpp` submits line/cross/circle through GLSL 4.30 Core + VAO/VBO + `glDrawArrays`.
- immediate `glBegin/glEnd` and `glVertex*` are forbidden from returning to that file.

## Current candidate — B2a

The shared primitive renderer now exposes explicit-color overloads.

`DetailMapGeometryPass` is the first fully migrated consumer:

- fixed-function `glColor*` removed;
- `GL_CURRENT_COLOR` removed;
- immediate orbit `glBegin/glEnd` removed;
- immediate `glVertex*` removed;
- existing orbit segmentation and hidden-side `0.16x` alpha are preserved;
- the architecture contract rejects any compatibility-only API in `DetailMapGeometryPass.cpp`.

## Transitional debt intentionally retained

The no-color `LocalMapPrimitiveRenderer` overloads remain temporarily for Hub/planet callers that still set fixed-function current color. Those overloads still read `GL_CURRENT_COLOR`.

B2b removes this bridge after the remaining callers move to explicit color. Only then is `LocalMapPrimitiveRenderer.cpp` marked fully compatibility-clean.

## Remaining major legacy areas

- `src/render/DebugGrid.cpp`;
- `src/game/system_map/DetailMapBackend.cpp`;
- `src/game/system_map/DetailMapPlanetPass.cpp`;
- `src/game/system_map/HubMapBackend.cpp`;
- `src/game/system_map/HubMapGeometryPass.cpp`;
- `src/game/system_map/HubMapPlanetPass.cpp`;
- `src/game/system_map/LocalMapAtmosphereRenderer.cpp`;
- `src/game/system_map/MapObjectOverlayRenderer.cpp`;
- transitional `GL_CURRENT_COLOR` bridge in `LocalMapPrimitiveRenderer.cpp`.

The machine scan output remains authoritative for the complete inventory.

## Local acceptance required for B2a

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify Detail Map volume edges, hub/player orbits, far-side attenuation, small-body circles/crosses and colors remain unchanged.
