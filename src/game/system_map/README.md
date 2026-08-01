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

Hub rendering is now physically split into `HubMapGeometryPass` and
`HubMapPlanetPass`. The geometry pass owns the Hub GPU geometry renderer; the
planet pass owns the soft-layer overlay, spherical-grid renderer and cached
planet visual geometry. `HubMapBackend` coordinates these passes and owns GPU
timing, while `SystemMapRenderer` only supplies shared celestial assets and
common local-map primitives through a temporary friend bridge.

`SystemMapRendererHub.inl` has been removed. The remaining
`SystemMapRendererDetail.inl` still contains Detail-specific low-level helpers;
the next Stage-6 slice extracts those resources and passes without changing
visual output.
