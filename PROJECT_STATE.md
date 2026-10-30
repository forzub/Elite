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
- Planned turns use min(0.5, combined RCS share) of the angular scalar.
  Capability losses reprofile / hand back; recoveries (+10 %, 0.5 s)
  reprofile up to the planned nominal (both laws stabilise and replan on a
  failed or invalidated route: 5 attempts >= 5 s apart).

## Map interaction contract
- ONE pointer contract (`game::presentation::PointerClickTracker`,
  `PointerInteractionSettings` 0.38 s / 12 px): hover never selects; a single
  click acts at once and is never undone; a double click on the same target
  adds its action; the cube trackers delegate to it.
- Galaxy Map clouds / icy fields: hover brightens (re-picked only when the
  cursor moves: projected bounding prefilter + ray vs coarse shell
  triangles, ~1.4 ms Debug per move); click selects (star selection
  dropped) and opens ONE info card (shared overlay) with physical data,
  provenance and the derived ENVIRONMENTAL hazard
  (`world::interstellar::environmentalHazard`, NEGLIGIBLE..SEVERE with
  reasons, never stored); risk for a ship: "not computed" (seam
  `RouteRiskForShip`); star / cube / empty click drops it and the card;
  double click flies to the navigation cube containing the clicked point.
- System Map bodies: single click selects only; double click opens the
  card, the next double click closes it; another body's click closes the
  old card; a card never outlives its body's selection.

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

### OPEN
- GATE NOT CLOSED: A3 reactor 60 % - the Assisted continuous optimizer
  finds no feasible path for the derated ship (physically a slower route
  should exist): planner side of the Assisted damage stack.
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
