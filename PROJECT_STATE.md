# ELITE — PROJECT STATE (single canonical continuation document)
Updated: 2026-10-30, Europe/Kyiv. Repository: `forzub/Elite`. Branch: **main ONLY**.

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
- Not done / OPEN: the game's map renderers still draw equirect textures
  (no in-game cube-sphere renderer; the patch streamer is ready for it); no
  procedural body is configured in the game data (baker path verified on a
  scratch root only); human visual review of the Lab pending. Journal of
  decisions, doubts and proposals: D:/tmp/PROCEDURAL_PLANET_NOTES.md (e.g.
  every generated temperate planet has ocean_coverage 1.0; no biosphere fact
  in the descriptor).

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

## Next step
- After the user's review: the separate stage INTERSTELLAR NAVIGATION
  (connected domain, ephemerides, gravity, InterstellarNavigationEnvironment
  hazards, icy fields, rogue objects, refuelling, ship-specific risk).
