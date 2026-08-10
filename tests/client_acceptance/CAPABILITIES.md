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
| Keyboard -> ship control | W/S, A/D, Q/E, target-speed keys, keypad manoeuvre thrusters, J cruise gating | client acceptance | protected |
| Player attitude | Production fixed-step control changes orientation; forward/right/up remain orthonormal and handed correctly | client acceptance | protected |
| Player HubTactical motion | Client command -> server acknowledgement -> authoritative hub-local movement | client acceptance | protected |
| Orientation drives thrust direction | After yaw, forward manoeuvre movement remains on the ship's forward side | client acceptance | protected |
| Engine target-speed control | Increase/decrease input changes authoritative target forward speed | client acceptance | protected |
| Idle stability | No-command player does not drift or rotate in canonical hub-local state | client acceptance | protected |
| Accelerated universe-time diagnostic | Enter/exit revision fence; controls touched while frozen do not leak back into gameplay | client acceptance + world runtime | protected |
| Remote NPC presentation | Hub Motion Lab ships move through authoritative snapshot/interpolation path | client acceptance + presentation pipeline | protected |
| HUD coordinates/speed data | Client render state -> production formatter -> exact `main_coord_*` UIText bindings | client acceptance + architecture guard | protected |
| F9 coordinate-format cycle (current binding) | F9 edge/latch -> `CoordinateDisplayService` cycle -> map footer format name | client acceptance + architecture guard | protected, binding temporary |
| F11 map open/close latch | F11 edge/latch semantics and `SystemMap` UI toggle remain connected in `Application` | client acceptance + architecture guard | protected |
| F12 constellation overlay (current binding) | F12 input toggles the gameplay starfield constellation overlay | client acceptance + architecture guard | protected, binding temporary |
| Game-system names on sky | Authored game-system name survives astronomical-star merge and reaches the sky-label formatter | client acceptance + architecture guard | protected |
| Galaxy player navigation marker | Real player navigation position -> shared Galaxy marker resolver -> Galaxy map/panel | client acceptance + architecture guard | protected |
| Map distance from player | `distanceFromPlayerLy` uses actual Galaxy player-marker position, not merely current-system center | client acceptance + architecture guard | protected |
| System-map WebView commands | Actual HTML command vocabulary -> production parser -> production command dispatcher | client acceptance + architecture guard | protected |
| Map command meaning | Select/open/Galaxy/System/Details/Hub/close commands dispatch the expected actions | client acceptance | protected |
| Galaxy map data | Live client/server request and timeline-consistent snapshot | client acceptance | protected |
| System map data | Live request for current system and selectable hub inventory | client acceptance | protected |
| Details map data | Semantic Details target survives live request/response path | client acceptance | protected |
| Hub map data | Selected hub survives live request/response path | client acceptance | protected |
| Map panel displayed data | Live map/navigation state -> production JSON payload -> `window.setSystemMapPanel` consumed fields | client acceptance + architecture guard | protected |
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

## Current function-key coverage and planned remap

The acceptance suite currently protects the bindings that actually ship today:

- `F9` cycles the map coordinate display format through `CoordinateDisplayService`.
- `F11` toggles the System Map UI.
- `F12` toggles the gameplay constellation overlay.

These key numbers are **not** treated as permanent product semantics. The planned
layout is `F9 = Galaxy`, `F10 = System`, `F11 = Details`, `F12 = Hub/local area`,
with coordinate-format and constellation toggles moving to `Alt`/`Ctrl` chords.
When that remap lands, update the hotkey assertions while keeping the protected
map actions, coordinate-display service and constellation-overlay behavior.

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
actual player marker rather than from the center of the current system. A true
interstellar/cruise travel mode should add its own acceptance scenario when it
exists.
