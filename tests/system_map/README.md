# System map behavior regression tests

This executable locks the behavior that must survive the next architecture
steps. It deliberately runs without GLFW, OpenGL, renderer resources or a
server snapshot.

Covered contracts:

- orbit preserves the complete camera pose around the chosen pivot;
- zoom scales target and eye consistently;
- body zoom never crosses the configured safe surface distance;
- rotation pivot priority is body, explicit cell, cursor cell, mouse plane;
- wheel zoom uses only the cursor body or the current camera target;
- refine/coarsen preserves the navigation point;
- an implicit root selection is not an explicit user selection;
- explicit cells resolve to a deterministic central terminal descendant;
- body selection clears explicit cell selection and reanchors navigation;
- native panel buttons follow the Galaxy/System-or-Space/Details/Hub action matrix;
- typed panel actions route to selected-context versus loaded-parent commands correctly;
- mode changes are applied only after outgoing-frame capture;
- a fixed mouse/scroll trace produces the same state after every replay;
- semantic System picking reads a supplied CPU presentation frame;
- immutable camera snapshots own System and local-map projection math.

## Run from MSYS2 MinGW64

From the repository root:

```bash
bash tests/system_map/run_mingw64.sh
```

The script configures an isolated Ninja build under
`build/tests/system_map`, builds one small executable and runs it with
CTest.

The project already needs GLM and nlohmann/json. The test target uses the same
headers and does not add another dependency or test framework.


`check_navigation_context.py` runs before the C++ target and locks the production
`SpaceState`/client composition boundary that unit tests cannot instantiate
without the full game: selected Details uses `m_loadedSystemMapId`, empty-sector
Details remains a negative-id `SpatialVolume`, System -> Hub prepares its parent
Details target, and Hub/Details parent navigation restores loaded targets rather
than the player's unrelated current system.

The suite also verifies that `SystemMapPresentationBuilder` owns system-change
reset, one-time camera fitting, presentation-time advancement and stale
selection cleanup before the immutable scene renderer is called.

Before compilation, `check_architecture.py` rejects render-time System view
mutation and accidental removal of the immutable presentation boundary.

The local-map checks verify that Detail spatial-volume constraints, Detail hub
selection validation and Hub pick geometry are prepared before rendering.
Detail and Hub scene renderers consume const views and immutable local-map
presentations; architecture checks reject render-time camera, selection or pick
cache mutation.

Stage-3 architecture checks require System pick geometry to be built before
input, forbid `SystemMapRenderer` from implementing the interaction context and
forbid scene rendering from rebuilding body/hub screen-point caches.

Stage-4 checks require Galaxy/System/Detail/Hub render paths to consume camera
snapshots owned by their Views. Renderer-side orbit/projection helpers and
direct renderer mutation of camera state are rejected.

Stage-5 checks require Galaxy and System navigation to share
`CubicNavigationGrid`, `CubicGridIndex` and `CubicNavigationCell`. Galaxy may
supply only its root-domain policy and light-year convenience API. Anchor,
hover and explicit selection remain independent states owned by the common
core.


Stage-6D checks require shared celestial assets, texture caches, environment
timing, clouds, globe/ring renderers and map starfields to belong to
`MapCelestialRenderResources`. Detail and Hub backend/pass components may not
include, reference or friend the `SystemMapRenderer` facade.
