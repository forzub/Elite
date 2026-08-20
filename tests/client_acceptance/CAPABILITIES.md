# Client capability regression matrix

This file is the functional inventory of client mechanics that the project is
allowed to treat as working. A capability is **protected** only when a test
exercises its production path, or when a narrow architecture guard ties a
headless test seam to the exact runtime/UI binding that ships in the game.

The acceptance gate protects behavior and displayed data, not visual styling.
Fonts, colors, antialiasing, decorative layout and pixel-perfect framebuffer
comparisons are deliberately outside this matrix.

| Capability | Current runtime interpretation | Regression owner | State |
|---|---|---|---|
| Local session boot/sync | Real `LocalGameSession` reaches gameplay-ready client state | client acceptance | protected |
| Player system/reference frame | Player starts in the authoritative active system with a valid hub frame | client acceptance | protected |
| Keyboard -> ship control | W/S, A/D, Q/E, +/- longitudinal command, keypad manoeuvre thrusters, Ctrl+F10 local-law switch, HOME/INSERT/END vector actions, J gating | client acceptance | protected |
| Player attitude | Production fixed-step control changes orientation; forward/right/up remain orthonormal and handed correctly | client acceptance | protected |
| Player HubTactical motion | Client command -> server acknowledgement -> authoritative ship-local movement inside owned travel frame | client acceptance | protected |
| Orientation drives thrust direction | After yaw, forward manoeuvre movement remains on the ship's forward side | client acceptance | protected |
| Dual local flight laws | Newtonian forward-thrust/coast and Assisted target-VREL control share per-ship speed/acceleration limits | client acceptance + architecture contracts | protected |
| Idle stability | No-command player does not drift or rotate in canonical local state while its travel frame is matched | client acceptance | protected |
| Accelerated universe-time diagnostic | Enter/exit revision fence; controls touched while frozen do not leak back into gameplay | client acceptance + world runtime | protected |
| Remote NPC presentation | Hub Motion Lab ships move through authoritative snapshot/interpolation path | client acceptance + presentation pipeline | protected |
| HUD coordinates/speed data | Client render state -> canonical travel-frame `VREL` -> production formatter -> exact `main_coord_*` UIText bindings | client acceptance + architecture guard | protected |
| Cockpit flight-vector instrument | One circular instrument consumes canonical VREL, bounded speed fraction, hull-vs-velocity basis, localized/preformatted speed + mode text | client acceptance + architecture guard | protected |
| F9-F12 navigation layout | F9 Galaxy, F10 current System/sector, F11 current Details context, F12 current Hub/local cube; direct selectors are idempotent and the latest pending selector wins | client acceptance + architecture guard | protected |
| Galaxy terminal cube drill | Refining/zooming past the terminal Galaxy cube enters the selected known system or empty System-space sector and synchronizes the global Navigation target | system_map behavior + architecture guard | protected |
| Ctrl+F11 coordinate format | Ctrl+F11 cycles `CoordinateDisplayService`; map footer advertises the chord | client acceptance + architecture guard | protected |
| Ctrl+F12 constellations | Ctrl+F12 toggles the gameplay starfield constellation overlay | client acceptance + architecture guard | protected |
| Alt+F12 sky culture | Alt+F12 cycles the selected star-culture topology without changing overlay visibility | architecture + catalog contracts | protected |
| Ctrl+Alt+F12 UI language | Ctrl+Alt+F12 cycles the global player-facing UI locale; missing translations fall back to English | localization + hotkey contracts | protected |
| Game-system names on sky | Authored game-system name survives astronomical-star merge and reaches the sky-label formatter | client acceptance + architecture guard | protected |
| Galaxy player navigation marker | Real player navigation position -> shared Galaxy marker resolver -> Galaxy map/panel | client acceptance + architecture guard | protected |
| Interstellar navigation presentation | Outside configured system-membership radius, navigation publishes `currentSystemId = -1` with galactic-absolute position; entering another catalog system rebases local coordinates | architecture contracts | protected |
| Map distance from player | `distanceFromPlayerLy` uses actual Galaxy player-marker position, not merely current-system center | client acceptance + architecture guard | protected |
| Native STAR ATLAS controls | Single-surface dropdown + semantic GALAXY/SYSTEM-or-SPACE/DETAIL/HUB navigation controls emit typed panel actions; the obsolete CLOSE slot is gone and no browser command transport participates | client acceptance + architecture guard | protected |
| Map action meaning | Select/open/Galaxy/System-or-Space/Details/Hub typed actions preserve the loaded navigation context and parent/child layer semantics | client acceptance + architecture guard | protected |
| Galaxy map data | Live client/server request and timeline-consistent snapshot | client acceptance | protected |
| System map data | Live request for current system and selectable hub inventory | client acceptance | protected |
| Details map data | Semantic Details target survives live request/response path | client acceptance | protected |
| Hub map data | Selected hub survives live request/response path | client acceptance | protected |
| Map panel displayed data | Live map/navigation state -> typed `SystemMapPanelPresentation` -> native OpenGL STAR ATLAS panel | client acceptance + architecture guard | protected |
| Map camera/grid/picking | Camera, cubic navigation, picking and presentation contracts | system_map | protected |
| Radar simulation/HUD | Both runtime feature flags are currently `false`; the subsystem is intentionally inactive | runtime feature flags | disabled, not claimed |
| Jump flight mode | `jumpActive` exists in control state but has no current keyboard mapping in `PlayerInputMapper` | none | not claimed working |
| Cruise as a runtime motion mode | J currently gates controls via `cruiseActive`; initial player runtime mode remains `HubTactical` | input acceptance | control flag only |

## Acceptance boundary

"Displayed data" means that the value reaches the real UI binding/API used by
the game. The suite does **not** compare screenshots or colors. A renamed HUD
node, severed `SpaceState` presenter call, missing map-panel payload field,
broken HTML command, wrong map command dispatch, stale client state, or dead
server/client path must fail the gate.

When a protected mechanic later regresses, add the smallest reproduction to
this suite before fixing it. This keeps the harness growing from real failures
instead of becoming a second game engine.

## Current function-key coverage

The navigation layout is now the product contract:

- `F9` opens Galaxy.
- `F10` opens the player's current System, or the highest meaningful interstellar sector.
- `F11` opens the player's current Details context (body/hub/local spatial volume).
- `F12` opens the currently matched Hub, otherwise the player's terminal local cube.
- Repeating the function key for the currently visible map level is a no-op; pressing another F9-F12 switches directly to that map level, and a newer pending selector replaces an older one.
- `Ctrl+F10` switches Newtonian / Assisted local flight laws on a debounced F10 release.
- `HOME` remains nose-to-VREL alignment in Newtonian; in Assisted it installs a persistent 100% local-speed target until +/- or END overrides it.
- Cobra Mk.I linear acceleration envelope is 7.5 g (angular/load envelope remains 5 g), giving 1.5x the previous longitudinal speed-change response in both local flight laws.
- `Ctrl+F11` cycles coordinate display format.
- `Ctrl+F12` toggles constellations.
- `Alt+F12` cycles sky culture without changing constellation visibility.
- `Ctrl+Alt+F12` cycles the global player-facing UI language, including menus/maps/constellation labels; missing translations fall back to English.

The acceptance harness protects key-edge semantics and the production services/actions;
visual map rendering remains covered by the map contracts rather than screenshots.

## Interstellar-flight boundary

The current accelerated-universe mode is a diagnostic trajectory branch, not a
travel-time accelerator. It freezes gameplay controls (`gameplayDeltaSeconds = 0`),
advances passive diagnostic trajectories and discards that branch on return.
Therefore the suite does **not** claim that holding a course toward another star
and enabling accelerated universe time can fly the production player there.

What is protected now is the real navigation/display chain that already exists:
normal player thrust changes the authoritative navigation position; the Galaxy
map player marker resolves from that live position; a selected game-system star
remains visible in the panel; and `distanceFromPlayerLy` is measured from the
actual player marker rather than from the center of the current system.

The accelerated diagnostic branch may also cross the navigation-domain boundary
without committing that travel into production state. Outside the configured
system-membership radius the published navigation state becomes interstellar
(`currentSystemId = -1`) and the Galaxy marker uses galactic-absolute position;
inside another catalog system it is rebased to that system. This makes the
in-game "point at a catalog star, accelerate time, watch the Galaxy marker"
check a geography diagnostic. It is still not a production interstellar/cruise
travel mode.
