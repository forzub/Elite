# ELITE — PROJECT STATE (single canonical continuation document)
Updated: 2026-11-02, Europe/Kyiv. Repository: `forzub/Elite`. Branch: **main ONLY**.

## Hard rules
- This file is the ONLY active continuation state. No alternative state /
  notes files, no side branches. History lives in git, not here.
- Full chronology up to 2026-10-29 (every dated gate, failed gates included):
  `git show 3435b220:PROJECT_STATE.md` (older: `PROJECT_STATE_HISTORY_THROUGH_2026-09-13.md`).
- Work in small source-grounded steps; WIP commits on main; commit != acceptance.
  Status levels: SOURCE / BUILD PASS / AUTOMATED PASS / GAME START /
  HEALTHY FLIGHT / DAMAGE FLIGHT.
- User Windows: MSYS2 MinGW64 (`/c/msys64/mingw64/bin` on PATH; do NOT put
  `/c/msys64/usr/bin` first - the game then loses LOCALAPPDATA/TEMP), CMake +
  Ninja. `build/` = game (Debug -O0), `build/headless_server/` = server + DEV tools.
- D:/tmp holds only transient `elite_*` files, emptied per stage; never touch
  other files there. Never kill processes by broad name; DEV scripts stop
  only the PID they started; check `tasklist` for a user game first.
- **Recovery checkpoint:** `44bbbcf829763bc1c86e41ab3cbf46cf17896308` (empty
  commit pointing to the pre-Claude-cleanup tree of `590eb24a`). Never rewrite.

## Architecture (current)

### Server / client
- Server is the only physical authority (equipment, damage, power, heat, real
  forces and torques, rigid body, NPCs, collisions). Client interpolates and
  extrapolates kinematically; no second force model. Pilot and autopilot send
  the SAME numbered `ShipControlState` through `ShipControlExecutionGateway`.
- `ObjectModuleRuntime` = authoritative equipment state (damage causes
  accumulate). `HitVolume.health` = structural hit volumes, not engine authority.
- Production ships: Cobrett-I N1 (Newtonian), A3 (Assisted) passports; I/Y/X
  are validation configurations. Body basis +X right, +Y up, -Z nose.

### Navigation (Newtonian stack)
- ONE factual `NavigationControlCapability` (main, signed RCS per direction,
  signed angular, gimbal, limits; zero is zero) read by route request,
  profile, compiler and pilot; published by the server
  (`ShipControlAuthorityTelemetry`) incl. the RCS gas pressure share.
- Route autopilot `DedicatedClientRouteAutopilot<Law, Pilot>` (per-law control
  flow, pilot) + `RouteExecutionCore` (.cpp; continuous reference, runtime
  speed profile, thrust-reversal coast, curve maps, program builder) on
  `RouteExecutionState` -> `NewtonianMotionProfile` (one profile owner) ->
  `NewtonianManoeuvreProgram` compiler -> `NewtonianOptimalController` pilot
  -> `NewtonianExecutionSafetyMonitor`. Doctrine constants:
  `NewtonianFlightPolicy`; one turn model `NewtonianTurnModel`; narrow zones:
  `NarrowZoneEnvelope::analyseRotationFit`; speed band
  `NewtonianSpeedTolerancePolicy`.
- Capability change during a route (`NewtonianCapabilityWatch`, 10 % / 0.5 s):
  in-place reprofile + recompile, deferred inside narrow zones; hand-back for
  a replan only when a speed ahead is unreachable, an RCS direction is dead or
  no authority is left. Reactors off: neutral coast, wait, replan.
  Failed attempts coast and replan (5 attempts >= 5 s apart).
- Rotating dock: terminal roll follows the live dock roll phase; with a
  rotating terminal frame rest is judged on pitch/yaw.
- RCS gas: installed Cobra AT-500 tank (module `rcs_gas_tank`); refill only
  while the nozzles draw nothing; below the 20 % regulator knee the nozzles
  (and the published authority) weaken; routes start from rest at >= 75 % fill.

### Propulsion / power / thermal (as in source; see the stage audit below)
- ER-5 propulsion generators feed main + RCS (RCS never draws from the 70 MW
  service reactor: `m_powerBus.setAuxiliaryDemandMW(0)`); ER-5 has its own
  radiation pad (11 x 11 m, 6 MW/m^2 law); main / RCS nozzles radiate from
  their own pads, residual heat stays in their thermal mass.
- `ShipCore::m_propulsionThermal` = propulsion coolant store separate from the
  service coolant `m_thermal` (service reactor, auxiliaries, 1600 m^2 radiators).
- Accepted design (user 2026-10-26): two electrically independent groups
  PROPULSION and SHIP SERVICES; cross-link = emergency thermal coupling only,
  bounded by real hardware, gradients and sink headroom; no free / doubled heat.
  Installed since 2026-10-29: module `propulsion_emergency_heat_exchanger`
  (ER-5 surplus -> service coolant, bounded, energy exact).

### Universe
- 61 stellar systems at real ICRS positions (identity audit), deterministic
  generator `src/world/generation/` (UniverseSeed 0x454C495445303031; stellar /
  body / satellites v1, resources v1, interstellar v4), physical catalog under
  `galaxy_details/systems/<NNN>_<slug>/`, StarAtlas schema 2, Kepler orbits on
  the UniverseClock, `CelestialEphemerisService` (read-only states, not yet
  used by gameplay), gravity from catalog mu / radius / atmosphere radius.
- Navigation domain (`GalacticGrids.h`): 140-ly envelope + remote 20-ly balls
  and corridors (313 root cells, 1 component); grids 1 ly / 40.32 ly root /
  25 ly interstellar generation cell.
- Interstellar layer v4 (see the stage section): cavity / shell substrate,
  causal structures, irregular density lattices (`DensityLattice.h`), compact
  objects (rogues, brown dwarfs, remnants, comets, debris), ISM extraction
  profiles. Resource layers (NaturalResource / IndustrialSuitability /
  Habitability / FuelPotential), no prices. System Map presentation levels
  (Margot Pi). Physical vs political system ids validated
  (`check_system_id_namespaces.py`).

### Code layout (2026-10-29 decomposition, behaviour preserving)
- `SpaceState`: core lifecycle / update / render in `SpaceState.cpp`, plus
  `SpaceStateDocking.cpp` (docking + route-autopilot orchestration),
  `SpaceStateMaps.cpp`, `SpaceStateHud.cpp`, `SpaceStateDevPanel.cpp`.
- `ShipCore`: `ShipCore.cpp` (init, slots, services) + `Capability`,
  `Propulsion`, `PowerThermal`, `Status`, `Damage` units.
- `SystemMapRenderer`: + `Primitives`, `Navigation`, `Input`, `Background`.
- `GameSimulation`: + `NavigationLab` (DEV), `DevScenarios` (DEV),
  `Replication`, `Activation`, `ReferenceFrames`.
- Shared file-level helpers: private `*Internal.h`. Source contracts read a
  split class as one unit via `tests/source_units.py`.

## Verified results (evidence)
- Healthy real flights (nav-stress:simple): Newtonian I/Y/X, narrow I/X DONE,
  0 corridor violations (`af7897ee`); Assisted-I failed=0.
- Damage real flights (`9441ae9a`): main 50 %, 3/4 X mains, RCS nozzle 60 %,
  reactors off/on, mid-route hand-back + replan - all PASS, 0 violations.
  RCS gas tank (`138da7a3`): I/X DONE.
- Offline scenario matrix (21 scenarios x I/X x delay 1/3/6 = 126):
  0 violations healthy; 90/126 PASS by the strict criteria (micro-brakes etc.).
  Reference result: `matrix_pre5.tsv` equivalent = matrix of `3435b220`.
- Frame hitch (2026-10-29): periodic 85-115 ms frames every ~1.5 s were the
  thrust-reversal coast recompute at each checkpoint; fixed bit-identically
  (`b47d682b`), 0 in-flight frames > 80 ms. `core/HitchProfiler.h`
  (ELITE_TRACE_RUNTIME=1 only; one bool test otherwise).
- Decomposition (2026-10-29, `cf5f97ae`, `4e45618e`): matrix 126/126
  identical to `3435b220`; real Newtonian-I flight COMPLETE, failures 0,
  0 violations. SpaceState.cpp.obj 43.4k -> 19.4k COFF sections.

## Baseline reds (pre-existing, not caused by current work)
- client acceptance "first debounced Ctrl+F10 release emitted no flight-law
  command"; feature contract "Ctrl+F11 coordinate-format route disappeared";
  contracts universe_thermal_radiation, engine_unit_damage_model,
  universe_material_engine_thermal, installed_engine_actuator, local_flight_control,
  engine_six_axis_matrix (EngineThrustHudPresentation), physical_model_runtime_selection
  (wire schema 13), ship_capability_layer (client prediction), ship_control_execution_gateway
  (client attitude prediction); LocalPredictedPresentationContractTests does not
  compile; tools/check_map_architecture.py misses a deleted .inl.
- Re-measured 2026-11-03 against restore/pre-sol-network-2026-11-03: python
  contract reds ship_capability_layer, ship_control_execution_gateway,
  ship_physical_control_models, trajectory_generator, travel_frame_ownership,
  wire_data_schema, client_acceptance, constellation_overlay,
  debug_ui_compatibility, feature_contracts, interaction_activation are all
  red at the restore point too. tests/system_map/check_architecture.py was red
  (facade assigned Detail/Hub view cameras, commit 97b39d73) - FIXED in the
  fourth pass.

## Factual capability (current contract)
- Server `ShipCore::controlAuthorityTelemetry()` is the one published fact,
  wire schema 22: signed RCS / angular authority (24-nozzle allocator
  samples, gas share, ER-5 power share), LIVE forward main
  (`workingForwardAccelerationMps2` = installed x ER-5 power share), the
  Assisted working thrust axis + emergency flag + drive-motor index, and
  the combined two-axis RCS share (`rcsPairFeasibility01`).
- The client (`applyAuthorityToControlParams`, MotherComputer adapter) takes
  these values; it never re-derives the axis or main from replicated module
  health. ER-5 power uses ONE law (`ShipCore::engineReactorAvailableMW`) in
  the executor and in the capability.
- The client still rebuilds a capability from the float module snapshot
  (hybrid). Its threshold decisions are made bit-identical to the server:
  main-engine switch-off law `plant::mainEngineCutoff` (a main engine
  damaged > 94 % is OFF while its bank keeps an engine <= 94 %; otherwise
  the bank keeps its real thrust), evaluated on float-rounded damage on both
  sides and used by the server executor AND every capability projection;
  failure threshold tolerance 1e-6 (> float rounding; 0.95f read as a live
  5 % engine before). Assisted: execution speed profiles brake with the
  planner's concurrent-turn share (1/3); pilot catch-up braking stays full.
- Planned turns use min(0.5, combined RCS share) of the angular scalar.
  Capability losses reprofile / hand back; recoveries (+10 %, 0.5 s)
  reprofile up to the planned nominal (both laws stabilise and replan on a
  failed or invalidated route: 5 attempts >= 5 s apart).

## Map interaction contract
- ONE pointer contract (`game::presentation::PointerClickTracker`,
  `PointerInteractionSettings` 0.38 s / 12 px): hover never selects; a single
  click acts at once and is never undone; a double click on the same target
  adds its action; the cube trackers delegate to it.
- Galaxy Map spatial frame (stage 2026-10-30 c, see below): the user is
  always inside ONE current parent cube (overview = virtual cube of all
  roots, then G0 / G1 / G2 cells); its children are the working cubes.
  Double click = child becomes the parent; `[ ↑ ]` / breadcrumbs / LEVEL 0
  go up (the cube left stays selected); the wheel only zooms. The parent is
  fitted whole (sphere fit), its floor + far wall carry the coordinate
  grid; hovered / selected points project onto them.
- Galaxy Map pointer (stage 2026-10-30 b, see below): ONE resolver
  (`GalaxyPointerResolver`) over what the last frame PRESENTED (renderer
  publishes `presentedStars` with glyph radius and `presentedCubeMarkers`
  exactly as drawn; SystemMapRenderer passes overlay glyphs + card-under-
  cursor). Ranking: UI card > drawn controls (cursor inside a star glyph,
  foreground first / cube marker / overlay glyph; else nearest drawn edge
  within `glyphHitMarginPx` 4) > star proximity 32 px (nearest, 1 px ties
  -> foreground, then id) > cloud shell (ray; a shell the camera is inside
  is not a target) / icy field > empty. Gesture: press = PendingClick
  (camera never moves), > 5 px = drag (rotation), release before = click on
  the PRESS target; right press / wheel / zoom keys cancel a pending click.
  The waypoint glyph of the selected cube, as the second click of that
  cube's sequence, IS that cube (`cube_sequence_glyph`) -> refine. Hover of
  clouds uses the same resolver. A star drawn exactly on a cube marker wins.
- Galaxy Map clouds / icy fields: hover brightens; click selects (star
  selection dropped) and opens ONE info card with physical data, provenance
  and the derived ENVIRONMENTAL hazard (`world::interstellar::
  environmentalHazard`); risk for a ship "not computed" (seam
  `RouteRiskForShip`); star / cube / empty click drops it and the card;
  double click flies to the navigation cube containing the clicked point.
  Hover / selection / trackers / gesture are transient: reset on reset(),
  onEntered(), resetNavigationToEntry().
- System Map bodies (2026-10-31 b): single click selects (and its cube);
  double click APPROACHES (its cube becomes the parent, camera centred on
  it and following it); the info card comes from the context menu; a card
  never outlives its body's selection.
- Context menu (every map, 2026-10-31 b): right click (press + release
  without a drag) or Ctrl + left press on an object / navigation cube ->
  `MapContextMenu`: name + commands (INFO = card, the item's route / docking
  panel actions, SHOW DETAILS = the object's next map, VIEW SURROUNDINGS =
  enter the cube). It owns the pointer while open; any other press, Esc or
  the wheel closes it.
- Map buttons (every map): one row bottom right - back (previous map), up
  (one level up), TRACK (System), LEVEL 0 - rounded wire bars
  (`NavigationCoordinateOverlay::drawButtons`); an arrow exists only where
  its command does. Back restores the previous map with its saved view
  (`GalaxyMapBookmark` / `SystemMapBookmark` / Detail / Hub cameras).

## Previous stage (2026-10-29, 24 points) - condensed
- Decomposition (`cf5f97ae`, `4e45618e`, `7d62dfb3`), sizes before -> after:
  SpaceState.cpp 410889 -> 96073, GameSimulation.cpp 402830 -> 119556,
  SystemMapRenderer.cpp 156965 -> 15700, DedicatedClientRouteAutopilot.h
  155185 -> 86035, ShipCore.cpp 153339 -> 23514; SpaceState.cpp.obj 43.4k ->
  19.4k COFF sections (`-mbig-obj` kept as a safety net only).
- Galaxy Map medium proxies (`0a49b731`): 59 -> 0.08 ms medium draw.
- Interstellar v4 (`bbaf7b9e`): causal clouds, no Sol privilege, provenance,
  icy fields, `InterstellarNavigationEnvironment` (see Universe).
- CPU: docking corridor cache 7.5 -> 2.3 ms (`7d62dfb3`); pilot steps on
  authoritative time (`d41d8284`); RelWithDebInfo perf path
  (`tools/perf/build_relwithdebinfo.sh`): flight frame median 9.5 ms.
- Emergency heat exchanger PROPULSION -> SERVICES (`a5314531`, `71c60818`):
  60 MW rating, engage 1950 K / release 1700 K, energy exact
  (`EliteShipThermalCheck` 9/9). Main / RCS nozzles conduct no heat into
  `m_propulsionThermal` (own pads): that store is inert; the exchanger
  relieves ER-5 surplus (pump-power limited ~5 MW in the tested state).
- Capability: ER-5 power, RCS x/y/z node projection (`71c60818`).

## Current stage (user order 2026-10-30): close the failed damage gates, residual capability, map interaction
### Root causes fixed
- A1 N1 ER-5 70 % (`afcd2f6e`): offline replay reproduced the game failure
  (3 violations, 1034 m). First wrong layer: the server published main
  11.37 m/s2 (= physics), but `applyAuthorityToControlParams` applied only
  the RCS bounds for Newtonian and kept the main from the installed
  capability cached per equipment revision (ER-5 damage does not change
  it) -> watch / profile / compiler / pilot kept 36.77. Now main = min(rating,
  live published). Replay delay 3 / 6: reprofiled 36.77 -> 11.37, 0
  violations, terminal reached.
- A2 A3 engine L lost (`a54b8f5d`): the client rebuilt a second axis from
  replicated module health (nose) while the server executed the 49.5 deg
  offset axis ("working thrust axis mismatch"); with the axis fixed the
  doctrine still rejected the emergency axis because the drive-motor index
  was the client's -1, which then read as "telemetry lost". Fix: the axis,
  emergency flag and motor index come from the server telemetry; wire
  schema had never carried `workingForwardAccelerationMps2` (local sessions
  copy structs) - now serialised; an Assisted capability loss stabilises
  and replans instead of failing.
- B1 X one main lost -> 10 % forward: EXPLAINED, kept. The projection
  balances every surviving aft main down to the weakest (95 %-damaged main
  ~10 % -> 4 x 10 %). An experiment projecting the gimbal-trimmed sum
  (~57-73 m/s2) let the planner fly fast while the asymmetry torque fell on
  the RCS: angular authority collapsed 1.90 -> 0.87 by nozzle heating and the
  flight had 6 violations (542 m). Reverted; the conservative projection is
  what the plant can sustain.
- B2 coupled RCS (`3975ff6b`): two axes at their single-axis limits deliver
  ~50 % each (healthy 0.502, one nozzle lost 0.486): published share,
  planned turns use min(0.5, share) - healthy plans unchanged.
- B3 recovery (`3975ff6b`): replay main 50 % at 60 s, restored at 120 s:
  main 18.39 -> 36.77 recovered-reprofiled, settled 232.7 s (no repair
  260.9 s), 0 violations.
- Maps (`ab9018f6`): see Map interaction contract. Verified: EliteMapPointerCheck
  7/7 (E1-E4); in game (DEV pick test, own-framebuffer capture): G cloud
  picked in 1.4 ms, card with all fields, double-click focus cube; Galaxy
  Map open: medium 0.5-0.6 ms, frame ~20 ms, 0 frames > 50 ms.
  Selected outline toned down after the first capture (alpha 0.10).
- Offline matrix after every step: 90/126 PASS, 0 violations (identical to
  the post-A1 state for B2 / B3).

- A4 Assisted damage stack (user order 2026-10-30, second half):
  - Root cause of the two thrust pictures: same capability function,
    different inputs - server double damage 0.95 >= failure 0.95 (engine
    failed, one-motor axis 49.5 deg, 36.77 m/s2), client 0.95f = 0.94999999
    (engine alive 5 %, balanced bank, nose axis, 5.15 m/s2) -> "axis
    mismatch"; plus min(client, server) forward would have cut the plan 7x.
    Remaining divergent inputs (not yet seen in flight): client applies no
    current mass / CoM, part of the engine operating state is not in the
    snapshot. Analysis: D:/tmp/elite_client_server_thrust_analysis.md.
  - Reverting the server axis (done earlier the same day on a mistaken
    reading of the user's "return Assisted") was wrong: restored.
  - Sway after a mid-route loss came from the hot-start replan at ~243 m/s,
    not from the axis: Assisted now brakes to rest after a capability loss,
    ALIGNs the emergency working axis onto the course flown (skipped without
    turning authority), plans from rest with the working axis as first
    course; infeasible Assisted plans retry slower (0.75 .. 0.20 cruise).
  - Execution reference braked at (reverse - reserve) x 0.9 x 0.85 = 55
    m/s2 while the plan assumed 21.8 (A3 L 93 %): late braking into
    curves. One braking law now (see Factual capability).
  - A general cruise cap sqrt(2 a D) was NOT added: the planner already
    caps speed by curvature (lateral + hull rate), angular acceleration and
    a backward braking pass; with D = 955 m it would cut healthy A3 400 ->
    184 m/s. Note: `tunnel_radius_floor_m` 955 is the minimum TURN radius,
    not the tunnel width.
  - Offline: EliteNavigationCapabilityCheck PASS (new: client == server
    engine decision / axis / forward at L 0, 0.93, 0.945, 0.95); scenario
    matrix mx7 90/126 PASS, 0 violations (unchanged).

### Acceptance A4 (real game, acc5..acc7, Debug; COMPLETE = corridor scenario, not docking)
| flight | result | max cross-track |
|---|---|---|
| A3 healthy | COMPLETE | 27.7 m (acc6 25.9) |
| A3 L 95 % before start | ALIGN 49.5 deg / 5 s, COMPLETE | 73 m (acc6 66) |
| A3 L 95 % at 60 s | stop, ALIGN 3.5 s, replan, COMPLETE | 82 m (acc6 77; was 115-155 sway) |
| A3 reactor 60 % at 60 s | flies (6 m on straights); harness FAILED: BLUE not reached in 180 s | 52 m |
| A3 L 93 % before start (below cutoff) | harness FAILED (BLUE 180 s) | 180 m (acc5 288): mixer limited by the 7 % engine, delivered braking 4-37 m/s2 |
| A3 L + fore R 95 % | honest refusal: no attitude control (0.0007 rad/s2) | - |

### Acceptance (real game, nav-stress:simple, Debug, acceptance.sh run acc2)
Runs acc2 (all) + acc3 (Assisted after `0287c7ea`); damage at t_moving 60 s;
hitches = frames > 80 ms after start.
| flight | result | violations | DONE (pilot s) | notes |
|---|---|---|---|---|
| N1 healthy | COMPLETE | 0 (19.1 m) | 234.9 | 0 hitches |
| Newtonian-X healthy | COMPLETE | 0 | 187.6 | 0 hitches |
| A3 healthy | COMPLETE | - (27.3 m) | 200-s window | 0 hitches |
| N1 / X narrow fixture | DONE; harness FAILED only by its 30-s BLUE transit limit | 0 | 291.7 / 241.3 | 1 single 80.5 ms recompile frame (I) |
| N1 main 50 % | reprofiled | 0 | 275.6 | |
| N1 ER-5 70 % | reprofiled 36.77 -> 11.37 | 0 (28.2 m) | not in window (slow) | was 3 violations / 1034 m |
| N1 RCS y-nozzle 60 % | reprofiled | 0 | 232.8 | |
| N1 reactors off / on | coast, replan | 0 | 217.9 | |
| N1 gas tank 60 % | COMPLETE | 0 | 233.0 | |
| N1 main 50 % + RCS 70 % | reprofiled | 0 | 271.5 | |
| X one main lost | hand-back, replan at 7.35 m/s2; harness BLUE 180-s timeout | 0 | - | 14 frames 110-130 ms (autopilot recompute on the slow profile) |
| A3 engine L lost | recover + replan on the server axis, COMPLETE | 291 m max on the new plan (inside the 250-500 m corridor) | - | |
| A3 reactor 60 % | loss detected, recover; Assisted optimizer finds NO feasible path (12 replans), ship held safe | 25.5 m | - | FAILED (harness timeout) - OPEN |
| A3 aft L + fore R lost | honest refusal "no attitude control" after 5 attempts, held safe | 25.4 m | - | controlled refusal |

### Galaxy Map pointer regression (user order 2026-10-30 b) - AUTOMATED PASS + scripted GAME PASS
- User report: stars / cubes picked "every other time", a star under the
  cursor gave a cube or nothing, cube double click unpredictable.
- Root causes (source + trace verified):
  R1 (old, `3975ff6b` and earlier) cube click tested only anchor + hovered
     cell centres in an invisible 18 px circle; hovered cell = cursor ray x
     plane through camera.target (not the drawn cube); the cube had first
     refusal, so the 18 px circle (marker drawn 4-5 px) hid stars (32 px).
  R2 (old) left press set camera.rotating at once: 1-4 px jitter rotated
     the camera and re-synced the anchor cell before the release hit test.
  R3 (old) waypoint-candidate glyph of the selected cube (cube +20,-2 px,
     12 px) overlapped the 18 px cube circle; the overlay took the second
     click -> no double click / refine.
  R4 (old) star pick 2D distance only (no depth tie-break).
  R5 (`ab9018f6`, the commit that made it user-visible) the cloud picker was
     added as the fallback of every click that missed R1/R4 circles; the
     camera sits INSIDE the Local Interstellar Cloud, so the ray hit its far
     wall and EVERY such click selected the LIC (star selection dropped, a
     card opened to the right of the click, covering the next targets). The
     same hit drove cloud hover. Cloud hover / selection / tracker were not
     reset on reset / re-entry; the DEV pick test kept static state in the
     production input path (inert without its env var).
- Changes: `GalaxyPointerResolver` (one resolver + `GalaxyPointerGesture`);
  renderer publishes presented stars / cube markers (old unused
  `screenPoints` removed); `GalaxyMapInteraction` resolves once per press,
  dispatches one target (`applyClick` / `applyCubeClick` with the old cube
  semantics unchanged / `applyMediumClick`); `pickNavigationCell`,
  `pickSystem`, `pickMedium`, `navigationCellPickRadiusPx` (Galaxy) and
  `ELITE_DEV_GALAXY_PICK_TEST` deleted; `PointerClickTracker::wouldComplete`;
  `MapObjectOverlayState::panelUnderCursor` + `allowItemPick` (default true:
  System Map unchanged); `GalaxyMapView::resetTransientPointerState`.
  DEV: `ELITE_GALAXY_PICK_TRACE=1` -> D:/tmp/elite_galaxy_pick_trace.csv (one
  row per sequence: press/release, movement, camera changed, candidates,
  winner, reason, single/double, selection after); `ELITE_DEV_GALAXY_POINTER_
  SCRIPT=1` scripted pointer through the production input path
  (`tools/perf/galaxy_pointer_script.sh`, PHASES=... subset).
- AUTOMATED: `EliteGalaxyPointerCheck` 29/29 PASS (G1 star vs cube, G2
  jitter, G3 drag, G4 cube double click incl. waypoint-glyph second click
  10/10, G5 depth, G6/G7 cloud behind star / cube + camera inside a cloud,
  G8 re-entry); mutation (absorption disabled) -> G4 FAIL as expected.
  `EliteMapPointerCheck` PASS, 0 failures (System Map card contract unchanged).
- SCRIPTED GAME (real game, Debug, nav-stress:simple, Galaxy Map; synthetic
  pointer, real presented geometry, production input path - NOT a human
  mouse): Sol 10/10, Alpha Centauri (Proxima, nearest) 10/10, close stars
  (15-21 px pair) 10/10, cube single 10/10, star after cube 10/10, cube
  after star 10/10, cube double click marker 5/5 + glyph area 5/5, cloud
  10/10, star after cloud 10/10 (cloud dropped 10/10), drag 5/5 (never
  selects); 100 click rows camera_changed=no. Earlier script runs failed on
  script defects (localised names, off-viewport second click, off-screen
  star) - fixed in the script, not in production.
- PERFORMANCE (same run): medium draw avg 0.53 ms (baseline 0.5-0.6), frame
  avg 20.9 ms, max 49.9, 0 frames > 50 ms (baseline ~20 ms).

### Galaxy Map spatial frame (user order 2026-10-30 c) - AUTOMATED PASS + scripted GAME PASS
- Order: restore and develop the nested-cube model (not a flat map, no
  reference plane): the user is always INSIDE one current parent cube; 25 per
  axis, addresses, topology and generation unchanged.
- OLD BEHAVIOUR / REGRESSION (source + git verified):
  * up to `4734bd67` the top level showed the 5 whitelisted root cubes as
    the visible boundary of the game space; `4734bd67` (connected domain,
    ~275 roots) drew only the focused root + neighbours: the top-level
    overview was lost;
  * the camera range was clamped to `labelMaxCameraDistance` 155 render
    units (~70 ly, checkpoint `c0a385b7`): a 40.3-ly root needs ~200 to fit,
    so upper cubes could never be framed (edges outside the viewport);
  * the working level was implicit: anchor = cell under camera.target,
    re-synced on every rotate / pan / zoom (`syncNavigationAnchorToCameraTarget`),
    and the wheel switched levels (`cubicNavigationWheelAction`) - the
    "parent" drifted with the camera; hover = ray x invisible plane through
    camera.target; current-level cells hidden below 56 px.
- RESTORED MODEL (`GalaxyHierarchy.h/.cpp`): explicit `hierarchyParent`
  (level -1 = virtual galaxy overview: a cube on the root lattice holding
  every navigable root, NOT a navigation cell; its children are the roots;
  levels 0..2 = real cells, children = level + 1). The grid works on the
  child level (Galaxy minimumLevel 0 so roots are children; config loader
  allowed `std::max(0, ..)`). It changes only by command: double click
  down, up button, breadcrumb, LEVEL 0, object double click; never by the
  camera or the wheel.
- CURRENT PARENT CUBE: camera fit = circumscribed sphere of the parent in the
  narrower FOV, margin 1.06 (`galaxyHierarchyFitDistance`): whole cube from
  any orbit angle. Wheel range = [0.035, 1.8] x fit of the parent. Far plane
  follows the distance; the pointer ray is built from the camera basis (an
  overview mvp's near/far ratio is beyond float precision - un-projecting
  its far plane gave no ray). Nested flights: 0.95 s, target coupled to the
  log scale (the entered cube moves monotonically to the centre, never
  leaves the view), the old parent fades, the new grids fade in; a press
  during such a flight finishes it.
- COORDINATE FACES: the floor (ceiling when below) and the far wall across
  X or Z the view looks into, chosen with hysteresis (10 % edge dead band,
  0.15 axis margin); major lines every 5 children (all lines for the
  overview's root lattice), minor faint; labels at major children (offsets
  -10..10 from the centre child; root indices in the overview) along the
  floor-wall junction, the floor side edge and the far vertical edge; X / Y /
  Z axis names; XYZ triad top-right.
- PROJECTIONS: hovered and selected point (star / cloud hit = exact point,
  cube = centre): guide lines to floor and wall, rulers to the labelled
  edges, the child's footprint (one grid cell) on both faces, a cross at
  the exact feet; "x y z" child coordinates next to the cube (G0 [..] in
  the overview). Not drawn for other objects.
- HIERARCHY NAVIGATION: single click selects a child (parent unchanged);
  double click = the child becomes the parent (terminal G3 child: System
  Map, unchanged); up `[ ↑ ]` left of LEVEL 0: parent of the parent, the
  cube left stays selected; breadcrumbs `ГАЛАКТИКА › G0 [x,y,z] › G1 ..`
  above the footer (click = jump to that ancestor); LEVEL 0 = overview with
  the player's root selected; the map opens on the overview.
- OBJECT LOD: compact interstellar objects only from the parent level where
  they matter and in / near the parent (brown dwarfs, remnants from a root
  parent; rogue bodies, comets, icy fields from a G1 parent); medium
  structures below 1.5 px not drawn (overview: below 6 px - no blobs; the
  overview framed ~1700 blobs = 6 ms/frame); selected / hovered always.
  Picking LOD: star proximity ring = clamp(0.6 child px, 8, 32) (glyph only
  in the overview), icy fields likewise, clouds below 4 px are not targets.
  Star halos of the selected / hovered cube only below the overview.
- POINTER INTERACTION: the ONE resolver gained tier 5: empty space inside
  the parent = the child where the cursor ray meets the hover layer (the
  child layer perpendicular to the axis most aligned with the view, through
  the selected child, else the selected object, else the centre; hysteresis
  0.10), drawn as the parent's cross-section while hovered. Hover = the
  resolver's winner + the child holding its semantic point (WYSIWYG: the
  highlighted cube IS the press target). Only the SELECTED child's marker
  is a pointer target, and only for a cube selection (a hover marker or the
  cube of a selected star / cloud never takes the object's click). Star:
  click selects star + its cube; double click = one level down into the
  star's cube (System Map from the terminal level). Cloud: click selects +
  card (unchanged); double click = into the cube of the clicked point,
  cloud kept. Alt + click: "What is here?" list of the resolver candidates
  (star / cloud / cube, de-duplicated), drawn above the cards, owns the
  pointer while open; a row = a normal single click. Fixed on the way: the
  selected cloud outline began its own line batch and erased the frame
  drawn before it.
- AUTOMATED: `EliteGalaxyPointerCheck` 41/41 PASS (G1-G8 rewritten for the
  child-cube model + H1 overview / fit at 48 angle x level cases, worst
  corner margin 34 px; H2 hover == press target 30/30; H3 wheel; H4 enter /
  up / breadcrumb; H5 star descent + terminal System Map; H6 cloud vs hover
  marker; H7 face hysteresis); mutations (no cloud descent, no face dead
  band) -> H6 / H7 FAIL as expected. `EliteMapPointerCheck` PASS (System
  Map unchanged). Run from the repo root (the check copy in
  build/headless_server/assets is stale).
- SCRIPTED GAME (real game, Debug, nav-stress:simple; synthetic pointer
  through the production input path, real presented geometry - NOT a
  human mouse): `ELITE_DEV_GALAXY_HIERARCHY_SCRIPT=1`
  (`tools/perf/galaxy_map_capture.sh "" 300`) 78/78 PASS: overview fits;
  branches A (overview -> G0 -> G1 -> G2), C, D through empty space (hover
  x3 with camera still, click, double click, fit after every entry); up x2
  by the button and breadcrumb GALAXY (cube left selected); star branch
  (click, 3 double clicks down to G2, star kept); cloud (click, double click
  into its cube, up); Alt list (star / cloud / 2 cubes, row selects);
  wheel; LEVEL 0; breadcrumb G0. Frames: D:/tmp/elite_gh_NN_*.png.
  Pointer regression script (`galaxy_pointer_script.sh`, phases adapted: 0/1
  in the entry G1 cube, 2/3 on empty-space cubes): Sol 10/10,
  Alpha Centauri / Proxima 10/10, close stars 10/10, cube single 10/10,
  star after cube 10/10, cube after star 10/10, cube double click same
  point 5/5, glyph second click 3/5 in the full run (the script's second
  click landed 23 px from the first - not a double click by contract;
  script fixed to place the first click where the glyph appears) -> phase 3
  rerun 5/5 + 5/5, cloud 10/10, star after cloud 10/10, drag 5/5.
- PERFORMANCE (Debug, same runs): medium draw window avg 0.86-0.95 ms (worst 2-second
  window 2.1 ms = the galaxy overview: 179 shells, no blobs; before the
  overview LOD it was 6.1 ms with 1721 blobs); hover resolve avg 0.20-0.25
  ms, max 0.84 ms (only when cursor / camera / frame change); frame avg
  15.5-15.7 ms (previous stage 20.9). Frames > 50 ms: the hierarchy run's
  36 include its 33 DEV frame captures (PNG write ~110 ms); the pointer run
  had 7 (startup, map open, one 101 ms docking-autopilot frame of the
  scenario flight), none from the map.

### Galaxy Map overview framing + neighbour cubes (user follow-up 2026-10-30 d)
- User: the top level was "moved too far" - the overview framed the whole
  navigation domain (275 roots, 282 ly) while the star systems lie within
  ~97 ly (most within 25 ly): they shrank to a dot. And the cubes next to
  the current parent could not be chosen.
- Overview now frames the roots holding star systems + the player's root
  (`overviewRoots`, `galaxyHierarchyBox(.., overviewRoots)`): 5 roots, 201 ly
  in the current catalogue (HD 102195 96.7 ly / HD 93083 90.9 ly set the
  extent). Empty navigable roots outside it stay navigable (a point there
  opens its own root as the parent - `focusHierarchyPoint`).
- Neighbours (REMOVED by the order of 2026-10-31, see below): the up-to-26 navigable cubes of the parent's level around it
  are pointer targets (resolver tier 5b `neighbour_cube`: ray x neighbour
  boxes when the ray leaves the parent), drawn as faint ghosts, hovered /
  selected like a child (address label, no projections); double click
  moves the frame sideways into the neighbour (level unchanged, same nested
  flight); up / breadcrumbs unchanged.
- AUTOMATED: `EliteGalaxyPointerCheck` 44/44 (H1 now: overview frames the
  star-system roots, 40 ly in the one-star test vs 282 ly domain; H8 new:
  neighbour hover, sideways double click, up). SCRIPTED GAME 84/84
  (+ 2 neighbour hover / sideways / fit). Medium draw avg 1.2 ms, worst
  window 2.3 ms. Human mouse acceptance still open.
- Incident: during this follow-up a manual (non-script) launch for a
  screenshot ignored a running user game and the cleanup killed the user's
  EliteGame. Rule kept: never start or stop a game while one is running;
  only the guarded scripts launch the game.

### One cube navigation for Galaxy + System maps (user order 2026-10-31)
- User: the top cube must fill the screen and never get smaller; the wheel
  must move between levels (a child filling the screen becomes the parent,
  a parent smaller than the screen hands over up); nothing outside the
  current parent drawn or interactive (clouds outside the giga-cube); the
  SAME cube mouse control on the System Map; planet: single click selects
  it and its cube (the selection follows the planet between cubes), double
  click opens its info card.
- Fit: the parent "on the whole screen" = its projected corners reach 0.98
  of the viewport in the limiting direction (bisection on the camera
  distance, current orbit angle; `GalaxyMapView::fillDistanceAt`,
  `SystemMapView::fillHalfHeightAt`); the hand-over size test uses the
  projected extent (perspective makes the cube asymmetric about the centre).
- Wheel: zoom IN scales the camera rig about the point under the cursor
  (the cube there grows); the child under the cursor (else at the view
  centre) with extent >= 0.98 becomes the parent (no flight); a terminal
  Galaxy cube filling the screen opens the System Map (its star / sector).
  Zoom OUT drifts the view to the parent centre; a parent below 0.94
  hands over to its parent (the cube left selected). Overview / S0 root:
  the distance range never lets it get smaller than the screen.
- Outside the parent: stars, labels, compact objects, the player marker,
  medium structures (drawn and picked only when their sphere reaches into
  the parent, a shell hit must lie inside it) on the Galaxy Map; bodies on
  the System Map (`filterBodiesByPresentationLevel`). The neighbour-cube
  targets of 2026-10-30 d were removed (they lie outside the parent; a
  neighbour is reached by zooming out one level and in).
- Shared code: `GalaxyHierarchy` works on any cubic grid
  (`cubicHierarchyBox`, ascend / chain with a top level: Galaxy -1, System
  0); `HierarchyFramePainter` (frame, inner faces, labels, projections,
  child cubes, hover layer, texts + triad, hover fade) draws BOTH maps.
- System Map (`SystemMapInteraction` rewritten): explicit `hierarchyParent`
  (S0 root on open / LEVEL 0), `GalaxyPointerGesture` (press never moves
  the camera), one resolver hub > body > selected-cube marker > hover-layer
  child (bodies / hubs outside the parent are no targets), hover = winner +
  its child, cube click selects / double click enters, body click selects
  + its cube, body double click toggles the card, the body's cube follows
  the body every frame, `[ ↑ ]` left of TRACK, breadcrumbs `СИСТЕМА › S1
  [..]`. Old anchor-follows-cursor / wheel-level / invisible cube circle /
  rotate-on-press code removed.
- AUTOMATED: `EliteGalaxyPointerCheck` PASS (H1 fill at 48 angle x level
  cases, min corner margin 7 px; H3 wheel: 2 levels in at ratio >= 0.98,
  3 levels out with the cube left selected, overview min ratio 0.98);
  `EliteMapPointerCheck` PASS. SCRIPTED GAME: Galaxy 79/79
  (`ELITE_DEV_GALAXY_HIERARCHY_SCRIPT`), System 21/21
  (`ELITE_DEV_SYSTEM_HIERARCHY_SCRIPT` + `ELITE_DEV_SYSTEM_MAP_AT`,
  `tools/perf/galaxy_map_capture.sh "" 240`): S0 fills, cube hover / click /
  double click x2, up x2 (button), Uranus click (selected + cube, no card),
  double click (card), its cube followed for 4.5 s, wheel in one hand-over,
  wheel out to S0. Frames D:/tmp/elite_gh_*.png, elite_sh_*.png.
- PERFORMANCE (Debug): Galaxy medium avg 0.33 ms (worst window 0.43), frame
  avg 12.2 ms; System frame avg 16.0 ms; frames > 50 ms only at startup and
  the DEV frame captures.
- OPEN: human mouse acceptance of both maps; the old Galaxy pointer script
  (`galaxy_pointer_script.sh`) was not re-run after this order.

### Map camera focus, context menu, back arrow, map buttons (user order 2026-10-31 b)
- User: System Map navigation "chaotic" - zooming toward a planet the
  parent-cube rule kept pulling the view to the cube centre, the planet
  and its moons were hard to keep in view; rotations at high zoom sharp and
  about an unclear point. Wanted: rotation about the object near the mouse,
  else about the pressed point; System double click = approach (as on the
  Galaxy Map); info via Ctrl + click / right click menu (name + commands:
  info, route, details) - also for navigation cubes (info, route,
  surroundings); a back arrow on Details (and the other maps) returning to
  the remembered map position; buttons as rounded transparent wire bars
  (text or arrow), no arrow where nothing is beyond.
- Root causes: zoom-out mixed the target 18 % toward the parent centre per
  notch; the zoom / rotation pivot fell back to the HOVERED CUBE CENTRE; a
  cube press rotated about the cube centre; the hand-over measured the
  cube's on-screen extent at its real position, so it depended on where
  the user looked (and forced the centre drift); the moving planet left
  the view (no follow), and once outside the parent it was hidden.
- Camera (both maps): the wheel zooms in AND out about the object near the
  cursor (`pivotPickRadiusPx` 36, disk distance), else the point under the
  cursor (hover layer / target plane) - never a cube centre; only the top
  cube (overview / S0) at >= 0.7 of its distance limit centres itself
  (0.35 per notch) so it fills the screen; its limit = the centred fit.
  Hand-over = `cubeSizeRatio` (the cube's size at the current zoom as if
  centred on the camera target): down when the child under the cursor
  reaches 0.98, up below 0.94. Rotation: about the object near the press
  point (System: body / hub via `nearestSystemObject`; Galaxy: star), else
  the pressed point (cloud hit / hover layer / target plane).
- Focus object (System): a body the user zoomed / turned about or
  double-clicked is FOLLOWED (`cameraFollowBodyId`: camera target, a
  running flight and the orbit pivot move with it, like TRACK); when it
  leaves the parent, the parent moves sideways to its cube
  (`shiftHierarchyParentTo`); a pan or framing a cube ends the follow.
- Double click on an object = approach (both maps): its child cube becomes
  the parent without framing, the camera flies to the object itself at the
  parent's fill size (`approachHierarchyPoint`); a terminal System cube:
  just closer (0.35 x). Cube double click still frames the cube.
- Context menu: `MapContextMenu` (data) + `SystemMapRendererContextMenu.cpp`
  (open / execute / pointer / draw) + `NavigationCoordinateOverlay::
  drawContextMenu`. Targets: overlay glyphs (every map), Galaxy star /
  cloud / cube (the resolver), System body / hub / cube
  (`SystemMapInteraction::resolvePointer`), Hub infrastructure module. The
  target is selected on open (single-click semantics, no click tracking).
  Galaxy star INFO = a star card (`galaxy-system:<id>`, route target = the
  system position, closes with the star selection); SHOW DETAILS: star ->
  System Map, body -> Details, hub -> Hub map, terminal System cube ->
  Details; VIEW SURROUNDINGS: cube -> entered (terminal Galaxy cube -> the
  System Map), cloud -> its cube approached. Labels localised
  (`map.context.*`).
- Back history: `SystemMapRenderer::setMode` records the map left
  (`MapHistoryEntry`, 24 max) unless the change is the back navigation; the
  back arrow emits `MapIntent::RestoreMap` -> `SpaceState::
  restorePreviousSystemMapMode` (Galaxy / loaded or known System / loaded
  Details / loaded Hub); the saved view is applied when the map is shown
  (Galaxy after `onGalaxyMapEntered`, System in the presentation builder
  after the fit, Details / Hub in `setMode`). Found by the scripted run: a
  press held across the map change hit LEVEL 0 of the next map (same
  screen slot) - a press that starts before a map change now belongs to
  the old map until released.
- Buttons: `NavigationOverlayButton` rows drawn with `HudPrimitiveBatch`
  (rounded wire outline, faint fill on hover / active), raised 34 px above
  the bottom (the game's hint bar covered them). `SystemMapView::
  suppressCameraGesture` now also suppresses the System pointer gesture.
- AUTOMATED: `EliteGalaxyPointerCheck` PASS 44/44 (H3 now measures the
  size ratio); `EliteMapPointerCheck` PASS. SCRIPTED GAME (Debug):
  System 29/29 - S0 fill, cube hover / click / double click x2, up x2,
  Uranus click (no card), right click menu (name, INFO, DETAILS) -> INFO
  opens the card, cube follows 4.5 s, double click approaches (deeper
  parent, centred 0 px, followed), still centred 3 s later, rotation drag
  near it (moved 0.01 px), wheel on it (stays under the cursor), menu
  DETAILS -> Details with a back arrow -> back: same parent and camera,
  wheel in / out to S0. Galaxy 83/83 - incl. star / cloud double click =
  approach (centred, parent size 0.90), star right click menu (name, INFO,
  route, DETAILS) -> INFO opens the star card, cube right click ->
  SURROUNDINGS enters it, wheel in / out, overview fills.
  OLD Galaxy pointer script (`galaxy_pointer_script.sh`, first re-run
  since 2026-10-30 c): first run 8 PASS / 3 FAIL - all three stale test
  expectations, not product faults: "2 Alpha Centauri" 0/10 (Proxima lies
  outside the Sol G1 parent -> by the 2026-10-31 rule not drawn / no
  target: now skipped when outside the parent), "9 cloud single click"
  0/10 (the script searched clouds without the parent clip; with the
  production scene no cloud reaches into that parent: now skipped - cloud
  click is covered by the hierarchy script), "8 cube double click (same
  point)" 4/5 (trace: the probe point lay on a cube border, the +1 px
  second click hit the neighbour cube - correct; the probe now avoids
  borders). Re-run: all rows PASS (phase 3 re-run 10/10).
- OPEN: human mouse acceptance of both maps; Detail / Hub maps: the menu
  covers overlay glyphs and Hub modules only (no body picking there).

### Procedural planet / asteroid / cloud surfaces + Planet Lab (user order 2026-10-31 c)
- User: deterministic procedural visual generation of planets, rocky surfaces,
  asteroids, cloud layers, climate-driven appearance, LOD from map to
  landing; maximum reproducibility (same identity -> same continents,
  ranges, craters, lakes; LOD adds detail, never re-generates); appearance
  derived from the generated physical descriptor; curated real data kept as
  a provider; deterministic weather; detail budget by reachability; a
  visual demo; a reproducibility regression check; plus "Earth not found on
  the Solar System map".
- Found in the old system (audit): CelestialTextureBaker is offline-only
  (`EliteGame --bake-celestial-textures`), seeds are literal JSON values,
  equirect global maps + a tile pyramid nobody reads; every Sol body is
  ImportedRealData; the runtime draws one equirect texture per body (no
  geometric LOD); CPU weather / cloud generators unused; map clouds got a
  clock-based random seed per map opening (`random_on_*`). No C++ reads the
  descriptor's terrain_profile / surface_materials / weather_potential /
  orbital_access_index.
- NEW `src/world/celestial/surface` (library EliteCelestialSurface, -O2,
  no GL; contract `PLANET_GENERATOR_ALGORITHM.md`): coordinate-addressed
  seeds (GeneratorVersion 1, one tag per layer), exponential-gradient noise
  with macro / LOD-detail band split, PlanetSurfaceField (continents with
  descriptor ocean coverage, range belts, uplands, damped hills, crater
  classes, volcanoes, lakes, ice caps, boulders, climate, biomes,
  materials from surface_materials), GasGiantField (bands, eddies, storms;
  no ground), SmallBodyField (ellipsoid + fracture planes + crater list +
  lattice craters), CloudWeatherField (layers from pressure, f(seed, world
  time)), cube-sphere patches with skirts, threaded streamer, reachability
  policy (ground 0.25 m / overflight 250 m / cloud deck).
- Reused / unified: GenerationRandom deriveSeed; generated body + reservoir
  JSON; the baker gets source kind ProceduralFromBodyDescriptor (equirect
  maps sampled from the same field -> existing normal / material / LOD set;
  verified by a scratch bake 1024x512 of the Earth descriptor); map clouds:
  identity seed only (random_on_* -> world_time, 49 asset JSONs migrated,
  clock seed removed); motion was already universe time.
- System Map Earth: root cause - the hierarchy filter hid a body whose
  CENTRE was outside the current parent cube (Earth leaves a small parent in
  minutes, its hub / ships stayed drawn), and a followed / selected body was
  dropped from the frame so the follow could not move the parent. Fix: a
  body stays when its sphere reaches the cube, when it is selected or
  followed, or when an object (hub, ship) inside the cube orbits it.
- Planet Lab `tools/planet_lab` (ElitePlanetLab): viewer (bodies 1..6:
  earth_like = Sol Earth descriptor through the procedural provider, rocky =
  Mercury, asteroid = Sol belt candidate, gas_giant = Jupiter, dry = Mars,
  hostile = Alpha Centauri A i 180 bar; F1..F5 far orbit / low orbit /
  regional / approach / ground; L LOD cap; C clouds; W wireframe; T / Space
  weather time; title + stdout telemetry), camera-relative + log-depth
  rendering, screen-space-error quadtree (5 px), per-pixel coastline from
  the macro field, seamless 1024-m periodic micro detail, `--capture DIR`
  (PNG per body / viewpoint + LOD-cap 3 / 6 pairs).
- CHECKS (`ElitePlanetLab --check`, all PASS): restart reproducibility (86
  checksums recorded, verified by a separate process: 0 differ); patch
  order / threads identical; parent/child (water, landmark id, macro height
  identical at coincident vertices; height diff <= detail bound); coastline
  (20000 directions x 6 band limits 50 km .. 0.25 m: 0 water / landmark /
  macro changes); seams (same face < 1 mm, across faces < 1 cm, T-junction
  gap <= 0.03 skirt); Earth climate (polar ice 1.0 vs tropics 0, desert
  peak 20-30 deg, 7-8 biomes along a latitude); clouds same time -> same
  sky, 6 h -> change; policy (gas giant no ground, 180-bar no ground);
  asteroid silhouette LOD-independent. Perf (one thread, -O2): 15-19 us /
  ground sample, 15-17 ms / patch; streaming 35-45 ms / patch on 14 workers.
- GAME (Debug): EliteGame builds; System Map script 30/30 incl. "all 8
  major planets presented" (Earth); EliteGalaxyPointerCheck PASS,
  EliteMapPointerCheck PASS.
- Not done / OPEN (superseded by 2026-10-31 d: flight renderer done, the
  patch streamer is NOT the production path): the game's map renderers
  still draw equirect textures; no
  procedural body is configured in the game data (baker path verified on a
  scratch root only); human visual review of the Lab pending. Journal of
  decisions, doubts and proposals: D:/tmp/PROCEDURAL_PLANET_NOTES.md (e.g.
  every generated temperate planet has ocean_coverage 1.0; no biosphere fact
  in the descriptor).

### Celestial runtime renderer: continuous field, local terrain, 3 styles (user order 2026-10-31 d)
- User: drop the patch / quadtree / pyramid as the production model of a
  planet; a planet = one continuous procedural field; LOD only changes how
  precisely the same world is asked; a correct runtime renderer of all
  bodies (positions, sizes, angular sizes, orientation, spin, motion, fly
  from body to body, LOD by distance, streaming); 3 styles (Elite /
  Modern / Anime) with a user setting; performance per style +
  recommendations; generator fixes (ocean coverage 1.0, alpine airless,
  blurry gas giant, default vegetation); game-grade, not scientific.
- FOUND: the flight view drew NO celestial bodies at all (only the
  starfield and the far hub proxy; `drawCelestial` filtered an object type
  nothing used). The patch pyramid existed only in Planet Lab.
- Patch pyramid -> `tools/planet_lab/experimental/` (SurfacePatch,
  CubeSphere; Lab viewer + old --check only).
- NEW production representation (`src/world/celestial/surface`):
  `GlobeBake` (cube-map cache of the field, texel = sample at 2 x texel),
  `LocalTerrain` (camera-centred clipmap on a planet-fixed equal-angle
  lattice, 128 cells per level, vertex = field at its lattice point,
  morph to level k+1 in the vertex shader, no skirts / no patch identity),
  `ImportedSurfaceField` (Sol curated maps as macro + procedural detail
  below the source texel).
- NEW renderer `src/render/celestial/flight` (EliteCelestialFlightRender,
  contract CELESTIAL_RUNTIME_RENDERER.md): sprites -> globe (displacement,
  albedo / normal / relief cubes 32..1024, sliced uploads) -> local terrain
  (stencil over the globe, depth copy in the near pass for ships / hubs),
  clouds (CloudWeatherField at world time, cross-fade), single-scattering
  atmosphere, rings with planet shadow, star discs + glow, asteroid belt
  lattice (SmallBodyField meshes), log depth per fragment, worker pool,
  512 MB texture budget (LRU), GPU timestamp stats.
  `FlightFrameBuilder` builds the frame from the SAME celestial snapshot
  and `makePlanetTextureOrientation` the maps use.
- GAME: `SceneRenderer` pass 0.25 (after the starfield, before the far
  proxy); `PreparedScene` carries the client celestial snapshot + system
  definition; atlas bodies carry `sourcePath`, stars teff / luminosity.
  Style setting: `ClientModeState::celestialRenderStyleId`, persisted
  (`celestial_render_style`), main menu SETTINGS dialog (button was
  disabled), Ctrl+F7 in flight, DEV `ELITE_DEV_CELESTIAL_STYLE`.
- GENERATOR: ocean coverage = 1 - exp(-4.6 D / max(H, 6 km)) from the water
  layer (generated bulk water mostly in the mantle); `geophysics.biosphere`
  (generated "none", Sol Earth observed "complex"); vegetation only for
  complex. Catalog re-materialized (determinism: 61 systems, 0 differ;
  only ocean_coverage / biosphere / 2 presets changed); temperate surface
  oceans now 0.42 .. 1.0 (Earth 0.75). Airless rocky / icy: smooth low
  hills, craters carry relief. Gas giant: band-limited shear filaments;
  no weather shell over the bands.
- CHECKS: `ElitePlanetLab --flight-check` 22/22 PASS (silhouette of Moon /
  Mars / Jupiter at 4 / 15 / 60 R on / off axis within 0.5 px of
  asin(R/d); Earth drift vs velocity 11 m / 60 s; Earth spin 15.04 deg/h
  east; terrain lattice rebuild 2793 shared vertices 0 differ; level k vs
  k+1 1089 points 0 water/land differences). `ElitePlanetLab --check` PASS
  (old field checks). EliteMapPointerCheck PASS, EliteGalaxyPointerCheck
  PASS. EliteGame + ElitePlanetLab build (Debug).
- GAME START (scripted, nav-stress:simple, `tools/perf/galaxy_map_capture.sh`
  + ELITE_DEV_CELESTIAL_STYLE): Earth drawn below the hub (Modern), rear
  monitor Elite (coast + grid) and Anime frames; no crash after the
  locale-safe asset path fix (first run: fatal filesystem conversion).
- PERF (`--flight-bench`, Intel UHD 630, 1600x900): far 1-2 ms; near
  orbit Elite 9.5 / Modern 11.3 / Anime 11.3 ms; approach + surface Elite
  4.7-5.4 / Modern 15.4-15.9 / Anime 16.0-16.2 ms; atmosphere from inside
  8.5-9 ms is the bottleneck; worst single frames 50-64 ms (open).
  Recommendations in CELESTIAL_RUNTIME_RENDERER.md section 7.
- Captures: D:/tmp/elite_flight_capture/ (10 bodies x 4 distances x 3
  styles + Earth->Moon travel), game frames D:/tmp/elite_game_*.png.
- Diagnosed during the work: sphere winding inverted (far hemisphere drawn,
  night crescent, no sky from inside); log depth in the vertex shader
  clipped primitives behind the camera; missing gamma encode (dark Earth);
  terrain morph window overlapping the coarse hole edge (cracks); texture
  budget never evicting (every body marked used).
- Late fixes (same stage): Earth curated maps disagree (DEM has no zero
  sea) -> water from the albedo ocean colour, DEM aligned by correlation,
  sea level = land 5th percentile; atmosphere over the ground additive only
  (Jupiter / Earth no longer greyed); gas giant atmosphere thinner; Elite
  atmosphere outline only for rays missing the body (in the game it filled
  the disc green). DEV `ELITE_DEV_CELESTIAL_FLAGS=noatm,noclouds,norings,
  nodetail` in the game. Re-run after them: --flight-check 22/22, in-game
  Elite / Modern / Anime frames OK.
- OPEN: Earth / Mars close-up limited by the curated sources (~20 km
  texels); worst-frame spikes; hub proxy not depth-tested against bodies;
  eclipses; map clouds and flight clouds are different functions; human
  visual review pending. Notes: D:/tmp/PROCEDURAL_PLANET_NOTES.md.

### Renderer stabilisation, scene-wide styles, System Map planets / moons (user order 2026-10-31 e)
- User: jitter / double image; freezes after one Venus planet marker;
  Modern = calm grey volumetric planet (reference D:/tmp/152905.png, no
  day / night show); water detail scale wrong from ~400 km; Elite Classic
  not green, retro sun with a living corona; styles for the WHOLE scene
  (ships, stations, sky, asteroids, rings, sun); small objects readable on
  big ones; System Map: Earth vanishes while its hubs show, moons flicker;
  map and flight one world.
- ROOT CAUSE (jitter AND load): the main camera and the rear monitor both
  planned celestial resources every frame with different pixel sizes (Earth
  717 vs 128 px, Moon 2.5 vs 0.45 px): globe tiers 1024 <-> 256/512 and the
  local terrain level set flip-flopped (each view re-requested its own);
  different tiers / levels displace the surface differently -> two nearly
  coincident images, plus continuous bakes / uploads. Fix: only the primary
  view plans resources (render(..., primaryView)); secondary views draw what
  exists. A/B (ELITE_DEV_CELESTIAL_SHARED_VIEWS=1, same 45-s scene, Venus
  marker): globe bakes 54 -> 2, terrain level builds 673 -> 12, uploads
  956 -> 38, mip generations 161 -> 8, celestial CPU 3.39 -> 0.33 ms avg
  (max 10.8 -> 3.6), worst frame 93 -> 51 ms. Coordinate path checked
  (ELITE_TRACE_CELESTIAL): Earth relative velocity frame-to-frame change
  median 21 m/s = ~0.4 m/frame at 422 km (no visible jitter).
- MARKERS: a HUD planet marker (map info card -> rememberCelestialBody ->
  NavigationHudMarker) never touches the celestial renderer. Measured with
  ELITE_DEV_CELESTIAL_MARKERS (none / +venus / +mars / +jupiter / all):
  identical resource work (2 loads, 2 globe bakes, 13 level builds, ~38
  uploads), celestial CPU 0.35 ms, GPU ~3 ms; Venus stays a point. Steady
  280-ms frames in those runs = the DEV PNG capture itself.
- Activation levels (screen size only): point < 1.5 px (sprite, nothing
  loaded) / disc 1.5-24 px (field + small tier, no weather) / globe >= 24 px
  (clouds) / detailed (local terrain, altitude < 0.6 R).
- DIAGNOSTICS: ELITE_CELESTIAL_DIAG=1|path (D:/tmp/elite_celestial_diag.log):
  FRAME line per primary frame (CPU / GPU per pass, loads, globe / cloud
  bakes, terrain builds, uploads + bytes, mips, evictions, cache hit / miss,
  memory) + EVENT lines (level changes, bakes, tiers live, evictions).
- STYLES: render::sceneStyle (uSceneStyle) in ship / station fill + edges,
  starfield, Milky Way haze, scoped to the flight scene (maps unchanged).
  Elite Classic: black opaque hulls, light grey edges, white point stars, no
  Milky Way colour, grey-white celestial lines, sun = white disc with an
  unstable edge + flickering ray corona (sprite, min ~10 px). Modern:
  ships / stations unchanged; bodies calm grey (land lighter than sea,
  wrap light, no night emission, grey limb / clouds / haze, coast and limb
  as faint light edges, less contrast far away). Anime: three flat bands +
  ink silhouette on hulls, heavier ink edges, neutral shadow tones, stepped
  desaturated sky.
- WATER LOD: sea surface smooth from the globe distance (no micro relief on
  water); terrain water gets wave normals of a 40-m wavelength faded by
  the pixel footprint (none at 422 km, gradual on approach).
- SYSTEM MAP: Earth vanished because bodies were cube-filtered while hubs /
  ships are always drawn, and Earth was kept only by objects INSIDE the
  cube. Moons flickered because each moon was cube-filtered on its own
  instantaneous position. Policy (presentation only, physics unchanged):
  the planet of any drawn hub / ship, of the selected / followed body, or
  of a moon reaching the cube is presented; a presented planet presents
  all its moons; any presented body is pickable. Script audit on every
  navigation state: SystemHierarchyScript 31/31 (new: moons + Earth with
  its hub, 14 states); the OLD policy fails it (cube0_entered: Earth not
  presented).
- CHECKS: --flight-check 22/22, EliteMapPointerCheck PASS,
  EliteGalaxyPointerCheck PASS, game builds; in-game captures
  D:/tmp/elite_v2_{modern,elite,anime}_*.png, Lab D:/tmp/elite_flight_v2.
- OPEN: drone-view capture (DEV ELITE_DEV_DRONE_VIEW_AT) did not switch
  the view in the nav-stress session, so no close-up player ship frame;
  HUD keeps its look; Modern limb a little bright; prediction offset of the
  player ship vs interpolated world (pre-existing) not examined.

### One main camera, Model Asset audit + presentation, station v4, thrust glow, sun (user order 2026-10-31 f)
- Scope: remove the rear monitor (F1 front / F2 rear / F3 recon drone / F4
  reserved, ONE full world render per frame); audit Model Asset Editor /
  ModelAsset v4 / legacy ObjectAssembly (review: D:/tmp/MODEL_ASSET_
  ARCHITECTURE_REVIEW.md, notes: D:/tmp/MODEL_ASSET_NOTES.md); station with
  all components and 5 LODs through ModelAsset v4; representation ladder,
  per-component LOD, HLOD, compartments / portals, damage variants,
  structural graph, animation foundation, thrust glow, sun by real size.
- CAMERA (source + compiled + in-game frames): UICameraView rear monitor and
  its second SceneRenderer pass deleted (DebugSettings renderRearCamera,
  forceAssemblyLod1, secondCam branches, SHARED_VIEWS DEV switch removed).
  FlightPresentationView = Front / Rear / Drone; F4 -> std::nullopt
  (reserved). SpaceState::setFlightCameraMode(ShipCameraMode) uses the
  existing ShipCameraMode; celestial / object LOD always plan for the one
  main camera. Recon drone: external camera behind / above the ship (min
  90 m; the 9 m descriptor attachment sat inside the hull), no cockpit frame.
  Contracts updated: check_function_key_presentation_route (F4 reserved),
  check_render_surface_ownership, PresentationCoordinatorContractTests,
  feature contract list. Evidence: D:/tmp/elite_cam_drone.png,
  elite_drone_*_{a,b}.png.
- STATION ROOT CAUSE (audit, evidence in the review): runtime never consumed
  ModelAsset (RuntimeModelAssetLibrary::useBinary had no callers); the legacy
  registry listed 6 of 29 LOD0 parts (no SolPet_* petals, no habitat
  external modules); legacy LOD is a binary lod0/lod1 switch at 10 km and
  the LOD1 OBJs exist only as stale build copies; tracked
  src/assets/compiled/models/station.elmodel is a v2 dump of the legacy
  registry (2 LODs, LOD1 RGE -1). The real 5-LOD / 29-component package
  lives ONLY in build/tools/model_asset_editor/workspaces/station/working
  (untracked) - never BUILT. Its LODs are the same 29-part assembly at
  decreasing density (577k / 538k / 240k / 76k / 51k tris; RGE 0 / .00238 /
  .00954 / .0382 / .153); no shell / proxy exists in any source.
- MODEL ASSET (compiled + EliteModelAssetPresentationCheck PASS, synthetic
  ship + real station): schema adds RenderLodRole (Component / ModuleProxy /
  Shell / Proxy), Compartment, CompartmentPortal, SurfaceEffectBinding,
  JointType::Prismatic + travel (chunks CMPT / PRES, older readers skip
  them); shared BUILD src/model_asset/ModelAssetBuild (editor uses it) +
  headless EliteModelAssetBuild; pure src/model_asset/ModelAssetPresentation:
  ladder CULLED/POINT/PROXY/SHELL/COMPONENT/FULL by projected diameter
  (1 / 6 / 40 / 180 / 700 px, hysteresis 1.2), per-component SSE LOD
  (coarsen 1.8 / refine 2.2 px, component cull 2 px, FULL 1 px), module
  HLOD collapse < 90 px, interiors only in FULL via BFS from visible open
  exterior portals (frustum, >= 8 px, facing; depth 2), structural islands,
  atomic ActiveSets, telemetry formatFrame. Check: 22 m ship approach /
  retreat POINT 20.6 km, PROXY 3.5 km, SHELL 524 m, COMPONENT 126 m, FULL
  40 m, 0 flips under +-1.5 % jitter; intact hull 0 interiors, breach ->
  engineering only, hatch open -> + cockpit, breach facing away -> none;
  detached engine = own island / object; intact -> breached switches
  collision / hit / opening / repair together. Real station: 60 m off one
  end all LOD0 (LOD1 error 13 m = 2.4 px at 5 km, honest SSE); 25 km off
  one end: near LOD1 / far LOD2 (worst 1.68 px); 30 km: LOD2 = 43 % of
  LOD0 tris; select() 55-70 us.
- STATION IN GAME (compiled + in-game frame + LOD log): ObjectType::Station
  renders from assets/compiled/models/station/station.elmodel (BUILT by
  EliteModelAssetBuild from the working package; untracked, see notes)
  through render::geometry::ModelAssetGpuLibrary: lazy GPU upload per
  (LOD, geometry), descriptor basis + logical length (scale 0.905 -> 4937 x
  4525 x 5021 m = legacy frame of hit volumes / docking), habitat ring spins
  with replicated legacy module angles. ELITE_MODEL_ASSET_LOD_LOG=<path>
  telemetry; ELITE_LEGACY_MODEL_ASSETS=1 = old path. Perf (nav-stress, same
  run length): objects CPU 1.6-2.5 ms vs 0.33 ms legacy (29 components
  instead of 6), slow-frame main_ms median 9.1 vs 8.5 ms; first lazy LOD
  upload hitch 161 ms (open). Collision / hits stay legacy (6 parts).
- THRUST GLOW (in-game frames, three styles, forced thrust): mesh part ->
  descriptor module -> physical-control slot -> executed thrust01
  (readEngineMotorLiveView); a visual engine without its own slot glows with
  the mesh-less aft main motor (Newtonian-I ship_newtonian_i_main_center).
  Pad = aft end zone of the engine part (Cobra nozzle caps face forward in
  the OBJ, so the facing term is two-sided). Modern emissive + white-hot core,
  Elite white plate, Anime flat cel. DEV ELITE_DEV_FORCE_THRUST01.
- SUN (Lab captures 1 / 0.2 / 0.05 / 0.015 AU, three styles): true projected
  disc on a screen quad (no GL point-size limit, no 10 px inflation);
  activity from projected size only (edge wobble from ~6 px, corona from
  ~25 px); Elite flickering vector rays, Modern limb-darkened disc +
  streamers, Anime flat core + posterised petals.
- OPEN: authored SHELL / PROXY / module proxies for station and ship;
  Cobra still legacy render path; hits / collision from asset (high risk,
  needs flight gates); station damage states / hidden parts not mapped to
  the v4 components; lazy-upload hitch (prewarm next LOD); visual ships /
  traffic proxies have no thrust glow; Blender zone import not written;
  editor UI does not author roles / compartments / portals / effects yet;
  station working package must move out of build/ (storage decision).

### Station LOD: moving parts + measured per-part error (2026-10-31 g)
- MOVING PARTS (compiled + check + in-game LOD log): the game passed an empty
  ObjectState to presentation::select, so the spinning habitat ring was
  LOD-selected / frustum-culled / HLOD-grouped at its rest pose. Now the
  replicated module angles go in as asset-space nodeMotion (SceneRenderer);
  component, module-group and object bounds are taken where the parts are
  now; draw matrix = assetModel x DrawItem.assetTransform (motion included).
  Translation / rotation of the whole object was already covered (camera is
  re-expressed in asset space each frame).
- MEASURED PER-PART ERROR (compiled + check): RenderLod::nodeGeometricErrorM
  (PRES trailing section; older files read without it). BUILD
  (makeProductionCopy -> ModelAssetLodError, editor BUILD now uses the same
  function) measures, per semantic node and component LOD, the two-sided
  distance LOD0 <-> LODk (vertex + centroid samples, BVH nearest triangle,
  identical parts = 0, instances measured once, monotone along the chain).
  select() uses it, LOD-wide ratio is the fallback. Also fixed: loading a LOD
  payload reset the PRES role of that RenderLod.
- FINDING: the editor LOD-wide errors were understated. LOD-wide LOD1 =
  10.8 m; measured LOD1: Solar_Panels 452 m (panels dropped), Core_Front
  60 m, Core_Rear 36 m, habitat 18 m, petals 0 (identical). Selection is now
  honest, so it is NOT cheaper: in game at ~550 px 579.6k tris (was 583.6k),
  30 km 491k (was 269k with up to ~11 px popping). Core_Front alone =
  201k tris at LOD0, 188k at LOD1: the real fix is decimation in the editor.
- BUILD of the station takes ~2 min (measurement). Check:
  EliteModelAssetPresentationCheck failures=0 incl. new moving-part and
  measured-error cases.

### ModelAsset station: GPU instancing (user order 2026-10-31 h, stage 1)
- Scope (order h): instancing on the ModelAsset path, real mesh
  simplification in the editor LOD generator (component cull stays), manual
  UI pass + Quick Start. ModelAsset / ladder / five LODs / measured per-node
  error are the fixed base.
- INSTANCING (compiled + in-game A/B, same build, ELITE_MODEL_ASSET_INSTANCING
  =0|1|alt): SceneRenderer queues ModelAsset draws separately and
  drawQueuedMeshPassesInstanced() groups them by MeshGPU (= LOD geometry +
  state variant; one lighting / shader set per queue): one fill + one edge
  submission per group, per-instance current model matrix (spinning ring
  included), fill of all groups before edges. Existing MeshGPU::drawInstanced
  / drawEdgesInstanced reused; new programs large_object_shader_instanced
  (large_object_fill.frag) and edge_shader_thick_instanced (edge.geom +
  edge.frag = the production thick / faded edges); MeshRenderer::
  drawInstanced picks large-object uniforms by shader type,
  drawEdgesInstancedThick mirrors setupEdgePass state.
- Visual equivalence (3 styles): alternating-mode consecutive frames
  (DEV capture now takes one request per frame) - adjacent mixed-mode diffs
  are smaller than same-mode diffs two frames apart, i.e. scene motion only.
- A/B (nav-stress station ~600 px, 45 samples / mode): instances 29, unique
  meshes 6; fill + edge draws 58 -> 12; CPU submission 0.764 -> 0.278 ms; GPU
  (timestamp queries) 5.79 -> 5.73 ms; tris 579.6k both; frame main_ms median
  9.15 -> 8.68 (noisy). Telemetry: DRAW line in ELITE_MODEL_ASSET_LOD_LOG,
  ma_* fields in [ScenePerf]; drawCalls now counts real submissions there.

### Generated LOD: surface simplification + editor UI pass (user order 2026-10-31 h, stage 2)
- GENERATOR (compiled + headless + UI): every generated level = component
  cull (unchanged, generated_lod_component_cull_v1) THEN surface
  simplification of the remaining geometry (tools/model_asset_editor/
  LodMeshSimplifier, lod_surface_qem_region_constrained_v1): libigl
  igl::decimate with own unweighted plane quadrics (cost = sum of squared
  distances to the original planes; an edge above target^2 never collapses),
  smoothing regions (crease > 25 deg / material / smoothing group / boundary)
  never crossed, border lines kept by constraint planes, corners and authored
  edge vertices pinned, no face flips, canonical edge topology (no
  positional welding of separate sheets), textured / non-edge-manifold
  components untouched, result re-canonicalized. Target per geometry =
  max(factor x feature threshold, measured cull deviation). Each level from
  LOD0, each geometry once (instance families stay shared), geometries in
  parallel. Measured two-sided deviation (ModelAssetLodError::meshDeviation)
  is reported per geometry; BUILD re-measures per semantic part (authority).
  libigl's own qslim placement was not usable (A.inverse() -> inf on planar
  regions). Geometry kernels compiled -O2 in Debug trees (72 s -> 2 s).
- UI: LODS stage gains SURFACE SIMPLIFY + tolerance factor, PREVIEW
  GENERATION (non-destructive, cached; APPLY reuses it) and a per-level /
  per-mesh report table; apply carries the options; per-geometry
  regeneration uses the same generator. Localization keys added (5 locales).
- STATION (editor UI, saved WORKING r4, BUILD from the UI): rendered tris
  LOD0 623.7k; LOD1 583.6k -> 86.7k, LOD2 269.1k -> 27.7k, LOD3 92.9k -> 3.7k,
  LOD4 67.3k -> 1.5k. Core_Front 201,332 / 18,480 / 8,824 / 911 / 448 tris,
  measured per-part error 0 / 64.0 / 132.1 / 377.5 / 377.5 m. Canonical
  failures 0. EliteLodSimplifyCheck --synthetic: box 768 -> 12 T, error 0,
  material seam kept; open sheet -> 2 T; bump within 2x target: PASS.
- RUNTIME FIX: SHELL / PROXY without an authored shell / proxy fell back to
  the coarsest component LOD with no error check (station at 30 km: LOD4,
  ~9 px for a 1.8 px budget). Now it stays on the per-component SSE path.
- IN-GAME (instancing on, same build, package swapped): scene ~620 px:
  580k -> 286k tris, GPU 4.89 -> 2.58 ms; 30 km (per-component SSE): 378.5k
  -> 56.3k tris, GPU 3.42 -> 0.63 ms; 60 m off the bounds: 616k / 607k tris
  (big parts must stay LOD0), GPU 15.9 / 14.0 ms median, noise 6-47 ms.
  First LOD upload hitch 241 -> 148 ms (scene), 152 -> 31 ms (30 km).
  DEV ELITE_DEV_MODEL_ASSET_PLACE_M=<m> places the station in front of the
  main camera for such measurements.
- EDITOR WORKFLOW BREAKAGES found in the manual pass and fixed: (1) package
  stamp compared raw mtimes; MinGW's last_write_time shifts by exactly 1 h at
  a DST change, so all authoring evidence was dropped on load ("editor_state
  unavailable or mismatched") - now +-1 h tolerated; (2) meshes certified by
  a retired canonical algorithm stayed green while their records were
  dropped -> PREPARE skipped them, generator blocked ("no canonical SOURCE
  record") - now demoted to pending; (3) BUILD's own stage status change
  marked WORKING dirty, so "save before BUILD" refused every attempt - BUILD
  status no longer dirties WORKING.
- Contracts: 79 reds before and after (same set; none added).
- OPEN: Core_Front stays LOD0 below ~33 km because the component cull
  removes long thin parts (LOD1 cull error 60 m). What-if without cull:
  Core_Front 42k T at 28 m, Core_Rear 18.5k T at 4 m, habitat 21k T at 12 m
  -> a Hausdorff-consistent cull criterion (or a simplify-only first level)
  would make the near station cheap at the same budget: user decision.
  Lazy-upload prefetch, authored shell / proxy still open.

### OPEN
- Galaxy spatial frame: HUMAN acceptance with a real mouse not done (the
  scripted runs drive the production input path, not a hand). Known: the
  cloud info card opens near the clicked point and covers the centre of the
  entered cube (card placement unchanged, user can close / minimise it);
  dense star labels in a root parent; "What is here?" texts are English
  (not in the localisation profile); empty-space depth = the hover layer
  (through the selected child / view axis) - no explicit layer stepping
  and no precise point cursor inside a terminal cube yet (targets already
  carry the exact point `hitLy`); children holding notable objects are not
  outlined (stars / clouds themselves are).
- Galaxy pointer: HUMAN repeated-click acceptance with a real mouse not
  done (incl. return from System Map); the LIC cannot be selected by click
  while the camera is inside it (by design of R5 fix); a cube whose marker
  is covered by a star glyph (entry cube on Sol) refines via wheel / the
  waypoint glyph only; star labels are not click targets.
- A3 below the 94 % cutoff (93 %): balanced bank 5.15 m/s2 vs 36.77 on one
  motor, 180 m on a curve. Candidate law (user decision): switch off when
  the one-motor axis gives more thrust than the balanced bank.
- Client capability still hybrid (re-derived from snapshot): mass / CoM and
  part of the operating state differ; target: control facts only from the
  server telemetry.
- Harness: 180-s BLUE limit fails slower damaged Assisted flights; no flight
  reaches the dock in the window (49 km route).
- A3 reactor 60 %: planner side solved (stop-before-replan: plan from rest
  found first time, A4); the gate is not formally closed - the harness 180-s
  BLUE limit fails the slower flight.
- X one main lost: 110-130 ms autopilot frames on the slow replanned profile.
- `updateDockingAdvisory` (3.8k lines) still one member function (debt: no
  behaviour-neutral seam taken during the acceptance run).
- Assisted after an engine loss tracks curves poorly (185 m cross-track on
  the replanned route; inside the 250-500 m route corridor, healthy <= 20 m).
- Combined RCS share is used by planned turns only; translation + rotation
  in narrow zones is not separately bounded (small attitude-hold torque).
- Exact 180 deg docking vertex: explicit refusal, no production route hits it.
- NOT visually reviewed by the user: Galaxy Map clouds / cards, System Map.
- Baseline reds above unchanged.

### Hub dock traffic + colonization canon (user order 2026-11-01, overnight)
Status levels: dispatcher / traffic = AUTOMATED PASS (offline, production
code); in game = GAME START + traffic live; player station capture = see
"Player dock smoke" below. Nothing here is user-ACCEPTED.
- Dock: `hub_hex_dock_a` = rotating hexagonal cylinder (apothem 150 m,
  400 m long, spin 2 deg/s about its own Z = entry axis), front slot
  160 x 60 m, apron, guide pylons, chamber with 6 pads (one per inner wall,
  "down" = wall normal). `hub_hex_dock_base` = fixed coaxial service block
  (600 x 240 x 240 m) behind it with a real tunnel (collision = 4 walls,
  no solid core). Geometry contract: `src/game/docking/model/HexDockFacility.h`;
  meshes + collision headers generated by `tools/hub_meshes/generate_hub_dock_meshes.py`.
  Replaces `guidance_dock_cube_a` (scene, semantic anchors, runtime test
  ports, BLUE route graph portal `hex_dock_a.dock_front`).
- Dispatcher: `DockTrafficController` extended with the full lifecycle
  (queue -> hold -> entry clearance -> station control -> docked ->
  departure queue -> cleared -> departed), 6 holds (station FIXED frame,
  flower at r 450 m, z -1600), 6 pads, shared resources outer_lane +
  chamber_lane (one ship in the critical section), departure priority when
  arrivals lack pads, directEntry (pilot at the portal, no hold).
- Traffic: `src/game/traffic/DockServiceTraffic` (deterministic, FIXED-frame
  kinematics that look Newtonian: burn - coast - flip - main-engine brake;
  roll sync with the dock spin on the axis; station-assisted transfer /
  descend / latch; departure lift -> corridor -> axis -> dispersal).
  Ships: new OCS-4 "Lastochka" cargo shuttle (Orbital Freight Union; passport
  `ofu.ship.ocs-4.lastochka`, Provisional; descriptor + catalog products),
  Earth <-> Hub manifests, 1 flight / 10 min (dock capacity ~12 cycles/h),
  Cobretts looping dock -> 20 km -> return. Game: TrafficShell ships
  (`GameSimulationDockTraffic.cpp`), env ELITE_DOCK_TRAFFIC=0 /
  _COBRETTS / _INTERVAL / _LOG.
- Offline gate `EliteDockTrafficCheck` (6 h, production code): 4 Cobretts:
  70 arrivals, 0 invariant / separation / structure violations, min gap
  38.6 m (neighbouring pads); 9 Cobretts (saturation): 72 arrivals, queue max
  7, wait max 1830 s, 0 violations. Logs: D:/tmp/elite_dock_dispatcher_c4.log, _c9.log.
- Player: server requests docking (directEntry) when a player ship enters
  the axis approach zone (z -1500..-150, r < 260 m, < 40 m/s); station
  capture when cleared, on the axis (r < 60, z -800..-150, < 15 m/s);
  captured ships are excluded from the physics loops and posed by the
  traffic model; L = departure request (ClientShipCommand
  RequestDockDeparture); released at the portal, lane released at z -2400.
  Status replicated in ShipSnapshot.dockDispatcher -> docking terminal.
  Root cause of the first failed smoke (run3/run4): nav route stops at
  z = -500 (gate volume front face), the capture radius was 160 m around
  -200 -> capture never fired; now the capture zone is the approach axis.
- Player dock smoke (run5, nav-stress:simple, Newtonian-I, game build of
  this commit): request accepted at z -1163 (t 198 s); direct entry cleared
  to P1 (322 s); station capture at z -500 -> corridor -> handoff at the
  chamber gate (435 s) -> latched on P1 (492 s), rotating with the dock;
  DEV auto-departure request (552 s) -> DEPARTURE QUEUED position 2; the
  720-s window ended before clearance -> player departure / release at the
  portal NOT yet seen in game (offline traffic departures are). Terminal
  shows REQUEST ACCEPTED / ENTRY CLEARED / STATION CONTROL / DOCKED / L.
  Frames: D:/tmp/elite_dock_run5/f*.png, log traffic.log.
- Tests: route_stage_compiler / route_volume_containment / docking_* PASS;
  contracts check_hub_guidance_test_geometry PASS (now pins the hex dock and
  the non-rotating service block). Pre-existing reds (not from this work):
  traffic_route_graph_tests (expects 700 m exit straight, data 150 m since
  e2dc2dc5), check_navigation_stress_field ("Hub visual/navigation basis
  sign contract changed", red at HEAD before this work).
- Colonization (Part B, PROPOSAL, offline-verified, not author-accepted):
  `Notes/History/COLONIZATION_CANON_3126.md`, `Notes/Colonycore/COLONYCORE_V2.md`,
  `tools/colonization_lab/` (python lab.py; deterministic: two runs and the
  committed out/ identical; --check-dataset OK), generation_ships.json now
  valid JSON with canon blocks (no code reads it). Reports in D:/tmp:
  COLONIZATION_CANON_RECONCILIATION.md, GENERATION_SHIP_SANITY.md,
  COLONY_PSYCHOLOGY_REVIEW.md, COLONY_ECONOMY_REVIEW.md. Open author
  questions: arrival dates, GENESIS-1, Earth 3126 anchor (5.5 bn), hidden
  answers, NOMAD-1 timer, Icy Faith, names, remote concessions.
- Next: user visual review of the dock + traffic; manual (non-autopilot)
  docking request via comm UI; traffic shells as physical obstacles for the
  player planner; dock collision vs captured ship path audit.

### Colonization: causal demography + Notes/Lore reference (user order 2026-11-02)
Status: PROPOSAL, offline-verified (deterministic lab), NOT author-accepted.
- Demography (`tools/colonization_lab/demography.py`): population is no longer
  a target. Yearly 5-year age groups; fertility from child economics (living
  space = built volume in 3 tiers: domes/lava tubes, moon/belt workings,
  rotating cylinders; child labour value = (1 - automation) x labour shortage;
  pensions, education cost, child mortality), family norm lagging ~45 y,
  religious subculture, regime policy, birth licences, tech capability in 12
  domains (critical mass of specialists, degradation, cannibalisation),
  migration only after 2750 over the beacon graph (Earth windows).
  Result: Earth 3126 6.0 bn (peak 13.35 bn 2100, collapse 2470-2540), off-Sol
  183.4 M (old targets 26.8 M); largest polities 15-33 M (New Karma 32.7,
  Citadel 23.4, Strife Belt 21.5, Red Outpost 20.6, Harmony 18.7). Strong
  sensitivity: without assumptions F9 (inherited family propensity) + F10
  (automated childcare) off-Sol 28 M, Earth 0.22 bn. 18 colonies differ from
  the author's numbers by > 2x (kept as `pop_3126_authored`, decision open).
  Weak spots: e0 saturates at 94 and automation at 0.95 everywhere (late
  colonies too uniform); research notes written from memory, no live web search.
- Economy: freight priced by mass; interstellar bulk share 51.1 % -> 15.8 %
  of trade value; 112 flows vanished (water/metal mostly pointless: 1e22-1e25
  kg volatiles per system), 179 appeared (electronics, He-3 into giant-less
  systems, precision equipment, isotopes, jump cores, biotech). Earth Return:
  Earth 33x population, 58x industrial index, but no own jump cores; Cognium
  controls the beacon network -> the war is about the network.
- Knowledge model: CANON / PROPOSAL x PUBLIC_RECORD / AUTHOR_TRUTH + UNKNOWN on
  every record (615 + 153 settlement proposals); list `out/proposals.md`,
  `Notes/Lore/AUTHOR/PROPOSALS.md`.
- `Notes/Lore/` (117 generated + RESEARCH.md): 35 ship dossiers (+ hidden
  LAST-LIGHT-1 in AUTHOR/), 34 colony dossiers, 25 settlement system files
  (152 public settlement records), ECONOMY (assets, mines, shipyards, trade
  routes, dependencies, technology), DEMOGRAPHY, POLITICS, TIMELINE, AUTHOR
  (hidden canon, missing ships, quest truths). All number tables are
  generated from `out/lore_facts.json` by `tools/colonization_lab/lore_md.py`
  (--write / --check: stale blocks, unknown ids, broken links, conflicting
  population numbers in prose, hidden missions in public files); narrative by
  hand with ~734 inline [PROPOSAL] tags. `Notes/History/COLONIZATION_CANON_3126.md`
  marked superseded.
- Fixed after writing: hidden-truth leaks in generated public rows
  (MAYFLOWER-2 2180 cause, GREEN-ARK-1 2480 symbiont origin, UHURU-2 sponsor
  field stating the PROPOSAL dock fire, public asset named after LAST-LIGHT).
- Gates: lab two runs byte-identical and equal to committed out/,
  --check-dataset OK, --check-lore OK, lore_md --check OK, second --write
  changes nothing.
- Open author decisions: accept F9/F10; model vs author populations (18
  conflicts); Earth peak / 10 bn in 2500 checkpoints; can Earth make jump
  cores; Citadel as main jump-core seller; settlement names; catalog vs old
  notes (habitable "super-earths" mostly hot/sub-Neptune -> colonies on moons,
  ice worlds, workings); [PROPOSAL] tag style (start vs end of sentence).

### Live hub traffic + map ownership + Anime render (user order 2026-11-02, evening)
Status: live-game verified on this machine (target Windows box); NOT yet
user-accepted.

**Why the user saw no traffic (diagnosed in the real runtime, not offline):**
1. The traffic itself ran: a headless probe (`EliteServer --dock-traffic-probe
   SECONDS LOG`, real ServerRuntime + attached session, reads the CANONICAL
   client snapshot) showed 6-8 traffic ships replicated with their
   ShipInstanceIds from t=0.
2. The default player start is 10.7 km from the dock and BEHIND the hub
   (dock FIXED z = +7548; the entry/holds face -Z). From there the docked ships
   are hidden inside the dock, the holds are 11.6 km away and a 22 m Cobrett is
   sub-pixel.
3. The server radar collected contacts only from materialized ships
   (`m_ships`); TrafficShell ships were never radar contacts -> invisible on the
   radar/HUD. FIXED: shells in the same system are radar inputs
   (`GameSimulation.cpp`, perception loop).
4. When the motion-fidelity LOD materialized a traffic shell near the player
   (350 m / aimed 3 km), `updateDockServiceTraffic` lost it (only `m_shipShells`
   was updated): the ship froze/drifted under physics and was orphaned when the
   actor ended. FIXED: the traffic model keeps writing the pose into the
   materialized Ship (same EntityId/ShipInstanceId); traffic ships are kinematic
   (`isDockStationControlledShip` covers them -> skipped by physics/AI loops);
   an ended actor's materialized ship is dematerialized then removed. Live
   evidence: CBR-13 materialized at 272 m and kept flying its route to hold H2,
   CBR-11 the same on the departure axis.
6. Elite Classic (the default look) drew every traffic ship invisibly: hub
   traffic renders through the instanced whole-ship PROXY lane, whose edge pass
   had been commented out ("jellyfish"), and Elite's hull fill is black - black
   on black. FIXED: Elite draws instanced proxy edges out to 4 km (fade from
   1.5 km); Modern/Anime keep fill only. Live evidence:
   `D:/tmp/hub_traffic_07_elite_cobrett_at_hold_now_visible.png`.
7. A client-only decorative system (`StationTrafficSystem`, 1500 "Cobra MK1"
   visual ships orbiting the station, no server entity) was still running in
   gameplay - exactly the decoration the order forbids, and it shared the
   traffic render lane. REMOVED (source, CMake, SpaceState); contracts
   `check_client_spatial_domain` (passes) and `check_feature_contracts` now
   point the traffic-visibility tag at the replicated shells. Elite frame time
   on the same scene 13.45 -> 12.05 ms. Note: `[ScenePerf] visual_ships_ms`
   ~12 ms after the removal is the GPU sync point moving into the instance
   upload, not cost (total frame time fell).
8. Throughput was low: one movement through outer+chamber lanes at a time;
   the next entry waited for the previous ship to LATCH. FIXED: arrival
   pipelining in `DockTrafficController::update` - while the previous arrival
   is under station control in the chamber (outer lane released), the next
   arrival may leave its hold (approach >= ~100 s > station transfer < 60 s);
   `chamber_lane` is handed over on latch. Older departures are never
   overtaken. Offline gate (6 h, 4 Cobretts): arrivals 70 -> 78, 31 pipelined
   entries, invariant/separation/structure violations 0, min gap 38.6 m.
   6 Cobretts saturate the 6 pads (73 arrivals) -> kept 4 Cobretts + shuttle
   every 600 s.

**Telemetry:** `ELITE_DOCK_TRAFFIC_LOG=<path>` now also writes a `[status]`
row per actor every 10 s (phase, tier shell/MATERIALIZED, fidelity, FIXED
position, dock and player distance) and the client prints `[TrafficRender]`
(shells in the flight scene, hydrated assemblies, nearest distance) every 5 s.
`ELITE_DEV_DOCK_OBSERVER=1|2|3` parks the player at a dock viewpoint
(2 = hold the view; 3 = near the hold flower). `ELITE_DEV_MAP_SCRIPT=
"SEC:f1|f3|f10|f11|f12|lanes|select:CALLSIGN;..."` drives views for captures.

**Live acceptance run** (`D:/tmp/elite_live_hub_traffic.log`, 41 min real
time, RelWithDebInfo client + local server, frames `D:/tmp/elite_live_hub/`):
- Shuttle ORB-1702 full cycle: materialised 25 km Earth-side t=30 -> queue
  gate + docking request t=180 -> hold H5 -> entry cleared t=428 (pipelined
  chamber hand-over t=485) -> station handoff t=555 -> latched P3 t=612 ->
  900 s unload/load -> departure request t=1512 (queue position 3) ->
  cleared t=1803 -> clear of the controlled zone t=1941 -> crossed the 11 km
  hub boundary ~t=2080 -> Earth burn t=2180 -> left the local zone t=2348.
- Cobrett CBR-12 full cycle: far point -> request t=205 -> hold H1 ->
  docked P1 t=485 -> service -> departure request t=725 -> departed t=1248.
- `[TrafficRender]`: all traffic shells are in the flight scene with
  assemblies; close views show them (`D:/tmp/hub_traffic_*.png`).

**Maps (one ShipInstanceId, one owning level):**
- Ownership rule: the server binds a traffic ship to the hub
  (`motion.hubId`) while it is inside the dock's controlled zone =
  `DockServiceTraffic::controlledZoneRadius()` = max(outermost queue gate,
  departure turn) = 11 000 m (HexDockFacility data, not a new constant).
  Hub Map shows only ships of that hub (existing rule); System and Detail maps
  now skip non-player ships with a hubId (the hub represents them). The local
  player keeps its marker on every level.
- Routes: `ShipSnapshot.traffic` (callsign, phase, destination, remaining
  route as world offsets) is the traffic model's real remaining plan
  (`DockServiceTraffic::plannedPath`, same segments `pose()` evaluates).
  Hub/Detail maps draw the SELECTED traffic ship's route; `T` on an open map
  shows all routes plus the authored dock lanes (holds, hold->axis join,
  arrival axis, departure clear/turn) from HexDockFacility. System Map: no
  per-ship route (a 20 km route is sub-pixel at AU scale).
- Known limit: the Earth Detail map is planet-scale, so a ship 11-25 km from
  the hub draws on the hub icon (selection ring confirms ownership).

**Anime render (same renderer, Anime branches + composite):**
- Ships/stations: colour masses (base, cool blue-violet shadow tone, narrow
  warm highlight, AA terminator); distance merges the bands and sinks the
  hull into a painted depth tone; ships use an own light steel tone (the
  Modern ramp starts near black). The old N.V "ink" blocks are gone.
- Contour: silhouette/fold ink in the cinematic composite from scene depth
  (MSAA depth is now a multisample texture read at sample 0; a depth resolve
  blit cost 1.4 ms). Only mesh pixels (1-depth < 0.06, > ~15 m) are inked -
  planets/Sun/space are backdrop; a planarity test (2nd difference) rejects
  grazing planes/near terrain; sub-3-px objects stay colour dots. Geometry
  edges in Anime are thin, slate, fading with distance (detail, not blueprint).
- Planets: posterise 6 levels without the x1.35 saturation, sphere normal
  dominant (less relief noise), soft terminator + half-tone, blue-violet night,
  bright atmospheric limb instead of an ink ring, no coastline ink; clouds with
  a soft brush edge and lavender shade. Sun: same physical disc/activity, clean
  core + warm rim + soft broad corona. Milky Way: soft wash, no hard steps.
  Composite in Anime: no grain/softening, half bloom, blue-violet shadow / warm
  highlight grade, faint painted wash on empty space only.
- Performance, same live scene, same 90-320 s window (RelWithDebInfo):
  Elite 13.4 ms, Modern 16.8 ms, Anime 17.1 ms per frame (before the MS-depth
  change Anime was 17.6-17.8; A/B without ink 16.2). Celestial GPU passes are
  identical in Modern and Anime (5.06 ms).
- Still open (artistic): Anime station palette is a single lavender-grey (no
  material accents), faint residual terrain contours very near the surface,
  thrust plume does not exist in any style (nozzle pad only); authored dock
  lanes on the Hub Map are tiny at the auto-fitted 11 km extent (zoom-in not
  verified in a capture).
- Baseline red still present: `check_feature_contracts` stops at "Debug
  Control checkbox renderShipUi has no verified runtime consumer" (its earlier
  stale read of `pushDebugControlState` from SpaceState.cpp was fixed: the
  function lives in SpaceStateDevPanel.cpp).

**Gates:** builds RelWithDebInfo + Debug OK; `docking_infrastructure_api`,
`route_stage_compiler` PASS; EliteDockTrafficCheck 6 h 0 violations;
architecture checks touched by this work identical to HEAD (pre-existing reds:
docking_module_boundaries, hub_motion_lab, multiplayer_client_local_identity,
navigation_api_purity, navigation_foundation_lock, navigation_guidance_layer,
client_acceptance architecture, system_map object_overlay).

### Anime v2 by the user's references + material baseColor (user order 2026-11-02, night)
Status: implemented, live-captured; NOT user-accepted.
- References `D:/tmp/anime-1..4.jpg` analysed in `D:/tmp/ANIME_REFERENCE_ANALYSIS.md`
  (measured palettes, sky structure, ANIME STYLE TARGET, self-comparison).
- **Material baseColor is model data now** (user rule: model/material ->
  baseColor -> each style interprets it; no animeColor). `MeshTriangle.baseColor`
  carries the face's material colour into vertex attribute 3 (MeshGPU no longer
  overwrites it): ModelAsset `MaterialDefinition.baseColor` per triangle
  (station, already editable in EliteAssetEditor SURFACES), OBJ MTL `Kd` per
  face, otherwise `ObjectAssemblyDesc::defaultBaseColor` (Cobrett 0.42/0.47/0.50,
  hex dock 0.62/0.64/0.66, service block 0.52/0.54/0.56, OCS-4 0.66/0.64/0.58,
  generic default 0.55/0.57/0.60). Shaders: near colour = material colour
  (Modern albedo, Anime tonal masses), LightingParams mid/far stay as the style's
  distance fade; `hullColorNear` is no longer used; Elite ignores colour.
- Anime: shadow = own colour darkened with a slight steel-blue shift (no
  lavender), highlight = lighter own colour; stations use a gentler shadow step
  and a shifted terminator so small parts merge; deep space = world-fixed
  painted sky (deep blue <-> violet) BAKED once into a 256^2 RGB16 cubemap
  (per-pixel procedural noise cost ~17 ms at 1920 px - measured 33.4 ms frame);
  planet limb glow (anime-2) as a separate cheap pass over a 1.9 R sphere,
  continued inside the air shell (no dark ring), dithered; soft stars; clouds
  cream/blue-grey; sea deeper blue.
- Performance (live, same scene, 1920 px): Elite 12.4, Modern 15.9 ms; Anime
  with the procedural sky 33.4 ms -> baked sky 18.0 ms -> two-pass limb glow
  17.6 ms (+1.7 ms / +11 % over Modern: depth ink at 1920 px, glow pass, sky
  fetch). Ink is limited to mesh pixels ~15 m..30 km, so the near-terrain depth
  pass seen from orbit gets no false contour lines (they were visible in drone
  views). PlanetLab GPU atmosphere timers in Anime read 10-20 ms
  even with the glow pass disabled while Modern reads ~0.6 (0.00 inside the
  air): those timers are not trustworthy here; end-to-end frame time is the
  measure. Open: profile the Anime atmosphere pass properly.
- Adaptive scene palette (user idea): evaluated, NOT implemented now (the
  references show own-colour shadows; gain only with other environments).
  Minimal ScenePalette design recorded in the analysis file.

### Anime objects + planets pass; Modern renamed Cinematic (user order 2026-11-03)
Status: implemented, live + PlanetLab captured; NOT user-accepted.
- UI: the style is "CINEMATIC" (main menu + localisation; `cinematic` accepted
  as an id alias); the persisted id stays `modern` (no preference churn).
- Anime objects: paleness came from the distance ramp (material mixed toward
  hullColorMid/Far greys + a depth tone; stations also toward the horizon
  colour) and a softened station shadow. Now: own material colour only, three
  values (light / base / shadow) with light materials compressed (no white
  clay), distance = less chroma + base band merged into light, never lighter.
  Silhouette = composite depth ink 2 px, near-black (object colour x0.10);
  internal depth folds 0.75. Geometry lines are RANKED: `MeshEdge.weight`
  (boundary 1, material boundary 0.85, crease 25..75 deg -> 0.35..1; OBJ and
  ModelAsset), attribute 1 of the edge VAO; Anime draws major lines near-black
  to ~2.2 km, minor lines faint and only within ~300 m.
- Olive calibration: Cobrett temporarily olive (0.36, 0.42, 0.18) for two
  calibration runs, then reverted to 0.42/0.47/0.50.
- A composite-shader name clash (`fold`) silently disabled the whole post
  chain in one calibration run ("[PostProcess] shader initialization
  failed"): fixed; every run is now grepped for ShaderUtils errors.
- Planets (both styles, separate palettes): natural zones from existing data
  (`planetZones`: vegetation = albedo greenness, dry = warm bright albedo,
  snow = bright low-saturation, mountains = relief > 3..5 km; sea depth from
  coast distance), read from a coarse albedo mip (`uZoneBias` Anime 3,
  Cinematic 1.5) -> continental masses, no biome noise. Anime: sharpened
  dominant zone, painted palette, cloud shadows, narrow soft limb; Cinematic:
  natural palette blended 35 % with the real albedo, deep-blue ocean with a
  fresnel glint, cloud shadows, blue aerial haze, scattered blue limb with a
  gentler exposure (no grey "LED strip"). Anime clouds: coarse cover mip,
  cream / blue-grey. Anime atmosphere: smooth (no stepped bands), limb glow
  = narrow halo + wide faint wash (keeps the blue sky of anime-2). Anime sky
  x0.92 (x0.82 lost the blue; the earlier blue partly came from the wide glow).
- Performance, same live scene, 1920 px: Elite 12.2, Cinematic 16.5, Anime
  17.9 ms (before this pass 12.4 / 15.9 / 17.6). Elite regression: unchanged
  look (`D:/tmp/elite_regression*.png`).
- Gates: RelWithDebInfo + Debug build; docking_infrastructure_api,
  route_stage_compiler PASS; client_spatial_domain, client_system_map_ships,
  hub_guidance_test_geometry PASS; no ShaderUtils errors in the final runs.
- Open: 8-bit banding in the wide limb glow at grazing view; Earth mountain
  threshold is a fixed 3..5 km (DEM-scaled heights); station dense geometry
  still busy at close range; no thrust plume.

### World economy architecture research (user order 2026-11-03)
Status: research documents + offline logistics reference implemented; PROPOSAL,
NOT user-accepted; no runtime change.
- Documents (D:/tmp): `WORLD_ECONOMY_ARCHITECTURE_REVIEW.md` (stocks / flows /
  decisions instead of themed layers; logistics = mandatory capacity filter
  between decision and effect; event/cause log; strategic / operational /
  physical levels with one persistent ShipInstanceId; motives instead of a
  relation number; SupportProgram pledged->allocated->shipped->arrived->absorbed;
  war trade channels official / sanctioned / re_export / smuggling /
  covert_state; 14 commodity categories + separate serialized parts catalogue;
  sealed containers; minimal primary data; lab keep/extend/delete; order),
  `REFERENCE_GAME_ECONOMIES.md` (sources: `elite_research_games_aid.md`),
  `WORLD_DATA_GRAPH.md`, `PLAYER_ECONOMY_BALANCE.md`.
- New offline tool `tools/colonization_lab/logistics.py` +
  `canon/ship_economics.json` (all money numbers PROPOSAL; ship physics from
  runtime descriptors): network stats, jump-fuel calibration, build cost,
  trip cost per tonne, yard replacement demand, fleet-from-flow. Output
  `out/logistics.json` / `out/logistics_report.md`; `--check` mode;
  deterministic (two runs, identical hashes).
- Results (3126 net: 24 beacons, 140 edges): mean edge 19.0 ly, hops real
  trips use 20.06 ly, mean trip 29.4 ly / 1.47 jumps. Jump fuel
  `k*(M/M_ref)*d`, k = 0.0544 t/ly: Cobrett full tank = 3.02 / 2.76 / 2.54
  mean jumps (empty / half / full cargo) -> author rule 2.5-3 met. Jump
  charge price derived from "mean jump = 2.5 x in-system leg" (leg 2216 Cr,
  jump 5539 Cr). Cobrett list 3.39 M Cr; trip Sol->19.7 ly with 50 t cheap
  cargo 31.8 k Cr = 637 Cr/t; heavy carrier 75.5 Cr/t; OCS-4 in-system
  75 Cr/t. Yard demand fleet 12000 / 35 y: 343 + 72 losses = 415 hulls/y
  (703 at 3 % war losses). 100 Mt/y needs 192 heavy carriers.
- Population vs author targets: targets need 1.1-1.8 %/y sustained since
  founding (late colonies 1.8-2.4 %/y), the model gives 0.4-1.25 %/y
  (off-Sol 183 M vs ~10 bn; lab Earth 5.5 bn vs 10 bn). Proposed causes, not
  multipliers: housing-cost step (habitat boom after belt industry),
  terraforming, Earth emigration after 2750; NOT implemented (needs author OK).
- Found inconsistency: traffic Cobrett cargo 40 t vs descriptor 50 t.
- Checks: `lab.py --check-dataset`, `--check-lore`, `lore_md.py --check` OK
  (117 files unchanged); lab data untouched.

### Political-economic world balance 3126 (user order 2026-11-03, second pass)
Status: offline model + permanent data + Lore layer implemented; runtime cargo
capacity unified (built, dock traffic check PASS); PROPOSAL, NOT
user-accepted. Population scale = AUTHOR_TARGET.
- Architecture moved from D:/tmp into `Notes/Economy/` (README,
  WORLD_ECONOMY_ARCHITECTURE, WORLD_DATA_GRAPH, POLITICAL_ECONOMY,
  LOGISTICS_AND_NAVIGATION, FLEETS_AND_SHIPYARDS, PLAYER_ECONOMY, BANKING);
  world facts generated into `Notes/Lore/ECONOMY/` (new WORLD_BALANCE, FLEETS,
  DEFENCE, FUEL, BEACONS_AND_NAVIGATION, SCENARIOS + blocks in the old files),
  polity report (14 sections) in every `COLONIES/<id>.md` and Sol in
  `POLITICS/EARTH_3126.md`, system report in every `SETTLEMENTS/*.md`,
  population eras per polity. One truth: canon/*.json -> out/ -> Lore.
- Demography: `canon/population_history.json` (targets + dated epochs:
  habitat boom, terraforming, high-fertility era, immigration programmes;
  Earth visa windows 6 / 4 / 1 M/y), new mechanisms in `demography.py`
  (recruiting polity never donates; fill-cheapest-tier split; Earth surface
  ~10 bn per the FINAL-ARK-1 quote), `calibrate_population.py` fits ONE
  number per polity (`intensity`, 0.38-1.4; no population multiplier).
  Result: Earth 9.96 bn, Sol outside Earth 1.71 bn, off-Sol 10.09 bn, all 18
  polities inside their ranges; boom-era TFR 3.4-4.6 (Free Clans 5.3);
  Earth emigrants 2752-3126 ~1.23 bn. lab.py D.8 table, POPULATION_3126.md.
  Old fate.md numbers kept as `pop_3126_authored`, superseded in checks.
- Cobrett capacity diagnosed: repo had no container data (one
  `container_mount_center`); 140 m3 = 6 x 23.3 m3. Decision: C20 container
  (<= 20 t payload, 23.3 m3, tare 1.7 t); Cobrett 6 x C20, 120 t limit,
  typical mixed load 56 t (volume-limited). Runtime:
  `src/game/catalog/ships/ShipCargoPassports.h` is the single source for
  `ShipStorageCaps` (cargoMass in TONNES, new containerSlots /
  containerPayloadTonnes) and dock traffic (no own 40 t); OCS-4 cargoMass
  was written in kg (450000) -> 450 t, 24 C20. Build (MSYS2 MinGW64) OK;
  `EliteDockTrafficCheck` 6 h: 78 arrivals, 0 invariant / separation /
  structure violations.
- Jump rule: `charge = q * d * (kappa * hull_envelope + M)` (field term +
  mass term), calibrated on Cobrett 3.0 empty / 2.5 full 120 t ->
  3.01 / 2.73 / 2.50 mean jumps; consumable / charge energy (25 % of reactor,
  Cobrett 27 TJ in 6 h) / peak discharge (2 s, 13.5 TW) kept apart;
  170 kg/(ly*1000 t) Cobrett, 110 train; charge 5.65 Cr/kg.
  Heavy carrier = jump tug + 12 modules (57.6 kt, 67 Cr/t); classes:
  Cobrett, medium freighter C-120, jump train / tanker, OCS-4, gas tanker,
  ore tug, service tender, patrol cutter, interceptor, escort frigate, line
  combatant, recon/EW picket, fleet tender, impact-defence tug.
- `world_economy.py` (out/world_3126/: economy, infrastructure, trade_flows,
  fleet_census, defence, beacons, scenarios, checks + WORLD_BALANCE_3126.md):
  15 commodities, 325 interstellar flows 1.36 Gt/y 7.5 T Cr/y over the real
  beacon net (+ short unbeaconed links, + expedition link to the far Kern
  pocket 37/41/45 which is disconnected at 32 ly), hostile pairs via neutral
  re-export, infrastructure from output (e.g. 6 008 mines, 28 big civil
  yards), fleet from flows (101 k shuttles, 35 k tugs, 34 k freighters, 31 k
  Cobretts incl. couriers, 3.3 k trains; 23 k line combatants, 15 k cutters),
  defence share 0.8 % (Sol) .. 3.5 % (New Karma) from doctrine + tasks,
  beacons (24 bridge + hub / station / relay / military / science), jump
  error model (950 km both-end beacons .. 190 000 km none at 20 ly),
  vulnerabilities, 6 scenarios (mine, beacon loss: Tianhe +39 % ships;
  yard; habitat disaster with SupportProgram; external aid; war + sanctions
  -> re-export via New India, import price x1.22).
- Checks (all OK): world_economy --check (author ranges, settlement sums,
  cargo identity 2e-16, no goods from thin air, infrastructure >= output,
  flows <= transport, fuel covered, replacement <= yards + imports,
  statuses), logistics --check, calibrate_population --check, lab
  --check-dataset / --check-lore, lore_md --check (123 files); two lab runs
  and two world_economy runs byte-identical (lab sha c8225243...).
- Open author decisions: Sol's navy scale (8.5 k line combatants from its
  GDP even at 0.8 %); Earth visa windows during the Cordon (4 M/y recruited
  out); Sol as the largest exporter of electronics after 3112; fleet scale
  constants (defence task norms) in `canon/polity_economy.json`.

### Spatial civilisation layer + fixes of the second pass (user order 2026-11-03, third pass)
Status: offline model + permanent data + Lore + first visible map layer
implemented; PROPOSAL, NOT user-accepted. News delay (<= 7 days to the far
periphery) = AUTHOR_TARGET. Restore point before this work: tag
`restore/pre-world-sites-2026-11-03` (= 9fa2fa19).
- Analysis answered before work (no blocking questions). Second source of
  truth risks named and closed: sites are generated from canon into ONE file
  the runtime only reads; deep-space objects are GENERATED `galaxy_object`
  copies (provenance, never edited); `earth_orbital_hub` keeps its runtime id
  (`runtime_binding`, never drawn twice); legacy
  `src/assets/data/galaxy/{actors,nodes,routes,systems}.json` (GalaxyDatabase)
  is NOT extended.
- Fixes of the second pass (all verified by checks):
  1. Defence: forces ONLY from 15 tasks; line combatants only from the
     deterrence task (key nodes x threat, squadron 4); the ship budget left over
     goes to munitions / stocks / sensors / repair / training / reserve crews /
     bases / impact & missile defence / R&D / mobilisation / mothballed / reserve.
     Line combatants 19 397 -> ~180 (226 with reserve / repair); Sol 8 477 -> 12
     (Sol's unspent ship line ~842 bn Cr/y goes to readiness). Check
     `line_combatants_only_from_deterrence_task`.
  2. Export production limited by installed (+ finishing) export plant built
     since joining the network (2 %/y of home output, cap x3; network fuels sized
     to the network's need, ramp 50 y). Flows 325 -> 261, 1.35 Gt/y,
     5.3 T Cr/y. Asset counts from installed plant (utilisation reported).
     Check `exports_within_installed_export_capacity`.
  3. C20 net / tare / gross: mass equations use gross (`logistics.cargo_gross_t`,
     runtime `catalog::ships::grossCargoTonnes`); Cobrett 120 t net + 10.2 t tare
     = 130.2 t gross; recalibrated 3.0 / 2.73 / 2.5 mean jumps (q 7.7e-5,
     kappa 0.247, field term 388 t). Dock traffic envelope mass = gross.
  4. OCS-4: passport 4 m/s2 at 830 t -> 450 t GROSS = 409 t net + 40.8 t tare;
     orbit only (0.4 g, cannot lift off Earth); new surface lifter SL-12.
  5. Runtime passports vs `ship_economics.json`: `logistics.py --check` parses
     `ShipCargoPassports.h` (dry mass, net, volume, places, C20) - PASS; a
     mismatch is detected (verified with a forced 450 t).
  6. Expedition edge removed: Kern pocket reached via Gliese 436 (real system,
     Kern staging depot "Pereval", beacon 3036 in world.json): legs 30.6 / 30.8
     / 30.4 ly. Check `no_edge_beyond_max_hop`.
  7. HTML Lore: the mirror had been made by an external tool (no style.css,
     stale since 9fa2fa19); now `tools/colonization_lab/lore_html.py`
     (markdown-it, same template) regenerates Notes/html; `--check` PASS
     (132 files).
- New: `canon/world_sites.json` (authored) + `world_sites.py` ->
  out/world_3126/{sites, society, law, comms, passengers, fleet_age, frontier,
  world_events}_3126.json, sites_checks, WORLD_SITES_3126.md; runtime copies
  `src/assets/data/galaxy_details/world_sites/sites_3126.json` (527 in-system
  sites + 4 zones) and `objects_details/world_dso_*.json` (5 deep-space
  objects). 498 sites (16 hub types), 60 Earth agglomerations at the real
  urban regions (the map Earth is the real Earth), 6 deep-space objects with
  world lines (Lotus 38 km/s; FINAL-ARK-1, LEGACY-1 in flight), 10 hazard zones,
  52 persistent administrators, social state (12 metrics, 9 archetypes,
  reactions to 12 shocks), law (13 families, offence matrix, competing
  claims, ship legal status fields), beacon comms (Sol -> Gliese 436 0.5 d,
  Kern pocket 1.4 d, Lotus 2.5 d, World's Ass 4.0 d; network diameter
  1.75 d), passengers (91 M interstellar trips/y, 2.4 M migrants, 7.3 k
  liners, 2.4 k surface lifters; evacuation of 1.2 M ~2 days), fleet age
  (8 states; jump ships never older than their polity's network link),
  piracy index (19 enclaves), wreck fields, schema registry + WorldEvent ->
  KnowledgeRecord example.
- World's Ass (月工门 = 月门 with 工 inserted: 月+工 = 肛, reads as 肛门):
  HD 102195 behind the Lotus (83 Leonis -> Lotus 21.1 ly -> HD 102195 18.4;
  direct 39.3 > 32); Cobrett needs a J20 charge tank (3.7 t round trip vs 3 t
  tank); Lotus dead zone 4.5 % unlocked / 0 % locked (beacon uptime 62 %);
  lotosite equilibrium ~9 M Cr/kg, ~74 kg/y, ~534 expeditions/y, ~26 ships
  lost/y, reserves ~60 y; average expedition EV -0.26 M of 1.03 M cost; six
  artifacts (one fraud); identity of the Lotus = DISPUTED (NIRVANA-1 fate is
  UNKNOWN to the author). Other frontiers: the Chest (9 Ceti, resource), the
  Monastery (HD 93083, science).
- Runtime (built RelWithDebInfo, MSYS2 MinGW64): `WorldSitesCatalog.h`
  (loaded with the StarAtlas by ClientCatalogService, presentation only),
  `ClientWorldSitesMapBridge.h`: System Map adds system-level sites as
  infrastructure markers (no Hub-map drill; bound runtime hub skipped);
  Detail Map adds the body's sites (surface ones at lat/lon in the planet
  texture frame, small glyph, no velocity arrow); Galaxy Map colours
  deep-space sites and shows them from the overview level.
- Evidence: offline checks PASS (world_economy 14/14, world_sites 15/15,
  logistics incl. passports, calibrate_population, lab --check-dataset /
  --check-lore, lore_md --check 129 files, lore_html --check 132 files); two
  lab runs and two world_economy + world_sites runs byte-identical (lab sha
  1895d762...); EliteDockTrafficCheck 6 h: 78 arrivals, 0 violations, min gap
  38.6 m (unchanged). Live game (nav-stress:simple, captures
  D:/tmp/elite_ws_*.png): Earth Detail map shows the agglomerations on land
  (East Asia, Japan, Korea, US west coast) as small markers - GAME VERIFIED;
  System Map / Galaxy Map markers present but not distinguishable at the
  scripted overview zoom - NOT visually verified. Architecture contracts for
  maps PASS; `tests/system_map/check_architecture.py` red on
  SystemMapRenderer.cpp (file not touched; pre-existing).
- Limits: orbit sites use a presentation phase law (1-day period); the server
  does not read sites yet; climate-only lat/lon for non-Earth bodies (terrain
  sampling = Stage 6); Earth agglomeration names = historical regions
  (author decision); epidemics / quarantine only as schemas (Stage 7).
- Open author decisions: Lotus = NIRVANA-1 or not (kept DISPUTED); lotosite
  (+3 ly hop) as a strategic resource; names of Earth's 3126 agglomerations;
  defence posture norms (squadron 4, 0.5 posture per key node).

### World's Ass rework: Developer World Truth, Lotus = NIRVANA-1, causal lotusite model (user orders 2026-11-03, third pass b)
Status: offline model + data + Lore; PROPOSAL (Lotus = NIRVANA-1 is internal
CANON by the author's decision; +3 ly = AUTHOR_TARGET). NOT user-accepted.
- Developer World Truth vs world knowledge split: `canon/world_sites.json ->
  lotus.truth` (route, 2680 fold accident, trajectory, crew cohorts, last human
  2712, systems, messages incl. which arrived, animals, discovery 3091 by Kern
  "Sirin", cover-up and leak 3103) -> `Notes/Lore/AUTHOR/WORLDS_ASS_TRUTH.md`;
  public layer `Notes/Lore/FRONTIER/WORLDS_ASS.md`.
- Canon check: the first draft (fold 2341) contradicted fate.md / reconciled
  canon (signals 2550 / 2650, "Do not look for us" 2680, 2 100 aboard) ->
  fixed to 2680; ART-0006 "cryo pods" contradicted "no cryo" -> infirmary stasis
  pods; legacy loss point HD 104067 kept: the field lies 21 ly beyond it.
  missions.json NIRVANA-1 fate -> derelict_found (author_knows), knowledge.py
  AUTHOR_TRUTH, unknown removed; COLONIZATION_CANON note added.
- Biology (fictional anomalous): combined profile uricase loss + GULO loss +
  CMAH loss (none unique alone); species table (dogs / cats / apes / guinea
  pig / ferret not affected); 5-stage field syndrome; phenomena registry with
  developer category physical / sensor / subjective.
- `lotus_model.py` (causal): finite primary stock by crew cohorts (upper
  estimate ~1.6 kg recoverable, ~0.96 kg left), medical + jump demand, free
  entry raids capped by physical hulk extraction (~80 bodies/y), price
  ~0.22 bn Cr/g, ~190 raids/y, ~43 ships lost/y, ~26 y to exhaustion,
  trafficking ~52 victims/y from incentives and the hidden "Garden" capacity,
  quarantine capacity, corruption pressure by role and zone (outer ~13-34 %,
  grey ~40-69 %, inner ~61-84 %), non-linear blockade (least smuggling at
  strictness 0.4, actual 0.6), lotus fever outbreaks from population and beds
  (Mandala 3093, Stella 3115, Pereval 3121 via a bribed inspector), +3 ly map
  effect (17 new beacon edges, GJ 1214 and 9 Ceti reachable, no hostile pair
  brought into one jump), six security units (lawful .. piracy in uniform).
  Previous numbers (9 M Cr/kg, 534 expeditions) withdrawn.
- Three zones (outer perimeter HD 104067 "Vorota", grey frontier Far Berth /
  picket / HD 102195, inner field Lotus / Garden / Dead Reckoning); cordon =
  Cognium + Kern (Cognium doctrine gains `quarantine`).
- Checks: world_sites 21/21 OK, world_economy OK, lab dataset / lore OK,
  lore_md 130 files OK, lore_html 133 OK.

### Industrial geography + rescue services (user order 2026-11-03, third pass c)
Status: offline model + data + Lore; PROPOSAL, NOT user-accepted.
- `canon/industry.json` + `industry.py` -> out/world_3126/industry_3126.json,
  rescue_3126.json, INDUSTRY_3126.md; Notes/Economy/INDUSTRY_AND_RESCUE.md;
  Lore ECONOMY/INDUSTRY.md, ECONOMY/RESCUE_SERVICES.md.
- 8 blocs (Cognium pool ~ West, Heavenly Pact ~ USSR/China, Lalande League ~
  Europe, Southern Circle ~ non-aligned with Mecca's own cores, Citadel
  Compact, Free Belts, Sol, outside blocs) from founders' cultures, jump-core
  lineage and grievances; inter-bloc channels open / licensed / restricted /
  black; design schools (modular premium, standard monohull, many-vendor
  modular, licensed kits, heavy military, hollowed asteroids and refits, mass
  production); 19 named makers incl. the runtime ones (Cobretty Corporation ->
  Second Hellas, Orbital Freight Union "Ural-Kourou" -> Sol); per polity and
  per block (10 ship, 4 mining, 3 gas, 8 station) concern / assembly / parts /
  import; who supplies whom through which channel; per-site production /
  assembly / sales; major-block BOMs; fleet uniformity (Pact main hull maker
  74 %, League 50 %). Jump cores only from Cognium, Mecca, Citadel (Citadel's
  never exported); Sol's own cores stay UNKNOWN (author). New Karma (refuses
  the network): in-system fleet only.
- world_economy.shipyards: yard-less polities buy from their bloc, then their
  main trade partner, then the nearest polity with yards (refusers never
  supply).
- Rescue: SAR, search, fire / damage control, medevac, towing, hazmat,
  disaster; organisation by legal family; counts from defence tasks and
  population; response time to every site (hours near the polity's bodies,
  ~half a day in belts with a patrol base, ~3 days otherwise; Lotus field only
  Kern SAR "Laska"; pirate enclaves rescue themselves).
- Checks: industry 7/7, world_sites 21/21, world_economy, logistics,
  calibrate_population, lab dataset / lore, lore_md 132, lore_html 135 OK;
  industry / sites runs byte-identical.

### Fourth pass (user order 2026-11-03, after 7ec5f4c1): lotusite price, Sol network, System Map, Orbital Transfer
Restore point: tag `restore/pre-sol-network-2026-11-03` (= 7ec5f4c1). Branch main.

#### Stage A audit (source-audited, 2026-11-03)
- System Map: one pointer resolver (hub > body > cube marker > child cube on a
  hover-layer plane); no arrow / Tab keys; cube frame colours borrowed from
  the Galaxy defaults (`GalaxyNavigationGridVisualSettings`, the System struct
  `SystemNavigationGridVisualSettings` is dead code); no toggle to hide cubes;
  world-site stations drawn with the ship triangle glyph (tacticalGlyphKind maps
  Station -> Infrastructure); no hover tooltip on objects; info cards 238 px,
  titles / values neither wrapped nor clipped; right panel (28 %) shows only
  the selected system; no beacon links on the System Map.
- `tests/system_map/check_architecture.py` red root cause: commit 97b39d73
  (map history "back arrow", 2026-10-31) assigns `m_detailView.camera()` /
  `m_hubView.camera()` directly in `SystemMapRendererContextMenu.cpp`
  (applyMapHistoryRestore) -> real rule violation, not a stale test.
  `check_object_overlay.py` red: stale token `clickedBodyId` (renamed to
  `selectedBodyId` in the same commit).
- Orbital motion: circular Kepler orbits (`OrbitalMotion`), hub world velocity =
  parent + orbit velocity, replicated with the full motion; several hubs are
  structurally supported (`m_orbitalHubs` map) but data has one and many
  places hard-code `earth_orbital_hub` / `hub_hex_dock_a`.
- Gravity: `GravityFieldSystem` result is navigation metadata only; the only
  stepped player motion is `HubTactical` in the hub's LVLH frame (no gravity,
  no fictitious forces) -> a ship with thrust off co-orbits with the hub
  forever; it cannot fall. Real gravity integration exists only in
  diagnostics (`PassiveTrajectory`, velocity-Verlet). `TravelFrameSystem::detach`
  and `KinematicFrame::rebaseLocalKinematics` are unused: there is no
  hub-tactical <-> system-travel transition. `maxCruiseSpeed` (0.1c) is unused.
- Navigation v2: routes are planned in one hub frame to fixed goals; terminal
  matching is scalar speed only; `NavigationArrivalProfile.matchVelocity` has
  no executor; the only executed route is docking.
- DockServiceTraffic: one instance, hard-coded facility `earth_orbital_hub.hex_dock_a`,
  "Earth Orbital Hub" strings, ORB-/CBR- callsigns, flight id EOH-n; geometry
  from C++ constants (6 pads); no ETA, no multi-hub identity.

#### Stage 1: lotusite price and technical role (offline; PROPOSAL)
- Author rejected ~219 M Cr/g and +3 ly. Removed: `jump_component`,
  `max_hop_bonus_ly`, `hop_bonus_effect`, mass `jump_g_y`, the map effect
  (17 edges, GJ 1214 / 9 Ceti reachability) everywhere (model, checks, Lore,
  Notes/Economy).
- New `lotus.phase_stabilizer`: insert 0.3 / 0.6 / 1.2 g in the emitter of
  Cognium G4+ cores; holds the jump field in jump-dead (only the outer 15 %),
  distortion (exit error x25 -> x6), unstable navigation (misjump 0.12 ->
  0.035) and the Lotus field; emergency exit with 30 % irreversible damage;
  not consumed, degrades only under heavy anomalous exposure; no effect in
  normal space. Demand from real users (cordon units in the field, SAR,
  research, anomaly patrols, licensed expeditions): ~24 ships, ~9 g installed,
  ~0.9 g/y; each fit decided by avoided hull loss vs ownership cost.
- Demand: medical = Pareto income tail of every polity (economy_3126 GDP per
  capita) x access (prohibition, taboo, distance from the Kern pocket);
  research = budgets (unit elastic); stabilisers as above. The ONLY parameter
  fitted to the AUTHOR_TARGET is `demand.medical.access_base` (0.0005).
- Supply: each raid segment enters on the economics it BELIEVES (rumoured
  outer-perimeter price, optimistic corridor), the hulk gives ~80 bodies/y and
  those who know the open compartments take them first (connected -> map ->
  blind); connected crews capped at 15 runs/y by bought pickets. Fixed two
  latent bugs: illegal-raid "insurance" double-counted hull risk (removed);
  charge / fuel keys did not match `trip_cost` (was always 0).
- Result (not canon): price ~3.11 M Cr/g (~0.92 Cobrett-I), course 0.12 g ~373 k
  Cr, body ~2.25 M Cr retail, ~1 300 medical buyers (mostly Sol), ~46 g/y
  refined, ~60 raids/y (15 connected, ~32 blind, ~13 cover-story), ~12.5 ships
  lost/y, primary stock exhausted in ~21 y; trafficking now ~0 (margin 0.83:
  while NIRVANA-1 bodies last, live raw material does not pay), after
  exhaustion ~46 M Cr/g and ~50 victims/y; corruption expected share outer
  17-40 %, grey 47-75 %, inner 68-88 %; blockade least smuggling at
  strictness 0.4 (actual 0.6); bribe pool ~3 M Cr/y.
- Checks: world_sites 25/25 OK (new: price in AUTHOR_TARGET, no jump-range
  effect, stabiliser demand small, medical main demand, networks self-source
  cheaper than buying < map-buyers); lore_md 132 OK; lore_html rebuilt.

#### Stage 2 (B): Sol hub network - world data (offline + runtime data; PROPOSAL)
- `canon/sol_network.json` (authored: names, history, owners, orbits, rules) +
  `sol_network.py` (derived: populations, traffic, docks, utilisation, routes,
  relays, checks); world_sites.py replaces the generic Sol hubs with it.
  `earth_orbital_hub` moved from world_sites named_sites into sol_network (one
  source; founded 3108 on the Cordon station site kept from the old record).
- Earth: 12 hub complexes (AUTHOR_TARGET) in 11 planes: Vysokiy 51.64 (gateway,
  player start), Baikonur 51.64 (opposite phase), Meridian 56.5 (the only plane
  Moscow reaches directly), Tikhiy 36.2 (Tokyo), Tanegashima 31.6, Canaveral 29,
  Tropic 24.5, Wenchang 19.8, Sriharikota 14.8, Kourou 6, Ekvator (GEO), Aurora
  (SSO 98.6). Surface traffic 398.4 M legs/y (~1.09 M/day, ~13.5 k SL-12
  flights/day) split per agglomeration over the planes it reaches directly
  (i >= |lat|, logit on the lost Earth-rotation boost); rest of Earth follows
  the agglomerations' latitude mix; no fixed city -> hub binding.
- Dock semantics: dock_sector = runtime 6-pad HexDock; hubs get
  `dock_sectors` x 6 pads sized to mean utilisation ~0.62 (peak x1.35); only
  Vysokiy's first sector is visible in runtime (presentation limit recorded).
- Other Sol: 31 hubs at Moon / EML1 / EML4 / EML5 / SEL2, Mars (Phobos, Ares,
  New Tharsis, Hellas, Deimos), Mercury, Venus (aerostat science), Belt
  (Ceres, Ahuna, Vesta, Pallas, Hygiea + the Hilda pirate enclave, rumour only),
  Jupiter, Saturn, Uranus, Neptune; counts checked against drivers (population,
  mines, gas platforms, research, military). Belt hubs stand at persistent
  anchors with real orbital elements (Ceres, Vesta, Pallas, Hygiea are not in
  the physical catalog). The two generic mining hubs at 246.10 / 245.17 deg
  were adjacent by id hash -> merged into the Ceres cluster; 26 generic ids
  mapped once as legacy aliases.
- 82 aggregate routes with persistent ids (Earth hub<->hub shuttles,
  interplanetary passenger / cargo / gas, belt ore tugs, Moon shuttles), 19
  local nav relays (2 per inhabited region + dense-node extra + SE L4/L5
  conjunction relays).
- Runtime data: initial_world_state.json orbital_hubs is now GENERATED (12 Earth
  hubs + "Pereval Tserery" co-orbiting the Ceres anchor, parent = the Sun, one
  command_station model family); sites_3126.json gains hub details, routes,
  relays, belt anchors and per-system summaries (state from events / epidemics /
  shortages, never a hand-written status).
- Found, not changed: OrbitalMotion's circle runs +X -> +Z (retrograde against the
  catalog's prograde convention) and is relative to the parent's ecliptic-aligned
  frame, not the Earth equator; the Ceres hub compensates with i = 173.9 deg.
- Checks: world_sites 38/38 (13 new sol.*), industry, world_economy, logistics,
  lab dataset / lore, lore_md 133 (new ECONOMY/SOL_NETWORK.md), lore_html 136
  OK; sol / runtime outputs byte-identical across runs.

#### Stage 3 (E): Orbital Transfer - planner + server mode (implemented; offline PASS, live partially verified; NOT user-accepted)
- `src/game/navigation/orbital/OrbitalTransferPlanner.{h,cpp}` (lib
  `EliteOrbitalTransfer`, linked into EliteNavigationWorldRuntime): plan in the
  central body's frame from the ship's actual state to the destination hub's
  approach point (15 km behind on -prograde, closing 20 m/s) AT THE ARRIVAL
  EPOCH with the hub's velocity then (moving target from `OrbitalMotion`);
  departure / arrival ramps (0.4 g inside 25 km), a nearly Keplerian phasing
  chain (radius blend start -> phasing radius -> end, angle at the local
  circular rate, plane change done where the orbit is high), minimum safe
  altitude (catalog atmosphere top + 150 km), smallest arrival epoch within
  the thrust envelope (min(18 m/s2, 0.25 x main engine)) and a delta-v budget
  25 km/s (PROPOSAL). Carrier law: feed-forward + real gravity + PD, clamped
  to the engine.
- Server (`GameSimulationOrbitalTransfer.cpp`): `ClientShipCommand::
  RequestOrbitalTransfer{target}` / `CancelOrbitalTransfer` (wire v19). The
  ship's travel frame is detached from the source hub and its origin driven
  by the carrier on the orbital clock; the ordinary tactical integrator keeps
  running inside it (maxCombatSpeed untouched). At the arrival epoch the ship
  is re-based into the destination hub frame with its actual relative state.
  Other body (Earth -> Ceres): ABSTRACTED transit (brachistochrone reported,
  90 s real, placed at the approach point with the hub's velocity).
  Normative note added to NAVIGATION_V2_EXECUTION_ARCHITECTURE.md section 11a.
- Client: map context menu "Орбитальный перелёт" on a hub (or O with a hub
  selected) previews the tunnel with the same planner (Earth state recovered
  from the source hub's replicated ephemeris) and sends the command.
- Evidence:
  - OFFLINE `EliteOrbitalTransferCheck` PASS: Vysokiy -> 10 LEO hubs at two
    epochs: ETA 1.9-2.9 h, path 46-75 thousand km, peak 7.7-8.3 km/s vs Earth
    and 5.6-15.7 km/s vs destination, dv 9.6-25 km/s, min altitude >= 251 km,
    arrival position error 0.8-2.5 m at the approach point, arrival relative
    speed 20.0 m/s (= the planned closing speed); coasting with thrust off
    stays in orbit (419.8-420.2 km); a 300 m/s retro burn falls below 100 km
    after 1029 s. OPEN: Vysokiy -> Ekvator (GEO) infeasible within the budget
    (no Hohmann / spiral optimisation yet).
  - LIVE (RelWithDebInfo, nav-stress:simple, `ELITE_DEV_SOL_MAP_SCRIPT`,
    stderr D:/tmp/elite_galaxy_capture_stderr.txt): Vysokiy -> Baikonur
    started (server ETA 11 196 s, dv 12.0 km/s, min alt 391.6 km); server log
    t+0..360 s: departure ramp 3.92 m/s2 for 113 s, then 5-9 m/s2, tracking
    error <= 1 m; at t+335 s the ship is 102 km from Vysokiy at 1 268 m/s
    relative (above the tactical 500 m/s). Arrival at Baikonur NOT observed
    live (3.1 h). Vysokiy -> Pereval Tserery (abstracted, physical 5.5 d at
    9.2 m/s2) handed over: 15 km offset, 20 m/s relative, matched to the
    Ceres hub frame - LIVE VERIFIED.
- Answers (section 19): with thrust off outside a transfer the ship does NOT
  orbit or fall - Hub Tactical carries it in the hub frame, gravity is
  metadata; inside a transfer the carrier IS gravitational (coasts / falls,
  offline-proved). The tactical -> system transition is the new detach in
  requestOrbitalTransfer; the return is handOverToHub.
- Known debt: ship attitude not slaved to carrier thrust; abstracted transit
  leaves the ship's world pose stale for the 90 s; a cancelled transfer that
  reaches the atmosphere stops updating its frame; no free gravitational
  flight outside transfers.

#### Stage 4 (C): hybrid System Map (implemented; scripted + live captures; human acceptance OPEN)
- Cubes stop being the user's model: wireframe = optional overlay (G, hidden
  by default, System-specific quieter colours); cubes are pick targets only
  with the overlay; drawn hubs / bodies are targets anywhere (removed the
  cube-containment rule for hub picks and the cube culling of bodies); arrows
  move a selected child cube to a sibling in the screen plane locked at the
  first press (1.5 s lock), never "into the screen".
- World objects: hubs use the hub glyph (world sites amber, replicated hubs
  cyan), relays a diamond, belt anchors a disc; far-zoom declutter (objects on
  a planet's or another marker's pixel appear as the region grows); hover
  tooltip (name, operator, type) clamped to the viewport; replicated hubs
  take name / operator / type from the world model.
- Right panel "КАРТА СИСТЕМЫ" (`SystemMapInfoBuilder.h`): nothing selected ->
  summary from the world model (actors, economy, imports / exports /
  shortages, state from events / epidemics / hazards, relation to the player
  "neutral, no records" until reputation exists, hubs BY REGION); hub / site /
  relay / anchor / body selected -> its facts and routes. Word-wrapped,
  clipped (scissor), scrollable, >= 15 px text; the system list moves under
  the information and opens upwards.
- Network routes: 82 aggregate routes drawn dashed (width / alpha by
  flights/day, T toggles) - distinct from the solid Orbital Transfer tunnel.
- Data: WorldSitesCatalog parses hub facts, routes, relays (placed), belt
  anchors (heliocentric, Kepler like the planets), Lagrange points (L1/L2/L4/
  L5 with Hill ratio) and system summaries.
- `tests/system_map/check_architecture.py` GREEN: root cause fixed in code
  (`DetailMapView::restoreCamera` / `HubMapView::restoreCamera` instead of
  assigning the views' cameras from the facade); `check_object_overlay.py`
  stale token updated (`clickedBodyId` -> `selectedBodyId`).
- Evidence: `tests/system_map/check_hybrid_map.py` PASS (new static
  contract); `EliteSystemMapInfoCheck` PASS (new, on the runtime data: the
  summary names "Перевал Цереры" and the Earth hubs without ids; Ceres / Тихий
  / Earth panels; 13 runtime hubs bound once; all 82 route ends real; 19
  relays placed; 4 anchors; Ceres hub 2.80 AU) - the check found and fixed a
  real discoverability bug (summary listed only the 12 busiest = Earth hubs).
  Live captures D:/tmp/elite_sol*_*.png: overview + panel summary, Earth
  approach (cislunar hubs, dashed routes to Venus / Mars / outer), LEO zoom
  (hub orbits, relays), Baikonur / Ceres panels, hover tooltip, grid toggle.
- NOT done / limits: primary view stays 3D perspective (hybrid = optional
  cubes, no 2D ecliptic lock); clicking a route line opens nothing (routes
  are listed in the hub panel); Detail Map has no zoom-to-lat/lon for a
  selected settlement; relation to the player is a placeholder until
  reputation / warrants exist; human mouse acceptance of the new map OPEN.

#### Stage 5 (D): generic dock traffic (implemented; offline PASS)
- `TrafficConfig` carries hub id / display name / dock facility / frame /
  system, callsign + flight namespaces, surface name, connected destinations
  and the surface / hub mix; no hub name, facility id or "ORB-/EOH-/Earth"
  literal remains in DockServiceTraffic; flights keep one persistent serial
  (no parsing of the callsign), manifests carry origin, destination,
  category and ETA of the aggregated far part. The game fills the config
  from the hub record and the world-model routes: "Высокий",
  earth_orbital_hub.hex_dock_a, namespace VYS, 11 connected destinations,
  shuttle interval 1 550 s = the hub's modelled arrivals per dock sector.
- Evidence: EliteDockTrafficCheck 6 h, 4 profiles (cobretts/interval 4/600,
  4/1549, 9/600, 6/300): 78 / 71 / 73 / 72 arrivals, 0 invariant / separation
  / structure violations, min gap 38.6 m; queue max 0 / 0 / 7 / 6.
- NOT done: physical dock traffic exists only where a dock sector is spawned
  (Vysokiy); other hubs have modelled traffic (panel, routes) but no
  materialised ships; hub A -> hub B flights are not yet one cross-hub
  schedule object.

#### Verification summary of the fourth pass
- Offline: world_sites 38/38 OK (13 sol.*), world_economy, industry,
  logistics, lab dataset / lore, lore_md 133, lore_html 136, Sol / runtime
  outputs byte-identical; EliteOrbitalTransferCheck PASS (LEO) + 2 OPEN (GEO);
  EliteSystemMapInfoCheck PASS; map tests 5/5 PASS; EliteDockTrafficCheck 4x6 h
  PASS. Python contract suites: 11 reds, all identical at the restore point
  (pre-existing, unchanged by this pass).
- Live: listed above. Human acceptance (find hubs without ids, read the
  panel, fly hub -> hub to arrival and dock) is the user's.

### Fifth pass (user order 2026-11-03, refinement): major-hub budget, 2.5D map, one transfer architecture
Restore point: tag `restore/pre-sol-major-hubs-2026-11-03` (= 2eb47aad). Lotusite part of
the order was already done in the fourth pass (cc56fd59) and is unchanged.

#### Stage B-D: major-hub budget (offline + runtime data; PROPOSAL)
- Rule: infrastructure capacity != number of map objects. Levels: A major hub
  (System Map), B sector / terminal / merged hub of a complex (data, panel,
  Detail / docking), C local facility / settlement (Detail Map or near the player).
- Earth: 12 stations -> 7 complexes (Vysokiy gateway, Tikhiy mass passenger,
  Canaveral 31.6 deg passenger + Mars, Wenchang cargo, Kourou lunar /
  interplanetary, Aurora polar SAR / science + Meridian terminal for Moscow,
  Ekvator GEO industry / repair); Baikonur / Tanegashima / Tropic /
  Sriharikota / Meridian are named terminals. Dock model hub -> sector (ring of
  <= 12 HexDock modules) -> module (6 pads): Tikhiy 12 sectors / 141 modules /
  846 pads ... Ekvator 1 / 1 / 6; Earth 57 k movements/day vs 92 k capacity
  (utilisation 0.62, reserve 0.38), terminals and berths per complex.
- Sol major hubs 29 / 30: Earth 7, cislunar 3, deep space 1, Mars 3, Venus 1,
  Mercury 1, Belt 5 (incl. the pirate enclave, hidden until known), Jupiter 4,
  Saturn 2, outer 2. Settlements and small stations (Shackleton, Imbrium,
  New Tharsis, Hellas, Ahuna, Kaloris, Europa / Enceladus science, quarantine
  L5) are level C.
- Other systems: development tier from population (developed >= 800 M,
  colony >= 100 M, periphery > 0, frontier) with budgets 10-18 / 4-10 / 1-5 /
  0-3; extra same-kind hubs merged into one complex as level B (capacity kept).
  Result: developed max 11, colony max 6, periphery 2, frontier 2; some
  developed systems are below the lower bound (Tau Ceti 4) - not padded.
- Checks: world_sites incl. map_complexity_within_tier_budgets,
  sol.sol_major_hubs_within_budget, region budgets, Earth 6-8,
  earth_capacity_ge_demand_plus_reserve - all OK; industry, economy, dataset,
  lore_md 133, lore_html 136 OK. Commit da09a65e.

#### Stage E-H: map 2.5D, clusters, one transfer architecture (implemented; scripted + live; human acceptance OPEN)
- Plan view (default): fixed tilt over the ecliptic (1.30 rad), only azimuth
  rotates, height stems to the parent's / ecliptic plane; V -> free 3D.
  Clusters: hubs hidden on a planet's pixel gather into one marker with a
  count ("Земля: N крупных хабов"; one hub counted once). Relays and dotted
  beacon links only with the N filter. Body panel: major hubs listed,
  settlements / local facilities counted (Detail Map). Hub panel: sectors /
  HexDock modules / pads, passenger / cargo terminals, service berths, named
  terminals.
- Transfer: Earth -> Ceres now uses the same plan type / mode / executor /
  hand-over in compressed time (`planCompressedTransit`, frame follows a
  bowed star-relative tunnel, arrival at the destination's approach point at
  the real epoch, physical brachistochrone reported). While a transfer runs
  the server executes no pilot / autopilot translation inside the frame (the
  nav-stress autopilot had pushed 500 m/s of local drift in a live run).
- Dock traffic interval per visible HexDock MODULE (Vysokiy 1 535 s).
- Evidence: map python tests 5/5 PASS (check_hybrid_map extended: plan view,
  beacons filter, stems); EliteSystemMapInfoCheck PASS (Sol major hubs drawn
  29 / 30, Earth 7, Vysokiy panel shows its terminals, Earth panel lists major
  hubs + counts settlements); EliteOrbitalTransferCheck PASS (6 LEO complexes x
  2 epochs; compressed Vysokiy -> Pereval Tserery: tunnel 3.92 AU, closest to
  the Sun 1.00 AU, physical 4.18 d, arrival 15.0 km / 20.0 m/s) + 2 OPEN (GEO);
  EliteDockTrafficCheck 6 h (4/1534 s, 9/600 s): 76 / 73 arrivals, 0
  violations, min gap 38.6 m; world_sites / lore_md / lore_html OK. LIVE
  (captures no longer exist - deleted by a D:/tmp cleanup error 2026-11-03;
  observations recorded here only): plan-view Earth with its
  complexes, transfer tunnel to Tikhiy around Earth, Ceres tunnel with the
  ship mid-way, arrival at Pereval Tserery 15.0 km / 20.0 m/s matched, cluster
  counts on the overview. Live arrival of a LEO transfer (~2.8 h) still not
  observed; human acceptance OPEN.

### Sixth pass (user order 2026-11-03): hub / beacon list, neutral beacon names, Ctrl+J free flight
Restore point: `restore/pre-hublist-freeflight-2026-11-03` (= ac71cfb6).

#### Map list (implemented; scripted + live; human acceptance OPEN)
- System / Detail map: the star-system dropdown is gone (it stays on the
  Galaxy map); in its place "Хабы и маяки" - every hub and beacon the System
  Map draws, grouped by planet (moons and Lagrange points with their planet,
  the belt as one group) in order of distance from the star, then the beacons;
  objects unknown to the player (map level none) are not listed.
  `buildSystemMapObjectList` (SystemMapInfoBuilder.h); row ids = marker ids
  (runtime hub id or site:<id>). A row = the click on the marker:
  `SystemMapRenderer::focusObjectFromList` selects it (selectedHubId, overlay
  activation -> panel facts), keeps it drawn through declutter / clusters /
  the beacon filter, flies the camera to it (planet neighbourhood framed;
  heliocentric objects keep the zoom).
- Beacons renamed to neutral codes (region + number: ЗЛ.1-4, МР.1-2, ПС.1-3,
  МК.1, ВН.1, ЮП.1-3, СТ.1-2, ВШ.1-2, СЗ.L4/L5) in canon/sol_network.json;
  check `sol.relay_names_neutral_codes` 19/19.
- Evidence: EliteSystemMapInfoCheck PASS (list 28 hubs + 19 beacons in 10
  groups, ids = marker ids, the pirate haven not listed, no person names);
  check_hybrid_map extended, map python tests 5/5 PASS; world_sites --check OK,
  lore_md / lore_html regenerated. LIVE (D:/tmp/elite_list1..5): grouped list
  open (Меркурий first, 47 objects), pick Рейсильвия -> panel facts + ringed
  marker centred, pick Маяк ПС.2 -> drawn with the beacon filter off + facts,
  pick Тихий -> camera at Earth (500 000 km scale) + facts.

#### Ctrl+J free flight (implemented; live LEO evidence; human acceptance OPEN)
- Contract: NAVIGATION_V2_EXECUTION_ARCHITECTURE.md §11b. Detached travel
  frame carries the global (system / galaxy reference) motion under the
  gravity of all bodies; local motion folded into it each tick (no envelope);
  Assisted +/- = direct main thrust; Backspace free rotation = no main thrust,
  its exit aligns with the GLOBAL velocity; attitude rates capped at 0.10 rad/s
  under thrust (PROPOSAL); off -> nearest hub frame with actual relative
  kinematics. Held J (cruise placeholder) unchanged; Ctrl+J excluded from it.
  WireProtocolVersion 20, snapshot schema 23 (motion.freeFlight).
- LIVE (no nav-stress autopilot, ELITE_ARGS=--dev-direct-flight):
  Newtonian-I at Vysokiy: coast 150 s -> altitude 422.63-422.65 km (free
  orbit under Earth gravity), 20 s '+' -> 12 -> 736 m/s vs the hub (36 m/s2),
  HUD "J НЬЮТОНОВСКИЙ 24.11 km/s" (global); off -> handed over to Vysokiy at
  734 m/s, 3.6 km. Assisted-I: 15 s '+' -> 1 103 m/s vs the hub, 20 s coast
  kept 1 103 m/s; off -> Vysokiy at 1 103 m/s, then Assisted brakes toward
  its envelope (938 m/s 3 s later).
- Contracts: the python contract loop is line-for-line identical to the
  restore point (no new reds).

#### J corrections after the user's live review (restore/pre-j-fix-2026-11-03)
- User report: on Ctrl+J the ship was inside the station; the velocity vector
  pointed anywhere; wanted: speed relative to the hub (starts ~0), a banner
  "speed no longer local", HOME full thrust / END brake / INSERT thrust off,
  limited manoeuvres.
- Root cause (inside the station): SceneRenderer placed hub-attached objects
  and co-hub ships in the PLAYER's reference frame whenever the hub id matched;
  after detach the frame origin is the ship, so the station was drawn around
  the ship (the same applied during Orbital Transfer). FIX: only a matched
  frame resolves hub-local presentation (HubFramePresentation.h; contract test
  extended).
- Implemented: §11b updated - velocity reference = the hub left (replicated),
  HUD vector / speed / Backspace exit / brake on v - v_ref; HOME / END / INSERT
  latched thrust through the ordinary main command and alignment controller
  (`freeFlightMainCommand`, gateway); keypad only with the engine idle; turn
  rate omega = 3 m/s3 / |a_thrust| (Cobra 0.082 rad/s) instead of 0.10;
  capture envelope 50 km + safe speed 500 m/s on Ctrl+J OFF; HUD banner.
  Wire protocol 21, snapshot schema 24.
- LIVE (D:/tmp/elite_jfix_run1, run2): station stays 10 km away on entry;
  Newtonian-I HOME 36.8 m/s2 -> 703 m/s vs Vysokiy in ~19 s; OFF at 737 m/s
  -> UNSAFE capture refused, stays in J; END flip + burn 737 -> 1.7 m/s, thrust
  off by itself; OFF at 10 km / ~2 m/s -> captured. Assisted-I: HOME latched +
  Backspace ON -> 0 thrust (735.8 m/s held); Backspace OFF -> alignment then
  thrust resumes at the full 72 m/s2 (nose along the relative velocity).
  The HOME burn aimed at the station passed ~1.5 km from it at 700 m/s: no
  station collision yet.
- Not verified live: the turn-rate value itself; Newtonian Backspace (Assisted
  only in the input); OFF with no hub within 50 km.
- Radar: the cockpit radar HUD was disabled (`RuntimeFeatureFlags.h`
  RadarHudEnabled = false) - superseded 2026-11-04 e: the scope now draws the
  session-private scan (see that section).
- Pending order (user, 2026-11-03, 60 sections): unify free flight, global
  autopilot, hub transfers and real ship components into one physics; it is
  written for main AFTER a generic ship / ShipDescriptor / equipment pass that
  is not on main - not started.

### Ships pass (user order 2026-11-04): 17 class base models + justified variants, passports, equipment passports, drawings, functional schematics
Restore point: `restore/pre-ships-2026-11-03`. Scope (user): one full canonical base model per fleet
class, variants only with a real design / lore reason; each a full passport with a runtime-compatible
descriptor / loadout; primitive placeholder geometry allowed.

- Data (authored): `tools/colonization_lab/canon/ship_designs.json` - 120 equipment products
  (drives, propulsion / service reactors, radiators, coolant loops, jump cores, tanks, RCS, MotherComputers,
  radars, comms, transponders, ECM, life support, capsules / lifeboats / refuges, medbays, C20 / P20 /
  habitat / cold-sleep / train / cryo modules, docks, bays, hangars, tow rigs, workshop, heat shield,
  landing gear, weapons, armour) by the industry.json manufacturers; 21 ship models = 17 bases + 4
  variants (Cobrett-I A3 Assisted - existing runtime; GT-3000 gas tanker - separate economic class;
  Harmony He-120 - standard-monohull school / allied_fleets_uniform; Stone Docks "Kamen" hollowed-asteroid
  tanker - Free Belts lore). Manufacturer mapping: Cobretty (Cobrett), OFU (OCS-4, SL-12), Bharat (C-120,
  T-5000), Harmony (train, tanker, He-120, frigate, fleet tender), Sol Lunar-Martian Yards (gas tanker,
  service tender, patrol cutter, PAO tug), Kern (ore tug), Bastion (interceptor, line ship), Hyperion
  (recon, liner).
- Tool: `ship_designer.py` (+ `--check`): layout fill (drives per I/Y/X main slot, one generator per
  Newtonian main / Assisted pair), mass budget (hull structure = dry - equipment; possible 15-90 %, typical
  30-75 % WARN, asteroid form own bounds), thrust / acceleration vs class, group power, hotel power,
  RCS from propulsion spare (30 % simultaneous), universe thermal law (6 MW/m2 pads, x10 radiation; drive
  emitter diameter and sustainable thrust fraction derived), jump core rating vs gross mass, charge,
  life support capacity / endurance, survival seats for everyone awake, C20 / passenger places, supplier
  reachability through bloc channels (weapons / cores only open or licensed), price by component vs the
  economics model x school (WARN), functional schematic (power propulsion / service, coolant, fuel, data,
  life), functions -> required units, single points of failure, redundancy doctrine (passenger /
  security / military: thrust, life, comms without SPOF - FAIL otherwise; the sealed jump core is a
  factory item and excluded), repair doctrine per category (crew / station / yard / factory, LRU,
  hours). Result: 21 ships, 0 FAIL, 39 WARN (price deviations from the economics estimate, truss
  freighters' structure share > 75 %, A3 = runtime fact 19.1 MN aft).
- Outputs: `out/ships_3126/` (ship_designs_3126.json, SHIP_DESIGNS_3126.md, svg/<id>.svg top/side/front
  drawing, svg/<id>.schematic.svg, index.html catalog page); lore `Notes/Lore/ECONOMY/FLEET_MODELS.md`,
  `SHIP_EQUIPMENT.md` (lore_ships.py, registered in lore_md; lore_md --check OK); runtime copy
  `src/assets/data/ships/ship_catalog_3126.json` (GENERATED).
- Runtime: `ShipDesignCatalog.h` (loader) + `GenericShipDescriptorBuilder.h`: ONE builder makes a
  ShipDescriptor from any design - one damageable module per unit (bulk positions - armour, fluids,
  radiator field, C20 rails - one module each), EngineUnitDescriptor / EngineReactorDescriptor from the
  product ratings, loadout on the fixed physical topology (CobraPropulsionPresets), generator groups,
  ship services, rigid body (box inertia), flight envelope (crew load by class, size-scaled rates,
  500 m/s tactical envelope), radiators, payload, survival, bays. `EliteShipCatalogCheck`: PASS - 21
  descriptors build, modules = installed units, slots / groups / services bound, forward accel = design
  thrust / dry mass, schematic nodes are modules, sealed cores not field-repairable, 18 economic classes.
- Not done (next): non-Cobra ships are NOT yet flyable - ShipCore still hard-codes the Cobra RCS nozzle
  presets / cockpit / mesh and the registry is keyed by ObjectType; radar range of the product is not yet
  bound to the runtime sensor; ship_economics.json radiator_mw (e.g. 3 400 MW for Cobrett) contradicts
  the universe thermal law used by the runtime (380 m2 / ~180 MW) - to reconcile; economics prices differ
  from the designed parts (WARN list) - the economics should take the design as source next.
- Decision recorded: Cobrett manufacturer = Cobretty Corporation (Second Hellas), founded as
  Kobersberg-Braunauer Raumfahrzeugwerke (KB-* stock numbers); code comment and industry note agree.

### Generic ships executable (user order 2026-11-04 b): passports -> real ShipCore for all 21 models
Restore point: `restore/pre-generic-ships-2026-11-04`. Order: no Cobra topology in the generic path,
real module hierarchy, passport positions in all physics, live mass, runtime devices, primitive geometry,
passport-id identity, runtime check, Cobra on the same path; economics NOT rewritten yet.

- Topology: the six fixed layouts moved from `presets/cobra` to `plant/CanonicalActuatorLayouts.h`
  (Cobra headers are aliases). Slot STRUCTURE (count, ids, roles, directions, generator wiring, the
  8-node x 3-nozzle RCS bank) stays the model; COORDINATES are the hull's own. `ShipLoadoutTopology.h`
  `registerLoadoutTopology()` validates the structure (replaces requireLoadoutMatchesLegacyTopology)
  and registers the loadout's mount positions (`plant/ActuatorTopology.h`, key = FNV(passport|loadout),
  never a pointer: ShipParams is written raw into case files). `ShipParams::actuatorTopologyKey`
  (appended) is set by the capability builder from the bound loadout; every lever-arm consumer reads
  positions through it: capability axial authority, actuator plant (3 fns), engine control matrix,
  preset manoeuvres (cache keyed by it), execution gateway r x F, Newtonian RCS allocator / authority /
  pair feasibility (`NewtonianRcsDemand::topologyKey`). ShipCore nozzle module ids come from the bound
  loadout (`rcsNozzleIds()`), not the Cobra table. The "all six loadouts" profile cache is generic
  (`installedProfiles`, per descriptor), not `typeId == CobraMk1`.
- Working-axis analysis: a CoM within a gimbal TRIM distance (|arm.z| tan(min(cone, 2 deg))) of an
  opposite pair's segment counts as a balanced axial ray (real hulls are never exactly centred). Cobra
  (CoM at origin) unchanged; legacy A3 with CoM shifted 0.02-0.2 m keeps 73.2-73.4 m/s2 (probe).
- Passport (designer): `place()` writes runtime slot / generator-group / RCS-node ids and a primitive
  block (centre, size in metres) per position; Cobrett keeps the canonical runtime coordinates (RCS
  corners +-1, ER-5 skeleton). Canon fixes: Cobrett height 4.3 -> 5.0 m (= runtime), RCS clusters a
  multiple of the 8-node bank (Kamen 6 -> 8, He-S240 12 -> 16, L-900 12 -> 16; new FAIL check
  rcs_fills_newtonian_bank). Designer still 0 FAIL / 39 WARN; lore regenerated (lore_md OK, html 138).
- Builder (`GenericShipDescriptorBuilder.h`, rewritten): hull.root (keel) -> hull.fore/mid/aft (load
  bearing, support links) -> equipment by block z; RCS cluster block = parent of its node's 3 nozzle
  modules (Cobrett: 24 nozzle units); every module has `ModuleDescriptor::placement` (box + mass); dry
  mass / CoM / inertia tensor summed from the placements (structure = dry - equipment split keel 25 %
  + sections 3 x 25 %); drives / reactor groups / nozzles at passport coordinates; existing runtime
  products (CR-1, N1-AX, ER-5, FC-RCS, AT argon tank) use their runtime passports verbatim; devices
  (`ShipDescriptor::catalog`): radar = sensor backend with the product range (N-2 2 000 km ... PA-100
  100 000 km), receiver / transmitter from the comm product, transponder, life support (power, persons,
  days), computer, jump core (rating, sealed), coolant loops, service reactors, docks; cargo holders
  share the passport payload; `primitiveBlocks` (keel + one box per installed position) and
  `meshPartIds` per module. `CatalogShipRegistry.h`: process registry by passport id.
- ShipCore: `ShipCoreMass.cpp` - live mass = attached (not detached) module placements + load
  (cargo in holders, C20 tare, people, tanks, stores); a detached holder leaves with its cargo;
  recomputed on setLoad / addCargo / removeCargo / module revision; legacy hulls keep the authored tensor
  (zero load = identical numbers). `serviceLoad()` (crew, full tanks and stores) at spawn for catalog
  ships. `refreshDeviceState()`: radar / receiver / transmitter / life support / jump core follow their
  modules; jump mass vs core rating. Player keeps a catalog radar (TestIdeal backend) instead of the
  synthetic one.
- Damage: hulls without a mesh get one hit volume per module placement (`ObjectRuntimeHitBuilder`),
  health from ObjectModuleRuntime.
- Identity: `ShipSnapshot::catalogShipId` (wire schema 25, protocol 22), `ShipInstanceRecord::
  catalogShipId`, client `ShipDescriptorRegistry::resolve(typeId, catalogShipId)` (world state + map
  bridges), procedural `CatalogShipAssembly.h` (box meshes, LOD proxy) and
  `AssemblyGpuLibrary::get(const ObjectAssembly&)`; no ObjectType needed for catalog ships.
- DEV spawn: `ELITE_CATALOG_SHIPS=all | id[,id...]` - row ahead of the player, each placed in the
  player's hub frame (`placeShipInReferenceFrame`), broadside.
- Evidence (offline): `EliteShipCatalogCheck` (now on the production ShipCore) PASS 227: per model -
  identity / topology key, passport coordinates in the loadout, hierarchy (no missing parent, every chain
  ends at hull.root, supports resolve), healthy slots + live generator groups (= passport MWe), devices
  (radar range = passport), in-service mass, loaded mass = passport base gross + canonical C20 tare
  (1.7 t from ShipCargoPassports; a full passport load occupies every declared C20 slot) and lower
  acceleration, docks / radiators / placement hit volumes, radar module destroyed -> radar off (second
  set keeps it), drive destroyed -> less thrust; train car detached -> mass -car -its cargo -its C20,
  acceleration up; fixed-step burns on the production control path (OCS-4, PK-30 Assisted capped by
  its 1.5 g crew load, C-120 empty 6.70 / full ~2.86 m/s2, He-T12 full ~1.22, L-900 full 3.99, B-90
  11.62, Cobrett N1 35.73, A3 71.8, B-4, ST-6) = thrust / live mass, spin < 0.02 rad/s; Cobrett
  passport vs legacy Cobra (N1 and A3): slot coordinates, drives, 24 nozzles, generators, mass,
  envelope, SR-70, radiators, C20 identical; NOTE A3 passport hold 110 t vs legacy 120 t (legacy one
  hold for both generations); inertia passport/legacy 1.06/0.84/0.66 (N1) - summed boxes vs one uniform
  box. `EliteNavigationCapabilityCheck` PASS 20, `EliteShipThermalCheck` PASS 9.
- Evidence (real game, target machine, Debug, D:/tmp/elite_genship_run1): `ELITE_CATALOG_SHIPS=all`
  spawned all 21 (log: modules, in-service mass, radar km), client hydrated all 21 with procedural
  assemblies, the row is visible from the cockpit / drone view next to Vysokiy. Not flown by a human.
- Tests (full suite tests/run_all_mingw64.sh, then the red blocks re-run at the restore point in a
  worktree): 8 red blocks - model asset (link: compartments/presentation V4), world runtime
  (interstellar medium), cross-timeline (travel-frame ownership text check), navigation runtime (6:
  traffic_route_graph, client_route_autopilot, assisted_authority_contract, newtonian_execution_authority,
  assisted_safe_course_recovery, ship_state), interaction activation, system map (SystemMapBehaviorTests
  abstract mock), feature contracts (renderShipUi), client acceptance (PlayerInputMapper) - ALL identical
  at `restore/pre-generic-ships-2026-11-04`. Caused by this pass and fixed: NewtonianRcsPhysicalTests /
  NewtonianPhysicalDiagnosticsTests no longer compiled (the allocator stopped including the Cobra nozzle
  header) - include added. Green: trajectory predictor, sensors, presentation, multiplayer, headless
  build + smoke, main build + fast universe smoke, standalone runtime, network process.
- Review correction after 24f40671: generic runtime no longer invents a 2.2 t C20 tare. Both payload
  (20 t) and tare (1.7 t) now come from the pre-existing authoritative ShipCargoPassports.h contract;
  EliteShipCatalogCheck marks a full passport load with containers=containerSlots (OCS-4 = 24 boxes).
  The designer's field name mass.gross_t / accel_g_loaded still excludes C20 tare and is therefore
  semantically misleading; reconcile/rename it when the passport/economics layer is made authoritative.
- Review correction after 5c971ead: catalog ships now use the same CPU CatalogShipAssembly on the
  authoritative ShipCore side as on the client. Its primitive meshes are module-local with the module
  origin at the passport placement, so detach/repair no longer depends on a legacy ObjectType /
  AssemblyMeshLibrary entry. EliteShipCatalogCheck now requires the He-T12 car to spawn a detached
  fragment at its passport position and to reattach with the parent's mass restored.
- Detached-fragment limitation: ObjectDetachedFragmentRuntime is still a kinematic salvage/repair
  representation, not an independent rigid body with its own mass/cargo state. Parent mass conservation
  on detach is correct; world-level fragment mass/cargo conservation remains future work.
- Known RCS granularity limit found in review: designs with 16 cluster units at 8 mount positions are
  represented as 8 damageable node-parent modules with 2x mass/authority each. Healthy mass/thrust is
  correct, but losing a node currently removes both co-located clusters together; unit-level 16-cluster
  damage requires a follow-up representation change.
- Not done: the player cannot yet board a catalog ship (cockpit / view rely on the Cobra descriptor);
  Assisted designs with RCS blocks (He-F22) carry them as hardware only (Assisted has no RCS bank);
  Shipyard UI not started (by order); economics reconciliation pending (radiator_mw 3 400 for Cobrett
  etc.); Cobrett manufacturer = Cobretty Corporation, historical origin Kobersberg-Braunauer
  Raumfahrzeugwerke - PROPOSAL until the author confirms.

### Clean generic ships + F6 Shipyard + F7 Repair & Refit (user order 2026-11-04 c)
Head checked first: review commits 5c971ead / 97f482af / e2d72b68 did not compile (`catalog::ships::C20PayloadTonnes`
resolved to game::catalog inside the generic builder) and the train detach contract measured the "detached"
acceleration after reattaching - both fixed (eda7e1f8), not reverted.

- Mass ladder (106c1bce): one meaning in designer, runtime and lore - dry -> service (tanks, stores, crew) ->
  net payload (cargo + passengers) + C20 tare in EVERY declared place (1.7 t from ship_economics cargo_model =
  ShipCargoPassports.h) -> gross (everything aboard at a full passport load). `accel_g_light` renamed
  `accel_g_service`; `mass.service_t / net_payload_t / container_tare_t / c20_tare_t` exported; runtime spawn
  mass = service_t and full load = gross_t within 10 kg for all 21 models. The economics class model
  (logistics.py cargo_gross_t) uses the same tare definition but its own class masses - reconciliation with the
  passports still pending (by order: economics not rewritten).
- RCS units (8f811f0d): several units per mount node, each its own damageable cluster module with its own three
  nozzles; `RcsNozzleBank24` in the existing allocator sums the living units of each bank nozzle (24-nozzle
  topology unchanged), ShipCore builds it once (`installedNewtonianRcsBank`) for capability and execution,
  operating life per unit. He-T12: destroying one of the two clusters of a node halves that nozzle (8 -> 4 MN).
- Detached mass (6adf44f0): `DetachedFragmentMass` (module dry, cargo it carried, C20 tare, total) on every
  fragment from the same load rule as the parent (`cargoAboard`: holders carry the payload and their C20
  places), replicated (snapshot schema 26). He-T12 car: 400 + 4800 + 408 = 5608 t = the parent's loss.
  The fragment is still kinematic (no own rigid-body dynamics yet), but no mass disappears from the data.
- Repair & Refit layer (64533b6a, `src/game/ship/refit/RepairRefit.*`): a throwaway ShipCore adopts the live
  instance (modules, links, load, propulsion switch, service reactor / coolant / radiators), the operation is
  applied with the same `ShipCore::serviceModule` an apply uses (`ObjectModuleRuntime::repairModule` - one
  module, wear kept; `replaceModuleWithNewUnit` - new unit, reinstalls a detached one), both states measured by
  the production capability / power / thermal / device code plus an 8 s fixed-step full burn (temperatures,
  margins, coolant, emergency exchanger). Deltas, compatibility (category, fit, generator power, Assisted
  gimbal), compatible catalog replacements, cost from passport price + repair doctrine through
  `assets/data/ships/ship_market_rules.json`. Another product: exact preview on a descriptor copy, apply refused
  until the per-instance configuration layer exists.
- Network: `ClientShipCommand::RefitPreview / RefitApply` (+ `detail`, subject ship in `amount`), server
  `GameSimulation::requestRefitPreview / applyRefit` (same owner only), `ShipSnapshot::refit` (ownership,
  registration, preview, apply result; snapshot schema 27, protocol 23). Owned ships always replicate their
  module graph. DEV lineup ships are owned by the player.
- F6 Shipyard (`ShipyardPresentation`, `ServicePanelsRenderer`): all catalog models (right), card (left) with a
  side / plan preview of the runtime primitive blocks, the passport / descriptor numbers, expandable passport,
  seller offers of concrete instances (`ShipMarket.h`: passport id, condition, age, hours, wear, damage,
  installed equipment, price, seller) - DEV mock stock; price: -20 % first sale, age / hours / wear / damage
  decay, seller margin, never below scrap + reusable parts.
- F7 Repair & Refit (`RepairRefitPresentation`): the player's ships with < >, functional side-view schematic of
  every module placement coloured by its replicated state (clickable), groups (propulsion, generators, RCS,
  service power, cooling, tanks, radar / comms, computer, life, jump, weapons, payload, docks, structure),
  damaged-module list, module card (product, state, health, damage causes, wear, mass, power, position, repair
  level, LRU, replace hours, price), Repair / Replace (previews from the server), compatible replacements,
  DEV apply. Selecting a damaged module requests its repair preview at once.
- Evidence (offline): EliteShipCatalogCheck PASS 227 (passport service / gross = runtime, RCS units, fragment
  mass), EliteRepairRefitCheck PASS 9 (radar failed -> restored, drive destroyed 4.46 -> 6.70 m/s2 and burn,
  generator at 57 %: 8 019 -> 9 900 MW and burn 5.52 -> 6.70 m/s2 - the capability alone did not show it,
  incompatible rejected, N-2 -> N-10 range, preview never mutates the live ship, apply == preview after),
  EliteServicePagesCheck PASS 7 (21 models one source, card values = runtime, offer ladder, real module ids,
  failed radar, preview carried), EliteNavigationCapabilityCheck PASS 20, EliteShipThermalCheck PASS 9.
- Evidence (real game, target machine, D:/tmp/elite_shipyard_run1, ELITE_CATALOG_SHIPS=C-120,He-T12 +
  ELITE_CATALOG_DAMAGE=1 + ELITE_DEV_SERVICE_SCRIPT): F6 shows 21 models / C-120 card / 4 offers; F7 shows the
  player's 3 ships, C-120 with the server-damaged drive (failed), generator (57 %) and radar (failed); radar
  selected -> server preview "radar no -> yes, range 0 -> 2000 km, 33 840 Cr"; DEV apply -> "[Refit] apply
  ... APPLIED", the replicated radar module becomes "in service 100 %". Not exercised by a human mouse yet.
- Tests (full suite after the last commit): the same 8 red blocks as at restore/pre-generic-ships-2026-11-04
  with the same messages (model asset link, world runtime interstellar medium, travel-frame ownership text
  check, the 6 navigation runtime reds, interaction activation, SystemMapBehaviorTests abstract mock, feature
  renderShipUi, client acceptance PlayerInputMapper); green: trajectory predictor, sensors, presentation,
  multiplayer, headless build + smoke, main build + fast universe smoke, standalone runtime, network process.
- Not done: per-instance configuration (installing another product), Remove / Install / Sell actions, real
  payments and seller inventories, Shipyard purchase transaction, legacy Cobra modules have no passport
  placement (F7 lists them, the schematic is empty for the Cobra hull), the human acceptance of F6 / F7.

### Catalog ship meshes, carriers, editor model list, hub module folder (user order 2026-11-04 d)
Restore point: tag restore/pre-ship-meshes-b-2026-11-04.
- Carriers (3e29abf3): hangar / boat bay volume from the CARRIED craft envelope (`craft_ship` -> its catalog
  design, else `craft_envelope_m`), two abreast + clearance; designer check `craft_bay_fit`. He-S240 AP-2 was
  9 x 5 x 12 m (smaller than one B-4) -> 30 x 7 x 24 m for 2 x B-4; new `bastion.bay.hangar4` (AP-4, 300 t) on
  the B-90 Tsitadel: 4 x B-4 -> 30 x 7 x 45 m ventral; boarding boat bays 8 x 6 x 15 m. 21 ships, 0 FAIL.
- Ship meshes (9ab242a6, c2368ec7): `EliteShipMeshExport` (tools/ship_meshes) writes
  assets/models/ships/<folder>/LOD0/<category>__<module>.obj from ShipDescriptor::primitiveBlocks (the boxes
  the game draws), one OBJ per module + primitive.mtl + ships/ship_models.json (folder <-> passport, class,
  carrier bays, primitive / authored). 19 folders; Cobrett-I N1 / A3 -> existing cobretty-mk1-int (authored);
  existing folders never touched; `--check` = PASS. NOTE: assets/ is gitignored (external tree) - the ship
  folders and index are NOT in git, they are regenerated by the exporter (index rebuild from scratch is
  byte-identical). Contact sheet: 0 inward faces.
- Hub (c5452903): dock models moved src/assets/models/hub/guidance_test -> assets/models/hub/<module>/LOD0
  (hex-dock-a, dock-service-block, guidance-dock-cube, guidance-dock-cylinder); .gitignore keeps
  assets/models/hub/ tracked; runtime paths, generator (byte-identical output), geometry contract, docs updated.
- Editor 0.10.87 (25436bed): ship folders named from the index, hub folders as Folder sources, grouped model
  select (catalog ships / ship folders / hub / stations / runtime), passports in the catalog message.
  Evidence (target machine, D:/tmp/elite_shipmesh_run1): WS probe of the running editor - 30 entries,
  20 catalog-ship entries = 21 passports, 4 hub modules; B-90 opened (LOD0 66 parts, editor_b90.png).
  Model-asset architecture checks: identical to the pre-change baseline (23 PASS, 30 old FAIL; 10 version
  pins moved 0.10.86 -> 0.10.87).
- Full suite: the same 8 red blocks with the same messages; game start: no OBJ load failure, dock traffic at
  hex_dock_a. A second immediate relaunch after a timeout-killed run died at startup ("Fatal error") - test
  harness artefact, a later relaunch was clean; not investigated.
- Not done: runtime still draws catalog ships from descriptor primitives (not from these OBJ / a compiled
  .elmodel); editor "open ship record -> load new mesh -> save as actual" flow (index source=authored is the
  hook); hangar interiors / parked craft geometry; human check of the editor list.

### Player radar scope + user report "freezes / slow route / hull oscillation" (2026-11-04 e)
- Radar (64dc8931): the cockpit PPI scope was off (`RadarHudEnabled = false`, recorded earlier as "by
  design") and read the empty legacy `radarContacts`; the server sensor unit already produced the
  session-private RadarScanReport. Now the scope draws those tracks (nothing when the radar module is
  destroyed / unpowered), automatic 1-2-5 km scale, state line above the scope; navigation-sensor
  contract updated (HUD allowed only when fed by the scan). GAME evidence: D:/tmp/elite_freeze_run1/
  radar_30s.png - scope visible, "RADAR 1 km | 8 tracks" at the start hub. Blips only on sweep (not seen
  in the single frame); human check pending.
- User report after a docking flight: strong freezes passing the station, very long route computation,
  vertical hull oscillation. A/B real-game flights (nav-stress:simple, D:/tmp/elite_freeze_run1, base =
  restore/pre-generic-ships-2026-11-04, head = 64dc8931):
  Assisted route worker 44.0 s (base) vs 44.8 s (head) - the slow Assisted planner is PRE-EXISTING
  (Newtonian 1.08 / 1.07 s); the user's 55.9 s request (elite_docking_planner.log) was Assisted.
  N1: DONE 230.6 / 234.0 s, max cross-track 15.0 / 20.6 m, hitches 0 / 1 (81 ms autopilot update);
  A3: hitches 0 / 0, xtrack 24.1 / 25.2 m. Pitch command sign changes ~550 / ~465 per flight, mean
  |omega| 0.266 / 0.270 - no new oscillation in the trace. Freezes NOT reproduced in the fixture;
  the user's scene (normal start, hub traffic) not reproduced yet. OPEN: Assisted planner time (45 s);
  user-visible vertical oscillation (needs the user's flight trace); freezes in the normal scene.
- "Fatal error:" (empty message) at startup: reproduced twice when EliteGame was relaunched right after a
  timeout-killed instance; a relaunch a minute later starts normally. No leftover Elite / WebView2
  processes found. Root cause not proven.

### Radar scope fixes + orphaned WebView2 / "Fatal error" (2026-11-04 f)
- User: scope blinks, one blip that keeps vanishing, no station / docks, nearby ships invisible; asked
  for a simple game radar and whether it is heavy. Root causes: (1) the PPI latch lit a blip only when
  the 1400 deg/s sweep passed within 3 deg (23 deg per frame -> ~1 revolution in 4), faded in 0.23 s;
  (2) my 64dc8931 capped the scale at the client descriptor's 1 km LCD preset; (3) the server sensor
  read only materialized ships - traffic shells (hub traffic) were never radar truth.
- Fix (0a8cae1d + 5efb79b9, the server half was first lost to a probe revert): traffic shells in the
  truth, track kind ship / structure (snapshot schema 28), every track drawn every frame (sweep only
  brightens), structures amber squares, beyond-scale tracks on the rim, scale = 1-2-5 step holding the
  tracks within 30 km, content redraw <= 30 Hz, sweep 180 deg/s, DEV ELITE_DEV_RADAR_LOG.
  Detection stays the simple game rule (server: in range of the installed radar, 2 Hz); the legacy
  perception radar (RCS / lock) stays off. GAME evidence D:/tmp/elite_radar_run2: 15 tracks (ships +
  structures), scale 20 km at the start hub, stable between frames (final_scope.png).
- Cost (temporary probes, reverted): server sensor 0.0036 ms / tick; scope 0.2 ms CPU + 0.07 ms GPU per
  frame (GL timer query; an earlier 1.9 ms figure was a glFinish sync artefact).
- Orphans (46ba1fb3): after a kill / crash EliteGame left its msedgewebview2 browser on the shared
  profile (%APPDATA%/EliteGame.exe - the per-pid WEBVIEW2_USER_DATA_FOLDER is ignored by the webview
  loader, 985 empty folders); the next start died with an empty "Fatal error" (webview error -3). A
  normal window close leaves nothing. Now main() puts EliteGame in a kill-on-close job object: 0 orphans
  after a kill, immediate relaunch starts. Fatal message names the exception type / webview code.
- Full suite: the same 8 baseline red blocks; NAVIGATION SENSOR + IDEAL RADAR, multiplayer, network
  process, standalone runtime PASS. Human check of the scope pending.

### User order 2026-11-04 g (accepted radar; queued work, in this order)
Restore point: tag restore/pre-order-2026-11-04-g.
1. Radar: episodic interference (the LCD glitch effects) from time to time; R cycles the scope range
   20 / 50 / 150 km; in J (global) mode the radar looks only forward along the course to 2 000 km
   (early hazard warning).
2. Player ship identity: the player flies Cobrett A3 (Assisted) - my reports said N1; verify.
3. Freezes: user video D:/tmp/freez01.mp4 (suspect: planet).
4. Visual: "Elite classic" render - planet white for ~1 s at start; entering J the hub disappears and
   then catches up the ship (time desync, as before).
5. Details map: player position = golden marker "игрок" (hub cube golden when the player is in a hub);
   planets opaque; labels behind a planet hidden; every map remembers its last view; rings / belts:
   repeatable procedural asteroids (same place -> same asteroids).
6. Hubs: describe the module composition next to the hub functions (shuttle dock 10 pads + hotel +
   10 interstellar passenger platforms; admin station; one staff habitat; fuel dock 6; service dock 6;
   fuel / gas terminal; cargo terminal (more docks); holding area).
7. Economy: 30-40 grouped commodities; per-hub bulletin board for couriers / carriers (clickable
   destination and hub list -> map, distance, owner, last known prices with network delay); ticks and
   price drivers, quarantines, blocs (customs may turn ships away), sector patrols; supply / crew
   delivery to remote hubs - beacons are hubs.
8. Navigation in J: orbit-relative velocity (direction vs the planet radius, gravity: falling and
   missing), star-relative outside planets - to explore with an interplanetary flight.

### User order 2026-11-04 g - results (game evidence D:/tmp/elite_order_g_run1)
1. Radar (f37a7a04): R cycles 20 / 50 / 150 km; J shows a 30-deg forward sector to 2 000 km
   ("RADAR FWD"); episodic LCD interference every 45-120 s for 3-8 s. GAME: default "RADAR 20 km [R]".
2. Player ship: the player flies Cobrett A3 (ASSISTED-I in the HUD); N1 in my earlier reports was the
   acceptance harness's own Newtonian model, not the player's ship.
3. Freezes (a0f330e3): user video = ~20 fps with 0.2-0.33 s stalls while the Earth limb is in view and
   the Assisted route plans (65-86 s, elite_docking_planner.log). Cause found in repro (planet view +
   docking request): terrain bake pool = hardware_concurrency - 2 = 10 normal-priority threads + route
   worker on 6 physical cores. Pool = physical - 1 (max 6), pool + route worker below normal priority:
   steady frame 34.4 -> 16.7 ms, worst 83 -> 34 ms. Hitch profiler always on -> D:/tmp/elite_hitches.log
   every session. OPEN: Assisted planner time itself (45-86 s).
4. Visual: Elite Classic grey planet at start fixed (61ec69c6: height 0 = a contour level before the
   globe bake). J-entry hub desync NOT reproduced (rest + full thrust, 25 ms / 400 ms captures) - DEV
   switches ELITE_DEV_FREE_FLIGHT_AT / _THRUST_AT kept (e34e7217). OPEN.
5. Detail map: opaque planet, markers / labels behind bodies hidden, golden "игрок" marker (golden hub
   cube when in a hub) (a0ba54eb); maps remember the last view - Detail per body, Hub per hub, Galaxy
   (bac78746); repeatable procedural ring bodies by ring cell, density by screen size and optical depth
   (68fd527b, Saturn). OPEN: asteroid-belt spatial cells. DEV SOL script: `detail`, enter aliases.
   NOTE for scripts: ELITE_DEV_FRAME_CAPTURE / ELITE_DEV_MAP_SCRIPT count GLFW seconds, the SOL / DEV
   switches count space-state seconds (~10 s later).
6./7. Hubs + economy (e8461c41, subagent, PROPOSAL data only): 22 module types / 538 hubs with
   composition + description; 37 goods in 14 groups; tick / price / events / quarantine rules;
   bulletin-board schema; price info age over the comms network; 8-bloc customs matrix.
   tools/colonization_lab/out/ECONOMY_LIVE_DESIGN.md. Author questions listed there (tick 24 h, bloc
   matrix, lotos in catalog, 32 liner platforms at Vysokiy). No runtime / UI yet (board, clickable
   destinations, ticks).
8. J orbit HUD (0c19ded7): speed vs the dominant body, radial, circular / escape, H / Pe / Ap
   (schema 29). Observation: the Earth hub moves 7.74 km/s at 422 km (circular 7.66) - a ship leaving
   it is on a 422 x 733 km orbit.
- Full suite after 0c19ded7: the same 8 baseline red blocks with the same messages; navigation sensor /
  radar, multiplayer, network process, standalone runtime, headless + main builds PASS.

### User order 2026-11-04 h ("belt asteroids and the rest"; the user approved the video-report format)
- Belt (aa1859ea): a belt body's Detail = ~3 million km window on the annulus, star-fixed 100 000 km
  cells seeded by (belt, sector, lane), screen-density LOD, edge arcs + arrow / distance to the star.
  GAME: Sol main belt (2.67 AU).
- Assisted route time (0ac359f4): the navigation runtime / geometry / trajectory libraries compile -O2
  in every configuration (the game builds Debug -O0). Real game: Assisted worker 44.8 -> 3.3 s,
  Newtonian 1.07 -> 0.52 s; N1 route identical (32 743.764 m), DONE 235 s, 0 violations.
- F8 Trade (1e90f78a): deterministic EconomyModel on the generated economy data (market drift + events,
  board 4-12 entries / hub / day, rewards by the rules, last known prices aged by the beacon-network
  delay) and the native F8 page (board / market / system hubs, hub card, SHOW ON MAP). GAME at Vysokiy.
- OPEN: trades (buy / sell / take an entry, credits, cargo), customs refusal at docking by bloc channel
  and quarantine / blockade events, sector patrols, server ownership of the economy state once trades
  exist; J-entry hub desync (needs the user's scenario / video).
- Full suite after 1e90f78a: the same 8 baseline red blocks / failing tests with the same messages.

### User order 2026-11-04 i (video fragments, J mode as orbital flight, atmosphere, terrain)
- frag01 (full automatic docking, 6.7 min): 28-29 unique fps, longest pause 0.13 s - the freezes are
  gone (was ~20 fps with 0.33 s stalls). The session hitch log was overwritten by a later start; it now
  appends (c.f. HitchProfiler).
- farg02 hub jump (87faa141): at J entry the station vanished for 8 frames. Root cause (DEV FFTrace): the
  world is drawn one interpolation window in the past; the newest snapshot already carried the ship's new
  free-flight frame and the player was presented at 'now' in it - ~10 km from the hub drawn at 'then'.
  The player now stays in the render-epoch frame until it crosses the change. Trace: player-hub distance
  21-24 km throughout (was a 10-12 km dip for 0.35 s).
- J mode (9ee86c53, 5497cf37): speed / alignment / brake against the dominant gravity body; Assisted holds
  the nose on the velocity (prograde) without input and turns the horizontal velocity toward the nose when
  steering (20 m/s2); HUD lower right: 1st cosmic speed, V (+vertical), H / min safe, dense layers, drag,
  colour ramp; warnings (stall, atmosphere entry, dense layers, low descent, crash / landed); physical
  drag rho0 exp(-h/H) v^2 / (2 * 800 kg/m2); safe altitude = drag 0.1 m/s2 at circular speed (Earth
  109 km), dense layers = 1 g (71 km); dense-layer impact + heating damage, camera shake; solid ground and
  sea. HubMotionLab debug lab (its cube followed the player frame) switched off.
- Terrain contact (5493b942): the renderer's surface loading moved to BodySurfaceFieldLoader (shared);
  the server samples the same field (Earth DEM / curated maps / procedural PlanetSurfaceField) with the
  renderer's orientation: ground height, slope, material (rock, regolith, sand/dunes, ice, water, lava),
  feature (crater, ranges). Landing tolerance by ground; HUD 'below us'. Earth verified (rock, 2.1 km
  terrain, crash at 131 m/s). Moon / other bodies: same path, not flown yet.
- Snapshot schema 31. Full suite after 5497cf37: the same 8 baseline red blocks.
- OPEN: catalog surface pressure for rho0 (Mars / Venus use Earth's now); soft landing / splashdown not
  flown by a human; J Assisted steering not verified with real input; Moon test; terrain field memory.

### User order 2026-11-04 j (Earth hub -> Moon flight in the game window, landing)
- J autopilot (476ebb32): fly to a body and land. Server J drive on the free-flight frame (a game drive,
  NOT the ship's main engine - the user asked "where does the acceleration come from with engines off":
  the HUD thrust line now says "J-ПРИВОД АВТОПИЛОТА N g"; reactor / fuel do not pay for it yet). 1 g
  doubling every 2 s up to 100 km/s2 (ELITE_J_AUTOPILOT_MAX_ACCEL), no speed cap, sqrt(2 a d) braking at
  0.4 of the cap, d/6 s approach tail to 12 km above the body on the ship's side, the departure body is
  never cut through; nose on the target during the transfer; level powered descent (belly down, nose to
  the horizon, 40 m/s2) to ~1.5 m/s. Keys: O on a body selected on the System Map while in J, PgDn = land
  here / cancel; DEV script autopilot:<moon|earth|mars|here|off>. Schema 32. Full suite after ada429ef:
  the same 8 baseline red blocks.
- Live Windows evidence (D:/tmp/elite_moon_run1..3, video D:/tmp/elite_moon_landing.mp4): Vysokiy (LEO
  422 km) -> Moon: engaged, departure phase (Moon behind Earth), transfer peak ~4700 km/s, descent from
  8-11 km at t+230 s, touchdown 1.53 / 1.62 / 1.62 m/s vertical, 0.0-0.2 m/s horizontal, slope 7.7 / 6.8 /
  17 deg, rock, landed at t+279..287 s - three flights, three soft landings. Not a human acceptance.
- User videos frag03 / frag04 + my run2: three defects found and fixed:
  1. descent attitude nose-up (tail-sitter): the cockpit saw only sky -> level descent attitude;
  2. "white sky" over the Moon (and Earth) low over the ground: the coarse displaced globe rose above the
     camera (Modern) and Elite's silhouette lit distant ridges past the limb (nv < 0) -> low over the
     local terrain the globe is a smooth backdrop sphere at the ground level under the camera; the Elite
     rim is not drawn past the limb (d9ebd040, ada429ef; Planet Lab Moon/Earth near-surface checks);
  3. hull shake on the airless Moon: a soft touchdown pulsed the impact shake; now only real impacts
     (atmosphere comes from the catalog body; the Moon has none). Climbing out of rising ground is no
     longer a "contact".
- Modern / Anime natural-zone palette painted the Moon green with "snow" (zone heuristic read grey
  regolith as vegetation; descriptor contract says no default greening) -> zones only for bodies with a
  stated biosphere or open water (ada429ef). Planet Lab: Moon grey, Earth unchanged. (One earlier grey
  Planet Lab run stays unexplained - probably a different binary.)
- The star map in run2 video was opened by the user (not a bug).
- User decision (2026-11-04): the Moon is Earth's signature and must be clearly visible; plausibility over
  hard realism, physical sizes unchanged. Done (e106bc1d, contract CELESTIAL_RUNTIME_RENDERER.md 1):
  cinematic apparent size - a small far body (> 8 R, full from 25 R) is drawn as if closer along the line of
  sight, extra angular radius softmin(2 x true, 0.8 deg): Moon / Sun from Earth 0.26 -> ~0.57 deg, Earth
  from the Moon 0.95 -> ~1.5 deg, moons over their planets likewise; nearer than 8 R exact. Sky discs: round
  mesh, Elite fills the lit phase, Modern / Anime raise the lit side's exposure. Geometry check (true size,
  option off) 22 PASS. Planet Lab PLANET_LAB_ONLY=sky shots (D:/tmp/elite_sky*). Game run4 (Elite): the
  Moon disc on the nose grows through the transfer; at this game date the near side is unlit (new-moon
  geometry), so from Earth it is a dark disc with its outline. Not human-accepted.
- OPEN: J drive energy accounting (reactor / fuel), human flight of the autopilot, autopilot to bodies
  without terrain data, hub as an autopilot target, camera framing of the transfer (drone view).

### User order 2026-11-04 k (J control, J off between planets, minimum altitude)
- J steering: yaw / pitch turn the velocity (vs the dominant body) toward the nose in 3D at constant speed,
  1.5 s time constant, 2..20 deg/s (J drive). The old horizontal-only 20 m/s2 turn was an arbitrary
  placeholder (not an engine figure) and did nothing when flying radially away from the Moon. Live run
  (D:/tmp/elite_jsteer/steer.log): 8 s yaw / 8 s pitch at 7664 m/s - speed kept (7641..7671 m/s), the
  velocity follows the nose at 20 deg/s, pitch climbed 416 -> 505 km. The hull itself spins at the ship's
  own yaw rate (~70 deg/s) under a held key - velocity lags behind it; human feel not yet verified.
- J off anywhere: away from a hub (or too fast for its capture) the ship keeps its global motion in its own
  ballistic frame (free fall under gravity) and the local speed starts at 0; J again -> free flight. Live
  (local.log): 160.3 km/s global kept, local 0 -> 368 m/s after 5 s of '+', global unchanged by it.
  OPEN: hub capture from the local frame only via J on / J off near the hub; HUD of the local frame does
  not show the global motion.
- "Why 100 000 km minimum": it was 109 309 m = 109 km (HUD printed metres). Reason: the altitude where
  drag at circular speed reaches 0.1 m/s2 (the orbit decays fast). HUD now prints km above 10 km and the
  reason (not visually re-checked in a capture yet).
- Follow-up (user video qwert01: the hull swung freely in J; "roll is blocked"): Assisted J now steers
  stiffly - hull pitch / yaw <= 4 deg/s, nose lead over the velocity limited to 10 deg (the velocity's
  inertia), velocity follows at angle / 2 s (~5 deg/s held), speed kept; not while braking or below
  50 m/s; Newtonian J unchanged (free hull). Roll was swallowed by the prograde hold (alignment owned the
  attitude while only pitch / yaw counted as steering) - roll now releases it. Live: steer2.log (lead
  stops at 10.0 deg, 5 deg/s, 7664 -> 7654 m/s), roll2.log (a 1.5 s roll stays after release).
  Full suite not re-run for this change. The user allowed closing a running EliteGame.exe for my checks.

### User order 2026-11-04 l (J reference frames, units, touchdown, signed acceleration)
- J reference = the deepest body whose Laplace sphere of influence holds the ship, catalog parent (the
  Earth-Moon pair is one system; the strongest-pull primary had switched to the Sun halfway to the Moon and
  the Sun-parented Moon sphere came out 160 000 km). Live (D:/tmp/elite_soi): Earth -> Moon flown against
  Earth until 69 773 km from the Moon, then the Moon; Earth -> Mars: Earth until ~925 000 km, then the root
  (Sol): "глобальная скорость 13630 км/с (отн. Sol) · гравитация планет пренебрежима", altitude / 1st cosmic
  dashes. Global speed includes Earth's orbital 29.8 km/s (28.2 km/s at the hub at rest vs Earth); at J
  autopilot speeds (thousands of km/s) it is ~0.2 % and looks equal. Schema 33.
- HUD units by magnitude (m/s -> km/s; m -> km -> thousand / million km -> AU) (7deda479).
- Touchdown: the hull centre rested at ground + 1 m - half the hull sank into the surface. Now ground = max
  terrain under the footprint (centre, nose/tail, sides) + half height + 0.5 m gear. Live t2.log: 1.64 m/s.
- Autopilot acceleration is signed along the velocity: '+9117.7 g (разгон)', '-4078.5 g (торможение)'.
- QUEUED (user, not started):
  1. a believable Moon: from Earth a cratered ball, on the ground a lunar landscape (craters, ejecta,
     regolith) - procedural, reusable for the many airless moons. NOTE: the game currently draws in the
     Elite vector style (black bodies + lines); the Moon's albedo / relief show in Modern / Anime.
  2. System Map flat (plan) as the main presentation; "sectors of interest": split the asteroid belt and
     Saturn's rings into selectable sectors viewable in Details.
  3. J drive energy (reactor / fuel) accounting; human feel of the stiff J steering.
- Follow-up (user video qwerty02, "the ship sank under the Moon"): ROOT CAUSE - updateFreeFlights ran before
  rebuildNavigationGravityContext, so J flew against last tick's bodies; the snapshot carried this tick's Moon:
  ~0.01 s x 29.2 km/s heliocentric = ~300 m. Measured (ELITE_TRACE_CELESTIAL + DEV jstate): rendered camera
  4888 m vs server 5183 m before; 4174.9 vs 4177.3 m after (camera offset). The hull-footprint touchdown is a
  separate, also valid fix. HUD near the ground: "H 19 м над грунтом (4190 м над ср. радиусом)" (H over the mean
  radius on the +5 km highlands misled). Landing glow: Elite terrain horizon rim faint, relief lines brighter,
  backdrop-globe outline faint low over terrain (user request). Not human-accepted.

### User order 2026-11-04 m (landing autopilot module, planner, terrain for landing)
- New module src/game/navigation/landing/LandingAutopilot.{h,cpp} (pure guidance, reusable): Pad mode (designated
  body-fixed spot) and Wild mode (ring search for a flat spot of the hull's size: footprint slope <= 6 deg, height
  spread, no water / lava). J autopilot plans the site AHEAD (from 200 000 km) and aims the approach above it; no
  site -> no landing: circular orbit, wider search under the ground track every 3 s. Transit height grows with
  speed; last 50 m straight down. Commands: autopilot:moon (wild), autopilot:moon@pad:<lat>,<lon>.
- Live (D:/tmp/elite_landing): wild - site chosen at 190 312 km, 150 m from the approach point, slope 5.2 deg,
  roughness 2.7 m; touchdown 1.63 m/s vertical, 0.19 m/s horizontal against the ground. Pad 3N 4E - aimed from
  190 324 km; touchdown 1.57 / 0.21 m/s (the pad itself 11.5 deg: a random spot, pads are not graded).
- Root causes fixed on the way: hovering at 2 km forever (velocity against the centre, the site turns ~4.4 m/s);
  landed ship slid and sank (rest was world-fixed while the Moon turned; now body-fixed: altitude constant over
  30 s); touchdown horizontal speed against the centre (Earth equator would crash); Elite outline line through
  the HUD cross over the local terrain (removed).
- My script bug on the way: a patch truncated LandingAutopilot.cpp (empty object, link error) - rewritten.
- QUEUED (user idea, not started): terrain generation with landing-friendly areas (more on plains / deserts,
  fewer in mountains) and a flat-with-features type: cracks, canyons, scarps (sharp cliffs), standing rocks /
  boulders. Plus the believable Moon and the flat System Map with sectors.

### User order 2026-11-04 n (lunar landscape: craters and surface)
- Root cause of the "mountains, no craters" Moon: the curated map is 2048x1024 (5.3 km/texel, 8-bit +-10 km) and
  below the texel ImportedSurfaceField added terrestrial hills + ridges with +-960 m amplitude (relief 8 km x 0.12).
- New reusable CraterLayer (src/world/celestial/surface): crater population 2.6 km .. 1.5 m radius, D^-2 law
  (halving cells, 85 % filled at density 1), fresh deep bowls / old shallow saucers, rim, ejecta to 3 R, bright
  ejecta, blocks and 0.5-6 m boulders on fresh rims; band-limited, deterministic (server = client). Airless / icy
  imported bodies: soft low hills + craters, regolith (rock on steep walls / fresh rims). Server ground: regolith.
- Elite style: contours follow the eye in NESTED power-of-two spacings (refine, never slide - the user saw the
  relief "flow like water" with a smoothly varying spacing); dense lines fade instead of a moire.
- Planet Lab captures (D:/tmp/elite_lunar1): Modern near surface shows craters with rims; Elite approach shows
  crater rings. Game: landing on regolith 1.64 m/s (D:/tmp/elite_lunar_landing.mp4, 110 s from the descent).
  Geometry check 22 PASS (an 8-bit map blur I tried failed it 21/22 - removed). Not human-accepted.
- DONE (user: "give the crater layer to procedural moons"): PlanetSurfaceField AirlessRocky / Icy get the CraterLayer
  below their smallest class (~28-95 m) down to 1.5 m (no second boulder set). Measured with Planet Lab --probe-fine on
  the airless 'rocky' body: mean |slope| 0.150 -> 0.165, heights change ~1 m (mostly old shallow pits) - a subtle
  ground-level effect; Planet Lab proc_moon (system 47) frames differ by a few pixels at 220 m. Not human-accepted.
- OPEN:
  globe-level stair contours from the 8-bit relief bake; ejecta rays; the landing-friendly-areas / cracks / canyons
  / scarps / rocks terrain type (queued).
- Follow-up (user: small craters deeper, bright ejecta): small craters fresher (youth exponent by size), d/D 0.05..0.2,
  ejecta halo + rays to 5 R; space-weathering albedo on airless / icy (mature darker, fresh lighter toward white - the
  multiply clipped at 1 on bright ice). Probe: albedo spread rocky 0.36..0.51, icy proc moon 0.64..0.68 (400 m line).
  Visual: Modern ground ~3 % darker overall, halos soft (the Modern tone curve compresses highlights); the Elite vector
  style shows no albedo at all - there the deeper small craters show only as contour rings. Not human-accepted.

### User video qwerty05 (2026-11-05): the hub jumps on a flight-mode change
- Measured with the DEV FFTrace extended to the DRAWN distance (nearest scene object's render position vs the player's
  render position; ELITE_DEV_FF_TRACE_SECONDS): steady 10 307.8 m except at the render-epoch frame crossing - J on
  -1175 m, J off (hub capture) -1452 m, recovering over ~60 ms. ROOT CAUSE: sampleRenderReferenceFrame - when the
  bracketing snapshots carry different frames, interpolateReferenceFramePresentation returns the new frame as of the
  NEWER snapshot, (1 - alpha) * span ahead of the render epoch (~36 km/s heliocentric x ~35 ms). Fix: carry the new
  frame back to the render epoch along its velocity; also no blend of old-frame / new-frame local positions on the
  crossing frame. After: max drawn step 0.1 m through both changes (D:/tmp/elite_q5/run/t4.log). Full suite: the same
  8 baseline red blocks. Not human-accepted.
- Earlier note (farg02, 87faa141): that fix kept the ship in the render-epoch frame until the crossing; this one fixes
  the crossing frame itself.

### User order 2026-11-05 o (route planner + autopilot; then damage mechanics) - STARTED
Scope as given (no questions, implement optimally, corrections after review):
A. Planner + autopilot hub -> hub / orbit of a body, incl. other systems: at the finish match the hub's velocity and
   leave J automatically, a stable position inside the hub bounds / on the new orbit; gravity obstacles and fields,
   anomalies, safety through interstellar clouds / anomalies, gravity assists; Assisted and Newtonian, all 6 layouts.
   UI: finish chosen on the map -> route-points info window buttons "official" / "unofficial" route -> planning,
   J on, flight. The dashed network routes = GREEN corridors (preferred, official: safer, faster). Unofficial =
   smuggling: away from outposts / patrol routes, through poor radar coverage; intermediate points may be hubs
   (legal / illegal refuelling). Waypoint actions (now: refuel): arrive, J off, dock (the dock given with the point),
   refuel, undock, J on, continue. A J on/off distance from hubs (collision avoidance). Zones: BLUE mandatory
   passage, RED forbidden, GREEN preferred; a zone violation is an administrative penalty, not a collision.
B. Then damage: collisions and weapons (lasers, missiles of different power; no fragmentation warheads - a common
   tacit agreement: melting / burning-through warheads, the slug is destroyed or stuck in the ship). A shell ship in
   view / in a duel becomes a full ship (hit volumes, modules). Hit volumes page in the mesh editor (exists) for
   every part incl. hull plates and "patches"; parts tied to the frame by bars (destroying them detaches a part, they
   carry impacts and break), plates joined by perimeter "sausages" (cut out to reach containers with their locks);
   finish the part dependency tree / graph (existed). Example: the Cobra mesh.

#### A - results (2026-11-05), live Windows runs D:/tmp/elite_route1, D:/tmp/elite_route2
- Route autopilot legs (GameSimulationRoute.cpp, 43eb2b67): hub (arrive 12 km from it with its velocity, automatic
  J exit = hub capture, at rest in the hub frame), orbit (circular, J exit into the free-falling local frame), point.
  Hub J boundary 20 km: gentle departure (<= 1 g, <= 300 m/s against the departure hub). Bodies gone round (R + 150 km
  / atmosphere + 50 km, never below the destination's altitude - Vysokiy -> Tikhiy first went THROUGH Earth).
- Route planner (src/game/navigation/route/RoutePlanner.*) + zones data (src/assets/data/navigation/route_zones.json:
  blue gate Earth-Moon at the Earth-Moon midpoint 15 000 km, red "range behind the Moon" 25 000 km, green capsule
  corridor Vysokiy-Tikhiy 600 km; hub radar 3000 km). Official: green corridor guides, blue gate between the gate's
  bodies, around red. Unofficial: around hub radar, red and blue (no customs). Server route sequencer: pass-through
  guide points (braking for the rest of the route), stops leave J; action "refuel" = PROPOSAL timed 20 s stay at the hub
  (no dock flight: automatic docking exists only at Vysokiy's dock module; ships have no fuel model).
- Map UI: finish info window "ЛЕТЕТЬ: ОФИЦИАЛЬНЫЙ / НЕОФИЦИАЛЬНЫЙ МАРШРУТ", intermediate hub "ЗАПРАВКА ЗДЕСЬ: ДА/НЕТ";
  the planned route drawn as the transfer line. DEV script: via:hub:<id>[:refuel], finish:hub:<id>|body:<moon>,
  route:official|unofficial, leg:hub:<id>|orbit:<body>:<km>.
- Zone watch (client): red zone entry / blue gate skipped -> "АДМИНИСТРАТИВНОЕ НАРУШЕНИЕ ... штраф N кр" banner, a
  fines record (no bank yet).
- Live: official Vysokiy -> (corridor) -> Tikhiy refuel -> (blue gate) -> Moon orbit 100 km: complete, gate passed (no
  fine), orbit held ~104 -> 101 km over 150 s (20 m/s entry error; now 3 m/s / 500 m); unofficial Vysokiy -> Moon
  orbit: around the gate, fined "пропущен обязательный створ" 2500 cr, orbit entry 18 m / 3 m/s.
- NOT DONE / honest gaps: interstellar legs (no inter-system movement exists in the game at all - a separate stage:
  jump / interstellar environment); gravity assists (the J drive dwarfs gravity; the planner flies direct); interstellar
  clouds / anomaly safety (InterstellarNavigationEnvironment exists, unused - with the interstellar stage); real dock
  flight + fuel for "refuel"; Newtonian vs Assisted: the J route drive acts on the free-flight frame, the same for all
  six layouts (Assisted / Newtonian x I / Y / X) - not flown with a Newtonian ship yet; zones not drawn on the map yet;
  the sites network routes (41, site-to-site) are not runtime hubs - only the zone-file corridor is green so far.
- Full suite after the route work: the same 8 baseline red blocks.
- Pre-existing: every EliteGame run killed by `timeout` ends with a [CRASH] in opengl32.dll after a WebView2 "Failed
  to unregister class" - shutdown under kill, seen in all earlier logs; not from this work.

#### B - plan (2026-11-05, started after A; restore tag restore-before-damage)
Infrastructure audit: runtime hit volumes come from ObjectRuntimeHitBuilder (module OBBs from mesh parts, auto
PanelSeam links between panels with link volumes, frame mounts); structural links + detach propagation exist
(ObjectModuleRuntime / ObjectStructuralLinkRuntime / ObjectDetachedFragmentRuntime, fragments replicated);
ShipCore::applyDamage existed with NO production caller; no weapons, no ship-ship collision (src/physics/Collision.h
empty); two conflicting game::damage::DamageType enums (DamageType.h unused); editor .elmodel COLL/STRL/HITR data
never read by the runtime; shell tiers exist, the Combat claim is never emitted.
Slices (each: offline check on production code, then live Windows evidence):
- B1 combat core: weapons data (src/assets/data/combat/weapons.json), penetration model (beam heat at a spot with
  cooling, burn-through holes, armour reflection; units 1 health = 1 MJ), laser fire command, server beams, beams in
  the snapshot, client fire key + beam render, DEV target ship, EliteDamageCheck.
- B2 missiles: projectile sim with guidance; warheads shaped (melting jet, consumed) and kinetic slug (destroyed when
  spent / stuck in the volume where it stops / leaves the hull); momentum to the hull and the struck part.
- B3 collisions: ship-ship OBB (SAT) contacts, impulse + separation, lost energy as damage, inertial loads through
  structural links (break -> detach); ship-hub later.
- B4 shell -> full ship on fire / missile lock / duel range (Combat claim).
- B5 editor: hit-volume page per part incl. plates and patches, bars (frame mounts), seam "sausages", container locks;
  .elmodel COLL/STRL into the runtime; dependency tree / graph on the Cobra.

#### B1 - WRITTEN, NOT COMPILED (2026-11-05; uncommitted working tree)
Auto-mode permission classifier began refusing every shell command (build, run, commit) mid-slice; file edits still
worked. Everything below is source only - no compile, no check run, no live evidence yet.
- Cobra audit (EliteDamageCheck --dump, run from build/ for assets/): 172 runtime hit volumes (panels, frame CF/CB/FL/FR/
  reactor, engines, tanks, lasers, cockpit, radar, control units, auto panel seams placed as thin boxes = the
  "sausages"); 102 structural links; the support-link (bar) volumes are NOT placed (centre 0, 0.05 m) - B5 must place
  them; hull 260 t. The old ShipCore::applyDamage ignored the hull's orientation (world offset, not rotated) - the
  combat path rotates into the hull basis itself.
- New: src/assets/data/combat/weapons.json (3 lasers 1.5 / 4 / 10 MW, 3 missiles: shaped 30 / 90 MJ, slug 40 kg;
  material constants), src/game/combat/WeaponCatalog.*, src/game/combat/Penetration.* (ray vs OBB volumes, spot heat with
  conduction, burn-through holes, armour reflection, shaped jet / slug walk, stuck slugs),
  src/game/simulation/ShipCombatSnapshot.h + ShipSnapshot.combat (wire schema 34), GameSimulationCombat.cpp (lasers from
  WeaponMuzzle attachments converging on the boresight target, damage flush 0.2 s / on burn-through, missiles with
  seeker cone + lateral-velocity nulling guidance + system gravity, contact by segment vs hit volumes, momentum to the
  hull, part impulse to the struck part; shell under fire -> Combat activation claim + materialize + promote; DEV
  target Cobra), ShipCore::applyVolumeDamage / applyPartImpulse / afterVolumeDamage (applyDamage tail shared),
  ClientShipCommand::FireWeapon (0 off, 1 on, 2 missile, 3/4 next type, 5 DEV target), keys SPACE (hold) / M / , / .,
  DEV script fire:on|fire:off|missile|laser:next|missile:next|target:<m>, SceneRenderer 1C beams / missiles / impact
  flashes, tools/combat/DamageCheck.cpp (EliteDamageCheck, headless_server build) with PASS/FAIL checks.
- Next: compile (build/headless_server EliteDamageCheck, build EliteGame), run the check, fix, live run with
  target:400 + fire:on + missile, commit; then B2/B3 per the plan.

#### B1 - COMPILED, OFFLINE PASS, LIVE PARTIAL (2026-11-05; still uncommitted)
- Compile fixes: a broken string literal in ShipCoreDamage.cpp (applyPartImpulse log).
- EliteDamageCheck (build/headless_server, run from build/): 0 failures after two fixes -
  (1) weapons.json missing in build/assets until the client build copied assets;
  (2) dependency tree: a part burnt to zero did not release what it holds (frame FL has DisableOnly -> state Disabled,
  links ignore the support's state; the link hit-volume mirror in syncDestroyedModulesFromHitVolumes then revived any
  broken link). Fix: syncDestroyedModulesFromHitVolumes first kills every link whose support (moduleB) volume is
  destroyed, with its link hit volume -> laser L detaches when frame FL is destroyed. Pulse laser burns the side panel
  in 11.95 s, military 10 MW in 0.67 s, 0.18 MW never; 30 MJ jet spent inside; 40 kg slug stuck at 1.2 km/s, out at 6.
- Full suite after these changes: the same 8 baseline red blocks (model asset, world runtime, cross-timeline,
  navigation runtime (same 6), interaction activation, system map, feature contracts, client acceptance).
- LIVE (build/EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple, Newtonian-I,
  ELITE_DEV_SOL_MAP_SCRIPT, stderr D:/tmp/elite_b1_stderr.txt):
  - root cause 1: the DEV target Cobra was flown by the NPC AI (20 m/s away, 6 -> 47 deg off the boresight).
    Fix: m_combatTargetDummies, skipped in the NPC AI loop (GameSimulation.cpp) -> target stays at 400 m, vLocal 0.
  - LIVE VERIFIED: military laser burns through Cobra panels (ship_panel_top_LF / CB / RB / RF BURN-THROUGH logs);
    target on the boresight (0.2-0.5 deg drift from the scenario ship's own slow rotation).
  - root cause 2: missiles launched with zero world velocity (tracks only kept while fighting) -> fixed, tracks are
    always kept. Missiles now lock target 12 but MISS: closest approach 61-79 m (hull radius 19 m) within ~1 s.
    Suspect: finite-difference velocities of hub-tactical ships are noisy / inconsistent (launch-velocity diagnostic
    added, not yet run - the auto-mode classifier blocked shell commands again).
  - DEV diagnostics in GameSimulationCombat.cpp (1 Hz laser line, MISS DIAG, missile t+N, passed-target, launch diag)
    - trim the noisy ones before commit.
- Next: run the launch diag; take ship velocities from motion (referenceVelocity + frame local velocity) instead of
  finite differences if they disagree; re-run missiles; trim diagnostics; commit; beep.
- PAUSED by the user (2026-11-05): damage work stops here. The B1 working tree must first be committed as a WIP
  checkpoint (status: compiled, offline PASS, laser LIVE, missiles miss - NOT accepted) before the next order starts.

### User order 2026-11-05 p - political order revision - SOURCE + GENERATORS DONE, AUTHOR REVIEW PENDING
Status: offline generators PASS; whole new order = PROPOSAL_PENDING_AUTHOR_REVIEW (not CANON). No runtime C++ change, not
compiled (runtime JSON shape kept: economy_rules.json blocs.matrix + hub "bloc"). Restore tag
`restore/pre-political-order-2026-11-05` (= B1 WIP checkpoint 7b6ebf5a, which the user ordered committed first).
Terminology (user 2026-11-05): **actor = acting party, node of political force** (sovereign subject of 3126).
- Single source of truth: `tools/colonization_lab/canon/political_order.json`, read only through
  `tools/colonization_lab/political.py` (`--list` / `--check`; out/world_3126/political_order_3126.json + POLITICAL_ORDER_3126.md).
  Layers: historical actor (src/assets/data/galaxy/actors.json 14 + subactors.json 2: early-world layer, runtime map
  ownership; only a top-level "layer" note added, records untouched) / actor 3126 (21) / economic unit (23 = world.json
  colonies + Sol) / province (hd40307_firmament -> eps_eri_harmony, CANON in world.json) / special zone (gj832_gaia_zone,
  quarantine_exclusion). Blocs: cognium_pool 4, heavenly_pact 2 (+ province), lalande_league 3 (hd114783_hellas3 associate),
  southern_circle 3, citadel_compact 2, free_belts 2, sol (pole) 1; non-aligned 4 (verde resource_neutral, next
  strategic_multivector, karma ideological_isolationist, zion newcomer_religious) - not a bloc, not mutual allies.
  Directional access seller -> buyer per category (civilian_goods, industrial, dual_use, military, strategic,
  people_finance_data; 0 open, 1 licensed, 2 restricted, 3 sanctions/indirect = channel "black", 4 closed) = max(export
  seller, import buyer) -> exception caps (Kern finance, Agora transit hub, Mecca jump math for the Southern circle,
  Providence, Verde fuel; Karma excluded from caps) -> 8 sanctions, each tied to a relations_anchor record (Cognium-Citadel,
  Cognium-Svarog, Harmony-Strife, Karma split, Maland-Firmament at unit level only, Hellas2-NewIndia, Kern-Hellas3 temporary,
  Europa-Freeclans); Zion-Kern = tension without measures (crisis of 3126, open).
- Migrated: industry.json `blocs` + hand bloc matrix in economy_live.json (+ polity_overrides) REMOVED; industry.json now
  `industrial_schools` (design + core_source per unit, values carried over; Verde -> design none). The hidden
  `if q == "wolf359_citadel"` in industry.py is replaced by strategic access 4. Runtime economy_rules.json blocs.matrix is
  now derived (headline_channel_rule over access keys: bloc / nonaligned:<actor> / zone:<unit>) plus political_access
  (unit -> unit six-digit codes). Consumers switched: industry.py, economy_live.py, ship_designer.py, world_economy.py
  (yard partner: Third Hellas now buys ships via Agora, was Sol), lore_md.py (POLITICS/POLITICAL_ORDER.md + a generated
  "Политический статус 3126" block in every colony dossier), lore_sites.py (INDUSTRY.md tables).
- Data adjustments made while wiring (PROPOSAL): Pact military export 2 (state decision; keeps Sol PK-30 / PAO-7 with
  Firmament armour/guns); UM transponder T-1 = industrial (network civil standard; Bastion ships carry it); jump-core
  supply prefers the unit's core_source incl. an assembler (Firmament gets cores from Harmony); buyers outside their own
  bloc spread purchases over 3 suppliers (keeps League fleet "many vendors").
- Hand text fixed: RELATIONS.md (bloc formation rows, per-actor table: "cores from Citadel" claims contradicted the 2850
  licence + Bastion-only-in-pact -> Cognium licence / Mecca; added Tianhe, Zion, Third Hellas, Providence, Sol rows),
  WARS.md (blocs and sanctions instead of wars, no new wars), STRATEGIC_DEPENDENCIES.md, COLONIES gj581_europa /
  hd40307_maland, Economy/INDUSTRY_AND_RESCUE.md §1, Economy/POLITICAL_ECONOMY.md (layer section), lab README.
- Gates (2026-11-05, all rc=0): lab --check-dataset, --check-lore, logistics, world_economy, world_sites, political,
  industry, economy_live (deterministic_rebuild), ship_designer (21 ships, 0 FAIL, 39 WARN), lore_md --check (pre-existing
  stale FLEET_MODELS / SHIP_EQUIPMENT blocks refreshed by the rewrite), lore_html --check (139), calibrate_population.
- Known, NOT fixed (outside the bloc layer): the lab.py trade-flow model still sells "jump cores" from New Shanghai to
  Mecca, Svarog and New Karma (STRATEGIC_DEPENDENCIES generated table) - contradicts own Mecca math / refused licence /
  refused network. Free belts formally receive Cognium cores at "restricted" (2850 licence covered all but Citadel /
  Karma; no post-2862 sanction in the canon) - author question.
- Next: author review of every bloc / actor / sanction / exception (open_questions in political_order.json); promote
  accepted rows to CANON; then decide on the lab.py trade-model core flows; then resume damage B1 (missiles miss).

#### Correction pass 2026-11-05 q (on top of 0f5a0bda) - SOURCE + GENERATORS PASS, still PROPOSAL_PENDING_AUTHOR_REVIEW
- Author decisions: "Святой Сион" retired -> **Ново-Фиваида** (short: Фиваида; government: Синод). Machine id `leo83_zion`
  kept (legacy). Linked to historical actor actors.json id 10 (same community: ark NEW-JERUSALEM-1, first target GJ 667 C c
  = the legacy systems.json "Ледяная система Ново-Фиваиды"; 2291 target change to 83 Leonis; arrival 3126). Name status
  CANON via knowledge.py AUTHOR_NAMES; missions.json keeps "Святой Сион" only under retired_aliases. Renamed in world.json,
  settlements.json, missions.json aliases, political_order.json, world_sites.json, hand Notes (RELATIONS, COLONIZATION_CANON,
  COLONYCORE_V2, fate.md heading, dossiers, TECHNOLOGY, MINES, EARTH_3126), regenerated lore / html / runtime sites.
- Design rule (author): Chekhov's gun - every political element carries `player_role` (blocs, non-aligned, province, zone,
  exceptions, licence, sanctions); political.py check every_element_has_player_role. No secret ruling societies.
- Jump cores: new licence layer `licences.universal_jump_licence_2850` (holders = world.json jump_technology how=licensed +
  Sol; non-holders get strategic 4 from Cognium: Citadel, Mecca, Svarog, Strife, Freeclans, Karma, Third Hellas, Novo-Thebaid).
  trade.py flows are now gated by political.py (lab.PoliticalTradeAccess: category per good, level 4 = no flow, markups
  0 / 5 / 25 / 60 %, end-use-controlled strategic goods exported only by their producer). Result: Cognium cores no longer go
  to Mecca, Svarog, Karma, Citadel, Strife, Freeclans; core resale by non-makers gone; Citadel munitions no longer reach
  Cognium. industry.py reads the trade core flows (flow_sources) + checks trade_core_flows_legal / _from_makers_only;
  stolen lineage -> own pirated copies. Southern circle import strategic 1 -> 2.
  Model limit (not tuned): Mecca makes 15.3 core units/y and consumes them; New India / Maland still buy the bulk from Cognium
  (legal: 2850 licence holders, "restricted").
- narcotics: access category civilian_goods; legality stays in trade_goods.legality x hub regime family (legality_layer);
  lotosite strategic; check controlled_goods_not_military.
- Sanity: Karma is no longer anyone's >15 % partner (RELATIONS partner narrative + per-actor table rewritten from the new
  flows; STRATEGIC_DEPENDENCIES "precision from Karma" / "cores for the outcast" rows fixed; Maland / Mecca dossiers fixed).
  New checks: kern_cannot_sell_strategic_to_citadel, providence_does_not_lift_sanctions, transit_never_strategic,
  finance_bridge_no_strategic_cap, citadel_civilian_open_strategic_closed, free_belts_military_only_grey, cognium cores only
  to licence holders, no cores for refusers / copiers, own core makers not licence buyers.
- POLITICAL_ORDER.md narrative "Как сложился порядок" written (no TODO; no invented exact bloc dates).
- Runtime audit: GalaxyDatabase (actors/subactors/systems/nodes/routes) is compiled but never loaded (SpaceState / GameServer
  comments); ownerActor (ActorIds::Player() = 1) is only an opaque same-owner token, never resolved to actors.json (where id 1
  = Cognium) - latent clash only if the legacy DB is ever loaded. In-game owner / operator labels come from sites_3126
  (polity 3126). No C++ change, nothing compiled; runtime JSON regenerated with the same shape.
- Gates: lab --check-dataset / --check-lore, logistics, world_economy, world_sites, political (32 checks), industry,
  economy_live, ship_designer (21 ships 0 FAIL 39 WARN), lore_md, lore_html (139), calibrate_population - all rc=0;
  lab.py twice -> sha256 98f58fd0... IDENTICAL.

#### Plausibility pass 2026-11-05 r (on top of 00c55971) - SOURCE + GENERATORS PASS, still PROPOSAL_PENDING_AUTHOR_REVIEW
- Author decision: Mecca's own jump mathematics -> CANON (world.json jump_technology; knowledge.py follows status).
  Asymmetry kept: own knowledge and core design, imported precision / electronics / isotopes (trade.py INPUTS).
- Core capacity: jump_technology.capacity_x (calibrated PROPOSAL): Mecca 2.5, Citadel 2.0. Bloc procurement quota
  (political_order blocs.*.strategic_procurement, read by trade.py via lab.PoliticalTradeAccess.preferred): Southern circle
  30 % of member core demand reserved for Mecca, overpay up to x1.6; Citadel pact 85 % for Citadel, x2.0.
  lab.core_autonomy_scenarios -> lab_state core_autonomy: peacetime Mecca share of the circle's external demand 0.53 (author
  guide 20-40 %), Cognium cut-off 0.12 coverage, mobilised 0.68 (Mecca civil output -22 %) -> survives. The flow model is
  path-sensitive here (x2.2: share 0.19, mobilised 0.57 -> fails); kept 2.5 = lowest tested value that survives. Svarog: 100 %
  of imported cores from Citadel, own pirated copies tiny (emergency capability).
- New data: interim_arrangements (83 Leonis status quo, unstable, outcome UNKNOWN; real lever = Kern operates beacon
  BCN-045-BR, so the supply corridor is revocable; arbitration seat itself disputed), licence accession (Third Hellas
  negotiating with conditions; Novo-Thebaid not applying, Synod split), cognium_pool.coordination (collective strategic /
  dual-use / IP, civilian national), fault_lines per bloc, free-belts risk details, self_interest of Kern / Agora /
  Providence / Verde / Mecca, sanctions_rule + evasion per measure (dual-use 4 -> 3 for Harmony-Strife, Karma split,
  Maland-Firmament; full closure needs full_closure_reason).
- Checks added: political (36): sanctions_realistically_leaky, no_pool_wide_embargo_on_karma, thebaid_kern_interim_open_no_sanctions,
  newcomers_not_licensed_yet; industry (16): cognium_core_flows_only_to_licence_holders, new_karma_no_strategic_trade,
  citadel_serial_cores_stay_in_pact, svarog_serial_from_citadel_with_own_copies, mecca_core_technology_own,
  southern_circle_survives_cognium_cutoff; lore_md --check: check_core_claims (hand-written core-source sentences vs
  political.py; proven on the old Mecca / New India texts).
- Narratives fixed (found by the lint or by hand): lalande_mecca (buyers incl. New Karma, monoculture), hd20794_newindia
  (Citadel cores / main buyer / collapse scenario), gj832_tianhe, eps_eri_harmony, hd69830_verde, lalande_hellas,
  hd20794_hellas2, delta_pav_karma (cores from Citadel / Mecca); POLITICAL_ORDER story (Mecca quota + mobilisation,
  Svarog-Citadel lever, Kern / Agora / Providence / Verde self-interest, 83 Leonis interim order, Third Hellas accession).
- Gates: all colonization_lab checks rc=0; lab.py twice sha256 75c06541... IDENTICAL. No C++ change.

Original order text (summary):
Full revision of the 3126 political structure: separate ideological bloc / economic integration / technology access /
sanctions (now mixed in tools/colonization_lab/canon/industry.json); polity 3126 vs historical actor
(src/assets/data/galaxy/actors.json, ~14 old actors) vs economic unit (~23) vs province / special zone;
hd40307_firmament = far province of Xin He (economic unit, not sovereign); gj832_gaia_zone = quarantine zone;
blocs: Cognium pool (tau_ceti_cognium, cyg61_kern, hd20794_hellas2, delta_pav_freekarma), Celestial pact
(eps_eri_harmony, gj832_tianhe + firmament province), Lalande League (lalande_hellas, gj581_europa, hd114783_hellas3 as
low-integration associate; hd69830_verde removed), Southern circle (hd20794_newindia, hd40307_maland, lalande_mecca),
Citadel pact (wolf359_citadel, proxima_outpost), Free belts (eps_eri_strife, gj581_freeclans), Sol separate pole;
non-aligned (not a bloc; "autarkies" only as a migration label): hd69830_verde, gj876_next, delta_pav_karma,
leo83_zion (arrives 3126). Access matrix by category (civilian, industrial, dual_use, military, strategic,
people_finance_data; 0 open .. 4 closed), sanctions as a separate overlay derived from history, explicit special
exceptions (Kern, Agora, Mecca, Providence, Verde) as data. All new decisions marked PROPOSAL_PENDING_AUTHOR_REVIEW;
existing author CANON kept. Regenerate lore with the existing generators; sync Notes/Lore/POLITICS/*,
Notes/Economy/POLITICAL_ECONOMY.md, manufacturers / ship bloc metadata; programmatic listing of polities / units /
provinces / zones / bloc membership; restore tag first; commit, no push. The user's full text is the source of the
order - ask for it again if it is not in the session.

## Next step
- Fourth pass follow-ups (after the user's review): human acceptance of the
  hybrid System Map and of a full live Vysokiy -> Baikonur transfer to arrival
  and docking (3.1 h); LEO -> GEO transfers (Hohmann / spiral phasing);
  ship attitude slaved to transfer thrust; dock sectors + physical traffic at
  the other Earth hubs (cross-hub flight objects); route click info;
  settlement zoom-to on the Detail Map; reputation / warrants for the
  relation-to-player line.
- After the user's review of the spatial layer: Stage 4 runtime communication
  (beacon coverage / relay delays in the client: messages, bank, warrants,
  sync on return to coverage), then Stage 5 (passenger / cargo flows ->
  schedules -> hub traffic), Stage 6 (seeded belts / asteroid ids, city
  placement by the runtime terrain sampler, night lights per style), Stage 7
  events (epidemics, piracy, failures, rescue, anomalies; perception layer
  for the World's Ass), Stage 8 law in runtime, Stage 9 information
  (KnowledgeRecord propagation, Public / Freeband radio and news).
- Economy runtime (after the author's review): persistent ShipInstance /
  component registry (legal status, age), Sol market with stocks, C20
  container contracts, banking and storage fees.
- After the user's review: the separate stage INTERSTELLAR NAVIGATION
  (connected domain, ephemerides, gravity, InterstellarNavigationEnvironment
  hazards, icy fields, rogue objects, refuelling, ship-specific risk).
