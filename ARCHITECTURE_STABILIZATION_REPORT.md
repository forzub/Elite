# Hanted — Architecture Stabilization Program: report and execution log

The program's own deliverable (its final report) and its running log. PROJECT_STATE.md keeps the project state;
this file keeps the per-stage evidence of the program.

## Stage 0 — baseline (2026-11-27)

- HEAD at the start: `39fd7f5b745d8c1384d625989e954806f3cda930`; working tree clean; backup tag
  `stabilization-baseline-2026-11-27`.
- Protocol: WireProtocolVersion 60, SimulationSnapshotWireSchemaVersion 85.
- third_party: `tools/third_party_manifest.json` (versions, sources, sha256 over each folder's files; `python
  tools/third_party_manifest.py check`): asio 1.28.2 (1658 files), websocketpp 0.8.2 (271), webview 0.12.0 (69),
  miniaudio 0.11.21 (137), WebView2 SDK 1.0.1150.38 (45). asio's version was CHOSEN on restore (the pre-loss version is
  unknown); the others follow recorded traces.
- Baseline Python contract checks: 185 pass, 17 not pass (the pre-existing set: client_galaxy_map_catalog,
  flight_law_hard_isolation, function_key_presentation_route, kinematic_frames, model_asset_dependency_ownership,
  model_asset_extraction_proof, model_asset_function_purity, model_asset_localization_complete,
  model_asset_module_ownership, model_asset_physical_modules, model_asset_wizard_decomposition,
  navigation_foundation_lock, server_runtime_ownership, tcp_wire_transport, test_suite_source_integrity,
  travel_frame_ownership, ui_platform_fonts). Any NEW red = a regression of this program.
- Baseline behavioural checks: EliteHubSpaceCheck OK; D1 fps_independence PASS (fc1cf6377); live ring landing LANDED
  98-102 s; Vysokiy B2-04 from the author's side position: planner replan loop (stage 3 target).
- Clean build from an empty directory (D:/tmp/cleanbuild, not the existing build dirs): see below.
- Rules: no recursive deletion of folders without checking physical paths (junctions / worktrees); D:/__elite/prodaction
  untouched until a new package is verified.

## Stage 1 — state owners and the world lifecycle

Inventory (source audit, `tests/architecture_contracts/check_world_state_ownership.py` is now the barrier): mutable
function statics / namespace globals of the authoritative code that held WORLD state and outlived their world:

| state | was | now (owner) |
|---|---|---|
| pad service's held place per ship (`held`, `heldSince`) | function static keyed by EntityId | `PadLanding::heldPlan / heldValid / heldForSinceS` (dies with the request) |
| pad release time, zone-exit latch, take-off hold | namespace globals `g_padReleasedAt / g_padAwaitZoneExit / g_padTakeoffs` | `GameSimulation::m_padReleasedAt / m_padAwaitZoneExit / m_padTakeoffs` |
| city strike radius, city pad-node, combat radius caches | function statics keyed by object id | `GameSimulation` members (keyed by the static-object generation) |
| construction overrides (process-wide store in model_asset) | never reset | `resetHubExpansionOverrides()` in the `GameServer` constructor (a new world drops a previous world's schedule; old plans retired, not freed) |
| ELITE_DEV_START done / wait | function statics | `GameServer::m_devStartDone / m_devStartWaited` |
| DEV script runner progress | process singleton | `DevScriptRunner::beginWorld()` from the SpaceState constructor |

Defect class: a second world in the same process (exit to menu -> new game; Host Game later) reuses ship ids (the
player is 204 again) and restarts server time and generations: the pad service would take the first world's place for
ship 204 (`p.sinceS > heldSince` false at a restarted clock -> the old place integrated), the take-off hold of world 1
applied in world 2, strike radii keyed by a reused object id with a restarted generation counter.

Allowed and listed (with reasons) in the barrier: immutable-asset caches, DEV schedules, diagnostics, the economy
service binding (one installed world with an explicit install/uninstall), the override snapshot cache (keyed by the
store's generation, which the reset bumps).

DEV test: `ELITE_DEV_NEW_WORLD_AT_S=<s after the world is ready>` leaves the world to the main menu and starts a new
local world in the same process.

Live evidence (relwithdebinfo, visible window):
- two worlds in one process (`ELITE_DEV_NEW_WORLD_AT_S=150`): world 1 requested A1-03 and was destroyed MID-LANDING;
  world 2 (the player is ship 204 again) landed A1-03, `LANDED (102 s)`, PadHold max step ~1 mm per frame - the
  same as a fresh process. Log `D:/tmp/lifecycle/run2.log`.
- The DEV start applied in world 2 (it did not before: the function static `done`).
- Not tested (residual): CONCURRENT worlds in one process - `EconomyService` binding and the DEV script runner are
  one-world-at-a-time; listed for stage 10 (Host Game).

## Stage 2 — simulation / update / presentation / render

Defect (source-audited; the cause of the 2026-11-27 Preview regression): `renderDockingWidget` computed the station
release notice and the console line the HUD terminal printed; `renderHudJump` set the system map's jump marks;
`renderHudDockingTerminal` expired the pilot notice and re-checked / expired the refused-berth watch;
`renderHudGuidanceAndCompass` ran the cockpit radio (reception, `radio.update()`, its notices). Skipping a render
(the Preview, a hidden HUD, a minimized window) changed what the game did.

Change: `SpaceState::update` now runs `updateJumpMapState`, `updateDockingWidgetState`, `updateCockpitRadioState`,
`updateTerminalNoticeState` every frame; the render functions only draw the stored result.
Barrier: `tests/architecture_contracts/check_render_purity.py` - every `SpaceState::render*` (18) is scanned for member
writes / state drivers; render scratch (camera pointer, prepared scene, camera-shake pulse, perf timers) listed with
reasons.

Live: the Preview flavor (`ELITE_DEV_PREVIEW_FLAVOR=1`) ring landing A1-03 `LANDED (102 s)`; the docking widget and
terminal lines present (log `D:/tmp/lifecycle/run3.log`). Python contracts 187 pass / the same 17 baseline reds.
Residual: renderers outside SpaceState (render/, system_map) are not under the barrier yet.
