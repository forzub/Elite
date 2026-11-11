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

#### Acceptance pass 2026-11-05 s - POLITICAL/ECONOMIC ORDER 3126 ACCEPTED / CLOSED
- Author accepted the structure: canon/political_order.json status = CANON (actors 21, units / province / zone, blocs,
  non-aligned, access profiles, licence layer incl. accession state, sanctions as now, Kern / Agora / Providence / Verde
  roles, Southern-circle procurement policy, Citadel pact + Svarog dependency, 83 Leonis interim order as the 3126 start
  state). Kept PROPOSAL: bloc genesis / dates (formed.*). Kept PROPOSAL/model: core_autonomy numbers, capacity_x,
  procurement floor / tolerance, trade.py shares and volumes, scenarios. Mecca own jump mathematics CANON everywhere.
- Southern circle: peacetime_share_target [0.2, 0.4] replaced by expected_peacetime_share [0.45, 0.60] (accepted
  calibration capacity_x 2.5 -> 0.533) + acceptance text; industry checks southern_circle_peacetime_share_in_accepted_range
  and southern_circle_survives_cognium_cutoff (0.678 mobilised). Texts say autonomy != autarky (Mecca imports precision,
  electronics, isotopes, equipment).
- open_questions = [] ; author_unknowns: leo83_crisis_outcome, sol_own_jump_cores (UNKNOWN on purpose).
- Licence 2850 wording: historical rule (majority of colonies then, except Citadel and New Karma) vs the 3126 holder list
  computed from world.json (licence basis + RELATIONS text). Harmony-Strife dual-use described as grey (level 3), not closed.
- Mixed documents: POLITICAL_ECONOMY.md / INDUSTRY_AND_RESCUE.md state "structure CANON, numbers PROPOSAL/model".
- political.py checks 40 (new: order_accepted_canon, no_open_design_questions, mecca_own_math_canon,
  harmony_strife_dual_use_grey_not_closed).
- Runtime smoke: build/EliteGame ninja (assets re-copied); EliteGame --dev-direct-flight with ELITE_DEV_SOL_MAP_SCRIPT
  (System Map, trade tabs, Detail map) - data loads, no JSON / economy errors; graceful close clean (a [CRASH] in opengl32
  appears only when the process is killed by timeout). Economy is lazily loaded only by the F8 Trade panel, so the data
  smoke is in EliteServicePagesCheck §6 (DEV tool, build/headless_server, run from build/): 37 goods, 538 hubs, names
  New Shanghai / New Mecca / New India / Citadel / Red Outpost / Novo-Thebaid / Firmament found, no retired "Zion",
  blocs Mecca + New India southern_circle (open), Citadel + Svarog citadel_compact, Firmament heavenly_pact,
  Thebaid nonaligned, Cognium-Citadel licensed, quarantine zone closed -> PASS. Not done: visual check of other systems'
  owner labels in the System Map UI (the DEV map script works only in the start system Sol).
- GUARD (technical debt, not a blocker): ActorIds::Player() = 1 collides with historical actor 1 = Cognium in
  actors.json. GalaxyDatabase is not loaded today; before re-connecting the legacy GalaxyDatabase separate the
  PlayerActorId / HistoricalActorId namespaces.
- Gates: all colonization_lab checks rc=0; lab.py twice sha256 fb41f9d4... IDENTICAL; EliteServicePagesCheck PASS.
- Next task is no longer a political revision (damage B1 missiles miss remains paused, see stage B).

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

### User order 2026-11-05 t - TOPONYMY / ENDONYMS / EXONYMS / HERALDRY - DONE (commit, not pushed)
- Scope: cultural shell only. Political / economic layer 3126 stays CLOSED: blocs, dependencies, sanctions, licences,
  population, industrial structure, jump-tech balance and the Southern-circle core maker are unchanged (machine ids too).
- Naming source of truth: tools/colonization_lab/canon/naming.json (entities by stable id: unit colony / polity,
  system:<StarAtlas id>, star:<HYG id>, body:..., site:<settlement or runtime id>, place:..., mission:<code>; endonym /
  formal / exonyms / aliases with kind + valid_from / valid_until / naming_history / verdict KEEP..VERIFY; heraldry;
  retired_authoring_names). naming.py (reader + resolver + 23 checks) writes the MIRRORS: world.json name / polity /
  asset names, political_order.json actors ru / government / short, settlements.json, population_history.json
  author_name, legacy src/assets/data/galaxy/actors.json display fields, runtime src/assets/data/naming/names_3126.json,
  out/world_3126/naming_3126.json. world_sites.py and economy_live.py resolve site / hub names by id. Pipeline: naming.py
  --write first (README). lore_md renders CULTURE/NAMING.md, CULTURE/HERALDRY.md, FRONTIER/FORTUNA.md and now refreshes
  document titles from data.
- Former Arabic branch removed (author decision): AL-QUDS-1 -> VAKA-MOANA-1 (Pacific Voyaging Compact: Aotearoa NZ,
  Hawaii, Samoa, Tonga, Fiji, Cook Is., Kiribati, Nauru, Tuvalu, PNG, Solomon Is., Vanuatu, FSM; seabed-nodule royalties,
  loss-and-damage funds, diaspora bonds). Capital Tauranga / Тауранга (mi: anchorage), polity Te Kāpehu / Капеху ("the
  Compass", formal Te Whakaminenga o Te Kāpehu), observatory Te Whare Tātai Arorangi, jump mathematics = "etak frames"
  (science, not myth). Machine id lalande_mecca kept. Dates, population, cohorts, regime parameters, fate kept; regime key
  scholar_theocracy -> navigator_council (same parameters). Culture vectors gulf_arab / pakistani removed; new
  pacific_polynesian / pacific_melanesian / aotearoa_nz (PROPOSAL priors). The legacy runtime record of AL-QUDS-1 is not
  exported (naming.json former_legacy = drop); NEW-HELLAS-1 -> HOMESTEAD-1 (Second Hellas -> Homestead / Хомстед).
- MODEL CAUSALITY FIX (user decision 2026-11-05, "not 1-3"): the honest Pacific culture vector collapsed Tauranga's core
  output (peacetime share 0.533 -> 0.0) because capabilities had a direct culture term (0.25 x culture axes) amplified by
  the cubic comparative-advantage allocation. Rejected (documented): culture tuning, capacity_x tuning (chaotic:
  0.29 / 0.18 / 0.45), market-order change, labour reservation (input-limited 11.8 cores), removing the culture term for
  all (Cognium -20 %, Citadel cores 8.2 -> 2.0), culture as a uniform level (Cognium -19 %, Free settlements -33 %).
  Chosen: industrial capability is a separate state - world.json colonies[].industrial_heritage (9 sectors, capability
  points; PROPOSAL/model, the sectoral profile of the accepted 9d44bee5 economy, same method for every colony; outposts
  inherit the owner's). lab.colony_profile: caps = 45 tech + industrial_heritage + 18 endowment + scale + 0.5 bias;
  culture is the social profile and acts on industry only through history (demography / technology). CAP_CULTURE removed.
  Result with the honest Pacific culture: drive_cores made Cognium 116.53, Tauranga 16.67, Citadel 8.23; Southern circle
  consumption Tauranga 16.74 / New India 20.47 / Maland 9.67; external demand 16.83 = Tauranga 8.97 + Cognium 7.86;
  share 0.533 (in [0.45, 0.60], not hard-coded), cutoff 0.124, mobilised 0.678; every actor's output value within 0.1 %
  of 9d44bee5. Tauranga bottlenecks: isotopes 100 % imported, electronics 94 %, precision 87 %. The 30 % quota only
  distributes existing output. Not done: core_maker is still a works-capacity multiplier on the research base
  (now part of the explicit industrial state, documented), not an installed physical capacity in cores / year.
- Nordic layer: Citadel / Skjoldborg (nb [ˈʂɔlːbɔrɡ], formal Protektoratet Skjoldborg, ON ceremonial Skjaldborg; RU
  «Скьольдборг» / exonym «Цитадель»; slang Борг, Щит, the Borg). Wolf 359 local name Ulven (folk etymology of Max Wolf's
  catalogue). Nyhamn (nn) / Nyhavn (nb registry), Jarnvik / Jernvik, Mjølner arsenal, Gruve 7, Arkivhvelvet, Heimdal
  vaktstasjon, Bifrost-varden (beacon BCN-003-BR), Toeren (asteroid IS-2). Territory language nordic_pmc -> nb
  (Novo-Thebaid christian_traditionalist -> en, not ru). Old Norse only ceremonial (check t21).
- Other corrections: Синь Хэ -> Синьхэ 新和 (old "core / river" etymology rejected; the real county 新和县 noted as VERIFY);
  Firmament endonym 天垒 (asterism 天壘城); missions 天命一号 / 天命二号 / 天河一号; Tau Ceti 天倉五 kept, local 天仓, capital
  新上海 / RU Синь-Шанхай (historical Новый Шанхай); Агор-Прим -> Новая Эллада (polity Νέα Ελλάδα, capital Αγορά, free port
  Σταυροδρόμι); Рестаурос -> Союз Новой Европы (la Restauration, slang «Ресторан»); Вердензальб -> остатки Гайи-Прайм;
  Уджамаа-Джахи -> Содружество Уджамаа (Jumuiya ya Ujamaa; "jahi" not attested); Сельта-Верде -> Сельва-Верде; Indian
  endonyms नया भारत, कर्मभूमि, धर्ममण्डल; Novo-Thebaid legacy note fixed ("прежнее название «Святой Сион» больше не используется").
- Fortuna: Fomalhaut / α Piscis Austrini / HD 216956 / HIP 113368 / Gl 881, Piscis Austrinus / Южная Рыба, 7.70 pc =
  25.13 ly, V 1.16, A4 V (HYG A3V), 北落師門; colony-era name Fortuna, zh exonym 命運之輪 / 命运之轮 (Mìngyùn zhī lún). Not in
  the StarAtlas flight catalogue (visible in the sky via HYG). canon/remote_systems.json holds physics; accessibility is
  computed (superseded 2026-11-05 u: Fomalhaut is now StarAtlas system 61, see below): nearest beacon Gliese 876 (Providence, 11.15 ly), Sol 25.13 ly (max hop 32), outside the 8-ly commercial
  unbeaconed limit, no beacon / fuel / repair / SAR, exit error x17 vs a beacon pair growing with ephemeris age (3σ 26 372
  km fresh, 1.9 M km at 120 years), Cobrett round trip 0.36 tank from Providence, 0.81 from Sol (no reserve -> extra tank
  or cache). Lexie / Лекси / 柏辛 / Bǎixīn = body:fomalhaut.lexie (spelling corrected by the user to Lexie), naming history
  AUTHOR_UNSPECIFIED; PROPOSAL physics 1.3 M⊕ at 4.4 AU, 0.86 S⊕, ~283 K, oceans, N2/CO2 1.4 bar, not breathable;
  compatible with JWST / RV limits; no reward, no resource.
- Runtime: EconomyModel HubEconomyInfo gets `actor`; EliteServicePagesCheck §6 finds hubs by actor id and compares owner
  names with names_3126.json (province hubs run under their actor) -> PASS.
- VERIFY (open): 盛和, 新和 vs 新和县, Shanghainese 上海, Maland origin + Maland Mpya, Skjaldborg heldr, skjoldborgere,
  Toeren; Russian ship names of non-Russian makers (Б-4 «Сокол», C-120 «Конвейер», Хэ-С240 «Тыловик», HT-20K «Мул») live in
  the render model registry - outside this pass; Fomalhaut as a flyable system needs EliteUniverseGenerator +
  navigation-cell regeneration - outside this pass.
- Gates: all colonization_lab checks rc=0 (naming 23/23, political, industry, lore_md, lore_html 142, calibrate_population,
  check-dataset, check-lore); lab.py twice IDENTICAL (sha256 f59e9182...); EliteServicePagesCheck PASS.

### User orders 2026-11-05 u (Fomalhaut system on the maps, Lexie generated) + v (final naming / political-economic cleanup after 51e9aacc) - DONE (one commit, not pushed)
- POLITICAL / ECONOMIC LAYER 3126: CLOSED / ACCEPTED. Structure = CANON (political_order.json); numeric simulation and
  calibration = PROPOSAL/model. Further changes only for a new gameplay need or a factual error.
- Fomalhaut is FLYABLE-CATALOGUE data, not world-data-only: StarAtlas local system id 61
  (src/assets/data/galaxy_details/systems/061_fomalhaut: system.json with observational_reference, stars/fomalhaut.json,
  observed/planet_lexie.json), materialised by EliteUniverseGenerator (16 bodies + debris disk = ALMA main ring
  136.3-149.8 au; stability PASS; no planet > 1 M_Jup; Lexie at 4.4 au, 1.3 M_E, 262-299 K, plate-tectonic continents,
  surface ocean, N2/CO2 1.4 bar, not breathable, preset terrestrial_temperate), atlas_visible + route_target, position = HYG
  HIP 113368 (merged with the sky star, no duplicate), localization world/star_systems/fomalhaut.json (Лекси / 柏辛).
  Interstellar cells unchanged (Fomalhaut already inside the navigation domain); generator determinism 62 systems, 0 differ.
  EliteServicePagesCheck §7 loads StarAtlasDatabase: 61 local systems, Fomalhaut on the maps, Lexie present -> PASS;
  client --dev-direct-flight loads with no catalogue error. NOT done: a live jump to Fomalhaut and a visual check of the
  Galaxy / System map. canon/remote_systems.json removed (physics now in the catalogue, one source); naming ids
  system:61 and body:system_61.Fomalhaut.Lexie.
- Endowment scale fix: endowment.py ranked resource indices over all catalogue systems, so adding an empty system moved
  every colony (Tauranga share 0.533 -> 0.331, off-Sol population -2.4 %). Now ranks are taken against a fixed reference
  set (canon/endowment_scale.json: the 60 systems of the calibrated catalogue); later systems are placed on that scale.
  Economy, colonies and demography are byte-identical to 51e9aacc.
- Lexie spelling: no `Lexi` anywhere (check t16b).
- Former AL-QUDS semantics removed: legacy actors.json id 7 ideology / culture / description / tags mirrored from
  naming.json polity.legacy_actor_profile (navigator council, Pacific navigation science); political_order texts and the
  Tauranga dossier no longer say "учёная теократия"; population_history story rewritten; legacy GalaxyDatabase
  nodes / routes / systems.json (Аль-Нур, Ан-Нур, Космический Халифат, Расад ат-Таухид, retired polity names) fixed;
  TRADERS-GUILD-1 crew "один грек и один таурангец".
- Type of power vs social-religious law: law family key theocratic -> religious_law (same parameters; still used by the
  real theocracies New Karma and Novo-Thebaid). navigator_council defaults are neutral (law customary_council, goods strict,
  family policy neutral, REGIME_FAMILY technocratic); Tauranga's religious-conservative law is the explicit colony field
  world.json lalande_mecca.law_policy {law_family religious_law, goods_policy faith, family_policy faith} read by
  world_sites, industry, economy_live, world_economy and demography. Outputs unchanged.
- VAKA-MOANA-1 motives from the new dossier: civilisational_insurance 0.45, prestige 0.2, scientific_vanity 0.2,
  religion 0.1 (church communities as the recruiting network, not the reason), ideology 0.05; elite_capital_flight
  removed; the 2185 fono raises patricians + scientists (not "faithful"). Voyage regime tendency is now "олигархия".
  Demography unchanged (colony culture uses cohort shares, not power).
- Retired-name ban is entity-aware: unique names of the retired colonial entities banned anywhere in the display layer;
  contextual words (Мекка, исламский, халифат, улем...) banned only next to a current Lalande entity and in any current
  entity name; real Mecca / the Islamic world of Earth history allowed (Europa motive text restored). Probes both ways (t07b).
- industrial_heritage: field_status status PROPOSAL/model, generated by knowledge.py (t26); culture does not set it (t19b).
- Gates: colonization_lab all checks rc=0, naming 29/29 (t07b, t16b, t24, t25, t26, t27 new), lab.py twice IDENTICAL,
  Southern circle 0.533 / mobilised 0.678 / cutoff 0.124 (= 51e9aacc), EliteServicePagesCheck PASS (economy by id +
  atlas), EliteGalaxyPointerCheck / EliteMapPointerCheck / EliteNavigationCapabilityCheck PASS, universe generator
  determinism PASS. CI: the repository has no CI configuration (.github/workflows absent); all results are local.
- Semantic grep (AL-QUDS / Мекк / Mecca / араб / ислам / теократ / theocr / улем / халифат / caliph): remaining hits are
  (a) the rule lists themselves (naming.json retired_authoring_names, naming.py checks), (b) real theocracies (New Karma
  caste_theocracy, Novo-Thebaid synod_theocracy, Gaia legacy actor, AMAZONIA voyage tendency), (c) Earth history
  (Europa "исламский мир"), (d) machine ids lalande_mecca / mecca_farms kept for compatibility, (e) dated archives
  (PROJECT_STATE history, LEGACY_HISTORY_IMPORT), (f) unrelated substrings (барабан, заработок, font files).

### User design decisions 2026-11-05 w (jump mechanics, space warfare) - DESIGN ACCEPTED, NOT IMPLEMENTED
- Documents (repo root): SPACE_WARFARE_DOCTRINE.md (analysis of space sims / SF / strategy articles mapped onto the 3126
  world: no cover -> ambiguity instead of stealth, war for nodes (beacons, depots, yards), logistics first, doctrine
  groups by bloc; PROPOSAL) and JUMP_MECHANICS.md (the accepted jump / J design).
- Accepted by the author: one decoupling field, three regimes (main drive, J, jump); J cap 0.8-0.9 c and J acceleration
  up to ~1e5 g (inner system <= 1 h); in-system short jumps along system beacons / green corridors to the outer planets;
  no beacon = lottery (exceptions: local smuggler knowledge, pirate beacon frequency + cipher, old self-propelled
  beacons); administrative zones red / blue / green with the transponder rule (on -> automatic fine, off -> police);
  automatic velocity sync to the local frame on exit (small residual error, planner hint, no autopilot); two planners
  (visible promise + hidden server-side deterministic result, honest 3 sigma along beacons).
- Author answers (same day): the star catches both interstellar and in-system jump steps (in-system jumps route around
  it via waypoints); the jump is instantaneous (screen ripple, new sky; HYPERION-1's 3 days = charging + in-system
  flights); charging is not visible, but the jump itself gives a detectable burst at departure AND arrival (place and time, not the destination; detection range always matters); a sump capture hides the real arrival - no burst there, but a "ghost" arrival burst appears at the intended destination; J is never mandatory; J and jump are forbidden near ANY object, a beacon included
  (fly away first). Still parameters: star capture radius (inner planets must stay reachable), keep-out distances by
  object mass / size, L4/L5 safety (default: natural, a beacon improves precision).
- Next (not started): implementation slices - data (sumps / jump limits / medium currents from the catalogue), visible
  planner, hidden server roll, exit sync, zones + transponder, J cap / acceleration / energy accounting.

### User order 2026-11-05 x - J drive slice 1 (cap, acceleration, keep-out) - CODE + LIVE MOON RUN
- src/game/simulation/JDrive.h: J figures - max acceleration ~1e5 g (980 665 m/s2, autopilot and route), speed cap
  0.9 c against the J reference body (autopilot / route setpoints and a hard clamp on every J flight), keep-out 20 km
  around hubs / stations / beacons (m_hubNavigationFrames) and 5 km around other ships: inside it no J field - a gentle
  push (<= 1 g, <= 300 m/s against that object) out first; the drive ramp (1 g doubling every 2 s) restarts outside.
  Route: the departure-hub boundary generalised to the keep-out; hub arrival aim moved to 25 km (outside the keep-out,
  inside the 50 km capture). HUD: refusal code 3 "J-поле не работает рядом с объектами".
- LIVE (build/EliteGame --dev-direct-flight, ELITE_DEV_SOL_MAP_SCRIPT 8:freeflight;10:autopilot:moon, D:/tmp/elite_j10):
  gentle 1 g / 300 m/s out of the Earth Orbital Hub keep-out (~48 s), ramp from 1 g outside, full 980 665 m/s2 at
  ~t+80 s, peak ~14 700 km/s, transfer ~130 s, descent, LANDED on the Moon after 242.9 s (1.42 m/s vertical).
  NOT yet: a flight long enough to hit the 0.9 c cap (Earth -> Mars), full test suite, energy accounting.
- AUTHOR CORRECTION (same day): J must end at an ORBIT point; descent, landing and the climb to orbit go on the MAIN
  ENGINES - otherwise "no J near objects" contradicts landing on J. Bodies were left out of the keep-out in this slice
  (the J landing still flies the 40 m/s2 J descent). Next slice: a J floor per body (proposal: max(top of the
  atmosphere + 50 km, 0.05 R, 50 km): Earth ~320 km - LEO hubs at ~420 km stay above it, Moon ~90 km, Mars ~170 km);
  body autopilot = J to a parking orbit above the floor -> J off into the ballistic frame -> landing / ascent on the
  ship's real main engines (needs the landing module on the main-engine flight path, outside J).

### User order 2026-11-05 y - J floor, landing from orbit on the main engines - CODE + LIVE MOON RUNS
- J floor per body (JDrive.h floorAltitudeM = max(atmosphere top + 50 km, 0.05 R, 50 km); Moon 86.87 km): below it no J
  field - the autopilot flies on the ship's own main engines (mainEngineAccelMps2: forward main-engine acceleration,
  legacy fallback the linear g envelope): a thrust-axis lander (acceleration = velocity error / 1 s + gravity
  compensation; the nose turns toward it at the hull's own pitch rate; thrust = its projection on the nose, within the
  real thrust) - tail down while hovering. Leaving a body below its floor: a straight 400 m/s climb against it first.
- Landing checks at engage: main engines >= 1.3 x surface gravity (land AND take off again), else REFUSED (HUD code 5);
  a body with an atmosphere takes only class surface_lifter (HUD code 6). Route orbit legs are raised to the floor.
- Landing sequence (author: "по касательной на орбиту, стабилизироваться, садиться с орбиты"): J to a circular
  parking orbit at the floor, in the plane of the approach and the site, the main-engine braking distance before the
  site (insertion tangential; close to the orbit the aim is the nearest point of the orbit circle - a fixed point
  had an equilibrium 6 s x v_orb short of it); J off, 15 s coast; de-orbit and landing on the main engines.
  LandingAutopilot takes the real authority (braking profiles scale with authority - gravity) and the hull half height
  along the vertical; ground contact uses the hull projected on the vertical (tail-down legs).
- J transfer attitude: the nose along the velocity against the target (prograde) instead of at the target centre
  (user: "нос болтается"); main-engine pointing ignores per-tick noise.
- LIVE (D:/tmp/elite_j10): orbit3 - ON ORBIT 86.98 km at 1638.95 m/s (circular 1638.9), coast 15 s, de-orbit,
  LANDED 524.9 s, contact 1.73 m/s vertical / 0.13 m/s horizontal; orbit4 (insertion tolerance 5 m/s) - ON ORBIT
  86.89 km, vertical -5.2 m/s. Earlier failures recorded: floor2 hard contact 18.4 m/s (per-tick axis projection of a
  deadbeat servo wasted thrust sideways - replaced by the thrust-axis lander); orbit/orbit2 hung ~10 km short of a
  fixed insertion point at ~5 m/s (fixed by the orbit-circle aim).
- NOT done: atmospheric (aircraft-mode, corridor) landing for shuttles; take-off from the surface to orbit not flown;
  belly-down landings / lay-down and stand-up of a hull (author question - analysis in the reply); 0.9 c cap not hit
  live; full test suite; J energy accounting.

### User order 2026-11-05 z - belly-down landing along the surface - CODE + LIVE MOON RUN
- Author: "посадку не кормой, а в обычном режиме и плашмя, двигаться вдоль поверхности". At engage the ship's ventral
  thrust (newtonianRcsPositiveMps2.y, else manoeuvreThrusterAccel, else strafeAccel) >= 1.3 x surface g selects a
  BELLY-DOWN landing (f.bellyLanding), else tail-down as before. Belly-down: hull level, ventral thrusters hold the
  weight, the main engine along the nose brakes / drives along the surface (retrograde heading above 30 m/s horizontal
  unless a reverse engine exists), side thrusters trim; a weight the ventral thrusters cannot hold pitches the nose up
  (<= 0.7 rad) so the main engine takes the rest. LandingAutopilot glide path: cruise height
  min(max(cruise, 10 x |v_h|), max(30 m, 0.15 x distance)) - the approach comes down along the surface.
- LIVE (D:/tmp/elite_j10/belly2): BELLY-DOWN chosen (ventral 7.5 g vs 0.165 g), ON ORBIT 86.89 km, DE-ORBIT 217 s,
  LANDED 534.2 s, contact 1.57 m/s vertical / 0.08 m/s horizontal, slope 4.1 deg. Failure recorded: belly1 fell back
  to tail-down (the dev ship's descriptor has no per-axis RCS telemetry -> ventral 0; fixed by the strafe fallback).
- Known gap (author remark "400 м/с как в обычном режиме, а ты пользуешься J"): de-orbit and landing still run inside
  the J autopilot container; moving them to the normal flight mode (real Assisted / Newtonian laws) is a later step.

### User order 2026-11-05 aa - Moon visuals (craters visible from far, no hard terrain switch) - CODE + PLANET LAB
- Diagnosis (Planet Lab captures, new views mid_space 3 R, pre/post_terrain 0.65 / 0.55 R, low_globe 0.30 R,
  terrain_on 0.15 R): Elite style showed a black disc with a rim (fixed 1.33 km contours faded by the anti-moire
  fwidth fade), then contour noise of the 8-bit real height map ("ущелья"), then at 0.6 R the local terrain switched on
  abruptly with a grid and polygonal, coarser relief (its outer rings run 11-22 px per cell; the 1024 globe is finer).
- ImportedSurfaceField, cratered bodies (AirlessRocky / Icy with imported maps - the Moon): the real height map is kept
  only as the macro shape (4x down-sample + gaussian sigma ~110 km: basins and maria); CraterLayer craters now span
  0.03 R (52 km radius on the Moon) down to 1.5 m at every band limit; complex craters (D > ~15 km) shallower
  (d ~ 1.04 km x D_km^0.3); the dark maria hold 0.2 of the highland density (albedo, tabulated on a 48^3 direction
  grid). Albedo stays the real map. Server and client share the loader - the same relief (landing heights change).
- Elite shader (cratered bodies): contour lines only where the slope > ~4 deg (crater walls, not basin slopes);
  per-pixel nested power-of-two spacing so lines stay >= ~7 px apart (craters readable from 4 R).
- LocalTerrain: activation below the altitude where its nadir cell is half the globe texel (Moon ~0.25 R instead of
  0.6 R), and no terrain level coarser than the globe texel - beyond its footprint the globe shows.
- Results (D:/tmp/elite_moonvis/after7, after8): cratered sphere at 4 R, 0.65 R, 0.3 R; terrain takes over seamlessly
  near nadir. Cost: the 1024 globe bake ~10 s instead of ~1 s (big craters sampled at globe scale; async, the 512 tier
  shows meanwhile). Pre-existing, not fixed: staircase contours on the terrain mesh close in; Modern style soft at near
  orbit (magnified globe). Planet Lab --check: 1 FAIL "rocky coastline landmark id changes 3443" - identical without
  these changes (baseline red, procedural body). EliteServicePagesCheck PASS (run from build/).
- (see bb below for the normal-flight landing that followed)
- LIVE on the new relief (D:/tmp/elite_j10/moonvis): BELLY-DOWN, ON ORBIT 86.89 km, LANDED 526.7 s, contact
  1.45 m/s vertical / 0.09 m/s horizontal, slope 4.6 deg. Author visual acceptance of the Moon in the game: PENDING.

### User order 2026-11-05 bb - landing in NORMAL flight (J off on the parking orbit) - IN PROGRESS, PAUSED BY THE AUTHOR, NOT COMMITTED
- Author: "у тебя скорость 400 м/с как в обычном режиме, а ты пользуешься J"; "переводи посадку в обычный режим полёта".
- Code (uncommitted, builds): J ends on the parking orbit; J-off below a body's J floor = BODY FRAME (travel frame moves
  with the ground under the ship, local velocity = velocity over the ground, server folds law motion + gravity and
  splits each tick; contact / rest / lift-off shared with J); landing virtual pilot SurfaceLandingPilot.h emitting
  ordinary controls of the ship's NATIVE law (Assisted: level hull, target speed via +/-, lift input; Newtonian: main
  + per-axis RCS + attitude); pilot controls override the input before the flight step; the J-container descent
  (old phase 4) deleted; manual J-off below the floor also enters the body frame. DEV trace ELITE_DEV_BODY_TRACE.
- LIVE failures (D:/tmp/elite_j10/normal1..7), root causes:
  normal1/2: Newtonian tail-down pilot, nose ~17-57 deg off, horizontal never < 2 m/s -> module never left Transit;
  normal3: CRASH 673 m/s - the pilot FORCED the Newtonian law on an Assisted plant (dev ship): main requested 73.5,
  delivered ~0 (author saw HUD "control Newtonian, engine group Assisted", sideways); fixed: the native law only;
  normal4: Assisted pilot, retrograde/prograde nose flip at 30 m/s horizontal (~110 deg swings); fixed: nose along the
  needed horizontal velocity change, blended;
  normal5: at the 500 m/s controlled envelope the law admits no speed-growing thrust -> engines 0, falling; fixed:
  wanted velocity capped at 0.9 x envelope;
  normal6: stable, but cruises ~400 m/s at 55-65 km for 500+ s without reaching the site (distance not logged);
  normal7 stopped at the author's request.
- Author (2026-11-05): an ARCHITECTURE bug found - without a hub, J-off loses the velocity reference (relative speed
  0) while J flies against the body; the author is deciding how to resolve it. Open contradiction raised: orbital
  speed (1639 m/s) vs the 500 m/s normal-flight envelope (options: orbit legitimate in normal flight / orbit = J only).
- Next: wait for the author's architecture decision; nothing accepted.

### Author decision 2026-11-05 cc - flight architecture: engines, flight modes, velocity references (replaces the bb frame design)
- ONE physics (global state, star systems at rest in our 0). Modes change only what is called ZERO velocity (HUD,
  the Assisted reference, the envelope), never the motion; a reference switch gives no impulse (Assisted re-captures
  its target as the current velocity in the new reference; the nose is not slewed by the switch).
- Engines: normal (~7 g) / J (huge g) - J becomes an ENGINE SWITCH only; + the jump. References switch AUTOMATICALLY.
- Velocity references: (1) interplanetary/interstellar - global; (2) inside a body's sphere of influence
  (r = a (m/M)^0.4, smallest containing sphere, hysteresis) - relative to the body; (3) STABILIZATION - holding a
  reference trajectory: a ballistic orbit point, a hub (its orbit), or a point on the surface (rotates with the body;
  thrust holds it against g) - surface = a kind of stabilization reference, not a fourth frame. The HUD names the
  reference (SPACE / body / ORBIT / GROUND / HUB).
- Flight modes: NORMAL (no speed limit) / STABILIZATION (current velocity = 0, manoeuvres within the 500 m/s envelope;
  the author keeps 500: combat comfort, manoeuvres in a bounded volume around the reference; with a hub as the visual
  reference neither fall nor escape; without one the ship may fall / escape - known, accepted). Stabilization turns
  on automatically on a closed orbit (periapsis above surface / atmosphere, inside the sphere, no thrust for N s) + a
  STABILIZATION ON/OFF button. Without stabilization the 500 m/s are not a limit (orbit = 1st cosmic is legitimate).
- Assisted under gravity: the target velocity follows the BALLISTIC continuation (falls with g) - with no input the
  ship follows its orbit, thrust only for the pilot's commands; on a surface reference it holds against g. Assisted
  rule: main thrust along the current velocity of the active reference; Backspace = partial Newtonian without '+'.
- Landing goals: (a) wild landing (soil sample, rescue): AUTOMATIC only - the autopilot picks a flat spot from the
  terrain field (author: a map click cannot find a pad at map scale); (b) settlement: a map mark (surface sites already have lat/lon in WorldSitesCatalog), a generated
  settlement by conditions (airless body / asteroid: underground entrances, 2-3 small domes, antenna, pad; asteroid:
  a passage in the rocks + pad), deterministic on server and client from one source; "land" -> the ship finds the pad,
  or the player clicks the map. Orbit insertion phased by the J autopilot: the plane through the site, the insertion
  point upstream of the site by the descent arc (site position at landing time) - no waiting for the site to come under.
- Next: slices 1 reference selection + J as an engine switch + HUD label; 2 Assisted ballistic rule, envelope only in
  stabilization, button + auto-on; 3 landing over these (map point / flat spot); 4 site-phased insertion; 5 settlements
  (marker, generation, pad). The uncommitted bb body-frame code is reused as the surface reference; nothing accepted.
- (2026-11-06) J keep-out rule (author): the J engine switches on only well away from other objects; else refused.

### Slice cc-1 2026-11-06 - J engine switch, stabilization switch, velocity references on the HUD - LIVE CHECKED, NOT ACCEPTED
- Code: motion.jEngine + motion.velocityReferenceKind (0 hub, 1 space, 2 body, 3 orbit, 4 ground; wire schema 35);
  commands ToggleStabilization (Ctrl+K, was ToggleFreeFlight) and ToggleJEngine (Ctrl+J, appended); server
  toggleStabilization / toggleJEngine / jEngineBlocked (JDrive.h keep-out 20 km objects / 5 km ships, every body's
  J floor) in GameSimulationFreeFlight.cpp. Normal flight = the old free flight; stabilization = hub capture / ground
  (below the J floor AND within 500 m/s over the body - new speed check) / own ballistic frame. The J engine = the J
  field along the main-engine thrust (1 g doubling every 2 s, x command; the END brake clamped at zero vs the
  reference); it drops out on entering a keep-out / below a floor (refusal 3 / 4), refused at engage (7 / 8). The
  Assisted velocity bending by the J field now only with the J engine on. Automatic stabilization: armed by the J engine
  going off, a new sphere of influence or the orbit closing; fires after a 5 s coast (no thrust / input) on a closed
  orbit with periapsis above max(safe altitude, ground + 1 km); disarmed when the pilot leaves stabilization or the
  autopilot engages. HUD banner for normal flight (engine, speed vs space / body), orbit and ground stabilization
  (manoeuvre speed, 500 limit, "no hub reference"); refusals shown 6 s even in a hub frame; flight vector "J " only
  with the J engine. DEV script verbs: stabilization, jengine.
- LIVE (D:/tmp/elite_cc1 run1-3, Earth hub): J refused at the hub (10 / 14 km < 20 km) PASS; Ctrl+K -> normal flight
  PASS; J engine ramp 2 -> 4 -> 8 -> 16 g on HOME, END brakes with J PASS; J dropped by ship 10 (ship_travel_10, a real
  hub ship at 4 km) - correct; automatic stabilization after the J drop and a 5 s coast on the closed orbit PASS;
  manual Ctrl+K -> orbit stabilization PASS; HUD banners seen on screenshots (normal / orbit), the long key line was
  cut at the right -> texts shortened (not re-shot).
- Not run: the compiled contract tests (wire round trip for schema 35) - not built in build/ (separate project).
- Open: the landing autopilot still ends J with enterBodyFrame (phase 9 -> 10, bb) - slice 3 rewires it; the J autopilot
  ignores motion.jEngine (it owns its drive); a manual J at 0.5 c into a keep-out drops J with the speed kept.
- Startup blocker found and fixed (not of this slice): FE-30 (fb5baf57) put the jump core / jump-charge tank (36A5 /
  37A5) into the Cobrett passports but not into the runtime - the Cobra descriptor's passport validation threw inside
  ObjectDescriptorRegistry's call_once on the client thread; under MinGW winpthreads the once stayed locked and the
  server thread waited forever (loading screen at 25 %, ~0.5 s CPU). Fixed: passive modules ship_jump_core /
  ship_jump_charge_tank in EliteCobraMk1.cpp, products um.core.z05 / std.tank.jumpcharge
  (src/game/catalog/jump/JumpSystemPassports.h, ProductCategory::JumpDrive, data from ship_catalog_3126.json) in
  CatalogRegistry. Lesson: an exception inside that call_once hangs the game instead of failing.
- Next: slice 2 is mostly in place (envelope only in stabilization, Assisted ballistic in normal flight); then slice 3 -
  the landing over the new modes (J to the orbit -> stabilization -> normal flight de-orbit -> ground stabilization).

### Slice cc-3 2026-11-06 - landing over the new modes - LIVE: 2 of 2 soft landings on the Moon, NOT ACCEPTED (author)
- Chain: J autopilot to the parking orbit at the J floor (86.8 km, 1639 m/s) -> phase 9 orbit hold 15 s (normal flight
  without thrust = ballistic; HUD "на орбите (стабилизация)") -> phase 11 DE-ORBIT in NORMAL flight -> HAND-OVER ->
  phase 10 GROUND STABILIZATION (body frame, 500 m/s envelope) -> touchdown by the landing module (Transit / Descent).
- Phase 11 (updateNormalDescent, GameSimulationFreeFlight.cpp; pilot flyNormalFlightThrust, SurfaceLandingPilot.h):
  ordinary controls only - the hull points the wanted acceleration, '+' = main along the nose, gated by the alignment
  (cos 20 -> 4 deg; under thrust the hull turns slowly, crew jerk limit). Wanted velocity over the ground: toward the site
  min(sqrt(2 0.35 a_main arc), arc / 15 s, circular speed), down along a glide reaching 1500 m as the arc closes (tgo),
  never down below 800 m; a = horizontal err / 4 s + vertical err / 3 s + g - v_h^2 / r, the vertical first within
  0.95 a_main. Hand-over: arc < 3 km, < 80 m/s, height < 2.55 km (or < 600 m and < 450 m/s).
- Phase 10 terminal, Assisted plants: the pilot puts Assisted into the Backspace mode (assistedFreeRotation: no speed
  servo, keypad axes = vectoring-main accelerations) - flySurfaceLandingVectoring: hull level belly down, nose to the
  site, per-axis velocity hold against gravity (tau 2 s horizontal, 1 s vertical); released when the pilot stops.
  Newtonian plants keep flySurfaceLanding.
- ROOT CAUSE found (also behind bb normal1..7): the body frame folded / split WORLD vectors into the local velocity
  directly while the frame kept the hub's localToWorldBasis inherited at the normal-flight start - the laws saw a
  rotated velocity (land2: the terminal pilot pushed away from the site to 490 m/s, envelope cut, CRASH 82 / 495 m/s).
  Fixed: enterBodyFrame sets an identity basis (the frame is non-rotating; local == world).
- LIVE (D:/tmp/elite_cc1): land1 FAIL - de-orbit good (hovered over the site at 4 km, 69 m / 5 m/s), the old Assisted
  terminal pilot ran away (speed servo along the rotating nose, 170 m/s) then circled 20-50 m up for minutes; also the
  descent first accelerated (wanted 2032 > 1639 m/s) -> capped at circular speed. land2 CRASH (the basis bug above).
  land3 PASS: LANDED vertical 1.45 m/s, horizontal 0.12 m/s over the ground, regolith, slope 2.4 deg, 575 s.
  land4 PASS (repeat): LANDED 1.45 / 0.13 m/s, slope 2.2 deg, 575 s; hand-over 1.95 km, 69 m, 80 m/s in both.
  Screenshot (terminal): banner "СТАБИЛИЗАЦИЯ НАД ГРУНТОМ Луна - скорость над грунтом", autopilot panel "к площадке".
- Open: the terminal module measures velocity against the body centre (the Moon's turn 4.6 m/s shows as horizontal
  speed; contact uses the ground); the hand-over arrives at ~150 m/s vertical (h/4 profile) - steep but within the
  envelope; tail-down vs belly-down: phase 11 flies tail-down (nose = thrust), the terminal belly-down; Newtonian
  plants, a body with faster rotation, an atmosphere (shuttle corridor) and the take-off not flown yet.
- Next: the author's in-game check of the landing (PgDn on the Moon); then slice 4 (site-phased orbit insertion).
- Author 2026-11-06: "корабль при посадке проваливается под грунт". Diagnosis: the server stood the hull on the field
  sampled at a 30 m band (GameServer::terrainHeight) while the rendered terrain near the ground shows it down to ~1 m
  (finest level 0.25 m spacing; CraterLayer to 1.5 m) - small craters, rims and ejecta were above the server's ground.
  Evidence (land5): with ~1 m detail the site search saw roughness 3.6 m (0.9 m on the 30 m relief) and picked another
  site; at contact centre 3009.88 m vs 3011.32 m (30 m), the footprint max 2.2 m above the centre. Fix: the ground
  height at the rendered detail (band 1 m), slope / appearance on the 30 m relief, TerrainSample::smoothHeightM for
  the contact log; footprint + 4 corners (the 26 m wide Cobra). land5: LANDED 1.57 / 0.06 m/s, slope 3.4 deg.
  The visual result (no sinking) is NOT checked - the author's look in game is the gate.

### Slice cc-5a 2026-11-06 - landing at a settlement: pads in the terrain, "site:<id>" autopilot - LIVE 1 of 1, NOT ACCEPTED
- Author: "что дальше - посадка возле поселения". Data: the Moon's surface sites in WorldSitesCatalog (Шеклтон -89.6 /
  129.0, Имбриум 32.8 / -15.6; the map and the autopilot share the lat / lon -> body direction convention).
- src/world/celestial/settlements/SurfaceSettlementLayout.h (header-only, deterministic from the site id / population /
  body radius): airless bodies - an entrance at the catalog point, 2-3 domes 75-125 m out, an antenna mast 120 m, pads
  in a row along the settlement axis from 320 m (1 / 2 / 3 by population); small bodies (< 400 km) - a rock passage and
  pads from 150 m. Player role: the pads are where the player lands (trade / missions / rescue), the structures mark the
  settlement from the approach. The catalog is loaded once (call_once, never throws out of it).
- src/world/celestial/surface/SettledSurfaceField.h: the field decorator - flat discs (the 30 m relief at each centre,
  smoothstep blend 0.8 r, min 25 m) under the pads and footprints; applied once in loadBodySurface (the ONE field of the
  renderer and the server), airless bodies only (surface pressure < 100 Pa: Earth would have got 52 domed settlements).
- Autopilot: landingSpec "site:<catalog id>" -> the body from the catalog, LandingMode::Pad on pad 1; engaging from a
  hub leaves the stabilization itself; the departure body is set at once (was empty when engaged from a hub). Map: O on
  a surface settlement = land there. DEV: autopilot:moon@site:<id>. "BELOW THE SURFACE" now against the sampled terrain
  (a mare lies below the reference sphere - Imbrium's pad at -506.6 m logged it falsely).
- LIVE site1 (Имбриум from the Earth hub): pad checked from 173 979 km (roughness 0 m, slope 0.93 deg), orbit 240 s,
  hand-over 1.95 km / 58 m / 80 m/s, LANDED 1.52 / 0.14 m/s, 0.045 m from the pad centre, 632 s. site2 (short): only the
  Moon gets settlements; Шеклтон pad 1 chosen, departure = Earth. Not flown: Шеклтон (polar) to touchdown.
- Next: slice cc-5b - the structures rendered on the surface (pad with markings / lights, entrance, domes, antenna),
  then the author's look; small bodies; atmosphere styles.

### Slice cc-5b 2026-11-06 - settlement structures rendered; terrain drawn at its true height in lowlands - NOT ACCEPTED
- src/world/celestial/settlements/SettlementMesh.h (CPU, no GL): from the layout - pads (concrete disc, yellow ring,
  "H", number bars, 12 rim lights), the entrance (apron, metal arch with a dark opening, portal frame, lights), domes
  (base ring, spherical cap, airlock), the antenna (mast, tilted dish, red top light), a rock passage (small bodies);
  body-fixed relative to the settlement anchor, on the field's flat footprints, 1 m into the ground; albedo > 1.5 =
  emissive (the mesh shader shows albedo - 2 unlit; asteroids unaffected). Renderer: built once per body when its
  field is ready (airless only), drawn with the mesh program after the nearest body's terrain within 250 km (Elite
  style fill + lines), and in the ships' near-pass depth within 20 km (settlementDepth program). ~6.5-7.3k vertices each.
- Author 2026-11-06: "все объекты ниже уровня земли метров на 20-30". ROOT CAUSE: LocalTerrain drew every land sample at
  max(height, waterLevel + 0.5) - an airless field reports water level 0, so every lowland below the reference radius
  (maria, crater floors; Imbrium -506 m) was DRAWN lifted to +0.5 m while the server (and the ship, and the structures)
  stood on the true height. Fixed: the lift only in the shoreline band (coastM < 400 m). Captures st_675 / st_690
  (before: the terrain grid above the pads and the ship) vs st2_600 / st2_615 (after: the pads and the ship on the grid).
  This may also be the author's earlier "проваливается под грунт" when landing in a mare / a crater.
- LIVE st2 (Имбриум): LANDED 1.52 / 0.14 m/s, 3.7 cm from the pad; captures by the game's own back buffer
  (ELITE_DEV_FRAME_CAPTURE), not screen grabs. The entrance / domes / antenna not in the captured views (not yet seen).
- HUD: phase 11 shows "МАРШЕВЫЕ АВТОПИЛОТА" (was "J-ПРИВОД АВТОПИЛОТА").
- Next: the author's look in game (structures, lowland terrain); Шеклтон (polar) landing; small bodies; styles for
  bodies with an atmosphere.

### Slice cc-5c 2026-11-06 - Шеклтон (polar, lat -89.6) settlement landing; ground slope under the hull - NOT ACCEPTED
- Author: "давай посадку на Шеклтон проверим". LIVE sh1 (autopilot:moon@site:site.000.sol.shackleton): J to the
  parking orbit 86.9 km / 1639 m/s at t+194 s, 15 s hold, de-orbit in normal flight (near-vertical over the pole),
  hand-over at 1947 m / 0.56 m from the site / 79.8 m/s, LANDED t+564.7 s, 1.53 / 0.10 m/s, 2.6 cm from pad 1, ground
  4108.28 m = the flattened pad (detail = 30 m relief). FAILED detail: slope reported 32.58 deg -> the contact counted
  as tipping over (not soft: impact serial + 0.5 equipment damage on every module) and the site choice logged 32.6 deg.
- ROOT CAUSE: GameServer::terrainHeight measured the slope one-sided 60 m from the centre - past the pad's flat disc
  (36 m + blend) onto the crater-rim slope beyond. Fixed: central differences +-15 m (30 m base, the hull's scale)
  on the 30 m relief.
- LIVE sh2 after the fix: site chosen slope 0 deg; LANDED t+564.5 s, 1.53 / 0.12 m/s, 2.6 cm from pad 1, material 2
  (regolith), slope 0 -> soft landing, no damage. A [CRASH] SEH in opengl32.dll was logged at the forced timeout
  shutdown (after "Failed to unregister class Chrome_WidgetWin_0"), not in flight - teardown under a kill, not chased.
- F3 captures sh2/f_600..f_650: the camera far from the ship - the crater rim and markers, structures not legible.
- Open: the pad sits on the crater-rim slope - the 29 m blend ring around the flat disc may be a steep cut (to be
  seen in game); the slope over a 30 m base also feeds the wild-site search (maxSlopeDeg) - more local than before.
- Next: the author's look in game at Шеклтон / Имбриум; small bodies; styles for bodies with an atmosphere.

### Slice cc-6 2026-11-06 - Assisted controls: J acceleration throttle, free hull, bounded turn; freeze diagnosis - NOT ACCEPTED
- Author order (2026-11-06): J = the stick sets ACCELERATION (exponential law; HOME full +, END full -, INSERT
  zero, +/- exponential trim); J off = wind the acceleration down to the main engines' level first; Backspace = a
  free hull like Newtonian, the velocity never corrected, zero acceleration along it; normal flight must limit
  the hull's turn (the velocity changes far slower than the hull turned); speed widget: red thrust bar + the
  acceleration under the speed; the terrain lattice grid under the ship commented out.
- Done (source + build + live DEV runs, D:/tmp/elite_cc1/j3*, rot5):
  - DynamicMotionState jAccelCommandMps2 / jAccelMps2 / mainEngineMaxMps2 / jStopping (wire schema 35 -> 36;
    contract tests not run). J block: command x2 per 0.5 s of +/- (from 1 g, to 1e5 g), field follows x2/s up,
    x4/s down; along the nose; a negative field stops at rest against the reference; main engines idle under J
    (freeFlightMainCommand returns 0). Live: HOME 2 -> 128 g in 8 s, INSERT winds down, '+' 3 s -> 59 g, END
    reverses through zero, Ctrl+J off at 2.8 g -> "J engine OFF (main engines)".
  - Ctrl+J "stays on" (author): the radar kept its J forward sector (FWD 2000 km) on freeFlight alone - now
    freeFlight && jEngine. The server toggle itself was correct (live j1 / j2).
  - Backspace: the free-flight prograde hold (ForwardToVelocity when not steering) ignored assistedFreeRotation
    - ROOT CAUSE of "returns to the velocity"; now no hold / no lead limit / no bending with a free hull (live:
    align 0 for 20 s after a yaw).
  - Normal flight (no J): the J bending model with the rate capped at a_main / v (live: nose stops at 10 deg,
    velocity turns 0.45 deg/s at 9.35 km/s = 7.5 g / v). The bend is assisted (no propellant accounting).
  - HUD: speed widget red bar (J: log 1 g..1e5 g, normal: main fraction; orange = negative), "J +36 g (+100000 g)"
    under the speed; banner line "УСКОРЕНИЕ J ... (задано ...)" / "J ВЫКЛЮЧАЕТСЯ" with the J keys.
- Freezes: hitch log now has sub-phases (ev_poll_hotkeys / ev_webview / ev_prepare_frame, r_starfield /
  r_celestial / r_far_passes / r_terrain_depth_labels / r_ships, remainder = stations pass) and
  ELITE_DEV_HITCH_MS. Author session 10:01: in flight ~50 ms in the stations / prepared-objects pass (suspect: LOD
  lazy uploads); map screens 35-86 ms prepare_frame and 90-270 ms hotkey-poll on F-key screen switches. Not
  reproduced in --dev-direct-flight rotation runs (0 frames > 35 ms). OPEN.
- Ship reference review (subagent): Cobrett A3 = ONE jump core um.core.z05 + 3 x std.tank.jumpcharge (1 t each);
  runtime has 1 tank entry and a 70 MW service reactor vs passport 150 MW; A3 dry mass ~67 t unaccounted (reuses
  the N1 economic class). Calibrated jump rule exists (ship_economics.json jump_rule, logistics.py): Cobrett
  1.1 t per mean jump, 2.5-3 jumps per 3 t tank. Proposed J rule: J leg = R x mean jump, R 1/3..1/2 by core load;
  rate ~ (a / 1e5 g)^1.5. Not implemented (J energy accounting still absent).
- Next: fuel (J charge) by the rule above after the author's calibration choice; Detail map hub click /
  context-menu info; F12 hub map (hub or player's space cube); Fomalhaut (Лекси) planet names; route waypoint
  window with per-point programs + planner-inserted waypoints (refuel, jump intermediates) + ship rendezvous.

### Slice map-1 2026-11-06 - Detail-map hubs select / go to hub; F12 = hub or the player's cube - LIVE DEV RUNS, NOT ACCEPTED
- Author report: (1) a Detail-map hub click opened the info card and the hub could not be entered; wanted: click
  = SELECTED + its name at the marker + its info and composition in the right panel, info card from the context
  menu (right click / Ctrl+click), a working "go to the hub". (2) F12 "chaos, sometimes nothing": it must show
  the hub or the navigation cube with the selected object, by default the player's cube, following the player
  into the next cube.
- Root causes (live baseline D:/tmp/elite_maps/b*, c*): (a) every non-celestial overlay glyph opened its card on
  a single click (MapObjectOverlayState::handlePointer); (b) F11 opens Details without composing a System map,
  so m_loadedSystemMapId stayed -1 and setSystemMapHubMode returned silently - the HUB drill from such a
  Details map did nothing; the right panel also stayed the bare "STAR ATLAS" block (its facts required the
  loaded System snapshot); (c) F12 outside a hub frame built a 2500 km terminal cube in the HELIOCENTRIC frame;
  in LEO its centre usually lies inside the Earth, so it was promoted to the whole Earth Details (c1/c2); else
  the cube was fixed while the player + hub co-move with Earth at ~30 km/s, crossing it in ~80 s - after that
  the cube was empty ("nothing at all", c7); F12 also ignored any map selection.
- Done (source + Debug build):
  - MapObjectOverlayItem::selectOnClick (Detail hubs + world sites, LocalMapPresentationBuilder): click selects
    only, the name is drawn next to the selected marker, double click -> MapIntent::openHub (Hub map);
    context-menu target selection also selects the Detail hub; INFO opens the card (unchanged menu).
  - Right panel: Details read the loaded Details system; the Hub map shows its hub too; composition =
    economy hub_modules_3126 blocks ("Пассажирский блок: челночный док (HexDock) x141, гостиница x21 ...",
    EconomyModel::HubEconomyInfo::composition), fallback = replicated hub-attached modules; F11 / F12 compose
    the parent System map first; setSystemMapHubMode on Details uses m_loadedDetailTarget.systemId.
  - F12 (preparePlayerNavigationMapLevel Local): an object selected on the open System / Detail map -> its hub
    (Hub map) or the terminal cube containing it; else (and from flight) the player's hub, else the player's
    cube. Such a cube is a SpatialVolume with focusClass Ship / Hub + the object id: never promoted to a body,
    and followLocalCubeFocus() (Detail live refresh) moves it to the neighbour cube when "player" / "entity:N"
    leaves it. MAP_NAVIGATION_CONTRACT (F12, body promotion) and MAP_OBJECT_OVERLAY_CONTRACT (selectOnClick)
    updated; check_navigation_context token updated.
  - DEV: ELITE_DEV_SOL_MAP_SCRIPT "pointer:click|dclick|rclick:<objectId>", "pointer:menu:<command>" (scripted
    pointer through the production input path), "overlay" (map items + panel + loaded targets); SOL steps log
    their GLFW time.
- Live DEV evidence (--dev-direct-flight, back buffer D:/tmp/elite_maps/): g1_selected (Тихий selected, name at
  the marker, panel: facts + Состав by block, HUB enabled), g2_menu (right click menu), g3_card (INFO card),
  g5_hubmap (double click -> Hub map of Тихий, from a fresh F11 Details); h2_f12_selected_hub (hub selected on
  the System map -> F12 = its Hub map), h3_f12_from_flight (F12 from flight = the player's hub);
  i2_f12_selected_ship_later (ship entity:11 selected -> F12 = its cube); f1..f4 (free flight + HOME: F12 =
  the player's cube, log "[LocalMap] player left cube 42957,2,-40907 -> shown in cube 42957,2,-40908").
- Gates: check_navigation_context PASS, check_object_overlay PASS, check_route_plan PASS; check_architecture
  RED pre-existing (DetailMapGeometryPass includes SystemMapRendererInternal.h, untouched); SystemMapBehaviorTests
  does NOT compile pre-existing (MockSystemMapInteractionContext abstract) - ctest ran a stale binary.
- OPEN: author's mouse acceptance; the cube does not draw the planet that fills part of it in LEO (SpatialVolume
  scenes have no body rendering); a cube of a world site does not follow (only player / entity ships); crowded
  Earth markers: a city click is won by the larger hub (size arbitration); F12 again while already on the local
  map is a no-op (re-centre needs F1 then F12); the panel mode label stays "ДЕТАЛИ" for the F12 cube.

### Slice cc-7 2026-11-06 - station freezes, J charge, Fortuna names, route programs R1, take-off - NOT ACCEPTED
- Author order: find the freezes (cache model loads, GPU offload), then the list without questions; the fleet
  reference = coverage / interchangeability / power+cooling -> how many models to draw.
- Freezes ROOT CAUSE (live app1): ModelAsset station LOD meshes uploaded lazily on the main thread at first
  selection - 3..105 ms each (LOD0 geometry 1, 201k tris = 105 ms; the paired 50-60 ms spikes = two LOD0 pieces).
  Fixed (419348b1): MeshGPU split prepare (CPU) / allocate + uploadChunk (GL); ModelAssetGpu prepares every
  (LOD, geometry) on a loader thread (coarse first) and pumpPrewarm(3 ms / main frame) uploads 32k-vertex
  chunks ahead of the selection; a mesh needed earlier finishes synchronously. Live app2: the same flight has
  no station spikes; the one early synchronous LOD0 piece 9.6 ms (was 105). Map screens: one 50-145 ms frame per
  F-key switch (first build), not continuous. GPU offload: the CPU frame is ~4-10 ms; no remaining per-frame
  CPU load worth a compute pass; the vertex-expanded MeshGPU (3 vertices / triangle, ~110 MB for the station)
  could become indexed + gl_PrimitiveID barycentrics (memory, not fps) - not done.
- J charge (20ffc825): JDrive.h rule - jump = q d (alpha + M), q 5.6e-5 (-17 % so 2 jumps + 3 J legs fit 3 t);
  J leg = 1/3..1/2 jump by core load; rate ~ (a / 1e5 g)^1.5; Cobrett 3 t (3 tanks), core 500 t; J refused /
  collapses when empty (refusal 9); banner "заряд J x.xxx / 3 т"; a hub refuel stay fills it. Schema 37.
- Fortuna (Fomalhaut) names (c9eb68c9): 15 PROPOSAL names, the kitchen series by conditions (Сковорода, Жаровня,
  Огарок, Скороварка, Духовка, Мыльный пузырь, Чайник, Парилка, Газировка, Морозилка, Пробка, Кубик льда,
  Крошка, Сухарь, Гренка); naming.py --check 29/29.
- Fleet review (subagent, report D:/tmp/elite_fleet_review.md): 21 models / 19 types / 6 blocs; gaps (Sol no
  jump freighter / liner, Cognium no security, Pact no passenger / shuttle / cutter, Southern circle and Citadel
  thin, Lalande no hulls, no miner / raider anywhere); 102 SKUs, 45 single-ship, ~33 visible equipment models;
  reactor oversizes (ST-6 3.84x, GT-3000 2.4x, He-S240 2.22x ...), PAO-7 / B-4 short at full fire, B-4 cooling
  251 % of its plan area; impossible part sizes (hangars, boat bay, train module, cryo tank). Recommendation: 19
  hulls (15-16 with shared kits), a minimum set of 9 covering all blocs, ~20 / ~33 equipment models.
- Route programs (b31126d8 design PROPOSAL ROUTE_PROGRAMS_DESIGN.md, 46698f0d R1 WIP): waypoint program +
  actions model; the panel cycles the programs the target admits, refuel / repair / pause / take-off toggles;
  the arrival-profile icons only for a ship target (they were shown and accepted on every finish); the route
  list shows the program line; launch honours the programs (dock services, orbit altitude; a landing finish ->
  the landing autopilot, intermediates not flown yet). Not yet visually verified in a capture; pause not
  executed by the server yet.
- Take-off: a route leg from a landed ship did nothing (the leg runs only off the ground; live to3: stayed on
  the pad 6 min). Added a take-off phase (vertical main-engine climb to the J floor, <= 0.9 main) before the leg.
  DEV verb leg: sent index 0 (= cancel) - fixed. LIVE to4: landed at Шеклтон pad 1 (2.6 cm), leg -> Earth orbit
  500 km: TAKE-OFF 5.9 m/s at 6.5 m, 400 m/s climb at ~0.16 g, complete at 86.88 km (J floor 86.87), the J leg
  ARRIVED after 156.7 s, 18 m from the aim, 3 m/s vs destination.
- Next: route window capture (not yet seen); R2 intermediate landings + take-off in a route, pause; R4 charge
  planner with refuel insertion; R5 ship rendezvous.

### Slice cc-8 2026-11-06 - router R2-R5 (partial), Fortuna mythology names, fleet reference gaps, war doctrines - NOT ACCEPTED
- Author: rename the Fomalhaut bodies "the way the other bodies are" (Sol = classical mythology), Lexie untouched;
  finish the router; complete the ship list in the HTML reference; then (later message) war-doctrine ideas.
- Names (e10ce647): Fortuna = Roman Fortuna + "mouth of the fish": Атропос, Лахесис, Клото, Немезида, Ананке,
  Спес, Деркето, Оаннес, Фелицитас, Морта, луны Окказия / Децима, карлики Тюхе / Плутос / Форс (PROPOSAL).
  naming.py now mirrors body names into generated/bodies (designation = ru, alternative_names = intl; only those
  two lines change) - the runtime reads designation. naming --check 29/29.
- Router (built; live DEV runs rt2 / rt4 / rv1; contract tests not run; wire schema 38):
  - Land legs (FreeFlightLeg::Kind::Land -> the landing autopilot, wild or "site:<id>" pad), take-off before a leg
    from the ground, service stays, pause points (pause action / landing without take-off) + PAUSE / RESUME
    (RouteLeg index 3 / 4), motion.routeState / legIndex / legCount, HUD hint "МАРШРУТ НА ПАУЗЕ", route list
    footer (state + summary), РАССЧИТАТЬ (summary without launch).
  - J charge walk + refuel insertion at the least-detour hub ("[план]" in the list), 10 % reserve, a refuel stop
    only has to be reached (first version looped 4x inserting at the stop itself - fixed). DEV
    ELITE_DEV_ROUTE_CHARGE_T=0.2: one refuel stop inserted at earth_orbital_hub (capture rt/insert.png).
  - LIVE rt4: Earth hub -> guide gate -> LANDING Шеклтон pad (2.6 cm) -> PAUSED -> RESUME -> TAKE-OFF to 86.88 km
    -> gate -> leg to Earth orbit (arrival not inside the run window; the same leg ARRIVED in to4).
  - Bugs found live: the pad spec was lost (sourceObjectId rebound to the body) -> actions.landingSite; a Land leg
    had no server position (remaining from the Sun, the guide passed at 19 600 km/s) -> body centre; the
    stale "на поверхности" autopilot panel during take-off -> cleared.
  - Rendezvous (R5 first part): Ship legs (target ship's position + velocity, arrival 6 km outside its J
    keep-out, its keep-out does not throttle the approach); live rv1: the leg tracks ship 10 and closes; arrival
    and the final approach to the arrival profile (formation / follow) NOT done.
  - Open: intermediate jump systems (no jump runtime), trade actions, orbit survey (= orbit), repair model,
    the 0.00 g readout during the take-off climb, DEV waypoint names show raw ids on the HUD markers.
- Fleet reference (d5b537ce, subagent): 17 PROPOSAL ships (1 new hull ТС-6 «Дозор», 13 variants, 3 small craft),
  2 parts (srw.reactor.sr26, kern.mine.cutter), coverage page; hulls to model 20 (19 new meshes) + 3 small craft;
  builder kept in Notes/html/ship_reference/tools/.
- War doctrines (78c53ae4): Notes/Lore/ECONOMY/WAR_DOCTRINES.md (author ideas + PROPOSAL). lore_html.py deleted
  Notes/html/ship_reference as "stale" (restored from git); it now only prunes mirrored folders.

### Slice map-2 2026-11-06 - hub composition status + pictograms, moon labels, HUB for lunar (world-site) hubs - LIVE DEV RUNS, NOT ACCEPTED
- Author report: (1) the composition is not trustworthy (as a rule only the station exists): module + "(строится)" /
  "(в плане)"; (2) the text line ripples: show module images; (3) System map: moon names once the moons' orbits
  exceed half the screen; (4) HUB from Details (Селена, lunar orbit) did nothing; HUB on the System map for Ворота
  Луны opened a Details cube without the hub marker.
- Root causes (live baseline D:/tmp/elite_maps2/b*, d*, e* first runs): Селена / Ворота Луны are unbound world sites
  ("site:<id>"), not replicated hubs, and (a) the Hub map existed only for replicated hubs (rebuildHubMapFromClientState
  finds no runtime hub -> composeHubMapSnapshot false -> setSystemMapHubMode returned silently); (b) a System-map CLICK
  on a site set no selectedHubId (sites are Station-kind objects, navigationHubId empty; only the hub-list path set
  it), so HUB fell to the free-tactical branch = the terminal-cube SpatialVolume Details (no hub marker, b2); (c) on
  the Detail map world sites had no navigation hub binding and the presentation builder cleared any selected hub
  absent from hubScreenPoints, so the HUB button stayed disabled (canOpenHub 0); (d) Lagrange sites were never on a
  body Detail map. Composition: the panel listed the economy hub_modules TARGET as if built; runtime hubs carry only
  their station (attached objects: 1 x Station for Высокий / Тихий).
- Done (source + Debug build):
  - Catalog Hub map: ClientWorldSitesMapBridge catalogHubRuntimeSample (orbit / Lagrange / heliocentric site at the
    same presented position as its markers, frame radial / prograde, ONE station module, far player dropped),
    used by ClientMapService::composeHub for "site:" ids; System-map site objects get navigationHubId = own id;
    Detail orbit / Lagrange-primary sites are "world_site_hub:<type>" hub targets (hubScreenPoints, selection kept);
    Lagrange hubs appear on their primary's Detail map; F12 with a site selected = its Hub map; the Hub-map horizon
    backdrop is skipped beyond 10 parent radii (L1 hub no longer skims Earth). The duplicate selected-hub label of
    DetailMapGeometryPass removed (the overlay draws it).
  - Composition: EconomyModel HubEconomyInfo::modules (per type); status in SpaceStateMaps: a type present in the
    runtime hub (Station -> admin_station, HubHexDock -> shuttle_dock, HubDockServiceBlock -> service_dock; a site
    hub = its station, a beacon = beacon_core) operates, refuel_dock / service_dock not present "(строится)", others
    "(в плане)"; "Состав" right after Тип; Доки / Терминалы / Ремонт / Склады carry the same status. Panel: block
    subheadings + one row per module with a 9x9 pixel pictogram (22 types) drawn with the panel's solid rects,
    colour by status (cyan operating, amber under construction, dim planned); a status never wraps inside.
  - Moon labels: a moon is named when its orbit diameter > half the shorter viewport side (or the old 200 km/px);
    labels ordered planets first, moons by size, a moon label overlapping a drawn label is skipped.
  - DEV verbs: "panel:hub|detail|system|galaxy|select:<objectId>" (native panel actions), "mapturn:<yaw>,<pitch>";
    "overlay" lists hidden items and the selected hub's attached runtime objects.
- Live DEV evidence (--dev-direct-flight, back buffer D:/tmp/elite_maps2/): c1/c2/c3 (System click on Ворота Луны ->
  HUB = its Hub map, no Earth horizon -> DETAIL = Earth Details with the Ворота Луны marker, HUB enabled); e1/e2/e3
  (Moon Details, click Селена -> HUB = Hub map of Селена over the Moon -> back); i2 (F12 with Ворота Луны selected =
  its Hub map); g1 / g2 (Тихий / Селена panel: pictogram rows, only the station operating); h3 / h6 (Jupiter: Ganymede
  / Pasiphae named at their thresholds, Europa not), k2 (Mars S5: Phobos, Deimos named).
- Gates: check_navigation_context PASS, check_object_overlay PASS, EliteSystemMapInfoCheck PASS; system_map
  check_architecture RED pre-existing (DetailMapGeometryPass depends on SystemMapRenderer facade).
- OPEN: author acceptance; the DEV wheel zoom did not change the System scale (Earth Moon label shown only at the
  cube-level steps: at S4 the Moon orbit is 275 px < 360, at S5 the Moon is off screen) - a human wheel zoom is
  expected to cross the threshold; surface sites (Шеклтон, Имбриум) have no Hub map (pads only); the catalog Hub map
  has no dock geometry / traffic; whether the station's own docking ports count as "челночный док" operating is a
  rule question (now: planned); moon names are the catalog's English names (Phobos, Ganymede).

### Slice cc-9 2026-11-06 - route window buttons on every map, moons as route points, body avoidance, network corridors - NOT ACCEPTED
- Author (2026-11-06): no calculate / start buttons for a finish chosen on Details; a moon selected on System /
  Details gives no route commands; routes are straight lines (Mars - Ceres across the Sun? through a planet?);
  the hub-to-hub lines near Earth are straight - green zones must follow them. Fortuna names accepted (CANON,
  02cb5d6a). Map items (hub composition statuses, pictograms, moon labels, HUB button for lunar site hubs) -
  subagent slice map-2 (9d3cc8ed).
- Route window (NavigationRouteOverlay footer, every map): РАССЧИТАТЬ / СТАРТ / ПАУЗА | ПРОДОЛЖИТЬ (by the server
  route state) + the summary line. Live capture rt/detail_buttons.png (Detail map, finish Тихий).
- Moons: a body without an overlay card gets a synthesized route card in the System context menu (add point /
  finish). Detail-map moons are 3D bodies without glyphs - still no context menu there (OPEN).
- Body avoidance: the planner treats the star (20 radii, the Sun ~0.09 AU, heat), planets and moons (r + 150 km /
  5 %) as obstacles (the same detour as red zones); the server leg keeps 20 star radii (was 150 km over the
  photosphere). Live ob2: Earth hub -> Neptune / Saturn gets "обход: Земля". No planet was behind the Sun at the
  run epoch - the star detour shares the code but was not seen live.
- Network corridors: the world-model scheduled routes between two runtime hubs become green corridors along
  corridorPath (round the bodies); an official leg between the same ends follows it; the blue gate still applies.
  Live ob1: Высокий -> Тихий "по коридору сети: Тихий – Высокий" (2 legs, 12 851 km). The dashed network lines on
  the System map stay schematic chords (aggregate schedule, not flight paths).
- DEV: Cyrillic body ids in ELITE_DEV_* env vars arrive mangled - Latin aliases added (mercury .. neptune).
- Author rule (2026-11-06): the station's own docking ports = an operating shuttle dock. Implemented: a Station
  object's enabled docking_port anchors (hub_semantic_anchors.json, by its hub module id) count as shuttle_dock;
  the current station model has none, so Высокий / Тихий still show the dock "(в плане)" until a model with ports.

### Slice map-3 2026-11-06 - F12 cube shows what is visible (Moon surface, player, sites), trajectories on Details, site hubs as cubes - LIVE DEV RUNS, NOT ACCEPTED
- Author report: (1) F12 in lunar orbit: "a cube and some arc", no ship marker; (2) Moon Details: no player
  trajectory - orbits of all objects on stationary trajectories must be shown; (3) Селена is not a cube.
- Root causes (live baseline D:/tmp/elite_maps3/b1_f12): the arc was the geometry-pass outline of the Moon (cube
  scenes had no body rendering); the player WAS in the scene but hidden by the Moon occlusion test (overlay
  "[entity:9 | hidden]"); the cube scene held only its interior; world sites were added only to body Details;
  ClientDetailMapBridge cleared playerOrbits ("no fabricated orbit") and nothing drew ship trajectories; Detail site
  hubs (world_site_hub:*) got the Infrastructure triangle at 0.55 scale. Also: the player's replicated hubId stays
  earth_orbital_hub after leaving for the Moon, so the Detail badge went to that hub (now only within 50 km).
- Done (source + Debug build): ClientDetailMapTrajectories.h - osculating conic of each free ship about its dominant
  body (deepest SOI = d (mu/muParent)^0.4, catalog GM), closed ellipse or the arc within min(SOI, 1e8 m): the
  player's into playerOrbits (gold, labelled "орбита игрока Pe .. / Ap .."), other free ships' bound orbits dim
  (DetailMapSnapshot::shipOrbits); orbital world sites get their rail circle in hubOrbits; DetailMapOrbit path
  offsets (interpolated with the centre). Cube scenes (DetailLocalVisibleReach = 6 half-edges): ships / hubs /
  beacons / bodies + the world sites of every body in view; DetailMapSnapshot::contextBodies rendered by
  DetailMapBackend with the body-Detail planet pass (body-centred orthographic view at its projected place and
  depth scale, far first; skipped when the eye is inside); the player (and the cube's focus) is never hidden behind
  a body (dimmed); the player reads "игрок" in cubes. Site hubs: Hub glyph, hub colour, full scale. Contracts:
  MAP_NAVIGATION_CONTRACT (F12 scene, trajectories note), MAP_OBJECT_OVERLAY_CONTRACT (site hub glyph, player kept).
- Live DEV evidence (back buffer D:/tmp/elite_maps3/): c1_f12 (leg:orbit:moon:100, F12: textured Moon, player
  marker + badge, orbit "Pe 96.6 km / Ap 103.3 km"); c3_moon_detail_turn (Moon Details: player orbit, Селена rail
  orbit); d2_moon_turn (Селена as the cyan hub cube on its orbit).
- Gates: check_navigation_context PASS, check_object_overlay PASS, check_route_plan PASS; check_architecture RED
  pre-existing (DetailMapGeometryPass -> facade).
- OPEN: author acceptance; F11 in lunar orbit opens EARTH Details (stale motion.hubId earth_orbital_hub, b2_f11) -
  not fixed; the cube body is an orthographic impostor (no perspective distortion near the eye); a ship under J
  thrust draws its instantaneous conic (changes while thrusting); heliocentric conics are not drawn; other ships'
  open (hyperbolic) arcs are not drawn; Ворота Луны (L1) has no orbit line (not a Kepler orbit).

### Slice cc-10 2026-11-06 - tangential orbit insertion, finish choice, station hand-over jump, Высокий docks back - NOT ACCEPTED
- Author (2026-11-06): the orbit leg flew into the body's centre and "magically" had the circular speed; a finish on
  a body must offer an arbitrary orbit / its altitude with v1 / a landing; the route card must report completion;
  Селена "Рассчитать" did nothing; the HUD terminal disappeared; freezes on the Moon-orbit -> Earth-hub flight; the
  station jumped at the stabilization hand-over; give Высокий back its docks (the cylinder with the blue zone and
  the hex dock). Map items -> subagent slice map-3 (857189b7).
- Orbit leg (GameSimulationRoute Orbit): the plane fixed at the leg start (position x motion), the aim = the tangent
  point of the orbit circle seen from the ship, the arrival velocity = v1 along the tangent. Live tg1: ARRIVED 18 m
  from the aim, orbit 100.0 km at 1633 m/s (circular). The approach angle is not logged - tangency is by
  construction, not measured.
- Finish choice: the System context menu of a body adds "ФИНИШ: ОРБИТА N КМ (v₁ …)" x3 (J floor x1 / x3 / x10) and
  "ФИНИШ: ПОСАДКА (АВТОМАТ)" for airless bodies (SystemMapRenderer::setBodyFlightFacts from the client catalog).
  Not yet clicked live.
- Route complete: the route window shows "МАРШРУТ ВЫПОЛНЕН" when the server route ends. Catalog site hubs as targets
  (marker position; a clear "нет координат цели" otherwise). Not verified live.
- HUD terminal: it was shown only on docking / BITE / ignition / alerts - now always in flight (author's wish).
- Freezes: NOT reproduced on the same flight (Moon orbit -> Высокий, 0 frames > 30 ms besides the capture frames);
  the author's 17:08 session had 0.2-0.6 s stalls every ~2 s with every phase inflated (hud, session_advance,
  scene_prepare). CLOSED (author 2026-11-06): he was switching windows during that session - an unfocused /
  background window, not a bug; reopen only for stalls with the game window focused.
- Station hand-over jump ROOT (DEV ELITE_DEV_HUB_POSE_TRACE): the station's orientation is continuous (0.000 deg at
  the switch); its POSITION relative to the player jumps 570-660 m when the station's presentation switches from the
  world pose (interpolated snapshot) to the hub-frame pose - an ~19-22 ms epoch mismatch at 30 km/s heliocentric
  between the free-flight frame and the objects (snapshots every 0.06 s, the station refreshed every snapshot).
  Mitigation: the relative pose difference is carried over and faded out in 1 s (live hp6: step 0.00 m at the switch,
  <= 11 m/frame after). The epoch mismatch itself is OPEN (the station is drawn ~0.6 km off in free flight near a hub).
- Высокий docks: commit 5497cf37 turned the HubMotionLab off - the same flag spawned the hex dock, its service block
  and the guidance cylinder; now spawned independently (live: attached objects 10/11/12, capture docks.png).

### Interim architecture audit 2026-11-06 (subagent, read-only; full report docs/ARCH_AUDIT_2026-11-06.md)
- Python gates: 209 check_*.py, 122 pass / 87 fail (mostly stale token-greps; they hide real new reds:
  GameServer.cpp includes the client RoutePlanner.h, system_map facade coupling, stale wire-schema token list).
- High: NAVIGATION_V2 §1 / §11b say route execution is client-owned via ShipControlState - the J autopilot, route
  legs, take-off, rendezvous and the landing pilot are server-owned (two owners, hand-over unspecified); zone fines
  computed / summed on the client; Высокий's docks still hang on diagnostic ids / HubDockingFlightTestScene; DEV
  commands accepted from any client; ActiveFreeFlight ~70 fields / 7 sub-machines on booleans and bare phases 1..11.
- Single-source breaks: 5 sphere-of-influence implementations, body clearance client vs server differ, the client
  planner's 260 t + Cobrett charge constants for every ship, 3 "airless / landable" criteria, J ramp / ApproachTauS
  twice, Pe/Ap twice, WorldSitesCatalog loaded 3x, route state in 3 forms; route legs as an unversioned string
  protocol (km vs m, magic indices 0/3/4).
- God functions: SpaceState::updateDockingAdvisory 3894 lines, SpaceState::update 1816 (~400 DEV interpreter),
  renderHUD 1115, updateFreeFlights 726. 65 ELITE_* env vars, some read per tick / frame and changing gameplay.
- Threading: ModelAssetGpu loader has no try/catch, MeshGPU never frees GL objects, detached terrain threads,
  call_once bodies that can throw in ObjectDescriptorRegistry / ObjectAssemblyRegistry.
- Tests: WireDataPlaneContractTests not built since schema 35 -> 38; no tests for the new pure logic (route codec,
  charge, J floor, settlement layout, SOI, conics); SystemMapBehaviorTests does not compile.
- Recommended order: gate triage + known-red allow-list; one SOI function + test; one route-safety module shared by
  planner and server (JDrive.h to a neutral layer); J figures from the ship descriptor; hub docks from data;
  DEV gating; zone fines + route completion on the server; typed route DTO in the wire schema; ActiveFreeFlight
  state machine; decompose the giant SpaceState functions; loader hardening; NAVIGATION_V2 ownership update.

### Gate triage 2026-11-06 (audit item 1; source/offline only, no live flight)
- Python gates 209: before 122 pass / 87 fail, after 196 pass / 13 known red / 0 new red. Nothing deleted; 74 stale
  checks updated to the current design (split SpaceState* / GameSimulation* units read as one, CURRENT_TASK/STATE
  markers dropped, exact version / schema pins -> minimum, retired subjects PredictivePilot / navigation_guidance /
  rear monitor / B-spline smoother asserted as gone, model-asset hash locks re-baselined at 0.10.87, regex bugs).
- One runner: `python tools/run_gates.py [-k text] [-v] [--timeout s] [-j n]`; exit 1 on a red not in
  tools/gates_known_red.json or on a stale allow-list entry. Allow-list = REAL reds left for the main session:
  GameServer.cpp:1 includes client RoutePlanner.h (4 gates, rec. 6); std::chrono in SegmentedRouteComposer.cpp:5/597/842;
  Sol body ids in client SpaceState.cpp:1409+ (DEV script, rec. 8); missing EOF newline SystemMapRenderer.h /
  SystemMapSceneRenderer.cpp; model-asset editor purity regressions (attachments.js:125, viewport/scene.js:11) and a
  function census frozen at v0.10.73 (4 gates, editor re-baseline slice).
- Production change: src/game/system_map/PlanetMapAxes.h holds the planet axis helpers; DetailMapGeometryPass.cpp no
  longer includes the SystemMapRenderer facade, DetailMapPlanetPass.cpp lost its duplicate copy (both objects compiled;
  tests/system_map/check_architecture.py green). Tracked __pycache__/*.pyc under tests/ get dirtied by direct gate runs.

### Audit items 1-4 (author order 2026-11-06: "начинай с пунктов 1-4") - BUILT, gates + offline check + live DEV runs
- 1 Gates (subagent, 78855be7 + main): tools/run_gates.py + tools/gates_known_red.json (explicit allow-list,
  exit 1 on a new red or a stale entry). 122/87 -> 201 pass / 8 known red / 0 new red. Left red (allow-listed):
  SegmentedRouteComposer std::chrono in pure route code; Sol body ids in the DEV script inside SpaceState::update;
  model-asset editor purity regressions + 4 census gates frozen at v0.10.73 (re-baseline slice). Fixed here:
  GameServer.cpp no longer includes the client RoutePlanner.h (the legs + codec moved to the neutral
  navigation/route/RouteWire.*: 4 gates green); planet-axis helpers in system_map/PlanetMapAxes.h (subagent);
  EOF newlines (check_map_architecture green).
- 2 One rules module: src/game/navigation/rules/BodyFlightRules.h - dominantBody (Laplace SOI; parent = catalog
  parentId, else the id prefix, else the star), floorAltitudeM, clearanceM (star 20 R; max(R + 150 km, atmosphere +
  50 km)), isAirless. Used by the server referenceBody, the client planner bodyAtPoint, the Detail-map trajectories,
  the route leg obstacles, the planner obstacles, the landing check and the map's finish rows. JDrive.h moved to
  navigation/rules (namespace game::navigation::jdrive) with DriveRampDoublingS / ApproachTauS (no duplicates).
  Offline EliteBodyFlightRulesCheck: 18 checks PASS (SOI Earth / Moon / 300 000 km / 1.5 AU, prefix == catalog
  parent, floors, clearances, airless, route codec round trip incl. a pad spec with ':', charge monotonic).
- 3 J figures from the ship: ShipParams jumpCoreRatingT / jumpChargeCapacityT (Cobrett descriptor 500 t / 3 t);
  motion shipMassT / jumpCoreRatingT / jChargeCapacityT set at spawn and replicated (schema 39); the planner uses
  them (no 260 t, no Cobrett constants); no jump core = J refused / no J route; route legs now charge the J (live:
  Earth -> lunar orbit 3.000 -> 2.992 t). Regression found and fixed live: the capacity was set only in free flight
  (0 in a hub) -> the planner "inserted a refuel" at the finish hub and lost the finish; also never inserts a refuel
  at the failing point's own hub.
- 4 Hub docks are data: initial_world_state.json orbital_hubs[].dock_modules (Высокий: hub_hex_dock_a,
  hub_hex_dock_base, guidance_dock_cylinder_b), spawned for ANY hub by spawnHubDockModules after the hub registers;
  the guidance-lab stress objects stay a diagnostic scene (off). Live: [HubDocks] 3 attached; the player's entity id
  is now 12 (the docks spawn first) - DEV scripts naming ship ids change.

### Audit rec. 8 / 10 - SpaceState god functions decomposed (subagent 2026-11-06; behaviour-preserving, BUILT + gates + live DEV runs)
- DEV script interpreter out of SpaceState::update (1816 -> 1233 lines): src/game/dev/DevScriptRunner.{h,cpp}
  (friend of SpaceState, one call per frame; every ELITE_DEV_* var of the block read once into a Config; latches
  have process lifetime like the old function-local statics; verbs split by group in the original dispatch order;
  log strings unchanged). The Sol body / hub ids are its DEV data table (per-verb alias tables kept exact), so
  check_feature_contracts is green and left the known-red list (no gate change needed: it reads SpaceState*.cpp).
- updateDockingAdvisory (3894 lines) -> 697-line orchestrator (helper lambdas + the frame aggregate) + phases:
  checkDocking{Newtonian,Assisted}Authority, updateDocking{SecuringCourse,Stabilizing,Planning,Executing},
  settleDockingCraftBeforePlanning, secureDockingCourseWhilePlanning, compileDockingTrafficRoute,
  checkDockingEntryHandoff, publishDockingAdvisoryPresentation, free runDockAdviceWorker (the detached thread body).
  Mechanical: blocks moved verbatim, outer locals bound by reference through a local aggregate (member templates
  so the lambdas keep their closure types), top-level `return;` -> `return false;` = "stop this update".
  Largest left: updateDockingStabilizing 567, runDockAdviceWorker 402.
- renderHUD (1115) -> 209 + renderHudWorldLabels 88 / GuidanceAndCompass 196 / CockpitInstruments 107 /
  DockingTerminal 535 (typed parameters, call order unchanged).
- Gates: 203 pass / 6 known red / 0 new red. Updated to the new locations, intent kept: foundation_lock checks the
  whole SpaceStateDocking.cpp; recent_navigation_regressions checks the draw order via the section calls in
  renderHUD; assisted_docking_entry accepts `return false;` after the exhausted-rebind recoverOrFail.
- Live (build_agent, --dev-direct-flight): freeflight/home/insert/jstate/report verbs log as before; finish:hub:
  earth_hub.tikhiy + route:official -> "[Route] ship 12: ROUTE of 2 legs", route:pause / resume OK;
  ELITE_DEV_DOCK_REQUEST_AT=15 -> request, planner worker (3.1 s), phase=executing, [DockClientTrack] tracking.
  Not done: an A/B log diff against the pre-split binary; further split of updateDockingStabilizing /
  renderHudDockingTerminal / the orchestrator's helper lambdas.

### Audit remaining items (author 2026-11-06: "продолжай с остальными пунктами аудита") - BUILT, gates 203 / 6 known / 0 new
- Hardening (edc98b5f): core::safeCallOnce (a throwing init rethrows to every caller, no MinGW deadlock) in
  ObjectDescriptorRegistry / ObjectAssemblyRegistry; the ModelAsset prewarm worker catches and falls back to
  on-demand. The pure route composer owns no clock (setSegmentedRouteDiagnosticClock, installed by main).
- DEV commands gated on the server: the embedded game allows them, the dedicated EliteServer only with --dev /
  ELITE_DEV_COMMANDS=1 (live: "[Server] DEV commands rejected"); gated: weapon-type cycle, combat target, equipment
  failure (the physical-model selection stays a player setting).
- Zone fines on the server (7474bc25): GameSimulationZones (red entry, skipped blue gate) with motion
  zoneFinesTotalCr / zoneViolation* (schema 40); the client only shows the notice; zone catalog + geometry in the
  neutral route/RouteZones. The server loads the 3 Sol zones; a violation was NOT triggered live (no DEV path into
  a red zone; a straight Earth -> Moon leg passes the customs gate, correctly no fine).
- NAVIGATION_V2 §1 / §11b (80aab8f6, 05931c7f): two owners by scale; the hand-over cancel rule marked OPEN.
- SpaceState decomposition (subagent, 4205d26d): DevScriptRunner (src/game/dev, update 1816 -> 1233 lines, env vars
  cached, the Sol DEV aliases out of SpaceState: check_feature_contracts green); updateDockingAdvisory 3894 -> 697
  + named phases; renderHUD 1115 -> 209 + 4 sections; live: verbs, a route, a dock request to "executing".
- Free-flight state (this commit): navigation/FreeFlightCodes.h - FlightRefusal (0..9) and AutopilotDisplayPhase
  (0..12) named, wire values unchanged, used by the server and the HUD; the internal AutopilotStage enum (0,1,2,3,
  9,10,11 - the two numbering spaces no longer share bare ints); ActiveFreeFlight sub-machines grouped (legRun,
  ground, landingPilot; 114 accesses). Bug fixed by the naming: the J charge was spent during the pad descent
  (display phase 4 counted as a J phase). Live regressions: Shackleton landing 3.7 cm / take-off to the J floor;
  the Tikhiy route starts.
- Left: the autopilot fields of ActiveFreeFlight are not grouped yet; the station epoch mismatch (faded) OPEN; the
  model-asset editor reds (6 allow-listed); build_agent/ (6 GB, untracked) removed.

### Architecture re-check 2026-11-06 (author: "всё ли уже хорошо?")
- Built and RUN the contract tests: wire_data_plane_contract_tests PASS on schema 40 (tests/architecture_contracts,
  separate CMake project, D:/tmp/elite_contracts); SystemMapBehaviorTests compiles again (bbfeb8af: stale mock +
  missing sources) - 28/33 PASS, 5 FAIL to triage (rotation pivot, zoom pivot target, explicit cube selection,
  the initial fit distance 5000 vs 94.5, the mouse/scroll trace) - stale expectations after the map navigation
  redesigns or real regressions: not decided.
- Still open from the audit: the airless rule duplicated by the settlements / renderer (surface pressure < 100 Pa
  vs the atmosphere radius); the player's Pe / Ap computed by the server and again by the Detail map; the
  WorldSitesCatalog loaded 3x (client service, settlements, dock traffic); the route command still a string
  protocol (neutral RouteWire + a round-trip test, but pause / resume / cancel are indices 0/3/4 and the bare DEV
  leg target is in km); HubDockingFlightTestScene / NavigationRuntimeLabEnabled constexpr in production; 65
  ELITE_* env vars (the DEV runner's cached, the rest not); the autopilot fields of ActiveFreeFlight ungrouped; the
  hand-over cancel rule not enforced; the station epoch mismatch; MeshGPU never frees GL objects; detached terrain
  loader threads; the 6 model-asset editor reds; the contract tests and the map behaviour tests are not run by
  tools/run_gates.py.

### Map behaviour test triage + C++ suites in run_gates (2026-11-06)
- SystemMapBehaviorTests 28/33 -> 33/33 (compiled, D:/tmp/elite_sysmap_tests). All 5 reds were STALE expectations
  of deliberate map redesigns, none a production regression; production untouched:
  rotation pivot (press = PendingClick, pivot at drag start: near object > pressed body / hub / layer point >
  target plane, never a cube centre - 97b39d73); zoom pivot (body pivot needs the resolver's hover body; else the
  point under the cursor - 97b39d73); explicit selection (cubes are targets only with the G grid overlay -
  2eb47aad; body click selects the body + its child cube of the parent, anchor not moved - b2d39fd2); initial fit
  (S0 root fitted, hierarchyFitHalfHeight, not fittedSystemRadius*padding - b2d39fd2); mouse trace (the drag-start
  frame does not turn; plan view default turns only yaw - ac71cfb6; a free-3D replay added). The stale §7 bullets
  of MAP_NAVIGATION_CONTRACT.md updated to the 2026-10-31 b camera contract.
- tools/run_gates.py: `--cpp` (Python gates + C++ suites) / `--cpp-only`; configures (Ninja Debug, if no cache),
  builds and runs cpp:wire_data_plane_contract_tests (D:/tmp/elite_contracts) and cpp:system_map_behavior_tests
  (D:/tmp/elite_sysmap_tests); allow-list keys `cpp:<target>`. Off by default (cold build minutes). Run:
  `--cpp` 211 gates: 205 pass, 6 known red (model-asset editor), 0 new red; both C++ suites PASS.

### Client-owned global flight G2..G4 (author 2026-11-06: "делай подряд все 4 пункта") - BUILT, live DEV runs; NOT ACCEPTED
Scope (NAVIGATION_V2 §1 author decision): the planners and autopilots on the CLIENT, the server = plant + authority.
- Step 1 (extraction, behaviour-preserving): `navigation/globalflight/BodyAutopilot.{h,cpp}` - the J transfer onto the
  tangential parking orbit, the coast, the de-orbit in normal flight, the hand-over, the terminal landing pilot, moved
  out of GameSimulation (updateFreeFlightAutopilot / updateNormalDescent / updateSurfaceLanding). Neutral: a ship
  sample + callbacks (bodies, body orientation, terrain, keep-out) in, the ShipControlState command out.
  The command surface (ShipControlState, wire protocol 25): globalFlightCommandValid; a J WORLD VELOCITY TARGET +
  the client's acceleration limit (instead of G1's raw acceleration: deadbeat tracking on the server tick, robust to
  latency); globalMainAccelWorldMps2 (main engines, |a| <= the hull's thrust); an attitude target; globalFrameRequest
  (1 normal flight, 2 ground stabilization, 3 stabilization = hub capture / ground / own orbit); globalFreeRotationRequest.
  The plant (GameSimulation::applyGlobalFlightCommand + applyGlobalFrameRequests): the J field toward the target within
  min(client limit, 1e5 g, its own ramp 1 g doubling / 2 s); in a hub/ship keep-out only the gentle 1 g; below a J
  floor none; an empty charge none; the attitude slew (J: 2 rad/s, else the hull rate). Rules, not intentions.
  Behaviour change on purpose: the de-orbit's ground turn uses the module's own last basis (the server's `lastBasis`
  was overwritten the same tick - the turn read ~0).
  LIVE ba1 (server-run through the module + plant, Shackleton pad): ON ORBIT 86.9 km / 1639 m/s at t+190.9 s (baseline
  194), hand-over 1948 m / 0.19 m / 79.9 m/s, LANDED t+561.6 s (baseline 564.5), 1.53 / 0.10 m/s, 0 m from the chosen
  site, regolith, slope 0 - soft, no damage.
- Terrain probe moved to `navigation/globalflight/BodyTerrainProbe` (process-wide field cache: an embedded server and
  its client load one field per body). GameServer::terrainHeight delegates.
- Step 2 (client body autopilot): src/game/SpaceStateBodyAutopilot.cpp - the module on every new server snapshot (dt by
  server time), bodies from the client's celestial runtime AT THE SNAPSHOT'S universe time (GameClient::
  resolveCelestialSystem), the ship from the replicated travel frame (+ local motion), keep-out from the replicated
  hubs / ships; out through submitInput (the docking autopilot's channel); HUD reads the client's display overlay;
  PAGE DOWN / map O / DEV autopilot: engage the client's. Restore of the flight law / free rotation after a landing.
  Bugs found live: (ba2) the replicated worldPosition / worldVelocity stay at the hub-frame values in normal flight ->
  the sample froze at the hub exit -> the travel frame; (ba3/ba4) the docking advisory released the shared control
  slot every frame (cleared the buffer the runner resends) and the keyboard path re-took the stream -> the server saw
  alternating valid / empty commands, the plant's ramp reset (~1/5 of the drive) -> ownership flags respected.
  LIVE ba5 (client body autopilot, the same Shackleton script, pre-G4 build + the fixes): keep-out exit at 1 g as ba1,
  phase 2 at t+77.6 s (ba1 78.4), ON ORBIT 86.89 km / 1638.95 m/s at t+190.74 s (ba1 190.94), the de-orbit pilot
  matching ba1 within ~1 deg of nose, hand-over 1946.7 m / 0.11 m / 79.55 m/s at t+537.66 s (ba1 537.92), the ground
  frame by the frame request, LANDED t+563.46 s, 1.54 / 0.12 m/s, 3.7 cm from the chosen site, regolith, slope 0 -
  soft; "landing pilot OFF (client)" - the law / free rotation restored, the keyboard owns the stream again.
- Step 3 (client route): navigation/globalflight/RouteLegPilot (updateFreeFlightLeg + the take-off moved, same laws /
  logs) and src/game/SpaceStateRouteRun.cpp - the route of the planner's RoutePoints (launchPlannedRoute no longer
  sends it): legs in order, pass-through guide points, landing legs on the body autopilot, a stop ends in the frame
  request "stabilization" (hub capture / ground / own orbit), refuel / repair = the server's timed service stay
  (RouteLeg command index 5 -> GameSimulation::startHubService: the ship stays in the hub frame 20 s, the server fills
  the J charge), "pause" points and PAUSE / RESUME client-side, the route state for the window / HUD from the client.
  DEV leg:hub / leg:orbit = a one-stop client route.
- Step 4 (server = plant + authority): deleted the server-run body autopilot (setFreeFlightAutopilot / sample /
  environment / dispatch), the landing-pilot control override (applySurfaceLandingPilots), the route legs, take-off,
  routes / pause / resume (FreeFlightLeg, ActiveRoute, legRun, landingPilot, m_pendingJExit); GameServer ignores the
  FreeFlightAutopilot command and route commands other than the service stay. The server keeps: the plant, J rules,
  the charge, ground contact / damage, zones, the service stay. Diagnostic: "[Plant]" log every 2 s of a J command.
- Gates (post-G4 build): run_gates -j 6 209: 203 pass, 6 known red (model-asset editor), 0 new; --cpp-only 2/2 PASS
  (wire_data_plane_contract_tests, system_map_behavior_tests).
- Process note: the G4 patch was applied before the G2/G3 commit by a mistaken invocation (a dry-run flag the script
  does not parse) - so G1..G4 are one commit; ba1 / ba5 evidence comes from the intermediate builds named above.
- OPEN: not author-accepted; zone fines / service stay not hit live; rendezvous (Ship legs) not flown live; motion.
  routeState / routeLeg* and freeFlightAutopilot* stay in the wire but the server no longer writes them (client overlay)
  - candidates for removal; the hand-over cancel rule (a pilot action during a client route) still OPEN; client
  autopilot is single-player (NPC global flight would need its own server-side or client-side runner); the older
  Orbital Transfer (hub -> hub around one body, requestOrbitalTransfer) is still planned and flown by the SERVER - the
  same migration applies to it (next). A landing leg engaged at full J speed (past a guide point) restarts the drive
  ramp from 1 g like the old server autopilot did (it overshoots the body and comes back) - carry the ramp (proposal).
- LIVE rt5 (post-G4 build, client route: guide gate -> Shackleton pad + pause -> RESUME -> gate -> Earth orbit): gate
  passed at 12 636 km/s, the landing leg on the client body autopilot (ON ORBIT, hand-over 0.11 m, LANDED 0 m from the
  pad, 1.54 m/s), PAUSED, RESUMED -> TAKE-OFF STUCK: on leaving the ground frame the contact re-landed the ship (0.06
  m/s down) before the first climb command, and only the HOME latch / the laws' lift-off freed it (the old server
  take-off dodged it by its order in the tick). FIX (plant rule): a main-engine command pushing up harder than the
  local weight lifts a landed ship off ("[Plant] LIFT-OFF").
- LIVE rt6 (same route without the pause, post-fix): gate 12 641 km/s -> landing leg ON ORBIT 86.89 km -> hand-over
  0.10 m / 79.5 m/s -> LANDED 0 m from the chosen site -> TAKE-OFF -> LIFT-OFF -> complete at 86.88 km (J floor 86.87)
  -> gate 12 630 km/s -> orbit of Earth (382 km): ARRIVED 18 m / 3.0 m/s -> stabilization (own ballistic frame) ->
  ROUTE COMPLETE (client) / "МАРШРУТ ВЫПОЛНЕН".

### Work queue after G2..G4 (author 2026-11-07: open items are closed in the same run, not left OPEN) - IN PROGRESS
Queue (from the G2..G4 OPEN list + author remarks 2026-11-07 on the live runs):
1. Orbital Transfer (hub -> hub around one body) planned / flown on the client; the server path removed.
2. A landing leg engaged at full J speed keeps the drive ramp (no restart from 1 g).
3. Dead wire fields (routeState / routeLeg* / freeFlightAutopilot*) removed, protocol version bump.
4. Live DEV runs: service stay (refuel), zone fines, rendezvous (Ship leg).
5. NPC global flight: check whether NPCs use the removed server autopilot at all.
6. (author) Landing profile like an aircraft: a descent along an arc with the nose along the path, then a hover
   over the pad and a vertical touchdown - instead of nose-down / tail-down; the player watches a moving picture.
7. (author) Assisted: the velocity marker is often perpendicular to the hull - diagnose and fix.
8. (author) The highlighted patch on the body under the ship in flight - remove; all lines a bit brighter.
9. (author) Moon spaceport: a crater / mound drawn with very dense levels - screen ripple (aliasing) - fix.
Author-only: the hand-over cancel rule (a pilot action during a client route); acceptance of everything.
Progress (source + builds; live evidence per item below):
- 1 DONE in code: GameSimulationOrbitalTransfer.cpp, OrbitalTransferPlanner.{h,cpp}, the EliteOrbitalTransfer
  library and EliteOrbitalTransferCheck deleted; "transfer to the selected hub" / DEV transfer: = a one-stop client
  route Hub leg (startClientRoute); handOverToHub moved into GameSimulationFreeFlight.cpp. LIVE q6 (Высокий ->
  Тихий): ARRIVED after 140 s, 120 m from the aim, 20 m/s vs the hub, handed over to earth_hub.tikhiy (25 km,
  23 m/s), ROUTE COMPLETE.
- 2 DONE in code: the plant keeps the J ramp over a command gap <= 0.5 s (globalIdleSeconds); a landing leg after a
  pass-through point hands the leg pilot's drive seconds to the body autopilot (carryDriveRamp). LIVE q7/q9: the
  landing leg after the gate started at the full drive (980 665 m/s2 at t+0.06 s, the plant's ramp going on from
  52 s) - no restart from 1 g, no overshoot of the Moon.
- 3 DONE: ClientShipCommand RequestOrbitalTransfer / CancelOrbitalTransfer / FreeFlightAutopilot removed, RouteLeg
  -> RouteService (the stop's service stay only); motion freeFlightAutopilot* / route* off the wire (client display
  only); WireProtocolVersion 26.
- 4 zone fines: the dominant body between Earth and the Moon is the Sun (its pull is the larger there; q1 log:
  primary Земля -> Sol -> Луна), so the gate rule's body chain read Earth -> Sun -> Moon and could never fine a
  skipped gate. FIX (source): the star is not a step of the chain. CORRECTION of my first reading: q1 / q6 (Earth
  -> Moon straight) were not violations - the straight line passes the midpoint gate (15 000 km sphere), so "no
  fine" was right there; a gate-skipping flight (an off-axis via point) was NOT flown live - the fix is
  source-audited only.
  Rendezvous: LIVE q6 FAILED - the traffic target (VYS-1702) sat inside Высокий's J keep-out; the leg aimed at the
  ship, entered the keep-out at 1.4 km/s on the gentle 1 g and oscillated in/out for 200 s. FIX: a target inside a
  hub keep-out (RouteLegResolver::hubAround) ends the leg matched at that hub's keep-out edge (the rest is a local
  approach). LIVE q8 (from Тихий, leg:ship:any): "target inside the keep-out of earth_orbital_hub", ARRIVED 120 m /
  20 m/s, handed over to earth_orbital_hub, ROUTE COMPLETE. DEV leg:ship:<callsign|any> added.
- 5 DONE (no change): NPC traffic never enters free flight / J (no caller of toggleFreeFlight / the global command
  outside the player's client) - nothing was lost by removing the server autopilot.
- 6 landing profile - two FAILED live attempts recorded:
  q2 (nose along the velocity, the old guidance: circular speed to the site, braking late, then a 57 km vertical
  drop): CRASH at 163 m/s, 78 m from the site. q3 (an S-curve height profile, uniform braking along the arc):
  CRASH 18 km short - the glide lagged, the late flare needed ~45 m/s2 up, the nose went 45-60 deg off and the
  alignment gate held the engine off. Diagnosis by an offline 2D model (D:/tmp/elite_glide, reproduces q3:
  hand-over 597 m / 19.9 km / 353 m/s vs live 588 m / 18.8 km / 350 m/s): this Assisted Cobra has NO keypad RCS in
  normal flight (manoeuvre authority 0) - the vertical comes only from the tilt of the axial thrust; within
  +-25 deg pitch the ship cannot descend ~80 km in ~300 km AND stop the sink. Design now (BodyAutopilot::stepGlide,
  ships with a fore braking bank): phase 1 nose +15 deg, fore bank pushing back+down (re-planned every 1 s),
  phase 2 nose -20 deg braking; ends at 450 m/s ~8-11 km high 15 km out (model: 285 / 330 / 400 km starts all
  end within 0.4 km of the aim, pitch -20..+15 throughout); then the ground stabilization (its vectoring
  thrust: 30-47 m/s2 any direction, hull level) stops the sink, flies the level approach, a 4 s HOVER over the
  spot (new in LandingAutopilot) and the vertical touchdown.
  LIVE q4 (nose +15 deg, Shackleton pad): GLIDE from 287.9 km / 82.7 km, braking (nose -20) at 29.5 km / 27.5 km
  high, HAND-OVER 6.85 km high 15.2 km out at 449 m/s, hover at ~100 m (vertical -1 m/s), LANDED 0 m from the
  site; descent 365 s (old 370 s). Frames: phase 1 at +15 deg showed ONLY STARS (the horizon is ~18 deg below the
  local horizontal from 80 km, the cockpit sees ~12 deg below the nose) -> +8 deg (model: 330-400 km arcs, pitch
  -20..+8, the insertion lead 340 km). LIVE q5 (+8 deg): GLIDE from 332.5 km, braking at 21.8 km / 19.7 km, HAND-OVER
  6.77 km / 15.2 km / 449 m/s, hover at ~65 m, LANDED 2.6 cm from the site, descent 423 s. Frames: phase 1 STILL
  stars only (the geometry: at 80 km the surface needs the nose ~ -15 deg or lower, which needs a sink the axial
  engine cannot stop) - see the author question below; phase 2 and the level approach show the surface flowing
  (the hull level, the horizon in the lower third). The transit re-accelerated after the hand-over (403 -> 500 m/s)
  -> FIX: a transit entered fast never exceeds its entry speed (LandingAutopilot m_transitCapMps).
  LIVE q7 (the full route below, transit cap build) FAILED: the 15 km transit at 160 m/s from 7 km sank at the
  150 m/s clamp against the ground UNDER the ship and met the rising Shackleton rim (pull-up to 22 m, then a crash
  421 m short). FIX: the transit holds its height over the highest ground 3..12 s AHEAD (terrain probe) and never
  sinks faster than it can stop over it. LIVE q9 (same route): lowest 840 m over the rim, hover at 32 m, LANDED
  9 cm from the site.
  HUD "МАРШЕВЫЕ АВТОПИЛОТА +0.5 g (разгон)" while braking -> the autopilot's acceleration is now signed along the
  velocity ("-0.4 g (торможение)", q5 frames).
- 7 Assisted velocity marker: DEV log [HudVel] (hull-to-velocity angle, law, free hull, reference, autopilot
  phase every 2 s) added to find WHEN it is ~90 deg; analysis pending.
- 8 DONE in code: one line weight for the globe and the local terrain (0.65; was 0.45 globe / 0.8 terrain, the
  haze floor 0.5 both); no extra fine contour set on cratered bodies (it drew the denser patch). Visual: pending.
- 9 in code: cratered-body contours >= ~20 px apart (was ~7 px; 12 px still busy in q4 x570), the level blended
  continuously (the finer set fades as it closes up) instead of a hard per-pixel switch. q5 x610 (20 px build):
  the crater rings by the port only slightly sparser (~8-10 px) - another source is likely (the globe and the local
  terrain drawn over each other, or the known terrain-mesh "staircase"); OPEN - next: an A/B in Planet Lab.
- DEV frame captures were silently skipped: the game window was minimized (no drawable surface, no frame drawn).
  Now logged, and the DEV capture restores the window once (q4/q5 captures written).
- Gates (post-change build): run_gates -j 6: 209 / 203 pass / 6 known red / 0 new; --cpp-only 2/2 PASS
  (wire_data_plane_contract_tests after the protocol 26 change).
- LIVE q9 - the whole client route with ELITE_DEV_ROUTE_CHARGE_T=0.2 (official: refuel stop inserted by the
  planner -> the gate -> Shackleton pad -> the gate -> Earth orbit): ARRIVED at Высокий 119 m / 19.9 m/s ->
  SERVICE STAY (refuel; the server's stay, "done, J charge 3 t") -> the gate passed at 12 618 km/s -> the landing
  leg with the carried ramp -> GLIDE 330 km -> braking at 21.5 km -> HAND-OVER 6.84 km / 15.1 km / 449 m/s -> hover
  -> LANDED 9 cm -> TAKE-OFF, LIFT-OFF, complete at 86.87 km -> the gate -> Earth orbit ARRIVED 18 m / 3.0 m/s ->
  ROUTE COMPLETE. (Item 4: the service stay is now live-checked; the fine path is not.)
- 7 analysis ([HudVel], q4): the HUD's velocity reference matches the autopilot's ground velocity (ground frame:
  HUD 432 m/s vs the autopilot's 449). Large hull-velocity angles are where the hull is deliberately held off the
  velocity: the glide's phase 1 (up to ~50 deg, the nose +8..15 while the path bends to -40), the vectoring
  terminal (the hull level, the velocity down: ~90 deg in the vertical descent). No reference bug found; asked the
  author WHERE they saw it (manual flight or the autopilot). The log stays behind ELITE_DEV_HUDVEL_LOG=0 to mute.
- The offline glide model is kept in tools/research/landing_glide (README there).
- Author question (item 6): in the glide's first ~300 s the cockpit shows stars (physics + cockpit geometry, above);
  options: (a) accept - the drone view F3 shows the ship over the surface; (b) the autopilot switches to the drone
  view for the glide; (c) a shorter, steeper glide (nose lower, a harder final braking up to ~-35..-45 deg).

### Docking freezes / the final-approach hang (author 2026-11-07, live play) - FIXED in code, DEV-verified, NOT ACCEPTED
- Author: "при обычном докинге периодическое лёгкое подфризывание ... в финале кошмар ... игра повисла, закрыл".
- Evidence (D:/tmp/elite_hitches.log, the author's session 13:09): frames of 85-280 ms every few s, all in
  "dock_output_handling" (the client docking autopilot + its logs), then 0.5-2 s frames with hud / render_tail /
  ev_poll_hotkeys inflated too, one frame 26.8 s (20.7 s in the docking span).
- Root cause: std::cerr on Windows is unbuffered - every `<<` is its own write; into a console window one docking
  trace line (~60 inserts) costs tens of ms, and a console with a text selection (QuickEdit) blocks every write
  (a "hang"). The docking, dock-traffic and HUD logs are densest on the final approach. (stdout had been silenced
  for the same reason long ago; cerr had become the main log.)
- FIX: core/AsyncStderr.h - std::cerr appends to a memory buffer, a background thread writes it to the real
  stderr every 50 ms (8 MB cap, the excess counted and dropped); the SEH filter / terminate handler drain it
  synchronously. Finer hitch marks inside the Assisted docking step (dock_advisory_enter/_setup, dock_planning,
  dock_assisted_pre_autopilot / _autopilot_update / _handle_output).
- DEV A/B (automatic docking at Высокий, ELITE_DEV_DOCK_REQUEST_AT=15, hitch threshold 60 ms): before (dk1) 26
  slow frames, many with dock_assisted_handle_output 30-48 ms; after (dk2) 0 slow frames through to "docked"
  (only the two known start-up load frames). Gates 209: 203 / 6 known / 0 new. Author re-check pending.

### Author remarks 2026-11-07 (map, route window, undocking) - BUILT, partly DEV-checked, NOT ACCEPTED
- Detail map zoom: the map frames the planet's whole neighbourhood (Earth: the Moon's orbit); x16 left Earth a dot.
  Now max x4000, wheel step 1.15; zoomed in (> 1.5 x the fit) the pan / cursor pivot work in the spatial volume
  (before: the pan was forced to 0 - only the centre could be magnified). Visual check: author.
- Beacons (nav_relay world sites) as hubs ("маяк по сути модуль хаба"): Detail map kind world_site_hub:nav_relay
  (selectable, double click = its catalog Hub map with one module), System map: a relay without a parent hub is a
  hub target; a route point there = the catalog-hub Point stop at its marker (existing planner path).
- Route window: START / unofficial START disabled until РАССЧИТАТЬ succeeded for exactly this route
  (RoutePlan::planSignature: the author's points, roles, order, programs, actions; planner-inserted points
  excluded); any change disables them again until the next calculation.
- Undocking: Ctrl+L (plain L kept) = LAUNCH (the terminal hint says CTRL+L: LAUNCH). The station now flies a pilot
  ship the same axial departure as the traffic (out along the dock axis, flip, brake) and releases it AT REST
  beyond the queue gates (departureTurnZ -11 km) instead of at the portal. DEV dk3: the request was accepted, but
  the dispatcher never cleared it in ~10 min (queue position 2 behind VYS-1701, cleared only at the run's end) -
  departure starvation by the arrival flow: OPEN, next with the docking polish (the release beyond the gates is
  therefore not yet flown live).
- Next (author): docking polish, then the approach to a chosen ship and taking station in a formation.
- Gates 209: 203 / 6 known / 0 new; --cpp-only 2/2.

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


### User order 2026-11-05 w - fleet engineering context, unified spares and ship passports
Status: DATA FOUNDATION / PROPOSAL-model; frame passports deliberately deferred.
- Engineering inventory frozen as context: 18 economic classes -> 19 engineering types (Cobrett N1 and A3 are distinct propulsion architectures) -> 21 current model passports.
- 10 ship integrators grouped into 6 industrial-cooperation ecosystems: Sol, Cognium pool, Heavenly Pact, Southern circle, Citadel compact, Free Belts. Cross-group scores describe industrial compatibility/supply, not ideology.
- New machine-readable sources: canon/fleet_engineering_context.json, canon/ship_parts_unified.json, canon/ship_passports_3126.json. Human context: Notes/Lore/ECONOMY/FLEET_ENGINEERING_CONTEXT.md.
- Spare-parts unification: 121 legacy product SKUs -> 102 canonical replaceable SKUs (66 major technical LRUs + 36 mission/survival/weapon/payload modules). Drives 17 -> 10; propulsion reactors 15 -> 5; service reactors 7 -> 5.
- Accepted engineering rule: a larger standard engine may be derated to the ship rating; reactor power may be composed from parallel banks of the same smaller standard reactor. Rated ship thrust/power are unchanged in this pass.
- All 102 canonical parts have equipment envelope + service-clearance geometry. Cobrett anchors: CR-1 emitter dia 5 m, ER-5 cooling pad 11 x 11 m; other dimensions are PROPOSAL/model scaling by mass/power using ship_designer.py-style effective densities.
- Southern-circle legacy component names mizan.* are not canonical in the new parts database: they resolve to kapehu.core.k5 / kapehu.core.k12 / kapehu.mc.k1; legacy ids are migration aliases only.
- Cobrett N1/A3 jump installation is explicit in both the engineering passport and CobrettPassports.h: Z-0.5 jump core + jump-charge tank.
- Not done: hull/frame passports, exact mount coordinates for future 3D models, runtime migration to unified parts, flight-performance rebalance.
- Static data invariants checked: engineering types=19, model passports=21, canonical SKUs=102; every passport BOM resolves to a canonical part and every canonical part has geometry. Dry-mass deltas use ship override or ship_economics class mass; 3 passports exceed the 5% mass-delta threshold and are flagged for frame revalidation.


### User order 2026-11-05 x - interactive HTML ship/parts reference
Status: DONE / static engineering reference.
- Added Notes/html/ship_reference/: index.html, ship.html, style.css, app.js, data.js, README.md. No server is required; open index.html directly.
- Main view groups ships by industrial/political bloc and manufacturer; right pane shows passport, design language, weapons and replacements; one-click detail page opens the ship assembly.
- Ship page presents functional equipment tree rooted at 35 with A1 propulsion, A2 manoeuvring/RCS, A3 cooling, A4 power, A5 jump, A6 avionics/sensors, A7 comms, A8 life support, A9 cargo/passenger, A10 survival, A11 service/docking/tow, A12 weapons, A13 protection/landing.
- Part view prioritises equipment envelope + service clearance and renders an automatic schematic sketch from dimensions; reactor cooling pads, radiators and weapon/missile sub-passports get specialised annotations.
- Design-language doctrine is stored in fleet_engineering_context.json per cooperation group and per ship integrator (modular vs monolithic, cost/technology, maintainability, visual signature, redundancy).
- ship_parts_unified.json carries a same-group replacement and a foreign/enemy functional analogue where one exists; external analogue is explicitly not assumed plug-and-play.
- ship_passports_3126.json carries inherited design language, ship-level standard/foreign analogue, functional groups and weapon summary.
- canon/weapon_system_reference.json bridges ship weapon hardware to combat weapons.json. Current laser/missile dynamics remain from combat data; physical missile/emitter dimensions and mount bindings are PROPOSAL/model for model construction.
- Cobrett PD-1 is presented as a twin-emitter mount (proposal) to reconcile the engineering BOM with the legacy runtime's two laser hardpoints; legacy missile capacity=4 remains visibly flagged because a separate VLS-4 hardware passport is not yet authored.

### Docks as objects, hold points for the pilot, docking widget (author 2026-11-07) - IN PROGRESS
Author decisions:
- The dock (not the station) owns docking: a station / terminal has docks; each dock = entrance(s), places, holds.
  Dispatchers: "as many as needed - it is a game"; one per hub is fine if it copes.
- Dock kinds: rotating drum (current 6-place, artificial gravity; Assisted ships, they manoeuvre easily), a
  non-rotating "railway station" terminal of parallel tracks for Newtonian ships (later; one dock per arrival, NO
  holds), a surface pad (the Moon).
- Allocation criteria in order: 1) does the ship fit, 2) occupied / free, 3) the dock's purpose vs why the ship
  goes there; plus ship size.
- One entrance cannot serve two ships at once -> the dispatcher queues arrivals / departures.
- 6 hold points in front of the drum entrance, off the rotation axis, far enough for "clearance -> onto the axis ->
  roll to the entrance -> in"; the straight departure along the axis always stays free. Green light at once ->
  the autopilot docks directly as now, else a hold is the route's intermediate point.
- Player widget: calculating / moving / distance / dock queue position / estimated wait.
- Later (design): how a piece of a mesh model is marked as a dock (what the dock part contains, how places are marked).
Found in code (2026-11-07): the hex drum already has 6 holds (r 450 m, z -1600, FIXED frame) used by the traffic;
the pilot uses "direct entry" (requested by the server only when the ship is already on the axis; while it waits
nothing else is cleared) -> no hold for the pilot; one dispatcher hard-wired to hub_hex_dock_a; the cube service
block (80 x 80 x 600 m tube) is not a dock; the dispatcher prefers the older of arrival / departure, and arrival
pipelining kept the lanes busy (dk3: a departure waited ~10 min).
Queue: (1) departure fairness; (2) the pilot asks the dispatcher when docking is engaged, green -> direct, else
the assigned hold -> wait -> axis -> entry; (3) the widget; (4) dock kind / purpose / fit / free allocation in the
dock model; (5) mesh-as-dock marking (proposal for the author).
Done in code (2026-11-07, this pass; offline evidence, live pending):
- (1) Departure fairness: arrivals and departures alternate when both wait (DockTrafficController lastClearedArrival).
  EliteDockTrafficCheck now also reports max departure wait / max hold wait / the wait-estimate error. A/B on the
  old 4 profiles (cobretts/interval 4/600, 4/1549, 9/600, 6/300): max departure wait 816/494/1272/1054 s ->
  606/206/964/916 s; max hold wait 707/392/1622/1658 -> 1016/572/1622/1643 s; 0 violations.
- Lively docking (author 2026-11-07 "влеты в станцию бодрее, кобра может дрифтом заходить на площадку"): portal
  crossed at 12 m/s (was 4), corridor to 8 m/s (was 1.5), in the chamber a drift onto the pad (one arc from the
  axis to above the pad, the hull swinging nose-to-exit, braking 1 m/s2), touchdown 6 s (was 12), departure lift
  6 s / to the gate 22 s (was 12 / 35), out at 12 m/s (was 5); the pilot's capture keeps its own closing speed
  (was a smoothstep from rest at 4 m/s, up to 150 s). Offline 4 profiles: 17.3 / 16.3 / 17.2 / 16.7 arrivals/h
  (was ~12), max departure wait 286 / 106 / 566 / 416 s, max hold wait 360 / 360 / 1094 / 1116 s, 0 invariant /
  separation / structure violations, min gap 34.9 m (neighbouring pads). Wait estimate = lane movements ahead x
  110 s, mean abs error 57 s (4/600) / 134 s (9/600).
- (2) Pilot hold points: ClientShipCommand::RequestDockArrival (wire 27) sent when automatic docking into the hex
  dock starts (cancel when the request is dropped); the server asks dispatcher.entryFreeNow(): free -> direct entry
  (as before), busy -> queued, hold nearest to the ship's azimuth; at the hold (< 60 m, < 2.5 m/s) the server
  reports AtHold; drop radius 20 km. Snapshot carries the wait estimate, movements ahead and hub-local hold / entry /
  axis. Client gate (SpaceState::updateDockingDispatcherGate): waits for the answer, flies to the hold (straight,
  a detour around hub obstacles, 90 m/s cruise, 1.2 m/s2 braking, terminal hold facing the dock), waits; on
  EntryCleared plans the normal docking from the hold - the route then starts at the new portal
  hex_dock_a.axis_approach (axis z -900) instead of the far blue cylinder. DEV ELITE_DEV_DOCK_FORCE_HOLD=1.
- (3) Widget: presentation/DockingWidgetText.h + SpaceState::renderDockingWidget (status: request / preparing /
  calculating / moving / to the hold / waiting / entry cleared / station control / docked + Ctrl+L / departure
  queue; distance to the entrance and speed; queue position, movements ahead, wait estimate).
- (5) PROPOSAL for the author - a dock inside a mesh model (Model Asset editor sockets already have kinds and
  local position / rotation; the editor colours "dock" sockets): a dock = a named group of sockets, the game builds
  DockFacilityDescriptor from them at load instead of HexDockFacility.h constants:
  dock_root:<id> (kind drum / track / pad, spin axis + rate for a drum, purposes); dock_portal:<id> (centre of the
  entrance, -Z = inbound, scale X / Y = aperture); dock_pad:<id> (surface centre, +Y = pad normal, -Z = parked
  nose, scale = footprint; size class, purposes); optional dock_hold:<n> (else 6 generated in a ring in front of
  the portal) and dock_lane:<id> polyline (portal -> pads). Collision stays the part's collision boxes.
  Track terminal = one portal + one pad per track, no holds. Author decision pending.
Author decisions 2026-11-07 (later the same day):
- A ship may or may not have an autopilot installed. The docking request builds a GREEN corridor (with a BLUE part
  where required) and that corridor may lead to the hold point; the hold point is marked by a VOLUME.
- The widget is a small panel with the key values (no long phrases); the status text goes to the HUD console
  (the docking terminal), which exists for that.
- Proposed (no answer yet): positioning tiers (exact in station / beacon range, inertial-navigation uncertainty
  circle between them shrinking on celestial / beacon fixes, star and planets always visible as references).
Results of the pass (2026-11-07, DEV live runs dk4..dk11, Assisted Cobra at Высокий, NOT author-accepted):
- dk4: the hold path worked (H4 reached to 4.5 m, wait, clearance) but the planner route from the hold was 44.9 km
  (route speed 500 m/s sized a multi-km terminal approach) -> near-dock route speed cap 35 m/s; dk5: still 5.8 km
  (the ordinary 1000 m minimum start straight) -> near-dock lead = 8 hull lengths; dk6 (holds at -1600): 4.9 km,
  the ship reached the axis then looped out to z -1900 (the planner's ~900 m terminal straight starts at z -1400,
  the hold was inside it) -> holds moved to z -2600 (HexDockFacility holdZ); dk7/dk8: the loop stayed (the axis
  portal at -900 / -1800 inside the planner's terminal geometry, 6.0 / 3.5 km).
- Decision: from a hold the entry is always the same geometry -> a dedicated hold-entry path (client gate): a
  quadratic arc from the hold onto the axis 1600 m out (control 2200 m out), the axis straight to 150 m out, flown
  at 35 m/s on the arc / 20 m/s on the axis (law-neutral stabilize controllers, cross-track correction); the
  station captures it on the axis (capture now < 25 m/s, z > -1000) and aligns the roll. dk9: clearance -> capture
  72 s (dk5 215 s, dk8 109 s), cross-track <= 16 m, 2.4 km path; full cycle to "released to the pilot beyond the
  dock lanes". The planner route from the hold (axis_approach portal) stays for starts in front of the dock.
- Lively entry live: capture -> latched 53 + 36 s (dk9; was ~4.5 min in run5); direct entry dk11: request 10.7 km
  away -> latched 272 s; departure request -> cleared after the alternation (162 s, a traffic arrival + departure
  ahead) -> released ~210 s later at rest beyond the gates.
- Docked = the docking request is complete (the client clears it; nothing replans a docking after the release).
- Green corridor (source StationTrafficControl, priority 150, green in GuidanceCorridorRenderer): to the hold
  (detour point included) + the hold volume (a 160 m box of frames facing the axis approach), then the entry path;
  hub-local, placed with the RENDER reference frame; the request enables the HUD corridor + server guidance modules.
  Requests without an autopilot (guidance mode) now also go to the dispatcher: corridor shown, nothing flown
  (source-audited; the manual request UI/key not yet added).
- Widget = a compact panel (title, status chip, label/value pairs: to entrance, speed, ahead / queue, wait, place,
  Ctrl+L); the phrase goes to the docking terminal as its own "dispatcher" line (printed on change, no RECOVERED).
- Offline final (holds at -2600): EliteDockTrafficCheck 4 profiles 14.8 / 14.8 / 14.8 / 14.5 arrivals/h, max
  departure wait 364 / 163 / 506 / 516 s, max hold wait 582 / 454 / 1260 / 1288 s, 0 violations, min gap 34.9 m;
  wait estimate (120 s per lane movement) mean abs error 58..238 s. docking_infrastructure_api_tests PASS (new:
  purpose, entry-or-hold, alternation). Gates 209: 203 pass, 6 known red, 0 new; --cpp-only 2/2.
- OPEN / next: the manual docking request (key / dock card) and HUD markers for a manual pilot inside the corridor;
  the far direct route through cylinder B is long (dk11: 48.65 km plan at up to 217 m/s from behind the dock -
  pre-existing planner behaviour); roll indicator for manual entry; the Track terminal / shuttles in the cube
  block (model ready: DockKind, purposes); positioning tiers (author answer pending).
Author 2026-11-07: "трафик нет смысла делать затянутым - обычное ожидание до 5 мин, но в случае эвакуации или
боевых действий нагрузка может и расти"; "маяки должны быть не только возле планет, но и по маршрутам; маршрут
меняется из-за движения планет"; dock markup - "главное, чтобы был механизм считывания этой разметки" (queued).
- Flow control (DockServiceTraffic::flowHeld, TrafficConfig normalWaitLimitSeconds 300 / emergency): scheduled
  traffic (a Cobrett at its far point, an inbound shuttle before it enters the zone) is held while a new arrival
  would wait longer than 300 s less its trip in, or while no pad would be free for it (known arrivals + inbound
  >= free pads); pilots are never held; ELITE_DOCK_TRAFFIC_EMERGENCY=1 / --emergency lifts it. Dispatcher API:
  predictedNewArrivalWaitSeconds, knownArrivals, freePadCount, TypicalMovementSeconds 120.
  Offline 4 profiles: max hold wait 234 / 216 / 276 / 230 s, max departure wait 240 / 186 / 199 / 261 s,
  12.2..12.5 arrivals/h, flow holds 134..348 per 6 h, 0 violations; emergency 9/600: 1260 / 506 s (unchanged).
  Gates 209: 203 pass, 6 known red, 0 new; API tests PASS. Live: not run for this slice (game build OK).
Author 2026-11-07 (regression report): "исчезла траектория на карте хаба ... мы теперь не проходим через синюю зону ...
верни". Root cause: the hold flow erased the planner's route (the Hub Map draws only the docking computer's corridor
id dock:<module>:<anchor>) and flew straight to the hold / from the hold (and the planner's near-dock start skipped
the cylinder). Fix: the planner's near-dock start (axis_approach portal, 35 m/s cap, short lead) REVERTED - the
direct route is again cylinder_b.entry_front -> dock front; the hold path now goes ship -> 700 m straight -> through
the guidance cylinder (blue lane) -> 150 m beyond -> the hold (free legs bent around obstacles), corner speed caps
braked into; the Hub Map gets the whole trajectory (to the hold + the entry preview into the dock) under the
docking computer's id. dk12: hold path 20 km through the blue lane, cross-track in the cylinder <= 14 m, but ~1 km
wide after the exit turn at 90 m/s -> corner caps (dk13 pending).
Author 2026-11-07 order: interstellar JUMPS next - charging 5..10 min max (not 6..30 h), limits: fuel + the
procedure; target (orbit / hub) -> "рассчитать прыжок" -> straight legs avoiding hazards, preferring inhabited
systems, fuel check + refuel points -> "старт"; a cartoon-style effect (shake, tail up, jerk, cockpit distortion,
new sky); the exit's direction and speed chosen to reach the hub / orbit, else hang in space. Beacons stay as they
are. Readiness audit (2026-11-07): server/client already follow the active system (GameServer::
resolveSingleActiveSimulationSystemId, client snapshot by playerNavigation.currentSystemId), ephemerides exist for
all 61 systems, the starfield is observer-relative (changes after a jump); MISSING: any code that moves a ship to
another system, runtime hubs outside Sol (all 10 orbital_hubs + 8 runtime bindings are Sol; 539 sites in 28
systems are map markers only), interstellar routes / max hop / neighbour table, jump charge consumption and
charging time, a cross-system planner, the zones (jump_distortion / unstable_navigation) are not loaded.
Author 2026-11-07: "мне кажется или у тебя полностью сломался и планировщик и автопилот?" / "а зачем нам к доку
лететь по ломаной? ломаная нужна была для прыжков" / "прыжок доставляет к хабу, а возле хаба уже действует обычная
механика стыковки". Diagnosis: the planner / autopilot code was not changed, but the client dispatcher gate took the
control stream away from them whenever the dock was busy (a home-made polyline pilot: 20 km to the hold, up to
~330 m off its own corridor; guidance-mode requests got no planner at all; a 4 s pre-wait). DECISION: the gate no
longer flies or draws anything - it only tells the dispatcher (RequestDockArrival, always a direct-entry request:
first in the queue, captured on the approach axis once cleared) and ends the request when docked. REMOVED: the
hold path / entry path pilots, the hold / entry corridors and the hold volume, the green frame colour (my misreading
of "green corridor" = the zone type, not a colour), ELITE_DEV_DOCK_FORCE_HOLD. Kept: dispatcher alternation, flow
control (<= 5 min), lively entry + drift, the panel + console line, holds at z -2600 for the traffic. The DEV dock
request now enables the same guidance modules as a dock-card request. Live dk16/dk17: route stages free ->
capture cylinder_b.entry_front -> blue lane (hard) -> release -> terminal; Hub Map trajectory shown; request
10.7 km out -> latched 272 s; full cycle to the release beyond the gates. A pilot hold point needs the planner
itself to plan a stop there (not done).
Author 2026-11-08: "почему после отстыковки не возвращается управление кораблем?" then "управление обратно отдано,
но уже и из дока вылетел и виджет пропал. на терминале ХУД хоть какую-то информацию выдавать надо". Diagnosis: the
station flies a departing pilot ship along the axis to rest at departureTurnZ (-11 km, ~2-3 min) and releases it
only there; the dispatcher marks it Departed at the lane clear point (-2.4 km), so the widget (hidden on Departed)
vanished while the station still held the ship. Author rule (same day): touching the controls while an autopilot
flies -> a warning, after 3 s the autopilot is off and the route is dropped; a route flown by hand (guidance mode)
is dropped only by removing its card on the map (maybe later a button).
DONE (source + live DEV): SpaceState::updatePilotTakeover - the pilot's controls are sampled every control frame;
owners: the docking autopilot (automatic request), the client route, the body autopilot, the station on the
departure axis (beyond the gate). Touch -> "ВНИМАНИЕ: руль - ... через 3 с"; letting go > 1 s cancels; 3 s of
handling -> docking request cleared (dispatcher cancel) / route cancelled / body autopilot cancelled / station
asked to release (ClientShipCommand::RequestDockDeparture index 1 -> DockServiceTraffic::releaseExternalToPilot,
AxialOut only, never inside the chamber). Inside the dock: only a notice. A released departing pilot off the axis
(radial > 300 m) frees the outer lane. Widget: the departure stays on the panel until the station lets go
("ВЫЛЕТ / СТАНЦИЯ ВЕДЁТ", ДО ВЫПУСКА km, СКОРОСТЬ, ПЕРЕХВАТ руль 3 с), then "УПРАВЛЕНИЕ ВАШЕ" for 8 s; console
lines without changing numbers (no repeats); the HUD console gets a separate pilot notice (warning severity,
6 s life). Live DEV dk19: docked -> departure queued 2nd, cleared after 177 s -> beyond the gate a 4 s yaw ->
warning -> "released early" at z -175, free flight. dk20: a 1 s yaw during the docking autopilot -> warning ->
"continues"; a 5 s yaw -> "АВТОПИЛОТ ВЫКЛ.: стыковка прервана", dispatcher cancel. Gates 209: 203 pass, 6 known
red, 0 new; docking API tests PASS; DockTrafficCheck 0 violations. Not flown by the author yet.
Author 2026-11-08: "иногда мне кажется, что повороты корабля на столько рваные, что я принимаю их за фризы".
Measured (DEV ELITE_DEV_ROT_TRACE=<csv>, the rendered attitude per frame): steady 90 deg/s yaw / pitch / roll,
hub frame and free flight: no stalls, frames <= 18 ms, the per-frame step follows the frame time (corr 0.64-0.73,
one-frame offset). Hypothesis "the render blend towards a tick-stepped target pulses" - a rate-propagated render
attitude was tried with a pre-registered PASS (frame-jump p95 <= 12 %): FAIL (no measurable change) -> REVERTED.
The author's 00:05 session hitch log had almost no frames >= 80 ms in flight. OPEN: catch the real case - a turn
exposes any 30-80 ms frame stall (4.5 deg at 90 deg/s) that the 80 ms hitch log misses; next author session with
ELITE_DEV_HITCH_MS=35 (+ ELITE_DEV_ROT_TRACE) to see which phase stalls.
Author 2026-11-08 (jump plan): the reading of points 1-6 confirmed; hubs exist only where they are written in (each
with its reason to be there); without a hub the jump exits on a planet orbit or just in space - START WITH ORBITS.
Author 2026-11-08 (Assisted route): "ассистед, выбрал соседний околоземный хаб, нажал перелёт - всё время вектор
главной скорости не совпадал с вектором тяги ... если полёт происходит в режиме стабилизации, не надо из него выходить;
если выходим, ассистед должен вести себя как ассистед". Reproduced (DEV D:/tmp/elite_as1, Высокий -> Тихий, A3):
HUD hull-to-velocity 106-110 deg from the first second, 95 deg on arrival. Root causes: (1) the route leg switched to
normal flight and the server reset the velocity reference to the dominant body (Earth, 7.6 km/s orbital), while the
client pilot flew against the hubs - the marker showed the orbit, the nose pointed at the destination; (2) inside
the departure keep-out the nose pointed at the destination while the field pushed AWAY from the hub; (3) the client
ramp ran on while the plant dropped the field under Earth's J floor (the chord between two 420 km hubs dipped to
293 km < 319 km floor) -> braking planned on 1e5 g with 2e2 g real -> 8 000 km overshoot, nose 170 deg off.
DONE (source + live DEV, wire protocol 28): ShipControlState::globalReferenceHubId - the route pilot names the hub it
flies against (the departure hub inside its keep-out, then the destination; a guide point before a hub flies
against that hub); the server keeps that hub as the velocity reference (kind 0, HUD "скорость ... отн. хаба Тихий",
the Assisted alignment, END); the nose = the wanted velocity against that reference (the J field bends the real
one onto it); the pilot follows the plant's real J level (held 0.5 s, replication lag) and plans braking with the
ramp time still ahead; route clearance never under a body's J floor + 10 km (BodyFlightRules clearanceM,
EliteBodyFlightRulesCheck PASS). Live as2..as4: as3 showed a false ramp sync at the guide point (lag) -> fixed by the
0.5 s hold; as4: departure 25 -> < 1 deg within 8 s, transfer and arrival 0-4 deg, no overshoot, arrived 119 m /
20 m/s, route complete (143 s). Remaining: at the reference switch (departure hub -> destination) the two hubs
differ by 8-11 km/s - the nose turns at once, the velocity follows in ~17 s (130 -> 15 deg) - an Assisted turn,
not removable while the thrust stays along the nose. Gates 209: 203 pass, 6 known red, 0 new. Not flown by the
author. OPEN reading (asked): "не выходить из стабилизации" = keep the hub reference (done) or literally fly the
in-keep-out departure / final approach in the stabilization mode on ordinary engines.
### Jump stage J1 2026-11-08 - interstellar jump to a planet orbit - LIVE DEV, NOT AUTHOR-FLOWN
- Survey: the runtime is keyed by systemId end to end (the server's simulated system = the player ships' system,
  GameServer::resolveSingleActiveSimulationSystemId; gravity / celestial snapshot / client sky + maps follow); every
  system sits at its own origin; nothing ever changed a ship's system before. Hubs exist only for system 0 (spawned at
  startup) - none elsewhere, as the author wants (a hub only where it is written in).
- Rules (navigation/rules/JumpRules.h, shared): hop <= 32 ly (canon max_hop_ly of 3126), mass <= jump core rating,
  charge q*d*(alpha+M) (JDrive.h logistics rule), charge-up 5..10 min by distance, refusal codes + texts.
- Server (GameSimulationJump.cpp): ClientShipCommand::RequestJump (index = system, -1 cancel; wire protocol 29,
  snapshot schema 41 - motion.jumpState / target / distance / cost / charge elapsed+duration / refusal + serials);
  START only in normal flight; the charge-up is silent and is lost if the ship leaves normal flight; at its end the
  jump waits until the ship is clear of objects and above every J floor (jEngineBlocked), then: charge spent, motion
  AND travelFrame systemId switched (the replicated frame identity carries the client's system - first run showed the
  client still in system 0), a placeholder until the new system's bodies are in the gravity context, then a circular
  orbit of the exit planet at its J floor + 100 km on the lit side (the planet with the most world sites, else the
  heaviest; no planet -> 1 AU from the star at rest); the automatic stabilization takes the closed orbit.
- Client (SpaceStateJump.cpp): galaxy map star context menu РАССЧИТАТЬ ПРЫЖОК -> notice (distance, charge, charge-up
  time or the refusal) -> ПРЫЖОК: СТАРТ (enabled only for the calculated star) -> ОТМЕНИТЬ ПРЫЖОК while charging;
  HUD panel (target, ly, charge-up m:ss with a bar, cost; "ЗАРЯЖЕНО: отойдите от объектов"); refusals in the HUD
  console; the jump = a cartoon ripple (flash + 4 dotted rings, 1.6 s). DEV: ELITE_DEV_JUMP_CHARGE_S, verbs
  jump:<id>[:<body>] / jump:cancel / jumpui:calc|start|cancel:<id>, the DEV pointer can click galaxy stars
  (galaxy-system:<id>), ELITE_DEV_JUMP_DIAG, ELITE_DEV_JUMP_NOSE_DOWN.
- Live DEV (D:/tmp/elite_jp1..12, Высокий -> Fomalhaut 25.1 ly, 0.91 t of 3 t): START before РАССЧИТАТЬ ignored;
  menu rclick + РАССЧИТАТЬ through the real map input (jp3); waited at 5 km from the hub, jumped at 20 km; exit on an
  840 km orbit of Fomalhaut_g, stabilization ON 5 s later; client system 61, snapshot 61, renderer alt 840 km.
  Gates 209: 203 pass / 6 known red / 0 new; C++ wire / map contracts PASS.
- Found, open: (1) Fomalhaut_g (Venus-like, 932 bar CO2, little relief) renders in the Elite style as a BLACK disc
  with no contours - nose-down frame has no stars at all, so it is drawn, but looks like nothing (author decision on
  the look of featureless / dense-atmosphere bodies); (2) for ~1 s after the jump the radar showed Sol's ships
  (12 tracks) - other-system ships are still replicated; (3) multiplayer: players in different systems not handled
  (the server simulates one system); (4) next: J2 multi-hop planner (hazards, inhabited systems, charge, refuel points)
  needs hubs in other systems (spawn from the written-in world sites).
### Assisted route = Assisted, jump exit + radar 2026-11-08 - LIVE DEV, NOT AUTHOR-FLOWN
- Author's reading resolved: "Assisted always turns along the main velocity vector; leaving the stabilization the
  ship lines its thrust up with the ORBITAL velocity and flies to the other hub from that attitude" - the main
  velocity is the one against the dominant body, not a hub (the ce884a11 hub reference made the motion look
  sideways). RouteLegPilot (Assisted law only; Newtonian keeps the old law): the reference = the plant's primary
  body (its velocity differenced on the client); LINING UP first (velocity held, nose onto the main velocity, < 3 deg);
  then the nose leads the real main velocity by <= 2 deg towards the wanted one and the commanded velocity stays
  within 2 deg of the hull; speed only while the hull is within 45 deg of the wanted direction (none beyond: as7 showed
  circling at 300 km - turning radius v/omega); a motion under 50 m/s frees the nose onto the wanted direction (as8
  freed it by a relative threshold at 7 km/s -> 20-28 deg); alignment kept past a guide point.
- Live DEV earth_orbital_hub -> earth_hub.tikhiy (D:/tmp/elite_as5..9): as9 HudVel hull-velocity <= 2.5 deg in 73 of 75
  samples (20 deg at 403 m/s while turning in place, 11 deg at the guide point - since fixed, not re-flown), arrived
  120 m / 20 m/s, route complete. Newtonian-I same route (elite_nw1, ELITE_SHIP_PHYSICAL_TEST_MODEL): arrived 119 m /
  19.8 m/s, complete (nose free, as Newtonian).
- Jump exit body chosen by the server: world sites > canon (observed/authored) planet > heaviest. Fomalhaut has no
  world sites -> Lexie (jp13/15: orbit 444 km, stabilized). Fomalhaut_g (Ananke, procedural) was the old choice.
- Radar blinded by the jump: from 3 s before the charge completes, while charged/waiting and 3 s after - no
  contacts, snow + streaks drawn on the scope (RadarWidgetBase::setJamLevel; shader effects also forced). jp15 frame:
  snow visible, 0 tracks; after the jump 0 tracks (no Sol ghosts).
- Gates 209: 203 pass / 6 known red / 0 new.
- OPEN (author directive 2026-11-08): "the server must simulate the systems or parts of systems where players are" -
  multi-system runtime (today one active celestial context; a second player in another system blocks a jump exit).
### Multi-system server + 153-player load test 2026-11-08 - BUILT, HEADLESS LOAD TEST + 1 LIVE DEV JUMP, NOT AUTHOR-FLOWN
- Author directives: "сервер должен считать те системы ... где находятся игроки"; "по 2-3 нпс в каждой из 61 систем ...
  нпс = имитация игроков"; "в населенных системах есть хабы? если нет - хотя бы по одному на жилую зону"; "погоняй ... с
  перемещениями, прыжками нпс, тиками по экономике".
- Multi-system tick: GameServer::update runs one GameSimulation::update pass per system that has a ship (plus the start
  system), after switchSimulationContext(systemId) (bodies from the analytic ephemeris at this universe time, gravity
  parameters on the first visit). GameSimulation parks / restores the per-system celestial context
  (m_parkedCelestialContexts); hub frames / velocities stay global (hub id keyed), each pass rebuilds its own system's.
  Pass 0 runs the once-per-tick work (clocks, combat, shells, dev scenarios, diagnostics); passOwns() filters the
  free-flight / jump / hub-service / reference-binding / zone loops. resolveSingleActiveSimulationSystemId removed;
  4 textual contracts (active_system_context, kinematic_frames, multiplayer_session_navigation, system_membership)
  rewritten to the pass-loop contract.
- World-site hubs: GameServer::materializeWorldSiteHubs - one runtime orbital hub per body with world sites outside Sol
  (the most populated orbital site; surface / belt sites -> 300 km above the J floor), command_station module:
  99 hubs in 27 inhabited systems (107 with Sol's 8). 33 systems have no sites -> no hubs.
- Replication per session: only the session's system's objects / hubs / signals (was: all 110 objects / 107 hubs of
  all systems in every snapshot, 134 KB, 2.2 MB/s per player); on a system change the old system's rows are removed.
  Also: the source id sets built once per publication (ReplicationSourceIds + fingerprints; were rebuilt per session),
  materializeCanonicalFromFullSet (moves the omitted graph fields; the generic merge deep-copied each ship 2-3 times).
- Jump exit: every ship its own point on the lit side (+-60 deg by ship id) - the load test showed all ships jumping
  to one planet exiting INSIDE each other and blocking each other's J engine / next jump for good.
- EliteServer --load-test [K|2-3] [SECONDS] [move] (build-relwithdebinfo, -O2): every bot is a real session (account,
  bootstrap player ship, own transport encoding every snapshot with the TCP wire codec), control state every tick,
  time sync 2 s, galaxy map 60 s; move = 4-min travel cycles (stabilization off, J on, full 37 s, brake 40 s; every
  other cycle a jump to a < 30 ly neighbour). ELITE_DEV_ECONOMY_TICK_S: a DEV server economy round (market / events /
  board of every economy hub, a new day per round) spread over the round's ticks. ServerTickProfile = per-stage timing.
- Results, 153 player bots in 61 systems, 50 Hz (20 ms budget):
  first run 48.6 ms avg / 123 ms p95 per tick, 2.4 MB/s per player; after the fixes (static): 14.8 ms avg, p95 22.8,
  74 % of one core, 165 KiB/s per player, 278 MB. Move scenario 480 s (jumps charge 20 s, economy every 10 s):
  15.3 ms avg, p50 11.9, p95 23.7, max 68 ms, 77 % of one core, 144 KiB/s per player, 291 MB; 141 jumps requested,
  101 done, 0 stuck exits, all 153 ships present. Per tick: simulation 11.1 (61 passes; physics 2.8, AI/controls 2.1,
  signals/radar 1.8, frames+free flight 1.6), snapshot build 1.0, selection 0.9, encode 0.9; economy 84 ms per
  full round (all hubs) = 0.31 ms per tick spread (all at once: 57 ms -> a 170 ms tick). Baseline 1 player in Sol:
  0.55 ms per tick.
- Found, open: (1) the bots' blind J runs hit planets (100 dense-layer entries, 46 surface contacts at up to 2000
  km/s): the J floor switches J off correctly, but the momentum carries the ship in and nothing happens to it (no
  damage / loss) - author decision on impact consequences; (2) the own ship is ~5.3 KB per snapshot (16.7/s) - the
  remaining wire load (ships' graph / systems payload); (3) the snapshot tick (every 3rd) is ~2x an ordinary one
  (p95 23 ms) - publication could be spread over the ticks; (4) 14 jump refusals "need normal flight" are the bots'
  toggle desync (open-loop script), not the server.
- Live DEV (EliteGame, jt1): Sol -> Tau Ceti jump, exit on Tau_Ceti_m orbit 418 km, stabilization ON, radar 0 tracks.
- Gates 209: 203 pass / 6 known red / 0 new.

### Impact destruction + staggered snapshot publication + sound effects off 2026-11-08 - BUILT, HEADLESS LOAD TEST + 1 LIVE DEV DROP, NOT AUTHOR-FLOWN
- Author decisions: "при ударе корабль разрушается"; "сделай рассылку снимков по тикам"; "отключи пока все звуковые эфекты".
- Destruction: DynamicMotionState::destroyedCause (0 intact, 1 surface impact beyond survival = the old non-survivable
  CRASH, 2 dense atmosphere above 30 g deceleration); wire 30 / snapshot 42. GameSimulation::destroyShipOnImpact:
  cancels a jump charge-up, J off, thrust 0, controls zeroed, ShipCore::destroyWholeShip (every module + support link
  destroyed, hit volumes rebuilt), NPC -> markShipDerelict; a player's session keeps the wreck. The wreck flies on
  ballistically (drag, gravity), never auto-stabilizes; GameServer zeroes its control stream and drops its commands.
  Client: HUD "КОРАБЛЬ РАЗРУШЕН: удар о поверхность / перегрузка в плотных слоях атмосферы"; a client route stops.
  Note: a steep entry at orbital speed (> 30 g) is now fatal; a shallow decaying orbit is not.
- Live DEV (EliteGame, ELITE_DEV_J_DROP=40,3 at Earth): "DESTROYED - dense atmosphere at 23655 m/s, 364 g", HUD
  message shown, wreck decelerates 6.9 -> 4.6 km/s at 42-43 km over 16 s.
- Staggered publication (ServerRunner): the snapshot is still built every 3rd tick; session k publishes it
  ((id-1) mod 3) ticks later (never more than one interval since its last publication); the session's system
  for the object / hub filter is taken from the published snapshot. DEV A/B ELITE_DEV_SNAPSHOT_STAGGER=0.
  A/B, 153 bots static 120 s, same machine state, alternating: off p95 32.7 / 33.7 ms, p99 35.4 / 38.7;
  on p95 26.3 / 27.7, p99 28.1 / 32.6; avg unchanged (18-20), p50 up (15 -> 18-19: every tick carries a third).
  Machine was slower than in the earlier run (simulation 13.4-14.7 ms vs 11.1 for the same scenario); absolute
  numbers between sessions are not comparable, the A/B is.
- Move run 480 s with destruction: 60 of 153 bots destroyed (24 surface, 36 atmosphere) by their blind J runs;
  102 of 141 jumps done; avg 21.2 / p95 28.4 ms on the slower machine state.
- Sound: AudioEngine::playTerminalTick (the only sound effect) is off unless ELITE_SOUND_EFFECTS=1.
- Open: respawn / what the pilot sees after the loss (author decision) - the wreck's cockpit still shows the service
  reactor panel running (ReactorSystem's temporary descriptor is not tied to modules); the bot pilot should avoid
  bodies (closed loop) so the load test keeps its population; the snapshot build tick still carries the build
  (~4 ms) - p95 26-28 > 20 ms budget.
- Gates 209: 203 pass / 6 known red / 0 new.

### Author canon 2026-11-08: the cockpit is the escape capsule (input for after-destruction) - NOT IMPLEMENTED
- For small frontier ships the pilot's cockpit IS the escape capsule: hard (not impossible) to destroy. On critical
  damage or on demand it detaches and flies fast on its own engine to the nearest inhabited zone broadcasting SOS
  (no range limit set yet). If the ship survives, a tug can be asked to haul it back, the capsule re-docks and the
  ship can be repaired - and the same path lets a pilot steal someone else's abandoned ship.
- Existing code to build on: ShipCore::ejectCockpitCapsule / ClientShipCommand::EjectCockpitCapsule (the cockpit
  fragment = the capsule carrier) and markShipDerelict (abandoned hull, no AI). Today's impact destruction destroys
  every module including the cockpit; with this canon a fatal impact should first try the capsule (author to decide
  whether a capsule survives a surface impact / dense-layer breakup).

### Ship reference HTML redesign 2026-11-08 (Notes/html/ship_reference) - BUILT, CHECKED IN HEADLESS CHROME
- Author order: catalog of ship cards (picture + name) by bloc -> manufacturer -> role / class; ship view with a brief
  side block and the functional A-frame schema of module cards with a shared "model ready" tick; module view with
  description, characteristics, dimensions, an image-generator prompt, a sketch drop group, a sketch modal with notes
  and re-save; the scroll position survives reloads everywhere. "The number before A is the ship's model code - each
  ship its own."
- One page index.html (?ship=, &part=, ?view=coverage; ship.html redirects). model_codes.js: one code per ship
  (Cobrett-I N1 35, the rest 36-72 in catalog order). CARRIED INTO THE CANON the same day (author: "перенеси шифры в
  канон"): ship_passports_3126.json passports have model_code and group codes <model_code>A<k> (21 passports, 235
  groups, 0 mismatches); the 17 proposal ships take theirs from build_proposals.js PROPOSAL_MODEL_CODES (groups by
  their A<k> part); data.js updated and reproduced byte-for-byte by the builder; model_codes.js removed, the page
  reads model_code. Nothing else consumes the passports canon (no C++ / gate reader).
  store.js: IndexedDB always + optional folder mirror via the File System Access API (workshop/: workshop.json,
  sketches/<part>/, ships/<ship>) so sketches can live in git. tools/render_check.js now checks the data
  (38 ships, 38 unique model codes matching their group codes, 104 parts, 0 problems).
- Checked by driving headless Chrome over CDP: 2 sketches dropped, note saved and persisted after reload, model-ready
  tick shared on the ship schema, label click inside a card toggles without navigating, scroll restored after reload
  (ship view 700 px, catalog 1200 px). Not checked headless: the folder picker (needs a user gesture).

### Escape capsule outcome at a fatal impact 2026-11-08 - BUILT, 2 LIVE DEV DROPS, NOT AUTHOR-FLOWN
- Author: "капсула может пережить удар, но выдержит ли внутри тело удар ... штука не хрупкая, но разрушаемая;
  несгораемая в атмосфере типа земной и более разреженной".
- navigation/rules/CapsuleRules.h: body limit 50 g (restrained short pulse, Stapp 46 g); surface impact: body g =
  v_vert^2 / (2 x 6 m) (hull crumple + capsule absorber), the shell crushed above 1000 m/s total; dense-atmosphere
  breakup: body g = max(drag now, Allen-Eggers ballistic peak v^2 sin(gamma) / (2 e H) while descending); the capsule
  burns / crushes only in an atmosphere denser than 1.5 x Earth's surface pressure. Only a ship with the detachable
  "ship_cockpit" (Cobrett-I) carries one; others -> "no capsule".
- Catalog surface pressure now reaches the server bodies (CelestialBodyDefinition / gravity parameters /
  GravityBody.surfacePressurePa; Earth 101325, Venus 9.2 MPa, Mars 610). The DRAG model still uses Earth density for
  every atmosphere (unchanged - a separate decision: it changes every descent / landing).
- Replicated DynamicMotionState capsuleState (1 alive, 2 pilot killed, 3 capsule destroyed, 4 no capsule) +
  capsuleBodyG; wire 31 / snapshot 43; HUD appends the outcome to the destruction line; load test counts them.
- Live DEV (Earth): 40 km at 3 x circular -> breakup 364 g, "капсула цела, пилот погиб" (HUD shown); 300 m drop ->
  vertical 68 m/s, ship crushed, pilot 39.7 g "капсула цела, пилот жив" (horizontal 461 m/s = Earth's rotation under
  a body-fixed drop; the body g uses the vertical component only - a simplification).
- NOT DONE (next slice, needs author input): the capsule separating and flying on its own engine to the nearest
  inhabited zone with SOS, rescue / tug back / re-dock / repair, stealing an abandoned hull; what the player does
  after "pilot killed". Gates 209: 203 / 6 known / 0 new.

### Ship naming by manufacturer + per-maker model codes + bloc completeness + manufacturers page 2026-11-08 - BUILT, CHECKED HEADLESS, NOT AUTHOR-ACCEPTED (names PROPOSAL)
- Author orders: per-manufacturer naming traditions (not translation; slang-born names allowed; own-script cultures
  carry the local name + a toneless export latin like "Mazda"; Chinese names loud like their space programme);
  the code before "A" is each maker's own marking; every bloc must field every ship type it needs (generate the
  missing); a manufacturers page with locations, hubs and hub modules (module unification per civilization).
- Lore research (4 read-only agents) grounded the doctrines: Cobretty = KOBRA dock slang of Kobersberg-Braunauer
  (history.md), Homestead en; Kern en, prospectus culture; Hyperion en + 中文, Greek titan line; OFU / Luna-Mars Yards
  Sol, en with Cyrillic drawing-code heritage (part codes КЦ-1000, АК-1); Harmony 新和 中文; Tianhe party state;
  Bharat hi; Bastion nb (BS hull numbers, Old Norse only for myths); Stone Docks no system (Seal Register).
- Canon: tools/colonization_lab/canon/ship_naming.json (11 doctrines, 53 models) + ship_naming.py --write/--check
  (mirrors: passports name / naming fields / model_code <prefix><n> / group codes / replacement names;
  ship_designs.json name + designation = ASCII export_index, the runtime installation-code root). IDs unchanged.
  Rebuilt: runtime ship_catalog_3126.json (ship_designer: 21 ships, 0 FAIL), Notes/Lore (lore_md, 45 files: names
  only), Notes/html lore; lore_economy.py class labels; naming.json VERIFY item on Russian ship names removed.
  C++ display strings of OCS-4 drop "Lastochka" (the Ural shop nickname) - labels only, not keys.
- Bloc completeness: rule = own model, or a partner hull at availability >= 3/5; raider = free belts only. 15 gap
  fillers generated as variants of existing hulls (build_proposals.js derive()): Pact courier 416型, lifter 421型,
  ice tug 和-T8 移山; Southern C-30 Doot, H-20 Majdoor, frigate F-120 Prahari; Citadel TS-6F, B-12L, B-20G "Hulken";
  Cognium F-900 Atlas; Sol frigate FF-3000 Gagarin; Free Belts T-5000R hive, OCS-4R bucket, Cobrett-I R stinger;
  Lalande corvette PC-30AK Akritas. Balance: 0 FAIL (warnings inherited: oversize reactors, fins). Reference now
  53 models (21 canon + 32 proposals), 53 unique codes, builder idempotent, render_check 0 problems.
- Manufacturers page (agent): manufacturers.html / .js / .css + tools/build_manufacturers.py -> manufacturers_data.js
  (19 manufacturers, 14 polities, 399 hubs, 22 module types). Open in data: Hyperion's plant type "orbital" has no
  orbital_hub site in Cognium; 52 Sol surface ports have no hub_modules entry; plants are matched by site type +
  polity only (several makers share a polity's hubs).
- Checked: headless Chrome (catalog search by old name «Пилигрим» finds L-900 «Эос»; ship view shows official /
  native / export / crew names and codes 和51A1...; manufacturers list and Harmony detail render, no JS errors).
- EliteGame builds; gates 209: 203 pass / 6 known red / 0 new.

### Manufacturer plants + data gaps + rescue zones (SOS) + difficulty layer "capsule survival" 2026-11-08 - BUILT, 1 LIVE HUD CHECK, NOT AUTHOR-ACCEPTED
- Author: "площадки должны принадлежать производителям - подумай, что оставить на орбите, что спустить на планеты
  или заныкать в поясе"; "в жилых зонах есть спасательные службы; предупреждение в терминале HUD, при расчёте маршрута
  или старте автопилота: вдали от обитаемых зон SOS могут не услышать; в обитаемой зоне подберут через 10-20 мин";
  "заложи слой - уровень сложности (как в Tin Can), включается / выключается в настройках". Names: not approved yet.
- Plants: 24 named sites in canon/world_sites.json (field manufacturer, placement_reason) + Деймос-Верфь
  (sol_network operator now Лунно-Марсианские верфи; founder stays Марсианские верфи, 2702); industry.json
  manufacturers[].plants. Rule: orbit = big hulls / clean rooms / sections bigger than any lifter (Harmony, Hyperion,
  Cobretty yard, Bharat rings, Mjølner, Agora, Xinzhi, Terra Habitat, OFU yard, Tianhe mass yard, Kāpehu core fab);
  surface = heavy casting, engine / reactor test stands, biology, lifters (OFU Ural works + Kourou, LMY lunar foundry,
  Sol Reactor Works Tranquillitatis, Cobretty K3 works, Tianhe danwei No. 1, Firmament armour, Eurohab, Uhuru);
  belt = mining kit and hollowed asteroids, the hidden state core fab (Kern Deep Dock, Stone Docks, Нулевая точка).
  Every manufacturer resolves (Hyperion's gap closed).
- Gaps filled: the 52 Earth megacity spaceports (spaceport flag) are now hubs (one standard port per 100 M people,
  level A > 50 M) -> 614 hubs (was 538); mfr.mizan (retired Arabic Lalande branch) -> mfr.kapehu_systems in
  industry.json and ship_designs.json (parts already used it). world_sites / economy_live / ship_designer / lore
  rebuilt; world_sites check OK, naming 29/29, lore check OK. Manufacturers page uses the canon plants + reasons.
- Rescue zones: navigation/rules/RescueRules.h - a zone = 30 light-seconds around an inhabited hub (canon
  comms.coverage_ls station); inside: pickup 10-20 min; outside: SOS may not be heard; a system with no inhabited
  hub: no service. SpaceStateRescue.cpp; HUD terminal line (stable text per zone); route plan summary counts the
  points outside the zones; route start / body autopilot start / jump calculation warn through the pilot notice.
  Live: Earth orbit -> "SOS: зона спасения «Высокий» - подберут через 10-20 мин". Outside / no-service: not flown.
- Difficulty layer: ClientModeState::capsuleSurvivalEnabled, persisted (difficulty_capsule_survival), webui command
  set_capsule_survival|on/off, main menu settings dialog (5 locales); the destruction HUD line notes the mode when the
  pilot is alive. The Tin Can-style gameplay itself is NOT built (later slice).
- EliteGame builds; gates 209: 203 pass / 6 known red / 0 new.

### Design 2026-11-08: damage + maintenance + Tin Can capsule layer - PROPOSAL, NO CODE
- Author: start on a ship graveyard, buy a Cobra there, it breaks often; the player lives in the cockpit capsule;
  a nanobot bay restores parts (slow, not free) and a repair drone crawls the hull; reliability grows with repairs
  until pirates shred the ship; an Army-style technical manual (fault isolation: typical faults, fault codes,
  corrective actions) per unit and per ship; an isolated, switchable layer. Then: "слой повреждений ещё толком не
  прорабатывался - может, получится совместить Tin Can и интересный слой повреждений".
- docs/MAINTENANCE_LAYER_DESIGN.md: one failure-mode catalog for wear AND battle damage (cause -> modes, energy ->
  stage, hit volume -> unit / part), effects through the existing EquipmentSample fields, BITE fault codes
  (CAT-NN-stage), repair levels operator / drone / nanobay / yard (Army -10/-20/-30/-40, LRU/SRU), TM generated
  from data (troubleshooting Malfunction -> Test -> Corrective Action, MAC, parts, PMCS) into the ship reference;
  draft failure modes for all 34 unit categories. Two layers: the damage model (base, always on) and
  reliability / junkyard / capsule survival (difficulty, switchable). Open author questions in section 10.

### Sol industry re-based on corporations + ship graveyards + damage/Tin Can design pass 2 - 2026-11-08 - DATA BUILT, NAMES NOT ACCEPTED
- Author: names not accepted; "ты всю корабельную промышленность Сола отдал России ... Сол - упор на корпорации, в
  том числе интернациональные; разработки - США (или во что оно превратится), промышленность - Китай, Россия -
  поставщик сырья и проблем; проверь по текущим данным". Canon check (SOL_NETWORK.md): Sol's operators are the
  Pacific Orbital Transport Co. (Tokyo), the Atlantic Port Co. (Canaveral), Wenchang Heavy (Chinese industrial,
  "огромные краны, круглосуточно"), the Equatorial Fuel Co. (Kourou), metallurgy / gas trusts; Russia appears only
  as the Baikonur orbit-plane heritage. The Russian layer was mine (Ural engine works, Cyrillic prefixes, shop slang).
- Changed: OFU = international freight union (Kourou fuel co. + Pacific + Atlantic; "Ural" = the founding titanium
  supplier); its engine works moved to Wenchang. Luna-Mars Yards = international corporation, controlling stake
  Wenchang Heavy, Atlantic design bureaus; new plant "тяжёлые цеха Вэньчана". Sol Reactor Works = Atlantic (US
  heir) R&D: new Canaveral research centre + the lunar series plant. Code prefixes OF / LM (were У / ЛМ); Sol crew
  nicknames Chinese / English shop slang (燕子, 雨燕, the Dozen, 大油桶, the Fixer, the Broom, the Flatiron).
- Graveyards: site type ship_graveyard (canon world_sites site_types + hub_modules type rule, dock split,
  salvage_yard module), 16 graveyards in 7 blocs, kinds recycling / wild / drifting (graveyard_kind), level C:
  Sol - Hermes breakers (Mercury smelters), Vesta trust scrap, the graveyard orbit above GEO, the wild yard by
  Тихая Гавань; Cognium - Tau Ceti breakers, Kuiper drift; Pact - Harmony recycling shop, Tianhe belt drift;
  Southern - Bharat breakers, Maland wild; Citadel - Ulven military disposal; Free Belts - the Great Strife wild
  yard, the GJ 581 rust belt; Lalande - Agora breakers market, Lalande drift; Kern - insurance breakers.
  632 hubs. world_sites / economy_live checks OK, lore OK, reference + manufacturers rebuilt.
- Design (docs/MAINTENANCE_LAYER_DESIGN.md 9a-9c): Tin Can = a mini-game (main game keeps percentages); the
  failure catalog is a cause graph (upstream / downstream / observable / masks): BITE shows the first failed
  signal, a deeper cause burns the replaced unit again; masking, intermittent faults, false sensor alarms,
  cascades. Nanites self-replicate from a feedstock (powder / scrap / regolith) to be replenished; the layer
  switches at any time; the start offers several wrecks with different control laws.
- Gates 209: 203 pass / 6 known red / 0 new.

### Fault catalog by equipment (slice 1: Cobrett-I) + sketch review workflow - 2026-11-09 - BUILT (DATA + REFERENCE), NO GAME CODE
- Author: "каталог по оборудованию, поскольку оно установлено во многих местах, а потом по совокупности
  оборудования - для корабля"; "проэкспектируй эскиз и покажи, как будем работать дальше".
- tools/maintenance/build_fault_catalog.py -> src/assets/data/maintenance/fault_catalog.json + reference
  fault_data.js: 19 equipment classes (all Cobrett-I N1 / A3 units), 50 failure modes (SRU, causes, BITE signals,
  per-stage percent effects for the main game, symptoms, upstream / downstream / masks through 8 ship buses,
  isolation steps with branches, repair by operator / drone / nanobay / yard, PMCS); ship graphs resolved from the
  BOM (N1 64 edges, A3 61). Reference: "Техническое руководство" on module pages, "Цепочки неисправностей" on ship
  pages. Gate tests/maintenance/check_fault_catalog.py (deterministic outputs, every isolation path concludes).
- Sketch workflow: the author's first sketch (CR-1 Vector, stored in Edge IndexedDB, part kb.drive.cr1-vector)
  reviewed: the sketch follows the passport envelope 5 x 5 x 6.5 m, but the in-game nacelle mesh
  (assets/models/ships/cobretty-mk1-int/LOD0/ship_engine_L.obj, model in metres) is 8.7 x 2.35 x 2.35 m - the
  passport geometry is wrong (OPEN: correct the passport to the mesh, author to confirm); no +-75 deg vectoring;
  chemical bell instead of a magnetic nozzle; camera cluster; floor stand. Review + prompt v2 written to
  Notes/html/ship_reference/workshop/workshop.json (reviews map, versioned prompts) + the sketch file; the page
  shows reviews read-only in the sketch window; merge fixed so a note-less record never erases the author's note.
- Gates 210 (new: tests/maintenance/check_fault_catalog.py): 204 pass / 6 known red / 0 new.

### Ship reference local server - 2026-11-09 - BUILT, CHECKED (headless Chrome over HTTP)
- Author: "что это за папка воркшоп? возможно стоит сделать сервер для подключения этого справочника и полноценной
  работы с ним?" - tools/ship_reference_server.py (stdlib, 127.0.0.1, default port 8790 - 8765 is taken by
  another local service - next free port if busy) serves Notes/html/ship_reference and keeps the workshop in the
  repository (workshop.json merged on every save; images via /api/file); Notes/html/ship_reference/start_server.cmd.
  store.js detects the server (GET /api/ping) and stores nothing in the browser; refreshes from disk on focus.
- Checked: page in server mode, a new sketch + note written to disk, an on-disk edit (review) survived a page save
  and became visible, the test sketch removed; workshop.json restored to the pre-test state.

### Equipment-first + reference designations (proposal) - 2026-11-09
- Author: in-game models are NOT canon for now; reverse order: design the equipment first, then assemble ships.
  The CR-1 sketch review corrected (no longer asks to match the old Cobra nacelle; the equipment passport 5 x 5 x
  6.5 m is the basis), prompt v3 in workshop.json. The earlier OPEN item "correct the CR-1 passport to the mesh"
  is withdrawn.
- Author: designations by LAYOUT, not function (46A1A2PT1 style); the functional schema groups chains
  (pad - engine - reactor - engine - pad, x2). docs/REFERENCE_DESIGNATIONS.md: this is the ASME Y14.44 / IEEE 200 /
  IEEE 315 reference-designation system (unit number, A separable assembly, U inseparable, class letters + number);
  proposed class letters for our equipment, the two schemas (layout tree / functional chains), a Cobrett-I A3
  example. Open: author to confirm the letters and whether the pad belongs to the reactor or the engine.

### Layout designations + per-maker standards + engine pads - 2026-11-09 - BUILT (DATA + REFERENCE)
- Author: class letters accepted; the unit number is the passport model code; each engine has its own cooling pad
  (heat goes to the common loop only when the pad is damaged); "американский стандарт не для всех - у китайцев
  точно будет другим".
- tools/maintenance/build_ship_layout.py -> src/assets/data/ships/ship_layouts_3126.json + reference
  layout_data.js: all 53 ships laid out from their equipment (propulsion bays = reactor + drive groups, then cockpit
  capsule, systems, cooling, jump, fuel, RCS, payload, weapons, docking, hull); designations in the maker's
  standard (ship_naming.json designation_standard): ASME Y14.44 9 ships, IEC 81346 25, GB/T 5094 12, NORSOK 7,
  Stone Docks = donor; functional chains (pad - engine - reactor - engine - pad xN; the 7 bus chains).
  Cobrett-I A3: KB44A1 / KB44A2 bays (A1 ER-5, A2 / A3 CR-1 with PT1 each), chain x2.
- Fault catalog: DRV-09 engine pad damage; the engine heat exchanger is the backup path; 51 modes; SRU class
  letters (sru_class) per equipment class. Passport functional groups renamed <model_code>F<k> (A = layout unit).
- Reference: ship page = "Компоновка" (compartments, unit cards with designations and part designators) +
  "Функциональные цепочки". Gate tests/maintenance/check_ship_layout.py. Gates 211: 205 / 6 known / 0 new.

### Fault catalog: all equipment (slice 2) - 2026-11-09 - BUILT (DATA + REFERENCE)
- Author: "каталог для оборудования остальных производителей - так и делаем". Classes generalised by technology
  (drive.plasma, reactor.fusion, apu.fusion, radiator.panel, coolant.loop, jump_core, rcs.thruster, computer,
  radar, comm, life_support, escape_capsule, weapon.pd) with per-model notes; 19 new classes (coilgun, VLS, mass
  driver, armour, cabins, anabiosis, lifeboats, bays, train modules, bulk hold, cryo tank, workshop, medbay, tow
  rig, mining rig, landing gear, heat shield, ECM, sensor mast). 38 classes, 97 modes, buses extended; fault graphs
  for all 53 reference ships; layouts: all 1 578 units classed, 5 141 part designations. The gate now requires
  every ship's equipment to be covered.
- Gates 211: 205 pass / 6 known red / 0 new.

### Jump presentation rework (author 2026-11-09) - IMPLEMENTED, live DEV checked, NOT yet accepted by the author
- Removed the cartoon ripple (flash + dotted rings) and the charge-up panel. Now (SpaceStateJump.cpp
  updateJumpPresentation, render/JumpScreenEffect): the HUD terminal prints the procedure (target, ly, charge-up,
  cost, cancel hint) and a live font progress bar `ЗАРЯД ЯДРА [####----] 45% m:ss / m:ss` (DockingTerminalSnapshot
  `jumpLines` printed once each + `live` line rewritten in place); the cockpit view shakes along screen x/y (no
  rotation), amplitude growing with the charge; from 60 % a fish-eye swell (corners fixed, centre magnified); at the
  jump (jumpSerial) a sharp 0.14 s fling - corners stay, centre drawn away like rubber; the old sky goes dark, 220
  brightest stars send rays from their old direction to the new one that collapse into the new points (0.2..1.8 s);
  the new sky fades up; the lens springs back with ~2.5 damped swings (1.9..2.8 s). Author corrections applied the
  same day: no whole-view shrink, half the lens strength, sharper fling / return, no colour fringes ("the cockpit must
  feel whole"). Whole-frame capture (glCopyTexSubImage2D of FB 0 after the cockpit + instruments) + distort shader;
  neutral params = no pass. Rays drawn under the cockpit frame.
- Hum: AudioEngine::setJumpHum(level) - semantic level only (sounds are off), rises with the charge.
- Terminal: font ~21 px at 1080 (was ~15), column 30 % wide, word wrap instead of cutting, top below the flight
  banner, brighter colours.
- Exit error (server, PROPOSAL model of beacons_3126 jump_error_model): sigma = d * 1e-9 / lock (lock proxy: system
  has world sites; both x200, destination x6, origin x12, none x1), deterministic 3D gaussian draw per ship + jump;
  circular orbit at the radius reached (clamped above floor, <= 0.3 Hill). Replicated jumpExitSerial / body /
  deviation / sigma / altitude. Exit report in the terminal: system, orbit + altitude, coordinates (AU from the
  star), deviation vs the 3 sigma box; outside it a warning "корабль снесло".
- DEV: ELITE_DEV_JUMP_FX_CAPTURE=<dir> - frames at fixed times after the jump. Live runs D:/tmp/elite_jfx1..4
  (Высокий -> Fomalhaut, 20 s charge): 220 rays, exit 25 752 km off the point (sigma 19 809 km, box 59 427 km);
  a false "ПРЫЖОК ПРЕРВАН" (2 -> 0 between snapshots) found and fixed.
- Gates 211: 205 pass / 6 known red / 0 new.
- Author review 2 (same day): "still bends to the centre too much - a picture, not a cockpit; the stars make it look like
  a jump BACKWARD". Lens cut ~2.5x again (swell 0.06, fling -0.06, swings 0.05); rays now follow a curve that first
  leaves the screen centre (forward rush) and then bends into the new point. Live D:/tmp/elite_jfx5: outward start
  confirmed; when the target lies behind the nose the curves' second half still comes back toward the centre.
- Author review 3: "now hardly seen; drop the 2-3 damped swings; the stars move the wrong way - as if the ship flies
  AWAY from them; flying forward they must spread to the edges". Lens set between the two tries (swell 0.11, fling
  -0.10); the return is one 0.22 s snap, no swings; the old->new star link dropped (with the target behind the nose
  it can only converge): two radial outward phases - the old sky's stars accelerate to the edges and leave (0..0.5),
  the new sky's stars burst out of the centre and brake into place (0.4..1). Live D:/tmp/elite_jfx6: both phases
  outward.
- Next: author's look review in a live flight; real audio backend for the hum; beacon runtime to replace the lock proxy.

### Ship reference: identical compartments as one stacked card (author 2026-11-09) - DONE
- app.js renderShipLayout groups compartments with the same content (key + parts / qty / class letters): one card
  with all designators (`=OF38+1, =OF38+2, =OF38+3 · двигательный отсек ×3`), frames drawn behind it (one for x2, two
  for x3+). 37 of 53 ships have such stacks. render_check: 53 ships, 0 problems; headless Chrome checked (SL-12).
- Functional chains (author: "in the functional chains do the same"): the propulsion group with N identical
  instances is drawn as the same stacked card (frames behind, xN, the shown group's drive and the others listed);
  .stacked / .stacked3 are now generic classes. render_check 53 / 0; headless Chrome checked (SL-12, x3).

### Ship reference: pads in the propulsion chain; reviews of the author's CR-1 / ER-5 sketches (author 2026-11-09) - DONE (reviews = PROPOSAL)
- Author: "в функциональных схемах вижу реактор и двигатели, пятаков не вижу". The propulsion chain card now spans the full
  width; each drive pad is its own item (round icon, designation, Ø from the drive passport), the reactor pad is drawn over
  the reactor (square icon, designation, side from cooling_pad_side_m). render_check 53 / 0; headless Chrome (Cobrett-I A3).
- 4 new author sketches reviewed into workshop.json reviews (Russian); prompts: CR-1 v4, ER-5 v1.
- Root cause found: my earlier CR-1 prompt v3 / review (plasma drive, magnetic nozzle, injectors, trunnion) contradicted the
  code - every drive preset is PropulsionPrinciple::FieldCoupling with field steering (no servo) and a solid 5 m pad
  (aperture 0). Corrected in the reviews and prompt v4.
- (resolved below) fault catalog class drive.plasma (build_fault_catalog.py: injector / magnetic-nozzle coil / plasma
  chamber / gimbal SRUs, DRV-01 propellant clogging) contradicts FieldCoupling; proposal: rename to drive.field with SRUs
  pad sectors, field coil ring, busbar coupling, backup HX, sensors; drop propellant modes.

### Field concept applied to equipment descriptions and faults (author 2026-11-09) - DONE (source-audited, gates)
- Author: "мы приняли полевую концепцию. поэтому давай подгоним все описание и проблемы в работе под это."
- Fault catalog (build_fault_catalog.py): drive.plasma -> drive.field (SRUs: field control unit, field coil ring, pad
  sectors, backup HX, busbar in the flange, sensors). DRV-01 field phasing drift, DRV-02 coil segment degradation,
  DRV-03 inter-turn breakdown in the coil ring, DRV-05 vector channel failure (field steering, matches gimbalFailure
  0.90), DRV-09 pad damage or wear-out (replace only: code ServicePolicy::ReplaceOnly, 8 000 / 12 000 h).
  Injector / chamber / mechanical gimbal modes removed. RCS: power switch (RCS-01), field coil (RCS-02), new RCS-03
  nozzle emitter damage (ManoeuvreEnginePreset derating). Signals: chamber_press / valve_pos removed, field_stab added.
- Argon RCS tank (kb.rcs.argon-tank) removed from canon: ship_designs.json, ship_parts_unified.json (101 parts; the
  std tanks' foreign analog -> null), ship_passports_3126.json (Cobrett-I N1 dry estimate 266 -> 264.5 t), data.js;
  class tank.argon and bus rcs_propellant removed (7 buses). Rebuilt: ship_designer (21 ships, 0 FAIL, 39 WARN, --check
  OK), runtime ship_catalog_3126.json, out/ships_3126, lore_md --write (check OK), lore_html, build_proposals (Stinger
  re-derived without the tank), layouts (0 problems), fault catalog (37 classes / 96 modes, 0 problems).
- Texts: docs/MAINTENANCE_LAYER_DESIGN.md (examples, drive / RCS rows, game-cycle example, new section 13);
  reference category descriptions and prompt hints (drive / rcs / tank, "field vector deflection" label); the old
  CR-1 sketch review marked outdated.
- Checks: check_fault_catalog 0 problems, check_ship_layout 0, render_check 53 ships / 103 parts / 0;
  gates 211: 205 pass / 6 known red / 0 new. No C++ change (no runtime code referenced the removed codes or argon).


### Equipment 3D models from passports: Cobretty CR-1 Vector v1 (author 2026-11-09) - PROPOSAL (awaiting author review)
- Author: no textures; a very detailed model seen from 20-30 m, simplified LODs further. 6 GB GPU rules out local
  image-to-3D with textures; chosen path: procedural geometry from the passport (no GPU), Blender 5.0 only for previews.
- Tool: tools/models/build_equipment_models.py (+ --check, idempotent) -> src/assets/models/equipment/cobretty_cr1_vector/
  LOD0..2/<part>.obj + .mtl (SOURCE_LAYOUT.md: one OBJ per part, shared material ids eq_*). Parts: cr1_flange,
  cr1_housing, cr1_coil_ring, engine_hot_emitter_annulus (the passport meshPartId), cr1_service_module, cr1_mounts.
  Frame: unit axis OBJ +Z, Y up, flange at z = 0, pad at z = 6.5; envelope 5 x 5 x 6.5 m, pad 5.0 m the widest.
  Triangles: LOD0 47 020, LOD1 4 872, LOD2 1 096.
- Preview: tools/models/render_preview.py (blender -b ... -- <asset dir> <LOD> <png> [pad|flange|side], 1.8 m figure);
  renders added to the CR-1 workshop sketches with a review card.
- Not done yet: import into the model asset editor (semantic parts, sockets, LOD distances), runtime package; ER-5.
- Author 2026-11-09: CR-1 v1 accepted visually ("сделано шикарно"); parts stay separate (game binds hot pad heat,
  hit regions, module hiding and damage variants by part name; the editor compiles them into one .elmodel).

### Equipment models: Cobretty ER-5 propulsion reactor v1 + editor "Equipment" catalog group (2026-11-09) - PROPOSAL
- Sources moved to the editor SOURCE root: assets/models/equipment/<unit>/ (src/assets/models holds docs only; the editor
  settings point sourceAssetsRoot at assets/models). .gitignore unignores assets/models/equipment/; README there.
- ER-5 v1 (generator er5_parts): flat torus dia 3.9 m x 2 m (axis OBJ +Y), flanges at z = -/+2.5 (flange to flange 5 m)
  mating the CR-1 flange exactly (dia 4.5 m, 24 bolts, spigot groove, 3 busbar sockets, data socket); pad PT1 11 x 11 x
  0.25 m, 8 triangular sections, at y = 2.3 on a heat-pipe column. Parts = reactor.fusion SRUs: reactor_cooling_pad,
  er5_confinement, er5_fuel_feed, er5_inverter, er5_plasma_ctl, er5_plasma_sensors + er5_vessel, er5_heat_pipe_column,
  er5_emergency_coolant (the only copper), er5_flange_fore/aft, er5_mounts. Triangles LOD0 61 708, LOD1 10 112, LOD2 1 648
  (empty parts are not written). Open deviation for the author: flange rims 4.5 m exceed the 4 x 4 m passport envelope by
  0.25 m per side (needed to mate CR-1).
- Generator: lathe(soft=deg) shared profile normals (CR-1 output unchanged, triangle counts identical); helpers ylathe,
  rrect, inset, prism, placed. render_preview.py: views top/under, optional hidden parts.
- Editor: ModelAssetEditorSession lists assets/models/equipment/* as group "equipment" (ObjectType::None, Folder SOURCE,
  id equipment_<folder>); UI group + localization key model_editor.catalog.group.equipment. EliteAssetEditor build PASS.
  Not verified in the running editor (GUI wizard SOURCE -> BUILD is the author's step: semantic parts, sockets for the
  flanges/lugs/pad, LOD distances, BUILD to src/assets/compiled/models/equipment_<folder>/).
- Renders + review card in the ER-5 workshop sketches.
- Author 2026-11-09 (same day): pad size questioned (sketch showed pad ~ reactor size) -> pad stays 11 m (passport
  padSideM drives the cooling area; the sketch was wrong). Then: reactor gets the drive's diameter so the bay is one
  cylinder. Done as ER-5 v2: canon kb.reactor.er5 geometry envelope 4x4x5 -> 5x5x5 m, service clearance 5.75 m (+ basis
  note), data.js same; ship_designer 21 ships 0 FAIL (runtime catalog unchanged), lore unchanged, proposals rebuilt.
  Model: torus now COAXIAL with the bay (axis OBJ Z, r 0.95-2.2, length 3.5 m + flanges on the end faces, mating faces
  z = -/+2.5), 8 poloidal confinement hoops, inverter +X, feed -X, plasma control -Y, column + pad on top (pad y 3.0).
  The flange rims are now inside the envelope (deviation closed). Triangles LOD0 69 916, LOD1 11 908, LOD2 1 980.
  Bay assembly render (CR-1 + ER-5 + CR-1) + ER-5 prompt v2 in the workshop.
- Author 2026-11-09: too many triangles -> lightened (commit d0caa342): visibility floor MIN_BOLT 6 cm / MIN_FEATURE 2.5 cm
  (1 px at 20-30 m ~ 3 cm), 32/16/12 segments, cheap hex heads. CR-1 LOD0 9 882 / LOD1 2 168 / LOD2 840; ER-5 11 596 /
  3 124 / 684 triangles. An open bay (CR-1 + ER-5 + CR-1) at LOD0 ~31k triangles.
- Author 2026-11-09 idea: hit volumes drawn with the model; gameplay stores hit volumes, not the full mesh; the hull in
  compartments, the interior rendered only for a breached compartment. Step 1 done: the generator writes
  <unit>/hit_volumes.json (CR-1 11, ER-5 31 volumes; boxes + cylinders per part, CollisionVolume fields; T*Rx*Ry*Rz),
  render_preview.py mode `hits`, renders in the workshop. Open: engine CollisionShape has no Cylinder (Box / Sphere /
  Capsule / ConvexHull) - additive enum value + binary/editor support proposed; editor import of hit_volumes.json into
  COLL; ship hull compartments + interior-on-breach presentation (station interiors/portals exist in
  ModelAssetPresentation; ships still use the legacy assembly path).
- Author 2026-11-09 "да, делай" (steps 1 + 2) - DONE, compiled + unit-tested, NOT yet exercised in the running editor:
  1. CollisionShape::Cylinder = 4 (src/model_asset/ModelAsset.h; flat ends, radius + halfHeight along local +Y). The
     binary stores the shape as a byte without range checks, so no format bump. Editor: name "cylinder", mass/inertia
     (solid cylinder), PHYSICS validation, viewport primitive, inspector option, localization model_editor.collision.cylinder.
     tests/model_asset: manifest roundtrip of a cylinder volume (model_asset_tests PASS). The runtime does not read
     .elmodel collision volumes yet (hits still use the legacy assembly), so nothing changes in flight.
  2. PHYSICS command import_source_hit_volumes (button "SOURCE HITS" / "ХИТЫ ИЗ SOURCE"): reads <SOURCE asset>/hit_volumes.json,
     binds each volume to the semantic part with the same id/moduleId, replaces earlier hit.src.* volumes; parts that
     are missing or whose node chain is no longer at the zero SOURCE transform are skipped and named in the status.
     SOURCE stays scaffold-only (its contract: collision authoring belongs to PHYSICS).
  Stale test fixed: tests/model_asset/CMakeLists.txt lacked chunks/PresentationChunks.cpp (link error readPresentationV4).
  Gates 205 PASS / 6 known red / 0 new. Next (author): open an Equipment asset, SOURCE -> PHYSICS -> SOURCE HITS, check
  the volumes; step 3 (ship hull compartments, interior only for a breached compartment) is a separate large stage.

### Reactor families per maker + passport geometry audit (author 2026-11-09) - PROPOSAL (models), CR-1 / ER-5 APPROVED
- Author: CR-1 and ER-5 models approved; reactors must differ per maker within one base concept; check that every
  passport has geometric dimensions; consider real-world device shapes.
- Audit: all 101 canon parts and all 103 reference parts have geometry.envelope_m and service_clearance_m (no gaps).
- Author choices (2026-11-09): Sol Reactor Works = linear FRC collider (ref. Helion Polaris); Tianhe Heavy = heavy
  tokamak with exposed D-coils on a flat-plate polygonal cryostat (ref. EAST/CFETR); Cobretty = the approved coaxial drum.
- Generator: srw_parts (formation sections, compression waist under dense coils, 4 capacitor banks), th_parts (12-gon
  cryostat, 12 D-coils, 12-gon flanges), shared heat_column / cooling_pad builders (ER-5 output unchanged), family
  scaling (ER-1, SR-2.6, TR-2.6V = big unit scaled to the passport, own pad), per-maker materials, hit volumes for all.
  LOD0 triangles: SR-4.8 / SR-2.6 13.4k, TR-10 / TR-2.6V 7.6k, ER-1 11.6k.
- Canon: SR-4.8 envelope 3.83x3.83x4.79 -> 3.4x3.4x6.1 m (clearance 3.91x3.91x7.02); SR-2.6 (build_proposals.js)
  3.215x3.215x4.02 -> 2.85x2.85x5.11 m; same volume and mass. Rebuilt ship_designer (21 ships, 0 FAIL), lore, proposals.
- Open: reactors pair with other makers' drives (OFU with Sol, Kern with ER-1, TZ-11 and TZ-28 with TR-10): flanges are
  each maker's standard, mating by a bay adapter ring - not modelled yet. Service reactors (APU) not modelled yet.
- Renders + review cards in the workshop for kb.reactor.er1, srw.reactor.sr48/sr26, th.reactor.tr10/tr26v.
- Author 2026-11-09: flanges = each maker's standard, mating to other makers' drives through a bay adapter ring
  (ACCEPTED); a reactor may stick out from under the hull where it does not fit its seat (accepted as a look).
  Models accepted -> linked in the ship catalog: model_asset = "equipment/<folder>" (SOURCE folder relative to
  assets/models) on canon ship_designs.json products and ship_parts_unified.json parts (CR-1, ER-5, ER-1, SR-4.8,
  TR-10, TR-2.6V), SR-2.6 in build_proposals.js; runtime ship_catalog_3126.json products carry it (params);
  reference: modelReady for the 7 parts + a "3D-модель" row on the part card. render_check 53 ships / 103 parts, 0 problems.

### Equipment models for every ship unit, LOD shift, design schools (author 2026-11-10) - IN PROGRESS
- Author: finish all equipment blocks before compartments; design per maker spirit and colony history; similar modules
  must differ (even in envelope) to make foreign replacement harder; LOD0 becomes a close-inspection level (harnesses,
  pipes, fine detail) and the 20-30 m model becomes LOD1 (shift by one), applied to existing units too; add missile
  racks, a recon drone (camera + antenna + power + thrusters) and a repair drone (spider / mantis, flexible body for
  animations, plasma cutter, fine manipulation). Cobra model review requested (non-canon, opinion only).
- Done (commit c94bc697): generator split into tools/models/eqlib.py (kernel, LOD policy close() / seg_for / small_for /
  min_bolt, register_materials, tube / harness / pipe_run / bezier / sag / cyl_path) + tools/models/units/<maker>.py +
  thin driver (4 LODs, orphan cleanup, --only). Former LOD0..2 = LOD1..3 byte-identical (459 files verified); close
  LOD0 extras for the 7 existing units (17-28k triangles).
- Done (commit e71f86ae): tools/models/DESIGN_SCHOOLS.md (18 makers: form language, palette, interface standard,
  close-up flavour, from colony lore), tools/models/UNIT_AUTHORING.md (unit contract, frames, budgets, checks),
  tools/models/briefs/<maker>.json (96 remaining parts with passports and fault-class SRUs).
- Running: 8 parallel builders (maker groups + new equipment: missile rack / recon drone / repair drone with a rig
  proposal in tools/models/briefs/new_parts_proposal.json - canon not edited until the author accepts).
- Result (2026-11-10): 106 equipment models in 18 maker modules (tools/models/units/*.py), every one with 4 LODs,
  serviceable units as parts, hit_volumes.json; all 103 reference parts covered + 3 new items (Sol PB-8 missile rack
  sol.wpn.pb8, Hyperion RD-1 recon drone hyp.drone.rd1, Agora RM-2 «Мантида» repair drone agora.drone.rm2 with a
  37-joint rig) proposed in tools/models/briefs/new_parts_proposal.json (NOT in canon). build --check passes; budgets
  met except SR-4.8 / SR-2.6 LOD2 4.5k (target ~4k). Each maker uses its own interface standard (DESIGN_SCHOOLS.md).
- Catalog: model_asset on all 101 unified parts, 103 reference parts and all 120 runtime products (22 legacy products
  inherit the model of their legacy_resolution canonical part: model_part + model_units, e.g. TR-30V = 3 x TR-10).
  ship_designer 21 ships 0 FAIL; reference render_check 0 problems; gates 205 PASS / 6 known red / 0 new.
- Open for the author: (1) hull-skin flat units whose passport puts thickness on Z (std.dock.collar, bastion.armour.heavy,
  std.armour.ceramic) were built +Y-outward with axes swapped - passports should read [W, thickness, L];
  (2) service reactors have no cooling_pad_side_m - builders used the envelope top face as the pad;
  (3) std.bay.boat cannot hold its boat (craft 9 x 4 x 3 m vs bay 4.87 x 2.43 x 6.08 m); (4) ofu.lift.gear is a set of
  4 with one envelope, stowed leg exceeds it; (5) sol.rad.panel50 radiating area ~46 m2 vs 50; (6) harmony
  cargo4800: 240 C20 do not fit the envelope (model carries 85); (7) accept the 3 new items into canon.

### Ship architecture doctrine + Cobrett-I layout sketch v1 (author 2026-11-10) - PROPOSAL
- Author: passports are NOT canon until the ship models are approved and disputes settled. Decisions recorded in
  docs/SHIP_ARCHITECTURE_DOCTRINE.md: Cobretty escape = the cockpit itself with a motor (big window, part of the hull
  design; other makers may use capsules); no machine guns (friendly debris) - lasers, missiles (burn internals, do not
  rip the hull), railgun on very large ships; Cobra = 2 forward lasers + 4 missiles in 2 wing racks, military = 4 heavier
  lasers; drone bays (repair + recon) on ships, docks for auxiliaries / fighters on carriers; fleet doctrine from
  military need (TODO); compartments under removable radiator cover plates (zero-thickness polygons, main + seam hit
  volumes, seams free the plate), equipment mount points (hit volumes; destroyed mounts free the module), inspection
  shafts / passages with handrails and ladders, folding landing skids (animated), container unloading hatches.
- Author's Cobra sources: D:/__elite/_data/objects/ships (Cobretti-mk1-PROTOTYPE-LEGACY-V2.blend = the author's layout
  topology: nose cockpit module, central power unit, 6 containers in two L groups, engines outboard, missile racks in the
  wings, lasers beside the cockpit; earlier service-volume studies in _codex_work_*).
- Tool tools/ships/layout_sketch.py: rough hull + REAL equipment models (LOD2) placed; output
  assets/models/layout_sketches/<ship> (untracked); renders in the reference as workshop sketches "hull:<ship id>",
  shown on the ship card (hullPic / hullGallery in app.js). Cobrett-I A3 v1: two side bays (fore CR-1 - ER-5 - aft CR-1)
  at x = +-10.75 (dia 5 m -> hull 5 m thick), 6 C20 in two L groups on the cargo deck with ventral hatches, spine with
  jump core / tanks / APU / coolant under an inspection shaft with cross passages and ladders, escape-cockpit proxy in
  the nose, 2 forward lasers (proxy), 2 wing missile racks of 2 (proxy), stern drone bay (RM-2 + RD-1), 5 skids,
  20 dorsal radiator plates, ER-5 pads as dorsal skin. Overall ~35 x 29 x 5 m vs passport 26 x 22.2 x 5.
- Next: author review of the sketch; then escape-cockpit model, forward laser and wing missile rack items, skid rig,
  plate seams / mount-point hit volumes; fleet doctrine; sketches of the other ships per maker principle.

### Cobra drive bay 3.5 m (conical pads), jump concept, kamikaze drones, maker-language names (author 2026-11-10)
- Author: silhouette over size -> the Cobretty bay shrinks to dia 3.5 m with a recalculation of overheating / failures;
  author's idea: conical engine pads. Runtime (PROPOSAL, compiled, NOT flight-verified): EngineThermalEmitterDescriptor
  .coneHeightM + physics::thermal::conicalEmitterAreaM2 (lateral cone / frustum area, flat when height <= 0) used at the
  4 emitter-area sites of EngineUnit.h; CR-1 preset (and N1, which inherits it): pad 3.5 m base + 1.3 m cone
  (11.99 m2 x 6 MW/m2 = 71.9 MW), efficiency 0.9855 -> 0.991 (waste 43.65 MW): margin x1.65 (was x1.68), full thrust
  down to ~61 % pad integrity (was 60 %), half pad -> 82 % sustained thrust. ER-5 unchanged (own 11 m pad, x1.31).
  New test tests/architecture_contracts/CobraCompactDriveThermalTests.cpp (cobra_compact_drive_thermal) PASS; EliteGame
  build PASS. Pre-existing stale reds (same failure on the baseline, not in run_gates): universe_thermal_radiation_tests
  (multiplier 2.5 vs 10), engine_unit_damage_model_tests, universe_material_engine_thermal_tests,
  installed_engine_actuator_tests ("CR-1 type identity missing"), equipment_damage_failure_pipeline_tests - queued.
  Needed for acceptance: a live flight with the Cobra (heat at full thrust, pad damage derating).
- Models: CR-1 / ER-5 / N1 scaled by 0.7 (scaled_unit / scaled_family in units/cobretty.py), CR-1 pad = conical 8-sector
  pad; N1 pad still flat in the model (physics conical) - to redo. Draft passports (not canon): CR-1 / N1 envelope
  3.5 x 3.5 x 5.85 m, emitter 3.5 m + cone 1.3 m, eff 0.991; ER-5 3.5 x 3.5 x 3.5 m; geometry_rule anchors updated.
- Cobrett-I layout sketch v2: hull ~25 x 23 x 3.5 m (passport 26 x 22.2 x 5), bays at x = +-8.4, walkway in the spine,
  wingtip jump-field emitters (proposal), skids flush with the belly when folded (author's choice).
- Jump: per JUMP_MECHANICS.md one field / three modes; the field is the jump core's (a separate unit), CR-1 is the
  marching mode only; PROPOSAL awaiting the author: wingtip field emitters of the core shape the field around the hull.
- Kamikaze drones (author: big military ships, like FPV): Bastion MJ-K3 Klegg drone + MJ-BK24 Bikube 24-cell cassette
  (units/bastion_drones.py), first carrier B-90; all 5 new items (LM-MP8 missile pod, RD-1, RM-2 Mantis, MJ-K3,
  MJ-BK24) are reference parts via build_proposals.js from tools/models/briefs/new_parts_proposal.json (108 parts),
  new reference category "drone". Names by maker language (ship_naming.json); ids / folders without Russian
  transliteration (sol.wpn.mp8 / smy_mp8_pod, bastion.drone.klegg3, bastion.bay.bk24). Open: a naming pass over all
  equipment names / legacy Cyrillic indices (КО-300, КЦ-1000, ...) per the naming canon.

### Author's list after the Cobra bay decision (2026-11-10, "continue down the list") - PROPOSALS, all committed
1. EC-2 escape cockpit (Cobretty): the escape system is the ship's nose module with canopy, seats, separation frame with
   latches / pyro bolts and an escape motor (2 field rings); passport kb.surv.capsule2 -> 6.5 t, 7.6 x 2.8 x 7.0 m.
2. Cobra weapons: FL-2 forward laser, FL-3H heavy (military, 1.25 x), WR-2 wing rack with 2 Aspid-10 missiles
   (missiles / doors separate parts); proposal parts kb.wpn.fl2 / fl3h / wr2.
3. LS-1 folding landing skid: flush belly panel when folded, parallelogram struts + damper, deploy rig (kb.lift.ls1).
   Cobrett-I sketch v3 uses the real cockpit / lasers / racks / skids.
4. Ship hit volumes (tools/ships/layout_sketch.py ship_hits): cover plates = main + 4 seam volumes (all seams destroyed ->
   plate free), equipment hit volumes moved to their installs, 4 mount points per module (all destroyed -> module free),
   ship_structure.json release rules. Cobrett-I: 544 volumes, 15 plates, 33 modules.
5. docs/FLEET_DOCTRINE.md: defence-model principles -> design consequences, weapons doctrine, J keep-out (close combat in
   marching mode), classes with equipment / armament / examples / world counts, carriers = fleet tenders, gaps.
6. tools/ships/auto_sketch.py: rough content-driven sketches for the other 50 ships (functional groups -> compartments
   stern->bow, real models in 3D shelves up to the passport height, hull by maker principle monolithic / modular /
   truss, cover plates, hit volumes); renders in the reference (hull:<ship id>); small craft skipped (own models).
7. Passports: docs/SHIP_PASSPORT_REVISION.md (tools/ships/passport_revision.py) - 19 ships must grow (small hulls
   cannot hold their equipment: B-4 / type 415, PC-30 family, TS-6 family...), 25 have far more volume than equipment
   (cargo / habitable volume or a reserve to declare). Names: tools/colonization_lab/equipment_naming.py ->
   canon/equipment_naming.json (PROPOSAL): maker-language index / official name / native script, display_name_ru =
   index + Russian description, legacy_index kept; applied to the canon parts, the reference (build_proposals.js applies
   it after rebuilding) and the proposals; no Cyrillic index left (--check). Part card shows official / native names.
Open (author): approve or correct the sketches and the proposed frames; jump-field emitters; accept the new parts
(LM-MP8, RD-1, RM-2, MJ-K3, MJ-BK24, EC-2 concept, FL-2 / FL-3H, WR-2, LS-1); Kapehu Maori wording to check.

### Author decisions 2026-11-10: jump-field emitters ACCEPTED, new parts ACCEPTED
- Parts: the 9 proposal items moved from tools/models/briefs/new_parts_proposal.json (now empty, accepted_ids kept) to the
  canon tools/colonization_lab/canon/ship_parts_unified.json with status "ACCEPTED (author 2026-11-10)"; EC-2 escape
  cockpit (kb.surv.capsule2) ACCEPTED. Geometry stays PROPOSAL/model. build_proposals.js overlays ACCEPTED canon parts
  onto data.js (no PROPOSAL label); equipment_naming.py reads the canon first, keeps authored native names, no longer
  double-prefixes the index (earlier "LM-MP8 · (…" / "CR-1 · Cobretty CR-1 …" forms repaired, idempotent).
- Jump-field emitters (concept ACCEPTED): one per core maker, keyed to its cores - ZP-FE1 (Zero Point), He-FE8 (Harmony),
  MJ-FE2 (Bastion), K-FE1 (Kapehu); models tools/models/units/field_emitters.py (4 LODs + hit volumes, LOD0 0.5-4.5k
  tris), passports cat jump_field_emitter with compatible_cores. Count rule: 2 up to ~40 m hull, 4 up to ~150 m, 8 above.
  Cobrett-I sketch: ZP-FE1 pair on the flat wingtips (x +-12.6, pods outboard; span ~26.9 m). auto_sketch places the
  core maker's emitters on every jump-capable ship. Damage rule (proposal, default): one emitter lost -> J ceiling
  halved, no jump until repaired (player role: interdiction by shooting the emitter). Docs: SHIP_ARCHITECTURE_DOCTRINE,
  JUMP_MECHANICS section 1, FLEET_DOCTRINE principles.
- Ship BOMs do NOT yet carry the accepted parts (passports are not canon); the list to add on passport approval is in
  docs/SHIP_PASSPORT_REVISION.md. tools/ships/ws_hull.py (workshop registration of sketch renders) moved into the repo.
- Runtime: nothing changed (no emitter in the jump code yet; the damage rule is a doc proposal).
Open (author): sketches and proposed frames; Kapehu Maori wording; emitter damage rule.

### Queue after the decisions (2026-11-10, "continue down the list")
- N1 model pad now conical like its physics (two tile courses on the 1.3 m cone, units/cobretty.py n1 pad + hits).
- Five stale engine tests re-baselined to the CURRENT production laws (production not touched). Root causes:
  universe_thermal_radiation (multiplier 2.5 -> 10, e41566c6: expectations now computed from the law);
  engine_unit_damage_model + universe_material_engine_thermal (pads became a dedicated 6 MW/m2 sink ramping from
  nominal to safe temperature, coolant decoupled, 975d2a5f: fixtures moved to the pad law / current CR-1 geometry,
  energy conservation and overheat-on-destroyed-pad kept; engine heat capacity is floored at 1 MJ/K, so the life
  fixture runs at 1000 x power); installed_engine_actuator (display name "Cobretty Cobra CR-1 Vector Main Drive",
  efficiency 0.991); equipment_damage_failure_pipeline (95 % damage switches the engine off per MainEngineCutoff.h
  51a5fc68 - the module stays Attached, the slot is not operational). All 5 + cobra_compact_drive_thermal are now
  C++ gates (tools/run_gates.py CPP_SUITES): --cpp-only 8 gates, 8 PASS. Python gates 205 PASS / 6 known red.

### Step 3 slices S1 + S2 (ship compartments asset) - OFFLINE PASS (compiled check), runtime S3-S5 not started
- S1 data: tools/ships/ship_asset_source.py derives the compartments from the Cobrett-I layout sketch (instead of the
  catalog blocks planned in the survey): one compartment per cover plate (footprint x hull thickness) + one per
  propulsion bay (ER-5 + its 2 CR-1, opened by the ER-5 pad). Equipment instances -> interior of the compartment
  containing them; hull-skin units (weapons, emitters, skids, radar, dock, turret, EC-2 cockpit) exterior.
  Output (untracked, regenerable): assets/models/ship_assets/<ship>/source (ship_asset.json + geo/*.obj, 13 MB).
- S2 assembler: tools/ship_asset_assemble/main.cpp -> EliteShipAssetAssemble (CMake target): semantic nodes, state
  "breached" on 15 plates + 2 ER-5 pads (plate render node intact-only, Opening + exterior CompartmentPortal), 17
  compartments, 558 collision volumes from the ship hit volumes (plate main volume intact-only, seams / mounts
  always), render LODs 0-2 Component (units' LOD1/2/3, one geometry per model instanced) + 3 Shell; hull shell with
  the plate / bay cut-outs (rasterised top). Explicit 1:1 calibration, then the engine's makeProductionCopy + validate.
  compiled/cobrett-i.elmodel 12 MB, assembled in ~4 s.
- Check: tests/ship_asset/ShipAssetCheck.cpp -> EliteShipAssetCheck (through ModelAssetPresentation::select, FULL at
  24 m): intact -> 0 compartments / 0 interior draws; plate over the core torn off -> only c_cover_6, interior = eq_core;
  ER-5 pad shot away -> only c_bay_l (ER-5 + 2 CR-1); two plates -> exactly two; breach facing away -> none; atomic
  switch (plate main collision leaves, one opening). Cost: intact 33 draws / 16.9k tris; +254 tris (core
  compartment), +9.3k (core + service compartments), bay +29.5k (the author's ~30k concern: only when a bay is open).
  Result: 9/9 PASS. Not yet a gate (needs the main build tree).
- Next: S3 runtime render (ships on ModelAssetGpuLibrary, nodeState from replicated state), S4 server breach state per
  compartment + replication, S5 hits/collision from the asset. Acceptance of the stage = live flight (see below).

### Step 3 S3 runtime render, opt-in (author 2026-11-10: option "a" - behind a flag, the game keeps the legacy Cobra)
- ModelAssetGpuLibrary: ObjectType::CobraMk1 -> assets/compiled/models/cobrett-i/cobrett-i.elmodel ONLY with
  ELITE_SHIP_MODEL_ASSETS=1 (or =<path>). The ship asset is in the game basis and true metres: identity
  assetToLogical, scale 1 (root cause of the first in-game frame - ship turned 90 deg and scaled 0.8: the legacy
  Cobra mesh basis / 22.2 m logical size were applied). The compiled package goes to src/assets/compiled/models/
  cobrett-i (git-ignored like every compiled package; regenerate: ship_asset_source.py + EliteShipAssetAssemble).
- SceneRenderer: real ships with a package go to prepared.modelAssetObjects (isShip, role -> shouldDrawRealShip),
  legacy meshes skipped for that ship; ObjectState.nodeState from the item. No replicated breach state yet (S4):
  DEV ELITE_DEV_SHIP_BREACH=plate_cover_6,pad_eq_er5_l,... ; DEV ELITE_DEV_SHIP_ASSET_PLACE_M=<m>[:<tilt>] puts the
  PLAYER ship in front of the main camera tilted to show the dorsal plates.
- Hull top rebuilt as exact trapezoid rows (no staircase on the swept wing edges); check still 9/9 PASS.
- LIVE (target machine, build/EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple, modern
  style, captures by the game's back buffer D:/tmp/elite_ship_s3_f.png intact / _h.png breached): FULL 976 px, 4
  breaches -> 4 compartments, 12 interior draws (walkway + tanks in the spine, ER-5 + CR-1 in the left bay), closed
  compartments not drawn, 57k triangles. Python gates 205 PASS / 6 known red.
- Not done: S4 (server breach state per plate / pad + replication -> nodeState), S5 (hits / collision from the
  asset). The legacy collision / hit volumes of the Cobra stay authoritative (22.2 m) while the asset is 27.8 x 23.3 m.

### Step 3 S4: breach state from the server, no wire change - LIVE PASS (opt-in path)
- Survey (subagent): no generic replicated per-ship flags exist; module states (Attached / Destroyed / Detached /
  Hanging) are replicated in ObjectModuleSnapshot; static node ids on the wire are forbidden
  (check_replication_static_definitions). Decision: derive like hiddenPartIds on the client.
- src/game/client/ShipBreachNodes.h: legacy Cobra module -> asset nodes (ship_panel_top_{L,C,R}{F,B} -> the cover
  plates of that column / row group; propulsion reactors *left* / *right* -> pad_eq_er5_l / _r).
  ClientWorldState::rebuildHiddenPartIds also fills ClientShipState::breachedNodeIds (a module not locally visible,
  itself or by its parent chain). SceneRenderer sets nodeState "breached" from it (+ the DEV env list).
- DEV ELITE_DEV_DESTROY_MODULES_AT=<s>:<id>,... (DevScriptRunner) destroys player modules through the debug session
  (the dev panel's server path).
- LIVE (nav-stress:simple, ELITE_SHIP_MODEL_ASSETS=1, destroy ship_panel_top_CF + ship_drive_left_reactor at 15 s):
  LOD log before 0 compartments / 0 interior draws, after 3 compartments (plates 6, 7 + left bay) / 9 interior
  draws. Captures D:/tmp/elite_ship_s4_{before,after}.png (dark lighting, evidence = the presentation log).
  Python gates 205 PASS / 6 known red.
- Limits: the breach follows destroyed / detached LEGACY modules (panel health), not the per-plate seams of the
  asset (that is S5 together with hits from the asset); penetration holes (BurnState) stay server-only.

### Author 2026-11-10: emitter damage rule ACCEPTED; placement per mesh; ship assets in the editor; Maori check
- Jump-field emitter damage rule ACCEPTED (one emitter lost: J ceiling halved, no jump until repaired) - docs + the 4
  passports updated; RUNTIME NOT IMPLEMENTED (the jump code has no emitter modules yet) - queued.
- Placement belongs to the mesh (author): ModelAssetGpuLibrary reads a socket of kind "object_frame" from the asset
  (origin, rotation T*Rx*Ry*Rz, extent.x = uniform scale); assets without it keep the legacy descriptor fit (station).
  The per-type CobraMk1 special case is removed. LIVE: "placement from its object_frame socket", logical size
  27.8 x 5.8 x 23.2 m, breached plate 7 + right pad drawn.
- Ship asset semantics filled by the generator: 52 structural links (15 plates WeldSeam to the hull with their 4
  seam damage proxies, 35 modules EquipmentMount with their 4 mount proxies, 2 ER-5 pads FixedMount to their
  reactor), 15 plate hit regions (intact), the object_frame socket.
- tools/ships/build_ship_assets.py: sketch -> SOURCE -> EliteShipAssetAssemble -> production
  src/assets/compiled/models/ship_cobrett_i/ship_cobrett_i.elmodel + WORKING copy in the editor workspace
  (build/tools/model_asset_editor/workspaces/ship_cobrett_i/working). Model Asset Editor catalog: new group
  "ship_assets" (compiled/models/ship_*), opens the WORKING head with every field filled. Not opened in the editor UI
  by me (no headless mode) - author's look.
- Kapehu Maori words checked against Te Aka (subagent): wrong ones replaced (rata -> hihiani radar, rererangi ->
  matatopa drone, tanga -> kura tank, kopae rererangi -> riu matatopa drone bay, rauangi wera -> papa hahana,
  kapuhera -> kowae mawhiti, poti oranga -> poti whakaora, comm -> punaha whakawhitiwhiti korero); coinages from
  attested parts marked in equipment_naming.py (reactor putauhohe karihi, emitter putuku whaitua, coolant loop ara
  whakamatao, life support punaha tautoko ora) - a fluent speaker should confirm; pukupuku (armour) kept with caveat.

### Jump-field emitter damage rule in the runtime (author-accepted 2026-11-10) - LIVE PASS
- JDrive.h: JFieldIntegrity + jFieldFactor (all intact 1; one emitter lost 0.5 and no jump; core or every emitter
  lost 0) + jumpFieldComplete; src/game/simulation/JFieldIntegrity.h counts "*jump_field_emitter*" modules and
  ship_jump_core (intact = Attached with health). Cobra descriptor: passive modules ship_jump_field_emitter_L / _R
  (like the jump core; no mesh / hit volume yet - shooting them needs S5).
- Server: J acceleration cap (route / global command, pilot +/- ramp, HOME / END command) x factor, J speed cap
  x max(0.5, factor), J refused without a field; requestJump refuses FieldIncomplete (new jump::Refusal 10 with
  Russian text) and a charge-up is cancelled when an emitter is lost mid-way.
- Test j_field_emitter_tests (C++ gate): PASS. Gates --cpp: 220, 214 PASS / 6 known red (one new red on the way
  - check_test_suite_source_integrity read "JDrive.h" in a CMake comment as a source - fixed).
- LIVE (nav-stress:simple, flown out of the hub keep-out on the main engines, then J HOME): emitter L destroyed ->
  J command 50 000 g, jump to Alpha Centauri REFUSED "поле прыжка неполное"; intact -> command 100 000 g.

### Cobra drive heat / pad derating - LIVE ACCEPTANCE EVIDENCE (2026-11-10, target machine)
- DEV tools: ELITE_DEV_DRIVE_THERMAL_TRACE=1 ([DriveThermal] per main engine per second: T, pad intact, burn,
  safe thrust, thermal damage), ELITE_DEV_DRIVE_PAD_DAMAGE=<s>:<intact01> (ObjectModuleRuntime::
  debugSetEngineEmitterIntact). Root-cause fix found on the way: the main-engine heat clock ran only for hub-tactical
  allocator samples; free-flight burns now feed it too (aft bank, requested share x the engine's safe thrust).
- Healthy pads, full thrust (CR-1 passport: nominal 450 K, safe 1250 K, 70 MJ/K, waste 43.65 MW, pad 71.9 MW):
  0.62 K/s at start, T(183 s) = 551.7 K live vs 551.6 K model; equilibrium 936 K < safe -> full thrust indefinitely.
- Pads at 50 %, full thrust 2650 s: T(2090 s) 1167.3 K live vs 1167.2 K model; the governor band (safe - 50 K) is
  entered at 2322 s (model 2306 s); safe thrust 0.99 -> 0.89 at 1231 K and falling to the 0.824 model value at the
  safe temperature; thermal damage 0 (the governor protects the drive). Matches cobra_compact_drive_thermal.
- Not covered live: the reactor (ER-5) pad damage and the RCS banks.

### Combat trials for balance data (author 2026-11-10: "a firefight with rams, how survivable are the ships")
- tools/combat/combat_trials.py: 9 live trials (pulse / beam / military laser, two missile salvos, duel, rams 90 / 30
  m/s, ram under fire) -> docs/COMBAT_TRIALS.md; conclusions + proposals docs/COMBAT_BALANCE_NOTES.md. Balance NOT
  changed (author's call).
- REAL BUGS FIXED (GameSimulationCombat.cpp): missile contact compared the step one tick early (tvel x dt ~500 m at
  orbital speed: every missile "passed" at 350-480 m) -> now 4/4 and 3/3 hits; a missile with a target integrates in
  the target's frame (world-frame drift at ~30 km/s defeated the guidance); the rack point summed every volume of the
  carrying module (first volume now).
- MISSING FEATURES found: no ship-ship collisions at all; NPCs never fire; a ship is never killed by combat (modules
  only). DEV prototypes behind flags (game unchanged by default): ELITE_DEV_SHIP_COLLISIONS=1, ELITE_DEV_TARGET_FIGHTS=
  duel|ram (+ ELITE_DEV_RAM_SPEED), ELITE_DEV_PLAYER_TRACK=1 (trial pilot sweeping the aim), ELITE_DEV_COMBAT_LOG=<csv>.
- Key numbers: default pulse laser 1.5 MW ~1.6 hp/s (5 panels in 150 s); military 10 MW: first module 2.5 s, ER-5
  48 s, mains 70 s; Needle missiles 52 hp for 4 hits (the jet leaves the hull); ram 90 m/s ~490 MJ per hit kills
  lasers and cockpit of BOTH ships. Python gates 205 PASS / 6 known red.
- Open (author): the 5 proposals in COMBAT_BALANCE_NOTES.md (jet into the compartment, default laser, ram crumple zone
  + RCS avoidance, disabled-ship condition + EC-2 escape, NPC duel AI).

### Combat round 2 (author 2026-11-10: items 1, 4, 5 + laser line-up with a controller + reactor explosions) - LIVE
- Lasers (weapons.json + WeaponCatalog): Pulsar (default), Discharger (capacitor 60 MJ), Squall 8 MW, Stiletto 1.2 MW,
  Cutter 15 MW; former ids aliased. Laser controller (GameSimulation::laserControllerStep): electrical = beam /
  efficiency from ShipCore::serviceReactorHeadroomMW (Cobra ~9 MW), (1 - eff) heats the laser body, which conducts into
  the service coolant (ShipCore::addServiceHeatMJ); throttled over the last 60 K of the body limit and near the
  coolant's critical temperature. First run found the body limits (900 K) below the coolant's working ~1000 K -> fixed.
- Item 1: a shaped jet that holed the skin spends its rest on the equipment within 3 m of its line (Needle x4: 52 ->
  271 hp). Item 4: disableShipIfCombatLost (cockpit or all propulsion reactors) -> destroyedCause 3, no control, NPC
  derelict, modules kept; HUD text. Item 5: noteCombatAggression -> m_combatHostility; the duel AI is standard (NPC
  fights its attacker; excluded from the traffic AI); ELITE_DEV_TARGET_PASSIVE=1 for pure weapon trials.
- Reactor explosions: catastrophic containment (ShipCore queues) / reactor destroyed while running (35 %, service 20 %)
  / reactor-bay frame destroyed (35 % per installed ER-5); one blast per reactor (marked before the blast - a chain
  re-queued the same one); 25 % flash within 4 m, 40 fragments: own hull by penetration rays, other ships by solid
  angle, random modules. Found: the legacy Cobra ER-5 modules have no placed hit geometry (only unplaced link
  volumes) - they sit in ship_frame_reactor, used as their location; isReactorModule excludes the frame (it was
  counted as a reactor in the disabled check and the breach mapping - fixed).
- Trials (13): docs/COMBAT_TRIALS.md, round-2 table in COMBAT_BALANCE_NOTES.md: Discharger 781 hp / 150 s, Squall 666
  (throttled by the Cobra reactor and heat), natural ER-5 explosion after the bay frame fell, duel decided in 30 s by a
  missile into the cockpit (90 hp). Python gates 205 PASS / 6 known red.

### Step 3: ship hull in compartments, interiors drawn only for a breached compartment (author 2026-11-09) - PLANNED
Survey (source-audited 2026-11-09):
- Ships render only through the legacy ObjectAssembly path (SceneRenderer ship loops ~1296-1340 / 1618-1760,
  hiddenPartIds from ClientWorldState::rebuildHiddenPartIds). ModelAssetGpuLibrary::packageFor is station-only.
- ModelAssetPresentation already implements Compartment / CompartmentPortal / Opening / StateVariant + activeStates:
  interiors are drawn only behind open, visible portals (BFS depth 2, Full representation >= 700 px); reference test
  tests/model_asset_presentation/PresentationCheck.cpp (synthetic 22 m ship, "breach -> engineering only").
  ObjectState.nodeState is never filled by the renderer (only nodeMotion).
- No breach / opened-compartment state exists in game or world code; the client receives only module state/health.
  Penetration BurnState holes are server-only.
- ship_layouts_3126.json compartments are a designation tree without positions; 3D placement exists only as catalog
  installed-item blocks (center, size) in ship_catalog_3126.json. Catalog hulls draw only primitive blocks.
Slices (each its own acceptance; commit != acceptance):
  S1 data: compartment extents per ship (union of its units' catalog blocks) + part -> equipment model folder map.
  S2 ship asset assembler (tool, ModelAssetBinary): hull shell per compartment (intact + breached variants), equipment
     instances sharing one geometry per model, Compartment / exterior Portal / Opening / StateVariant "breached",
     collision from hit_volumes.json; output a compiled .elmodel per ship. First ship: Cobrett-I.
  S3 runtime render: ships on ModelAssetGpuLibrary (packageFor + SceneRenderer ship loop), nodeState from replicated
     state, ship-specific presentation thresholds (an open breach must show interiors closer than the 700 px rule).
  S4 server: per-compartment breach state (hull section health / penetration holes) + replication to the client.
  S5 (high risk, later): hits and collision from the asset instead of descriptor AABBs.
Acceptance for the stage: live flight on the target machine - an intact Cobrett-I shows no interior; breaching one
compartment shows only that compartment's equipment.

### Author 2026-11-10/11: containment drama, compact ER-5 pad, ordinary collisions incl. stations, impact tiers
- Containment emergency (commit 541702ef): hidden 20-60 s fuse, core heats, replicated Containment alert -> red
  flashing beacon (no sound - author), blast or quiet death by chance; reactors off x0.4; repair drone (Ctrl+R)
  stabilises in ~8 + 25 s; LIVE: none -> blast after 37 s + chain emergency on the other ER-5; reactors off -> quiet
  collapse; drone -> stabilised at 36 s of a 37.9 s fuse. Cockpit 260 hp / armour 40.
- ER-5 compact pad (23409a7f): eff 0.985, waste 76 MW, 16.4 m2 as two 2.35 x 3.5 m plates top + bottom, thermal
  capacity 80 MJ/K; er5 failure matrix + emergency bridge PASS; model, canon, ship asset, sketch renders.
- Collisions are standard now (ELITE_DEV_SHIP_COLLISIONS=0 turns off): broad spheres + SAT over the hit boxes, impulse
  with lever arm (push + spin through pitch/yaw/rollRate), static objects (stations) with infinite mass and their spin,
  dock modules excluded for now, dock-controlled ships excluded; tiers: < 0.5 MJ per module no damage, damage to the
  touching modules, ShipCore::applyPartImpulse tears links (parts knocked off), > 3 GJ crushed (cause 4, HUD).
  LIVE: Cobra into the station at ~490 m/s -> 30 GJ, 22 links torn, crushed; 30 m/s ship-ship -> 50 MJ, 3 links torn,
  spin 0.1-0.8 rad/s. Reactor blasts push and spin the hull (~2 % of the energy).
- Station "teleporting" (author report): not reproduced. DEV ELITE_DEV_STATION_TRACE=<csv> (per frame camera-space
  origin + spinning ring angle): the station at 10 km steps 0.16-0.4 m / frame (sub-pixel); the ring turns every frame
  but its rate jitters 1.2-6.3 deg/s around 2.9 (interpolated module angle) - not visible at 10 km. Need the author's
  situation (distance, mode, docked / J / map) to chase it.
- Open: S5 (hit geometry from the assets for every module in the game - the legacy Cobra ER-5s still have none),
  dock-module collisions, munition norm tables (author: the fleet plans by expenditure norms) + NPC missile economy.

### Author 2026-11-11: model storage, reference model statuses, celestial textures queued
- Storage audit (commit baeb058b, docs/MODEL_ASSET_STORAGE.md): where equipment meshes, ship hulls, sketches, the
  compartment asset, the station and the editor workspaces live; the in-game Cobra mesh (cobrettymk1) existed only in
  build/ - rescued into assets/models/ships (now versioned). Open steps there (each one alone, checked in the game):
  author picks the canonical Cobra (cobrettymk1 = game vs cobretty-mk1-int = editor), station compiled package under
  git + old copies removed, ships into .elmodel (= S5), sketches out of assets/models, dead MeshLibrary paths, equipment
  OBJ in git or generator only.
- Ship reference model status (author: "all Cobrett-I A3 internals shown ready and the hull done, but it is a sketch"):
  root cause - my workshop scripts set modelReady.ready = true whenever generated model renders were attached (108
  parts, two identical timestamps); the hull flag lived only in the author's browser (IndexedDB). Fix: the checkbox
  "модель готова" is replaced by a status none / набросок / в игре (не утв.) / утверждена (store.js status(),
  setStatus(); a legacy ready record without a status reads as draft; only "approved" counts as ready). workshop.json:
  all 108 parts = draft with a note (generated by tools/models), all 51 hulls = draft with a note naming the sketch
  (A3: layout sketch v3; the game draws the old cobrettymk1 assembly, not this hull); the newer records override the
  browser's old flags on merge. Checked in headless Chrome: A3 page 19 statuses, all draft; header "утверждено 0 из
  995 мест, набросков 992". Approvals are the author's clicks only.
- assets/generated/celestial (analysis): a cache baked from assets/source/celestial (--bake-celestial-textures
  --import-real); read by BodySurfaceFieldLoader (fallback), MapCelestialRenderResources (system map) and
  DetailMapPlanetPass. Author: most of the textures are ugly (Earth best, the Moon maybe; Jupiter, Phobos, Deimos
  awful) - QUEUED: procedural replacement through the existing generators (PlanetSurfaceField, GasGiantField,
  SmallBodyField) with an "appearance passport" per famous body so it stays recognisable; order Jupiter, Phobos,
  Deimos -> Moon, Mars -> the rest; Earth stays mostly as is. The old texture path is switched OFF behind a flag, NOT
  deleted; deletion only after the author checks the result. Also: assets/source/celestial (21 MB) under git; Tau Ceti
  data (778 MB) looks orphaned - check before touching.

### Author 2026-11-11 (cont.): procedural bodies on the map, appearance passports, Jupiter root cause
Source-audited + compiled + live DEV captures (back buffer, --dev-direct-flight, Sol); NOT committed, NOT accepted.
- Map procedural source (src/game/system_map/ProceduralBodyTextureCache.*): equirect albedo baked on a background
  worker (below normal priority) from the body's generated descriptor through makeSurfaceField - the same field flight
  draws; GL row order / longitude as the cached TGA (TextureLoader flip). Wired into all three map paths:
  System (globalAlbedoTextureForBody), Detail (globalAlbedoTextureForIdentity, shape-model OBJ albedo override),
  Hub (albedo + cached normal suppressed when procedural). Policy ELITE_MAP_PROCEDURAL_BODIES: unset = procedural only
  for bodies without a cached texture (every system except Sol / Tau Ceti), "all" = procedural everywhere (compare),
  "0" = old behaviour; ELITE_MAP_PROCEDURAL_SIZE (default 1024). Old cache path untouched (not deleted).
  LIVE: Mars Detail map in "all" mode drawn from the bake (2.3-2.6 s per 1024x512 body); Phobos Detail = real OBJ shape
  with the procedural albedo. Other systems: same code path, descriptor lookup by system folder NNN_ + body JSON id;
  not yet seen live (the player starts in Sol; needs a jump) - author to check.
- Appearance passports (src/world/celestial/surface/AppearancePassport.*, data src/assets/data/celestial/
  appearance_passports.json): per famous body, procedural=true (generator instead of curated / cached maps, in flight
  via loadBodySurface and on the map) + mean_albedo_linear (the field's sphere-mean colour scaled per channel to the
  observed one; relief / pattern untouched). Applied inside makeSurfaceField, so map, flight, server terrain probe,
  baker and preview agree. Author decision 2026-11-11: Io, Europa, Phobos, Deimos look better procedural, colour to be
  matched -> passports for these four (Io 0.76/0.56/0.17, Europa 0.66/0.60/0.50, Phobos 0.080/0.068/0.058, Deimos
  0.082/0.069/0.057, NASA fact-sheet albedos). Colour acceptance = author.
- Jupiter "awful" root cause: the Detail map never showed the Cassini texture - environment/sol/jupiter.json had
  surface_visibility hidden + load_surface_textures false, so a procedural banded cloud deck was drawn instead.
  Changed (TEST, author to judge): surface visible, cloud deck off, atmosphere haze kept. Same flags remain on Saturn,
  Uranus, Neptune, Venus (untouched). GasGiantField itself is far worse than Cassini (cartoon bands) - procedural
  replacement for gas giants needs a new band / turbulence model first.
- Generator bug fixed: PlanetSurfaceField::lakeInCell kept a thread_local std::unordered_map; its destructor
  corrupted the heap at thread exit under MinGW (tls_atexit) -> crash of any short-lived sampling thread on
  terrestrial bodies (Earth). Replaced by a trivially destructible direct-mapped memo of the same pure function.
  Planet Lab --verify against checksums recorded with the old code: 86/86 identical. Pre-existing FAIL (before and
  after): "rocky coastline ... landmark id changes 3443".
- Tools: build/EliteCelestialPreview.exe <body.json> <out_prefix> [width] (equirect map + lit globe PNG, headless);
  DEV aliases body:phobos|deimos|io|europa in ELITE_DEV_SOL_MAP_SCRIPT. Preview page (private artifact):
  https://claude.ai/artifact/EKaGhQD1PzTzBG2KfQyDCD
- Next: author checks the map live (another system after a jump; Jupiter Detail close up; passport colours);
  then gas giant band model, Phobos Stickney / grooves, Moon / Mars review; Tau Ceti 778 MB data still to check.
- Celestial resource audit (source audit, no measurement yet): docs/CELESTIAL_RESOURCE_AUDIT.md - CPU surface fields
  never released (flight renderer + terrain probe, up to 3 copies per body on one machine), curated maps held as
  float (4x), map GPU caches unbounded, Sol look from two sources (flight = curated sources, map = restyled baker
  cache + hidden surfaces), lod/tiles (~70 % of the 4.9 GB Sol cache) never read. Target: one BodySurfaceRegistry per
  process + one budgeted map texture cache + map bakes the flight field. Not started - author decides.
- Repository cleanup (author order 2026-11-11, group 1 only): removed root headless_server/ (stale 2026-09-28 build,
  6.1 GB; canonical build/headless_server untouched), the stray "$' " file, root last_* / navigation_* traces and logs,
  tools/navigation_runtime/last_* traces and logs, third_party/asio-1-24-0.tar.gz. None were tracked.
- DEV memory ledger (step 0 of docs/CELESTIAL_RESOURCE_AUDIT.md): src/core/DevMemoryLedger.{h,cpp} (lib
  EliteCelestialSurface: client + server). ELITE_DEV_CELESTIAL_MEMORY=<s> prints every s seconds "[CelestialMemory]"
  process working set / private + one line per cache (cpu flight.surface_fields, flight.bodies_known, gpu flight.globes
  / clouds / terrain / asteroids, cpu terrain_probe.fields, gpu map.generated_albedo / preview / normal,
  map.procedural_albedo, map.cloud_layers, cpu map.procedural_pending). SurfaceField::memoryBytes() (curated maps,
  settlement discs, passports forward). Inert without the env var.
  First live numbers (Sol start near Earth, D:/tmp/elite_mem/err.txt): process working set ~1.1 GB, private ~1.0 GB;
  flight fields 66 MB CPU for 2 bodies (Earth + Moon curated maps as float, matches the 17 B/texel estimate);
  flight globes 96 MB GPU for 2 bodies; Jupiter Detail map +10.7 MB GPU (cached albedo). Not yet measured:
  accumulation over a long flight / a jump, the server process, the terrain probe (needs a landing).
  DEV note: a game window started in the background is minimized and draws nothing; ELITE_DEV_FRAME_CAPTURE restores
  it. ELITE_DEV_SOL_MAP_SCRIPT "system;body:X;detail" after a first Detail map does not switch to another body.
- Memory scenarios (DEV, ELITE_DEV_OFFSCREEN_WINDOW=1 + ELITE_DEV_CELESTIAL_MEMORY=15, logs D:/tmp/elite_mem/A..D):
  A Earth hub -> Moon autopilot (10 min): fields 66 MB / 2 bodies all along; the terrain probe loaded a SECOND Moon
  field (32 MB) at the approach - client + server share one probe cache in --dev-direct-flight (one process);
  globes 96 MB -> 0 -> 96 MB during the J transfer (budget works); terrain rings 3 -> 8 MB near the ground;
  transient peak 1.50 GB at ~150 s with the celestial caches DOWN (~400 MB outside the ledger - to find).
  Landing not reached in 600 s (580 s: 269 m from the pad at 179 m/s); November runs landed in ~4 min - to check.
  C hub start @50 km -> jump to Tau Ceti: exit on Tau_Ceti_m 418 km; Sol's Earth + Moon fields (66 MB CPU) and their
  globes (96 MB GPU) are NOT released in Tau Ceti (globes 192 MB / 3, known bodies 36 -> 45); transient 1.63 GB for
  ~30 s after the jump, steady 1.22 GB vs 1.10 GB before.
  B/B2 "jump failed" root cause: not a jump bug - the DEV thrust:30 flew the ship nose-first into the Earth Orbital
  Hub (1.1 km/s, CRUSHED), the charge-up was then interrupted; with no thrust the ship co-orbits the hub at 10 km and
  never reaches the 20 km J clearance. Minor UX: a destroyed ship's charge reports "вышел из нормального полёта".
- DEV start point ELITE_DEV_START (author 2026-11-11), applied once ~3 s after the start (GameServer::applyDevStartPoint):
  hub:<id>[@<km>] normal flight km above the hub at its velocity (GameSimulation::devPlaceShipNearHub);
  body:<stem>[@<system>] orbit of any body of any system at J floor + 100 km (descriptor file stem: moon, phobos,
  jupiter, eps_eri_b; reuses devPlaceShipInSystem = a jump exit); system:<id>. LIVE: hub@50 -> jump PASS; body:phobos
  -> 150 km orbit of Phobos, frame D:/tmp/elite_mem/D/f080.png.
- Fixed: a zero-distance jump exit (DEV placement, also the server load test) asserted in std::normal_distribution
  (sigma 0, debug libstdc++) - the scatter is drawn only for sigma > 0.
- DEV window: ELITE_DEV_OFFSCREEN_WINDOW=1 opens the game window off the desktop without focus (a minimized window has
  a 0x0 surface and renders nothing); frame captures work.
- Transient memory peaks FOUND + FIXED: GlobeBake::bakeGlobeFace held a whole face of SurfaceSample + dvec3 directions
  ((n+2)^2 x ~110 B = ~115 MB per 1024 face), several faces in parallel on the flight workers -> +400-500 MB for tens of
  seconds on every large globe tier (J transfer, after a jump). Now baked in strips of 32 output rows (+1 halo row):
  ~4 MB per face. Bit-identical output: 6-face bake hashes of Earth / Jupiter / Phobos / Alpha_Centauri_A_j at face
  200 (incl. a partial last strip) equal before / after (scratch checker built against libEliteCelestialSurface).
  LIVE (2 s ledger, D:/tmp/elite_mem/P1, P2): Moon transfer peak 1.60 -> 1.16 GB at the 72 MB globe bake; after the
  jump to Tau Ceti 1.63 -> 1.25 GB. Ledger got cpu flight.globe_bakes_pending / gpu flight.globe_staging /
  cpu flight.cloud_bakes_pending / cpu flight.terrain_builds_pending.
- Startup crash FOUND + FIXED (pre-existing, intermittent; 3/3 with ELITE_DEV_START + 2 s ledger): EconomyModel::load
  iterated `delay.value("hubs", json::object()).items()` - the json temporary died before the range-for body (pre-C++23)
  and the keys were read from freed memory (0xfeeefeee). Bound to a local; 3/3 runs pass. No other such pattern in
  src/ or tools/.
- KNOWN, not fixed (pre-existing since at least 2026-11-10, D:/tmp/elite_combat/em_*.txt): every run killed by
  `timeout` ends with [CRASH] SEH 0xc0000005 in opengl32.dll after "Failed to unregister class Chrome_WidgetWin_0" -
  a GL call after the window / WebView teardown on a forced shutdown. Not a gameplay crash; to look at separately.
- Step 1 of docs/CELESTIAL_RESOURCE_AUDIT.md DONE (source + compiled + live DEV, not author-accepted):
  (a) curated maps compact: ImportedSurfaceMaps::albedoRgb (source 8-bit, decoded through a 256-entry pow(c/255, 2.2)
  table) instead of glm::vec3 - bit-identical globe bakes (6 faces, size 160) of Earth / Moon / Mars / Jupiter;
  memory Earth 35.7 -> 16.8 MB, Moon 33.6 -> 14.7, Mars 8.4 -> 3.7, Jupiter 100.7 -> 25.2.
  (b) acquireBodySurface (BodySurfaceFieldLoader): one surface per body per process, weak references, concurrent
  requests wait for one load; flight renderer + terrain probe use it. (c) flight releaseStaleBodies(): a body not in
  the frame for 300 frames frees textures / terrain / its surface ref; the probe drops other systems' bodies on a
  system change. Ledger: cpu surfaces.registry_unique; holders report *_ref (not summed into the total).
  LIVE (D:/tmp/elite_mem/P1, P2): start fields 66 -> 30 MB; Moon approach: probe holds the Moon (14 MB ref), unique
  stays 30 MB / 2 (before: a second 32 MB copy); jump to Tau Ceti: ~5 s after the exit Sol's surfaces and globes are
  released (unique 0 MB / 1, globes 192 -> 96 MB / 1, known bodies 45 -> 9), working set ~1.0-1.07 GB vs 1.22 GB
  before. Planet Lab --verify 86/86 identical (pre-existing rocky coastline FAIL unchanged). Runs still end with the
  known forced-shutdown [CRASH] in opengl32.
- Next: step 2 (one map texture cache with a byte budget, dropped on a system change), step 3 (the map bakes the
  flight field; Sol look unified; assets/generated no longer needed at runtime).
