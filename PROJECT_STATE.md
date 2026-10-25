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
