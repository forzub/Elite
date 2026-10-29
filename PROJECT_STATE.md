# ELITE — PROJECT STATE (single canonical continuation document)
Updated: 2026-10-29, Europe/Kyiv. Repository: `forzub/Elite`. Branch: **main ONLY**.

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

### Universe
- 61 stellar systems at real ICRS positions (identity audit), deterministic
  generator `src/world/generation/` (UniverseSeed 0x454C495445303031; stellar /
  body / satellites v1, resources v1, interstellar v3), physical catalog under
  `galaxy_details/systems/<NNN>_<slug>/`, StarAtlas schema 2, Kepler orbits on
  the UniverseClock, `CelestialEphemerisService` (read-only states, not yet
  used by gameplay), gravity from catalog mu / radius / atmosphere radius.
- Navigation domain (`GalacticGrids.h`): 140-ly envelope + remote 20-ly balls
  and corridors (313 root cells, 1 component); grids 1 ly / 40.32 ly root /
  25 ly interstellar generation cell.
- Interstellar layer v3: background medium field (Local Bubble cavity, wall,
  warm neutral), irregular density lattices (`DensityLattice.h`), compact
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

## Open (before this stage's audit; to be re-classified)
- Two narrow zones, delay 6, main lost inside zone 1 (offline): curve entered too fast.
- Exact 180 deg vertex rejected by the docking planner.
- Dead translational RCS direction -> replan refuses; capability recovery
  does not speed a route up; micro-brakes after a reprofile.
- Emergency thermal coupling not installed (ER-5 overflow granted 0 MW).
- NOT visually verified by the user: Galaxy Map (domain, ISM), System Map
  ellipses / presentation levels; resource audit not reviewed.

## Current stage (user order 2026-10-29 evening, 24 points)
Order: decomposition -> PROJECT_STATE -> interstellar clouds (causal,
no Sol privilege, provenance, shells, icy debris fields, hazard API, cheap
map proxy) -> CPU waste (docking corridor cache, scene prepare,
recompile, RelWithDebInfo) -> propulsion / power / thermal / damage /
capability / planner / autopilot audit and fixes (Newtonian + Assisted) ->
acceptance matrix -> STOP (next stage: interstellar navigation).
Not in scope: planets / orbits / resources regeneration, interstellar
route planner, docking capture system.

### Progress
- 1. Decomposition: DONE (see Code layout). OPEN: `updateDockingAdvisory`
  (3.8k lines) is still one member function; `-Wa,-mbig-obj` kept as a
  safety net only.

## Next step
- Interstellar clouds (stage points 3-11).
