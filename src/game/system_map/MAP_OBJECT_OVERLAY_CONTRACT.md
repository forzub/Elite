# Map tactical object overlay contract

This file is the canonical baseline for ship/hub tactical presentation on
System/Space, Detail and Hub maps. It describes what the current code means,
not every helper that happens to remain in a renderer.

Status vocabulary:

- **PROTECTED** — intentional current behavior; a change requires code + tests +
  this contract to change together.
- **TEMPORARY** — current implementation detail that may be replaced without
  preserving its exact representation, while preserving the higher-level
  contract around it.
- **LEGACY** — dormant or transitional code that is not a product requirement.
- **NOT IMPLEMENTED** — architecture may contain a seam, but the feature must
  not be described as working.

Navigation semantics remain owned by `MAP_NAVIGATION_CONTRACT.md`. This file
covers only object presentation and interaction layered on top of that already
protected navigation context.

## PROTECTED: object glyph semantics

Ships and map-scale hubs/infrastructure use a compact tactical glyph instead of
the old debug axes/cross/cube marker presentation.

For self-propelled craft:

- the triangular glyph points along the object's **facing/orientation**;
- the velocity arrow is a separate visual and points along the object's actual
  motion vector;
- facing and motion are deliberately independent, because Newtonian flight may
  move sideways or backwards relative to the nose;
- a short numeric track label is displayed next to the object;
- the glyph owns an enlarged hit radius so interaction does not require clicking
  a one-pixel line.

The central Hub on Hub Map is an explicit exception:

- its existing structural Hub geometry remains visible;
- the tactical layer does **not** replace that geometry with a triangle;
- it adds a broad translucent arrow for the Hub's global motion vector;
- the Hub card still reports Hub-local speed, which is zero for the reference
  Hub itself.

On System/planet Detail maps, hubs participate in the normal compact tactical
object presentation rather than the Hub-map structural special case.

## PROTECTED: velocity policy

Two velocity meanings are kept distinct.

### Global velocity — blue

Global velocity is used on:

- System maps;
- celestial-body Details;
- ordinary non-spatial Details;
- map-scale hub/infrastructure presentation outside Hub Map.

It is expressed in the world/stellar reference frame and is used by the card
for stellar azimuth/elevation.

### Local/relative velocity — green

Local velocity is used on:

- Hub Map ships;
- terminal empty-space `SpatialVolume` Details.

For terminal empty-space Details, replicated ship `localVelocityMps` belongs to
the ship travel frame. It must be transformed with
`travelFrame.localToWorldVector(...)` before screen projection. Treating those
components as world XYZ is forbidden because it produces a plausible-looking
but physically wrong arrow.

### Hub reference-object exception

The Hub reference object's information card is local (zero relative speed), but
its broad motion arrow visualizes the Hub's **global** motion. The presentation
model therefore has separate `velocityMode` (card semantics) and
`arrowVelocityMode` (arrow-color semantics). Collapsing these into one flag
would make one of the two statements false.

## PROTECTED: faction-color seam

`MapObjectOverlayItem` carries `factionColor`. Tactical glyph color is therefore
not hard-wired to object kind and can be driven by faction/affiliation once an
authoritative player-visible faction source exists.

The current neutral/player/hub colors are presentation defaults only; see
TEMPORARY below.

## PROTECTED: zoom-aware tactical scale

Tactical glyphs remain readable at long range in screen space. Once the
projected physical size of an object becomes comparable with the glyph, the
glyph and velocity arrow grow with the object.

`mapObjectGlyphScale()` is deliberately clamped:

- distant objects stay at the minimum readable scale;
- close objects grow smoothly;
- the symbol cannot become an unbounded screen-filling arrow.

The same physical/zoom scale is used for the glyph and hit radius. Velocity-arrow
length additionally carries speed magnitude through a bounded logarithmic scale:

- zero velocity draws no arrow;
- local arrows use a local-speed reference range with a very small minimum so
  `1 m/s` and `100+ m/s` remain visibly different;
- global arrows use a wider stellar-speed reference range;
- increasing speed always increases arrow length until the protected maximum;
- the maximum remains the pre-log tactical arrow length, so this change cannot
  create longer screen-space spears than the previous overlay.

`mapObjectVelocityArrowLengthScale()` owns this policy. Do not substitute raw
linear metres-per-second scaling; orbital and manoeuvring regimes differ by too
many orders of magnitude for that to remain readable.

## PROTECTED: multiple information cards and one active tactical object

The map supports any practical number of simultaneously open object cards.
Opening a card and selecting an object are deliberately different states: many
cards may remain open, but at most one tactical object is active at a time.

Each card owns independent state:

- object ID;
- screen-space top-left position;
- drag offset;
- z-order;
- drag state.

Interaction rules:

- clicking a visible object glyph activates that object and toggles its card;
- clicking the same glyph again closes only that card but leaves the object as
  the current tactical selection;
- clicking anywhere on an already-open card except `X` reactivates that object
  without toggling the card;
- the active card/object receives a visible selection treatment;
- clicking the card `X` closes only that card; closing information does not
  silently choose another object;
- dragging the header also activates that card's object and then moves only
  that card;
- clicking/dragging a card or glyph captures the left-button gesture until
  release, so the underlying map camera cannot accidentally begin rotating or
  panning during the same mouse gesture;
- multiple other cards remain open when one card is toggled or closed.

Card positions are screen-space positions. They do not orbit in 3D.

### Tactical selection and navigation focus

`MapObjectOverlayState::activeObjectId()` is the presentation-level tactical
selection seam. It is intentionally **not** a trajectory solver and it does not
manufacture a route. It only provides an unambiguous selected moving/static
object for future navigation work.

Selection synchronization rules are protected:

- activating a Hub glyph/card restores the canonical Hub selection on System or
  Detail maps, therefore the `HUB` navigation action becomes available again;
- activating a ship/infrastructure object clears stale body/cube/Hub semantic
  focus without moving the camera/navigation anchor;
- selecting a body or explicit navigation cube through the ordinary map clears
  the tactical-object focus;
- selecting a Hub through the ordinary map synchronizes the tactical active ID
  back to that Hub;
- open cards are not closed merely because another target becomes active.

This gives future route planning one explicit target endpoint while preserving
all existing map selection contracts. Route generation, drive choice, path
validation and trajectory rendering remain outside this baseline.

## PROTECTED: crowded-object click arbitration

A pointer hit is first collected against the existing per-object hit radii; it
is **not** resolved by vector/insertion order and not simply by the nearest
screen centre.

When several tactical objects occupy the same click cluster:

1. the physically largest candidate wins;
2. screen distance is used only as a tie-breaker for equal physical size;
3. the object ID provides a deterministic final tie-breaker.

This is important on distant System/Details views where many ships and a Hub
collapse into almost the same pixel cluster. A Hub must therefore win over the
surrounding ships when all of them are under the same click.

On System Map, directly hit planets/moons participate in the same semantic
arbitration before the tactical overlay consumes the mouse gesture. A directly
hit body that is physically larger than the overlapping Hub/ship glyphs owns
the click. Body halo-only picking remains available when no tactical glyph has
already claimed the direct cluster; the large body's extended halo must not
steal unrelated nearby glyph clicks.


### Distinct Hub glyph and real-size click rank

Outside Hub Map, a Hub is rendered as a compact **cube glyph**, not as the
ship arrowhead. The glyph is still screen-readable at distance and uses the
Hub/faction color. Hub Map is the exception: the central Hub keeps its
structural scene geometry and does not receive a duplicate cube glyph.

The current replication sample does not yet carry aggregate authored Hub
bounds. Until it does, Detail and System presentation use the same temporary
`4000 x 1500 x 4000 m` Hub envelope. That envelope is presentation metadata,
not authoritative collision geometry. It is nevertheless the correct semantic
size for crowded map picking, and prevents the 200 m Hub Motion Lab analytic
cube (or an ordinary ship) from outranking the several-kilometre Hub merely
because the Detail bridge previously left Hub size at its 1 m fallback.

### Localized card label wrapping

Information-card field labels are measured with the active native font. If a
localized multi-word label does not fit the label column, it wraps on word
boundaries and the value aligns with the final wrapped line. The card gains
vertical room for that second line; translations must not overlap field values
or be shortened merely to fit the English layout.

## PROTECTED: leader-line behavior

Every visible open card is connected to the current projected object anchor by
a screen-space leader line.

Therefore:

- moving the object updates the line endpoint;
- rotating/panning/zooming the map camera updates the object endpoint;
- a dragged card stays at the user's chosen screen location;
- the card itself always faces the camera because it is a 2D overlay rather
  than world geometry.

The overlay preserves and restores the OpenGL program/depth/blend/matrix/line
state it temporarily changes. It must not leave hidden render state behind for
later map or HUD passes.

## PROTECTED: card information model

Current standard fields are:

- ship/object type;
- object name in the card header;
- context-selected local or global speed in metres per second;
- stellar-motion azimuth;
- stellar-motion elevation;
- owner/faction text when the current presentation source provides one.

`MapObjectInfoField` / `extraFields` form an extension seam for future rows.
Adding a new information source must not require redesigning card ownership,
dragging or object anchoring.

Azimuth/elevation are derived from the same motion regime as the displayed
speed, expressed in stellar/world axes when necessary. They therefore describe
local relative motion on Hub/terminal-local views and global motion on larger
maps. They are not derived from ship nose orientation or the currently rotated
camera. If the displayed speed is zero, the card shows an em dash instead of
inventing a bearing.

## PROTECTED: short track numbers

`MapObjectOverlayState` assigns a short numeric label the first time it sees an
object ID and reuses it for that ID for the lifetime of the overlay state.

The purpose is rapid visual identification of moving targets over time. The
number is presentation identity, not authoritative entity identity.

## PROTECTED: Hub close-inspection camera

Hub Map permits substantially closer tactical inspection than the old
`maxZoom = 8` constraint:

- current protected maximum zoom is at least `64`;
- Hub pan bounds gain zoom-dependent allowance, so zooming in does not leave
  nearby ships unreachable behind a fixed central pan clamp;
- the Hub remains the map's coordinate origin; this change does not convert Hub
  Map into a ship-owned camera mode.

## PROTECTED: trajectory-ready data boundary

`MapObjectOverlayFrame` contains a separate collection of
`MapObjectTrajectory` values. A trajectory has:

- stable object ID;
- semantic kind: history, prediction or planned;
- time-stamped spatial samples.

Current velocity and trajectory are different facts. The renderer/builder must
**not manufacture a flight path from one position + one velocity vector** merely
so that a line can be drawn.

This distinction is protected now so a future authoritative/history/predictive
producer can be attached without changing glyph/card ownership.

## PROTECTED: existing navigation must survive overlay interaction

The tactical overlay is presentation layered on top of the map navigation
baseline. It must not replace semantic map selection.

In particular:

- clicking a Hub glyph on System still updates the canonical System Hub
  selection through `SystemMapInteraction::focusHubSelection()`;
- clicking a Hub glyph in Details still updates `DetailMapView` Hub selection;
- therefore the existing `DETAIL`/`HUB` panel actions remain valid after a card
  is opened;
- Galaxy/System/Space/Details/Hub loaded-context rules from
  `MAP_NAVIGATION_CONTRACT.md` remain unchanged.

## Localization contract

The map localization asset reserves these keys:

- `map.object_info.type`
- `map.object_info.name`
- `map.object_info.local_speed`
- `map.object_info.global_speed`
- `map.object_info.azimuth`
- `map.object_info.elevation`
- `map.object_info.owner`

English, Russian, Simplified Chinese, Spanish and Japanese entries exist in the
current asset set.

## TEMPORARY

The following are real implementation details, but their exact representation
is not a product contract:

- faction colors currently come from local presentation defaults (player, Hub,
  neutral object); there is no authoritative/player-known faction pipeline yet;
- short track numbers are session/overlay-state local and are not durable
  across process restart or reconnect;
- some System infrastructure physical sizes use conservative presentation
  defaults where no static descriptor is available;
- the native OpenGL overlay currently mirrors the small localization vocabulary
  internally while the JSON keys reserve the public localization contract;
- panel dimensions and typography are first-pass compact values and may be
  tuned without changing card semantics;
- card auto-layout is intentionally minimal: a newly opened card is offset from
  its object, then the user may place it manually.

## LEGACY

The following helpers/code may remain during cleanup but are not desired visual
behavior:

- `SystemMapRenderer::addMapObjectCube()` if no production object pass calls it;
- old Detail axes/velocity primitive helpers if no ship/hub presentation path
  calls them;
- historical cross/circle/cube markers for ships/hubs;
- permanent ship name labels that duplicate the short track number + card.

Do not restore a legacy helper merely because it is still present in a source
file.

## NOT IMPLEMENTED

These features are intentionally **not** claimed by the current baseline:

- rendered historical trails;
- rendered predicted trajectories;
- rendered planned/navigation trajectories;
- authoritative trajectory-history storage/source;
- persistent track numbers across sessions;
- authoritative or sensor-derived faction/affiliation coloring;
- fog-of-war / identification-confidence-driven card fields;
- automatic non-overlapping placement/routing of many cards and leader lines;
- minimizing/pinning/grouping cards;
- automatic camera lock/follow on a selected ship.

The trajectory data seam is present; the trajectory product feature is not.

## Regression gates

Run from repository root:

```bash
bash tests/system_map/run_mingw64.sh
bash tests/architecture_contracts/run_mingw64.sh
bash tests/client_acceptance/run_mingw64.sh
```

`tests/system_map/check_object_overlay.py` additionally locks production wiring
without needing an OpenGL runtime. Behavioral C++ tests lock glyph scaling,
stellar angles, multi-card state, pointer capture/dragging, the no-fake-path
trajectory invariant and Hub close-inspection zoom.
