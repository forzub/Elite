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
  Installed since 2026-10-29: module `propulsion_emergency_heat_exchanger`
  (ER-5 surplus -> service coolant, see the stage section).

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

## Current stage (user order 2026-10-29 evening, 24 points) — WIP
Order: decomposition -> PROJECT_STATE -> interstellar clouds -> CPU waste ->
propulsion / power / thermal / damage / capability / planner / autopilot
audit + fixes -> acceptance -> STOP (next stage: interstellar navigation).
Not in scope: planets / orbits / resources, interstellar route planner,
docking capture system.

### Done this stage (commits on main)
- Decomposition (`cf5f97ae`, `4e45618e`, `7d62dfb3`): see Code layout;
  `DockingCorridorPresentation` extracted. Behaviour preserving: matrix
  126/126 identical, real flight COMPLETE. Sizes before -> after (bytes):
  SpaceState.cpp 410889 -> 96073 (+ Docking 156964, Maps, Hud, DevPanel),
  GameSimulation.cpp 402830 -> 119556 (+ NavigationLab 113595 ...),
  SystemMapRenderer.cpp 156965 -> 15700, DedicatedClientRouteAutopilot.h
  155185 -> 86035 (+ RouteExecutionCore.cpp), ShipCore.cpp 153339 -> 23514.
  SpaceState.cpp.obj 43.4k -> 19.4k COFF sections: `-mbig-obj` no longer
  needed (kept as a safety net). No dependency cycle removed (same classes).
  OPEN: `updateDockingAdvisory` is still one 3.8k-line member function.
- Galaxy Map medium (`0a49b731`): static voxel-shell proxies (greedy-meshed
  boundary faces, 2 LODs, molecular inner shell, far blobs, frustum culling,
  one multi-draw, premultiplied, no depth writes); background not drawn;
  faint warm / strong molecular weights. Map open, Debug: medium draw 59 ->
  0.08 ms, frame 82 -> 22 ms; with the v4 data (2237 structures) 0.55 ms /
  21 ms, 0 frames > 50 ms. `tools/perf/galaxy_map_perf.sh`,
  `ELITE_DEV_GALAXY_MAP_AT`, `[FrameStats]`/`[GalaxyMapPerf]` traces.
- Interstellar v4 (`bbaf7b9e`, generation version interstellar 3 -> 4 only;
  planets untouched): cavity / shell substrate (`InterstellarMediumField`:
  Local Bubble, Loop I S1/S2 observed; procedural superbubbles beyond 300
  ly), immersed / compressed / intersecting walls, `CloudFormationPotential`,
  structures per cell = rate x integrated potential, calibrated on the
  observed census (14.97 expected warm clouds within 15 pc vs ~15); same
  environment = same density near Sol and elsewhere; provenance observed /
  procedural_inferred / procedural_generated; 2237 structures within the
  domain + 400-ly context, 10 cavity descriptors, 50 icy debris fields
  (statistical solid populations, deterministic on-demand bodies);
  `InterstellarNavigationEnvironment` (physical samples / segment
  integrals, no route cost). Check: `EliteUniverseGenerator ism-check`.
  VISUALLY UNVERIFIED by the user (window snapshot only: clouds very faint
  near Sol, stars / labels dominate).
- CPU waste: docking corridor presentation 7.2-7.8 -> 2.3 ms / frame
  (`7d62dfb3`). Scene prepare: debug assembly collection measured
  negligible (one small struct per entity) - not changed. Manoeuvre
  recompile: no > 80 ms frame in flight - not moved to a worker.
  `tools/perf/build_relwithdebinfo.sh` (build-relwithdebinfo/): Release
  flight frame median 9.5 ms, 0 frames > 50 ms after start.
- Route pilot cadence (`d41d8284`): the pilot stepped once per RENDER frame
  with the server step -> pilot clock x2.7 at 100 FPS (root cause of the old
  "pilot dt x1.64" and "game ~15 % slower than replay"); now steps on elapsed
  authoritative time. Debug / Release flights: pilot time = flight time.
- Emergency heat exchanger PROPULSION -> SERVICES (`a5314531`, thresholds
  fixed in `71c60818`): installed module + descriptor rating (60 MW, 0.25
  MW/K, pump 20 MW/MW on the service bus, engage 1950 K / release 1700 K of
  ER-5 (passport safe 2100 / critical 2400), service coolant interlock
  90 / 97 % of critical); energy exact. `EliteShipThermalCheck` 9/9 PASS.
  Finding: main / RCS nozzles conduct nothing into `m_propulsionThermal`
  (own emitter pads, coolantHeatMW = 0 by design) - the store is inert;
  ER-5 surplus is what the exchanger relieves. In the tested state the
  transfer is pump-power limited (~5 MW: little spare service power).
- Capability honesty (`71c60818`): ER-5 available power (one law,
  `engineReactorAvailableMW`) in the published Newtonian main / RCS
  authority; RCS node projection over x/y/z nozzles (stale authority after
  y/z damage fixed). `EliteNavigationCapabilityCheck` 14/14 PASS. Matrix
  90/126 PASS, 0 violations, 4 narrow X runs differ 0.1-3.5 s.

### Audit classification (this stage)
- Two narrow zones, delay 6, main lost in zone 1: ALREADY FIXED (offline
  replay on current main: I main 50 % at 40 s and X 3 mains lost at 40 s,
  delays 1/3/6: 0 violations, DONE).
- Exact 180 deg vertex: STILL REAL in DockingAdvisoryPlanner (explicit
  refusal "degenerate docking turn", no silent failure); no production
  route produces it; a fix needs a lateral loop outside the corridor
  volumes - OPEN.
- Combined RCS axes: MEASURED - two signed axes at their single-axis limits
  together deliver ~50 % (healthy 0.502, one nozzle lost 0.486); per-axis
  values stay honest for single axes; planning still uses min-of-axis
  scalars - OPEN (needs a combined-authority rule in the pilot / planner).
- Dead translational RCS direction: hand-back even if the remaining route
  might not need it (terminal hold needs all directions) - OPEN, low.
  Dead angular direction: refusal is physically honest (cannot stop a turn).
- Capability recovery (repair) does not raise route speeds: STILL REAL - OPEN, low.
- Assisted working axis: execution and autopilot use the real (emergency)
  axis; the planner is direction-agnostic on conservative single-motor
  thrust - no change.
- Reactor off while executing: Newtonian telemetry withdrawn (client waits,
  replans); `capabilities()`/`effectivePhysics` ignore reactor state, the
  published telemetry is the gate.

### Acceptance flights (real game, nav-stress:simple, Debug)
Run `tools/navigation_runtime/newtonian/acceptance.sh` on the build of
`d41d8284` + capability commits (2026-10-30). Damage at t_moving = 60 s.
| flight | result | violations | DONE (pilot s) |
|---|---|---|---|
| N1 healthy | COMPLETE | 0 (xtrack 19.5) | 239.8 |
| Newtonian-X healthy | COMPLETE | 0 | 192.4 |
| A3 (Assisted-I) healthy | COMPLETE, failures 0 | - | (200 s window) |
| N1 narrow fixture | DONE; harness status FAILED only by its 30-s BLUE transit limit (zone flown at 33.9 m/s = 35 s) | 0 | 289.9 |
| X narrow fixture | same harness limit | 0 | 244.5 |
| N1 main 50 % | reprofiled 36.77 -> 18.39 | 0 | 269.2 |
| N1 RCS y-nozzle 60 % | reprofiled (rcs 2.0 -> 1.85: y/z projection fix visible) | 0 | 233.3 |
| N1 reactors off 60 s / on 75 s | coast, replan | 0 | 214.0 |
| N1 gas tank 60 % at 30 s | COMPLETE | 0 | 233.2 |
| N1 main 50 % + RCS x-nozzle 70 % | reprofiled | 0 | 270.1 |
| X one main (upper-left) 95 % | main 73.55 -> 7.35, hand-back + replan | 0 | not in window |
| **N1 ER-5 70 %** | **FAILED gate**: published main = physics (11.37 m/s2), hand-back "cannot brake to the speeds ahead", on the new plan the safety monitor went UNRECOVERABLE in a curve (t 135 s, s 22.6 km), max xtrack 1034 m, recovered at 170 s, no DONE | 3 | - |
| **A3 engine L lost** | **FAILED**: "working thrust axis mismatch" (server vs client axis dot 0.65) blocks replanning, then "authoritative motor telemetry lost during docking" | - | - |
- OPEN (from the gate): N1 ER-5 70 % - root cause not isolated (replan
  on 31 % main enters a curve it cannot hold); X: one lost main of four
  leaves only 10 % working forward (torque-neutral ray?) - verify; A3: the
  server / client working-axis disagreement after an engine loss (client
  doctrine override in MotherComputer) - Assisted damage NOT accepted.

## Next step
- Fix the two failed damage gates above (N1 ER-5 70 %, A3 engine loss),
  then re-run acceptance.sh. Then user review: Galaxy Map (v4 clouds, icy fields), System Map. Then the
  separate stage "interstellar navigation" (ephemeris, gravity, domain,
  InterstellarNavigationEnvironment hazards, resources, jump logic).
