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

## 2026-10-24 native build gate: missing ShipDynamics include fixed

- User's Windows `cmake --build ... --target ship_control_execution_gateway_tests`
  stopped in `ShipControlExecutionGateway.cpp` with
  `'controlledSpeedLimitMps' is not a member of 'game::ship'`:
  new request-side speed limit called the helper but lacked its
  declaring `src/game/ship/core/ShipDynamics.h` include.
- `b9c87705` adds only that include to the existing gateway source
  on `main`. **Not compiled/CTested after this repair.**
- The subsequent user `ctest` command executed despite Ninja's
  failure: `ctest` ran the **previous binary**, so its old
  `[PHYSICAL_IMPULSE_MISMATCH] 3.57749 m/s` is STALE output,
  not evidence that the new source implementation fails.
- **Windows native gateway gate GREEN (user-reported, 2026-10-24):**
  after include correction `b9c87705`, the user rebuilt and reports
  all checks green. `ship_control_execution_gateway` is now PASS,
  including reactor-off, half-power, rejected allocator and the
  near-speed-limit actual impulse equality regression. No full user log
  was provided in this message, so record this as user-confirmed
  targeted gate rather than a full suite or gameplay PASS.
- The previous `[PHYSICAL_IMPULSE_MISMATCH]` was before source correction;
  the later duplicate output was from the old binary after failed build.
  The latest confirmed native test resolves that specific regression.
  No additional main-propulsion code change is required without new
  evidence.

## 2026-10-24 route-start direction after obstacle avoidance (source fix, native gate pending)

- User observed the craft turning away from an obstacle while the
  planner's first tunnel leg pointed back along the obstacle-facing
  direction, including concern about nonzero-speed planning.
- Source-confirmed mismatch in `src/game/SpaceState.cpp`:
  initial automatic `RoutePlanRequest.initialForward` is assigned by
  `DockingRouteEntryPolicy::initialCourse(startVelocity, hullForward)`
  BEFORE `PreplanningSafetyCourse::evaluate`. This policy is
  CORRECT for the initial *hazard probe*: >0.5m/s uses real velocity,
  at rest uses hull nose. But when Assisted selects an alternative
  safe course and begins physical acquisition in parallel with planning,
  the request's initialForward was NOT updated. The expensive planner
  therefore retained the rejected unsafe tangent, and
  `compileAgent` in `planningWhileSecuringCourse` also predicted the
  wrong course. Newtonian branch already propagated its safeCourse
  explicitly, revealing the Assisted-only omission.
- `1bc9742f` fixes exactly that in the Assisted selected+planning path:
  the *same selected collision-free inertial direction* drives physical
  safe-course acquisition AND `request.initialForward` before any
  workers, route curves, or compiled autopilot are created. Retains
  measured `initialSpeedMps`; does not point the route at the hull
  when moving, invent endpoint/time/position, or restart expensive
  planning. Physical acquisition still completes before route
  adoption. Existing one-shot projected late entry binder obtains
  position and velocity from live ship after turning.
  Logs `[AssistedSafeCourse] phase=selected+planning` now show
  `course`, `route_initial_course`, and `initial_speed_mps`.
- `898f05d2` and `fea6d17e` extend existing
  `preplanning_safety_course` CTest (no new test target):
  blocked START FROM REST must probe hull nose and select a different
  safe tangent; at 100m/s and a nonaligned hull, probe must use actual
  velocity, preserve speed, select new safe tangent. Both selected
  tangents feed actual `RoutePlanner::plan`, and the first route curve
  must point along selected safe direction, not rejected original.
  The planner test route is deliberately obstacle-free after
  selection to isolate tangent handoff from collision/terminal geometry.
- **SOURCE ONLY.** No new Windows compilation, CTest, or in-game
  visual/manual acceptance since this fix. Next targeted gate:
  `preplanning_safety_course`, `route_planner_api`,
  `route_entry_binder`, `assisted_safe_course_executor_isolation`,
  and `client_route_autopilot` in existing navigation runtime tests,
  then build and fly a blocked entry both at rest and moving.
  Compare safe-course selected direction, first route segment,
  live ship velocity, and projected connector; physically verified
  blocking/collision avoidance remains required. Do not repeat
  the 84/84 general control oracle to test this planner seam.
- No changes to Newtonian safe course, physics, engine solvers,
  physical speed limiter, or route optimizer. Potential further
  issue if user still observes trouble: old snapshot origin is used
  for async backbone, but live velocity/position are explicitly
  rebound by projected connector. Diagnose actual binder logs before
  modifying that established mechanism.

## 2026-10-24 route-start targeted Windows CTest: 3/5 PASS; two test-contract corrections

- User ran 5 selected navigation runtime CTests successfully after compiling
  updated `SpaceState.cpp` and `PreplanningSafetyCourseTests.cpp`.
  PASSED: `route_entry_binder`, `assisted_safe_course_executor_isolation`,
  `preplanning_safety_course` (including blocked-at-rest and 100m/s
  safe-direction first-tangent assertions).
- `route_planner_api` FAILED on diagnostic text assertion
  `"continuous-assisted ready"` although the continuous plan had
  already passed `optimized.valid()`. Current production text is
  `"continuous-assisted preset=STANDARD ready curves=..."` with
  `execution_gates=` and `evals=` fields. Commit `c3ef02a3` updates
  ONLY the existing test to check that actual diagnostic schema;
  no route planning geometry changed.
- `client_route_autopilot` FAILED on pre-existing test
  `testAlignedVelocityIgnoresMisalignedNose`: it demanded pitch/yaw
  ZERO even though the deliberately 30-degree-off-nose healthy Assisted
  craft must realign its PHYSICAL thrust axis to the already correct
  VREL course. `Autopilot::Output.courseErrorRad` correctly stayed
  zero (velocity is navigation truth); `AssistedAutopilotKernel::make`
  explicitly computes independent desired hull pose from actual course.
  Commit `a097940f` corrects the EXISTING test: navigation course
  must remain zero-error; hull actuator error must be present; ordinary
  pitch/yaw rotates the actual physical working axis without inventing
  lateral-translation or direct navigation acceleration channels.
  This matches current healthy Assisted doctrine; no control law modified.
- These TWO test changes are source-only until a new Windows CTest run.
  **Do not report the previous 3/5 as 5/5 green**. Rebuild these two test
  targets and rerun the 5-test selector. If green, proceed to normal game
  build and visually inspect safe-route first leg both stationary and moving;
  selected safe vector and tunnel-first-vector must match, and physical
  late-entry binder must join the actual post-turn ship state.
- User's initial observed unsafe tunnel starts were addressed by
  `1bc9742f`: that gameplay patch has NOT yet been user-validated
  in an actual blocked-flight run; unit-level selector/binder PASS alone
  is not the final evidence.

## 2026-10-25 Windows targeted planner gate: 4/5, remaining off-corridor capture test

- User rebuilt/reran five tests after `c3ef02a3` and `a097940f`.
  PASS: `preplanning_safety_course`,
  `assisted_safe_course_executor_isolation`, `route_entry_binder`,
  `route_planner_api`. FAIL: `client_route_autopilot` on
  `parallel-offset state did not strongly rotate back toward center`.
  The earlier misaligned-nose test now PASSED; the failure happened later.
- Checked actual source: that off-corridor scenario puts the craft at
  (+100,0,+80) m on a +X route with velocity and hull forward +X.
  It already successfully requires large cross-track error, nonzero
  signed capture-angle and desired capture angular rate, and navigation
  course error >0.05. Old assertion additionally demanded normalized
  pitch/yaw input magnitude >0.5, contradicting the accepted
  `AssistedAutopilot` 10-degree MAX steering lead and 0.50-second
  `HullAttitudeControl::dampedPlanarInput`. This does not prove failure
  to steer; an instantaneous high-torque threshold isn't valid evidence.
- `4f214ea4` changes only `ClientRouteAutopilotTests.cpp`:
  require active emergency course recovery; require POSITIVE yaw
  (body +Y turns +X forward toward -Z/route center from +Z offset);
  require physically damped, nonsaturated pitch/yaw magnitude <0.5;
  require no bypass via direct navigation acceleration/velocity.
  This checks steering *direction*, *control-law bound*, and authority
  provenance instead of merely weakening a numeric threshold.
- **SOURCE ONLY until next Windows run.** First rebuild and run
  `client_route_autopilot_tests` on current `main`; if PASS, repeat
  the same five targeted checks before real-game Assisted blocked-route
  visual validation at rest and 100m/s. Do not claim full route/game
  acceptance from module tests; user-observed tunnel direction remains
  pending. No navigation/flight-law source was changed by this test fix.

## 2026-10-25 focused client autopilot CTest: next curved-tangent assertion (native gate pending)

- User reran `client_route_autopilot` on Windows after
  `4f214ea4`. The earlier off-corridor capture case now passed; the
  test next FAILED on
  `on-center curved route did not actuate strongly enough to rotate
  velocity into the bend`.
- Inspected `testVelocityCourseTracksAuthoredTunnelTangent`: the
  accepted quarter-circle test route has radius 1000 m and cruise
  speed 60 m/s; probe is exactly on the curve at 22.5 degrees while
  its 60 m/s velocity and hull still point +X. Velocity course must
  turn +X toward +Z (body yaw about -Y) with required angular rate
  v/R = 0.06 rad/s. The code ALREADY detected a substantial genuine
  velocity-course error >0.20 rad; the old next assertion merely
  demanded a normalized hull pitch/yaw command >0.5, inconsistent
  with the current accepted max 10-degree Assisted hull lead and
  critically damped 0.50-second attitude servo. This does NOT prove
  the current physical turn is too weak; a single command-magnitude
  threshold does not express physical v/R.
- `9d34c555` updates ONLY the existing test's old threshold:
  require CORRECT signed yaw (-Y) and that the angular velocity
  change implied by real configured angular authority over the
  already accepted 0.50 s hull response horizon exceeds 0.06 rad/s
  for the specified route. Also reject direct-navigation-demand
  shortcuts. On failure a new numeric
  `[ARC_ACTUATOR_INSUFFICIENT]` diagnostic prints commanded yaw,
  available projected yaw-rate, required v/R, course error, speed,
  radius to make further diagnosis finite and actionable.
- **NOT Windows-built/tested after `9d34c555`**. Rebuild/rerun
  `client_route_autopilot` ONLY; if green, all five selected checks
  from the previous 4/5 gate are green on same planner implementation.
  Then build client/server and physically verify obstruction escape
  and correct start tangent at rest AND at ~100m/s. Do not confuse
  passing controller CTest with gameplay acceptance.
- The obstacle-route START mismatch source fix is still
  `1bc9742f`; no changes to route optimizer, installed motor laws,
  Assisted controller, or Newtonian in this corrective test step.
- Avoid further generic input-magnitude tests for physically damped
  controllers. Validate error direction, signed physical authority
  and bounded dynamics instead. Retain `PROJECT_STATE.md` as the
  only state artifact and work on main only.

## 2026-10-25 gameplay visual acceptance and damaged Assisted keypad axis (code/log audited)

- User flew the revised obstacle-entry/route tunnel and reports the
  geometry is **visually excellent**. This is user-observed visual PASS
  for the originally reported unsafe first-tangent mismatch, not a
  full physical docking/damaged-autopilot acceptance.
- User then asked which axes gray numpad manoeuvre commands use after an
  engine fails. Source-audited `PlayerInputMapper.cpp` and
  `DynamicMotionSystem::applyLocalFrameInput`: Numpad 8/2 request
  forward/back along current HULL nose, 6/4 right/left, 9/3 up/down.
  In Assisted free-rotation mode these requests are transformed through
  current ship `forward/right/up` basis; they are NOT rotated into the
  damaged working-thrust direction. This is a 3-D TRANSLATION request;
  separate pitch/yaw/roll requests remain body-fixed rotations.
- Assisted has NO separate ordinary RCS units: its keypad linear demands
  and hull torque are physically allocated to the surviving installed
  vectoring main motors in `ShipPresetEngineManeuvers` via the execution
  gateway. Torque is computed about the real current center of mass by
  `r cross F`. Damaged installations may have reduced or zero feasible
  translation in particular body directions; no fake side force/spin
  is permitted. Newtonian is different: its 8 physical RCS clusters
  handle body-axis manoeuvre translation separately from main engine.
- User-provided `elite_equipment_failures(1).log` reports
  `Assisted-I slot=0 ship_engine_L target=1.0 status=APPLIED`.
  The supplied `elite_attitude_jitter(20261025-065531).csv`
  has 4501 samples; all show damaged-axis emergency active,
  `axis_body=(-0.76049403,0,-0.64934492)`,
  `axis_offset_deg=49.50776982`,
  `axis_motor_index=1`, motor0 thrust always 0.
  That working thrust ray is 49.51 degrees toward hull -X from
  nose -Z, through CoM. It serves main propulsion, not the numpad
  reference frame. The CSV lacks numpad command channels and DOES NOT
  prove that individual keypad commands were physically executed
  in all directions. Test with deliberate single-key inputs if needed.
- No source-code changes were required for this question. Do not
  conflate thrust-axis fallback with reorienting cockpit/manual body
  controls or claim six-axis authority survives intact after failure.

## 2026-10-25 source audit: physical keypad manoeuvres after Assisted motor failure

- User specifically clarified: not whether NumPad axes follow the emergency
  working-thrust ray, but whether **physical** force and moment are actually
  applied when a damaged Assisted craft executes body-axis NumPad controls.
- Confirmed end-to-end real installed free-rotation chain:
  `PlayerInputMapper.cpp`: Num 8/2 -> `forwardInput`, 6/4 ->
  `strafeInput`, 9/3 -> `liftInput`.
  `DynamicMotionSystem::applyLocalFrameInput` with
  `motion.assistedFreeRotation`: projects inputs on the CURRENT HULL
  `f/r/u`, writes a requested `mainEngineAccelerationMps2`,
  EXPLICITLY zeros `manoeuvreAccelerationMps2` and
  `assistedStabilizationAccelerationMps2`.
  `ShipCore::updateTranslationControl` binds actual installed
  `MountedEngine` instances through `ObjectModuleRuntime` damage,
  health, operating state, electrical reactor budget.
  `ShipControlExecutionGateway::evaluateTranslationControl` converts
  body request into `EngineMatrixDemand`,
  `AssistedManeuverPrograms::execute` allocates six-axis wrench
  using current damaged motor/passport/CoM/cone, physically executes
  each `Engine`, calculates ΣF/m and Σ((r-COM)×F)/inertia.
  `DynamicMotionSystem::updateLocalFrameMotion` integrates EXECUTED
  acceleration when `installedMainPlantExecuted`, not the originally
  desired command. This is not a synthetic RCS path.
- A single failed motor DOES NOT prove that every independent body-axis
  pure translation remains feasible. Available 3D translations and
  rotations depend on remaining motor geometry, power, cones, and coupled
  torque. The preset builder searches a smaller feasible force or leaves
  its achievable value zero. A specific NumPad experiment/physical
  allocator report is needed to establish authority in all directions.
  User's earlier equipment log confirms Assisted-I left-engine failure,
  not individual NumPad motion outcomes.
- **Distinct confirmed SOURCE BUG in NORMAL (non-free-rotation) Assisted:
  lingering fictitious manoeuvre acceleration.**
  `src/game/navigation/DynamicMotionSystem.cpp` near the end of
  `applyLocalFrameInput`: computes
  `unservedLongitudinalAcceleration` after limiting commanded main
  acceleration, then sets `motion.manoeuvreAccelerationMps2` to a
  nonzero fraction of that unmet demand. But Assisted has NO ordinary
  separate RCS. `ShipControlExecutionGateway.cpp` installed-motor
  execution path sends only requestedMainWorld plus AssistedStabilization
  through real motor matrix, then sets
  `motion.engineAccelerationMps2 = actualMain + manoeuvre`.
  `DynamicMotionSystem::updateLocalFrameMotion` even when
  `motion.installedMainPlantExecuted` then applies actual main PLUS
  `actualManoeuvreLocalAcceleration` (and zeroed stabilization).
  Thus if unserved residual is nonzero, unsupported additional dv
  bypasses installed Engine ΣF/m. A real physics inconsistency!
  The FREE-ROTATION Assisted branch zeros this channel; defect is
  ordinary Assisted longitudinal control under unmet acceleration,
  especially when damaged, not the keypad path under examination.
- This was an audited pre-fix source finding; corrected in the
  2026-10-25 patch below. The earlier historical behavior describes the
  bug, NOT current main. Newtonian independent RCS and Assisted free-
  rotation keypad execution were not modified.
- Continue on main with only canonical `PROJECT_STATE.md`; do not
  conflate a feasible commanded displacement in body frame with
  direction of emergency working-thrust axis.

## 2026-10-25 FIXED on main: eliminate phantom Assisted braking/longitudinal RCS

- **SOURCE FIX `381943f3`:** in
  `src/game/navigation/DynamicMotionSystem.cpp`,
  `applyLocalFrameInput` ordinary Assisted removes the old
  `unservedLongitudinalAcceleration` -> `manoeuvreAccelerationMps2`
  fallback. Explicitly zeros `motion.manoeuvreAccelerationMps2`.
  `motion.engineAccelerationMps2` now combines only the main request
  and the existing vectoring-main-assisted stabilization request.
  Both are sent to installed `MountedEngine` units through the same
  six-axis engine matrix. Inadequate reverse/forward authority MUST
  leave the unserved acceleration unexecuted, not generate fake Δv.
- **TARGETED REGRESSION `a3d94518`:** extends the existing
  `ShipControlExecutionGatewayTests.cpp` with one real-plant case:
  Assisted-I with both fore motors physically disabled, aft surviving,
  ship drifting 20 m/s along its nose, zero-speed setpoint, no pilot
  input, and explicit nonzero legacy manoeuvre limit. It demands
  ZERO synthetic manoeuvre channel; requires the installed engine
  matrix to execute; checks no impossible reverse thrust, and checks
  actual integrated delta-v stays zero. This case would detect the
  old fake RCS fallback, rather than checking a mere source string.
- The FREE-ROTATION Assisted NumPad path is unchanged: its body-axis
  input still requests a physically allocated main-engine six-axis
  wrench. Newtonian RCS, main engines, reactor power, disabled-motor
  state, route geometry and autopilot course law are unchanged.
- **Windows/native verification PENDING.** Run only the existing
  `ship_control_execution_gateway` CTest first, then optionally
  `ship_control_behavior_oracle`, `ship_physical_actuator_plant`,
  `assisted_authority_contract` if first gate passes. Do not claim
  acceptance before the user's build/CTest or real-flight observation.
- This fixes the ordinary manual Assisted residual. Legacy diagnostic
  `applySystemAccelerationDemand` and precision RCS/navigation seams
  remain separate, outside the scope of this patch; investigate only
  with a reproducing production source path. Main-only; no extra
  state/status files.

## 2026-10-25 user Windows green gate and canonical runtime build

- User reported **all green** after the `381943f3`
  Assisted phantom manoeuvre/RCS fix and `a3d94518`
  regression in `ship_control_execution_gateway`. Treat this as
  user-reported native test success; no pasted per-CTest list or full
  project gate was provided. Do not claim broader gameplay acceptance.
- User requested building the game next. Verified against CURRENT
  `build_mingw64.sh` and `tests/helpers/build_layout.sh`
  that the canonical INCREMENTAL client+server build command is
  `cd /d/__elite/work && git pull --ff-only origin main &&
  bash build_mingw64.sh` in MSYS2 MinGW64. It requires no running
  `EliteServer`, checks the toolchain, removes obsolete duplicate
  runtime binary layouts without deleting canonical build trees,
  configures/builds EliteGame then EliteServer and verifies
  `build/EliteGame.exe` and
  `build/headless_server/EliteServer.exe`.
  Do NOT substitute `rebuild_all_mingw64.sh` unless clean rebuild
  is specifically warranted: it deletes the entire build tree.
- The user must run this command on their Windows machine; we have
  not executed or verified the full runtime build here.
- Next in-game validation after successful runtime build: repeat
  ordinary Assisted damage braking (no phantom deceleration), and
  independently check NumPad translation in free-rotation with one
  failed aft engine, correlating actual engine thrust, net force, and
  induced body moment. The safe first-route tunnel was visually
  accepted earlier.

## 2026-10-25 emergency HUD vector markers constrained to ship HUD frame (source-only)

- User confirmed post-physics-fix gameplay appears correct and reported
  a VISUAL issue: the new direction-vector indicators leave the HUD
  frame even though older navigation markers stay inside.
- Root cause in current main: `PlayerShipView` already renders normal
  markers using `hudEdgeMapper.isInsideBoundary/projectDirection`
  against `ShipHudProfile.edgeBoundary.contour`, but
  `FlightVectorIndicatorRenderer::renderEmergencyThrustAxis`
  (amber working-thrust vector) and
  `renderEmergencyVelocityVector` (green true VREL direction)
  independently clipped projected positions to the full viewport
  rectangle using `xMax/yMax = halfScreen - 31`. The viewport
  rectangle is NOT the nonrectangular ship HUD contour.
- **SOURCE PATCH on main, pending native build and visual acceptance:**
  `d2b713d5` adds the canonical `HudEdgeMapper` parameter to
  both emergency sights;
  `4f5eedbc` introduces a shared pure projected-marker placement
  helper using the SAME HUD polygon and ray intersection as the
  existing markers. It finds a valid center INSET by checking a
  16-point perimeter over the full marker radius (24px amber, 23px
  green) along the projected camera ray, so the glyph is not half
  outside the contour at corners. Screen-space marker projection,
  actual camera FOV, emergency working-axis physics and true
  velocity vector are unchanged. Labels use measured Font text width
  and are only drawn in a position whose bounding corners fit inside
  that polygon, never forced outside the frame.
  `b7fb8e36` passes `m_playerView->hudEdgeMapper` from the two
  `SpaceState.cpp` call sites. The invariant is visual containment,
  not a new HUD contour or a new direction.
- Existing healthy boresight/flight instrument and ordinary world
  labels are unchanged. No autopilot, motor, damage or physics code
  was edited.
- Source readback verified exactly two updated call sites matching
  new renderer signatures and HUD mapper included in the SAME
  `EliteGame` CMake compilation unit list; Windows compilation is
  NOT verified here. NEXT: user pulls main, runs
  `bash build_mingw64.sh` in MSYS2 MinGW64, checks amber and green
  emergency sights near all four/slope HUD edges at various
  orientations/camera FOVs; labels must not cross the frame. If
  cockpit profile boundary is absent, the two marks now fail closed
  instead of inventing a full-viewport frame.
- Canonical main-only `PROJECT_STATE.md` remains the sole project
  context; keep this as pending runtime visual acceptance.

## 2026-10-25 SIX-MODEL END-TO-END SOURCE AUDIT — ORIGINAL FINDINGS (superseded by native Newtonian work below)

User requested careful review of input -> flight law -> physical installed
force/moment -> motion -> replicated MotherComputer -> HUD across Assisted
I/Y/X and Newtonian I/Y/X. This is a SOURCE AUDIT of main (not six native
runtime acceptances). DO NOT claim all six are physically verified or
fix these by weakening any green synthetic-oracle tests.

### What is structurally correct

- Shared pilot `PlayerInputMapper` drives body-axis Num 8/2 forward,
  6/4 strafe, 9/3 lift and W/S Q/E A/D attitude; laws are independent.
- Assisted fixed installed topologies: I 2 aft+2 fore, Y 3+3,
  X 4+4; actual installed CR-1 engines and installed ER-5 reactors
  are in the persistent Cobra module graph for all three Assisted
  layouts, switched through `ShipCore::assistedCobraInstalledProfiles`.
  `ShipCore::updateTranslationControl` binds `MountedEngine` from
  `ObjectModuleRuntime`; `AssistedManeuverPrograms` physically resolves
  ΣF/m and Σ(r-COM)×F/I with installed damage/thermal availability and
  real reactor group power. `DynamicMotionSystem` integrates actual
  motor impulse, not requested impulse, on the installed path. The
  former ordinary Assisted ghost `manoeuvreAccelerationMps2` was
  removed in `381943f3`, user reported target Windows CTest green.
- Newtonian actual `newtonianRcsNozzles()` enumerates 8 position nodes
  x 3 distinct fixed-direction engines = 24 nozzle modules.
  `allocateNewtonianRcs` executes actual damaged `MountedEngine`
  samples, computes ΣF and moment, and returns actual nozzle duties.
  Runtime `ShipCore` conditionally uses it for a native installed
  Newtonian construction; it does NOT execute for DiagnosticOverride.
- HUD primary path `SpaceState::render` ->
  `MotherComputerClientAdapter::build...Cockpit` ->
  `buildEngineThrustHudPresentation(cockpit)` ->
  `EngineThrustIndicatorRenderer`. Motor bars use actual
  `engineTelemetry` thrust/angles/availability, ER-5 group
  electricity/temperature status and module BITE; 8 Newtonian RCS
  indicator dots are aggregated from groups of 3 physically reported
  duties when available. The prior emergency thrust/velocity HUD-frame
  constraint is SOURCE FIXED but user visual acceptance is still pending.

### Blocking P0/P1 issues, exact source evidence

1. **P0 actual-versus-diagnostic Newtonian path / missing MAIN bindings.**
   `EliteCobraMk1.cpp` default construction is Assisted-I; its
   `ELITE_SHIP_PHYSICAL_TEST_MODEL=Newtonian-*` branch explicitly
   clears `physicalControl.slotModuleIds` (around line 1160) but
   adds 24 RCS equipment modules (around 1243-1255).
   `ShipState::initialize` sets `ShipActuatorStateSource::ModuleRuntime`;
   `ShipCore::updateTranslationControl` can bind installed MAIN motors
   only when IDs match the selected topology (around 705-755); its
   native `productionInstalled && !physicalEngines` branch (around
   931-948) rejects main propulsion. Conversely, switching to Newtonian
   from the ordinary Assisted Cobra in `selectPhysicalConstructionForDiagnostic`
   selects `DiagnosticOverride`, mounting synthetic main engines and
   skipping real Newtonian RCS. Thus neither route is full native
   engine+RCS end-to-end as a production Newtonian ship. This can make
   HUD projected availability appear healthy while physical thrust is 0.
   DO NOT claim six native equipped ships ready.
2. **P0 unphysical Newtonian fallback on missing 24-nozzle binding.**
   `ShipCore::updateMotionControl` sets `m_newtonianRcsPhysical=false`
   whenever any real nozzle module is absent (around 617-645). When
   that flag is false, angular `ShipController` output is NOT restored
   before physical RCS torque. `ShipCore::updateTranslationControl`
   only substitutes physical RCS force inside
   `if(m_newtonianRcsPhysical)` (around 972), leaving the original
   `DynamicMotionSystem::applyLocalFrameInput` virtual
   `motion.manoeuvreAccelerationMps2` intact otherwise. Installed
   Newtonian missing a nozzle can therefore receive synthetic
   six-axis authority rather than failing closed. The per-nozzle
   "complete=false" branch fails closed, but is unreachable when the
   earlier all-24 check has already disabled the physical path.
3. **P1 physical moment about WRONG origin in native RCS.**
   `NewtonianRcsAllocator.h` uses
   `r=mount.positionBody * demand.mountHalfExtentsM` in BOTH solver
   columns and applied `r×F`; `NewtonianRcsDemand` and
   `ShipCore` supply no `centerOfMassLocalMeters`. The main engine
   plant explicitly subtracts the live CoM. Cargo/fuel CoM shifts
   therefore change the true RCS moment but not this model's output.
4. **P1 Newtonian physical RCS roll sign inconsistency.**
   `ShipController::propagateOrientation` rotates `rollRate` around
   hull FORWARD (-Z); `ShipEngineControlMatrix.h` correctly maps
   physical +Z torque to `-torque.z/rollInertia`. By contrast
   `ShipCore.cpp` ~1020,1048 builds the RCS requested moment.z
   with +roll rate × inertia and applies
   `rollRate += executed.deliveredMomentNm.z / rollInertia * dt`
   without sign flip. The RCS wrench uses +Z aft and so its roll
   angle convention is opposite to actual body torque compared with
   the main engine path. Need a fixed-input/sign-of-torque native test
   before patch, NOT arbitrary pitch/yaw changes.
5. **P1 Newtonian 24 RCS versus legacy 4/6/8 ability estimator.**
   `CobraPropulsionPresets.h` defines Newtonian I/Y/X as 1/3/4 aft
   main slots PLUS 4/6/8 omnidirectional ManoeuvreClusters.
   `NewtonianPhysicalControlModel::evaluate` and
   `ShipCapabilityBuilder::makeCapability` derive scalar RCS acceleration
   and angular authority from that OLD matrix; they do NOT project
   24 individual installed `rcs_fore_*_* / rcs_aft_*_*` engine module
   health, orientation and delivered wrench. MotherComputer/planner
   may advertise ability after actual nozzle damage. In contrast,
   `MotherComputerClientAdapter` and 8 HUD lights consume the 24
   per-nozzle truth. Contradictory authority sources for one craft.
   Legacy `ShipPhysicalControlModelsTests.cpp` assert 4/6/8; these
   tests are not physical RCS acceptance.
6. **P1 Newtonian executed Δv can be discarded AFTER motor firing.**
   `DynamicMotionSystem::updateLocalFrameMotion` manages legacy
   `manoeuvreGasPressure01/manoeuvreGasDepleted` and changes
   `actualManoeuvreWorldAcceleration=0` after the RCS allocator has
   executed real engines; the native Newtonian Cobra nominal profile
   in `EliteCobraMk1.cpp` sets gas use=0.20 and recharge=0.08 per sec.
   `ShipCore` nozzle execution is NOT conditioned on this pressure.
   At depletion, electricity/heat and physical RCS angular torque can
   be registered while translational impulse is artificially removed.
   Move that resource limit BEFORE physical nozzle execution; then
   integrate unmodified actual engine wrench.
7. **P2 electrical budget for RCS lacks per-tick enforcement.**
   `allocateNewtonianRcs` returns per-nozzle electricalMW/heat,
   but `ShipCore::updateTranslationControl` does not route its draw
   through the reactor-power group budget; it only sets each nozzle's
   enabled flag from `m_propulsionReactorsOn` and startup state.
   Main engines ARE scaled to installed ER-5 budgets. The RCS model
   therefore does not yet constrain duty to independently available
   electrical MW. Confirm intended RCS supply (ship-services bus)
   before changing power topology.
8. **P2 HUD distinction for diagnostic Newtonian.**
   Actual primary HUD groups engines by `MotherComputerCockpitView`;
   it renders 8 RCS dots using real 24-nozzle snapshot + executed
   samples when present. For the `DiagnosticOverride` Newtonian law,
   the main engine is synthesized; RCS physics remains the old virtual
   4/6/8 cluster path while the HUD expects 24 installed nozzles.
   Thus HUD dots may be UNKNOWN despite simulated motion. Do not
   portray this mode as a physically installed Newtonian ship.

### Current test coverage LIMITATIONS

- `ShipPhysicalControlModelsTests` confirms six static topologies
  but explicitly asserts legacy Newtonian 4/6/8 clusters.
- `ShipControlBehaviorOracleTests::execute` and
  `AssistedLayoutFlightMatrixTests` call
  `ShipControlExecutionGateway::evaluateTranslationControl` WITHOUT
  `mountedEngines`, so passing their six-mode motion fixtures does
  NOT exercise genuine `ShipCore` ObjectModuleRuntime/power/24 RCS.
- `NewtonianRcsPhysicalTests.cpp` only tests the isolated 24-nozzle
  allocator, one combined wrench, and removal of a nozzle; it does
  NOT test ShipCore installation binding, reversion to virtual path,
  mass-shift CoM, signed roll, gas depletion or power budget.
- `ShipControlExecutionGatewayTests` contains a real mounted-engine
  Assisted regression for no ghost braking; user reported this gate
  green on Windows. No new Windows native or 6-layout in-game tests
  were run during THIS AUDIT.

### Recommended targeted repair sequence, NOT executed

A. Decide the six production hardware contracts once (Assisted I/Y/X
   genuine installed; Newtonian I/Y/X true installed main(s)+24 real
   nozzle modules) and remove the conflicting 4/6/8 RCS fiction from
   production capability/HUD; retain legacy only explicitly in labs.
B. Bind physical Newtonian main module IDs and measured nozzle damage;
   make physically missing RCS FAIL CLOSED, with no model/source fall
   through to synthetic translation or angular velocity.
C. Make RCS 6D wrench CoM-relative and fix the physical roll sign
   consistently at demand and actual response. Gate with real
   sign/CoM Newtonian-I first, then Y/X.
D. Resolve 24-nozzle achievable signed abilities and gas/electrical
   supply constraints BEFORE execution. Never erase delivered motor
   impulse after allocation. Only then align MotherComputer caps/HUD
   with execution and check damage/repair.
E. One actual installed-equipment six-model acceptance matrix: NumPad
   ±XYZ, pitch/yaw/roll, full and partial damage, generator off/restart,
   powered-vs-unpowered RCS, actual ΣF/m and Σ(r-COM)×F/I versus Δv/
   Δω and replicated per-motor/24-nozzle HUD across all six.

User asked for careful REVIEW, not authorization to broadly rewrite
the architecture. No production changes in this audit. Preserve
main-only and ONE canonical PROJECT_STATE.md.

## 2026-10-25 NEWTONIAN I/Y/X native hardware and physical RCS repair — SOURCE CHANGES, WINDOWS GATE PENDING

**User intent:** Newtonian I/Y/X must use REAL main engines and EIGHT
physical RCS mount nodes, EACH with three independently damageable,
fixed-direction electric nozzles; current main is the only active branch.
Preserve the gameplay replenished manoeuvre gas (limit marathon RCS use)
and its 0.20/s consumption, 0.08/s recharge, 0.20 restart fraction.
The gas is a control/resource budget, not a requirement to simulate
propellant mass. The healthy Assisted I/Y/X behavior, engine efficiency,
heat and damage were deliberately NOT retuned here.

### Source patches actually committed to main

- `2edb1e18`: `NewtonianRcsAllocator.h` uses live
  `centerOfMassLocalMeters` to calculate both solver and executed
  Σ(r-COM)×F. New `availableElectricalMW` gate samples physical
  `Engine::execute` to size total power BEFORE final accepted nozzle
  force/torque; scales all 24 duty values uniformly.
- `fa5580f8`, `b273a32f`: `installedRcsPlantExecuted` is separate
  current-step provenance. `DynamicMotionSystem::updateLocalFrameMotion`
  NO LONGER revokes actual Δv after real RCS execution by running old
  gas-pressure cutoff. Legacy slot-only diagnostics retain their gas path.
- `a2de8787`, `c54813f5`, `f9f5edf3`: standalone deterministic
  `advanceNewtonianRcsGas` updates a shared renewable 0..1 budget
  BEFORE 24-nozzle allocation. Pilot translation PLUS commanded
  pitch/yaw/roll stabilization draw from the same reservoir; after
  exhaustion, nozzles are gated until recovered to the restart threshold.
  Completely unpowered or destroyed installations recharge instead of
  charging the player for unattainable commands. The old requested
  manoeuvre acceleration is replaced with executed force/mass, retaining
  actual main thrust. Longitudinal −Z hull orientation, correct +Z
  physics sign on roll demand and delivered roll Δω, and CoM are
  consistent in the new server seam.
- `13900ba3`: native installed Newtonian NEVER substitutes virtual
  attitude or RCS if one of the 24 modules is missing. Missing RCS
  modules are zero-force `MountedEngine` columns. RCS thermal lifecycle,
  damage and executed nozzle throttle stay in `ObjectModuleRuntime`.
- `1a362b18`, `c061fa09`, `ad627e8a`:
  all Newtonian I/Y/X share the SAME 8 mount-node slots aligned with
  the fixed 24-nozzle catalog (not obsolete 4/6/8 virtual layouts).
  Persistent Newtonian main and reactor IDs are defined for each
  layout. Current capability module/snapshot projection conservatively
  combines each physical node's 3 nozzle states; a missing nozzle cannot
  silently resurrect authority from the other two.
- `67d59ab9`, `71fb55fc`, `7b8f3b7a`:
  Cobra module graph now PREINSTALLS all six REAL main-drive
  construction variants, their dedicated ER-5 reactor groups and the
  24 independent RCS nozzles. `ShipCore` selects a persistent
  `std::array<ShipDescriptor,6>` view on Ctrl+F9/F10, binds installed
  `ModuleRuntime`, PRESERVES damage/temperature, and no longer invokes
  fake generic motors for a Cobra Newtonian switch. A direct native
  `ELITE_SHIP_PHYSICAL_TEST_MODEL=Newtonian-I/Y/X` launch has nonempty
  engine/module bindings and reactor groups.
- `92a789ae`: replicated cockpit descriptor reconstructs the selected
  installed Newtonian I/Y/X main+reactor passport binding from the
  server's per-instance physical model, instead of treating switched
  Newtonian as synthetic healthy diagnostic equipment. Existing HUD
  8 RCS indicators read the actual 24 nozzle installed/usage data.
- `6fed232f`, `2d6db724`, `a8882ad4`,
  `6111c582`: physically executed 24-nozzle MW is now an auxiliary
  ship-services PowerBus demand; it contributes to reactor output
  request/load shedding and replicated requested-MW telemetry.
  Execution itself is capped to power available AFTER priority
  service consumers; RCS cannot borrow independent ER-5 motor power.
  RCS heat/wear remains sourced from actual throttle/EngineUnit.
- `defa6623`, `0abc9281`: existing topology tests now assert
  8 nodes for all three Newtonian constructions and recalculate loss
  of one out of eight (instead of one out of four).
  `cfdabb2c` adds direct tests of nozzle failure, live COM torque,
  zero-electrical-power thrust/heat, and gas depletion/recharge.
  `9a42a0b9` aligns physical-control contract documentation.

### Known limits / explicit OPEN issues — do not pretend completion

- SOURCE commits above have NOT YET been built or run in user's native
  Windows MSYS2 environment. First gate is compilation of
  `ship_physical_control_models_tests` and
  `newtonian_rcs_physical_tests`; run their two CTests, then
  `ship_control_behavior_oracle`,
  `ship_control_execution_gateway`, `engine_thrust_hud_presentation`
  and `newtonian_execution_authority`. Only then build client+server
  with `bash build_mingw64.sh`. No test reports from new patches yet.
- **P1 genuine 6D signed capability/accuracy remains approximate:** the
  capability model conservatively projects three nozzles per node but
  still uses aggregate 8-node scalar `manoeuvreThrusterAccel` and
  `angularAccel`. The installed 24-nozzle optimizer knows EXACT
  actuator directions and CoM, but MotherComputer planner still lacks
  a runtime solved 6D feasible force/torque polytope under arbitrary
  asymmetric nozzle damage and electrical limits. Do not claim
  every independent XYZ/pitch/yaw/roll demand is achievable just because
  the corresponding input key works. After native gates, validate
  physical reachability or replace the scalar adapter using bounded
  concrete 24-nozzle authority sampling.
- RCS power is now part of PowerBus via last executed-tick measured
  demand, while the per-step RCS allocator takes current free output.
  Reactor dynamics and heat transport in this one-tick-lag feedback
  need end-to-end gameplay observations, particularly ignition off/on,
  large service-system loads, nozzle overheating and damaged reactor.
- Graphic nose/gimbal animation, HUD temperature gradient on engine
  emitter/pads, exact system coolant allocation, degradation of engine
  efficiency across failures and engine impact aftershock STILL require
  final cross-layout gameplay/thermal validation. Do NOT silently
  change Assisted behavior merely to make Newtonian tests pass.
- Hit-geometry / damage coverage remains UNFINISHED for non-native
  layout meshes: Newtonian I/Y/X installed main modules were cloned with
  meshPartIds/supportLinks cleared (as with dormant Assisted Y/X).
  `CobraRcsEquipmentAssembly.h` explicitly does NOT create fabricated
  physical hit volumes for nozzle equipment. Stable module-ID debug
  damage and state replication exist, but real collisions striking the
  visible hull do not yet prove per-nozzle/newton-main damage routing.
  User asked about damage: do not claim this layer visually or physically
  complete until hit ownership is authored.
- The new selected 8-node capability projection is a conservative
  summary (minimum availability among node x/y/z motors), not an
  independent 6D feasible wrench certificate. Its scalar healthy
  acceleration may still exceed real 24-nozzle motor force per mass.
  Physical execution refuses impossible accelerations, but autopilot
  speed/turn planning must be calibrated against measured actual
  authority after the native physical gate.
- The full six-model acceptance criterion is PHYSICAL Causality:
  same-tick installed module availability and power -> requested
  forces/moments -> per-engine EXECUTED thrust, heat/MW -> exact ΣF/m
  and Σ(r-COM)×F/I -> integrated Δv/Δω -> replicated Motor/RCS HUD.
  Test each I/Y/X with healthy equipment, single nozzle/main outage,
  severe damage/heat, depleted gas and gas restart, generator shutdown/
  ignition, and arbitrary body-axis keys.
- No new side branches, alternative project-state documents or attempts
  to conceal pending test failures. Focus on native build and actual
  physical regression evidence before further architectural expansion.

## 2026-10-25 native Newtonian focused Windows check: 5 PASS, BITE HUD link fixed (rerun pending)

- User uploaded actual Windows MSYS2 MinGW64 build/CTest log from
  `build/tests/navigation_runtime` after Newtonian repairs. Of six
  tests, **five PASSED natively**:
  `ship_control_execution_gateway`,
  `ship_physical_control_models`,
  `newtonian_execution_authority`,
  `ship_control_behavior_oracle`,
  `newtonian_rcs_physical`.
- `engine_thrust_hud_presentation` was `NOT RUN` solely because
  `engine_thrust_hud_presentation_tests.exe` failed linking:
  undefined `game::bite::BiteApi::BiteApi(Thresholds)` and
  `BiteApi::evaluate(...)`. This is the only blocking error reported
  in the uploaded build. The warnings `unused variable coolant`,
  `wasteMW`, unused reactor arg, MeshData signed/unsigned compare
  are nonfatal and NOT the linker failure.
- Root cause: standalone `tests/navigation_runtime/CMakeLists.txt`
  linked its HUD executable only with
  `EliteNavigationRuntimeControl`, whereas production defines
  `EliteBite` as a library from `src/game/bite/BiteApi.cpp`
  and links EliteGame and EliteServer to it. The HUD presentation
  now calls BITE methods via `MotherComputerClientAdapter`.
- **FIX `a32d28bb`:** this standalone CMake project defines the SAME
  `EliteBite` static library from the production source under
  `if(NOT TARGET EliteBite)`, with the correct root include dirs
  and C++17; `engine_thrust_hud_presentation_tests` now links
  `EliteNavigationRuntimeControl EliteBite`. Only test CMake changed;
  flight physics, model health, HUD presentation and BITE behavior
  remain untouched. Readback verified BiteApi.cpp defines requested
  symbols; build/test on native Windows still PENDING.
- NEXT user gate:
  `cd /d/__elite/work && git pull --ff-only origin main`
  then
  `cmake --build build/tests/navigation_runtime --target engine_thrust_hud_presentation_tests -j 6 && ctest --test-dir build/tests/navigation_runtime -R '^engine_thrust_hud_presentation

- Fore-only propulsion (`aft` completely dead, `fore` alive) is
  inconsistent: manual `DynamicMotionSystem::applyLocalFrameInput` chooses
  -nose as travel forward, while `FlightControlProfile`/MotherComputer
  demand safe AFT working-forward thrust, currently reject this doctrine.
  Needs coherent signed forward/backward banks AND flip-and-burn control;
  an isolated sign flip or changing axis label is unsafe.
- Command → Request → Executed still needs end-to-end inspection in
  damaged/reverse and ordinary gameplay paths. The installed MAIN-engine
  post-execution speed clamp was fixed and passed its targeted Windows
  regression; legacy/non-installed motion and low-speed BrakeToStop/RCS
  channels still require separate physical audit.
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

## 2026-10-25 Assisted stationary hull-turn fixture: MODE ERROR FOUND, source-only repair

- **2026-10-25 user feedback after `90833c16`:** Windows build
  **succeeded**. User wrote "сборка - прошла" without separate CTest
  `100% tests passed` summary; do not assert native
  `client_route_autopilot` PASS until test output is confirmed.
  Proceed to requested in-game stationary and ~100m/s obstacle-escape
  tunnel verification, checking selected course versus first tunnel
  tangent and projected live route entry.


- User's most recent native `client_route_autopilot` CTest was RED on
  `Assisted autopilot failed to settle on commanded turn angle`.
  This is `testAssistedAutopilotCapturesTurnsWithoutOvershoot`, which
  simulates 5/15/45/90/-45/-90 degree hull turns at ZERO velocity, each
  over 400*0.02 = 8s, with a keyboard actuator ramp, angular inertia,
  and real normalized pitch/yaw inputs. The failure did NOT originate in
  the new `SpaceState.cpp` safe-course direction patch.
- ROOT CAUSE verified by reading control code and independently reproducing
  the discrete 1D yaw physics: the test omitted
  `AutopilotControlRequest.terminalAttitudeHold`, so a PURE STATIONARY
  HULL POSE manoeuvre was routed through moving velocity-course steering,
  whose max 10-degree hull lead intentionally limits turning rate.
  At 90 degrees, the reconstruction ended ~77.10 degrees after 8s;
  with terminal attitude HOLD (full desired hull pose at zero velocity)
  it ended ~89.98 degrees after 8s, overshoot <0.25 degrees. This
  reproduces input-hold ramp, angular damping, the 0.5s attitude servo,
  and online angular-authority estimation. It is a calculated local
  result; NOT a Windows CTest result.
- Commit `90833c16` changes ONLY
  `tests/navigation_runtime/ClientRouteAutopilotTests.cpp`:
  adds `request.terminalAttitudeHold = true;` in this stationary-hull
  turn fixture, retaining 8s and the original stringent 0.25-degree
  settlement AND overshoot requirements. This is a semantic test
  correction, NOT an accommodation by weakening timing or thresholds.
  The distinct ordinary MOVING velocity-course tests remain unchanged.
  The real `AssistedAutopilot.h` and physical controller remain untouched.
- This repair is **SOURCE ONLY / Windows test pending**.
  NEXT run only `client_route_autopilot_tests` with build-gated CTest.
  If it passes, the other four focused planner tests were already green
  in the prior Windows 4/5 run. If another failure emerges, diagnose
  that specific contract and avoid blind threshold chasing.
  The real priority remains an in-game blocked-entry tunnel check at
  rest and ~100m/s, comparing escape vector, first tunnel tangent and
  live late-bind entry. Native gameplay not yet accepted.
- Source history: Assisted damped inner-loop entered main on Oct 22,
  and the current safe-course-to-planner fix `1bc9742f` changes ONLY
  the initial tangent. Earlier sequential assertions were legacy;
  do not infer a new physics regression from each now-reached test.

## Immediate next validation — blocked route entry, THEN general regression and gameplay

**Priority now:** rebuild `client_route_autopilot_tests` and run its
single native CTest after `90833c16`. If green, proceed to actual
game obstacle escape and safe initial-tunnel course, stopped and at
~100 m/s. Do not claim runtime acceptance based solely on CTests.


- The current Windows native `ship_control_execution_gateway` CTest is
  **PASS (user-reported)**. The old six-model in-game oracle was previously
  **84/84 PASS** on its separate older gate. Neither proves the entire
  damage/autopilot/reactor gameplay.
- PRIORITY BEFORE broad regression: the Assisted obstacle-avoidance
  route-start patch above must pass its targeted Windows navigation
  tests and in-game route-entry acceptance at both zero and nonzero speed.
  The course requested by planner must match the one physically selected
  by PreplanningSafetyCourse; a stale connector must be rebound rather
  than turning back into the obstacle.
- NEXT: build and run existing focused regression targets in the
  `tests/navigation_runtime` suite (without inventing new tests):
  `ship_control_behavior_oracle`,
  `ship_physical_actuator_plant`,
  `ship_propulsion_state`,
  `newtonian_rcs_physical`,
  `working_thrust_axis_analysis`,
  `ship_mobility_assessment`,
  `assisted_authority_contract`,
  `hull_attitude_control`,
  `navigation_execution_replan_policy`,
  `client_route_autopilot`,
  `engine_thrust_hud_presentation`,
  `assisted_layout_flight_matrix`,
  `ship_physical_corridor_matrix`.
  Run the existing `equipment_damage_failure_pipeline` architecture
  contract test as the hardware/damage companion.
- Then native build `EliteGame.exe` and `EliteServer.exe`, and actually
  fly: healthy Assisted-I response; single-aft-engine outage with
  Ctrl+Alt+Shift+F and safe working-thrust ray; restart/off reactor
  Ctrl+Alt+Shift+I and no phantom acceleration; RCS and route-following
  while degraded. Collect `D:/tmp` logs if anything fails. Do not
  assert gameplay PASS until the user observes it.
- Do not spend user time verifying the optional PASSED/FAILED dialog.
  Do not begin unrelated fore-only or large capability migration before
  this flight gate. Avoid tests-for-tests and parallel state files.
 --output-on-failure`.
  If PASS, focused six-test gate was 5/5 + repaired HUD pass,
  THEN start full client/server `bash build_mingw64.sh`.
  Never claim a full native build is green until that output is seen.

## 2026-10-25 HUD test BITE link now PASS, fixture runtime HUD invalid-installed-map failure FIXED (Windows recheck pending)

- User's newest MSYS2 output after `a32d28bb`: CMake regenerated,
  `engine_thrust_hud_presentation_tests.exe` linked successfully, but
  native CTest `engine_thrust_hud_presentation` FAILED with
  `damaged engine HUD unexpectedly hidden` (0/1 of this rerun).
  Thus BITE undefined references ARE RESOLVED; do not confuse a
  running test failure with a linker failure. The earlier other five
  target native CTests passed.
- Root cause in `EngineThrustHudPresentationTests.cpp::
  testFaultFlagComesFromPhysicalSlotAvailability`: a deliberately
  BROKEN four-slot Assisted-I installation binding with only first ID
  present (`engine:aft_left`) and three empty IDs, plus no actual
  matching runtime equipment. `ShipCapabilityBuilder::slotStates`
  correctly returns EMPTY for such a malformed installed binding,
  rejecting propulsion authority rather than substituting healthy
  engines. However `buildEngineThrustHudPresentation(ClientShipState)`
  returned an EMPTY/hidden instrument instead of visually reporting
  unusable installed propulsion.
- **SOURCE FIX `43860e90`:** only in HUD presentation, if the
  per-slot projection cannot match the known physical topology, render
  all known physical engine slots as NONOPERATIONAL (capability=0,
  delivered thrust=0), flagging BITE/HUD warnings. Physical capability
  projection and command executor rejection semantics unchanged.
  A missing ship descriptor or empty topology still hides HUD.
- **REGRESSION `294f1336`:** retained original test's intended
  visible+warning behavior, additionally demands exactly four
  Assisted-I engine bars AND zero capability/executed thrust on
  every bar. This tests no phantom healthy engine indication; no
  numerical flight tolerance was loosened.
- Source readback confirms C++17 aggregate
  `EngineSlotState{false,0.0,0.0}` in renderer; Windows rerun PENDING.
  NEXT:
  `cd /d/__elite/work && git pull --ff-only origin main`
  then `cmake --build build/tests/navigation_runtime --target
  engine_thrust_hud_presentation_tests -j 6 && ctest --test-dir
  build/tests/navigation_runtime -R '^engine_thrust_hud_presentation

- Fore-only propulsion (`aft` completely dead, `fore` alive) is
  inconsistent: manual `DynamicMotionSystem::applyLocalFrameInput` chooses
  -nose as travel forward, while `FlightControlProfile`/MotherComputer
  demand safe AFT working-forward thrust, currently reject this doctrine.
  Needs coherent signed forward/backward banks AND flip-and-burn control;
  an isolated sign flip or changing axis label is unsafe.
- Command → Request → Executed still needs end-to-end inspection in
  damaged/reverse and ordinary gameplay paths. The installed MAIN-engine
  post-execution speed clamp was fixed and passed its targeted Windows
  regression; legacy/non-installed motion and low-speed BrakeToStop/RCS
  channels still require separate physical audit.
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

## 2026-10-25 Assisted stationary hull-turn fixture: MODE ERROR FOUND, source-only repair

- **2026-10-25 user feedback after `90833c16`:** Windows build
  **succeeded**. User wrote "сборка - прошла" without separate CTest
  `100% tests passed` summary; do not assert native
  `client_route_autopilot` PASS until test output is confirmed.
  Proceed to requested in-game stationary and ~100m/s obstacle-escape
  tunnel verification, checking selected course versus first tunnel
  tangent and projected live route entry.


- User's most recent native `client_route_autopilot` CTest was RED on
  `Assisted autopilot failed to settle on commanded turn angle`.
  This is `testAssistedAutopilotCapturesTurnsWithoutOvershoot`, which
  simulates 5/15/45/90/-45/-90 degree hull turns at ZERO velocity, each
  over 400*0.02 = 8s, with a keyboard actuator ramp, angular inertia,
  and real normalized pitch/yaw inputs. The failure did NOT originate in
  the new `SpaceState.cpp` safe-course direction patch.
- ROOT CAUSE verified by reading control code and independently reproducing
  the discrete 1D yaw physics: the test omitted
  `AutopilotControlRequest.terminalAttitudeHold`, so a PURE STATIONARY
  HULL POSE manoeuvre was routed through moving velocity-course steering,
  whose max 10-degree hull lead intentionally limits turning rate.
  At 90 degrees, the reconstruction ended ~77.10 degrees after 8s;
  with terminal attitude HOLD (full desired hull pose at zero velocity)
  it ended ~89.98 degrees after 8s, overshoot <0.25 degrees. This
  reproduces input-hold ramp, angular damping, the 0.5s attitude servo,
  and online angular-authority estimation. It is a calculated local
  result; NOT a Windows CTest result.
- Commit `90833c16` changes ONLY
  `tests/navigation_runtime/ClientRouteAutopilotTests.cpp`:
  adds `request.terminalAttitudeHold = true;` in this stationary-hull
  turn fixture, retaining 8s and the original stringent 0.25-degree
  settlement AND overshoot requirements. This is a semantic test
  correction, NOT an accommodation by weakening timing or thresholds.
  The distinct ordinary MOVING velocity-course tests remain unchanged.
  The real `AssistedAutopilot.h` and physical controller remain untouched.
- This repair is **SOURCE ONLY / Windows test pending**.
  NEXT run only `client_route_autopilot_tests` with build-gated CTest.
  If it passes, the other four focused planner tests were already green
  in the prior Windows 4/5 run. If another failure emerges, diagnose
  that specific contract and avoid blind threshold chasing.
  The real priority remains an in-game blocked-entry tunnel check at
  rest and ~100m/s, comparing escape vector, first tunnel tangent and
  live late-bind entry. Native gameplay not yet accepted.
- Source history: Assisted damped inner-loop entered main on Oct 22,
  and the current safe-course-to-planner fix `1bc9742f` changes ONLY
  the initial tangent. Earlier sequential assertions were legacy;
  do not infer a new physics regression from each now-reached test.

## Immediate next validation — blocked route entry, THEN general regression and gameplay

**Priority now:** rebuild `client_route_autopilot_tests` and run its
single native CTest after `90833c16`. If green, proceed to actual
game obstacle escape and safe initial-tunnel course, stopped and at
~100 m/s. Do not claim runtime acceptance based solely on CTests.


- The current Windows native `ship_control_execution_gateway` CTest is
  **PASS (user-reported)**. The old six-model in-game oracle was previously
  **84/84 PASS** on its separate older gate. Neither proves the entire
  damage/autopilot/reactor gameplay.
- PRIORITY BEFORE broad regression: the Assisted obstacle-avoidance
  route-start patch above must pass its targeted Windows navigation
  tests and in-game route-entry acceptance at both zero and nonzero speed.
  The course requested by planner must match the one physically selected
  by PreplanningSafetyCourse; a stale connector must be rebound rather
  than turning back into the obstacle.
- NEXT: build and run existing focused regression targets in the
  `tests/navigation_runtime` suite (without inventing new tests):
  `ship_control_behavior_oracle`,
  `ship_physical_actuator_plant`,
  `ship_propulsion_state`,
  `newtonian_rcs_physical`,
  `working_thrust_axis_analysis`,
  `ship_mobility_assessment`,
  `assisted_authority_contract`,
  `hull_attitude_control`,
  `navigation_execution_replan_policy`,
  `client_route_autopilot`,
  `engine_thrust_hud_presentation`,
  `assisted_layout_flight_matrix`,
  `ship_physical_corridor_matrix`.
  Run the existing `equipment_damage_failure_pipeline` architecture
  contract test as the hardware/damage companion.
- Then native build `EliteGame.exe` and `EliteServer.exe`, and actually
  fly: healthy Assisted-I response; single-aft-engine outage with
  Ctrl+Alt+Shift+F and safe working-thrust ray; restart/off reactor
  Ctrl+Alt+Shift+I and no phantom acceleration; RCS and route-following
  while degraded. Collect `D:/tmp` logs if anything fails. Do not
  assert gameplay PASS until the user observes it.
- Do not spend user time verifying the optional PASSED/FAILED dialog.
  Do not begin unrelated fore-only or large capability migration before
  this flight gate. Avoid tests-for-tests and parallel state files.

  --output-on-failure`. If HUD test passes, run the focused six-test
  group or proceed to `bash build_mingw64.sh` after recording its gate.
- This is an HUD presentation defect, NOT evidence of a new defect in
  installed Newtonian forces, actual nozzle heat, CoM or gas pressure.

## 2026-10-25 HUD test progresses to old acceleration-based expectation; corrected telemetry fixture (Windows rerun pending)

- User ran the HUD-only Windows test after fixes `43860e90` and
  `294f1336`. It BUILT and LINKED cleanly; the previous invalid
  installed-binding hidden-widget failure is now PASSED.
  The next failing assertion was `forward Assisted thrust mapped to
  wrong engine bars` in
  `EngineThrustHudPresentationTests::testAssistedForwardThrustUsesAftBars`.
  Four compiler warnings (unused coolant, unused reactor parameter,
  MeshData signedness and unused wasteMW) were nonfatal.
- Root cause SOURCE-PROVEN: this historic test populated ONLY
  `ship.transform.motion.engineAccelerationMps2 = forward*36.775`
  (and `right*36.775` in the next test) and then expected red
  per-engine HUD bar thrust=0.5. The accepted installed-equipment
  telemetry contract says red bars are exactly ACTUAL
  `motion.engineTelemetry.thrust01[i]` clipped to current availability;
  no per-motor thrust may be reconstructed from net ship acceleration.
  `EngineLiveTelemetryPresentation.h` intentionally defaults to
  zero red for no valid executed motor sample. Reenabling the old
  inference would BREAK real damage causality.
- **TEST FIX `fffdb213` (only test code):** the forward HUD fixture
  first asserts fake net acceleration with NO executed motor telemetry
  leaves all four red bars ZERO. It then publishes authoritative-
  shaped four-slot actuator samples: indices 0/1 aft each 0.5,
  indices 2/3 fore zero, all available=1.0. Verifies physical slot
  order and aft-only bottom-up red bars while all yellow bars remain
  1.0. The lateral vectoring HUD fixture now publishes all four
  reported motor fractions 0.5 rather than presuming red from
  `engineAccelerationMps2`; asserts four bars. This is a
  PRESENTATION MAPPING fixture (not a claim to have executed a
  physical wrench); actual physical allocation is separately
  exercised by the existing installed matrix/gateway tests.
  Corrected the final test printout to say red bars follow executed
  actuator telemetry, NEVER net acceleration.
- PRODUCTION `EngineThrustHudPresentation.h`, damage projection,
  torque physics, `ObjectModuleRuntime`, gas and power paths UNCHANGED
  in this turn. No test tolerances changed or fake green results.
- Next Windows check:
  `cd /d/__elite/work && git pull --ff-only origin main`
  `cmake --build build/tests/navigation_runtime --target
  engine_thrust_hud_presentation_tests -j 6 && ctest --test-dir
  build/tests/navigation_runtime -R '^engine_thrust_hud_presentation

- Fore-only propulsion (`aft` completely dead, `fore` alive) is
  inconsistent: manual `DynamicMotionSystem::applyLocalFrameInput` chooses
  -nose as travel forward, while `FlightControlProfile`/MotherComputer
  demand safe AFT working-forward thrust, currently reject this doctrine.
  Needs coherent signed forward/backward banks AND flip-and-burn control;
  an isolated sign flip or changing axis label is unsafe.
- Command → Request → Executed still needs end-to-end inspection in
  damaged/reverse and ordinary gameplay paths. The installed MAIN-engine
  post-execution speed clamp was fixed and passed its targeted Windows
  regression; legacy/non-installed motion and low-speed BrakeToStop/RCS
  channels still require separate physical audit.
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

## 2026-10-25 Assisted stationary hull-turn fixture: MODE ERROR FOUND, source-only repair

- **2026-10-25 user feedback after `90833c16`:** Windows build
  **succeeded**. User wrote "сборка - прошла" without separate CTest
  `100% tests passed` summary; do not assert native
  `client_route_autopilot` PASS until test output is confirmed.
  Proceed to requested in-game stationary and ~100m/s obstacle-escape
  tunnel verification, checking selected course versus first tunnel
  tangent and projected live route entry.


- User's most recent native `client_route_autopilot` CTest was RED on
  `Assisted autopilot failed to settle on commanded turn angle`.
  This is `testAssistedAutopilotCapturesTurnsWithoutOvershoot`, which
  simulates 5/15/45/90/-45/-90 degree hull turns at ZERO velocity, each
  over 400*0.02 = 8s, with a keyboard actuator ramp, angular inertia,
  and real normalized pitch/yaw inputs. The failure did NOT originate in
  the new `SpaceState.cpp` safe-course direction patch.
- ROOT CAUSE verified by reading control code and independently reproducing
  the discrete 1D yaw physics: the test omitted
  `AutopilotControlRequest.terminalAttitudeHold`, so a PURE STATIONARY
  HULL POSE manoeuvre was routed through moving velocity-course steering,
  whose max 10-degree hull lead intentionally limits turning rate.
  At 90 degrees, the reconstruction ended ~77.10 degrees after 8s;
  with terminal attitude HOLD (full desired hull pose at zero velocity)
  it ended ~89.98 degrees after 8s, overshoot <0.25 degrees. This
  reproduces input-hold ramp, angular damping, the 0.5s attitude servo,
  and online angular-authority estimation. It is a calculated local
  result; NOT a Windows CTest result.
- Commit `90833c16` changes ONLY
  `tests/navigation_runtime/ClientRouteAutopilotTests.cpp`:
  adds `request.terminalAttitudeHold = true;` in this stationary-hull
  turn fixture, retaining 8s and the original stringent 0.25-degree
  settlement AND overshoot requirements. This is a semantic test
  correction, NOT an accommodation by weakening timing or thresholds.
  The distinct ordinary MOVING velocity-course tests remain unchanged.
  The real `AssistedAutopilot.h` and physical controller remain untouched.
- This repair is **SOURCE ONLY / Windows test pending**.
  NEXT run only `client_route_autopilot_tests` with build-gated CTest.
  If it passes, the other four focused planner tests were already green
  in the prior Windows 4/5 run. If another failure emerges, diagnose
  that specific contract and avoid blind threshold chasing.
  The real priority remains an in-game blocked-entry tunnel check at
  rest and ~100m/s, comparing escape vector, first tunnel tangent and
  live late-bind entry. Native gameplay not yet accepted.
- Source history: Assisted damped inner-loop entered main on Oct 22,
  and the current safe-course-to-planner fix `1bc9742f` changes ONLY
  the initial tangent. Earlier sequential assertions were legacy;
  do not infer a new physics regression from each now-reached test.

## Immediate next validation — blocked route entry, THEN general regression and gameplay

**Priority now:** rebuild `client_route_autopilot_tests` and run its
single native CTest after `90833c16`. If green, proceed to actual
game obstacle escape and safe initial-tunnel course, stopped and at
~100 m/s. Do not claim runtime acceptance based solely on CTests.


- The current Windows native `ship_control_execution_gateway` CTest is
  **PASS (user-reported)**. The old six-model in-game oracle was previously
  **84/84 PASS** on its separate older gate. Neither proves the entire
  damage/autopilot/reactor gameplay.
- PRIORITY BEFORE broad regression: the Assisted obstacle-avoidance
  route-start patch above must pass its targeted Windows navigation
  tests and in-game route-entry acceptance at both zero and nonzero speed.
  The course requested by planner must match the one physically selected
  by PreplanningSafetyCourse; a stale connector must be rebound rather
  than turning back into the obstacle.
- NEXT: build and run existing focused regression targets in the
  `tests/navigation_runtime` suite (without inventing new tests):
  `ship_control_behavior_oracle`,
  `ship_physical_actuator_plant`,
  `ship_propulsion_state`,
  `newtonian_rcs_physical`,
  `working_thrust_axis_analysis`,
  `ship_mobility_assessment`,
  `assisted_authority_contract`,
  `hull_attitude_control`,
  `navigation_execution_replan_policy`,
  `client_route_autopilot`,
  `engine_thrust_hud_presentation`,
  `assisted_layout_flight_matrix`,
  `ship_physical_corridor_matrix`.
  Run the existing `equipment_damage_failure_pipeline` architecture
  contract test as the hardware/damage companion.
- Then native build `EliteGame.exe` and `EliteServer.exe`, and actually
  fly: healthy Assisted-I response; single-aft-engine outage with
  Ctrl+Alt+Shift+F and safe working-thrust ray; restart/off reactor
  Ctrl+Alt+Shift+I and no phantom acceleration; RCS and route-following
  while degraded. Collect `D:/tmp` logs if anything fails. Do not
  assert gameplay PASS until the user observes it.
- Do not spend user time verifying the optional PASSED/FAILED dialog.
  Do not begin unrelated fore-only or large capability migration before
  this flight gate. Avoid tests-for-tests and parallel state files.

  --output-on-failure`.
  Earlier five Newtonian/Assisted targeted CTests were reported green;
  sixth remains PENDING. If green, check native client+server
  `bash build_mingw64.sh` and then visual physical flight in
  all Newtonian I/Y/X. Do not conflate targeted test pass with full
  in-game acceptance.

## 2026-10-25 automated Newtonian physical log suite — code committed, native gate PENDING

- User reported all SIX prior focused tests GREEN on their Windows
  MSYS2 host after HUD test `fffdb213`. This confirms those targeted
  CTests, NOT the full client/server runtime under all damage conditions.
  Asked to automate diagnostic tests and write all outputs under
  `D:/tmp` so next discrepancies can be analyzed from logs rather
  than requiring video on every iteration.
- Added **isolated executable** `tests/navigation_runtime/
  NewtonianPhysicalDiagnosticsTests.cpp` (`8d64a6d7`) registered as
  CTest `newtonian_physics_diagnostics` (`9f220ab8`). It runs
  actual production `executeEngineControlMatrix` for Newtonian
  I/Y/X (1/3/4 aft engines) and `allocateNewtonianRcs` with
  24 real Engine passports and fixed nozzle geometry, reporting
  all 12 signed force/moment axes plus combined demand, missing
  nozzle, damaged nozzle, zero-available-power, restricted power,
  shifted CoM, and a sustained replenish/deplete/restart gas cycle.
  It checks that delivered output equals independent ΣF, Σ(r-COM)×F,
  sampled nozzle MW, and never exceeds the input bus budget.
  `f6e8845b` also requires genuine nonzero correctly signed delivered
  force/torque for EACH healthy ±X/±Y/±Z and pitch/yaw/roll axis.
  It deliberately does NOT force unrealistically perfect multi-axis
  cancellation; CSV residuals show actual unmet demand and cross-torque.
  **This is a deterministic isolated solver check**, NOT a
  `ShipCore` full-game simulation or claim of six-model acceptance.
  Files:
  `D:/tmp/elite_newtonian_matrix.csv` (requested vs delivered
  forces/moments, impulses, MW, gas, main and RCS),
  `D:/tmp/elite_newtonian_nozzles.csv` (module IDs, all 24 nozzle
  thrust/availability/power/heat),
  `D:/tmp/elite_newtonian_summary.txt` (PASS and model coverage);
  on failure `elite_newtonian_diagnostics_failure.txt`.
- Added **actual native server trace** in
  `ShipCore::updateTranslationControl` (`4ae60c2e`,
  `75788e7e`). Triggers only for player-controlled
  `ModuleRuntime` Newtonian in HubTactical; creates
  `D:/tmp/elite_newtonian_runtime.csv` on first real tick, collects
  up to 240 seconds, flushes every 30 control ticks. It is read-only,
  does NOT alter control demand or physics. For each tick it records
  current I/Y/X model, manual ±XYZ/attitude, reactor startup and
  power headroom, state of gas accumulator, before-gas and
  gas-limited demand, actual ΣF/ΣM, estimated actual RCS Δv/Δω,
  main-engine acceleration and torque, current body angular rates,
  real per-engine thrust/availability/electrical/heat (up to 4 mains),
  all 24 nozzles' availability/actual duty/electricity/heat, and
  current CoM. The velocity field is explicitly PRE-INTEGRATOR:
  do not misinterpret it as post-propagation state. Runtime trace
  stays distinct from isolated physical scenario CSV.
- Added `scripts/run_newtonian_physics_diagnostics.sh`
  (`8764db5c`) to build and run seven targeted CTests once,
  **without touching game state or branches**, with all compilation
  output in `D:/tmp/elite_newtonian_build.log` and CTest console in
  `D:/tmp/elite_newtonian_ctest.log`.
- **REQUIRED WINDOWS GATE (new code is not compiled here):**
  In MSYS2 MinGW64:
  `cd /d/__elite/work && git pull --ff-only origin main &&
  bash scripts/run_newtonian_physics_diagnostics.sh`.
  If green, `bash build_mingw64.sh` for real client+server,
  launch ship with Newtonian I/Y/X, try NumPad translation and
  pitch/yaw/roll, a damaged nozzle and engine/generator off/on;
  send three isolated diagnostics files, build/CTest logs, and
  `elite_newtonian_runtime.csv`. Do not treat the isolated
  matrix test as proof that all mounted modules, thermal inertia,
  reactor bus or network HUD are healthy in actual gameplay.
- All script/log code is diagnostic/test-only, except passive
  instrumentation inside production `ShipCore.cpp`.
  Assisted control physics unchanged; Newtonian gameplay gas
  remains a replenishing **gameplay** limit.

## 2026-10-25 first Newtonian D:/tmp diagnostic compile gate: explicit GLM vector fix

- User ran `cd /d/__elite/work && git pull --ff-only origin main &&
  bash scripts/run_newtonian_physics_diagnostics.sh` in MSYS2 MinGW64.
  The standalone CMake configuration completed, but the NEW
  `newtonian_physics_diagnostics_tests` source FAILED COMPILATION;
  CTest never started. The compiler identified implicit conversion
  from a one-element braced initializer `{0.0}` into
  `const glm::dvec3&`, where installed GLM 1.0.x marks
  `glm::vec3(double)` explicit. Primary errors at diagnostic test
  lines 156/157 (default function parameters), 259/260 (near())
  and 382 (two explicit arguments). EngineUnit unused coolant/wasteMW
  warnings were nonblocking.
- **SOURCE FIX `a658ea94` on main:** change ONLY the new
  `tests/navigation_runtime/NewtonianPhysicalDiagnosticsTests.cpp`
  to use `glm::dvec3(0.0)` for default reference argument
  temporaries and explicit zero-vector calls; keep deliberate direct
  local `glm::dvec3 value {0.0}` initialization unchanged because
  it is legal direct-list initialization. Verified no remaining
  single-value implicit argument braces in this file. No physics,
  mass, damage, power, gas, solver coefficients or acceptance
  thresholds changed.
- NEXT NATIVE GATE:
  `cd /d/__elite/work && git pull --ff-only origin main &&
  bash scripts/run_newtonian_physics_diagnostics.sh`.
  Logs still belong under `D:/tmp`. Compile and seven-test results
  after this correction are PENDING; do not yet claim any of the
  new diagnostic scenarios pass. Afterwards inspect physical CSV
  residuals and the real `elite_newtonian_runtime.csv` from game.

## 2026-10-25 Newtonian logged seven-test PASS; CSV audit found under-stressed limits — repaired diagnostic

- User sent actual MSYS2 MinGW64 `elite_newtonian_build.log`,
  `elite_newtonian_ctest.log`, `elite_newtonian_summary.txt`,
  `elite_newtonian_matrix.csv`, `elite_newtonian_nozzles.csv`.
  The earlier GLM C++17 compile fix WORKED. **7/7 CTests PASS on Windows**,
  including `newtonian_physics_diagnostics`; only preexisting
  `unused coolant` and `unused wasteMW` compiler warnings.
- Source dataset was parsed: 252 scenario/time rows (84 each for
  Newtonian I/Y/X); 1296 nozzle rows (432 each). Main-engine
  4 m/s^2 demand produced −1,040,000 N forward and zero moment
  for all three healthy installed layouts, with 1/3/4 actual
  main slots respectively. This is a REQUESTED acceleration, NOT
  evidence that I/Y/X have the same full-throttle capacity.
  Eight physical RCS nodes and 24 individual fixed-direction
  12,000-N motors = 288,000 N summed unidirectional magnitudes,
  NOT 288 kN achievable along any chosen body axis.
- Every healthy signed 12-kN XYZ and 30-kNm pitch/yaw/roll
  request produced matching actual wrench with negligible
  residual and low electrical draw. One missing/damaged nozzle
  reported zero own force, power and heat; other nozzles
  redistributed demand. Zero-power case produced zero wrench,
  power and waste heat. CoM-shift case used actual CoM lever arms.
  Replenishable gameplay gas exhausted, latched off and restarted,
  including repeated cycle while input was held. This is
  isolated allocator evidence only, NOT server physics/HUD gameplay.
- **Test coverage hole found by numerical CSV audit:**
  `power_limited` requested 0.30 MW but actual nozzles consumed
  only ~0.231 MW. The test DID NOT actually exercise electrical
  current limiting, even though it passed. Also ordinary 12-kN
  requests never exercised saturating actuator authority.
- **TEST FIX `013f3f94` on main, WINDOWS RECHECK PENDING:**
  lower the physical RCS `power_limited` budget to 0.05 MW,
  require strictly positive delivered thrust and electricity
  but real force less than half the unrestricted request, and
  enforce draw <= budget. Add `force_saturation` scenario
  requesting 450,000 N in +X, more than the entire 288-kN
  installed sum, require residual >10 kN and output no higher
  than available nozzle sum. Both write requested vs actual
  measurements into existing `D:/tmp/elite_newtonian_matrix.csv`
  and per-nozzle measurements into `elite_newtonian_nozzles.csv`.
  Neither game physics nor solver tolerances were changed.
- Next short gate:
  `cd /d/__elite/work && git pull --ff-only origin main &&
  bash scripts/run_newtonian_physics_diagnostics.sh`.
  If green, proceed to canonical native client+server
  `bash build_mingw64.sh`, then run REAL Newtonian I/Y/X ship
  manoeuvres and provide `D:/tmp/elite_newtonian_runtime.csv`.
  That server trace must be checked for per-tick actuator duty,
  MW, temperatures, gas, applied force/CoM torque, orientation
  and damage, including reactor start/stop. No actual in-game
  trace was uploaded yet; do NOT claim runtime six-model
  flight acceptance. Retain only main and this canonical
  `PROJECT_STATE.md`.

## 2026-10-25 Claude analysis + Newtonian saturation test audit — ANALYSIS ONLY

**User scope**: review an external Claude code audit and uploaded seven-test/CSV
data against CURRENT `main`; DO NOT alter universe constants, physical
ratings, efficiencies, temperatures, thermal laws or flight code. This is a
source review and numerical log interpretation. No physics changes authorized.
Reviewed uploaded 300-line Claude report `Вставленный текст(20261025-205538).txt`,
`elite_newtonian_summary(1).txt`, `elite_newtonian_build(1).log`,
`elite_newtonian_ctest(1).log`, `elite_newtonian_matrix(1).csv`,
`elite_newtonian_nozzles(1).csv`.

### New empirical gate (Windows native)

- **7/7 selected CTests PASS** after genuine 0.05-MW power-limited and
  impossible 450-kN authority tests; compile shows ONLY two existing
  unused-variable warnings in EngineUnit/CobrettyCR1 preset.
- 255 scenario/time rows (85 Newtonian-I, 85 Y, 85 X);
  1368 nozzle rows (456 per model), 24 unique physical nozzle IDs per
  layout. 1/3/4 installed main motors respectively.
  An isolated 4 m/s² forward command produced −1,040,000 N total,
  zero residual body torque for all three layouts; this is a fixed
  **requested** acceleration and does NOT show their distinct full
  throttle peak capabilities.
- Unrestricted ±X/Y/Z force demands 12,000 N and ±pitch/yaw/roll
  moment demands 30,000 N*m were satisfied within CSV precision.
  On the `power_limited` scenario 0.05 MW was ACTUALLY consumed,
  only ~2.71 kN of ~12.53 kN requested force was delivered; force
  residual ~9.819 kN. On `force_saturation`, +450 kN demand delivered
  +48 kN, leaving 402 kN residual. Zero-power gave 0 output.
  Missing/damaged single nozzle was not resurrected. Gas depletion,
  latch, recharge and rearm appeared for all three. Shared identical
  RCS behavior is expected because same 24 geometry is used. These
  fixtures do not certify actual server flight, cooling, hit geometry,
  control intent translation, autopilot, networking or HUD.
- No real `D:/tmp/elite_newtonian_runtime.csv` from gameplay was sent
  in this turn. That remains the main next empirical acceptance artifact.

### Claude report: VERIFIED source defects / useful observations

1. **High priority service-power ordering defect**:
   `ShipCore::init` sets `m_serviceThrottleSaved=0.0`, then explicitly
   `m_reactor.setThrottle(0.8)`. Normally `updateSystems` resets
   throttle to `m_serviceThrottleSaved` (zero) BEFORE
   `m_powerBus.update()`. PowerBus samples
   `available=m_reactor->getCurrentOutput()` BEFORE setting the
   newly requested throttle; ordinary consumer allocation therefore
   sees zero despite nonzero power demand, even if reactor output is
   reset to a positive value afterwards. `getCurrentOutput()` reads
   current throttle directly. This is stronger source evidence than
   the external report's tentative INFERENCE. Live telemetry should
   verify effects on pump/RCS, not infer other unexamined game paths.
2. **High priority ER-5 thermal damage persistence**:
   `ShipCore::updateSystems` writes
   `engineReactorOperating.damage=equipmentDamage.accumulated`,
   calls `advanceEngineReactor` which adds Thermal to only that
   operating copy, without committing increment into module
   `equipmentDamage`. The next systems tick overwrites it again.
   ER-5 thermal damage accumulation is not authoritative/lasting.
   Fix in a future code pass through module-runtime damage accumulator
   (with revision quantum), NEVER update physical constants here.
3. **High priority capability power blindness**:
   `ShipCapabilityBuilder::applyMountedEngineAxialAuthority` uses
   rated thrust * damage/temperature projection but does not use
   available ER-5 power groups; those budgets apply at the executor
   later. Separate client replication also lacks matching real
   power/startup facts. This permits planning claims above
   actual execution in low-power cases.
4. **Autopilot provenance / revision**: Both
   `ClientRouteAutopilot.h` (~2713) and
   `DedicatedClientRouteAutopilot.h` (~2838) assign
   `build.capabilityRevision=requestSerial` instead of revision of
   physical state. The identifier is not sufficient evidence
   that geometry is rebuilt when installed equipment weakens;
   full invalidation not independently established.
5. **Learned bounds**: `AssistedAutopilot.h` allows learned authority
   up to 1.5*configured, and the measurement estimator up to 2*.
   It should never be mistaken for newly granted physical authority.
6. **PowerBus numerical issues**: `PowerBus::update` computes
   `totalRequested/maxOutput` without zero-capacity guard,
   and its function-static `wasEmergencyMode` is set but never used.
   Any fix is future scope.
7. **Runtime module integration/hit geometry**: `CoolingSystem` owns
   separate radiator/pump states; it does not directly project
   `ship_common_cooling`/service-reactor damage from
   `ObjectModuleRuntime`. `CobraRcsEquipmentAssembly` expressly
   leaves module hit-volumes unassigned. These remain genuine
   implementation gaps, not physical-constant problems.
8. **Request vs executed**: `DynamicMotionState` still reuses fields
   for pre-execution demand and executed acceleration, with correct
   interpretation depending on phase/flags. This is a robustness
   hazard; future explicit execution DTO preferable.
9. **Directionality and Newtonian fallback**: Source
   `DynamicMotionSystem::applySystemAccelerationDemand` places
   leftover requested acceleration into manoeuvre channels and
   uses hull forward rather than emergency working axis. This
   is bounded later by the physical executor; reachability in every
   Newtonian autopilot path has NOT been established. The dedicated
   signed `ShipControlAuthorityTelemetry` exists but its reduced
   ShipParams adapter still loses sign information.

### Claude report: architectural CHOICES and unverified PROPOSALS

- Engine `advanceEngineOperatingState` sets `coolantHeatMW=0`,
  removes excess via dedicated pad and thermal mass; ER-5
  `advanceEngineReactor` similarly uses ITS OWN pad with
  `overflowToSharedCoolantMW=0`. This is directly confirmed code,
  BUT also matches the currently explicitly authored dedicated-pad
  architecture. The common SERVICE reactor's `ReactorSystem::update`
  DOES pump heat into `ThermalSystem` and radiators; it is
  misleading to say no reactor→coolant loop exists anywhere.
  Some `ShipCore` comments nonetheless falsely imply common
  coolant/failed pumps affect every motor; reconcile documentation
  with agreed design before any thermal migration.
- Do NOT adopt Claude's proposed Stefan-Boltzmann law, design T,
  emitter fractions `phi`, reactor coolant fraction `psi`,
  changed conductivity, heat fractions, failure exponent,
  damage multipliers, efficiency formula or Newtonian controller
  formula on the strength of this audit. They are new UNTESTED
  **gameplay physics proposals**, not bugfixes or confirmed
  requirements. They change current rules and user explicitly
  prohibited physical value changes now.
- `ShipDynamics.h::mainAccelerationLimitMps2` and
  `installedDirectionalMainAccelerationMps2` already enforce
  `maxLinearGs` for requested limits, so raw 3* / 4* 9.56MN
  over-G arithmetic in the external report is not itself proof
  of a production over-acceleration. Actual execution under all
  profiles still needs runtime acceptance.
- Claude did not inspect all files and supplied no compiled patches;
  claims outside its reviewed source should retain NOT ENOUGH
  EVIDENCE labels. No assumption that flat healthy-pad temperatures,
  coolant starvation or capability planner failure appear in real
  gameplay was inferred solely from green fixture tests.

### Next step after ANALYSIS, no changes applied here

1. Build/launch current native client+server (if not yet done),
   reproduce Newtonian I/Y/X with service loads, reactor ignition
   off/on, partial RCS/main loss and heat. Obtain
   `D:/tmp/elite_newtonian_runtime.csv` plus existing system/reactor
   telemetry, particularly throttle before/after PowerBus,
   requested/granted per-consumer MW and thermal state.
2. Address **proven data-flow bugs** before considering any re-tuning:
   service throttle state, persistent ER-5 thermal damage,
   power-aware capability and revision provenance,
   missing cooling module-to-runtime linkage. Preserve accepted
   universe constants and don't change Assisted motion behavior.
3. Use ONE permanent `PROJECT_STATE.md` on `main`;
   this entry is documentation only, not a production patch.

## 2026-10-25 Claude audit follow-up — source repairs on main, native gate pending

User explicitly authorized fixing confirmed Claude audit findings without
changing ANY accepted universe physical constants, ratings, heat formulas,
efficiency curves, or three independent hardware stacks. Only `main`, one
canonical `PROJECT_STATE.md`. **All below are SOURCE changes; none has
been built/CTest'ed in Windows during this pass.** Previous 7/7
Newtonian diagnostics are the baseline BEFORE the following changes.

### Applied — deterministic state / power errors

1. `0898ed67`, `f3d66f87`: PowerBus gets
   `update(bool reactorOnline=true)` (legacy calls unchanged). It
   calculates current aggregate consumer+measured Newtonian RCS demand,
   sets service-reactor throttle FIRST, then samples current actual output
   and distributes it to consumers. An explicit disabled/startup gate
   forces throttle AND consumer allocations to zero, never silently
   restarts a reactor. Handles zero-rated/structurally destroyed reactor
   without divide by zero; clamps emergency critical allocations to
   finite nonnegative remaining MW; removes function-static
   `wasEmergencyMode`. Aux physical RCS demand increases actual reactor
   demand but does NOT preempt critical cooling/life-support; RCS execution
   independently uses free MW after those allocations. Existing
   universe values and priorities are untouched.
2. `246d2ef1`, `d64edc6c`: `ShipCore::init` synchronizes initial
   saved service throttle with actual configured startup throttle
   (previously saved=0.0 but live reactor=0.8); `updateSystems`
   no longer restores stale saved throttle on EVERY frame and explicitly
   gates PowerBus on `m_propulsionReactorsOn && startupSeconds<=0`.
   The 10-second startup duration is unchanged. Firing all reactors OFF
   now genuinely leaves consumers unpowered through PowerBus, not only
   cosmetically setThrottle(0).
3. `af040a70`, `af0519a5`: independent installed ER-5
   `EngineReactorState` gains a `pendingThermalDamage01` accumulator.
   `ShipCore::updateSystems` measures `advanceEngineReactor()`'s
   produced thermal damage, retains sub-`1e-5` increments, and commits
   only completed quanta via `ObjectModuleRuntime::addEquipmentDamage`
   to the canonical equipment Thermal channel. After commit it syncs
   reactor operating damage to canonical module state; only actual
   revisions call `refreshRuntimeState()`. Fixes the previously
   discarded local-only ER-5 thermal damage without altering failure
   thresholds, heat, material, thermal capacity or efficiency.
4. `7508f9e3`: Assisted attitude identification still LEARNS measured
   response, but `learnedAuthority` is now upper-clamped to
   `configured` instead of `1.5*configured`, preventing an estimator
   from granting more angular authority than the physically reported
   bound. No dynamics law or controller time constant changed.
5. `e8fa4897`, `39d3b2ac`, `47870f76`: real
   `ship_power_bus_execution_tests` links production
   `PowerBus.cpp`, `ReactorSystem.cpp`, `ThermalSystem.cpp`, tests
   demand after initial zero throttle, OFF/startup/ON, shortage
   allocation, zero-capacity finite behavior and emergency critical
   priority. Registered in standalone CMake and added to existing
   `scripts/run_newtonian_physics_diagnostics.sh`. Eight focused
   CTests now expected. NEW tests are NOT yet confirmed green.
6. `a69fe13f`, `2ad65cc0`: passive player-only server
   `D:/tmp/elite_power_thermal_audit.csv` logs up to 240 seconds
   with throttle/output before and after PowerBus, active reactor,
   startup seconds, requested/granted services, critical consumers,
   cooling power, RCS actual MW, service/coolant temperature,
   radiator MW, and selected ER-5 temperature, actual
   canonical Thermal damage, output/heat/pad MW. Flush every
   30 systems ticks; no effect on physics. Use WITH existing
   `D:/tmp/elite_newtonian_runtime.csv` when validating gameplay.
7. All source edits are restricted to above power/state/diagnostic
   concerns; Assisted and Newtonian physical ratings, pad/coolant
   routing, RCS gas law, route geometry and equipment constants
   remain unchanged.

### Remain OPEN; do not accept speculative Claude patches blindly

- Full **power-aware, six-axis signed** capability for planner and
  client snapshot. The current scalar `ShipCapabilityBuilder`
  projection still uses installed motor state and may overpromise ER-5
  power (or service-RCS headroom) while executor honors group MW.
  Do not fake an available power percentage or blanket-set all
  surviving motors to zero just because one generator is damaged.
  Need a canonical per-group entitlement/feasibility seam sourced
  from actual ER-5 state and shared by server/client.
- `capabilityRevision=requestSerial` in TWO client route autopilots
  is still not actual equipment capability provenance. Needs
  server-snapshot revision threading and precise replanning semantics.
  Do not change a serial to a different arbitrary counter.
- Engine, ER-5 and service reactor/cooling channels still retain
  their intentionally independent thermal sinks. Claude-proposed
  fractions/pump coupling/Stefan-Boltzmann calibration are new
  unapproved universe law changes, not part of this repair.
- Cooling/radiator/service generator collisions remain incompletely
  connected to actual module damage; RCS geometry uses stable IDs but
  lacks per-nozzle real hull hit volume. Fix physical hit ownership
  separately, without inventing arbitrary geometry.
- ER-5 `maintenance.wearPerOperatingHour` read/use and signed
  capability after irregularity, reactor pressure/gas and tested
  client reconstruction need separate source-level analysis.
- RCS service-reactor generation is demand-driven using last
  actual RCS MW as auxiliary bus request and free output for this
  tick; at step transitions it is possible to experience one-tick
  shortfall. New runtime trace should resolve whether additional
  zero-lag scheduling is necessary without violating priority.

### Immediate native verification and logs — before more edits

```bash
cd /d/__elite/work
git pull --ff-only origin main
bash scripts/run_newtonian_physics_diagnostics.sh
# Expect 8 focused tests, including ship_power_bus_execution
bash build_mingw64.sh 2>&1 | tee /d/tmp/elite_full_build.log
```

Use `set -o pipefail` before the piped build if running manually so
failures propagate. Check `D:/tmp/elite_newtonian_build.log`,
`D:/tmp/elite_newtonian_ctest.log` and `D:/tmp/elite_full_build.log`.
Once native client and server build, run Newtonian I/Y/X and switch ALL
reactors off, restart >10s, burn RCS, then generate/damage ER-5.
Provide `elite_power_thermal_audit.csv` plus
`elite_newtonian_runtime.csv` to validate actual power/thermal coupling.
Never declare gameplay success without these results.

## 2026-10-26 native 8-test gate: PowerBus build syntax blockers fixed, Windows retest pending

- User provided `Вставленный текст(20261026-040120).txt` and
  `elite_newtonian_build(2).log` from MSYS2 MinGW64 after pulling
  `main` through `3535eacc`. The original 7-test prior pass is
  shown at the top of pasted output, but the NEW 8-test script stopped
  at C++ compilation; NO new test result can be claimed.
- Two *independent* errors in newly authored files:
  (1) `src/game/ship/core/PowerBus.cpp:37` had a commented-out
  `// void PowerBus::update(bool reactorOnline)` followed by a live
  function body `{`, triggering GCC
  `expected unqualified-id before '{' token`.
  (2) `tests/navigation_runtime/PowerBusExecutionTests.cpp:39` used
  `game::ship::ReactorDescriptor` although
  `src/game/ship/ShipDescriptor.h` defines `ReactorDescriptor`
  in the **global namespace**; subsequent test calls could not see
  `reactorDescriptor(...)`. The compiler warnings about unused
  coolant/maxOverflowTransferMW/dt, sign-compare, field initialization
  order and unused wasteMW were non-fatal.
- **FIXES on main:** `4c0032af` restores the live out-of-line
  `void PowerBus::update(bool reactorOnline)` definition;
  `c6b99e58` corrects the two test-only type references to
  global `ReactorDescriptor`. Read-only source comparison
  confirms the expected declarations and the only two erroneous
  type references. No universe physics, watts, thermal coefficients,
  cooldown parameters, damage fractions, game control or HUD
  were changed.
- **Next exact native check:**
  `cd /d/__elite/work && git pull --ff-only origin main &&
   bash scripts/run_newtonian_physics_diagnostics.sh`.
  It now includes EIGHT targeted tests; output remains
  `D:/tmp/elite_newtonian_build.log` and
  `D:/tmp/elite_newtonian_ctest.log`.
  If the new power-bus fixture exposes a *behavioral* mismatch,
  analyze production versus expectation rather than adjusting
  tolerances or altering accepted physical constants.
  Once eight pass, validate full client/server by
  `set -o pipefail; bash build_mingw64.sh 2>&1 |
  tee /d/tmp/elite_full_build.log`.
- The core repair from Claude's analysis still awaits actual
  native verification and gameplay `D:/tmp/elite_power_thermal_audit.csv`
  and `D:/tmp/elite_newtonian_runtime.csv`; DO NOT claim
  these subsystems are fully accepted yet. Keep `main` only,
  one canonical `PROJECT_STATE.md`.

## 2026-10-26 native PowerBus plus Newtonian eight-test gate PASS — full build / game pending

- User uploaded `elite_newtonian_build(3).log`,
  `elite_newtonian_ctest(2).log`,
  `elite_newtonian_matrix(2).csv`,
  `elite_newtonian_nozzles(2).csv` and
  `elite_newtonian_summary(2).txt` after fixes
  `4c0032af` and `c6b99e58` on `main`.
- **NATIVE MSYS2 MinGW64 COMPILE/LINK SUCCEEDED**, including the
  production-source `ship_power_bus_execution_tests.exe`.
  CTest reports **8/8 PASS (0.42s)** for
  `ship_control_execution_gateway`,
  `ship_physical_control_models`,
  `newtonian_execution_authority`,
  `engine_thrust_hud_presentation`,
  `ship_control_behavior_oracle`,
  `newtonian_rcs_physical`,
  `newtonian_physics_diagnostics`,
  `ship_power_bus_execution`.
  Nonfatal warnings include unused coolant, unused ER-5 overflow arg,
  unused ThermalSystem update dt, signedness, and member init order.
  None was a test failure.
- **NUMERIC CROSS-CHECK OF UPLOADED CSV**, not merely PASS assertion:
  matrix has 255 samples (85 per Newtonian I/Y/X); nozzle CSV has
  1368 samples (456 each) across 24 IDs/model. For all 57 detailed
  scenario/model pairs, independent sum of individual nozzle force,
  torque (about CoM), electricity and heat matches matrix totals
  within about 4e-9 of CSV rounding. No row used power above its
  stated budget. `power_limited` uses exactly 0.050 MW and creates
  ~2.71 kN against ~12.53 kN requested; `force_saturation`
  requests +450 kN and physically delivers only +48 kN with +402 kN
  residual. Missing nozzle zero availability and electricity;
  gas exhaust/recharge/rearm observed on all three models.
  Common nozzle law explains identical RCS scenarios I/Y/X.
  Main-engine fixture generates -1,040,000 N for the SAME
  requested 4 m/s^2 acceleration on 1/3/4-engine layouts,
  NOT evidence their maximum rated acceleration is equal.
- **What this proves**: the isolated production engine matrix,
  installed-nozzle allocator and service-reactor PowerBus tests
  meet their focused requirements after the Claude-guided repairs.
  **What it does NOT prove**: new ShipCore.cpp server diagnostics
  compile with full client/server; heat/damage persistence in real
  gameplay; reactor startup/shutdown with actual ship consumers;
  autopilot replanning or replicated HUD after reactor/engine damage.
- NEXT NATIVE GATE: from MSYS2 MinGW64,
  `cd /d/__elite/work && git pull --ff-only origin main &&
  set -o pipefail && bash build_mingw64.sh 2>&1 |
  tee /d/tmp/elite_full_build.log`.
  Only after the complete client/server build passes, run Newtonian
  I/Y/X and test reactor off/on (10s cold start), positive/negative
  translation, body pitch/yaw/roll, ER-5 damage/overheat, gas
  exhaustion, and HUD; collect
  `D:/tmp/elite_power_thermal_audit.csv` and
  `D:/tmp/elite_newtonian_runtime.csv`.
  The first is read-only service/ER-5 state, the second includes
  server actuator wrench. Keep any next changes on `main`;
  maintain this SINGLE `PROJECT_STATE.md`.

## 2026-10-26 REAL NEWTONIAN-I PLAYER FLIGHT: RCS authority gap and 14% main, production repair pending native gate

**User complaint**: Newtonian control feels like heavy truck, little RCS
angular/lateral reaction, main thrust HUD tops out around 10%;
Assisted still responsive. User uploaded ALL
`elite_full_build.log`, `elite_newtonian_runtime.csv`,
`elite_power_thermal_audit.csv`,
`elite_attitude_jitter(20261026-041845).csv`.
User earlier prohibited retuning fixed universe physics values without
separate authorization. All changes below preserve installed equipment
force, torque, thermal and power ratings.

### REAL EVIDENCE (not isolated scenario PASS)

- `elite_full_build.log`: canonical **EliteGame.exe and EliteServer.exe
  linked successfully**; no code compile/link failure.
- `elite_newtonian_runtime.csv`: **7395 physical server ticks /
  147.9 s, ALL Newtonian-I (model 3), ModuleRuntime (source 0)**.
  This user trace does NOT certify Newtonian-Y/X gameplay yet.
  24 RCS modules healthy/available after reactor startup.
  One installed aft main reports 100% availability.
- `manual_forward` was nonzero for **637 ticks** and
  `main0_thrust01` ZERO during ALL 637. **IMPORTANT: this is
  NOT proof of a misrouted main engine**: source
  `PlayerInputMapper.cpp` assigns `Num8/Num2` to
  `control.forwardInput` (RCS), whereas `+`/Numpad Plus
  controls `targetSpeedRate` (primary main thrust); `-`
  deliberately does NOT reverse Newtonian main engine.
  The existing runtime CSV unfortunately omitted targetSpeedRate,
  so it could not show what the pilot did with +.
- Physical RCS input demanded **520,000 N** along an axis from
  legacy 2.0 m/s² times 260,000 kg. The 24 installed fixed
  12,000-N nozzles achieve at most about **48,000 N per
  signed translational axis**, **0.1846 m/s²**, NOT 2.0 m/s².
  User requested 520kN while 48kN possible; under continuous
  inputs gas latches OFF and periodically recharges. In all
  7395 samples `gas_depleted` was ~29%, and during
  nonzero `manual_forward` ~65% of ticks were depleted.
- Angular body controller nominal `3.0 rad/s²` asks for
  tens of millions of N·m from fixed nozzles: actual isolated
  yaw moment peaks ~**1.1568 MN·m**, implying at most
  ~**0.0457 rad/s²** around 25.3M kg·m² body yaw inertia,
  i.e. about **2.62 degrees/s²**, far less than the old
  declared 3rad/s² command budget. This REAL hardware limit,
  not merely HUD latency, explains slow large-angle turn.
- Main aft motor physical measured max was
  **0.143229 = 14.32%** actual duty with
  **694.66 MW** electrical load, **100% availability**,
  and **5.2672 m/s²** actual acceleration magnitude (the
  world-z component alone was ~3.458m/s² during oriented flight).
  Motor burn occupied ~1203 ticks, mostly with zero Num8/Num2
  input, entirely consistent with distinct main + input.
  ER-5 generator was rated/available ~5000MW and thus did
  not throttle this 694.66 MW burn. Unclear whether 14.3% is
  limited by command, gimbal/planner, actual ShipParams or
  other path: old telemetry did not capture
  `control.targetSpeedRate` and
  `requestedMainEngineAccelerationMps2`; DO NOT guess.
- `elite_power_thermal_audit.csv` ~9222 systems rows:
  70MW service reactor running near 69.73 MW,
  about **8.7–8.97 MW free for physical RCS**, actual
  RCS power max ~1.44MW for installed 12kN passport;
  coolant and ER-5 logs received. The underpowered RCS
  is **primarily limited by 12kN hardware thrust**, not
  the currently available ~8.7MW bus headroom.
  Future RCS thrust-rating increase must be accompanied by
  electric budget evaluation: 70MW service bus already
  carries ~61MW other consumers. Do not silently raise
  hardware ratings or service-reactor output.

### Production changes made to main (Windows retest PENDING)

1. `ee7fa057`: ShipController no longer artificially derates
   **Newtonian** manual pitch/yaw/roll input to 18% initially,
   ramping up for 0.25s. Newtonian instantly commands available
   authority; 24-nozzle allocator still physically enforces
   max force, moment, gas and electrical budget. Assisted retains
   its ACCEPTED old ramp and control law unchanged.
2. `fa2b3618`: New `sampleNewtonianRcsSignedAuthority`
   uses **actual production 24-nozzle allocator** to sample
   positive and negative X/Y/Z forces and roll/yaw/pitch
   body moments about current CoM, based on installed
   damageable nozzle Engine descriptors. It uses a no-gas,
   no-electrical-limitation PROJECTION, not fictitious
   `3 rad/s²`/2m/s². Actual per-tick gas and power
   enforcement remains unchanged.
3. `d7a2765b`: On native Newtonian
   `ShipCore::capabilities()` cache recomputation, bounded
   signed instantaneous 24-nozzle authority overwrites
   `ShipCapabilitySnapshot` signed strafe/lift and
   pitch/yaw/roll values, and CONSERVATIVELY clamps
   the legacy control-scalar `ShipParams.manoeuvreThrusterAccel`
   and `angularAccel` to minimum feasible bidirectional
   physical values. This prevents continuous commands
   requesting impossible 520kN/60MN*m.
   Shape/damage/mass invalidate the snapshot. No 12kN or
   260t gameplay physical constant changed. Neither
   an arbitrary global multiplier nor thrust scaling was used.
   NOTE: client replication of detailed six-axis + signed
   under-damage/power capability still requires follow-up;
   the server's physical executor remains authoritative.
4. `530994e2`: signed roll projection obeys canonical body
   convention: +roll rotates about body -Z, so positive roll
   authority comes from negative physical Mz, and vice versa.
   Keep axis sign correct under asymmetric RCS damage.
5. `e5689c77`: direct physical regression test probes all
   twelve healthy signed RCS force/moment directions and
   demands that whole-bank destruction makes all projected
   authority ZERO. The test is added to the existing
   `newtonian_rcs_physical` target. Windows compile + CTest
   after the patch is PENDING.
6. `66549670`: Append main
   `targetSpeedRate`, requested main acceleration XYZ/magnitude,
   physical forward main capability, RCS translation/attitude
   capability to new `D:/tmp/elite_newtonian_runtime.csv`.
   This will settle why main thrust sits at 14.32% during
   the player's + input; DO NOT assume max capability was
   physically reached.

### Next validation

```bash
cd /d/__elite/work
git pull --ff-only origin main
bash scripts/run_newtonian_physics_diagnostics.sh
set -o pipefail
bash build_mingw64.sh 2>&1 | tee /d/tmp/elite_full_build.log
```

Need all 8 focused tests green before running current game. Then NEW
Newtonian-I gameplay: explicitly hold keyboard **+** for 3–4s,
release; separately Num8/Num2 (RCS), and S/W/Q/E/D/A
angular keys. Test gas discharge/recovery. Upload new
`D:/tmp/elite_newtonian_runtime.csv` plus
`elite_power_thermal_audit.csv`, focusing
`main_target_speed_rate`, `requested_main_accel_norm_mps2`,
`main_forward_capability_mps2`,
`main0_thrust01`, actual RCS/torque.

Expected outcome of fixes: properly bounded
requested accelerations, immediate Newtonian short-tap torque and
no planner/servo claim beyond real installed nozzles.
**NOT promised**: all desired fighter agility. That would require
user-approved physical re-rating of the 12kN ×24 RCS nozzles,
electrical load and gas mechanics under the existing universe
laws. Current 48kN/260t≈0.185m/s² is fundamentally limited.
Stay on main; maintain this single PROJECT_STATE.md.

## 2026-10-26 native Newtonian authority-patch acceptance: 8 PASS and full client/server build

- User uploaded `elite_newtonian_ctest(3).log`,
  `elite_full_build(1).log`, `elite_newtonian_summary(3).txt`,
  `elite_newtonian_matrix(3).csv`, `elite_newtonian_nozzles(3).csv`
  following source fixes `ee7fa057`, `fa2b3618`, `d7a2765b`,
  `e5689c77`, `66549670`, `530994e2` on **main**.
- **NATIVE WINDOWS COMPILE/LINK PASS**: `build/EliteGame.exe` and
  `build/headless_server/EliteServer.exe` both linked (client/server
  changed `ShipController.cpp` and `ShipCore.cpp` compiled).
  Only upstream CMake warnings; no compile or link failure.
- **NATIVE FOCUSED CTEST PASS: 8/8, 0 failed, 0.38s.**
  Includes the new `newtonian_rcs_physical` checks for signed
  actual-nozzle envelope and zero authority after complete RCS loss,
  alongside `ship_power_bus_execution` and all earlier physical
  solver/HUD/controller checks. This confirms test contracts only,
  NOT Newtonian response in running server gameplay.
- Independent user CSV numerical verification (not merely CTest PASS):
  255 scenario/time rows (85 each I/Y/X), 1368 nozzle rows,
  sums of per-nozzle F, M, electricity and heat match corresponding
  matrix totals within about `4e-9` in CSV rounding. There are
  zero MW-budget overshoots. `force_saturation` requests +450kN
  but physically delivers +48kN along X; `power_limited`
  consumes exactly 0.050 MW and cannot deliver requested combined
  wrench. Full gas burn/recharge/rearm scripts pass on all three
  layouts. All three matrices sample the SAME installed RCS geometry.
- **Current unverified gameplay concerns** from previous real
  Newtonian-I flight: sluggish maneuvers and body turning,
  gas starvation under sustained RCS, and approx 14.32%-peak
  indicated main-engine duty. This new upload contains NO post-patch
  `elite_newtonian_runtime.csv`, `elite_power_thermal_audit.csv`
  or `elite_attitude_jitter.csv`, so neither agility improvement
  nor main-thrust root cause is yet established.
- Next required targeted MANUAL game flight: Newtonian-I, cold
  engine-off/on if useful, hold **+ (or NumPad +)** independently
  for 3-4s for MAIN (not `Num8`), release, then separately
  `Num8/Num2` longitudinal RCS, `Num4/Num6` lateral,
  `Num9/Num3` vertical, `W/S`, `Q/E`, `A/D`
  attitude, with enough time for gas reset. The new runtime
  trace includes `main_target_speed_rate`,
  `requested_main_accel_norm_mps2`,
  `main_forward_capability_mps2`, actual `main0_thrust01`,
  24 physical RCS nozzles and gas state; pair with
  `elite_power_thermal_audit.csv` to isolate power throttling.
  Test I first; expand to Y/X only after I behaves as intended.
- If the pilot still finds 0.185m/s² max translation and
  0.046rad/s² yaw too sluggish, re-rate actual RCS, service MW
  and game-gas jointly only after explicit approval. **No
  universe physical values were changed as part of this pass.**
  Stay on main and update this only canonical `PROJECT_STATE.md`.

## 2026-10-26 REAL Newtonian-I/Y/X follow-up: + requests 36.775 m/s2, delivered 14.32%, dual-axis input absent

- User observed Newtonian control STILL unresponsive and claimed it
  cannot yaw/roll/pitch simultaneously; one installed main is still
  capped around 14% while same CR-1/ER-5 passport works at full
  rating in Assisted. Uploaded:
  `elite_power_thermal_audit(1).csv`,
  `elite_newtonian_runtime(1).csv`,
  `elite_attitude_jitter(20261026-060551).csv`.
  The analysis below uses ACTUAL CSV numbers, not isolated test PASS.
- New `elite_newtonian_runtime` has **4316 physical ticks /
  86.32 sec**. Samples by actual selected hardware:
  Newtonian I/model3=3004; Y/model4=109; X/model5=1203.
  P audit contains 5438 systems rows, including an initial
  Assisted-I/model0 1122 rows. Attitude trace 4501 rows.
- **Main driver:** model3 sees `main_target_speed_rate=+1`
  for **458 ticks**, and
  `requested_main_accel_norm_mps2=36.77493668` throughout
  (exact healthy single-CR1 rated 9.561 MN / 260t).
  Physically DELIVERED magnitude is only **5.2672333m/s2**
  = `main0_thrust01=0.14322888`, consuming
  `main0_electric_MW=694.66008`. Motor
  `main0_available01=1.0` on all model3 ticks;
  engine is NOT commanded to 14% by the pilot/input.
  This is stable through all + held ticks at ship speed 0-48m/s,
  nowhere near 500m/s speed cap. Difference is downstream,
  likely generator-group MW derating:
  694.66/4850 = 0.1432289, implying a ~694.66MW
  ENTITLEMENT if the unscaled CR-1 draw is 4850MW.
  But existing logger did not record *pre-scaling* group.availableMW
  nor all reactor equipment-damage causes. **Do not claim a
  damaged generator as proven**.
- **Equal passport:** `CobrettyCR1VectorMainDrive` rated
  9,561,483.75N and 4,850MW; installed ER5 rated
  5,000MW. Intact rated ER-5 can power full healthy CR1,
  independently of Assisted/Newtonian doctrine.
  Audit `ER0_temp_K` on model3 stays around
  900.0-901.36K, thermal-damage=0; this is NOT sufficient
  to certify other equipment causes (impact/laser/wear/internal).
  PowerBus service free MW=8.7-8.97 and full nozzle electricity
  <1.45MW: not the primary hard torque-force shortage.
- **Manual concurrency:** among 4316 Newtonian physical frames,
  NOT ONE has two simultaneously NONZERO
  `manual_pitch`, `manual_yaw`, `manual_roll`
  input values. User's stated simultaneous button presses were
  not actually recorded by pilot state in this sample, so
  cannot conclude whether keyboard/input/network lost combos
  vs user sequence. BUT real output often contains 2 nonzero
  torque components from cross-coupling/damping; the underlying
  vector allocator does not require serial actions. A regression
  test for SIMULTANEOUS requested yaw+roll/pitch+roll/pitch+yaw
  was newly added; native outcome still PENDING.
- **Sluggishness quantified:** installed 24 nozzle motors
  12kN each. One-axis feasible translation ~48kN
  -> 0.184615 m/s2 for 260t. Yaw max actual torque
  ~1.1568MNm -> 0.045678rad/s2 for 25.3Mkgm2.
  Model3 with pitch held 630 ticks had gas-depleted
  on ~64% of them; model5 roll held 396 ticks,
  gas depleted ~63%. On those depleted ticks no
  physical RCS command can be delivered. Current
  model/gas physical constants were NOT changed.
- **Committed diagnosis and contract tests on main**:
  `8a7fa2de` appended to read-only
  `D:/tmp/elite_newtonian_runtime.csv` per-tick
  (1) actual motor temperature/total module damage,
  (2) pre-execution ER5 *group.availableMW*,
  (3) ER5 temperature/total damage/function.authority01,
  (4) rated motor input MW and thrust N. This is the first
  instrumentation able to locate 14.32% scaling origin
  instead of inferring it from POST-scaling heat/telemetry.
  `4d9f4555` added an integration test to existing
  `ship_control_execution_gateway` verifying a healthy
  REAL CR-1 passport plus 5GW ER5 supply yields full
  nominal thrust, 4850MW, on `+` in Newtonian-I.
  `d0a46fcc` added 24-nozzle simultaneous two-axis
  positive moment checks (pitch+roll, yaw+roll, pitch+yaw)
  in `newtonian_rcs_physical`, without newtonian physical
  force/duty/passport changes. Compiler/test results PENDING.
- No Assisted source changed; NO passport values, thermal
  formulas, gas rates or actual output scalers modified.
  Explicit request from user to make ship maneuverable
  will require separately agreeing the RCS physical thrust,
  the 70MW service power budget and 0.20/sec gameplay gas
  if ~0.18m/s2 translation / ~0.046rad/s2 yaw remain
  unacceptable after root cause is known.
- **Next short native gate**:
  `cd /d/__elite/work && git pull --ff-only origin main &&
  bash scripts/run_newtonian_physics_diagnostics.sh`
  followed by
  `set -o pipefail; bash build_mingw64.sh 2>&1 |
  tee /d/tmp/elite_full_build.log`.
  Native 8/8 last PASS is BEFORE these new tests.
  If green, run Newtonian-I + for ~3 sec and
  physically HOLD two manual attitude keys at the SAME TIME
  (e.g. Q+D and W+A). Send
  `D:/tmp/elite_newtonian_runtime.csv` and
  `D:/tmp/elite_power_thermal_audit.csv` afterward.
  Compare `main_ER5_group_available_MW` with nominal
  rated 5000MW and `main_ER5_total_damage01` before
  any claim of damaged generator or other root cause.
  A 5GW budget + undamaged engine with 14% executed
  thrust would implicate solver/execution scaling, not ER5.
  Keep main only and this single PROJECT_STATE.md.

## Important remaining architecture work (do not rush)

- Fore-only propulsion (`aft` completely dead, `fore` alive) is
  inconsistent: manual `DynamicMotionSystem::applyLocalFrameInput` chooses
  -nose as travel forward, while `FlightControlProfile`/MotherComputer
  demand safe AFT working-forward thrust, currently reject this doctrine.
  Needs coherent signed forward/backward banks AND flip-and-burn control;
  an isolated sign flip or changing axis label is unsafe.
- Command → Request → Executed still needs end-to-end inspection in
  damaged/reverse and ordinary gameplay paths. The installed MAIN-engine
  post-execution speed clamp was fixed and passed its targeted Windows
  regression; legacy/non-installed motion and low-speed BrakeToStop/RCS
  channels still require separate physical audit.
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

## 2026-10-25 Assisted stationary hull-turn fixture: MODE ERROR FOUND, source-only repair

- **2026-10-25 user feedback after `90833c16`:** Windows build
  **succeeded**. User wrote "сборка - прошла" without separate CTest
  `100% tests passed` summary; do not assert native
  `client_route_autopilot` PASS until test output is confirmed.
  Proceed to requested in-game stationary and ~100m/s obstacle-escape
  tunnel verification, checking selected course versus first tunnel
  tangent and projected live route entry.


- User's most recent native `client_route_autopilot` CTest was RED on
  `Assisted autopilot failed to settle on commanded turn angle`.
  This is `testAssistedAutopilotCapturesTurnsWithoutOvershoot`, which
  simulates 5/15/45/90/-45/-90 degree hull turns at ZERO velocity, each
  over 400*0.02 = 8s, with a keyboard actuator ramp, angular inertia,
  and real normalized pitch/yaw inputs. The failure did NOT originate in
  the new `SpaceState.cpp` safe-course direction patch.
- ROOT CAUSE verified by reading control code and independently reproducing
  the discrete 1D yaw physics: the test omitted
  `AutopilotControlRequest.terminalAttitudeHold`, so a PURE STATIONARY
  HULL POSE manoeuvre was routed through moving velocity-course steering,
  whose max 10-degree hull lead intentionally limits turning rate.
  At 90 degrees, the reconstruction ended ~77.10 degrees after 8s;
  with terminal attitude HOLD (full desired hull pose at zero velocity)
  it ended ~89.98 degrees after 8s, overshoot <0.25 degrees. This
  reproduces input-hold ramp, angular damping, the 0.5s attitude servo,
  and online angular-authority estimation. It is a calculated local
  result; NOT a Windows CTest result.
- Commit `90833c16` changes ONLY
  `tests/navigation_runtime/ClientRouteAutopilotTests.cpp`:
  adds `request.terminalAttitudeHold = true;` in this stationary-hull
  turn fixture, retaining 8s and the original stringent 0.25-degree
  settlement AND overshoot requirements. This is a semantic test
  correction, NOT an accommodation by weakening timing or thresholds.
  The distinct ordinary MOVING velocity-course tests remain unchanged.
  The real `AssistedAutopilot.h` and physical controller remain untouched.
- This repair is **SOURCE ONLY / Windows test pending**.
  NEXT run only `client_route_autopilot_tests` with build-gated CTest.
  If it passes, the other four focused planner tests were already green
  in the prior Windows 4/5 run. If another failure emerges, diagnose
  that specific contract and avoid blind threshold chasing.
  The real priority remains an in-game blocked-entry tunnel check at
  rest and ~100m/s, comparing escape vector, first tunnel tangent and
  live late-bind entry. Native gameplay not yet accepted.
- Source history: Assisted damped inner-loop entered main on Oct 22,
  and the current safe-course-to-planner fix `1bc9742f` changes ONLY
  the initial tangent. Earlier sequential assertions were legacy;
  do not infer a new physics regression from each now-reached test.

## Immediate next validation — blocked route entry, THEN general regression and gameplay

**Priority now:** rebuild `client_route_autopilot_tests` and run its
single native CTest after `90833c16`. If green, proceed to actual
game obstacle escape and safe initial-tunnel course, stopped and at
~100 m/s. Do not claim runtime acceptance based solely on CTests.


- The current Windows native `ship_control_execution_gateway` CTest is
  **PASS (user-reported)**. The old six-model in-game oracle was previously
  **84/84 PASS** on its separate older gate. Neither proves the entire
  damage/autopilot/reactor gameplay.
- PRIORITY BEFORE broad regression: the Assisted obstacle-avoidance
  route-start patch above must pass its targeted Windows navigation
  tests and in-game route-entry acceptance at both zero and nonzero speed.
  The course requested by planner must match the one physically selected
  by PreplanningSafetyCourse; a stale connector must be rebound rather
  than turning back into the obstacle.
- NEXT: build and run existing focused regression targets in the
  `tests/navigation_runtime` suite (without inventing new tests):
  `ship_control_behavior_oracle`,
  `ship_physical_actuator_plant`,
  `ship_propulsion_state`,
  `newtonian_rcs_physical`,
  `working_thrust_axis_analysis`,
  `ship_mobility_assessment`,
  `assisted_authority_contract`,
  `hull_attitude_control`,
  `navigation_execution_replan_policy`,
  `client_route_autopilot`,
  `engine_thrust_hud_presentation`,
  `assisted_layout_flight_matrix`,
  `ship_physical_corridor_matrix`.
  Run the existing `equipment_damage_failure_pipeline` architecture
  contract test as the hardware/damage companion.
- Then native build `EliteGame.exe` and `EliteServer.exe`, and actually
  fly: healthy Assisted-I response; single-aft-engine outage with
  Ctrl+Alt+Shift+F and safe working-thrust ray; restart/off reactor
  Ctrl+Alt+Shift+I and no phantom acceleration; RCS and route-following
  while degraded. Collect `D:/tmp` logs if anything fails. Do not
  assert gameplay PASS until the user observes it.
- Do not spend user time verifying the optional PASSED/FAILED dialog.
  Do not begin unrelated fore-only or large capability migration before
  this flight gate. Avoid tests-for-tests and parallel state files.


## 2026-10-26 explicit RCS power-source audit (user request: service reactor -> engine ER-5)

- User requested to disconnect Newtonian manoeuvring nozzles from the ship/service reactor and feed them from the engine reactor. Inspection of **current main source** shows this is ALREADY implemented; do not create duplicate connections or another power owner.
- In `src/game/ship/core/ShipCore.cpp` Newtonian installed 24-nozzle executor computes each ER-5 group's spare supply after the executed main-engine MW demand; the argon compressor reserves up to 2MW from those groups, then the physical RCS allocator is constrained by remaining ER-5 MW. Executed per-nozzle electrical draw is assigned to `m_newtonianRcsEr5DrawMW` per reactor group; generator operation/heat/aging uses the corresponding draw in the ER-5 update.
- The ship/service `PowerBus` explicitly sets auxiliary RCS electrical demand to **0 MW** before update (`m_powerBus.setAuxiliaryDemandMW(0.0)`), so RCS does NOT draw from the 70-MW ship reactor. If engine reactors are off, starting up, or have no available watts, nozzle mounting / available electrical power gates their output; argon compressor recovery likewise requires ER-5 watts.
- This is a **source audit of main**, not a new C++ code change or new Windows build/test pass. No C++ modifications are necessary to carry out this already-satisfied wiring request. No physical ratings, thermal rules, main-thrust priorities or Assisted flight logic were altered.
- OPEN: in real Newtonian-I flight, prior 14.32% main duty and poor attitude response remain unresolved by this audit. Existing trace fields `main_ER5_group_available_MW`, main requested versus executed thrust, `rcs_actual_electrical_MW`, and compressor MW can distinguish generator derating from RCS activity. Run targeted native build/flight before claiming behavior repaired. Keep commits on `main` and this sole ongoing context file.


### 2026-10-26 clarification: ER-5 cooling versus Newtonian argon compressor

- User correctly distinguished the **Newtonian RCS argon pressure-recovery compressor** (2 MW ER-5 load) from any **ER-5 thermal cooling compressor**. The former exists only to recover RCS working-fluid manifold pressure; it does not cool the reactor, and Assisted does not require that compressor.
- Verified `src/game/equipment/reactor/EngineReactorUnit.h`: each ER-5 has its own 10m x 10m radiation pad, heat goes to `depositedToEnginePadsMW`, excess remains in its thermal mass (`lastStoredHeatMW`), and `overflowToSharedCoolantMW=0.0` / `lastCoolantHeatMW=0.0`. `ShipCore.cpp` passes `advanceEngineReactor(...,padHeadroomMW,0.0,dt)`; no engine-reactor emergency common-coolant transfer is implemented.
- Future design suggestion only, NOT authorized implementation: optional emergency heat exchanger/pump transferring ER-5 heat into shared coolant if pad capacity is inadequate; strictly constrain by actual pump and radiator surplus, prevent heat double-counting. First verify nominal ER-5 pad heat balance versus real electrical load. Do not confuse working-fluid compression with cooling, or add phantom generator cooling hardware.
- This clarification is documentation only, not a gameplay fix or native test PASS. Remain on main.


## 2026-10-26 cooling architecture clarification — current-source audit, design NOT yet changed

- User requested end-to-end clarification of confusing ship thermal circuits. Read current `ShipCore.cpp`, `CoolingSystem.cpp`, `ThermalSystem.cpp`, `EngineReactorUnit.h`, ER-5 preset and `UniverseThermalRadiation.h` from main.
- CURRENT three thermal scopes: (1) independent 70MW ship service reactor uses temperature/hysteresis pump and common coolant/radiator network; (2) each installed ER-5 5GW engine generator uses its OWN dedicated radiation pad and thermal mass, zero heat overflow to common coolant; (3) main engine nozzles radiate some heat from their own pad and conduct the remainder to common coolant. A 2MW Newtonian argon working-fluid compressor is NOT an ER-5 cooling compressor.
- Configured shared radiator array at ShipCore init is 160 panels/1600m2; `CoolingSystem::update` directly radiates from shared coolant independent of ship-reactor pump transport, while reactor-to-coolant path is pump controlled. This dependency distinction must remain explicit.
- ER-5 current installed preset has **padSideM=11.0m**, not the earlier design-discussion 10m. Universe law `PropulsionPadCoolingMWPerM2=6.0`, so 121m2 x 6=726MW rating at design temperature/intact pad. At full 5000MW electrical / 0.90 efficiency plus 2MW standby reactor heat is ~557.56MW; nominal rated thermal headroom ~168.44MW **only at design temperature**. `advanceEngineReactor` cooling depends on temperature and pad integrity; surplus heat raises local ER-5 temperature, derates and damages it. No emergency shared coolant exchanger/pump exists.
- OPEN design decisions: reconcile whether reactor pad should be 10x10 or actual 11x11; inspect main drive conducted/radiated balance and 24 RCS actual waste heat sinks; specify optional ER-5->shared coolant emergency exchanger with bounded pump/radiator capacity ONLY on user approval. Do not mix pump, compressor, radiating pad, or coolant concepts. No C++ source change, native test, or runtime PASS claimed from this audit. Maintain main and this document.


## 2026-10-26 AGREED DESIGN DIRECTION: two thermal/electrical groups and emergency cross-cooling (not implemented)

USER PROPOSAL: Model exactly two top-level ship groups:
1. PROPULSION GROUP: installed main and Newtonian RCS actuators, ER-5 propulsion generator(s), own engine and generator heat-emission pads, and own coolant/circulation pump. In ordinary operation this group dissipates its heat locally to space. The RCS argon pressure-recovery compressor, when present, belongs to propulsion but is NOT a coolant pump.
2. SHIP SERVICES GROUP: 70MW ship generator/reactor, life support, weapons/lasers, mechanical actuators, radar, other consumers, a generator-to-coolant transfer pump, shared coolant loop and radiator panels. Supply remains independent from propulsion.
3. Cross-link is EMERGENCY THERMAL COUPLING only, normally isolated: a valve/pump/heat exchanger may transfer **surplus propulsion heat** to the ship common coolant if main engine, ER-5, or propulsion radiating pads are damaged or thermal headroom is insufficient. Its purpose is survival and avoiding runaway/explosion, NOT guaranteeing rated thrust. Bound transfer by temperature gradient, actual exchanger/pump capability, ship coolant/radiator available headroom and finite thermal capacity. No heat or MW duplication and no automatic propulsion electrical bus coupling.
4. When ship-side heat sink cannot accept full surplus, unaccepted energy remains in the originating propulsion module; thermal throttling/shutdown is required. Prevent ship coolant overheating and maintain emergency isolation/hysteresis. Damage/temperature are read from authoritative installed module runtime, not a new parallel state.
5. Existing SOURCE DOES NOT YET MATCH: main-drive conducted heat currently feeds shared coolant during NORMAL flight; ER-5 has independent pad but no overflow; RCS thermal-sink path requires verification. Propulsion circulation pump/heat exchanger not yet implemented. `CoolingSystem` currently has ship-reactor temperature-controlled pump and direct panel radiation, which need careful topology-preserving adaptation. Keep existing universe heat laws and physical ratings until agreed/verified.
6. IMPLEMENTATION STATUS: architecture agreed at concept level only; no C++ source edits, builds or tests are claimed for this decision. Next: define precisely where each heat source deposits heat; power/flow/damage for propulsion pump, return path and priorities; make smallest incremental code migration and retain accepted Assisted gameplay. Remain on main. Keep ONLY PROJECT_STATE.md as canonical progress context.


## 2026-10-26 implementation checkpoint — propulsion coolant separation (source-only, incomplete)

- `f000f6df`: introduced `ShipCore::m_propulsionThermal`, a finite coolant store separate from the ship-service `m_thermal`. Initial mass/temperature currently copied from the ship thermal descriptor, pending a propulsion-loop equipment passport.
- `b69aa85d`: main-engine and Newtonian RCS coolant temperature and conducted thermal energy now connect to propulsion loop, not ship-service radiator coolant. Engine pads continue to radiate independently; ER-5 still uses its dedicated pad; service reactor, auxiliaries and common radiator remain on ship coolant. No new heat was invented or disposed of.
- INCOMPLETE/NOT ACCEPTED: the new propulsion thermal reservoir does not yet have a pump or dedicated coolant-to-pad heat exchanger; emergency valve into the ship loop not yet implemented. Sustained conduction into this reservoir may overheat main/RCS; **do not claim flight-ready**. No native compilation or tests performed in this session. Finish pump/heat sink and bounded emergency coupling before gameplay acceptance. Maintain only main and this file.


## 2026-10-26 cooling follow-up: physical source audit + ER-5 emergency transfer accounting groundwork

- IMPORTANT CORRECTION to previous checkpoint: `EngineUnit.h::advanceEngineOperatingState` currently sets `step.coolantHeatMW=0.0` for installed main/RCS motors because dedicated hot-emitter pads handle their heat and the remaining energy stays in each motor's thermal mass. Thus the new `m_propulsionThermal` is currently a separate reservoir but is NOT a demonstrated engine heat load. Do not invent a second shared pad sink or claim the reservoir cools engines. The previously rerouted `transferredMW` may be zero by design. Future implementation must define a real damageable thermal-transfer mechanism before using the reservoir.
- Commit `cad6bed8` edits ONLY `src/game/equipment/reactor/EngineReactorUnit.h`: the existing, previously unused `maxOverflowTransferMW` parameter now permits a strictly capped emergency heat diversion from ER-5, only from the heat remaining after dedicated reactor-pad emission. `overflowToSharedCoolantMW`, `lastCoolantHeatMW`, temperature integration and `lastStoredHeatMW` agree so diverted watts are subtracted ONCE from local stored heat. Default `maxOverflowTransferMW=0` at `ShipCore.cpp` caller is unchanged, so CURRENT PLAYER SHIP has NO connected emergency path and no free heat sink.
- This is an internal thermodynamic accounting mechanism, NOT an installed pump, valve, heat exchanger or ship heat-sink claim. Production must NOT supply positive capacity until actual installed/damageable emergency hardware, the direction/temperature gradient and receiver radiator headroom are evaluated, and exactly this output is deposited into service coolant. Main/RCS emitter heat MUST NOT be counted twice.
- Native compiler/CTest/gameplay acceptance remain UNVERIFIED. Next: add focused conservation regression test using positive transfer allowance vs zero and damaged pads; then install real pump/valve topology and boundary-limited cross-link, within main-only commits. Preserve all installed universe physics constants.


## 2026-10-26 validation instrumentation checkpoint — two-loop thermal telemetry, implementation still incomplete

- After reviewing current main, `EngineUnit.h` main/RCS engine thermal life always records `step.coolantHeatMW=0.0`; engine waste heat radiates from its own pad and residual stays in its own thermal mass. Do NOT invent an extra pad sink. ER-5's new emergency overflow calculator from prior commit is still called with ZERO capacity in `ShipCore.cpp` and thus no rescue heat path is activated.
- Commit `3e9f42f3` adds five read-only CSV columns to existing `D:/tmp/elite_power_thermal_audit.csv`: `propulsion_coolant_T_K`, `propulsion_stored_MJ`, `ship_coolant_stored_MJ`, `ship_radiator_potential_MW`, `emergency_transfer_MW`. The last column is explicit ZERO until installed hardware and a bounded transfer actually exist. Existing log columns preserved in place. This is diagnostic telemetry only, not HUD presentation or an operational valve.
- IMPORTANT: request to finish pump + emergency architecture is **NOT YET SATISFIED**. Missing: damageable propulsion pump/heat exchanger descriptors bound to canonical ObjectModuleRuntime; genuine heat-transfer mechanism from installed overheated engines to propulsion loop; coupling constrained by equipment integrity, temperature gradient and shared radiators; player HUD display of this telemetry. Do not claim finished or native PASS. Windows build and runtime audit still pending.
- Next user-side validation for this instrumentation after native rebuild: inspect CSV new columns and confirm no cross-loop transfer (zero) while intact engine pads radiate, and ER-5 pad heat and temperature are reported correctly. Continue main only and this canonical state file.


## 2026-10-27 Claude architecture review and selective adaptation (SOURCE ONLY)

- Read user-supplied Claude review and `elite_emergency_thermal_bridge.patch`; Claude analyzed OLD HEAD `da28fa8` and had NOT seen later project commits `cad6bed8`, `3e9f42f3`, etc. Applying patch wholesale would overwrite existing work. Its readout points to real limitations: installed main/RCS `EngineUnit.h` has `coolantHeatMW=0`, so `m_propulsionThermal` presently does not model a physically connected engine loop; ER-5 pad damage is not bound to real installed equipment; radiator size comments need comparison against actual descriptor; existing some thermal tests have stale laws/expectations. Do NOT claim those fixed.
- ACCEPTED: bounded emergency thermal bridge math and hard interlocks (damage authority, power, sink, gradient, rated transfer, hysteresis) and exact ER-5 heat-energy accounting. REJECTED for now: Claude's direct `ShipCore` wiring (it references absent `propulsion_thermal_bridge` module and draws from previous-tick ER-5 budget; no 6-model replication, no real engine/RCS heat path).
- `1e61ff1e`: ER-5 `advanceEngineReactor` now permits bounded emergency removal of *previously stored* heat above idle as well as current surplus, respecting granted maximum MW and dt. All normal `ShipCore` calls still grant **zero**, so no gameplay change to the coolant bridge.
- `b5b1f445`: new pure `EmergencyThermalBridge.h` isolated heat-only bridge model, with Off/Auto/ManualOpen modes, equipment authority, power budget, gradient, source extractability, ship sink/radiator headroom and hard overheat interlocks. It is not an installed ship object.
- `f685dd15`, `731deff3`, `79bcd92d`: added and registered focused `emergency_thermal_bridge` test for interlocks, limits, ER-5 heat accounting and existing default isolation. These are SOURCE commits, neither native Windows build nor CTest PASS demonstrated here.
- NEXT REQUIRED: run Windows MSYS2 `git pull --ff-only origin main`, build `emergency_thermal_bridge_tests`, `ctest -R emergency_thermal_bridge --output-on-failure` and full native ship/server build before proceeding; repair build failures as actual evidence dictates. THEN choose actual damageable exchanger/pump module descriptor, wire it through six I/Y/X ship descriptors and networking, give ER-5 pads real damage/health, integrate sink heat with correct service headroom and telemetry. Never activate bridge with a phantom module or phantom watts. Only `PROJECT_STATE.md` records this checkpoint; main only.


## 2026-10-27 pad damage chain audit and focused HUD fix

- User correctly recalled radiator-pad damage and HUD representation. Verification: MAIN/RCS emitter `EngineUnit.h` models `emitter.surface.intactArea01` and surface wear, derates pad cooling using actual effective area, and propagates reduced cooling into thermal derating. `EngineThrustIndicatorRenderer.cpp` already drew individual main-engine radiator pad marker and integrity percentage, BUT until today partially damaged markers always had the same neutral label color (only zero was dead).
- Commit `3fb0e964` changes only main-engine pad marker COLOR mapping: integrity >=70% green, 30%-70% yellow, below 30% hot/red, zero dead. It reads existing `hasEmitterIntegrity` and `emitterIntegrity01`; no new state, no physics constants, no gameplay forces changed. Native Windows compile/HUD screenshot pending.
- ER-5 is DISTINCT: `EngineReactorUnit.h` supports `remainingPadCapacityMW`, but `ShipCore.cpp` currently supplies unconditionally `6.0*padSideM²` (rated full capacity). There is NO separate installed ER-5 pad health in `EngineReactorState` or `EngineReactorStatus`. The reactor module's aggregate damage is not equivalent to pad-area damage. Thus ER-5 pad hit/damage -> capacity -> HUD marker is genuinely missing. Do not claim it is fixed just because main-engine pad works; first map appropriate installed module/impact and replication ownership, then add an ER-5 pad integrity field and physical derating without inventing separate sources of truth.
- Ship COMMON radiator panels have `CoolingSystem::RadiatorPanel.health` and are enumerated into `ShipCoreStatus.cooling.panels`; overall damage counts are reported. The service circuit is distinct from engine/reactor pads.
- Keep main-only; update single canonical `PROJECT_STATE.md` each step.


## 2026-10-27 ER-5 pad damage first physical connection (SOURCE ONLY)

- User requested to implement missing ER-5 pad damage; inspection verified main-engine emitter integrity was already modeled and HUD-colored (recent `3fb0e964`), while reactor pad had no physical damage link.
- Commits `f7d79a76`, `0d6d606b`: `EngineReactorState::padIntegrity01` (default 1) is stored INSIDE the canonical installed `ObjectModuleRuntime` reactor operating state. `ShipCore::applyDamage` now subtracts real normalized hit volume health loss from this ER-5 pad area proxy when the ER-5 module is hit; `ShipCore::updateSystems` multiplies ER-5 rated pad capacity by surviving integrity. Thus a damaged reactor module can reduce reactor cooling and increase temperature/thermal derating. This is explicitly a PROXY until pad-specific hit geometry is created, not a separate hit-surface implementation; existing ER-5 mechanical damage remains unchanged.
- Commit `df54060b` adds `EngineReactorStatus.padIntegrity01` populated by `ShipCore::getStatus()` from real reactor operating state; `e3695410` appends per-reactor `ER{i}_pad_integrity01` fields to `D:/tmp/elite_power_thermal_audit.csv` while preserving prior columns.
- OPEN BLOCKER: client HUD drive presentation is built via other cockpit/replication snapshots; the new ShipCoreStatus field is NOT YET proven to reach client presentation or draw a reactor-pad damage marker. No claim of visual HUD PASS. Verify `EngineThrustHudPresentation.h`, cockpit drive snapshot and `WireDataSchema.h` and implement end-to-end replication before acceptance.
- SOURCE EDITS ONLY; no native build/CTest/in-game acceptance executed. Run Windows full build and damaged ER-5 test; look for pad integrity decline, reduced pad MW, temperature rise and thermal throttling (while holding electrical draw constant). Keep main only and this one PROJECT_STATE.md.


## 2026-10-27 ER-5 pad replication and HUD mapping repair (SOURCE ONLY, native PASS pending)

- Follow-up to ER-5 pad hit-area proxy and reactor status telemetry: audit found `EngineReactorStatus.padIntegrity01` was NOT in `WireDataSchema.h`, not in `MotherComputerDriveHudGroup`, and absent from engine HUD presentation. Therefore server pad damage could not appear on client. Fixed data path in main:
  - `99f6ba62` wire schema now encodes reactor pad integrity with status.
  - `354f6447` MotherComputer cockpit drive state has explicit hasPadIntegrity and padIntegrity01.
  - `721758f1` MotherComputerClientAdapter copies true server status integrity when corresponding installed ER-5's status is valid.
  - `2358f957` EngineThrustHudPresentation retains those fields end-to-end.
  - `7cae4177` draws a discrete ER-5 radiator status marker next to reactor HUD square, green >=70% intact, amber 30-70%, hot/red below 30%, dead at 0. No unverified healthy color when status missing.
- Existing physical cooling derating in ShipCore multiplies ER-5 pad power by runtime padIntegrity01, and CSV logs ER{i}_pad_integrity01. Distinguish a hit to whole ER-5 module from true pad-only hit: per-pad collision topology remains an open requirement. No changes to autopilot or thrust laws.
- CRITICAL: no MSYS2 Windows compilation, complete game run or screenshot was possible through remote GitHub actions here. Must run build, targeted HUD presentation and thermal tests, plus an actual damaged ER-5 flight and compare HUD to CSV to confirm acceptance. Repair failures before claiming PASS.

- Additional test commit `f486f90a`: existing `EngineThrustHudPresentationTests.cpp` now asserts a 42% ER-5 radiator integrity in MotherComputer cockpit sample survives conversion into actual engine HUD presentation, rather than being discarded. This test is source-only and has not been compiled/executed here.


## 2026-10-27 Windows full-build failure: standalone Asset Editor unresolved symbol

- User provided actual MSYS2 MinGW64 Ninja build transcript (Debug), configure succeeded and build reached 435/439. `EliteGame.exe` link step completed and runtime was staged; overall Ninja build FAILED because standalone `EliteAssetEditor.exe` linking reported `undefined reference to game::ship::plant::physicalControlSlots(PhysicalControlModelId)` from `EliteCobraMk1.cpp:1195`. Large Embree/Eigen and Windows-header messages are warnings, not the blocker.
- Root cause in root `CMakeLists.txt`: editor directly compiles `EliteCobraMk1.cpp` but does NOT include physical-control-model translation units which implement `physicalControlSlots`; main game already includes them. Commit `e9e78f01` adds `AssistedPhysicalControlModel.cpp`, `NewtonianPhysicalControlModel.cpp`, and `ShipPhysicalControlModels.cpp` to *editor* sources only. No game physics, render, HUD or network changes.
- STATUS: CMake fix source committed to main; Windows relink/full Ninja PASS still PENDING user rerun. Run `git pull --ff-only origin main && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && set -o pipefail && cmake --build build -j 4 2>&1 | tee /d/tmp/elite_build_retry.log`; if succeeds, proceed with ER-5 damaged-pad gameplay/HUD/CSV acceptance. Single canonical PROJECT_STATE.md maintained.


## 2026-10-27 deterministic ER-5 fault matrix diagnostic (source-only)

- New dedicated executable in `tests/architecture_contracts/Er5FailureMatrixScenario.cpp`, target `er5_failure_matrix_scenario`, registered CTest `er5_failure_matrix`; commits `59221744`, `ce6ead97`, `2ae8a336`.
- Purpose: user has no manual in-game ER-5 failure controls and engine OFF is visibly ambiguous with FAILED. Standalone deterministic matrix records actual `EngineReactorDescriptor` / `EngineReactorState` physics stored in `ObjectModuleState`; six cases: nominal healthy; intact but switched OFF; 50% emitter pad area; critical 10% pad area; zero pad area; internal reactor failure with intact pad. Simulates 1801 one-second updates each case from the SAME 900K idle initial condition and constant 4200 MW nominal requested load where ignition on. It does not rely on random hit events.
- Full CSV log output `D:/tmp/elite_er5_failure_matrix.csv` with per 10 second rows and t=1800, source/cause columns, ignition flag, physical pad integrity, module structural health, canonical equipment internal failure, electric output, pad thermal power, core temperature, stored/coolant heat, failure and signed MW balance residual. Console prints each case final values, ends `ER5 FAILURE MATRIX: PASS` only after bounds/conservation assertions.
- VERY IMPORTANT SCOPE: test is **standalone reactor physics**, not full `ShipCore` with installed module registry, force allocation, damage event handling, network replication, actual HUD pixels, I/Y/X topology or automated game flight. It creates an ObjectModuleState directly, so it does NOT prove ObjectModuleRuntime mutation and replication. No assumption that zero pad area means reactor immediately fails; check actual thermal evolution. Emergency bridge requested transfer explicitly zero.
- MSYS2: `cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build/tests/architecture_contracts --target er5_failure_matrix_scenario -j 4`; `ctest --test-dir build/tests/architecture_contracts -R '^er5_failure_matrix$' --output-on-failure`; inspect CSV and console. Native build/CTest not yet performed in this session, PASS pending actual Windows output.
- Next if PASS: add integrated ship/runtime fault injector with isolated debug opt-in and render-independent cockpit/BITE assertions; do not mistake shutdown for equipment failure. Keep project context main-only here.


## 2026-10-27 ER-5 temperature-dependent conversion efficiency (SOURCE; Windows retest required)

- User correctly pointed out missing heat/efficiency feedback: before this change `EngineReactorUnit.h` reactor efficiency depended only on equipment damage and age, not core temperature, so hot reactor derated power without paying an extra conversion-efficiency penalty.
- Commits `e348799b`, `d713b90d` change ONLY ER-5 type/operating physical law: add separately authored `maxColdEfficiencyLoss01=0.12` and `maxHotEfficiencyLoss01=0.18` defaults to `EngineReactorDescriptor`. Cold penalty grows quadratically below `operatingTemperatureK` down toward deep-space temperature; hot penalty grows quadratically ABOVE `safeTemperatureK` up to `criticalTemperatureK`. Rated 90% nominal efficiency is retained inside 900..2100 K for healthy new Cobra ER-5; damage and age existing penalties still apply. Conversion electrical output is bounded independently by existing thermalAuthority. Actual heat generation then uses actual electrical output and effective efficiency, so at matched electrical output a hotter/colder reactor generates more waste. Existing heating/damage integration consumes this additional heat; no new hidden thermal source or added independent failure state. The particular cold/hot maximum-efficiency-loss numbers are GAME BALANCE defaults needing simulation acceptance, not experimentally established constants.
- Commit `11e31521` updates the existing standalone ER-5 failure matrix CSV to include `efficiency01`, plus direct fixed-output assertions: hot >safe is less efficient and emits more waste heat, cold <operating is less efficient, while hot/safe electric output is identical in this targeted comparison. Full six-scenario CSV must be rerun because original 30-minute temperatures/powers are stale.
- Not tested natively in this assistant environment; do not claim passing compiled build or gameplay. MSYS2: `git pull --ff-only origin main`; `cmake --build build/tests/architecture_contracts --target er5_failure_matrix_scenario -j 4`; `ctest --test-dir build/tests/architecture_contracts -R '^er5_failure_matrix$' --output-on-failure`; inspect regenerated `D:/tmp/elite_er5_failure_matrix.csv`, compare intact vs 50/10/0% pad, runaway/derating, age and thermal damage. An integrated ship damage-run and network HUD still pending.


## 2026-10-27 persistent ER-5 thermal degradation in extended failure matrix

- User requires feedback loop: overheating => reduced efficiency => greater waste => accumulated irreversible reactor thermal damage => degradation of capability and conversion efficiency. Actual `advanceEngineReactor` ALREADY accumulates `EquipmentDamageKind::Thermal` above safe temperature and computes functional damage for subsequent ticks; `ShipCore::updateSystems` commits thermal-damage quanta to canonical `ObjectModuleRuntime`. The previous 1800 s isolated matrix TEST had a defect: it reset `state.damage` from unmodified `module.equipmentDamage.accumulated` on every tick, inadvertently erasing thermal damage, so CSV did not validate cumulative degradation.
- Commit `d51f5568`: test now writes post-step `state.damage` back to `module.equipmentDamage.accumulated` each tick, extends all six cases to 7200 sec, logs `thermal_damage01`, `total_damage01` and retained `efficiency01`, asserts destroyed-pad scenario accumulates thermal damage, and healthy nominal remains undamaged. Commit `d4f80931` displays final thermal damage per case in console.
- This is a SOURCE-ONLY diagnostic correction; reactor runtime law not changed and no new duplicate damage state introduced. The updated matrix should show compounding derating via both temperature and persistent damage, but this is NOT native CTest PASS until Windows run. Inspect 7200-second matrix for max thermal damage, actual electric output, whether core enters failure (damage >=95%), and heat balance. The standalone matrix still does not validate in-game collision, networking or HUD; those await separate integration.
- MSYS2 rerun `git pull --ff-only origin main && cmake --build build/tests/architecture_contracts --target er5_failure_matrix_scenario -j 4 && ctest --test-dir build/tests/architecture_contracts -R '^er5_failure_matrix$' --output-on-failure`; CSV `D:/tmp/elite_er5_failure_matrix.csv`.


## 2026-10-27 ER-5 two-hour uploaded matrix review

- User-provided `elite_er5_failure_matrix(2).csv`: 4326 rows, six scenarios x 721 samples, 0..7200 seconds. Signed heat-balance residual and emergency transfer equal zero for every row. The CSV is output from the standalone reactor model, not an integrated game/HUD test.
- At 7200 s: healthy 1674.115 K, 4200 MW, efficiency 0.900, thermal damage 0; half pad 2284.703 K, 1735.335 MW, efficiency 0.829342, thermal damage 0.096908; 10% pad 2385.447 K, 191.451 MW, efficiency 0.731773, thermal damage 0.210648; zero pad 2415.985 K, output ZERO, efficiency 0.714193, thermal damage 0.232323. Power-off intact and internally failed both 0 MW and 903.303 K but distinct ignition and failed flags.
- Full pad loss: T first >2100 K at 1540s; T first >=2400 K and power first hits 0 MW at 2410s. `failed` remains false after thermal power blocking because aggregate damage is below 0.95 functional failure threshold; these are distinct states. At 7200s reactor still produces 2 MW hot-standby heat with NO pad, so T drifts up even at zero electric output. This is a genuine open safety/design issue: distinguish heat-off shutdown from electrical zero output, and ensure controlled scram/passive cooling vs emergency exchanger rescue. Do not treat zero electric MW as physically deenergized. Long-run degradation loop was verified numerically by CSV; native full game path remains unverified.
- Next engineering decision: implement explicit stopped/isolated thermal source after true reactor scram and meaningful startup/restart constraints; finish damaged emergency bridge and telemetry with proper real installed module before gameplay acceptance. Avoid arbitrary balance constants without comparison.


## 2026-10-27 ER-5 catastrophic containment loss gate — SOURCE ONLY

- User requested threshold for ER-5 "boom" based on BOTH core temperature and accumulated equipment damage. Added authored `EngineReactorDescriptor::catastrophicTemperatureK=2500 K` and `catastrophicDamage01=0.35` (tunable GAMEPLAY values, distinct from safe=2100 K, thermal shutdown critical=2400 K and standard functional failure=0.95). Threshold is a logical AND; neither high T alone nor damaged/cold ER-5 alone is sufficient. `EngineReactorState::catastrophic` latches permanently. Subsequent tick of latched reactor generates no electricity or further simulated thermal output.
- `ShipCore::updateSystems` now consumes `tick.catastrophic`, writes `[ER5_CATASTROPHE]` to stderr and immediately calls `m_moduleRuntime.markModuleDestroyed(reactorIds[g],0)`, forcing real loss of installed ER-5 and refreshing runtime capabilities. NO direct player-key fake or parallel state object. This establishes **module-level catastrophic failure**; physical explosion impulse, heat, shrapnel, neighbor-module damage, particles/audio and hull destruction are NOT yet implemented, so do not claim visual/physical explosion in-game.
- `Er5FailureMatrixScenario.cpp` now has explicit AND gate assertions for hot/undamaged, cool/damaged and hot/damaged states and appends `catastrophic` CSV field to `D:/tmp/elite_er5_failure_matrix.csv`. User must rerun native Windows test; compiler, CTest and gameplay all remain unverified at this checkpoint. Verify reactor module destruction on actual ship and view it in cockpit via module state.
- CAUTION: if reactor is fully thermally power-blocked at 2400 K, continued 2 MW hot standby with destroyed pad may slowly push temperature to 2500 K while thermal damage accumulates; this is intentional pending scram implementation. For an actual explosion event, damage threshold must be met too. This criterion MUST be tuned using extended runtime rather than treated as physically calibrated constants. Both threshold configs belong to reactor descriptors for future per-type tuning.
- Main-only and single canonical PROJECT_STATE.md retained.


## 2026-10-27 native baseline at `6d4c4d56` + Assisted hull-lag fixture correction (VERIFIED on this Windows host)

- Agent ran the native gates directly on the user's Windows MSYS2 MinGW64 checkout (`D:/__elite/work`), not via remote edits. Logs: `D:/tmp/elite_baseline_*.log`.
- Baseline at `6d4c4d56` (no tracked modifications): `bash build_mingw64.sh` exit 0, `EliteGame.exe` and `headless_server/EliteServer.exe` linked; `cmake --build build --target EliteAssetEditor` linked. This is the first native confirmation of editor link fix `e9e78f01`.
- Baseline CTests: `scripts/run_newtonian_physics_diagnostics.sh` 8/8 PASS; `emergency_thermal_bridge`, `er5_failure_matrix` 2/2 PASS; `navigation_scenario_runner`, `navigation_v2_tunnel_proving_ground`, `autopilot_law_api`, `assisted_layout_flight_matrix`, `ship_physical_corridor_matrix` PASS; `client_route_autopilot` **FAILED** at sub-test 29/30 `testHullAttitudeControlIsNotSlowedByCourseLag` (`|yawInput|>0.5` for a 5 degree hull error).
- Root cause (measured with a scratch probe against production `AssistedAutopilot.h`, same test params): 5 degree error gives yaw +0.142379 in moving course mode AND +0.142379 with `terminalAttitudeHold`; moving mode caps at +0.284758 from 10 degrees (accepted 10-degree hull lead), hold mode reaches 1.0 by 45 degrees. Course lag does not throttle hull attitude; the 0.5 threshold (added `e59bef52`, 2026-10-06) predates the accepted 0.50 s damped attitude servo, same class as `4f214ea4`/`9d34c555`. Not a production defect. The sub-test was first reached only after the earlier sequential fixes.
- User-approved test correction: assert correct positive yaw sign AND exact equality (<=1e-6) between moving-mode and attitude-only (`terminalAttitudeHold`) commands for the same error. No production code, constant or tolerance elsewhere changed. Native rerun: `client_route_autopilot` PASS (all 30 sub-tests, including never-before-reached `testClientStabilizerUsesOrdinaryControls`).
- NOTE: at this commit `client_route_autopilot` still exercises the non-production `ClientRouteAutopilot.h` copy for its route sub-tests; production player flight uses `DedicatedClientRouteAutopilot`. De-duplication is the next slice.


## 2026-10-27 route-autopilot de-duplication: production is now the ONLY implementation and the test authority (VERIFIED natively)

- Removed `src/game/navigation/autopilot/ClientRouteAutopilot.h` (2828 lines). It was a stale near-copy that tests, the matrices and `NavigationScenarioRunner` exercised, while production `SpaceState` flies `DedicatedClientRouteAutopilot<Law, PilotController>`. The copy lacked three production behaviours: Assisted `InCorridorCenteringGuidance` (`e16627d7`/`fe13923e`/`300683fe`), the damaged working-axis orientation lead (`fd938e26`/`3b851ddc`), and the `AssistedConcurrentManeuverBudget` curve-speed/braking share. It also dispatched law at runtime via `AutopilotLawController`.
- **Authority now:** `DedicatedClientRouteAutopilot.h` = the pilot-agnostic route layer. NEW `DedicatedClientRouteAutopilotLaws.h` = the ONLY place binding `AssistedClientRouteAutopilot` / `NewtonianClientRouteAutopilot` to `AssistedAutopilot` / `NewtonianOptimalController` (moved there, byte-identical aliases). `SpaceState.h` and all tests include it. `AutopilotLawController` remains ONLY as the server NPC runtime law seam.
- No production behaviour, constant or tolerance changed. Production edits are limited to the include/alias move, the `SpaceState.h` include, an `AutopilotLawController.h` comment, and `NavigationScenarioRunner` moving its follower loop into a per-law template dispatched on `request.controlLaw` (same `SharedShipPhysics`/`DynamicMotionSystem` chain as before).
- Tests moved onto production: `client_route_autopilot`, `navigation_v2_tunnel_proving_ground` (Assisted type); `assisted_layout_flight_matrix`, `ship_physical_corridor_matrix` (per-model template dispatched on `nativeFlightLaw(model)`); `navigation_scenario_runner` (through the runner); `autopilot_law_api` route-layer text contract retargeted to `DedicatedClientRouteAutopilot.h` with identical assertions (true only because of the alias move). `check_navigation_api_purity.py` (not a CTest) retargeted the same way: PASS before and after.
- **Behavioural difference found** (soft-fail scratch run of all 30 `client_route_autopilot` sub-tests against production): exactly ONE. `testInCorridorCrossTrackDoesNotReplaceAuthoredCourse` demanded ZERO steering for a 10 m parallel offset. Production Assisted gives yaw +0.152512 toward centre, 5.356 degree correction, 1.875 m/s closing, no capture, no strafe/lift (its deliberate 2026-10-14 centering design). User-approved: the sub-test (renamed `testInCorridorCrossTrackCentersGentlyWithoutCapture`) now asserts the production contract. Inside the 2.5 m deadband (10% of 25 m tolerance): exact tangent, zero steering, zero closing speed. At 10 m: no capture, positive yaw toward centre, no pitch, closing speed in (0, error-deadband], no strafe/lift. All other 29 sub-tests passed unchanged against production.
- Native gates after the change (MSYS2 MinGW64, this host; logs `D:/tmp/elite_dedup_*.log`): the 6 tests above 6/6 PASS; `scripts/run_newtonian_physics_diagnostics.sh` 8/8 PASS; `bash build_mingw64.sh` exit 0 (EliteGame/EliteServer relinked, `SpaceState.cpp` and the runner recompiled); `EliteAssetEditor` exit 0. Full 73-test `navigation_runtime` suite NOT run in this pass.
- **Remaining issues found, not fixed (out of scope):**
  - Stale normative docs: `NAVIGATION_V2_EXECUTION_ARCHITECTURE.md:45,291` still names `ClientRouteAutopilot -> AutopilotLawController` as the production contract.
  - Unregistered Python contract scripts already FAILING at `6d4c4d56`, unchanged by this work: `check_automatic_docking`, `check_docking_module_boundaries`, `check_local_flight_control`, `check_manual_docking_advisory`, `check_navigation_live_runtime_control`, `check_navigation_route_boundaries`, `check_assisted_follower_centering_contract`, `check_assisted_docking_entry_contract`.
  - `NavigationScenarioRunner` integrates via `SharedShipPhysics`, not the production `ShipCore`/gateway path.
- NEXT: await user instruction for the next cleanup item.


## 2026-10-27 universal-ship migration slice 1: loadout topology owned by ShipDescriptor (VERIFIED natively; Newtonian AP blocked by a PRE-EXISTING defect)

- Migration start point: `4ffb7942` (tag `claude-autopilot-cleanup-ok`). Recovery tag `claude-start-2026-10-27` = `6d4c4d56`. Agreed direction: installed equipment defines which hardware exists; flight law only defines how it is controlled. `PhysicalControlModelId` is kept on the wire, in CSV and in diagnostics as a TEMPORARY legacy loadout index.
- Source change (no constant, coordinate, module ID, slot order, thrust, power, thermal, gas, damping or allocator change):
  - new `ShipLoadout.h` (`ShipLoadoutDefinition`: id, legacy model, flight law, propulsion slots, module IDs, ER-5 groups with powered engine slots, RCS nozzle mounts);
  - `ShipDescriptor::loadouts` + `findLoadout()`;
  - `CobraLoadouts.h` builds the six Cobra loadouts 1:1 from the existing `presets::cobra` tables;
  - `ShipLoadoutTopology.h`: `propulsionTopology()` / `engineReactorGroups()` (explicit legacy fallback only for descriptors without loadouts) and `requireLoadoutMatchesLegacyTopology()`, which throws if a loadout diverges from the tables still used by pure plant code.
- **Production consumers moved to the loadout:** `ShipCore` installed-profile builder, ER-5 power groups, ER-5 thermal/aging loop and all 8 construction topology lookups; client `replicatedInstalledCobraDescriptor`; `MotherComputerClientAdapter` (ER-5 drive groups + 2 topology lookups).
- **Still on Cobra/global tables (later slices):**
  - RCS nozzle catalog in `NewtonianRcsAllocator`, `ShipCore` (3 sites), `MotherComputerClientAdapter` (slice 3);
  - pure `physicalControlSlots(model)` in capability builder, actuator plant, preset maneuvers, gateway, `ShipState`, `SpaceState`, `GameSimulation`, `GameServer`, HUD;
  - `typeId==CobraMk1` gate (slice 2); env model block in the descriptor (slice 7).
- Test fixture: `EngineThrustHudPresentationTests` Cobra-like descriptor now carries `cobraLoadouts()` like the production hull; assertions unchanged.
- **Native gates (this host, `D:/tmp/elite_slice1_*`):** `bash build_mingw64.sh` exit 0 (EliteGame, EliteServer), `EliteAssetEditor` exit 0; focused 8/8 PASS; `mother_computer`, `ship_physical_actuator_plant`, `working_thrust_axis_analysis`, `assisted_layout_flight_matrix`, `ship_physical_corridor_matrix` PASS.
- **Pre-existing reds, unchanged by slice 1:**
  - `ship_state` is RED at HEAD: it expects the retired 4-RCS-cluster Newtonian-I topology.
  - `assisted_authority_contract` does NOT COMPILE at HEAD (missing includes in `ReplicatedInstalledAssistedDescriptor.h` / `ReplicatedAssistedControlAuthority.h`). With slice 1 it compiles and fails `testEmergencySingleAftCoMThrustHasNoYaw` ("outward nozzle did not acquire the required CoM angle"), on code this slice does not touch.
- In-game oracle `EliteGame.exe --diagnostic-scenario ship-control`:
  - slice 1: 2 of 3 runs PASS 6/6, 84/84; HEAD 1/1 PASS.
  - The one FAIL (Assisted-Y step 13, `unresolved_probe_runaway_speed`) is an ORACLE OBSERVATION DEFECT, present at HEAD too: `SpaceState.cpp:6005-6046` projects the WORLD-frame `motion.mainEngineAccelerationMps2` onto HUB-LOCAL body axes, so step 13 depends on the hub's real-time orientation (the failed log shows `v_forward` constant while "forward" main acceleration is 48 m/s^2). Not fixed.
- **Real flight (unattended, `--dev-direct-flight --diagnostic-scenario nav-stress:simple` + `ELITE_SHIP_PHYSICAL_TEST_MODEL`):** slice 1 Assisted-I/Y/X build the route, enter Executing, reach the 400 m/s envelope and pass to checkpoint 49 while braking.
- **User manual check: manual flight OK; autopilot did not move.** Root cause (NOT slice 1; identical on HEAD `4ffb7942`, Newtonian-I, 100 s and 180 s runs):
  - the Newtonian planner finishes (`job_ready=1`), but automatic handover `Planning -> Executing` requires |omega| < 0.01 rad/s for 0.2 s (`SpaceState.cpp:~4450-4520`), with zero translation commanded meanwhile;
  - the client stabilizer (`NewtonianOptimalController`, `terminalAttitudeHold`, correct when tested offline at any hull orientation) acts on replicated, delayed rates with near bang-bang commands, while the installed RCS now delivers 1.90 rad/s^2 after `8aa9aae0` (12 kN -> 500 kN nozzles);
  - result: a limit cycle with |omega| ~0.18 rad/s, torque ~90 degrees out of phase (~150-200 ms loop lag), so handover never happens.
  - Server hardware is ready (ER-5 5000 MW, main 100% available, forward capability 36.8 m/s^2); the server simply receives zero translation. Assisted settles and flies.
- DEV tool committed separately (`03e08a94`): `EliteGame.exe --dev-direct-flight` = the NEW LOCAL GAME path with player "DEV PILOT", explicit flag only; ordinary flow unchanged.
- Status: SOURCE COMPLETE, BUILD PASS, AUTOMATED CHECK PASS, GAME START PASS. GAMEPLAY: Assisted autopilot verified by real unattended runs; Newtonian autopilot blocked by the pre-existing limit cycle above.
- User decision: commit slice 1; fix the Newtonian stabilizer as a SEPARATE slice. The fix must account for real RCS authority and replication latency, and the control change needs user approval. NEXT: await instruction (slice 2 or the Newtonian stabilizer).


## 2026-10-27 healthy Newtonian I/Y/X autopilot unblocked (VERIFIED by unattended real flights)

- Root causes found on real `EliteGame.exe` (`--dev-direct-flight --diagnostic-scenario nav-stress:simple`, `ELITE_SHIP_PHYSICAL_TEST_MODEL`), all present at `27dd6cb6`:
  1. **Planning stabilization limit cycle.** The client rate-hold ran the one-tick predictive law on replicated, delayed rates against 1.90 rad/s^2 of RCS authority (after `8aa9aae0`); |omega| stayed ~0.18 rad/s and `Planning -> Executing` never happened.
  2. **No turn onto the safe course at rest.** `legacyNewtonianSafeCourseControl` at rest requested no velocity change, so the hull never turned onto the safe course (HEAD only "acquired" it by spinning through it).
  3. **Main drive at ~14%: the long-standing "14.32%".** `executeEngineControlMatrix` handles a purely axial demand exactly, but ANY small lateral component sends it to FISTA, whose global step is dominated by the gimbal torque columns. After 96 passes the axial thrust reached only ~14% (reproduced offline for I/Y/X).
  4. **Two capability sources.** The client Newtonian controller normalized against the descriptor nominal 3.0 rad/s^2, while the server executes the installed-nozzle bound 1.903.
- Fixes (Newtonian only, Assisted behaviour unchanged):
  - `SpaceState` zero-rate planning hold sends neutral rotation, so the server's existing neutral angular damping removes rotation from fresh authoritative rates.
  - The safe-course turn uses a pose hold onto the selected course when no velocity change is required.
  - `NewtonianOptimalController` uses the same 0.50 s critically damped `HullAttitudeControl` servo as Assisted.
  - `ShipCore` caches the signed 24-nozzle authority and publishes it through the existing `ShipControlAuthorityTelemetry` (no wire change). `MotherComputer::physicalFlightEnvelope` applies it with the SAME `applyNewtonianRcsScalarBounds` the server uses; Newtonian planning waits for it like Assisted.
  - The matrix FISTA is warm-started from the exact equal-bank axial maneuver (only Newtonian main uses this solver).
- Native gates: `bash build_mingw64.sh` exit 0; focused 8/8; in-game six-model oracle PASSED 6/6, 84/84 (now unattended via `--dev-direct-flight`).
- Real flights, 150 s each:
  - Newtonian I/Y/X: safe course acquired, Executing, up to the 400 m/s envelope, checkpoint 41; main executed/requested 0.993-0.994 (was 0.142); RCS torque 1.000; no gas depletion.
  - Assisted-I regression: unchanged, checkpoint 52.
  - Logs: `D:/tmp/elite_h_*`.
- Open, observed in the real tunnel flight and confirmed by the user's review:
  - The movement STRATEGY is wrong: frequent hull rotation, stop-turn-go at broken corners, one overshoot swing after turns, ~1 input reversal/s.
  - Cause (audited, see the next plan): polyline route through portal and centreline centres; Newtonian corner speed ~0; the Newtonian controller corrects only with the main engine (translational RCS unused in flight, TVC only as a residual). Planned as the Newtonian-only N1-N4 work.
- Power/gas are not yet in the published capability. Docking/damage/emergency are NOT verified.


## 2026-10-27 product passport/catalog layer + Cobrett-I N1/A3 identities (VERIFIED natively)

- World model change: Assisted/Newtonian I/Y/X are historical VALIDATION configurations, not six variants of one ship. Real ships are separate serial products. The first two canonical models are from Kobersberg-Braunauer Raumfahrzeugwerke GmbH:
  - `Cobrett-I FRN-U N1`: older Newtonian generation; realized by the Newtonian-I loadout.
  - `Cobrett-I FRN-U/INT-A A3`: modern Assisted generation; realized by the Assisted-I loadout.
  - They temporarily share the legacy Cobra hull geometry; their data identities are distinct. Y/X have no ship passport.
- Codes:
  - `Cobrett` = family; `I` = axial main propulsion layout (not a generation, not a flight law).
  - `FRN-U` = Frontier Utility; `INT-A` = Inter-system Armed Transport. `/` lists operational classes; it is not a mode.
  - `N1` = Newtonian Generation 1; `A3` = Assisted Generation 3.
- New `src/game/catalog/`, split by category:
  - `CatalogTypes.h`: `ProductPassport`, `ProductCategory`, `PassportStatus`, `CompatibilityRelation` (InstalledOn / CompatibleWith / Replaces / ReplacementFor / OptionalFor), `CodeDescription`, `InstallationPosition`.
  - `codes/OperationalClassCodes.h`, `codes/FlightArchitectureCodes.h`.
  - `propulsion/KobraMainDrivePassports.h` (`KB-N1-AX`, `CR-1`), `power/KobraReactorPassports.h` (ER-5), `rcs/KobraRcsPassports.h`, `ships/CobrettPassports.h`.
  - `CatalogRegistry.h`: lookup plus installation validation.
- Identities:
  - product identity (`stableId`, stock number, part number) = WHAT; installation code = WHERE inside a model; serial number = WHICH physical unit (future per-instance data, never in a passport).
  - Stock scheme (in-game, NOT real NSN): `KB-<SHP|ENG|RCT|RCS>-<family/gen>-<NNNN>`, e.g. ships `KB-SHP-CBI-N1-0001` / `KB-SHP-CBI-A3-0001`, engines `KB-ENG-N1-0001` / `KB-ENG-A3-0001`, reactor `KB-RCT-ER5-0001`, RCS `KB-RCS-N1-0001`.
  - Installation-code trees (roots provisional): N1 `36` (36A1 axial propulsion group: A1 ER-5, A2 KB-N1-AX; 36A2 RCS group: 8 nodes x 3 nozzles; 36A3 service reactor; 36A4 cooling); A3 `37` (37A1/37A2 right/left propulsion groups: A1 ER-5, A2 aft CR-1, A3 fore CR-1; 37A3; 37A4).
  - The descriptor build validates every passport position against the real installed module and its technical type (throws on mismatch).
- Technical data stays in equipment presets, linked by `technicalTypeId`. New `KobraN1AxialMainDrive` preset (narrow-vector N1 unit) on the Newtonian main modules; CR-1 remains the A3 wide-vector unit (75 deg).
- **Vectoring is a passport property:**
  - removed the flight-law clamp in `ShipEngineControlMatrix` and the 7 deg / 8 deg/s `ShipParams` values from the hull descriptor;
  - `applyMountedEngineAxialAuthority` (server and client) projects `mainGimbal*` from the installed main drive passports, reduced by vectoring damage.
- **PROVISIONAL:**
  - N1 main gimbal retains the legacy +/-7 deg as provisional only;
  - N1 thrust/power/thermal/wear are copies of CR-1;
  - all stock/part numbers and installation roots are provisional;
  - CR-1/ER-5/RCS manufacturer names are legacy preset values;
  - ER-5 and RCS are shared by N1/A3.
  - `ELITE_DEV_N1_GIMBAL_DEG` is a TEMP dev override for the gimbal selection sweep.
- Gimbal sweep at this point (0/1/2/3/5/7 deg, Newtonian-I tunnel, 150 s each): NO effect. The production autopilot never deflected the nozzle (max 0.00 deg) and never used translational RCS, so every correction became hull rotation. The angle cannot be selected until actuator cooperation (nozzle + RCS translation + RCS counter-torque) exists.
- Gates: `build_mingw64.sh` + `EliteAssetEditor` exit 0; focused 8/8; real start passes passport validation; Assisted-I flights unchanged (checkpoint 52); six-model oracle 84/84.


## 2026-10-27 Newtonian ballistic execution slice: findings, failed gates, new doctrine (PARTIALLY COMMITTED)

Baseline for comparison: working tree on top of `4f67103c` with the uncommitted actuator-cooperation work, gimbal 7 deg, real `EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple`, 150 s Newtonian-I tunnel, logs `D:/tmp/elite_bp_base_I*`. Metrics: `python D:/tmp/elite_bp_metrics.py TAG [--timeline]` (client trace `ELITE_NEWTONIAN_POLICY_TRACE`, profile trace `ELITE_NEWTONIAN_PROFILE_TRACE`, both DEV-only, uncommitted).

### Root cause found and COMMITTED: hidden RCS force during pure rotation
- The "160 kN avg / 520 kN max parasitic RCS force during rotation" was NOT the allocator: with a true pure-torque request `allocateNewtonianRcs` delivers 0.1 kN.
- The source was the manual Newtonian RCS release doctrine in `DynamicMotionSystem::applyLocalFrameInput`: when translation inputs return to 0, the server fires RCS to cancel the whole ledger of RCS delta-v, so every autopilot RCS trim was undone later, often during flips.
- Fix: `ShipControlState::rcsKeepsTranslationDeltaV` (wire protocol 16 -> 17), copied by the gateway into `DynamicMotionState::newtonianRcsKeepsDeltaV`; when set the ledger stays empty and nothing is released. Default false: manual flight and Assisted unchanged. The flag is set by the (uncommitted) new Newtonian pilot.
- Evidence with the new pilot: client rotation-only ticks, net RCS force 221.2 kN mean / 520 kN max (baseline) -> 0.0 / 0.3 kN.

### Also COMMITTED: `MotherComputerClientAdapter.h`
- Regression introduced by the passport commit: the client Newtonian envelope used descriptor nominals (gimbal 0, main 73.5 m/s2) while the server executed the installed N1 (36.8 m/s2, passport cone). Installed main drives are now projected for every flight law, as on the server.

### Gates for the commit
- Subset syntax-checked alone in a worktree at `4f67103c` (DynamicMotionSystem, gateway, ShipCore, WireProtocol, adapter).
- Focused Newtonian CTest 8/8 PASS on the full working tree.
- `local_flight_control_contracts` is RED ("surviving fore main was not promoted to primary propulsion") and equally RED with the HEAD versions of the touched files: PRE-EXISTING, Assisted fore-main logic, not caused by this change.
- No full `build_mingw64.sh` of the exact commit subset; the full working tree (superset) builds with exit 0.

### UNCOMMITTED experimental Newtonian pilot (NOT ACCEPTED)
- Explicit COAST/FLIP/ALIGN/BURN phases, predictive safe tube (35% tolerance, 4 s), burn gate on the achievable thrust error, latched burn attitude, calm flip (50% rate / 50% angular acceleration), coast-before-brake runtime profile, pure profile feed-forward for Newtonian (tracker position feedback was a centreline rail).
- Best Newtonian-I runs (bp6/bp8) vs baseline: progress 21.7 km / checkpoint 47 (same); hull rotation 22.9 vs 55.6 rad; big turns 10 vs 37; reversals 24 vs 187; max omega 0.85 vs 1.57; strong main (>50%) at >15 deg thrust error 0 vs 2553 ticks; cross-track max ~40 vs 42 m.
- FAILED gates: Y/X up to 0.5-1 km cross-track (bp7/bp8); bp9-bp12 all three models stalled at the first broken corner (988 m) for 2+ minutes. Root causes found during the iterations: stale route projection at an overshot vertex, catch-up rule stalling a position-based profile, preview retro-only planning at low speed, anticipation vs burn alternating the hull target by ~160 deg. Each local fix exposed the next; ~13 build+flight iterations. Stopped by the user.

### Diagnosed architectural causes (user-confirmed)
- `PreplanningSafetyCourse` picks the MINIMAL course change (score = angle*10 + goal angle, 15 deg rings) even at rest -> artificial 15 deg start bend plus a protected lead straight (~2 km) before the real turn.
- Newtonian planner: `roundTurns=false`, polyline through portal/centreline centres, every vertex a near-stop -> stop-turn-go even on almost straight runs.
- The controller re-invents strategy every tick from preview; a nearly finished turn can be cancelled by a secondary correction.

### NEW DOCTRINE (user decision 2026-10-27)
- The main task (good safe natural route honouring special zones and speed limits with the real ship volume; an autopilot that executes it) is above internal contracts. `roundTurns=false`, stop-turn-go, mandatory vertices/centreline points, tangent contracts, sustained-RCS/drift prohibitions, old speed-profile assumptions and old tests are working hypotheses and may be changed or removed. Tests follow the accepted doctrine.
- Hard constraints only: collision geometry, obstacles, hit volumes, ship size, narrow corridors, special-zone speed limits and rules, real actuator capability, power/gas/damage, physically impossible manoeuvres.
- Narrow zones: no full hull rotation inside; slow down and orient before entry; traverse with coast, translational RCS, small vectoring. Orientation must fit the ship envelope into the cross-section (conservative approximation, never plan a point and rotate the hull through a wall).
- Near-zero speed ends a manoeuvre: reset transient commands, orient to the next segment, settle, then accelerate. The current manoeuvre outranks secondary corrections; safety outranks everything.
- Newtonian planner may emit a manoeuvre program (ALIGN, BURN, COAST, FLIP, ALIGN, BRAKE, SETTLE); the autopilot executes it with local corrections.
- Arcs/drift allowed for Newtonian when the real lateral budget (vectoring + RCS, time, corridor width) permits. Near-collinear points merged; a gate is a window, not a centre point.
- Experiment rule: 2-3 consecutive real flights without substantial improvement of the same problem -> STOP and report.

### Next steps (accepted order)
1. Newtonian planner geometry: no artificial start bend at rest, merge near-collinear vertices, broad curves/drift, gates as windows, corridor width, keep special-zone constraints. A short design proposal goes to the user BEFORE implementation.
2. Explicit manoeuvre program / autopilot sequencing with priority and transient reset.
3. Narrow-zone safety (envelope, safe orientation, limited-rotation regions, conservative speed).
4. N1 gimbal sweep only once the new strategy really uses vectoring.


## 2026-10-27 Step 1a Newtonian route geometry: offline PASS, flight NOT accepted (UNCOMMITTED, stopped for review)

Source changes (working tree, not committed):
- Newtonian pipeline: `roundTurns=true` (existing fillet arcs with collision proof), new `RoutePlanRequest/DockingAdvisoryRequest::mergeNearCollinearVertices` (drop interior vertex < 3 deg when the shortcut is clear; default off, Newtonian only).
- One Newtonian curve capability: `newtonianPreferredLateralAccelerationMps2 = 0.5 * vector main authority` (`ManeuverCapabilityAdapters.h`), used by the planner request and by the accepted-trajectory execution authority; actuator choice stays with the pilot.
- Start at rest (Newtonian, v <= 0.5 m/s): no `PreplanningSafetyCourse` minimal-turn search, no launch heading, no protected lead (`SpaceState.cpp`, log `phase=skipped-at-rest`).
- Follower stop at tangent breaks only for joins > 0.5 deg (`DedicatedClientRouteAutopilot.h`).
- BLUE KeepInside erosion (sphere radius + 22 m) marked PROVISIONAL in `RouteVolumeContainmentValidator.cpp`.
- 700/150 m tunnel straights: inherited route-graph design values (`3c112f32` copied the dock ingress `max(700 m, 3*standoff)`; `e2dc2dc5` hand-shortened the exit to 150 m). Not a derived safety requirement; kept for 1a.
- DEV dump `ELITE_ROUTE_DUMP`; comparison `python D:/tmp/elite_s1a_compare.py s1a_old s1a_new`.

Offline comparison (first accepted route, same scene):
- I: 12 lines / 7 mid-route stops / 33819 m / profile 231 s -> 10 lines + 5 arcs (R 134..9910 m) / 0 stops / 32866 m / 195 s.
- Y/X: 12 lines / 7 stops / 27750 m / 161 s -> 11 lines + 6 arcs (R 104..4955 m) / 0 stops / 27143 m / 137 s.
- Artificial 1000 m start lead and its bend are gone; BLUE vmax 220 respected; KeepInside validation passed (plan accepted).

Real flights (2, current experimental pilot, 150 s): NOT an improvement in execution.
- I: progress 18.9 km, cross-track max 198 m (outside the 64 m tolerance), lateral velocity up to 151 m/s; X: 16.5 km, 388 m.
- Root cause (trace): at arc entry at ~333 m/s the required a_lat (v^2/R ~ 11 m/s^2) exceeds RCS/vectoring without turning the hull. The pilot only anticipates braking, so it starts a ~3 s hull turn AT the arc entry and drifts out meanwhile; the follow-up reference then jumps to the next tight arc (R 143 m, 48 m/s) while the craft is still at 330 m/s.
- Conclusion: geometry goal reached offline; flight failure is execution (arc entry needs lateral preparation before the arc) = Step 2 territory, plus an open planner question (arc entry transition / speed when the needed a_lat requires a hull turn).

### Step 1a follow-up: arc execution in the experimental pilot (3 flights, STOPPED by the experiment rule)
- Chosen locally (user rule: decide obvious local choices): pilot-side arc anticipation instead of a planner "no-hull-turn lateral" cap. Reason: at constant speed without turning the hull only RCS (~1.2 m/s2) is lateral; vectoring comes with axial main thrust; such a cap would cut arc speeds to ~30-110 m/s. A preparation-straight rule does not bind on this route (km-long straights before arcs).
- Flight 1 (no change, Step 1a routes): I cross-track max 198 m, lateral velocity max 151 m/s.
- Flight 2 (+ preview anticipation of curves RCS cannot carry): 137 m, 25 m/s, mean 26 m. Remaining exit on the tight R~143 m arc: the latched-braking burn gate (rate <= 0.12 rad/s) never opened while the thrust vector rotated with the arc.
- Flight 3 (+ hull follows the burn direction continuously on curves, no rate gate): 105 m, but lateral velocity back to 152 m/s, 400 command reversals. Cause: when the next preview sample is on a curve the curve candidate becomes "urgent" and replaces the braking demand with that sample's authored curve acceleration (scaled to NOMINAL, not actual speed): the required thrust jumps ~80 deg in one tick before the broad arc.
- Conclusion: Step 1a geometry is right (offline); arc execution keeps failing on conflicts between the pilot's per-tick heuristics (anticipate / urgent / latch / gate). This is the Step 2 problem (explicit manoeuvre program with phase priority). Pilot changes stay uncommitted.

## 2026-10-28 working rules before Step 2 (user decision) + WIP checkpoint of the experimental Newtonian stack

Rules (user, 2026-10-28):
- `main` only, no branches. WIP/checkpoint commits on main are allowed; a commit is NOT acceptance. Every slice records: SOURCE COMPLETE/WIP, BUILD, OFFLINE CHECK, GAME START, HEALTHY FLIGHT VERIFIED/NOT VERIFIED, known problems.
- Before each experiment series: problem, metric, baseline, expected result, PASS/FAIL.
- Newtonian acceptance priority: safety (corridor, KeepInside, zone speed limits, envelope vs wall, no strong main at bad misalignment) > route execution (completes or stable progress, cross-track <= corridor, no lateral runaway, no stop/restart or reversal loops) > control quality (no needless big rotations, no tens-of-degree target jumps between ticks without physical cause, finished phase not revived, local correction never cancels the main manoeuvre) > performance. `progress after 150 s` is no longer a quality criterion.
- 2-3 real flights per hypothesis; offline/headless iterations unlimited but only on production physics/control code; stop on no directed improvement offline as well.
- Step 2 pre-approved: route geometry + speed profile -> manoeuvre compiler -> explicit phases -> Newtonian execution -> low-level actuators. Planner stays a planner; Assisted unchanged. Carry over only low-level execution (safe tube, smooth attitude, burn gate as safety, coast, calm flip, RCS/nozzle cooperation, real capability/damage); delete old policy heuristics (competing urgent targets, preview urgency, latch where the phase covers it, stop-vertex hacks).
- D:/tmp: my files only with `elite_` prefix, cleaned per stage; never touch other files.

WIP checkpoint commits (all NOT ACCEPTED; status BUILD PASS, GAME START PASS, HEALTHY FLIGHT NOT VERIFIED):
1. Newtonian nozzle/RCS counter-torque cooperation (`ShipEngineControlMatrix.h`, `ShipControlExecutionGateway.cpp`, `ShipCore.cpp`).
2. Step 1a planner geometry (section above): offline PASS, flight NOT accepted (execution problem).
3. Experimental ballistic pilot (COAST/FLIP/ALIGN/BURN heuristics) + DEV decision trace. Superseded by Step 2; kept only as the comparison baseline. Unit `NewtonianOptimalControllerTests` PASS. Pre-existing red: `local_flight_control_contracts` ("surviving fore main was not promoted"), also red with HEAD versions of the touched files.

### Baseline decision trace of the old pilot (1 flight, Newtonian-I, Step 1a route, 150 s)
DEV traces: `ELITE_NEWTONIAN_POLICY_TRACE=<tsv>` (10 Hz decision rows + every event tick), `ELITE_NEWTONIAN_EVENT_TRACE=<tsv>` (ENTER/LEAVE_CURVE, TARGET_SWITCH >25 deg/tick, URGENT_ON/OFF, BURN_GATE_OPEN/CLOSE, LATCH_SET/CLEAR, PHASE_CHANGE, COMMAND_REVERSAL, CORRIDOR_VIOLATION). Route context comes in `AutopilotControlRequest::trace*` (diagnostic only). Note: a second controller instance without a corridor (tol 0, not the route pilot) shares the files; the analyser keeps only rows/events of the route instance.
Files kept as the Step 2 baseline: `D:/tmp/elite_s2base_decision.tsv`, `D:/tmp/elite_s2base_events.tsv`, `D:/tmp/elite_s2base_route.txt`; analysis `python D:/tmp/elite_s2_decision.py s2base`, window `python D:/tmp/elite_s2_window.py s2base T0 T1 [step]`.

Result: 5 corridor violations (max cross-track 100.3 m, tol 64 m), lateral velocity up to 139 m/s, 207 hull-target jumps >25 deg/tick, 92 command reversals, 191 burn-gate openings, 78 latch set/clear pairs, 410 phase changes in 155 s. No strong main at >15 deg achievable thrust error (burn gate as safety works).
Representative conflicts:
1. t 31.2-34.9 s, s 8945-10192 m, arc R 9910 m entered at 337 m/s: the curve needs 11.25 m/s2 lateral (> RCS). The latched retro braking burn and the curve are separate competing targets; the hull ends ~80 deg off the needed vector, gate closed 3.2 s, lateral velocity 1 -> 34 m/s, cross-track 2 -> 64 m -> CORRIDOR_VIOLATION. The combined vector (retro 22.5 + lateral 11.25) is only ~27 deg from retro and could have been prepared in one pre-orientation.
2. t 35.16-35.6 s, s 10257-10415 m, arc exit: reference target speed drops 330 -> 48 m/s at once (segment cap); `profileGapOnRcs` keys on the SIGN of a ~0 feed-forward, so the demand alternates 146/69 m/s2 every tick, hull target jumps 61 deg every tick, BURN<->FLIP each tick (~20 cycles).
3. t 46.5-51.3 s, s 13275-13868 m: main burn on/off every tick (feed-forward flickers -22.5/0), 30 latch set/clear pairs in 0.5 s; the reference speed itself has one-tick dips 157 -> 48 m/s (long error -98 m/s).
4. t 54.3-57.8 s, s 14114-14252 m, arc R 143 m at 36-40 m/s: curve anticipation turns 79 deg to the curve normal, braking takes the hull back 0.5 s later, at curve entry the hull is 71-81 deg off, gate closed > 1.5 s.
5. t 72-75 s, s 14700-14840 m: 75-100 m outside the corridor while the profile accelerates (+22.4 m/s2); the limited burn is blocked by the wall-side rule while lateral emergency is active: local correction and the main manoeuvre stall each other.
Conclusions for Step 2: (a) one thrust vector per phase planned from geometry+capability (braking and curve combined), prepared before the phase; (b) phase commitment, no per-tick re-selection; (c) reference inputs must be continuous (speed caps and feed-forward without one-tick jumps); (d) the safety correction modifies the phase vector, never replaces it.

## 2026-10-28 Step 2: Newtonian manoeuvre program (WIP; BUILD PASS, OFFLINE CHECK PASS, HEALTHY FLIGHT NOT VERIFIED)

Architecture (pre-approved Step 2): route geometry + runtime speed profile (navigation) -> `compileNewtonianManoeuvreProgram` (`autopilot/NewtonianManoeuvreProgram.h`) -> explicit phases COAST/ORIENT/FLIP/BURN/BRAKE/CURVE/SETTLE -> `NewtonianOptimalController` executes the CURRENT phase -> actuators. The route planner is unchanged; Assisted is unchanged (the route layer hands stations only to a pilot exposing `setManoeuvreRoute`, detected at compile time).
- Compiler: required acceleration a = a_t*T + v^2*kappa*N per station; main where |a| > 0.8 * RCS trajectory budget, braking and curving combined in one axis; short coasts (<1 s) merged, short burns (<0.4 s) dropped. Speed profile reshaped only downward within the planned authority: (1) on a main-carried curve a speed-up is limited to the nozzle cone beside the path-normal push; (2) turn windows: between two burns whose axes differ beyond cone+10 deg with no coast long enough for a calm turn, braking into a curve ends earlier, acceleration out of a curve starts later, braking right after a curve slows the curve (3 passes). Planned hull: burns hold their axis, coasts turn early onto the next burn axis, rate-limited at 0.8 x the calm preferred rate. An optional speed-up the hull cannot reach is dropped; braking/curve burns are never dropped.
- Executor: plan read by route progress (phase index never goes back; plan read no earlier than the current phase start). Demand from the ACTUAL state: profile accel + speed error/2 s, curve normal acceleration at the actual speed, safe-tube correction. Main only in burn phases (or a clear >2x RCS deficit / wall emergency, through the gate). On a CURVE the burn axis is held within the nozzle cone around the planned axis. Burn gate kept as safety (achievable error <=2 deg full, <=10 deg limited, rate gate except curves, wall-side rule). Safety override (wall exit RCS cannot stop, hull cannot burn against it) is the only thing that takes the hull off the plan. Terminal settle: full velocity/position trim on RCS until the precision stop.
- Removed old policy heuristics: preview urgency/anticipation, competing urgent targets, burn latch/relatch, turn-target dwell, profile-gap sign rule, stop-vertex hacks; the pilot no longer uses the route layer's composite target speed.
- Offline harness (DEV): `EliteNewtonianReplay` (`tools/navigation_runtime/NewtonianCaseReplay.cpp`, target in the ELITE_BUILD_SERVER block, sources shared via `ELITE_SERVER_SIMULATION_SOURCES`) replays a case dumped by the game (`ELITE_AUTOPILOT_CASE_DUMP`, `diagnostics/AutopilotCaseFile.h`) through the production route autopilot and the production ShipCore server step with a 1..6 tick control delay. Fidelity check with the OLD pilot: delay 2-4 reproduces the game's failure signature (lateral velocity ~143 vs 138 m/s, ~225 target jumps, ~110 reversals). Not modelled: replanning, traffic, gravity, network jitter. Scripts: `D:/tmp/elite_s2_replay_build.sh`, `elite_s2_replay.sh`, `elite_s2_all.sh`, analysis `elite_s2_decision.py`, `elite_s2_window.py`.
- Offline result (cases `D:/tmp/elite_s2cap_case.txt` I, `elite_s2capY_case.txt`, `elite_s2capX_case.txt`): I delay 1/3/6 and Y/X delay 3 all reach the route end and settle (< 0.5 m/s), corridor violations 0 (max cross-track I 19-20 m, Y/X 46 m, tol 64 m), lateral velocity <= 23 m/s, safety overrides 0, hull-target jumps 4-7 per flight, no strong main at >15 deg achievable error. Old pilot on the same I case: 2-3 violations, up to 157 m, 143 m/s, 225 jumps.
- Offline iterations recorded: plan anticipation leaking into a curve (fixed: burns hold their axis), unreachable short burns (dropped if optional), back-to-back burn junctions (turn windows), coast too short between burns (window rule generalised), a speed catch-up cancelling a curve (curve axis cone), periodic re-planning REJECTED (a committed-once program made I worse: re-anchoring on the actual speed is what keeps the plan feasible).
- Unit: `NewtonianOptimalControllerTests` rewritten for the program doctrine (8 tests) PASS.

### Pre-registered real-flight series (max 3 flights: Newtonian-I, then Y and X sanity; 240 s each, Step 1a route)
Hypothesis: the program pilot executes the Step 1a route safely and calmly. Baseline: old pilot `s2base` (I): 5 violations, max 100 m, lateral 139 m/s, 207 jumps, 0 strong misaligned main.
PASS per flight: 0 corridor violations (cross-track <= 64 m), no strong main at >15 deg achievable error, lateral velocity <= 30 m/s, hull-target jumps (>25 deg/tick) <= 20, stable progress to the terminal hold. FAIL: any lateral runaway / corridor exit > 10 %, or stalled progress.

### Real-flight series 1 (Step 2 program pilot, commit `dcf89376`, 240 s each) — corridor PASS, terminal FAIL
| model | corridor violations | max cross-track | max lateral v | hull jumps >25 deg/tick | strong misaligned main | route end reached |
|---|---|---|---|---|---|---|
| I | 0 | 25.0 m | 11.1 m/s | 4 | 0 | 226.8 s |
| Y | 0 | 22.8 m | 17.0 m/s | 8 | 0 | 194.9 s |
| X | 0 | 30.2 m | 19.1 m/s | 10 | 0 | 187.2 s |
Baseline old pilot (I, `s2base`): 5 violations, 100 m, 139 m/s, 207 jumps.
FAIL at the terminal: in the game the route projection stalls ~2 m short of the last station (I at 32863.9 of 32866 m) while the plan still reads a BRAKE station (9 m/s, -22.5 m/s2). The brake feed-forward kept burning after the craft stopped; equilibrium = plan speed - 2 s * 22.5 = -36 m/s: the craft accelerated BACKWARD to 36 m/s (Y/X 77 m/s) instead of holding. The offline harness did not reproduce the stall (its projection reaches 32865.8 m).
Fix (offline + unit verified, flight pending): a braking feed-forward fades out once the craft is no faster than its profile and slower than 6 m/s along the track (near-zero speed ends the brake); the terminal settle (RCS only, main disabled) also starts when the actual speed is <= 2 m/s with a stop requested. Unit test `testBrakeNeverReversesTheMotion` added (9/9 PASS); offline I/Y/X matrix unchanged (0 violations, all settle).

### Pre-registered real-flight series 2 (terminal hypothesis; max 3 flights I/Y/X, 270 s)
Hypothesis: with the brake fade and speed-triggered settle the craft holds at the route end. PASS per flight: series-1 corridor criteria still met AND after the route end is reached the speed drops <= 0.5 m/s and stays there (no backward runaway). FAIL: speed at the end > 2 m/s for > 30 s or growing.

### Real-flight series 2 (terminal fix, commit `4c703cc7`, 270 s each) — PASS: Step 2 HEALTHY FLIGHT VERIFIED (nav-stress:simple, Newtonian I/Y/X)
| model | corridor violations | max cross-track | max lateral v | hull jumps >25 deg/tick | strong misaligned main | route end | settled <= 0.5 m/s | final speed |
|---|---|---|---|---|---|---|---|---|
| I | 0 | 21.6 m | 7.7 m/s | 4 | 0 | 213.2 s | 229.5 s | 0.21 m/s |
| Y | 0 | 26.0 m | 18.5 m/s | 8 | 0 | 189.7 s | 211.1 s | 0.21 m/s |
| X | 0 | 28.1 m | 19.7 m/s | 9 | 0 | 187.7 s | 191.5 s (one 1.6 m/s blip) | 0.22 m/s |
Server: parasitic RCS 0.0 kN; pitch/yaw command reversals 53-61 per full route (old pilot: 305 in 150 s); hull rotation 44-47 rad per FULL route incl. terminal (old pilot 26 rad over only 150 s / 24 km).
Assisted-I sanity (same build, 150 s): trajectory identical to the accepted Assisted baseline `D:/tmp/elite_pass_Assisted-I.log` within +-1 checkpoint per sample (start offset), failed=0; the new log is longer (417 vs 308 samples) and ends at checkpoint 76.
Unit `NewtonianOptimalControllerTests` 9/9 PASS (`bash D:/tmp/elite_s2_unit.sh` from MSYS2 MinGW64).

Known issues / next:
- Terminal attitude hold (route layer, after the stop) turns the hull with the full-authority servo: one 2.2 rad/s yaw/roll episode at rest (I t=220 s). Not a Step 2 regression; candidate: calm rate for large terminal turns.
- The offline harness does not reproduce the in-game route-end projection stall (~2 m short); covered by a unit test.
- Pre-existing red `local_flight_control_contracts` unchanged.
- Remaining plan: Step 1b (portal as a window), Step 3 (narrow-zone safety: envelope, orientation in the cross-section, limited rotation), Step 4 (gimbal sweep).
- D:/tmp cleaned (542 obsolete `elite_` files of this stage: bp*, s1*, replay r0-r17, temp scripts). The old `elite_bp_metrics.py` / `elite_bp_base_I*` referenced in the 2026-10-27 sections no longer exist; current tools: `elite_s2_*`. Kept: `elite_s2base_*` (old-pilot baseline), `elite_s2cap*_case.txt` (replay cases), `elite_s2fly*`/`elite_s2fly2*` (flight evidence), `elite_r18*` (last offline matrix).

## 2026-10-28 Step 2 tail: terminal sequence, tolerances, hysteresis, RCS path (WIP until the terminal flight)

User rules (2026-10-28): no fight for a mathematical zero anywhere — physical tolerances, deadbands and hysteresis in every layer; the last metres are RCS work (main inhibited, no hull turn for a few metres); RCS trim also has deadbands; velocity / thrust direction / hull attitude / roll are separate objectives depending on the phase (open transit: thrust vector, roll free; docking: full pose incl. roll vs the dock frame; narrow corridor: orientation-aware envelope = Step 3); docking precision comes from the capture geometry, not from double precision.

Diagnosis of the series-2 terminal (I, t 213-240 s):
1. The 2.2 rad/s turn: once speed <= 0.5 m/s the route layer sets `terminalAttitudeHold` with the final route pose (forward + up); the pilot executed it with the full-authority pose servo (pitch/yaw/roll inputs saturated at +-1) from the retro braking attitude. No tolerance, no calm rate.
2. `precisionStopControl` commanded (v_des - v)/dt: a 0.2 m/s residual became 10 m/s2, RCS saturated with alternating sign every tick and the speed stayed at 0.21 m/s ("fight for zero").
3. The speed-triggered settle pulled an 18 m lateral offset in with tau 2.5 s, saturating RCS and reaching 2.7 m/s sideways, toggling in/out.
4. A route checkpoint refresh at the route end recompiled the manoeuvre program after the stop (revision 77).
5. The calm-turn / pose-servo switch at 25 deg had no hysteresis (COAST t 205-209 s: switch every ~0.5 s, pitch command reversals +-0.45).
Gray keyboard: the keypad (`PlayerInputMapper.cpp:80-93`) writes `forwardInput/strafeInput/liftInput`; the autopilot writes the SAME fields; both go `ShipControlExecutionGateway` -> `DynamicMotionSystem::applyLocalFrameInput` (input x `manoeuvreAccelerationLimitMps2` per body axis) -> `ShipCore::updateTranslationControl` -> `allocateNewtonianRcs` (24 nozzles, power/gas bounded). Only differences: the manual release-cancel ledger is off for the autopilot (`rcsKeepsTranslationDeltaV`), and the autopilot uses 60 % of the scalar limit (the rest stays for attitude/counter-torque). The scalar limit is the weakest signed axis of the real allocator probe (infinite power, no moment): it understates strong axes and overstates availability while main burns/rotation competes. Signed per-axis authority is already published (`ShipControlAuthorityTelemetry.positive/negative`) — candidate refinement, not needed for the terminal.

Changes:
- Route layer hands the pilot an explicit terminal target (`AutopilotControlRequest::terminal*`): final position, forward, up and the route's acceptance tolerances (position 3 m, speed 0.75 m/s, attitude 5 deg, roll 5 deg until a dock frame defines its own, angular rate 0.05 rad/s).
- Pilot terminal sequence (stop requested + terminal target, entered at <= 2 m/s): BRAKE (program) -> POSITION_HOLD (main inhibited, hull keeps the arrival attitude, RCS trim toward the end point at <= 0.6 m/s) -> ATTITUDE_ALIGN (calm full-pose turn at the preferred rate/acceleration = 0.5 x installed, RCS keeps the position) -> RATE_SETTLE (entered when aligned and slower than 4 x the rate tolerance) -> DONE (no command at all). "Stopped" = drift <= position tolerance / 60 s. Forward transitions at 1 x tolerance, leaving any reached step only beyond 2 x; RCS trim deadband inside 0.5 x. No manoeuvre-program recompilation once the terminal sequence runs.
- `precisionStopControl` (non-route stops): residual trimmed over 1 s, deadband 0.05 m/s.
- Calm-turn switch hysteresis: enter > 25 deg, leave < 12 deg.
- DEV trace: `term_step`, `term_dist_m`, `term_att_deg`, `term_roll_deg` columns and `TERMINAL_STEP` events; report `python D:/tmp/elite_s2_terminal.py TAG`.
- Unit 12/12 PASS (new: last metres are RCS trim, calm terminal turn, DONE corrects/recompiles nothing).
- Offline (replay I d1/d3/d6, Y, X): corridor unchanged (0 violations); terminal POSITION_HOLD -> ALIGN -> RATE_SETTLE -> DONE exactly once, max omega 1.05-1.09 rad/s (was 2.2), throttle 0 throughout, after DONE throttle/RCS/rotation all 0, final drift 0.04 m/s, attitude <= 1.2 deg, roll <= 1.1 deg, no recompilation.

### Pre-registered terminal flight (1 real flight, Newtonian-I, 270 s)
PASS: corridor criteria of series 2; terminal steps in order without oscillation; main throttle 0 after the terminal sequence starts; max angular rate in the terminal <= 1.2 rad/s (preferred rate); DONE reached; after DONE no main, no RCS, no rotation command and no PROGRAM recompilation.

### Terminal flight 1 (commit `f8c09e3e`, I, 270 s) — near PASS, one criterion missed
Corridor 0 violations (26 m). Terminal: POSITION_HOLD 15 m -> 3 m on RCS (main 0) -> ATTITUDE_ALIGN (180 deg + 154 deg roll, max omega 1.10 rad/s) -> RATE_SETTLE -> DONE at 243.6 s; no PROGRAM recompilation after the terminal start; final 0.4 m, 0.04 m/s, attitude 0.7 deg, roll 1.5 deg. MISSED: 0.1 s after DONE the rate rose 0.014 -> 0.117 rad/s (commands still executing through the control delay) and the sequence went DONE -> RATE_SETTLE (rotation command <= 0.2 for 0.4 s) -> DONE.
Fixes (offline): DONE needs the still/aligned/slow state continuously for 1 s; with that, RATE_SETTLE exposed a limit cycle (omega 0.05-0.07 rad/s, inputs +-0.1 alternating) — the calm pose law (sqrt(2 a theta), infinite gain near zero) kept correcting a 0.4 deg error inside the 5 deg tolerance through the delay. Now RATE_SETTLE only damps the rate (no angle correction), ALIGN has an angle deadband of 0.5 x tolerance and a linear final approach (theta / 1 s). Offline I d1/d3/d6, Y, X: steps exactly once, RATE_SETTLE 1.6-3.0 s, after DONE throttle/RCS/rotation all 0, final attitude 1.9-4.4 deg, roll <= 3.8 deg, 0.5-2.1 m. Unit 12/12 PASS.
Terminal flight 2 (same pre-registered criteria) is the last flight of this hypothesis.

### Terminal flight 2 (commit after `f8c09e3e` + dwell/deadband, I, 270 s) — PASS: Step 2 terminal HEALTHY FLIGHT VERIFIED
Corridor 0 violations (21.7 m), lateral <= 8.5 m/s, 5 hull jumps, no strong misaligned main. Terminal: POSITION_HOLD 217.6 s (12.7 m, 1.29 m/s; RCS trim only, approach 0.6 m/s, RCS idle in its deadband once on the approach speed) -> ATTITUDE_ALIGN 233.4 s at 3.0 m (calm 180 deg + 65 deg roll, max omega 1.09 rad/s = preferred rate, main 0) -> RATE_SETTLE 240.1 s -> DONE 242.2 s (0.59 m, 0.04 m/s, attitude 1.6 deg, roll 1.9 deg, omega 0.000). After DONE: main 0, RCS 0, rotation commands 0, no PROGRAM recompilation; 33 s later still 0.49 m, 2.9 deg, 2.7 deg (inside 3 m / 5 deg; residual 0.04 m/s drift keeps it inside the 3 m tolerance for > 60 s by construction).
Step 2 is closed. Next: Step 1b (portal as a window) / Step 3 (narrow-zone safety with an orientation-aware envelope). Open refinements noted: signed per-axis RCS authority (published telemetry) instead of the weakest-axis scalar; dock frame (position, normal, up, right) and ship docking frame with capture-geometry tolerances when docking is implemented.
Evidence kept: `D:/tmp/elite_s2term2I_*` (this flight), `D:/tmp/elite_s2fly2*` (series 2), cases `D:/tmp/elite_s2cap*_case.txt`; the user's archive `D:/tmp/elite_s2base_build.zip` holds the earlier stage files.

## 2026-10-28 diagnostics tooling moved into the repository; D:/tmp policy
User decision: the agent alone keeps D:/tmp clean; files needed for work live in the repository and are committed.
- `tools/navigation_runtime/newtonian/` (see its README): `unit.sh`, `replay_matrix.sh`, `replay.sh`, `fly.sh`, `series.sh`, `decision.py`, `window.py`, `terminal.py`, and the replay cases `cases/newtonian-{I,Y,X}.case` (the accepted Step 1a routes). Verified: `unit.sh` 12/12 PASS, `replay_matrix.sh` reproduces the offline results above exactly.
- D:/tmp (`$ELITE_DIAG_DIR`) holds only transient `elite_*` outputs of the current run and is emptied after each stage. The `D:/tmp/elite_*` script and trace paths named in earlier sections of this file no longer exist; their results are recorded here as text and can be regenerated with the tools above (old-pilot baseline: `fly.sh` on commit `1f136356`).

## 2026-10-28 checkpoint before the external integration audit

Architecture boundary (user decision): the Newtonian route autopilot is a NAVIGATION system — current state -> safe route -> fly it -> reach a navigation terminal state -> stop and stabilise (`POSITION_HOLD -> ATTITUDE_ALIGN -> RATE_SETTLE -> DONE`). DONE = "navigation terminal state reached" (staging / waiting / rendezvous / plain end point), NOT "docked". Automatic docking (rotating dock, moving platforms, relative motion, capture envelope) is a separate future layer taking over from DONE (`ROUTE_AUTOPILOT DONE -> DOCKING_AUTOPILOT ACQUIRE`); formation flight likewise (not implemented). Code comments on the terminal target/sequence state this explicitly. The nav-stress target `dock_gate_front` is a staging point in front of the dock.

Status of the Newtonian commits on main (WIP = merged but not accepted as a whole on their own):
- `c1c99fcc` RCS delta-v ownership + installed main projection: VERIFIED (flights).
- `5e621ebd` nozzle/RCS counter-torque cooperation: WIP by meaning (flown in every healthy flight since, no dedicated acceptance; damaged-ship behaviour not checked).
- `571fc01e` Step 1a route geometry: flight-verified only together with the Step 2 pilot (series 2); Y/X routes' 700/150 m tunnel straights remain inherited design values; BLUE KeepInside +22 m erosion PROVISIONAL.
- `1f136356` old experimental ballistic pilot: superseded (baseline only), its code was replaced by `dcf89376`.
- `fe8bfde6` offline replay harness: DEV tool, working.
- `dcf89376`, `4c703cc7`, `f8c09e3e`, `ae0ba2d1` Step 2 program pilot + terminal sequence: HEALTHY FLIGHT VERIFIED on nav-stress:simple (I/Y/X corridor + terminal; terminal sequence flight-verified on I, offline on Y/X); healthy ship only; gimbal 7 deg PROVISIONAL until Step 4.
- `df6c3773` diagnostics tooling in the repo.
Known red test: `local_flight_control_contracts` (pre-existing, Assisted fore-main logic).
Not verified: damaged ships, other scenarios than nav-stress:simple, narrow zones (Step 3), portal-as-window (Step 1b).

Next (pre-approved order): Step 1b portal as window -> signed per-axis RCS capability + no impossible manoeuvre phases in the compiler -> Step 3 narrow-zone safety (orientation-aware envelope) -> Step 4 N1 gimbal sweep. Damaged-ship navigation is a separate later stage.

## 2026-10-28 Step 2 cleanup 5.2: the compiler no longer plans impossible manoeuvre phases (WIP; unit + OFFLINE PASS, flight pending)
Diagnosed with the new DEV dump `ELITE_NEWTONIAN_PROGRAM_DUMP=<tsv>` (+ `..._REVISION=<n>`: station by station input/planned speed and acceleration, main, phase, hull vs tangent/axis). Causes of the "~171 deg deficit" burns and their fixes:
1. A short coast (< 1 s) merged into a burn got a default +T axis while the burns around it were brakes -> merged stations now inherit the surrounding burn's axis and phase kind.
2. A sub-m/s trim (profile re-anchored on an actual speed slightly above its cap, e.g. 400.94 vs 400 m/s) glued itself via the gap merge to the previous acceleration -> burns are split at along-track sign changes and burns whose delta-v the RCS clears within 2 s become RCS work BEFORE the gap merge; their speeds are re-integrated at the RCS rate and the change is only accepted if the trim is complete before the next burn starts (never faster than the profile where the next manoeuvre begins).
3. Main burns below 2 m/s are not planned (near-zero speed ends a manoeuvre; the last metres are RCS).
4. Missing turn windows for opposite longitudinal burns: accelerate->brake (peak: the plateau is lowered inside the peak's own runs until a calm 180 deg turn fits; if it cannot fit, the pointless speed-up before the brake is removed) and brake->accelerate (valley, e.g. into a zone limit: a plateau at the valley speed; the brake no longer turns the hull away mid-brake before a speed-limited zone). A first version let the peak plateau spill across a curve and flattened a whole 5.6 km straight (offline caught it); plateau now confined, and reshaping uses the profile's own planned authority (its largest +/- acceleration) instead of the controller's main limit (the replay showed 31.3 vs the profile's 22.5 m/s2).
5. Recompiled programs planned the hull from the route START with the present hull -> "deficits" on stations behind the craft. The hull forward pass, the unreachable-burn rule and the deficit report now start at the craft's present station; the speed profile and turn windows still cover the whole route (truncating the stations broke the curve->brake windows: offline Y entered R 668 at 241 m/s).
6. Terminal approach: POSITION_HOLD glides in at up to the 2 m/s entry speed, bounded by sqrt(RCS budget x distance), instead of a fixed 0.6 m/s (I needed 36 s for 19 m).
Offline (replay_matrix): I d1/d3/d6, Y, X: 0 corridor violations, max cross-track 19-26 m, lateral <= 10 m/s, DONE everywhere (main/RCS/rotation 0 after DONE); program thrust deficit 0 deg ahead of the craft (single-compile values only at the at-rest start, where the gate holds main until aligned). Unit 13/13 (new: a sub-m/s trim is RCS work).

## 2026-10-28 Step 2 cleanup 5.1: signed per-axis RCS capability (WIP; unit PASS, flight pending)
Finding: the authoritative control telemetry already publishes signed six-axis RCS authority (`ShipControlAuthorityTelemetry.positive/negative`, from the real 24-nozzle allocator probe, with damage). Only the weakest of the six linear directions reached the pilot (`applyNewtonianRcsScalarBounds` -> `manoeuvreThrusterAccel`) AND the server mapped every translation input as input x that scalar, so a strong axis could never be used beyond the weakest one (keypad included).
Change (same physical path as the keypad; Assisted untouched):
- `ShipParams::newtonianRcsPositiveMps2/NegativeMps2` (appended; body X right, Y up, Z aft) filled by `applyNewtonianRcsScalarBounds` on both server capability and client telemetry; helpers `newtonianRcsPerAxisKnown`, `newtonianRcsDirectionAccelerationMps2` (ShipDynamics.h).
- `ShipControlState::rcsTranslationPerAxisAuthority` (wire protocol 17 -> 18): when set, `DynamicMotionSystem::applyLocalFrameInput` scales each input by the signed authority of ITS body direction; the keypad keeps the scalar meaning (flag false).
- Pilot `applyRcsTranslation`: demand split onto body axes, each bounded by its own direction's authority x share (a lost direction is simply 0), uniform scaling inside that box, per-direction normalized inputs, flag set when per-axis data exists; used by program, direct, terminal and precision-stop control.
- Not modelled in the published numbers: simultaneous torque, power while the main burns, gas (the server allocator still scales the request by available power); the pilot keeps its 40 % attitude reserve.
- Replay cases dumped before this change have no per-axis data (the reader now accepts the shorter ShipParams) and exercise the scalar fallback; offline matrix unchanged (0 violations, DONE everywhere). Unit 14/14 (new: per-axis normalization, lost direction never commanded).

### Pre-registered real-flight series (5.1 + 5.2; Newtonian I/Y/X 270 s, Assisted-I 150 s regression)
PASS: Newtonian corridor 0 violations, lateral <= 30 m/s, terminal DONE with main 0 and no command after DONE, no PROGRAM deficit ahead of the craft beyond the at-rest start; `rcsTranslationPerAxisAuthority` active; Assisted-I trajectory unchanged vs `aac73728`-era behaviour (checkpoint progression, failed=0). The flights also dump fresh replay cases with per-axis data (to replace `cases/newtonian-*.case`).

### Real-flight series 5.1/5.2 (commit `6fb24bd6`; I/Y/X 270 s + Assisted-I 150 s)
- Corridor PASS on all: 0 violations, max cross-track 18-20 m, lateral <= 12.2 m/s, 1-2 hull-target jumps, no strong misaligned main, no program deficit ahead of the craft.
- Assisted-I: checkpoint 76, failed=0 (same as before) -> unchanged.
- Terminal FAIL: all three stuck in RATE_SETTLE at 0.12-0.16 m/s drift (DONE never reached). Cause: with the per-axis authority the RCS dribble filter compared a ~0.08 m/s2 trim with 1 % of the STRONGEST axis (I: ~15 m/s2 axes -> 0.15 m/s2 threshold) — and then with 1 % of its own strong axis — so fine trims never reached the thrusters. Offline had not shown it because the old replay cases had no per-axis data (scalar fallback). Fix: a component is a dribble only below min(1 % of its own direction's authority, 0.01 m/s2). The replay cases in `tools/navigation_runtime/newtonian/cases/` are replaced by the s51 flight dumps (with per-axis physics); offline I d1/d3/d6, Y, X then reach DONE (~16 s terminal, final drift 0.03 m/s, attitude <= 3.8 deg).
- Known (performance, secondary): routes got slower — I reached the terminal at 266 s (217 s before 5.2) in the real flight, 216 s offline; losses at the R 143 arc / 700 m block, the R 810 arc and the straight after it, partly plan churn after recompiles (e.g. a brief 179 deg hull target during an acceleration) and the conservative peak/valley reshaping of 5.2. To revisit after Step 1b/3.

## 2026-10-28 Step 1b: portal as a window (WIP, flight pending)
Map (Explore): portals have no aperture of their own; the planner used anchor centres (`RouteStageCompiler::resolvePortalWorld`), the BLUE volume sections are circles of radius 100 m with required clearance 22 m, and the composer wrote exact centre lines `blockStart -> portal -> centreline -> portal -> blockEnd`. KeepInside validation already accepts off-axis points (eroded volume, agent radius + clearance).
Change: `traffic/TrafficWindowCrossing.h::chooseWindowCrossings` runs right after `RouteStageCompiler::compile` for Newtonian only (Assisted keeps centred portals). For every straight KeepInside/PreferInside block it picks one lateral offset inside 0.9 x (section radius - agent radius - required clearance), on 4 rings x 16 spokes, minimising the turn of the free leg into the block plus the turn out of it toward the next block/goal (tiny preference for the centre), and shifts the whole block (portals, authored centreline, both protected straights, the free stages' endpoints, via points). The volume is not moved; bent tunnels keep their centreline. Log `[TrafficWindowCrossing]` (offset, admissible radius, centred vs chosen turns).
Pre-registered flight (I/Y/X 270 s; same build also verifies the terminal dribble fix `d0955173`): PASS = route accepted (KeepInside valid), chosen offsets within the admissible radius, total turn into+out of the block <= centred, corridor 0 violations, terminal DONE with main 0 and no command after DONE.

### Step 1b results (real flights `s1b`, then a cost fix)
- Real flights I/Y/X with the first cost (pure turn angle): route accepted (KeepInside valid), corridor 0 violations (max 18-20 m), lateral <= 13 m/s, terminal DONE on all three (also verifies the RCS dribble fix `d0955173` in flight: POSITION_HOLD -> ALIGN -> RATE_SETTLE -> DONE, main 0, no command after DONE, final 0.03 m/s). The block was moved to the window edge (54.7 m of 54.7 admissible) for a gain of only 0.35 deg (264.19 -> 263.83 deg total turning: the free legs are kilometres long) and the routes got 70-96 m longer.
- Cost fixed: clearance is a criterion — an offset must buy turning (edge = 2 deg, quadratic in the used share). On nav-stress the crossing now stays centred (log `[TrafficWindowCrossing] ... offset_m=0`, written to `D:/tmp/elite_docking_planner.log` because the native client has no stdout) and the route is identical to the flight-verified Step 1a geometry.
- Unit `tests/navigation_runtime/TrafficWindowCrossingTests.cpp` (ctest `traffic_window_crossing`): an oblique short approach moves the whole block toward the approach side inside 0.9 x (100 - 22 - agent radius), reduces the turning, shifts portals/straights/free-stage ends/via points consistently and never the volume; far legs keep the centre.
- Status: Step 1b SOURCE COMPLETE; mechanism unit-verified; on the only flight scenario the window gives no benefit (centred) — its flight value shows only with short/oblique approaches, which nav-stress does not have. Not yet considered: choosing among several consecutive gates jointly, curvature/braking in the cost (straight-line turn proxy only), bent tunnels.

## 2026-10-28 Step 3: orientation-aware narrow zones (WIP, flight pending)
Principle: carry the physical ship, not its centre of mass. The hull is a box (half width/height/length from the descriptor's logical dimensions, now in `RoutePlanRequest::vehicleWidthMeters/vehicleHeightMeters`).
- `traffic/NarrowZoneEnvelope.h::analyseVolumeFit`: a straight volume the bounding sphere fits in = free rotation (old behaviour, sphere + required clearance). If only the ALIGNED box fits (nose along the volume axis; for a rectangle also the roll that fits, level or rolled 90 deg; cross-section widened by halfLength x sin(5 deg attitude tolerance)) the volume is a narrow zone. Neither fits -> the route is rejected ("the ship does not fit the mandatory volume in any attitude").
- Validator: `validateKeepInsideRouteIntervalInsets` with separate right/up insets (aligned box extent + clearance); `progressOnRoute`.
- Composer: narrow volumes validated with the box insets and emitted as `RoutePlan::narrowZones` (progress portal->portal, axis, up, lateral room right/up, lane speed). Window crossing (1b) uses the box's own lateral room there.
- Execution: zone stations reach the program (`ManoeuvreRouteSample::narrow*`). Compiler: no main inside; inside speed <= sqrt(RCS rate x zone length), changed only at the RCS rate; main brakes to it before the entry (planned braking authority) and may accelerate after the exit; the zone is crossed nose-first, or stern-first when the craft arrives from a brake (the box is symmetric: no 180 deg turn for the entry); the zone entry is a fixed-axis target for the turn windows; the planned hull holds the zone axis. Executor: inside a zone no main, no safety hull override, tube tolerance = the zone's real lateral room; from 12 s before the entry and inside, the full pose (nose + roll, up of either sign, whichever is closer) is held with the calm pose law (deadband 2.5 deg). Trace columns `narrow`, `up_x/y/z`.
- DEV fixture: `ELITE_NAVIGATION_VOLUMES_OVERRIDE=tools/navigation_runtime/newtonian/scenarios/narrow_blue_volumes.json` turns the cylinder B transit volume into a 40 x 24 m rectangle with 3 m clearance (game data untouched). Cobra-I: sphere 17.3 m does not fit (9 m vertical room); aligned box 13.97 x 3.47 m fits level only, 3.0 m lateral room; RCS speed cap ~29 m/s over 1200 m.
- Unchanged scenario: offline matrix identical (no narrow zones), unit 14/14 + window 2/2.
Pre-registered narrow flight (Newtonian-I, 330 s): PASS = route accepted with one narrow zone; inside it throttle 0, cross-track <= the zone room (3 m), attitude error <= 5 deg, roll level, angular rate small, speed <= the RCS cap, no contact; route continues to a terminal DONE.

### Step 3 narrow flight 1 (`s3n_I`) and offline fixes
- First game build failed: a local variable named `near` collides with the Windows `near` macro (the replay build has no windows.h) -> renamed (`nextToTouched`, `samePoint`).
- Then the route was rejected by a SECOND KeepInside check in `SpaceState` (post-plan "assigned BLUE corridor containment") still using the sphere -> both checks now call one function `traffic::validateKeepInsideForShip` (sphere where it fits, aligned box in a narrow volume).
- Flight 1 (route accepted with one narrow zone, s 17672-18872 m): inside the zone throttle 0, attitude error <= 2.5 deg, roll error <= 2.6 deg, omega <= 0.015 rad/s, speed 31.6-34 m/s on RCS, no safety override, terminal DONE; FAIL: cross-track 9.6 m at the entry (zone room 3.0 m) — the 700 m approach was flown with the 64 m corridor tube, RCS then brought it to 0.3-0.6 m inside. Fix: the zone's room governs the tube from the same 12 s lead as the attitude.
- Offline with the dumped narrow case (`cases/newtonian-I-narrow.case`): delay 1/3 then PASS, delay 6 FAIL (a pointless accelerate-then-brake peak before the zone; a recompile flipped the hull 175 deg 6 s before the entry; entered at 66 m/s still turning). Fix: before a narrow zone the profile holds a plateau at the zone speed for 15 s (attitude lead + margin); the brake ends before it.
- Offline after both fixes, delay 1/3/6: in the zone cross-track <= 1.02 m (room 3.0), attitude <= 0.63 deg, roll <= 1.4 deg, omega ~0, speed 33-35 m/s, DONE at the end. Unchanged scenario matrix identical. Report tool `tools/navigation_runtime/newtonian/narrow.py TAG [CASE]`.
Pre-registered narrow flights 2 (I and X, 330 s): same criteria as flight 1.

### Step 3 narrow flights 2 (`s3n2_I`, `s3n2_X`, commit `ab5d897e`)
- I: PASS — in the zone cross-track 0.78 m (room 3.0), attitude 2.0 deg, roll 2.4 deg, omega 0, throttle 0, no override, speed 34-35 m/s; terminal DONE.
- X: FAIL before the zone — entered the R 104 arc (2 km before the zone, cap 48 m/s) at 253-261 m/s, corridor exit 3.9 km. Reproduced offline (delay 1). The plan was right (retro brake, then a turn window to the arc); the executor's narrow-zone lead (12 s x the CURRENT speed = 4 km at 334 m/s) already saw the zone during that brake and switched the attitude to the calm full-pose law with the zone roll; the large roll correction swung the nose 50-90 deg off the brake axis, so the brake hardly worked.
- Fix: the zone pose and tube start only on the approach plateau (plan speed <= 1.1 x the zone speed + 1 m/s), never across an earlier brake or curve. Offline narrow cases I and X (`cases/newtonian-{I,X}-narrow.case`) at delay 1/3/6: all PASS (zone cross-track <= 0.96 m, attitude <= 2.3 deg, roll <= 2.7 deg, omega 0, no corridor violation before the zone, DONE).
Narrow flights 3 (I, X) are the last flights of this hypothesis.

### Step 3 narrow flights 3 (`s3n3_I`, `s3n3_X`, commit `88f4a093`) — PASS: Step 3 HEALTHY FLIGHT VERIFIED on the DEV narrow fixture
| model | corridor violations outside | zone cross-track (room 3.0 m) | attitude | roll | omega | throttle in zone | terminal |
|---|---|---|---|---|---|---|---|
| I | 0 | 0.96 m | 2.5 deg | 2.9 deg | 0.015 rad/s | 0 | DONE |
| X | 0 | 0.84 m | 1.7 deg | 2.3 deg | 0 | 0 | DONE |
Both crossed the zone stern-first on RCS at 33-35 m/s after a braked, aligned 15 s approach plateau. Limits of the verification: one DEV fixture (straight rectangular volume, level roll), healthy ships; bent narrow tunnels, rolled-90 fits and damaged RCS are not exercised.

## 2026-10-28 Step 4: N1 main-nozzle travel selected = +/-7 deg (offline sweep on production code; 7 deg flight-verified)
Tool: `tools/navigation_runtime/newtonian/gimbal_sweep.sh TAG [build] [deg...]` (server N1 passport via `ELITE_DEV_N1_GIMBAL_DEG`, the replay applies the same travel to the client physics; summary `sweep_summary.py`). Cases: healthy I/Y/X + narrow I/X, control delay 3, 330 s.
| travel | corridor violations | max cross-track Y/X | max lateral v Y/X | note |
|---|---|---|---|---|
| 0 deg | Y 5, X 6, X-narrow 6 | 417-535 m | 72-92 m/s | no vectoring: Y/X fail |
| 2 deg | 0 | 52-57 m (tol 64) | 21 m/s | no margin |
| 4 deg | 0 | 23-26 m | 13-14 m/s | sufficient |
| 5 deg | 0 | 25-29 m | 10-12 m/s | |
| 6 deg | 0 | 23 m | 9 m/s | |
| 7 deg | 0 | 19.5 m | 8-10 m/s | accuracy plateau (I floor 19-20 m = safe tube) |
| 10 deg | 0 | 17-19 m | 3-4 m/s | calmer laterally, no accuracy gain |
| 15 / 20 deg | X-narrow 1 | 112 / 69 m | 57 / 39 m/s | wide cone degrades (to investigate for wide-cone engines, e.g. CR-1 75 deg) |
Newtonian-I is insensitive across the range; time to DONE and hull rotation vary < 3 %.
Decision: the smallest travel at which every case reaches the accuracy plateau is 6-7 deg; +/-7 deg is kept (already verified by ~25 real flights this session). `KobraN1AxialMainDrive` / passport note updated (passport status stays Provisional for its CR-1-copied thrust/thermal data).
Known: the 15-20 deg degradation on X-narrow is not understood yet (likely nozzle side-push vs counter-torque budget or the limited-burn gate at wide cones) — relevant before any wide-cone Newtonian engine flies.

## 2026-10-28 NEW STAGE (user decision): architecture hardening + regression matrix, THEN damaged flight, THEN speed
Order (user, binding): A regression matrix of `adfa5012` -> B speed tolerance corridor -> C one capability snapshot + remove fictitious `max(0.1, authority)` -> D Newtonian policy constants in one place (no retune) -> E single owner of the Newtonian speed/motion profile (reproduce current safety/timing first) -> F constrained rotation / swept hull (flip-between-walls fixture) -> G independent execution safety monitor -> H full healthy regression (offline matrix, real I/Y/X, narrow I/X, Assisted) -> I damaged cases A-D -> J only then speed optimisation. No rewrite of working mechanics, no coefficient tuning cycle, Assisted unchanged (shared low-level change => Assisted regression). Per step: checkpoint commit on main (commit != acceptance), report SOURCE / BUILD / OFFLINE MATRIX / REAL FLIGHT / ASSISTED REGRESSION / KNOWN LIMITATIONS; stop if the healthy baseline degrades.

### Step A: offline behaviour matrix (tooling committed; baseline = `adfa5012` production code)
- `EliteNewtonianScenarios` (`tools/navigation_runtime/NewtonianScenarioCases.cpp`): 21 synthetic scenarios planned by the PRODUCTION Newtonian pipeline (window crossing, `NewtonianPlanningPipeline` / `SegmentedRouteComposer`, ship KeepInside check), written as replay cases per model with a `.geom` sidecar. The request capability mirrors SpaceState's Newtonian branch (DEV copy until step C). Physics per model from the committed game-dumped cases, hull from the Cobra descriptor.
- Scenarios: long straight + wide arc, S-curve, short straight between two sharp turns, oblique portal window (13.7 m shift), obstacles on both sides, wide KeepInside tube, narrow straight (level), narrow roll-90, narrow with 200 m approach, two narrow zones (level + turned roll-90), 25 km run-up into a narrow zone, out-and-back (175 deg), reverse start (150 m/s away), 45 km cruise, starts at 399.90 / 400.00 / 400.01 / 400.50 / 400.90 / 410.00 m/s, flip-between-walls tube (50x14 m; flip itself is step F).
- `EliteNewtonianReplay`: `ELITE_REPLAY_STATE_TRACE`, `ELITE_REPLAY_STOP_STILL_S`. `matrix.py` hard criteria: corridor violations, REAL hull box corners vs KeepInside volumes with exact clearance, obstacle spheres, terminal DONE, finite state, main inside narrow zones, main during the terminal sequence, hull-target jumps not explained by a phase/program/terminal event, micro-brakes (BRAKE episode shedding < 3 m/s that is not the terminal stop). Recorded: recompiles, worst program thrust deficit, narrow angular rate, DONE time, max cross-track, re-holds after DONE. `scenario_matrix.sh TAG [build]` runs scenarios x I/X x delay 1/3/6 (8 parallel, ~15 min for 189 runs incl. Y).
- Healthy Y and X replay bit-identically (same aggregate main thrust, same 8 RCS nodes); Y is dropped from the healthy default and returns with main damage.

#### Baseline result (`adfa5012`, 189 runs I/Y/X x 1/3/6)
- PASS on every run: KeepInside (narrow hull margin 1.0-4.0 m, roll-90 zone 1.4-2.0 m), obstacles, DONE, finite, no main in narrow zones, no terminal main, no unexplained target jumps.
- FAIL `micro-brake` almost everywhere (every start speed != 400.00, cruise, narrow approaches): the runtime profile is re-anchored on the measured speed at each checkpoint (`recomputeRuntimeSpeedSuffix`: `runtimeSpeeds[anchor] = actual`), a 0.01-1 m/s excess becomes a negative-acceleration segment, the executor deadband does not apply (it requires |a_plan| <= 1e-3) and the compiler plans a BRAKE (start-at-400.01: a 173 deg flip planned to shed 0.01 m/s). Program recompiled at EVERY checkpoint (30-143 per flight). -> step B.
- FAIL `corridor` (real safety defect), X/Y only, all delays: `oblique-portal` (two opposite 112 deg R141 arcs joined by 105 m = 2 s; the hull cannot swing to the other side, 2nd arc flown without thrust, deficit 104 deg, cross-track 313 m) and `reverse-start` (836 m lead before a 173 deg R36 arc from 150 m/s; distance suffices for X braking but not the hull flip, arc entered at 90 m/s instead of 34, cross-track 240-265 m). ROOT CAUSE (one): the planner's speed limits and leads ignore hull re-orientation time between consecutive manoeuvres; the slower I passes both. -> step E (profile owner knows turn preparation time) and step G (monitor). Not hidden by criteria changes.
- Observed: program thrust deficits of 160-170 deg at single stations right after narrow exits / burn starts (burn scheduled before the hull can turn; the executor's burn gate waits) - recorded metric, not a hard criterion.
- Observed: after DONE the craft drifts with the residual 0.03 m/s and re-enters POSITION_HOLD after ~3 min (6 m = 2x tolerance) - terminal hold does not null the residual velocity.
- Planner limitation found: an exactly collinear terminal approach after a mandatory block, and an exact 180 deg reversal, are rejected ("degenerate docking turn"); scenarios use a realistic approach angle. Open.
- Note: segmented routes overshoot the goal and loop back (172 deg + 150 deg turns) when the goal is closer than the planner's terminal approach; this gives sharp-turn coverage.

### Step B: Newtonian speed tolerance corridor (WIP; unit + OFFLINE MATRIX PASS criteria below; real flight pending, batched with step C)
- Owner: `src/game/navigation/autopilot/NewtonianSpeedTolerancePolicy.h`: soft band `0.5 + 0.005*|v|` m/s (= the executor's long-standing longitudinal deadband, so that law is unchanged, it now reads the policy), real deviation beyond the band, release at half the band (hysteresis), program refresh step = 0.1 band. Terminal/capture speeds keep their own geometric tolerances.
- `DedicatedClientRouteAutopilot` (all changes Newtonian-only by `Law`/`law`; Assisted paths untouched):
  1. a start speed inside the band of the route's start limit is that limit for the strategic trajectory (400.5 on a 400 route is no braking manoeuvre); beyond the band the measured speed stays authoritative;
  2. at a checkpoint on a profile speed PLATEAU (runtime profile neither accelerating nor braking there), a measured speed inside the band re-levels the plateau at the measured speed (`toleratedPlateauEnd` + `recomputeRuntimeSpeedSuffix(..., toleratedPlateauEndIndex)`): no micro brake/burn segment, and the backward pass starts the next braking ramp where it is feasible from that speed. On ramps nothing changed (measured-speed anchor = the ramp's tracking loop);
  3. the manoeuvre program is recompiled at a plateau checkpoint only when the plateau speed moved by more than the refresh step from the one it was compiled with; ramps recompile as before.
- Failed intermediate variants (recorded, all offline): (a) anchoring on the raw nominal / the runtime value without re-levelling: the craft kept up to +2.5 m/s inside the band and entered the final braking ramp with it -> 30-37 m terminal overshoot (+10 s to DONE): the braking ramp has no recovery margin (runtime re-anchoring brakes at the planning fraction); (b) recompile only when the profile moved beyond the band: the program kept the old ramp onset -> same overshoot; (c) per-sample profile difference: the thrust-reversal coast insertion moves the ramp onset by ~20 m per checkpoint, so every checkpoint "changed" -> churn back to 65-96 recompiles. Final rule = plateau level comparison.
- `matrix.py` micro-brake criterion narrowed to a MAIN-engine BRAKE episode (throttle > 5 %) shedding < 3 m/s (a one-tick BRAKE label without main or an RCS trim is no manoeuvre); the baseline was re-judged with the same criterion.
- OFFLINE MATRIX (125 runs I/X x 1/3/6, same criteria): PASS 60 -> 90, main micro-brakes 75 -> 38, program recompiles 8129 -> 4433, total time to DONE 19343 -> 19246 s; corridor/KeepInside/obstacle/cross-track identical. Starts at 399.90 / 400.00 / 400.01 / 400.50 / 400.90: all PASS at every delay, no qualitative difference, 9-15 recompiles per flight (was 65-66). 410 m/s (beyond the band): bounded RCS correction, no flip.
- Unit: `unit.sh` PASS. Assisted: not affected by construction (law-gated); Assisted regression flight with H.
- KNOWN LIMITATIONS: residual one-tick main pulses at some in-band checkpoints in narrow approaches, curves and X at delay 6 (38 episodes) - left to step E (one profile owner) instead of more threshold tuning; the braking ramp has no recovery margin for an entry speed excess (runtime re-anchoring at the planning fraction) - step E; X/Y corridor failures unchanged (step E/G).

### Real flights B+C round 1 (`bc`, commit after `c7aba171` + step C source, nav-stress:simple) - pre-registered: 0 violations, DONE, max xtrack <= 30 m, recompiles clearly below the old ~76/flight, no cruise main micro-brake; Assisted-I unchanged
- Newtonian-I 270 s: 0 violations, max xtrack 19.2 m, DONE 244.6 s (5.2-era real flight 266 s); recompiles 71 -> criterion NOT met.
- Newtonian-X 270 s: 0 violations, max xtrack 19.4 m, DONE 211.4 s, recompiles 58; NEW finding: terminal entry flicker POSITION_HOLD <-> NONE three times in 0.3 s (route-end progress projection jitter of 0.1 m toggles `stopRequested`, the pilot reset its terminal sequence on every drop). Harmless here (no main), real fragility.
- Assisted-I 150 s: 36.6 of 49.1 km, max xtrack 14.8 m, no failure.
- Diagnosis of the recompile criterion: on this route 71 of 76 checkpoints are on ramps or on thrust-reversal coasts below the nominal (not nominal plateaus), where the plateau-only rule kept recompiling although the craft flies exactly on its runtime profile.
- Corrections: (1) terminal sequence latched while the terminal target stays valid (`NewtonianOptimalController`; unit test `testTerminalSurvivesStopRequestFlicker`); (2) recompile rule replaced: the program is refreshed at a checkpoint only when the craft is OFF the runtime profile it flies by more than the refresh step (covers the re-levelled plateau, tracked ramps and real deviations); replay of the committed real I case: 71 -> 52 recompiles, same DONE time / cross-track. Offline matrix (126): PASS 90, recompiles 4433 -> 3365, micro-brakes 38 -> 36, total time equal, safety identical; individual straight-in runs vary -10..+16 s to DONE (braking ramp without recovery margin, step E).

### Step C: one factual navigation capability, no invented authority (WIP; DEV check + unit + OFFLINE MATRIX PASS; round-1 real flights above)
- `src/game/navigation/NavigationControlCapability.h`: read-only facts re-presented from ShipParams (main forward/reverse + availability, working thrust axis, gimbal, nozzle-vectoring side force, signed per-axis translational RCS + weakest scalar, signed per-axis angular authority (new: `ShipParams::newtonianAngular{Positive,Negative}RadPerSec2` from the same control-authority telemetry) + scalar, rate limits, controlled speed limit, validity). No policy inside.
- Single reader: `makeNavigationVehicleProfile` (explicit-law overload), `newtonian::makeExecutionAuthority`, `makeManeuverCapabilitySnapshot`, the route autopilot's trajectory vehicle, and the Newtonian pilot (`factsOf`, all 27 former direct ShipDynamics reads) now read these facts. Numbers unchanged for a healthy ship: offline matrix bit-identical to step B.
- `src/game/navigation/RouteRequestCapability.h` `applyRouteRequestCapability`: the ONE request-capability implementation for SpaceState and the offline tools (reserve, 90 % planning share, Assisted concurrent-turn share, Newtonian preferred lateral share). The `max(0.1, ...)` floors in SpaceState and `max(0.1)`/`max(0.5)` in the autopilot start are removed: zero authority -> the request is refused (`PhysicallyImpossible: propulsion insufficient: no ...`), the autopilot start fails `capability-unavailable`.
- `EliteNavigationCapabilityCheck` (`tools/navigation_runtime/NavigationCapabilityCheck.cpp`, production ShipCore): 10/10 PASS - healthy facts equal the adapter; main = 0 -> refused, acceleration/braking stay 0; RCS -X = 0 -> only that direction 0 (scalar bound 0); yaw- = 0 -> signed kept, scalar 0, refused; reactors off -> Newtonian control-authority telemetry withdrawn (client waits, nothing planned), still withdrawn at 5 s of the 10 s restart, back at 11 s.
- FOUND (not fixed, damage/resource stage): RCS gas depletion scales execution on the server (`advanceNewtonianRcsGas`) but is NOT in the published capability - planner/pilot see nominal RCS with an empty manifold. Reactor loss DURING a route: the client keeps the last accepted ShipParams (capability-change invalidation is step I/22). A one-direction angular or RCS loss now refuses the route through the weakest-direction scalar (conservative; asymmetric planning is the damaged stage).

### Real flights B+C round 2 (`bc2`, commit `0070e896`) - pre-registered: 0 violations, DONE, xtrack <= 30 m, recompiles I <= 55 / X <= 50, no terminal flicker; Assisted-I no failure, xtrack <= 20 m
- Newtonian-I 270 s: 0 violations, xtrack 20.3 m, DONE 234.9 s (round 1: 244.6), 58 recompiles (limit 55: MISSED by 3), no terminal flicker.
- Newtonian-X 270 s: 0 violations, xtrack 18.3 m, DONE 198.9 s (round 1: 211.4), 51 recompiles (limit 50: MISSED by 1), no terminal flicker.
- Assisted-I 150 s: 36.6 km, xtrack 15.0 m, no failure (= round 1).
- Verdict: safety / route / terminal PASS, DONE faster than round 1; the recompile limit is missed narrowly. The remaining recompiles are ramp tracking (craft off its profile by more than the refresh step), which the terminal stop relies on; no further threshold iteration (experiment rule) - the recompile/profile structure belongs to step E.

### Step D: Newtonian policy constants in one place (behaviour-neutral; OFFLINE MATRIX bit-identical to C3, unit PASS)
- `src/game/navigation/autopilot/NewtonianFlightPolicy.h`: every doctrine constant of the manoeuvre compiler and the pilot (actuator shares, turning, burn gate, program shaping, tracking/safe tube, narrow zones, precision stop, terminal sequence) with its meaning. The pilot and the compiler keep their local names as aliases (`Policy::...`), values unchanged.
- Relations made explicit instead of duplicated numbers: one `MainShareOfRcs` (was defined twice), `NarrowApproachSeconds = NarrowAttitudeLeadSeconds + 3` (was 15 vs 12 by comment), `UnreachableSlackRad = BurnGateLimitedRad` (both 10 deg by comment); the two different "settle speeds" got distinct names (`PilotSettleSpeedMps` 2.0, `ProgramSettleSpeedMps` 0.5). Speed tolerance stays its own owner (`NewtonianSpeedTolerancePolicy`, now also `PlateauAccelerationMps2`).
- Not done (by decision, no retune): deriving the fixed seconds from capability/geometry/control delay.

### Step E: one owner of the Newtonian motion profile + turn-time aware profile and planner (WIP; unit + OFFLINE MATRIX PASS; real flights `e` pending)
- E1 (`239c735f`, bit-identical): `NewtonianManoeuvreProgram.h` split into `NewtonianManoeuvreTypes.h` (stations/phases/capability), `NewtonianMotionProfile.h` (`shapeNewtonianMotionProfile`: the ONE owner of the executable speed profile - narrow plateaus, curve speed-up within the cone, turn windows) and the compiler (phase labels + hull plan only, no second speed planner). Finding: for program control the route layer's composite target speed no longer reaches the Newtonian pilot (only `desiredVelocity` for direct mode, stop request, traces).
- E2: turn window CURVE -> CURVE (missing case = root cause of `oblique-portal` X/Y): two curve burns whose thrust axes differ beyond the cone with a gap shorter than the hull's turn window are flown, with the gap, at max(speed at which both curves are RCS work, speed at which the gap lasts the window); braked into, accelerated out of. Guard: only ahead of the craft with room for the turn and the brake (a manoeuvre already entered is not re-planned - without it a mid-S recompile demanded an impossible brake: s-curve/short-straight corridor failures, recorded). Decisions persist for the route (`ManoeuvreSpeedCap`, pilot-owned `speedCaps_`): later recompiles start from them (without persistence delay 3/6 recompiles found no room and the speed jumped back: failure unchanged, recorded).
- E3: ONE hull turn model `autopilot/NewtonianTurnModel.h` (calm rate/acceleration shares, planned turn time, flip lead) used by the pilot and the route request (`RoutePlanRequest::newtonianFlipSeconds`, filled by `applyRouteRequestCapability` from the facts); `NewtonianPlanningPipeline::withFlipRoom`: a moving start keeps a protected first straight of v*flip + v^2/(2*braking) (root cause of `reverse-start` X/Y: 836 m < ~950 m flip + brake).
- OFFLINE MATRIX (126, vs step D): corridor violations 9 -> 0 (`oblique-portal` X 313-321 m -> 19-20 m, `reverse-start` X 240-265 m -> 18-28 m), sum of max cross-track 3270 -> 1543 m, PASS 91 -> 98, micro-brakes 36 -> 34, recompiles equal, total time +0.4 %; cost: `oblique-portal` +7-10 % time (I and X), KeepInside margins unchanged or larger. Observed: `narrow-short-approach-I_d6` two 26-28 deg one-tick hull-target steps during a FLIP (threshold 25 deg; probably an acceleration step at a cap boundary) - recorded, not tuned.
- Not done in E: removing the route layer's own thrust-reversal coast insertion (a second shaping of coasts before brakes) and the planner's gate profile as separate speed owners; the braking ramp still has no recovery margin.
