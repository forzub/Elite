# GL43 migration checkpoint — 2026-09-14

Current branch: `chatgpt/mae-v01075-semantic-workflow-motion-v5`.

## Accepted visual checkpoints

### B1 — accepted

The user reported no visible change after `LocalMapPrimitiveRenderer` line/cross/circle submission moved from immediate mode to GLSL 4.30 Core + VAO/VBO.

### B2a — accepted

The user reported Detail Map still looked as before after `DetailMapGeometryPass` moved to explicit-color modern primitive submission and retired fixed-function color/current-color/immediate orbit drawing.

Preserved behavior includes hub/player orbits, hidden-side `0.16x` attenuation, volume edges and small-body markers.

## Current candidate — B2b Hub geometry cleanup

`HubMapGeometryPass.cpp` is now compatibility-clean under the machine contract:

- fallback box edges use explicit-color `drawLocalMapLine`;
- fallback X/Y/Z axes use explicit colors;
- fallback velocity line uses explicit color;
- fallback screen circles/crosses use the modern local-map primitive renderer;
- adaptive grid and principal grid axes use explicit colors;
- no tracked `glColor*`, `glBegin/glEnd` or immediate `glVertex*` remains.

`DetailMapGeometryPass.cpp` and `HubMapGeometryPass.cpp` are both in `NO_COMPATIBILITY_FILES`.

## Transitional debt still retained

`LocalMapPrimitiveRenderer` still exposes temporary no-color overloads for callers that have not yet migrated. Those overloads read `GL_CURRENT_COLOR`.

Remaining bridge callers are in:

- `DetailMapPlanetPass`;
- `HubMapBackend`;
- `HubMapPlanetPass`.

B2b closes only after those calls become explicit-color and the compatibility overloads/`compatibilityCurrentColor()` are deleted.

## Remaining major legacy areas

- `src/render/DebugGrid.cpp`;
- `src/game/system_map/DetailMapBackend.cpp`;
- `src/game/system_map/DetailMapPlanetPass.cpp`;
- `src/game/system_map/HubMapBackend.cpp`;
- `src/game/system_map/HubMapPlanetPass.cpp`;
- `src/game/system_map/LocalMapAtmosphereRenderer.cpp`;
- `src/game/system_map/MapObjectOverlayRenderer.cpp`;
- transitional `GL_CURRENT_COLOR` bridge in `LocalMapPrimitiveRenderer.cpp`.

The machine scan output remains authoritative for the complete inventory.

## Local acceptance required now

```bash
python tests/architecture_contracts/check_gl43_modernization_boundary.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify Hub Map adaptive grid, axes, fallback boxes, screen circles/crosses and velocity lines retain their previous colors, positions and orientation.
