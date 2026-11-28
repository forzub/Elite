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

## Stage 3 — planner reliability, finite processes

Cause (source-audited, `SpaceStateDocking.cpp` executing-phase result handling): a RETRYABLE plan failure
(`job->plan.retryable()`, e.g. "continuous Assisted optimizer did not reach a feasible path") sent the request back to
Stabilizing -> Planning with NO bound. From a stabilized rest state the same request gives the same answer, so a
deterministic infeasibility re-planned for ever: the author's EN ROUTE at Vysokiy (B2-04 from the side, 2026-11-27).
The other recovery path (`recoverOrFail`) was already bounded (5 attempts, 5 s apart).

Change: `m_dockRetryablePlanFailures` per request (reset with the request serial). Attempts 1..2 re-plan (logged
`phase=replan attempt=n/3`); the 3rd refuses the request: `phase=refused`, the pilot gets the localized reason
`cockpit.dock.refused.no_route` (en / ru / zh-Hans / es / ja) in the docking widget and the terminal line.

Reproduction (live): `ELITE_DEV_START_CITY_AT=0,2810,5588` (new DEV: a normal-flight start at a city-frame point; the
author's ship was at planning (-5588,-3190,0) = city (0,2810,5588)) + `padland:B2-04`: the first plan infeasible
(the author's failure, reproduced), replan 1/3 feasible, LANDED 101 s. Grid tool: `tests/planner/planner_start_grid.sh`
(LANDED / REFUSED / CRASH / HANG per start point).

## Stage 4 — B3: one authoritative executed route

Source audit (an exploration of every route representation): no single object held "the route being flown". The
server's pad-service route (`PadLanding::route` / `leg`) was never replicated - the HUD recomputed it from the ship's
smoothed place (`hubPadRoute` + a guessed leg, function statics) and could disagree with what the server flew; the HUD
also kept world-anchored "held" frames and synthesized a straight row to the route's finish (my 2026-11-27 fallback),
which drifted off a moving / turning hub and drew frames to waypoint #1 even when landed.

Change:
- `ShipExecutedRouteSnapshot` (requestId, revision = content key of points + current point, owner, phase, frame module,
  points in the city frame, current point) - filled by the pad service from the SAME values its steering switch uses,
  replicated on the player's ship (wire schema 85 -> 86).
- The HUD pad tunnel draws `playerShip.executedRoute` from its current point; the client route builder is gone.
- The docking computer's own plan stays published while the SAME request stabilizes / secures its course / re-plans
  (`keepExecutedRoutePublished`); a refused request shows none.
- The HUD fallback (held / straight frames) removed.
- Barrier `check_executed_route_single_source.py`.
Live: ring A1-03 landing - the tunnel from the server's route, 12-13 frames in phase 1, frames down to the touchdown in
phase 2 (`D:/tmp/lifecycle/run4.log`, `[PadTunnel]`).
Residual: between two plans of the docking computer (at the ring's sync gate, while the co-rotating leg is planned) the
old plan is all behind the ship - no frames for those seconds (honest: no route exists yet); the map's route line still
reads the published corridors, not the executed-route object (one owner per producer, not one object for all).

## Stage 5 — A2 / A3 / A6

- A6 (source + unit): ONE infrastructure state vocabulary `InfrastructureState {Unknown, NotBuilt, Existing, Damaged,
  Destroyed}` (`CityPadPresence.h`), used by the server's pad request refusal and the client's pad state. DEFECT FIXED:
  the client answered from the city package of an EARLIER construction stage while the new stage's package assembled
  (`stageKey` != the stage in force) - a pad just finished read "not built"; now Unknown (the server decides).
  HubSpaceCheck: the state mapping and its usability equal the presence's.
- A2 (source + unit): ONE co-rotation rule on `CityCoRotationAt` (`cityVelocityAt`, `relativeVelocity`, `pointToMap`,
  `vectorToMap`, about the attachment pivot); the two hand-written copies in SpaceStateDocking use it. HubSpaceCheck:
  worst residual 0.18 mm at 3.3 km - the float rotation matrix of `hubLocalEulerDegToMatrix` (residual limit, noted).
- A3: the marker / physical projection split (`applyMarker` vs `apply`) and picking by stable id were done on
  2026-11-27 (9900afb34, live LAND); not reworked.

## Stage 8 — the world save

`docs/WORLD_SAVE_CONTRACT.md`. DEFECT FIXED: the economy and the runtime ships were two files written at different
moments (a ship bought between them: the price debited, no ship on load). Now ONE generation of all parts committed by
one atomic manifest write, verified on load (size + FNV-1a 64), the previous generation as the fallback, the legacy
layout read once; shutdown writes the last generation while the owners exist. `writeFileAtomically` moved into
WorldSaveStore.cpp (the one write primitive). Unit: `EliteWorldSaveCheck` OK. Live: save -> exit -> load (generation 1,
day 10204) -> continue -> generation 2 (`D:/tmp/save1`).
Residual: the save is enabled by `ELITE_ECONOMY_STATE_FILE` only; a default location / "Continue" is an author decision.

## Stage 9 — decomposition, DEV switches, one capability policy

- `core::Capability` (10 named capabilities with reasons) + `capabilityAllowed()`: every preview difference asks the
  policy; only BuildFlavor.cpp asks the flavour. Barrier `check_capability_policy.py`.
- DEV switches inventory `docs/DEV_SWITCHES.md` (`tools/dev_switch_inventory.py`, `check` mode): 197 switches - 133
  `ELITE_DEV_*`, 64 others (configuration, but also cheats and dumps: `ELITE_FUEL_FREE`, `ELITE_CATALOG_SHIPS`).
- The real preview package REMOVES every `ELITE_*` variable from its process at start except `ELITE_SESSION_LOG` and
  `ELITE_SERVER_OWN_CLOCK` (`core::scrubDeveloperEnvironmentInPreview`, client and server mains).
- SpaceState* split by responsibility: NOT done in this pass (the files stay large; stages 2 / 4 moved state out of the
  render functions, which was the coupling that broke things).

### Stage 3 — the start grid (live, 10 points around Vysokiy, pad B2-04, `D:/tmp/grid_full2/summary.txt`)

| point (city frame) | outcome | replans | t |
|---|---|---|---|
| 0,2810,5588 (the author's) | LANDED 101 s | 1 | 309 s |
| 0,2810,-5588 | LANDED 102 s | 0 | 282 s |
| 5588,2810,0 | REFUSED (3 infeasible plans) | 2 | 74 s |
| -5588,2810,0 | LANDED 101 s | 0 | 302 s |
| 0,6000,0 | **HANG** | 0 | 482 s |
| 0,-6000,0 | LANDED 92 s | 0 | 266 s |
| 0,1500,3000 | LANDED 102 s | 0 | 339 s |
| 3000,1500,3000 | REFUSED (3 infeasible plans) | 2 | 78 s |
| 0,4000,10000 | LANDED 102 s | 0 | 260 s |
| -8000,500,-8000 | REFUSED (3 infeasible plans) | 2 | 74 s |

Shares: success 60 %, refusal 30 %, hang 10 % -> the gate (zero hangs) FAILED on the first grid. Cause of the hang
(point 0,6000,0 = the hub's centre, inside `station_core_front`): a SECOND unbounded loop - lift-clear 62 m -> "clear,
planning" -> the safe-course check still "in contact (865 boxes)" -> lift again (215 lift traces). Fix: at most 3
lift-clears per request, then `cockpit.dock.refused.enclosed`; plus a REQUEST WATCHDOG - no route executed for 240 s of
server time (any loop between the bounded paths) -> `cockpit.dock.refused.timeout`. Re-run of the failed / refused points:
below.
The three REFUSED points are deterministic planner limits (the continuous Assisted optimizer infeasible from those
starts three times): finite and told to the pilot, NOT solved - a planner capability residual.

Re-run after the fix (`D:/tmp/grid_retry/summary.txt`): 0,6000,0 -> REFUSED "still in contact after 3 lifts" (182 s);
the three optimizer-limited points REFUSED again in 77-83 s (deterministic). FINAL GRID: 6 LANDED / 4 REFUSED / 0
CRASH / **0 HANG** (gate passed on the re-run; the first-run failure stays recorded above).

## Stage 7 — D1 / D2: the server owns time

Matrix (`tests/server_clock/clock_matrix.sh`, live, ~100 s each, `D:/tmp/clock_matrix`), the clock's own account line
every 10 s (`[ServerClock] account`):

| case | before the fix | after |
|---|---|---|
| own clock baseline | simulated 70.16 / wall 70.10 s, dropped 0 | 70.12 / 70.07, dropped 0 |
| menu pause 10 s (`ELITE_DEV_PAUSE=40:10`) | PAUSED / RESUMED, simulated = wall - 9.95 s, no catch-up | (unchanged path) |
| render stalls 1000 + 500 ms | dropped 0 | (unchanged path) |
| worker blocked 300 ms (`ELITE_DEV_WORKER_STALL`, new DEV) | **dropped 63 ms** (FAIL) | dropped 0, caught up |
| worker blocked 3000 ms | OVERLOADED, dropped 2762 ms, recovered | OVERLOADED, 0.97 s carried (the cap), dropped 2036 ms, recovered |
| the old client-driven clock, render stall 1000 ms | dropped 601 ms | - |

Cause of the FAIL: the runner's per-call frame clip (250 ms, made for the client-driven frame) cut a worker stall the
debt cap (500 ms) could carry. Fix: the own clock hands its elapsed time in <= 250 ms pieces up to frame cap + debt
cap (0.75 s); the rest in one last call that the runner clips and counts (an explicit, bounded drop).
D1 (determinism across frame rates) was accepted on 2026-11-27 (fc1cf6377) and not reworked.
Default: the own clock stays OPT-IN (`ELITE_SERVER_OWN_CLOCK=1`) - not yet run: a TCP / remote session and the window
losing focus under it. These two runs are the remaining acceptance before it becomes the default.

## Stages 6, 10, 11 — status of this pass

- Stage 6 (B1 / B2 / B4): NOT DONE in this pass. B1 retained rows (aa85da7ee) and the 20 km visual radius
  (d00ab432f, a POLICY, not the root cause of remote extrapolation drift) stand as before. The B4 flight wobble is
  pre-existing (the morning build 014da2f94: 30-32 attitude reversals / s; now 23-26): the server's rates are smooth,
  the client's drawn transform jitters per snapshot - the authoritative / predicted / correction / presentation layering
  is the open work. Two attempts on 2026-11-27 (error blending, dead reckoning) made it worse and were reverted.
- Stage 10 (D8 Host Game): NOT DONE. The analysis stands (NetworkServerHost owns one ServerRuntime; ServerWorker would
  own it when a host port is set). Its precondition - the own clock as the default (stage 7) - is not met yet.
- Stage 11:
  - D6: the third_party manifest with hashes (stage 0); a clean build from an empty directory (below); packages are
    built into a clean staging directory by `tools/preview/package_preview.sh`; D:/__elite/prodaction NOT replaced in
    this pass (no new package was requested).
  - D4: the context is already GL 4.3 core (GLFW 4.3 CORE_PROFILE); 63 shaders `#version 330 core` are valid in it,
    6 use 430 - nothing to port.
  - D5: low-overhead metrics exist as DEV switches (`ELITE_DEV_WORKER_TIMING`, the perf HUD, `[ServerClock] account`);
    no always-on metrics channel - partial.
  - D7: NOT DONE (data_terms are still keyed by the Russian source text; `TextRenderer::setTextFilter` remains the
    temporary bridge).
- Stage 12 (C): `tests/scenarios/run_acceptance.sh` gathers the accepted scenarios (ring landing, Preview landing, two
  worlds, save / continue, B2-04 side) + the unit checks; each scenario was run live in its stage (evidence above); the
  suite as one run: below if done.

## Scores (before -> after this pass; "before" = audit #8/#9 where scored)

| item | before | after | basis |
|---|---|---|---|
| A1, A3, A4, A5, A7, A8, A9, A10 | 9 / 8 / 8 / 8.5 / 8 / 8 / 8 / 8 | unchanged | accepted, not reworked (A3 marker split 9900afb34) |
| A2 transforms / co-rotation | 7.5 | 8 | one co-rotation rule + unit; axial-dock entry split still open |
| A6 infrastructure state | 7.5 | 8 | one vocabulary server + client; stale-package defect fixed |
| World lifecycle / state owners (stage 1) | - | 8 | 9 states moved to owners, barrier, live two worlds; concurrent worlds not covered |
| Simulation / render split (stage 2) | - | 7.5 | SpaceState render functions pure, barrier; renderers outside SpaceState not under it |
| Planner finiteness (stage 3) | - | 8 | 2 unbounded loops bounded + watchdog; grid 6 / 4 / 0 hang; 3 optimizer-limited refusals remain |
| B1 epochs / provenance | 6.5 | 6.5 | not reworked |
| B2 one presentation sampler | 5.5 | 5.5 | not done |
| B3 one executed route | 5 | 7 | pad service publishes its route, HUD reconstruction removed; the docking computer still via corridors, the map not on the object |
| B4 layers | (wobble open) | unchanged | not done |
| C behavioural suite | 7 | 7.5 | grid + matrix + suite script |
| D1 determinism | accepted | accepted | - |
| D2 server time | own clock opt-in | 7.5 | matrix passes incl. debt fix; TCP + focus loss runs open; not the default |
| D4 GL | - | 8 | already 4.3 core |
| D6 build / manifest | (P0 open) | 8 | manifest + hashes; clean-directory build PASSED (D:/tmp/cleanbuild2, 15641 steps, client + server + unit checks green) |
| D7 localization ids | - | unchanged | not done |
| D8 Host Game | - | unchanged | not done |
| Save (stage 8) | - | 7.5 | contract, one-generation atomic commit, verified load, live; no default location / Continue (author) |
| Preview policy (stage 9) | - | 8 | one capability policy + env scrub + inventory; SpaceState* split not done |

The program's target (>= 8 for every direction) is NOT met for: B1, B2, B4, D7, D8, the SpaceState* decomposition, and
(narrowly) B3, D2, the save and the render split. Those are the next work, in this order: B4 / B2 (one presentation
sampler with the correction layer), the own clock's TCP / focus runs -> default -> D8, B3's docking side, D7 ids.

## Commits of this pass

f65fa81f8 (stages 0-2), 90afb39c4 (stages 3-9), 42d59707a (stage 3 loops + stage 7).

Clean build (stage 0 / D6): from an empty directory `D:/tmp/cleanbuild2` (`-DELITE_BUILD_CLIENT=ON -DELITE_BUILD_SERVER=ON`,
RelWithDebInfo) at 42d59707a: exit 0, 15641 steps; EliteHubSpaceCheck and EliteWorldSaveCheck from it pass. (The first
attempt on 2026-11-27 failed because I edited sources during it - not a baseline defect; recorded.)

## Audit #11 continuation (2026-11-28)

### Planner: a route is ALWAYS built (the author's rule; exception: an enclosure without an exit wide enough for the hull)
- Root cause of the 6 km refusals (source + live): the lane-chain search chose the direct free leg by TIME (64 s vs 86 s
  through the lanes, `direct_m` / `chain_cost` are seconds); the continuous optimizer could not fly that 6 km leg, and
  every retry asked for the same leg. The 3-attempt bound (stage 3) turned the old endless re-planning - which sometimes
  succeeded by chance - into a refusal: the regression the author saw. The route-draft net is loaded and used (its
  drafts were infeasible there; not the cause).
- Strategy 2 (lanes-only recovery): a retried request forbids the direct free leg while a lane entry exists - "dive into
  the nearest green zone". Live: the 4 points refused before (incl. the author's) LANDED 61-120 s, 1 replan each.
- Strategy 3 (THE GUARANTEED ROUTE, `VoxelRoutePlanner.h`): resolution-complete voxel search - an escape on a fine grid
  (half the hull radius: a gap narrower than the hull stays closed) off a pad / out of a hangar / out of a solid volume,
  then A* over a coarse transit grid; every polyline segment checked against the exact obstacle geometry. Flown leg by leg
  (escape legs slowly with the attitude held; else stop - turn - go), shown as frames (the polyline is the plan's gates),
  the pad handed to its service in reach. Refusal ONLY when the escape search proves no exit for the hull.
  Unit `EliteVoxelRouteCheck`: round a wall, off a pad, out of a hangar through a door wider than the hull, ENCLOSED for a
  door narrower than the hull and for a closed cube, out of a solid block.
  Live: a start 660 m inside the hub's solid core box -> "deep embedded: straight out 825 m", flown at 5 m/s; after
  ~470 m the physics' contact with the core's real collision geometry held the ship and the progress watchdog let go at
  90 s (finite, told). Residual: a DEV start inside a solid core is not a gameplay state; the escape fights the contact.
### E3 watchdogs (audit #11 P0)
- The watchdogs now run BEFORE every early return (the lift-clear and the fallback course returned before the old one).
- A separate PROGRESS watchdog for every executed motion (route progress, fallback points / distance, lift height): no
  5 m of progress in 90 s -> refused (`cockpit.dock.refused.no_progress`), the autopilot released, a stop commanded,
  every executor's state cleared in fail(). A legitimate wait (the dispatcher queues / holds / flies the ship, the ring
  sync hold) pauses it. Live: the embedded run above ended by this watchdog.
- NOT yet run live: a ship without thrust (ELITE_DEV_DESTROY_MODULES_AT on the engines + RCS mid-route).
### E4 save (audit #11 4.1-4.5)
- 4.1: the ships part is serialized by `buildRuntimeShipsJson()` INSIDE the economy's flush (the provider), i.e. the same
  server tick as the economy - no older copy.
- 4.2: `WorldSaveStatus` {None, Loaded, LoadedPrevious, Legacy, ManifestCorrupt, VersionUnsupported, NoUsableGeneration};
  a present manifest never falls back to the legacy layout; protected statuses disable saving.
- 4.3: commit names as fallback the newest generation that VERIFIES; a damaged newest stays on disk unnamed, never deletes
  the known-good one. New-generation numbers never collide with existing part files.
- 4.5 (unit, `EliteWorldSaveCheck`): corrupt manifest (no legacy fallback, commit refuses), unsupported version, missing
  part, partial write -> previous; save after a restore keeps the known-good; the new generation damaged again -> the
  known-good loads; a LOCKED manifest -> the commit fails and the previous generation stays intact. The locked case found
  a real defect: an unreadable manifest read as "absent" and generation 1 was rewritten over the good one - fixed
  (Unreadable state, the commit waits).
- Live: `run_acceptance.sh save_continue` PASS (run 2 restored run 1's last generation, committed a newer one, ships part
  included). The acceptance suite now returns a non-zero exit code on any failure.
### D2: the own clock's chunk sizes come from the runner's `ServerTickPolicy` (no second copy).
### B4 (started): the own ship's attitude LAYERS in ClientWorldState (authoritative raw snapshot / predicted to "now" in
every mode / decaying correction / presentation; `ELITE_DEV_ATTITUDE_LEGACY=1` A/B). Cause found by the source audit: the
presentation target was reset to each snapshot (16.7 Hz) - no extrapolation outside HubTactical - and chased by a 22/s
exponential filter: a rate pulse per snapshot. Live A/B on the B2-04 approach (`tools/attitude_report.py`): reversals
26.9 -> 22.3 /s, drawn max 81 vs auth 88 -> 20.3 vs 20.0; the drawn spread did NOT improve (2.29 -> 2.35, auth ~1.6).
NOT solved: the remaining jitter is the next measurement (suspected: the presentation clock vs the frame clock).

### B4 / B1 — the own ship's attitude (2026-11-28, continued; supersedes the "26.9 -> 22.3" note above)
Investigated by measurement, step by step (`[AttitudeAuth]`, `[AttitudeLayers]`, `[AttitudeLayerFrame]` under
ELITE_DEV_ATTITUDE_TRACE=1/12):
1. The AUTHORITATIVE stream is smooth: consecutive snapshots 60 ms apart, the replicated rates match the actual turn within
   ~5 % (e.g. 17.6 vs 18.7 deg/s). The wobble is entirely client presentation.
2. The first layered version was WRONG (my defects, all found by the traces): (a) slerp from identity to a w<0 error
   quaternion took the 360 - angle way; (b) the correction was taken against the previous FRAME's presentation - with
   frames about as often as snapshots that froze the drawn turn at every snapshot, the lag grew to the 25 deg snap limit
   (13-23 deg corrections); (c) the horizon on the server-time estimate ran to the 0.3 s cap.
3. B1 FINDING: the server-time estimate and the snapshots' stamps are DIFFERENT time bases - with the (default)
   client-driven server the stamps lose time at frame clips while the estimate runs on wall time. The snapshot's
   processing time is frame-quantized (+-30 ms). The consistent base is the presentation clock (built from the stamps,
   slewed): horizon = render time + its mean delay behind the newest snapshot - the snapshot's stamp.
Result (live, B2-04 approach, turning at ~15-17 deg/s): the per-snapshot correction (the visible jolt) **13 deg -> 1.3-1.7
-> 0.35-0.57 deg mean** (max ~2 deg), horizon ~20 ms; the presented attitude follows the authoritative continuously
(median 0.1 deg from the snapshot). The legacy path (ELITE_DEV_ATTITUDE_LEGACY=1) keeps the staircase + 22/s chase.
Measurement caveat: `tools/attitude_report.py` reversals use the clamped frame dt (50 ms) - biased at 16-20 FPS in the
city; the per-snapshot correction is the reliable measure. OPEN: position layers (same staircase for the position outside
HubTactical), the cockpit/HUD/map single epoch (maps use the snapshot epoch, the cockpit "now"), remote ships.
