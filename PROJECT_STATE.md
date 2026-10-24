# ELITE — PROJECT STATE (single canonical continuation document)
Updated: 2026-10-24, Europe/Kyiv. Repository: `forzub/Elite`. Branch: **main ONLY**.

## Hard rules

- Keep this ONE file as the only active continuation state. Do not create
  alternative status/state/notes files or side branches. Historical details
  remain recoverable in git history; do not append whole earlier state text.
- Work in small source-grounded steps; avoid tests-for-tests, duplicating
  physical laws, unverified native-test claims and unrelated changes.
- User Windows: MSYS2 MinGW64, CMake/Ninja; diagnostics and CSV in `D:/tmp`.
  Native tests run on user's Windows machine, not in GitHub source edits.
- **Recovery checkpoint:** commit
  `44bbbcf829763bc1c86e41ab3cbf46cf17896308` is an EMPTY commit on
  main pointing to the exact pre-Claude-cleanup tree of `590eb24a`.
  To compare/rollback use this SHA; never rewrite the branch or erase it.

## Architecture — current and accepted

- SERVER is the only physically authoritative world: equipment and installed
  main/RCS motors, damage, reactor power, heat, real F and r×F, mass/inertia,
  rigid-body motion, NPCs/collisions and physics timestamps.
- CLIENT receives authoritative snapshots and telemetry, uses interpolation
  and kinematic extrapolation for smooth views; NO second motor/force/torque
  physics, including fractional render frames. Client pilot and autopilot send
  the SAME numbered `ShipControlState` commands. Autopilot substitutes pilot;
  it may not bypass real installed actuators or invent acceleration.
- `ObjectModuleRuntime` is the authoritative equipment source; equipment
  accumulates 5+ damage causes. `HitVolume.health` separately represents
  geometric/structural hit volumes (not a rival engine capability).
  `ShipCore::capabilities()` + current mass computes the installed
  `ShipCapabilitySnapshot`; `ShipParams` is its transitional adapter.
  `AssistedManeuverPrograms` is one installed Assisted motor allocator;
  Newtonian has independent main/TVC and eight RCS-node hardware. The
  `ShipPhysicalControlModels` approximate slot profiles remain only as
  diagnostic/compatibility adapters, NEVER a production engine source.
- Assisted I=4, Y=6, X=8 main motors; Newtonian aft main + 8 RCS clusters.
  Main nozzles gimbal under mechanical limits; engine damage and reactor
  readiness can remove/limit authority. Working-thrust axis may deviate
  from body -Z; for a damaged Assisted-I single aft survivor the known
  conservative ray is ~49.5 degrees off nose, through current CoM.
- Body basis: +X right, +Y up, -Z nose; manual keypad Num8/2 moves
  relative to nose, Num4/6 right/left, Num9/3 up/down, even when
  emergency main working ray differs. `+` Assisted normal propulsion
  follows available working axis. `Backspace` allows independent nose
  rotation; the actual executed force remains physically constrained.
  HUD cyan = nose, amber = thrust ray, green V = actual local flight path.
- Route: visible corridor, 500m nominal / 250m near final; Assisted
  first straight + tangent arc + last straight, lead-turn, stop before
  broken corner. Physical capability (not planner wishes) constrains speed
  and cornering; emergency preplanning safe-course must complete before
  a route is admitted. Newtonian has distinct flip/brake and curve behavior.
- Normal healthy Assisted pitch/yaw and roll damped control response 0.5s
  accepted by user's game flight. Do not retune it without new evidence.
  Engine heat laws: canonical propulsion pad cooling 6 MW/m² with actual
  annulus area/integrity; no second radiator/coolant heat-sink in main
  engine thrust governor. Reactors have real readiness and 10s restart;
  Ctrl+Alt+Shift+I toggles reactors. Ctrl+Alt+Shift+F toggles an injected
  DIAGNOSTIC internal engine failure, not real impact/wear or reactor power.

## Latest verified gameplay / last user Windows gate

- User's 2026-10-24 healthy Assisted-I gameplay after roll damping was
  subjectively smooth. `elite_attitude_jitter(8).csv`: 4501 server
  samples/90s; no full-scale roll command; max |roll cmd|~0.0153,
  p95 |roll rate|~0.0354rad/s vs previous 0.405rad/s; max
  |roll rate|~0.0368rad/s vs old 0.5275rad/s. No roll limit cycle.
  Healthy initial yaw-capture momentary 0.936 command but no lasting
  divergence; 400m/s speed plateau briefly reached ~405.63m/s.
- Damaged Assisted-I test: with one aft engine out, server/client axis
  matched, valid route planned and eventually adopted (~40.8s), mandatory
  blue corridor entered and exited, no stress emergency stop; transient
  early cross-track ~205m then converged to ~8.94m at scenario end.
  This is test-scenario completion, NOT confirmed docking.
- 2026-10-24 user's Windows MSYS2 MinGW64 gate: **PASSED**.
  `EliteGame.exe` and `EliteServer.exe` linked successfully.
  Focused CTest: `equipment_damage_failure_pipeline` 1/1,
  `hull_attitude_control` 1/1,
  `ship_control_execution_gateway` 1/1 — all PASS.
  This is THREE selected tests, not evidence that the entire test suite
  passed. Compiler warnings remain (winsock header order, unused
  coolant/overflow variables) but did not block builds.
- Prior `equipment_damage_failure_pipeline` failures were fixture/law
  mismatches resolved in sources `fa688f92`, `4607184e`,
  `e7651828`, `6645d47a`. The Claude cleanup has now cleared
  the focused native build/test gate, but **post-cleanup GAMEPLAY is
  not yet verified**. Do not promote source-only claims to flight PASS.

## Claude 2026-10-24 review — source check and accepted corrections

Claude's `Вставленный текст(20261024-105726).txt` is a review of a
PARTIAL code bundle; it explicitly lacked server/client, original
capability builder, gateway, runtime, telemetry and HUD files. Its concrete
findings were independently checked against current main, not blindly
adopted. Conclusions:

1. **Verified N1 / fixed:** absent fore bank used to leave
   `reverseMainEngineAvailable=true` while `reverse...Acceleration=0`,
   allowing legacy `0 → full load` fall-through. `ba02087b` clears flag;
   `7fd0a605` adds existing equipment contract for zero reverse authority.
   Aft-bank zero authority was already properly disabled in builder.
2. **Invalid plant fail-open / fixed:** `d13f358e` clears request force,
   stabilization and motor outputs on failed physical or diagnostic
   allocation; `40a5cbb0` adds gateway regression.
3. **Unbound real equipment / fixed:** `52df4fee` prevents native
   production missing-motor binding from falling back to synthetic force.
   Each unpowered installed main slot receives explicit 0MW power group.
   Genuine diagnostic Ctrl+F9 I/Y/X construction retains synthetic proof
   sources without contaminating real ship equipment.
4. **Invalid capability / fixed:** `bc9a6df4` makes
   `ShipCore::effectivePhysics()` return a zero-authority adapter,
   rather than default `ShipParams{}` that could invent main thrust.
5. **Stale motor cache / fixed:** `07b374d5` matches the actual
   `assistedWorkingThrustAxisBody` in addition to motor index/flag,
   engine availability, CoM, geometry and mass.
6. **Request versus delivered / in progress:** `22f8ede3` adds
   `requestedMainEngineAccelerationMps2`,
   `requestedManoeuvreAccelerationMps2`,
   `requestedAssistedStabilizationAccelerationMps2` in motion state.
   `2e059cdc` captures them before allocator; `789a839f` records
   even when native engine binding fails. The integrator currently reads
   legacy force channels AFTER gateway overwrites them with ACTUAL
   executed acceleration. Complete typed model migration remains open
   and requires native gate; do not duplicate an engine solver.
7. **Partial angular failure / fixed:** `1fa12700` lets a working
   pitch/yaw control channel remain usable if roll authority is lost,
   and vice versa; `d9e01710` exercises both. Healthy both-present
   control is unchanged.
8. **Reactor readiness / fixed:** `94ae3004` returns invalid
   `ShipControlAuthorityTelemetry` when reactors are off or restarting.
   Physical engine execution still uses per-group real MW.
9. **Dead code / removed:** `4910962d`, `ebce9513` remove unread
   `m_preparedManeuverActuatorRevision`; `600d8e58` removes
   unused duplicate `m_params`. `ad2f150e` removes client adapter's
   dead `survivingAft` count without changing the physical ray.
10. **Not actually missing:** source confirmation: `ShipCoreStatus`
    already replicates `propulsionReactorsOn`, restart seconds, switch
    revision and per-reactor statuses via `WireDataSchema`. Client motor
    simulator has already been removed and MUST NOT be restored. Only one
    installed Assisted allocator and one geometry-based ray computation
    active for production.

## Important remaining architecture work (do not rush)

- Fore-only propulsion (`aft` completely dead, `fore` alive) is
  inconsistent: manual `DynamicMotionSystem::applyLocalFrameInput` chooses
  -nose as travel forward, while `FlightControlProfile`/MotherComputer
  demand safe AFT working-forward thrust, currently reject this doctrine.
  Needs coherent signed forward/backward banks AND flip-and-burn control;
  an isolated sign flip or changing axis label is unsafe.
- Full typed Command → Request → Executed separation. No motion integration
  of unexecuted demand under ANY invalid allocator. Existing speed-envelope
  post-processing still alters physically delivered main acceleration;
  must decide whether to enforce speed BEFORE motor execution, without
  regressing controlled-speed gameplay or Newtonian RCS behavior.
- `ShipCapabilitySnapshot` already has signed axes, but legacy adapters
  collapse some axes to scalar `strafeAccel/angularAccel`, losing available
  channels under asymmetric failure. Do not scrap accepted healthy
  Assisted controls; introduce signed per-axis controllers incrementally.
- `ShipCore::capabilities()` caches construction/actuator/mass revisions
  but live engine power and temperature can affect actual execution at
  higher frequency. `controlAuthorityTelemetry()` currently derives
  signed Assisted motor programs; reactor on/off gate exists, partial
  per-reactor MW/brownout/slew envelope needs grounded design, not a
  second power model.
- Conservative motor program cache `matches()` uses 0.5%-point motor
  capacity and 0.005rad cone tolerances; need measure planner-vs-delivered
  accuracy before replacing with per-tick heavy inversion.
- Do not move per-reactor source-of-truth into a second owner, or delete
  builders/SharedShipPhysics on the assumption that all are duplicates.
  Source-executable tests and preserved local flight experience outrank
  architectural rebranding.
- UI/HUD telemetry `ShipCoreStatus`, `EngineLiveTelemetry` and current
  motor capability are distinct scopes; one same-tick factual provenance
  would be useful but should not be a parallel simulation or new owner.

## Immediate next validation: IN-GAME ACCEPTANCE

The 2026-10-24 user Windows build and three focused tests are GREEN.
Next run gameplay on the built client/server: healthy Assisted-I ordinary
flight and yaw/pitch/roll (no regression); simulated single aft engine
fault with Ctrl+Alt+Shift+F, working-thrust ray, reversal and route capture;
reactors off/restart with Ctrl+Alt+Shift+I (no free acceleration when power
unavailable); no acceleration on missing motor/invalid plant. If a failure
occurs, collect targeted D:/tmp logs and compare actual vs requested.
Do not begin fore-only or broad capability migration before this gate.

Commands for repeating the targeted Windows gate after a later code change:

```bash
cd /d/__elite/work
git pull --ff-only origin main
bash build_mingw64.sh
cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/architecture_contracts --target equipment_damage_failure_pipeline_tests -j 8
cmake --build build/tests/navigation_runtime --target ship_control_execution_gateway_tests hull_attitude_control_tests -j 8
ctest --test-dir build/tests/architecture_contracts -R '^equipment_damage_failure_pipeline$' --output-on-failure
ctest --test-dir build/tests/navigation_runtime -R '^(ship_control_execution_gateway|hull_attitude_control)$' --output-on-failure
```

The commands above are repeatability instructions, not outstanding
actions for the current green revision. Only in-game behavior remains
unverified at this point.
