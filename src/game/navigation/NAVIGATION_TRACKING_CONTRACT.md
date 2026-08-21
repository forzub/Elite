# Client navigation tracking contract

This file defines the current player-private navigation-memory layer that joins
map selections to cockpit navigation markers. It does **not** define route
physics, trajectory generation, drive choice or autopilot execution.

Status labels:

- **PROTECTED** — intentional current behaviour; change code, tests and this
  contract together.
- **TEMPORARY** — deliberately usable baseline whose representation/policy may
  be replaced without preserving it as product design.
- **NOT IMPLEMENTED** — architecture seam exists, but behaviour must not be
  inferred from it.

## PROTECTED: client-only ownership

`NavigationTrackingState` is player-private client state.

It owns three navigation-memory classes:

1. tracked tactical objects — ships, Hubs and other map tactical objects;
2. tracked celestial bodies — stars, planets and moons selected through System
   Map body cards;
3. route waypoints — spatial destinations which do not have to correspond to a
   physical object.

This state is presentation/navigation intent. It is not authoritative world
state and must not be replicated to the server merely because the player opens,
closes or moves a card, tracks a body, or chooses a route point.

A later autopilot may submit an explicit movement/navigation command derived
from this state. That is a different boundary: UI/navigation memory itself does
not become server state.

## PROTECTED: tactical cards are cockpit tracking intent

Tactical object cards continue to be owned by `MapObjectOverlayState`.
`SystemMapRenderer::synchronizeNavigationTracking()` reconciles the open-card
set into `NavigationTrackingState`.

Consequences:

- opening a ship/Hub/infrastructure card tracks that object for cockpit HUD;
- closing its card removes that tactical tracking intent;
- many tactical cards may remain open and therefore many objects may be tracked;
- the single active map object remains separate from the many tracked objects;
- changing active selection does not silently close other cards or remove their
  cockpit markers.

The tracking layer stores stable presentation identity/metadata, not a copied
world transform. Current position and velocity for normal runtime entities are
resolved from `ClientWorldState` when cockpit markers are built.

## PROTECTED: cockpit speed semantics

Cockpit navigation markers display speed according to the player's current
motion regime, not merely according to target distance.

Current policy:

- manoeuvring/non-cruise modes use the target's **own speed in the player's
  current travel frame**;
- `MotionMode::Cruise` and `MotionMode::JumpTransit` use **global speed**;
- the local tactical number is directly comparable with the player's own HUD
  `|localVelocityMps|`; it is **not closing speed** and does not subtract the
  player's local velocity from the target's local velocity;
- when no valid common travel frame exists, the marker explicitly falls back to
  global speed instead of fabricating a relative value from unrelated vectors.

This is intentionally different from choosing a display regime by range. A
nearby target can still be globally tracked in a cruise mode, and a manoeuvring
intercept uses the local frame even if the absolute orbital velocities are very
large.

## PROTECTED: cockpit navigation-marker geometry

Tracked navigation targets are rendered through the existing HUD boundary
machinery rather than world-size labels.

Marker invariants:

- marker geometry is screen-space and does not shrink/grow with distance;
- targets inside the HUD safe boundary are shown at their projected direction;
- targets outside/behind the visible boundary are clamped through
  `HudEdgeMapper` and keep an indication of the direction to the target;
- target distance is carried as text only and does not alter marker size;
- very large distances use direction-only projection, avoiding far-plane and
  float-precision dependence for light-year-scale targets;
- long type/name strings are width-clamped/truncated before drawing.

Current visual vocabulary:

- tactical object — fixed-size translucent outline triangle, always pointing
  upward in screen space and never scaling with distance; its map track number
  is textual metadata rather than geometry inside the symbol;
- celestial body — diamond marker;
- route waypoint — open/corner marker, visually distinct from physical objects;
  intermediate route markers carry their current route sequence number.

For tactical markers the current text layout is:

- object type to the right of the marker;
- optional object name below the type;
- relative/global speed to the left;
- target track number directly below speed, sharing the same right-aligned data
  column;
- distance below the marker.

Exact pixel dimensions and artwork are presentation tuning, not navigation
semantics.

## PROTECTED: celestial-body cards and tracking

System Map provides information cards for stars, planets and moons.

Current body-card rules:

- clicking a body on System Map toggles that body's card;
- multiple body cards may remain open simultaneously;
- body cards are rendered only on System Map; they do not leak into
  Galaxy/Details/Hub presentation;
- open body cards are represented in the client navigation tracking collection
  and therefore generate cockpit markers;
- body cards do not show tactical speed, azimuth or elevation rows;
- current body metadata includes type/name and radius.

The ordinary single semantic System-map body selection still exists for
navigation/Details actions. Multiple open body cards are a separate player
tracking memory and do not turn the core map selection into a multi-select.

## PROTECTED: spatial route waypoints

A route waypoint is a navigation entity, not a physical universe object.

`NavigationTrackingState` owns waypoints as a vector and each waypoint has:

- stable client-local waypoint ID;
- semantic role (`Finish`, `Intermediate`, or unassigned candidate);
- sequence slot for intermediate route ordering;
- precise `WorldPosition`;
- navigation address/display name.

The collection is deliberately plural. The current UI supports exactly one
Finish plus multiple Intermediate points. Intermediate points are numbered
`1..N` in collection/route order and are renumbered after an intermediate point
is removed or changes role. This avoids encoding route identity in a stale
display number.

### Selected-cube information affordance

Selecting an empty Galaxy/System navigation cube keeps the existing central cube
marker dedicated to selection/drill semantics. Selection alone does **not** open
a route card. Instead, the selected cube gets a small translucent blue triangle
next to its centre marker:

- the central marker continues to mean "this cube is selected";
- the blue triangle is a screen-space information affordance;
- clicking the triangle opens/closes the cube's route-information card;
- clicking the triangle must win inside its compact hit area even if projected
  world geometry lies behind it;
- changing the selected cube moves the blue affordance to the new selection,
  but already opened route cards remain open and keep their own spatial target.

This separation prevents one click from simultaneously changing cubic
navigation selection and opening route UI.

### Current Finish / Intermediate workflow

The route still has exactly one **finish waypoint** at a time, plus zero or more
numbered Intermediate points.

On Galaxy or System Map:

1. the player explicitly selects a navigation cube rather than a physical
   object;
2. the blue information affordance opens a persistent card for that spatial
   target; losing cube focus does not close that card;
3. `SET AS FINISH` / `CANCEL FINISH` is a toggle for the one allowed Finish;
4. while another Finish exists, other route cards do not offer a second Finish
   action;
5. `SET INTERMEDIATE` / `CANCEL INTERMEDIATE` toggles an intermediate route
   point;
6. intermediate points are numbered `1..N` and renumber immediately after
   add/remove/role changes;
7. route cards may be collapsed without removing their target;
8. closing a route card removes that spatial target from client tracking and
   therefore cancels its Finish/Intermediate role.

Finish itself is not route-sequence numbered; only intermediate points use the
contiguous sequence.

Galaxy cube centres are converted from galactic light-year coordinates to
`WorldPosition`. System cube centres combine the loaded system root with the
selected AU-space cell centre. The waypoint therefore identifies actual space,
not the player's current position and not whichever object happens to be near
the cursor.

Selecting a known Galaxy star/system is not treated as an empty-space waypoint
candidate.

## PROTECTED: map velocity arrows are linear

Tactical map velocity-arrow **length** uses linear magnitude scaling with a
clamped maximum. A velocity twice as large therefore produces approximately
twice the arrow length until saturation.

Local and global arrows retain separate reference maxima because manoeuvring and
stellar/J-flight magnitudes occupy different physical regimes. Arrow colour
semantics remain unchanged:

- green — local/relative velocity;
- blue — global velocity.

Zero velocity draws no arrow.

## PROTECTED: localization seam

Fixed navigation-HUD and map-card vocabulary comes from the shared localization
asset/service. Current localization keys include:

- `map.object_info.radius`
- `map.object_info.address`
- `map.object_info.space_target`
- `map.object_info.set_finish`
- `map.navigation_hud.object`
- `map.navigation_hud.celestial`
- `map.navigation_hud.finish`
- `map.navigation_hud.waypoint`
- `map.navigation_hud.relative_speed_short`
- `map.navigation_hud.global_speed_short`

Dynamic object/body names remain sourced from the existing localized/catalog
presentation data where available.

## TEMPORARY

The following are current implementation choices, not permanent product rules:

- linear local velocity-arrow saturation currently uses `250 m/s` and global
  saturation `100000 m/s`; these are tuning constants, not propulsion limits;
- tactical/celestial/waypoint marker artwork and exact HUD pixel offsets;
- the textual `REL/GLOB`-style abbreviated speed prefixes;
- a tracked celestial body's precise cockpit position is currently refreshed
  while its System-map presentation is composed and then retained client-side;
  continuous off-map ephemeris evaluation for arbitrary tracked systems is not
  implemented yet;
- diagnostic objects without a resolvable stable runtime entity may keep their
  map card but cannot produce a live cockpit marker until they have a normal
  client-world identity source.

## NOT IMPLEMENTED

The following must not be inferred merely because waypoint/tracking structures
exist:

- route ordering/reordering controls;
- trajectory calculation;
- intercept calculation for moving ships/Hubs;
- choosing manoeuvring vs J/jump propulsion for a route;
- route feasibility/fuel/time estimates;
- automatic flight/autopilot execution;
- server-authoritative route execution;
- trajectory history/prediction rendering from the new tracking state.

Those features should consume this client navigation target model after their
physics and authority contracts are decided; they should not be smuggled into
HUD rendering.
