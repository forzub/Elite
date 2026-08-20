# Map navigation and interaction contract

Status: **production baseline / regression-protected**
Baseline date: **2026-08-20**

This document describes the map mechanics that exist **now**. It is deliberately
not a wishlist. Its purpose is to distinguish intentional gameplay/navigation
contracts from temporary implementation debt and from features that are not yet
implemented.

## Status vocabulary

- **PROTECTED** — intentional behavior. Tests should fail if it changes by
  accident. Change only as an explicit design decision.
- **TEMPORARY** — implementation or compatibility mechanism that is currently
  required, but is not the desired final shape.
- **LEGACY** — old shortcut/path still present for convenience or transition.
  It must not be treated as the canonical UI contract.
- **NOT IMPLEMENTED** — planned or discussed behavior that the current game must
  not claim as working.

## 1. Map hierarchy

The semantic hierarchy is:

```text
Galaxy
  -> System / Space
       -> Details
            -> Hub
```

`Hub` may be entered directly from `System / Space` when a hub is selected. This
is a user shortcut only: the client must still prepare the exact parent
`Details` target before entering `Hub`, so navigation back to `Details` is
deterministic.

### Galaxy — PROTECTED

Galaxy is the highest navigation layer. It contains catalog systems,
interstellar objects and the Galaxy cubic navigation grid.

A Galaxy selection can resolve to either:

- a known catalog system; or
- an empty interstellar sector.

The next semantic button is therefore named **SYSTEM** for a known system and
**SPACE** for an empty sector. `SPACE` is not a separate `MapMode`; it is the
player-facing name of the generic cubic System layer when no star system exists.

### System / Space — PROTECTED

`MapMode::System` is the generic cubic-space navigation layer.

For a known star system it contains celestial bodies, hubs/infrastructure,
ships and navigation cells. For an empty Galaxy sector the same mode displays a
pure spatial volume and the UI labels it **SPACE**.

The loaded System/Space map is a navigation context. It is not required to be
the system where the player's ship physically exists.

### Details — PROTECTED

Details is a semantic refinement of the currently loaded System/Space context.
It can represent:

1. `CelestialBody` — a star, planet, moon or other supported body;
2. `LocalObject` — currently used for a hub without a celestial parent;
3. `SpatialVolume` — the terminal cubic address selected by the user.

A Details request must be built from the **loaded map context and current map
selection**, never from `playerNavigation().currentSystemId` as a fallback.

### Hub — PROTECTED

Hub is the local environment of a selected stationary infrastructure object.
It is a child of a concrete Details target.

A direct `System -> Hub` shortcut is allowed only when a hub is selected. The
client first composes the corresponding parent Details target, then composes the
Hub target. `Hub -> Details` therefore returns to the same selected object and
not to a newly resolved player-relative location.

## 2. Loaded navigation context versus player navigation context

This distinction is a core contract.

### Loaded context — PROTECTED

Contextual panel navigation and map drill-down use the map currently displayed:

- `m_loadedSystemMapId` identifies the loaded System/Space context;
- `m_systemMapSnapshot.systemPositionLy` identifies its Galactic position;
- `m_loadedDetailTarget` identifies the loaded Details context;
- `m_loadedHubMapSystemId` + `m_loadedHubMapHubId` identify the loaded Hub.

If the player is physically in Sol while inspecting an unrelated system or an
empty sector, `System/Space -> Details -> Hub` must continue inside the inspected
context.

### Player context — PROTECTED, but only for direct selectors

F9-F12 are global presentation selectors. They intentionally resolve from the
player's current navigation state rather than from an arbitrary map selection:

- **F9** — Galaxy;
- **F10** — player's current System, or the highest meaningful interstellar
  Space sector;
- **F11** — player's current Details context; if no meaningful Details address
  exists in interstellar space, falls back to System/Space;
- **F12** — player's matched Hub when available, otherwise the player's local
  Details/terminal spatial context according to the existing direct-selector
  rules.

Repeating the selector for the already visible navigation level is a no-op.
A newer pending selector supersedes an older pending selector.

Contextual panel buttons and direct F-key selectors are therefore **not the same
operation** and must not share player-relative fallback logic.

## 3. Empty Galaxy sectors

### Synthetic runtime identity — TEMPORARY but PROTECTED until replaced

An empty Galaxy sector has no catalog `systemId`. The current implementation
creates a unique negative synthetic `SystemMapSnapshot::systemId` for each
entered empty sector.

This negative id is intentional infrastructure today because it:

- gives each empty sector a distinct loaded-map identity;
- forces System navigation/camera state to reset between different empty
  sectors;
- lets the loaded context survive parent/child map transitions without being
  replaced by the player's physical current system.

The negative-id representation may later be replaced by an explicit spatial
address/domain type. Until that migration exists, removing or normalizing the
negative id is a regression.

### Empty-space Details — PROTECTED

A terminal cube inside an empty interstellar sector is a valid Details target.
The client composes it locally as an unbound `SpatialVolume`:

- `systemId < 0` remains attached to the target;
- `systemPositionLy` remains the selected Galaxy-sector position;
- `centerAu` and `edgeAu` come from the selected terminal cell;
- no `CelestialSystemSnapshot` is invented;
- no fallback to Sol/current player system is allowed.

This contract prevents the old failure where selecting empty space opened
Details near the Sun merely because the player happened to be in Sol.

## 4. Cubic navigation and terminal selection

### Explicit selection — PROTECTED

A navigation-grid anchor is not automatically a user selection. Details from a
cube requires an explicitly selected cell.

### Non-terminal selection — PROTECTED

The player does **not** need to manually descend to the final System-grid level
before opening Details.

When a cell at S0/S1/... below maximum depth is explicitly selected,
`resolvedTerminalSelection()` deterministically chooses the **central child at
every remaining level** until the maximum level is reached.

Equivalent rule:

```text
selected cell
  -> central child
       -> central child
            -> ...
                 -> terminal cell
```

The resulting terminal cell is the Details spatial address.

### Body occupying the selected address — PROTECTED

Inside a real known system, a terminal `SpatialVolume` is normally displayed as
empty/local space. If the **centre of that terminal volume lies inside a
celestial body's physical radius**, the semantic Details target is intentionally
promoted to that body.

This is address resolution, not a fallback to an arbitrary nearby body.

## 5. Selection semantics

### System/Space selection — PROTECTED

The current System layer can expose three relevant selection types:

- celestial body;
- hub/infrastructure;
- explicit cubic navigation cell.

`DETAIL` is enabled when at least one of those can resolve to a valid Details
target.

`HUB` is enabled only when a hub is selected and the current mode supports Hub
entry.

For a selected hub with a celestial parent:

- Details uses the parent body as the Details anchor;
- the hub remains the Details focus;
- Hub opens the selected hub itself.

For an unbound/deep-space hub, Details uses `LocalObject` with the hub as its
anchor/focus.

## 6. Native STAR ATLAS panel

### Fixed Close/toggle slot — REMOVED

The native panel does not contain a `Close` action and does not use a fixed
"current layer / Detail / Hub / Close" button model.

Buttons are semantic destinations generated from the current map mode.

### Navigation button matrix — PROTECTED

| Current mode | Button 1 | Button 2 | Button 3 |
| --- | --- | --- | --- |
| Galaxy | SYSTEM/SPACE | DETAIL disabled | HUB disabled |
| System/Space | GALAXY | DETAIL conditional | HUB conditional |
| Details | SYSTEM/SPACE | GALAXY | HUB conditional |
| Hub | DETAIL | SYSTEM/SPACE | GALAXY |

For `System/Space`, `DETAIL` is active only when the current body/hub/cell
selection resolves to a Details target. `HUB` is active only for a selected hub.

For `Details`, `HUB` is active only when the current Details selection exposes a
hub.

For `Hub`, the panel contains only parent destinations. It must never render the
old nonsensical `DETAIL, DETAIL, HUB` combination.

### Action routing — PROTECTED

The panel emits typed `SystemMapPanelActionType` actions. They resolve to
commands with mode-specific meaning:

- `OpenGalaxy`
  - Galaxy -> no-op;
  - System/Details/Hub -> Galaxy.
- `OpenSystem`
  - Galaxy -> enter the explicitly selected known system or empty sector;
  - Details/Hub -> return to the already loaded System/Space context;
  - System -> no-op.
- `OpenDetail`
  - System -> open Details for the selected body/hub/terminal cube;
  - Hub -> return to the already loaded Details target;
  - Galaxy/Details -> no-op.
- `OpenHub`
  - System/Details -> open the selected Hub;
  - Galaxy/Hub -> no-op.
- `SelectSystem`
  - selects the requested catalog system id through the native panel path.

Browser command strings are not part of this navigation contract.

### System selector dropdown — PROTECTED

The native panel contains a system list/dropdown with native scrolling and
selection. Panel input owns the right-side panel region before map camera/picking
input, so a list/button gesture must not leak through to the map viewport.

## 7. Camera and map interaction mechanics

The following behavior is already regression-tested and remains part of the
current map contract:

- orbit rotates the complete camera pose around the chosen pivot;
- zoom scales eye and target consistently;
- celestial-body zoom respects safe surface clearance;
- rotation pivot priority is body, explicit cell, cursor cell, mouse plane;
- zoom pivot is cursor body or current camera target;
- refine/coarsen preserves the navigation point;
- implicit anchor and explicit cell selection are separate states;
- fixed mouse/scroll traces are deterministic on replay;
- semantic picking consumes the prepared CPU presentation frame rather than a
  renderer-owned previous-frame cache;
- camera projection math belongs to immutable camera snapshots/View state.

## 8. Frame and transition mechanics

### One frame-stable map snapshot — PROTECTED

The production frame order is:

```text
refresh authoritative/local data
-> prepare map presentation before input
-> input and picking consume that prepared frame
-> simulation/network update may continue
-> rendering consumes the same prepared frame
```

Map input and rendering must not observe two different snapshots in one
application frame.

### Deferred mode transitions — PROTECTED

Internal Galaxy/System/Details/Hub mode changes use `MapTransitionController`.
The outgoing frame remains the outgoing mode until transition capture/commit is
complete. A mode change must not mutate the visible mode early.

### Direct F-key navigation — PROTECTED

Direct F9-F12 routing is application-level presentation selection, not an
internal map drill animation. A direct selector cancels stale internal map
transition state so an outgoing map frame cannot overwrite the requested global
presentation destination.

## 9. Data ownership

### Server authority — PROTECTED

The server remains authoritative for runtime facts, IDs, simulation state,
replication and timeline epochs.

### Client map composition — PROTECTED

Production maps are assembled client-side from:

```text
authoritative replicated facts / epochs
+ local catalog data
+ deterministic celestial state
+ local presentation state
```

System/Details/Hub-specific presentation is not a second authoritative world
state channel.

An empty-sector Details scene is entirely local spatial composition because
there is no star-system runtime to request.

### Timeline revision fence — PROTECTED

A universe timeline revision invalidates old map snapshot/interpolation data.
The semantic loaded target survives the fence so the client can request/rebuild
the same System/Details/Hub context on the new timeline branch.

## 10. Other active map controls

- `Ctrl+F11` cycles the global `CoordinateDisplayService` notation used by map
  address presentation. **PROTECTED**.
- `Ctrl+Alt+F12` cycles the player-facing UI language, including map labels,
  with English fallback for missing translations. **PROTECTED**.
- The panel occupies the right side of the single native OpenGL surface; the
  map renders in the remaining viewport. **PROTECTED current UI architecture**.

## 11. Temporary and legacy mechanics still present

These exist in source today but must not be confused with the canonical player
navigation design.

### `MapMode::System` also represents Space — TEMPORARY

There is no separate `MapMode::Space`. The generic cubic layer is still named
`System` internally and is relabeled to `SPACE` in presentation for empty
sectors. This is acceptable current debt; changing it requires a deliberate
model migration, not a text-only rename.

### Negative empty-sector system ids — TEMPORARY

Negative synthetic ids are an implementation mechanism, not durable universe
identity. They are nevertheless required until a first-class spatial-domain
address replaces them.

### `P` map shortcut — LEGACY

While the System Map UI owns input, `P` still invokes
`setSystemMapDetailMode()`. The native semantic panel is the canonical route.
This key may be removed after manual/debug reliance on it is no longer useful.

### `Backspace` current-system shortcut — LEGACY

While the System Map UI owns input, `Backspace` still invokes
`setSystemMapCurrentSystemMode()`. This is not the semantic parent-layer
contract; the panel uses `setSystemMapLoadedSystemMode()` when returning from
Details/Hub so the inspected context is preserved.

### `SystemMapRenderer` facade and `.inl` backend splits — TEMPORARY

The facade still coordinates Galaxy/System rendering and includes some
low-level `.inl` backend pieces. Detail/Hub rendering ownership has already been
extracted. Further backend cleanup is architectural work and must preserve the
navigation contracts in this file.

## 12. Explicitly not implemented / not complete

The following must not be inferred from existing code:

- full ship selection on System Map -> Details is **NOT COMPLETE**;
- historical/planned/predicted trajectory presentation is **NOT IMPLEMENTED**;
- a first-class durable `SpaceSectorId`/spatial-domain type replacing negative
  synthetic ids is **NOT IMPLEMENTED**;
- a separate public `MapMode::Space` is **NOT IMPLEMENTED**;
- browser/WebView command transport for STAR ATLAS navigation is **REMOVED** and
  must not be revived as a parallel path;
- a `Close` map-panel button is **REMOVED** and is not part of the design.

## 13. Regression gates

Before modifying this behavior, run:

```bash
bash tests/system_map/run_mingw64.sh
bash tests/architecture_contracts/run_mingw64.sh
bash tests/client_acceptance/run_mingw64.sh
```

The map gate owns camera/cubic-navigation/panel-semantics/context contracts. The
architecture and client-acceptance gates additionally protect the production
SpaceState/native-panel/live request path.

A change to any **PROTECTED** item in this file should be accompanied by an
explicit test update and an explicit edit to this contract. A test should not
be weakened merely to make a behavioral change pass unnoticed.
