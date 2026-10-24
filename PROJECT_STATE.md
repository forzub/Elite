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

## 2026-10-24 power-budget safety patch — Windows gate PASSED

- **Power-budget fail-closed, native build and focused CTest PASS:**
  `a8a57790` makes installed/native `ShipCore` pass its explicit
  `powerGroups` pointer even when the list is empty; diagnostic-only
  callers retain the optional/null budget path.
  `b5f293e2` makes the installed physical execution gateway reject an
  explicitly present but EMPTY power-group vector instead of interpreting
  it as unlimited electricity; demand does not become executed force.
  Existing per-motor missing-source zero-MW groups remain in place.
- `33371d85` extends the EXISTING
  `ship_control_execution_gateway` test with a real mounted Newtonian-I
  main-engine fixture: diagnostic null budget can execute, but an
  explicitly empty installed power budget MUST deliver zero force/torque
  while retaining the request. No new test target or architecture layer.
- **User native Windows verification on 2026-10-24:** after pulling
  current main, `bash build_mingw64.sh` linked both `EliteGame.exe`
  and `EliteServer.exe`. The existing focused
  `ship_control_execution_gateway_tests` target compiled and CTest
  `ship_control_execution_gateway` passed 1/1 (0 failures, 0.05s).
  CMake deprecation and unused-variable warnings were non-fatal.
  This latest gate checks only ONE selected test; the earlier equipment
  and attitude tests passed on the previous revision, not re-run here.
  **In-game acceptance is still outstanding.**

## 2026-10-24 in-game ship-control oracle runaway (new source fix)

- User supplied `ship_control_oracle_matrix(4).log`,
  `elite_attitude_jitter(10).csv` and
  `elite_equipment_failures.log`. In the first Assisted-I model,
  steps 0..12 passed (forward, coast, strafe/release, lift/release,
  pitch/yaw/roll and release); step 13 `family_vectoring_probe`
  held `strafeInput=1` indefinitely. Body-right speed reached
  ~500 m/s; no step-13 PASS or transition to another model.
- Confirmed oracle bug: its Assisted step 13 expected the obsolete
  intermediate `assistedRightAccelerationMps2` to be nonzero, while
  the installed physical executor delivers the lateral demand through
  `mainEngineAccelerationMps2` and zeroes the intermediate assisted
  channel. Logged executed right acceleration was positive.
- `c3937c04` (SOURCE ONLY): step 13 checks EXECUTED
  `mainRightAccelerationMps2>=0.10` and a +1m/s body-right
  velocity increment. If step 13 is still unresolved at 100m/s,
  log FAIL and stop commands instead of unlimited acceleration.
  No physical law/engine allocator/production control changed.
- The attitude CSV columns labeled `linear_body_ax/ay/az` contain
  WORLD vector `motion.mainEngineAccelerationMps2`, a misleading
  diagnostic header, not proof of swapped body axes. The separate
  `elite_equipment_failures.log` shows toggles/restores only.
- NEW Windows gate needed: pull main, run `bash build_mingw64.sh`,
  launch `./build/EliteGame.exe --diagnostic-scenario ship-control`
  and inspect new oracle log. This result is NOT yet Windows-built
  or gameplay validated. Previous power-budget CTest success remains
  limited to its earlier revision.

## 2026-10-24 six-model ship-control oracle PASSED (user Windows runtime)

- User supplied `ship_control_oracle_matrix(5).log` and
  `elite_attitude_jitter(20261024-174554).csv` after rebuilding oracle
  fix `c3937c04`. **Real in-game six-model diagnostic COMPLETE: 6/6
  models, 84/84 steps PASS, 0 FAIL.** Assisted I/Y/X and Newtonian I/Y/X
  each completed all 14 steps, including their final vectoring probe.
  Last log: `SUMMARY status=COMPLETE models=6`. Previous endless lateral
  acceleration was an oracle predicate bug; this run did not reproduce it.
- Telemetry CSV: 4501 records across 90s; max observed speed ~23.65m/s
  (no 500m/s runaway). Higher yaw rate under Newtonian setup/spin during
  the scripted probe does NOT validate or invalidate ordinary autopilot.
  This proves the scripted basic control oracle, NOT damage handling,
  reactor-off/restart acceptance or route-following under failures.
- UX omission: completion previously only printed one stderr line while
  the graphical client offered no visible end-of-test signal.
  `17339f8c` adds one-shot topmost Win32 result dialog, announcing
  PASSED (6 models/84 checks) or FAILED with report location, plus
  explicit stderr PASSED/FAILED. This source change is **NOT yet
  Windows-built/tested**. Existing log is from before dialog change.
- Next user action if verifying dialog: pull main, build only required
  native target(s) through `bash build_mingw64.sh`, launch
  `./build/EliteGame.exe --diagnostic-scenario ship-control`.
  At completion a modal Windows notification should appear. No changes
  to engine control or physics in this UI fix. After confirming the
  sign, proceed to ordinary damaged-flight/gameplay acceptance.

## 2026-10-24 next focused PHYSICAL execution regression gate

- User authorized all necessary tests to investigate Command -> Request ->
  Executed rather than repeating the already-green 84/84 six-model
  scripted oracle. Added assertions inside the EXISTING
  `tests/navigation_runtime/ShipControlExecutionGatewayTests.cpp`
  (no new framework/target):
  - `dda54318`: empty installed power groups produce no `delta-v`
    after propagation; installed 0 MW reactor yields zero delivered
    electrical demand/force/velocity while a request remains visible;
    full installed reactor produces real motion; 50%-rated reactor
    scales actual force, electrical consumption, and motion together.
  - `dda54318`: near the 400m/s control envelope, integrated
    `delta-v` MUST match physical motor-delivered acceleration times dt.
    The current source still has the confirmed
    `DynamicMotionSystem::updateLocalFrameMotion` POST-execution speed
    envelope, so this last assertion is expected to expose a REAL
    design violation. This is an intentionally strict diagnostic gate;
    do not relabel an observed red result as flaky or alter expected
    results to pass. Limit COMMAND/REQUEST **before** allocating actual
    motors; never silently edit EXECUTED impulse after allocation.
  - `948b2724`: invalid allocator request must not move velocity OR
    position during physical propagation (previous test checked only
    acceleration/torque fields).
- `46c9a93b`: if the speed-boundary invariant is violated, the
  focused test prints `[PHYSICAL_IMPULSE_MISMATCH]` with measured
  initial/final speed, executed/applied delta-v, and numeric error;
  this is diagnostic output only, not a physical fix.
- **These NEW tests have NOT been compiled/run on Windows.** Earlier
  84/84 in-game PASS is from prior code and only proves the basic
  healthy six-model control matrix.
- Test reuse (do not duplicate): existing
  `ship_control_behavior_oracle`,
  `ship_physical_actuator_plant`,
  `newtonian_rcs_physical`,
  `working_thrust_axis_analysis`,
  `hull_attitude_control`,
  `assisted_authority_contract`,
  `navigation_execution_replan_policy`,
  `client_route_autopilot`,
  `engine_thrust_hud_presentation`,
  and `equipment_damage_failure_pipeline` cover control, damage,
  independent RCS, replanning, and presentation.
- Immediate action: pull main and run focused
  `ship_control_execution_gateway` native CTest first. If it reports
  the expected post-execution speed limiter impulse mismatch, fix the
  control/integrator seam without changing accepted healthy assisted
  control law. Then run the existing broader focused suite. Normal
  damaged-flight gameplay remains unverified.

## 2026-10-24 Windows gate feedback: first red was TEST FIXTURE

- User's Windows build of `ship_control_execution_gateway_tests` succeeded,
  but CTest failed at `invalid motor allocator test lacked a real input
  request`; the speed-envelope assertion was NOT reached.
- Root cause checked against `DynamicMotionSystem::applyLocalFrameInput`:
  Assisted manual `forwardInput=1` is expressed through
  `requestedAssistedStabilizationAccelerationMps2`, since the
  longitudinal servo yields its main-forward axis on a held manual
  input. The new test incorrectly required only
  `requestedMainEngineAccelerationMps2` to be nonzero.
- `fc260a7b` corrects the TEST assertion to consider the combined
  requested main + assisted vectoring channels. The physical gameplay
  control law, reactor handling and integrator remain unchanged.
  **Not Windows-verified after this fix.**
- Re-run just `ship_control_execution_gateway` before broader suite.
  Expect any next red to report its real assertion; if
  `[PHYSICAL_IMPULSE_MISMATCH]` appears, it is separate from this
  fixture error and concerns the post-executor controlled-speed cap.

## 2026-10-24 physical-impulse RED CONFIRMED and source-side correction (native gate pending)

- User reran `ship_control_execution_gateway` natively on Windows after
  test-fixture correction `fc260a7b`. The test progressed to the actual
  speed-limit boundary and **FAILED**:
  `[PHYSICAL_IMPULSE_MISMATCH]` speed before 399.9 m/s, after 400;
  installed engines delivered 3.67749 m/s delta-v but integrated ship
  gained only 0.1 m/s; 3.57749 m/s physically executed delta-v was
  silently dropped. This definitively confirms the old post-allocation
  `DynamicMotionSystem::updateLocalFrameMotion` speed projection breaks
  Command->Request->Executed. All preceding tests in that executable
  reached/passed their checks in this run. This is NOT a failed engine
  geometry test.
- Minimal targeted implementation on `main` (not yet rebuilt on Windows):
  - `f8778dea`, `1307f690`: expose and reuse the exact pre-existing
    controlled-speed request projection via
    `DynamicMotionSystem::limitRequestedPropulsionToControlledSpeed`;
    for **installed main motors** integrate the actual executed
    acceleration unmodified rather than silently clipping its delta-v
    a second time. Keep legacy/no-mounted diagnostic behavior unchanged.
  - `e1e0015e`, `26484e91`: one-fixed-step provenance flag
    `installedMainPlantExecuted` on `DynamicMotionState`, reset after
    propagation; record truthful applied-acceleration semantics.
  - `90cd51c2`: installed physical gateway now bounds its combined
    manual/Assisted main translation demand in the travel frame BEFORE
    sending it to the physical motor matrix. Original raw
    `requested*` intent remains diagnostically visible. Newtonian
    independent RCS input is excluded from the main speed envelope.
    Actual motor force, torque, electricity and heat remain solely
    allocator-/reactor-executed values.
  - `aa2ed4ac`: clear one-tick installed-main provenance even if
    `ShipCore` takes the fail-closed missing-motor path.
  - `3630d396`: strengthen the existing gateway regression to prove
    request is bounded before installed allocation, delta-v equals
    delivered acceleration*dt even at 400 m/s, cap is approximately
    preserved, and provenance clears after propagation.
- **NO native build/CTest result yet on the correction.** Request
  rerun of `ship_control_execution_gateway` first, then the already
  registered control, damaged-motor, reactor/RCS, planner/follower/HUD
  focused gates. Do not repeat 84-step in-game oracle just to check
  its result window. Warnings `unused coolant/wasteMW` are unrelated
  to the confirmed failed assertion.
- Remaining independent scope: low-speed `BrakeToStop` epsilon snap and
  legacy manoeuvre gas integration are not yet comprehensively audited
  against the installed physical RCS. Do NOT conflate this specifically
  confirmed main-engine speed-boundary bug with full game acceptance.

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

## Immediate next validation: visual completion signal, THEN normal game

The reactor budget patch passed focused Windows CTest; the
corrected oracle also passed all 84 steps across all six models in
the user's native game. NEW result-dialog patch `17339f8c` is
source-only and awaits Windows rebuild to verify the one-time
foreground PASSED/FAILED window. Do not reclassify the 84-step
oracle as damaged-flight, full autopilot or reactor-off acceptance.

## Normal gameplay plan (still unverified)

The post-change native Windows build and focused
`ship_control_execution_gateway` CTest are GREEN.
The three-test gate remains recorded separately for the prior revision.
Next run gameplay on the newly built client/server: healthy Assisted-I ordinary
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

The commands above are repeatability instructions for later source
changes; the current focused power-budget safety gate already PASSED.
Keep any gameplay mismatch logs in `D:/tmp`. Do not label the gameplay
checks as passed until the user reports their results; then proceed
incrementally to Command/Request/Executed and fore-only/signed-capability
work without regressing healthy flight.
