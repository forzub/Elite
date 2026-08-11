# Map subsystem architecture

This directory implements four map modes over one authoritative world model:

- **Galaxy** selects systems and interstellar objects.
- **System** selects bodies, infrastructure, ships and navigation cells.
- **Detail** renders either a celestial-body scene, a local object scene or a
  terminal spatial volume.
- **Hub** renders the local environment of stationary infrastructure.

## Ownership boundaries

Each mode follows the same ownership split:

- `*View` owns persistent camera, selection and presentation state.
- `*Interaction` mutates a view from input and returns intents. It does not
  render and does not own OpenGL resources.
- `*PresentationBuilder` synchronizes persistent view state and produces an
  immutable frame presentation before any draw pass begins.
- `*SceneRenderer` owns pass order and consumes only const view/presentation
  input. It does not access `SystemMapRenderer` members directly.
- `*RenderContext` is the low-level rendering contract.
- `SystemMapRenderer` is the shared OpenGL/resource backend and the public
  facade used by `SpaceState`.

A scene renderer must use only the view and context passed to it. References to
facade members such as `m_systemView`, `m_detailView` or `m_hubView` inside an
extracted scene renderer are contract violations.

## Translation-unit boundary

View, interaction and scene-orchestration implementations are standalone `.cpp`
translation units. They must not be included into `SystemMapRenderer.cpp`. The
remaining `SystemMapRenderer*.inl` files are temporary low-level backend splits;
they stay included by the facade until backend/resource ownership is extracted in
a later stage.

## Local-scene object model

A local scene contains three entity classes:

- `CelestialBody`: star, planet, moon or asteroid.
- `Ship`: an object capable of changing its own trajectory.
- `Hub`: stationary infrastructure such as a station, base, mine, beacon,
  relay or buoy.

A navigation cell or a ring/belt/cloud is spatial context, not a fourth entity
class. Procedural objects use stable procedural keys and become persistent only
when durable gameplay state is attached to them.

## System camera contract

System camera geometry belongs to `SystemMapView`. It is the only component
that converts yaw, pitch and visible half-height into a camera basis or applies
3D orbit/zoom transforms. `SystemMapInteraction` only resolves the semantic
pivot and invokes those operations.

Pivot priority is explicit:

- zoom: nearest star/planet/moon anchor within the bounded cursor radius, then
  the current `camera.target`;
- rotation: nearest star/planet/moon anchor within the bounded cursor radius,
  explicitly selected navigation cell, navigation-cell centre under the
  cursor, then the point under the mouse.

A celestial camera anchor is a small screen-space area around the body's
centre. The physical disk is used for object selection only and must never
become an unbounded camera capture area when a planet fills the viewport.

Orbiting rotates the complete camera pose around the pivot. Zooming scales the
camera target and eye distance together around the pivot. Body zoom scales the
clearance above a safe surface radius, so it approaches the surface without
crossing the body centre. Screen-space reprojection corrections are forbidden
because they become singular when a close pivot reaches or crosses the eye
plane.

Camera flight is advanced before manual System-map input, and the navigation
boundary is applied once after the resulting pose. System-change reset, initial
camera fit, presentation-clock advancement, orbit interpolation, stale-selection
cleanup and navigation-hover animation are performed by
`SystemMapPresentationBuilder` before rendering. `SystemMapSceneRenderer`
receives `const SystemMapView&` and must not mutate persistent view state.


## System presentation lifecycle

The System map frame follows one directional lifecycle:

```text
refresh authoritative snapshot
-> synchronize persistent view state
-> build immutable SystemMapPresentation
-> build CPU SystemMapSceneFrame
-> input/picking consumes that frame
-> rebuild only camera-dependent projection data when input moved the camera
-> render the current frame
```

`SystemMapPresentation` owns the visual-time body copy and map scale.
`SystemMapSceneFrame` owns matrices, visual positions, LOD metrics and semantic
pick geometry for the same snapshot. The authoritative `SystemMapSnapshot` is
never rewritten. OpenGL code consumes the prepared frame and must not create a
second renderer-owned pick cache.

`SystemMapFrameInteractionContext` is a concrete adapter over the prepared CPU
frame. `SystemMapRenderer` no longer implements the interaction context and
cannot answer picks from whatever happened to be rendered on the previous
frame.

## System-map pass order

The System map uses explicit pass ordering:

1. background, navigation grid, orbits and cartographic lines;
2. back halves of visible planetary rings;
3. ring-parent body geometry;
4. front halves of visible planetary rings;
5. other physical bodies, including moons;
6. proxy markers, selection overlays and labels.

This order is intentional. Planetary rings use the shared profile-driven ring
renderer, while moons remain readable and are not swallowed by a translucent
fullscreen ring pass.

## Ring invariants

- Ring appearance comes from `SystemMapRingVisualProfile` and per-band data.
  Jupiter dust, Saturn bands and Uranus/Neptune narrow rings are not rendered
  by one hard-coded solid-annulus approximation.
- Ring patterns are not rigidly attached to the planet's day rotation.
- Edge-on projection keeps a thin stable representation instead of abruptly
  removing the ring.
- System-map visibility uses a smooth screen-space LOD fade.

## Marker invariants

Proxy markers are navigation overlays, not celestial geometry. They are used
only for stars and planets that would otherwise be unreadable. Moons never use
synthetic circle-and-cross proxies: their physical body, orbit and label provide
the presentation, while picking keeps an independent minimum hit radius.

## Patch and source hygiene

Text files use UTF-8, LF endings and no trailing whitespace. Generated `.orig`
and `.rej` files are not source files and must not be committed.

## Behavior regression gate

Before changing camera, navigation or map-mode architecture, run:

```bash
bash tests/system_map/run_mingw64.sh
```

The standalone test target does not initialize GLFW or OpenGL. It verifies the
camera invariants, pivot priority, refine/coarsen behavior, anchor versus
explicit selection semantics, deferred mode transitions and deterministic
mouse/scroll replay against the production interaction code.

## Detail and Hub presentation lifecycle

Detail and Hub follow the same pre-render boundary as System:

```text
refresh local snapshot
-> synchronize persistent local-map state
-> build immutable DetailMapPresentation / HubMapPresentation
-> input/picking consumes the prepared frame
-> rebuild projection data only when camera input changed it
-> render from const view + presentation
```

`LocalMapPresentationBuilder` owns spatial-volume zoom/pan constraints,
Detail hub-selection validation, local-map scale calculation and Detail/Hub pick
geometry. Detail hub selection belongs to `DetailMapView`; it is synchronized
back to the System selection only when the mode changes. OpenGL passes must not
reset cameras, clear selection or append pick records to view state.

## Detail and Hub backend ownership

Detail and Hub scene renderers target dedicated `DetailMapBackend` and
`HubMapBackend` objects. `SystemMapRenderer` no longer implements the local-map
render-context interfaces. The Hub backend owns its asynchronous GPU timer-query
state and exposes read-only performance statistics to the facade.

Hub rendering is physically split into `HubMapGeometryPass` and
`HubMapPlanetPass`. The geometry pass owns the Hub GPU geometry renderer; the
planet pass owns the soft-layer overlay, spherical-grid renderer and cached
planet visual geometry. `HubMapBackend` coordinates these passes and owns GPU
timing.

Both legacy local-map implementation files have been removed.

Detail rendering is physically split into `DetailMapPlanetPass` and
`DetailMapGeometryPass`. The planet pass owns the Detail-only shape-mesh cache
and screen-space sculpt shader/VAO; the geometry pass owns spatial-volume,
orbit, marker, axes, velocity and label drawing. `DetailMapBackend` coordinates
both passes.

The simple line, cross and circle primitives that were historically defined in
the Detail implementation are provided by `LocalMapPrimitiveRenderer` and
shared by Detail and Hub.

## Shared map celestial resources

`MapCelestialRenderResources` is the explicit owner of resources shared across
map modes:

- generated celestial asset and environment-profile libraries;
- preview, albedo and normal texture caches;
- procedural cloud cache and environment presentation clock;
- globe, ring and Hub surface renderers;
- astronomical map and distant-galaxy starfields;
- Detail and Hub visual settings.

Detail and Hub backends receive this owner explicitly. They do not include,
reference or friend `SystemMapRenderer`. The Hub camera snapshot is owned by
`HubMapBackend` for the duration of its immutable presentation frame, rather
than being stored in the facade.

`SystemMapRenderer` is now the map subsystem coordinator and the render context
for the still-shared Galaxy/System primitive pipeline. It owns mode state,
Views, interactions, presentation builders, scene frames, transitions and the
backend objects, but no Detail/Hub-specific rendering resources or hidden local
camera state.


## Shared coordinate display format

`CoordinateDisplayService` owns the active coordinate notation for the entire
running game rather than for one map renderer. Galaxy and System addresses,
including the addresses shown while Detail or Hub is active, are formatted
through `formatCurrentNavigationAddressLine()`.

The three reserved display names are:

- `STRAIGHT THERE` — hierarchical path notation;
- `THREE AXES` — separate X/Y/Z hierarchy notation;
- `VERY SECRET CODE` — packed Crockford Base32 notation.

Every rendered address line carries its format name. `Ctrl+F11` is polled at
the application/Windows level, so changing the notation does not depend on
whether GLFW or the WebView currently owns keyboard focus. Reports and future
navigation interfaces must use the same service rather than store their own
format setting.
