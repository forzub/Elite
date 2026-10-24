# ELITE — LIVE PROJECT STATE
Updated: 2026-10-24, Europe/Kyiv. Canonical branch: `main`.

> This is the ONE authoritative, living continuation document for `forzub/Elite`.
> Update it after every material finding, code correction or Windows/gameplay gate.
> Keep this document **current and concise**; historic narratives are recoverable
> from Git history. `CURRENT_STATE.md` and `CURRENT_TASK.md` are legacy histories,
> not competing current state files. Work on `main` only (no side branches).
> Record real verification level: SOURCE, LOCAL TEST, WINDOWS TEST, GAMEPLAY.
> User diagnostic outputs go under `D:/tmp`. No tests merely to add tests.

## 2026-10-24 — Windows build PASS; equipment contract fixture thermal regression FIXED IN SOURCE

**WINDOWS evidence from user:** `bash build_mingw64.sh` successfully built and
linked `build/EliteGame.exe` and `build/headless_server/EliteServer.exe`.
The `equipment_damage_failure_pipeline_tests.exe` target also compiled and
linked successfully. Native `ctest -R equipment_damage_failure_pipeline`
FAILED with `overheated engine did not accumulate thermal damage while idle`.
CMake/Embree deprecation warning, Winsock include-order and two unused
engine/reactor variable warnings are NON-FATAL, not the failure cause.

**Diagnosis (SOURCE):** an older lifetime fixture attempted one `7200 s`
full-power thermal integration with fixture engine `nominalEfficiency01=.80`,
`31.25 MW` and `12 MJ/K`. Without an authored emitter the current
single installed-engine pad law makes a full 7200-s, ~6.25 MW-waste-heat
thermal tick reach approximately 4950 K (including idle warm-up). Trapezoidal
thermal damage therefore fills the `thermal01` channel to 1.0 during the
WEAR subtest. The following `3600 s` idle step expects ANOTHER new thermal
damage event despite the channel already being full and rejects the correct
`false` no-state-change return. Its old assumption that `1500 K` coolant
causes engine overheating is also obsolete: canonical engine heat is routed
to the own propulsion pad, not back-fed from hot coolant.

**Minimal source-only fix:** `fa688f92` / `4607184e` modify ONLY existing
`tests/architecture_contracts/EquipmentDamageFailurePipelineTests.cpp`:
accelerated 2-hour operating-hour/wear scenario uses zero-waste fixture
efficiency (wear remains full-rate, thermal stays below safe T); separate
realistic 1-s idle step starts with explicitly overheated installed engine
at 1400 K and nominal 1050 K coolant. Thermal damage at 1400 K is
0.05/3600≈1.39e-5 per second, above the existing 1e-5 damage-commit
quantum. This tests real thermal damage without mixing two unrelated
timescales or changing production physics.

**Verification status:** SOURCE reviewed, Windows rerun PENDING. Do not
claim CTest passed until the user reports it. Next action:
`git pull --ff-only origin main`, build
`equipment_damage_failure_pipeline_tests`, then
`ctest --test-dir build/tests/architecture_contracts -R '^equipment_damage_failure_pipeline (SOURCE, WINDOWS GATE OPEN)

**User screenshot:** `image(20261024-073337).png` shows off-center
amber emergency working-thrust marker near right-side tunnel at ~46–50°,
fixed cyan nose sight in center. User cannot reasonably fly a narrow
manual corridor using ONE static displaced sight, reports
`Ctrl+Alt+Shift+F` disables engines but never restores them, and keypad
translation remains referenced to hull nose.

**Verified actual architecture and semantics:**
- Working thrust ray comes from server-installed `ObjectModuleRuntime` /
  `WorkingThrustAxisAnalysis`, passes through CoM for selected
  survivable motor, appears in MotherComputer and `ShipPresetEngineManeuvers`.
  Engine-out preset executes this exact selected motor and torque-neutral
  vector first, then residual 6-axis control. The offset is a physical
  **acceleration reference**, NOT the inertial flight path.
- `FlightVectorIndicatorRenderer::renderEmergencyThrustAxis` originally
  assumed `focal=0.48*min(viewport widths/heights)` and zero camera
  attachment orientation; actual `PlayerShipView` cockpit uses vertical
  70° FOV and attachment/world transform. Amber marker was therefore
  not an accurate line-of-sight projection; steering by it can miss.
- Keypad `KP8/KP2` longitudinal, `KP6/KP4` lateral,
  `KP9/KP3` vertical are **body-axis pilot controls**. This is
  correct independent of damaged main-thrust axis: the damaged engine
  does NOT redefine "up/right/forward" directions of the cockpit.
  Assisted executes these through installed main-engine vectoring;
  actual attainable acceleration may be reduced/asymmetric.
- `Ctrl+Alt+Shift+F` was NOT power off: it was irreversible in-session
  `InjectEquipmentFailure`, and consecutive presses broke successive
  healthy main motors. `Ctrl+Alt+Shift+I` independently toggles the
  propulsion-reactor lifecycle (including 10-s restart startup).

**Source changes on main, NOT Windows compiled/gameplay verified:**
- `6bbb502d`/`da92e447`: ObjectModuleRuntime now has
  `debugRestoreInternalFailure(moduleId, previousInternalFailure01)`,
  restoring ONLY its internal-failure field and revising module capability.
  Other cause channels (impact/laser/thermal/radiation/wear), emitter
  history and structural damage remain untouched.
- `8a629122`/`ceb1ea62`/`3c3dda7d`: GameServer tracks which
  installed motor F explicitly faulted for each player ship. Next F
  *restores that same test fault* instead of destroying the next drive;
  authoritative status is logged in
  `D:/tmp/elite_equipment_failures.log` as
  `APPLIED` or `RESTORED`. Existing network message/type unchanged,
  and the state-of-truth is still ObjectModuleRuntime.
  `dab77264` extends existing equipment-damage pipeline test to ensure
  test fault can restore with impact/wear preserved and correct thrust.
- `357eb8cd`/`4c98f772`/`30187ba1`: amber working axis sight
  now uses **actual cockpit camera view matrix**, actual
  ship presentation orientation, viewport dimensions and camera vertical
  FOV. Body-space ray remains the installed authoritative module fact.
  Offscreen edge pointer remains for rays outside optical field.
- `4e783b0b`/`651ce683`/`01fa4c83`: emergency mode now also draws
  a distinctive GREEN `V` flight-path marker projected from live
  hub-local measured velocity through current ship reference frame and
  exactly the same camera. Amber = physical thrust/acceleration axis;
  green V = actual velocity vector; center cyan = optical hull nose.
  It is not physically correct to draw the off-axis ray at center
  without rotating the actual view. A fully thrust-centered
  *electronic view* would require a separate deliberate camera mode,
  not a misleading relocation of the marker.

**Next verification:** native build and existing equipment-damage
pipeline CTest; gameplay undamaged -> press F -> one engine fails,
press F again -> SAME engine recovers and original physical damage
(if any) remains; press I twice -> normal reactor restart with 10-s
delay. Compare F labels/telemetry & module state against
`D:/tmp/elite_equipment_failures.log`.
Test manual emergency with amber thrust sight and green V flight path;
confirm both projections follow the rendered cockpit camera when
pitch/yaw/roll changes. Confirm keypad relative body semantics and
actual attainable acceleration in 3 axes. Do not change healthy
Assisted roll law, already user-accepted.

## 2026-10-24 — Windows/gameplay roll acceptance, follow-up telemetry audit (LOG-VERIFIED)

**User observation:** normal Assisted-I now flies smoothly; only slight corrections
in authored turns. New user logs after `9cfcda52@main/clean`:
`elite_attitude_jitter(8).csv` (HEALTHY 90.02 s, 4501 ticks),
`elite_attitude_jitter(9).csv` and
`01_Assisted_I_aft_failed_attitude(3).csv` (same emergency run, 90.02 s),
`01_Assisted_I_aft_failed(3).log` (~120 s scenario),
`elite_docking_planner(20261024-072609).log`,
`elite_equipment_state_aft-failed(2).csv`,
`assisted_damage_flights_summary(6).csv` (exit code 0).
This confirms actual gameplay physics behavior, NOT a native CTest result.

**Healthy vs prior healthy, quantitative:**
- Healthy `axis_emergency=0`, installed 4 Assisted-I motors,
  `program_valid=1`, `engine_sample_valid=1`,
  `reactor_on=1` on all 4501 ticks.
- Before (`elite_attitude_jitter(7).csv`, 50 s): roll pilot `|manual_roll|>0.95`
  on 67.85% frames, max `|roll_rate|=0.5275 rad/s`,
  p95 `|roll_rate|=0.4050 rad/s`, peak roll torque=37.24 MN·m.
- Now: **0% full-scale roll**, max `|manual_roll|=0.0153`;
  p95 `|roll_rate|=0.03537 rad/s` (11.4x lower),
  max `|roll_rate|=0.03679 rad/s`,
  peak roll torque=1.376 MN·m. Requested and actual roll accelerations
  nearly identical (corr=0.9999), p99 absolute discrepancy ~0.0012 rad/s².
  Roll remains low through 20–90 s: no recurring limit cycle.
- Healthy yaw/pitch physical tracking remains sound: correlation requested
  vs actual angular accelerations yaw=0.9711, pitch=0.9824, and
  no opposite-sign requested/executed rate-acceleration demand above
  0.02 rad/s². Initial acquisition has transient
  `|manual_yaw|max=0.936` around t=10.54s,
  `|yaw_rate|max=0.2837 rad/s` around t=10.68s; no yaw full saturation
  and for t=20-40s p95 `|yaw_rate|≈0.007 rad/s`.
  Route turns still command small finite corrections, which is expected.
- Normal-speed overshoot still observed: peak 405.635 m/s around
  t=20.30s vs ~400 m/s plateau (1.4%). Not a blocker, but don't call
  speed regulation mathematically perfect.
- This healthy file has attitude/motor telemetry, NOT pose-vs-route
  cross-track/collision metrics; it cannot establish exact lateral path
  error or final docking precision.

**Emergency separate:** axis=49.50777°, one aft engine fails
`internal_failure=1`, `live_available=0`, other equipment intact.
Peak `|roll_rate|=0.5744 rad/s` and brief `manual_roll=+/-1`
  near t~41s while adopting the route, but t=60-90s
  p95 `|roll_rate|≈0.073 rad/s`, p95 `|manual_roll|≈0.019`.
  This is transient emergency behavior, not evidence of remaining
  healthy roll instability.
  Stress report confirmed single safe-course correction, valid route
  adopted at ~40.8 s after FOUR late-entry connector retries;
  blue mandatory corridor both entered/exited, `failures=0`,
  `emergency_stops=0`, `blue_entered=blue_exited=1`,
  `status=COMPLETE` for the **scenario**, not proof of physical dock.
  Early post-rebind cross-track peak 204.657m, max course error
  33.483° near t=45s, later settles to cross-track 8.94m
  and course error 0.373° on scenario termination, under blue corridor.
  Planner valid continuous route (12 curves rebind); reactor remained on.
  No source changes needed from this audit; preserve accepted normal flight.
  If future changes needed: isolate initial damaged-route handoff
  transient (cross-track ~205m) without retuning healthy controllers.

## 2026-10-24 — Healthy Assisted route roll limit cycle (SERVER LOG VERIFIED, SOURCE CORRECTED, WINDOWS GATE OPEN)

**Request:** New user videos
`20261024-0656-24.2369226.mp4`,
`20261024-0701-04.7465595.mp4` and associated failed-engine
flight receipts/logs (scenario exit code 0) show pronounced hull rocking
even in NOMINAL/healthy Assisted mode. This is unacceptable in healthy mode.
Read server actuator CSV `elite_attitude_jitter(7).csv` and damaged
`01_Assisted_I_aft_failed_attitude(2).csv`, plus planner report and
scenario log. This does not concern remote interpolation.

**Root cause (CONFIRMED BY SOURCE + USER'S SERVER CSV):**
- The healthy trace has `axis_emergency=0`, `model=Assisted-I`,
  `engine_count=4`. After ~10.68 s, `manual_roll` repeatedly jumps
  to `+1/-1` with a ~0.35-0.40 s half-cycle; e.g. at 25.02, 25.26,
  25.62 s, commands reverse sign. At 38-39 s, the same healthy roll
  limit cycle continues. `request_roll_acc=+/-2.451663 rad/s²`
  and `actual_roll_acc` often reaches identical extreme magnitudes,
  with roll rate excursions ~0.5 rad/s. This is a *REAL server-executed
  pilot/autopilot torque*, not a camera interpolation issue.
- The same damaged scenario includes `manual_roll=+/-1` later in
  flight (from ~40 s onward), but focus of the fix is making HEALTHY
  undamaged flight quiet, not suppressing realistic failure mechanics.
- `DedicatedClientRouteAutopilot` correctly uses ONE
  `HullTunnelRollGuidance` source for live tunnel roll phase and
  `desiredRollRateRadPerSec`: the latter ALREADY includes the
  signed two-point pose error divided by actuator response time plus
  tunnel phase rate. `AssistedAutopilot` previously set
  `HullAttitudeControl::pitchYawResponseSeconds=0.50` but did NOT
  set any roll response. So `HullAttitudeControl::predictiveAxisInput`
  converted desired angular rate into acceleration using
  `(desiredRate-currentRate)/(authority*dt)` at dt=0.02,
  saturating even small changes to full +/- roll thrust every tick.
  This is an independent single-axis implementation oversight; no
  need to modify tunnel geometry, hardware, pitch/yaw gains, or netcode.

**Targeted source fixes on main (not Windows-tested):**
- `eeb42643` adds an OPTIONAL `rollResponseSeconds` to
  `HullAttitudeControl::Request`. For callers that set it,
  `dampedRollInput` follows the already-computed desired roll rate on
  a finite physical response horizon (bounded `(targetRate-rate)/(T*authority)`),
  rather than on one dt. Only when no external desired rate is supplied
  does it derive one from the pose error. Preserves legacy behavior if
  `rollResponseSeconds=0`, and preserves separate roll vs pitch/yaw
  authority channels.
- `f2aa6da2` sets `rollResponseSeconds=0.50` ONLY in
  `AssistedAutopilot`, consistent with existing accepted pitch/yaw
  0.5-s response. Zero changes to engine matrix, server physics,
  `HullTunnelRollGuidance`, route geometry or Newtonian pilot.
- `561f7812` and `f5b9e1e7` extend existing
  `HullAttitudeControlTests` to ensure mild roll does not saturate;
  the roll rate is arrested after acquisition; a bounded rigid-body
  integration converges to both STATIC and steadily ROTATING tunnel
  roll phase/rate without new +/- full-torque oscillations.
  No native C++ tests/gameplay were executed in tool environment.

**Required next Windows gate:** update main, build with
`bash build_mingw64.sh`, run navigation runtime CTest
`hull_attitude_control` and relevant Assistant/navigation tests.
Play intact Assisted-I using ordinary route; capture new
`D:/tmp/elite_attitude_jitter.csv`. Check especially
`axis_emergency=0`, `manual_roll`, `request_roll_acc`,
`actual_roll_acc` and `angular_after_roll` from seconds
10-50: no repeating +/-1 roll commands, angular rate must settle
around moving tunnel phase rate. Repeat damaged test only AFTER
healthy pass, compare allowed degraded response. Avoid disguising
remaining real actuator limits by visual smoothing.

## 2026-10-23 21:00+ — Client/server dataflow and clock regression audit (SOURCE, WINDOWS GATE OPEN)

**User request:** examine all command/physics/replication/render stages after
severe player motion and visible nearby ship jitter in
`20261023-2054-07.1108583.mp4`. Choose one physical authority, remove
duplicated calculations. Reviewed latest user files
`01_Assisted_I_aft_failed(1).log` (build
`486de16ff7f0@main/clean`),
`01_Assisted_I_aft_failed_attitude(1).csv`,
`elite_attitude_jitter(6).csv`, equipment status and receipt.

**LOG-VERIFIED:**
- PREVIOUS pre-planner server/client axis mismatch IS FIXED:
  `mother_current=1`, `mother_doctrine_valid=1`,
  `mother_server_axis=1`, `mother_client_axis=1`,
  `mother_axis_dot=1.000000`; physical working-forward=36.774938 m/s².
  Planner worker runs, finishes `job_ready=1`, `job_plan_valid=1`.
- Route adoption is STILL BLOCKED by never-completing physical
  `SecuringCourse`, not planner failure:
  `phase=5`, `safety_active=1`, `route_valid=0` until stop.
  Real SERVER angular yaw begins ~0 and then repeatedly changes direction
  up to ~1 rad/s after safe-course control begins. It is not *only*
  client visual artifact. Server `manual_yaw` (automatic pilot command)
  swings near -1/+1, actuator delivers about 0.7 rad/s² in typical
  periods, while requested acceleration approaches 2.25 rad/s².
  **Source of excessive steering FOUND:** the ordinary Assisted pilot
  already selects `HullAttitudeControl::dampedPlanarInput` with its
  accepted `pitchYawResponseSeconds=0.50`, whereas
  `AssistedSafeCourseExecutor` left that field at zero and therefore
  selected legacy `predictivePlanarInput` with dt^-1 saturation.
  `f81282e2` switches ONLY the Assisted safe-course stage to the
  established 0.50-second damped inner attitude law. This reuses a proven
  controller, does NOT change healthy route follower or engine physics.
  `4c730bce` adds a focused regression to the EXISTING isolation tests:
  a five-degree yaw correction must not demand full-scale +/-1.
  Server safe-course stability and 3D route handoff still require a
  fresh native/gameplay run; never claim the change is sufficient until then.
- User reports nearby ship jitter. SOURCE demonstrates another plausible
  presentation discontinuity: low-fidelity remotes were extrapolated from
  `itOld` alone; `itNew` position was ignored until next snapshot-pair
  switch. Across that boundary the drawn position could jump even with
  correct network clocks. The 0.2-second remote presentation delay versus
  local predicted-player timeline is an independent *deliberate* epoch
  difference; must quantify its effect rather than allege broken wire protocol.

**Complete physical/data path audited:**
`GameClient::sendAndPredictFixedStep` sends numbered
`ShipControlState` in `ClientMessage`; `GameServer` consumes
one command per fixed simulation step from
`FixedStepControlQueue` and calls
`GameSimulation::update` -> `ShipCore` physical execution (real installed
`ObjectModuleRuntime`, motors, power, torque, collision and NPC world).
`GameSimulation::buildReplicationSnapshot` publishes
`ShipSnapshot::transform` plus per-tick
`controlAuthorityTelemetry`; `GameServer` stamps server time/tick
and last acknowledged input. Existing `WireDataSchema.h` serializes
these fields and was NOT EDITED by our recent commits;
`GameClient::acceptSnapshot` monotonic-tick-fences and
`ClientWorldState::applySnapshot` hydrates.
`ClientPresentationClock` selects one delayed remote render epoch;
`ClientWorldState::update` samples remote snapshots, while local player
uses latest locally extrapolated state for low-latency controls.

**ARCHITECTURE DECISION (source):** SERVER owns equipment state, damaged motor
allocation, physical forces/torques, rigid-body/collision, thermal/reactor,
world time and NPCs. CLIENT owns pilot/autopilot guidance, route planning,
HUD, commands and strictly kinematic presentation (position/orientation
from replicated velocity/rotation). Client's outgoing packet remains
numbered pilot controls; accepting client-authored position/engine
force as truth would violate authoritative collisions/equipment/NPC
in this existing world, so not adopted. Avoid any second client motor
allocator in runtime.

**SOURCE patches on main, not yet Windows/gameplay-verified:**
- `72aa299b`: add shared `advanceReplicatedHubKinematics` using current
  server-replicated velocity and angular velocity only, with frame position
  conversion. No active control, motor program, acceleration or fake damage.
- `f8cfecfd` identified/fixed a regression in prior `c5805530`:
  off-axis fixed prediction was guarded, but the fractional render path
  still re-ran physical `MountedEngine`-less slot approximation each frame.
- `4cc1bb90`: remove ALL motor and angular-control calculations from
  ACTIVE `ClientWorldState::predict` AND
  `prepareLocalPredictedPresentation` for every ship flight family.
  Both fixed and fractional paths use one kinematic provider.
- `acc79cba` and `05add0cb`: old compatibility prediction helpers,
  including the fractional helper used by architecture tests, now
  exclusively call the same kinematic provider; no executable
  client-side actuator physics remains in these client helpers.
  `3442b78e` extends existing fractional contract test to ensure
  pilot throttle/yaw cannot synthesize thrust or angular acceleration
  in the client and to check fixed/fractional positional continuity.
- `d7f89b17` + `2174fa3c`: low-fidelity remote rendering now uses
  both adjacent canonical snapshots and endpoint velocities (Hermite)
  instead of old-only extrapolation, with one shared delayed playhead
  for all remote ships. Boundary velocity/position are continuous;
  `f110babf` extends existing remote presentation contract tests.
  High-fidelity remote rendering and server network codec unchanged.
- This removes *duplicated* physics, but **does not yet solve**
  the server's physically unstable Assisted safe-course; it may
  reduce visual player-vs-neighbor artifacts but the video outcome
  must be verified after a new native build.
- No changes to `WireDataSchema.h`, `ShipControlState` network DTO or
  `GameServer` command consumption; hence no wire-layout regression
  introduced by this change series. This is SOURCE inspection, NOT proof
  against runtime clock starvation or lost snapshots.

**Next concrete gate:** Windows user `git pull --ff-only origin main`,
`bash build_mingw64.sh`, build/run EXISTING
`local_predicted_presentation_contract_tests` and
`remote_ship_presentation_contract_tests` in
`build/tests/architecture_contracts` using their CTest names,
and existing `assisted_safe_course_executor_isolation` +
`assisted_safe_course_recovery` in nav runtime. Repeat single
`-AssistedIOnly -OnlyPreset aft-failed` matrix,
check `phase=5` acquisition, yaw request/actual acceleration,
render jitter, and valid plan admission. If phase 5 persists,
inspect server plant simultaneous wrench and safe-course braking
using the same existing D:/tmp stress+jitter traces before changes.

## 2026-10-23 late — Exact pre-planner rejection + false client yaw (LOG-VERIFIED, SOURCE PATCH PENDING WINDOWS)

**Fresh Windows artifacts from user:**
`Вставленный текст(20261023-201529).txt` contains actual full client/server
MinGW builds PASS, `mother_computer` and `working_thrust_axis_analysis`
PASS; however `hull_pose_guidance` FAIL and
`ship_control_behavior_oracle` FAIL. The new
`01_Assisted_I_aft_failed.log` is build
`23407d1e0f1c@main/clean` (prior to the changes below);
the single-engine-failure matrix still exits 3, no route.
Video `20261023-2013-15.6001157.mp4`, equipment state CSV,
`elite_attitude_jitter(5).csv` and scenario
`01_Assisted_I_aft_failed_attitude.csv` are associated evidence.

**Exactly observed route gate:** `pending=1, serial=1` but
`workers=0, job_stage=-1, phase=0, failed=0`.
The planner and follower NEVER START. Snapshot on that same ship/tick:
`mother_current=1`, `mother_envelope=1`,
`mother_motors=1`, `mother_model_match=1`, but
`mother_doctrine_valid=0`, `mother_server_axis=0`,
`mother_client_axis=1`, `mother_axis_dot=0.649345`,
`mother_working_forward=36.774938 m/s²`,
`mother_hull_forward=28.353333 m/s²`,
`mother_steering=2.25`, `mother_lateral=55.162407`,
`mother_reverse=73.549875`.
Log explicitly says `authority_status="rejected physical flight doctrine"`.
`MotherComputer::observe` intentionally rejects a client emergency
working-axis candidate whose server boolean remains nominal.
This is **axis provenance/replication disagreement**, not absence of
real acceleration and not a geometric planner failure.

**Exactly observed yaw discrepancy:** the actual SERVER player
`ShipCore` physical 50-Hz CSV (90 s) shows
`axis_emergency=1`, angle `49.50776982°` constant,
`manual_yaw=0`, `nav_angular=0`, angular yaw rate
~`-0.000124` down to `-0.0000034 rad/s` and
physical residual yaw torque from ~-2307 N·m briefly to ~+10 N·m,
with controlled thrust after 10 s near zero.
The stress CLIENT world observed `omega≈0.66..0.78 rad/s`,
`main_torque_yaw_nm≈124,299,000 N·m`,
`main_alpha_yaw≈4.908 rad/s²`, even with `autopilot=0`
and `main_cmd=0`. These are **not matching physical states**.
This is why a new large controller-gain correction or a one-shot
instantaneous hull rotation would be wrong; the SERVER already
arrests angular velocity by actual motor torque.

**Confirmed code source of two truths:** local
`ClientWorldState::predict()` calls
`ShipControlExecutionGateway::integrateAttitude` and then
`predictHubTacticalMotion` with projected `EngineSlotState`
and NO actual installed MountedEngine/operating/thermal/power state.
The server `ShipCore` uses real motor passports and accepted
six-axis `AssistedManeuverPrograms`. Local client prediction
therefore fabricates an unrelated off-axis force/torque. Its
yaw acceleration can saturate at 4.908 rad/s² despite correct
server motion. This explains the client/server yaw disagreement
more directly than an endlessly compensating autopilot:
the failing request never activates the autopilot.

**Source changes published on main, WINDOWS UNVERIFIED:**
- `e8a06ec2` in `GameSimulation::buildReplicationSnapshot` now stamps
  the authoritative Assisted working axis and emergency condition
  from the same `ShipCore::effectivePhysics()` that commands real
  motors, instead of a potentially stale last motor-tick field
  in the presentation transform. This should let `MotherComputer`
  compare matching server/client physical axes.
- `c5805530` in `ClientWorldState::predict` disables only the
  impossible *off-axis Assisted* local actuator/attitude predictor.
  It uses the existing server-replicated ship motion + render
  smoothing, not imaginary slot-only forces. Healthy centered
  Assisted and Newtonian local prediction are unchanged.
  Check whether losing damaged-mode client prediction produces
  visual smoothing/lag before designing a future real motor-powered
  predictor. Do NOT synthesize engines.
- `638ad118` reports pre-planner gate cause only on changed reason,
  with source ship/tick, server/client emergency flags, axis dot,
  motor model, and rejects unknown data conservatively.
  `d8879888` maps precise known warning categories into
  HUD terminal rather than generic log message.
  `94767dbf` adds translation keys in existing five-language
  cockpit localization file. `041d886f` also surfaces a
  FINISHED BUT INVALID planner job's existing failure category in
  the same cockpit terminal (no duplicate planner/logger), while full
  worker details remain in `job_result` and planner log.
- Existing two source tests previously FAILED for unrelated fixture
  mistakes. `307bd36d`: healthy-on-course test incorrectly demanded
  a nonzero yaw command while nose/course were already aligned; now
  expects ZERO yaw. `1c651ce5`/`9901720a`: HOME test inspected
  `ship.yawInput`, but `SharedShipPhysics::evaluateControl`
  explicitly clears input after a tick; test now inspects
  actual `ship.yawRate` and terminal mode. These change no
  physics/control parameters. Native rerun is required.

**Next Windows acceptance gate:** pull main, full
`bash build_mingw64.sh`; rebuild focused
`hull_pose_guidance_tests`, `mother_computer_tests`,
`working_thrust_axis_analysis_tests`,
`ship_control_behavior_oracle_tests`; run selected CTest entries.
Then one existing Assisted-I `-OnlyPreset aft-failed` run
into `D:/tmp/elite_engine_out_gate`.
Key checks: `mother_doctrine_valid=1`,
`mother_server_axis=1`, `mother_client_axis=1`,
`mother_axis_dot≈1.0`, then
`workers>0`, `job_result`, `route_valid`.
At the same time compare player rendered yaw with physical server
`elite_attitude_jitter.csv` yaw: if discrepancy remains,
inspect temporal/replication path, not another gain change.
Even a built and accepted planner does NOT prove damaged ship
can complete docking. No claim of new gameplay pass yet.

## 2026-10-23 — Axial-zero audit and mesh coordinate conventions (SOURCE, WINDOWS GATE OPEN)

**Request:** review every meaningful flight/navigation prerequisite that equates
"flyable" with nonzero motor acceleration along hull local -Z. DO NOT
replace geometric Z coordinates, pitch/yaw body bases, signed motor-wrench
telemetry, or Newtonian rear-main conventions with a new "thrust axis" position
coordinate. They have different meanings.

**Audited active flow:** Cobra installed engine slots and `ShipCore` real
motor allocator -> `ShipCapabilityBuilder` geometry/CoM working axis ->
`ShipControlAuthorityTelemetry` signed acceleration matrix ->
`MotherComputer` flight doctrine and envelope ->
`SpaceState::updateDockingAdvisory` admission ->
`ManeuverCapabilityAdapters` / trajectory programmer ->
`AcceptedManeuverProgramBuilder` feasibility ->
`DedicatedClientRouteAutopilot` response horizon ->
`AssistedAutopilot` -> `ShipController` -> runtime actuator executor.
Examined Newtonian primary-main selector, physical-maneuver compiler,
route geometry and safe-course separately to avoid incorrectly replacing
body axes with course axes. Source results:
- `b9568691`: real six-axis actuator telemetry now ALWAYS publishes
  `workingForwardAccelerationMps2`: healthy symmetric bank gets
  `negative[2]`; off-axis case gets selected CoM-aligned motor's attainable
  acceleration. `applyAuthorityToControlParams` ALWAYS uses working-forward
  magnitude to set Assisted forward availability/rating, irrespective of
  potentially delayed emergency status. `negative[2]` is retained as
  distinct pure hull -Z *diagnostic* or wrench basis; it is not necessarily
  physically useful course authority.
- `6a02c239`: the docking pre-planner gate now ALWAYS checks actual
  `workingForwardAccelerationMps2`, no mode-conditional pure -Z.
  It still requires bidirectional lateral, pitch/yaw and reverse basis.
  Those are separate feasibility conditions: full engine-out still might be
  rejected as a 3D docking craft even when working-forward thrust exists,
  and those constraints must be evaluated against reachable manoeuvres;
  do NOT remove or fake them merely to make a test pass. The exact failing
  magnitudes are logged via `DockClient::fail`.
- `d593011f`: `ManeuverCapabilityAdapters::makeManeuverExecutionAuthority`
  no longer grants a fictitious 0.1 m/s² to forward, reverse or lateral
  Assisted when physically absent; minimum is 0.0, so validation correctly
  reports impossible movement rather than adopting a fake manoeuvre.
- `3b851ddc`: the active Assisted route follower's extra off-axis
  angular alignment allowance uses actual body working-axis geometry,
  not solely a damage boolean; centered healthy behavior remains unchanged.
  An invalid working axis has no healthy-nose fallback.
- `62886924`, `2eb43012` extend EXISTING MotherComputer tests:
  healthy basis provides a working-forward component; missing pure -Z
  with a valid working ray preserves control authority even when status
  boolean lags; nonzero -Z with zero actual working authority grants none.
  `c0f1d38a` and `08c5ab6d` align physics field comments with this
  established separation (working-forward vector vs body -Z).
  SOURCE commits only; tests not compiled in this environment.
- A **remaining deliberate boundary**: direction-tagged sample
  `forwardMap` in `AcceptedManeuverProgramBuilder`, follower and
  trajectory planner is a WORLD/ROUTE tangent, not the ship's body -Z.
  Its acceleration projections are necessary and should remain. Newtonian
  `choosePrimaryMainBurn` still steers a physically rear main using
  an explicit hull turnaround model; this is a distinct flight law.
  No blind global substitute `z -> workingForward` is permitted.
- **Potential further requirement beyond this audit:** true loss of
  lateral or reverse *pure-body* basis does not alone prove a curved route
  impossible if the craft can reorient an angled main and execute
  sequential burns. That is a reachable-set/time-feasibility question,
  distinct from the now-correct basic working-forward admission. Must
  read new authority log before changing planner/reachability assumptions.

**Ship body coordinates remain a stable, immutable convention:**
`+X right, +Y up, -Z geometric nose, +Z stern`. Body coordinates
describe slot positions, mesh dimensions, inertia and torques; they never
need to swivel with a damaged propulsion axis. The working thrust ray is
a **dynamic vector expressed in that fixed body frame**, transformed
through the current ship attitude into world/navigation coordinates.
E.g. healthy `(0,0,-1)`; emergency `(-0.760494,0,-0.649345)`.
No new coordinate system or alternate ship "front" is created.

**Authoring/import:** `IObjectDescriptor::meshToLogicalBasis4()`
already converts the object's authored `meshForwardAxis` and
`meshUpAxis` into the canonical logical body frame. The
`AssemblyMeshLibrary` applies that matrix to OBJ mesh vertices,
module positions, pivots and rotation axes. Other ecosystems are
different: Blender mesh default Z-up and a common art-facing -Y
front, whereas this Cobra OBJ specifically has authored
`meshForwardAxis=(-1,0,0)` and `meshUpAxis=(0,1,0)`; its actual
right is -Z. `1bee56d6` corrected a contradictory comment
in `EliteCobraMk1.cpp`; NO visual/physics basis changed.
`visualBasisRotationDegValue` remains a legacy fallback and
`AssemblyMeshLibrary` currently copies authored
`module.localRotationDeg` without basis conversion: verify the
full asset-animation path for future new OBJ model orientations
before shipping them. Keep physical mounts and visual orientation
consistent through their intended authored/logical binding, not
by changing the world flight law.

**Verification:** all listed modifications are SOURCE ONLY. The last
uploaded native run was pre-audit `71bf09e8e001`, with one 50%
successful route and one 100% pre-planner blocked request.
Run a fresh MinGW64 client+server build, existing `mother_computer`
and navigation tests, then the existing single-case
`-AssistedIOnly -OnlyPreset aft-failed` flight. Look for
`mother_current`, `mother_doctrine_valid`,
`mother_axis_dot`, `mother_working_forward`, separately
`mother_hull_forward`, and `mother_lateral/steering/reverse`
to identify any *remaining* true gate. Do not claim pass yet.

## TOP PRIORITY — 2026-10-23 evening: damaged-I docking never starts planning (LOG-VERIFIED; SOURCE PATCH UNVERIFIED)

**Fresh target Windows evidence from build `71bf09e8e001@main/clean`:**
User sent `20261023-1820-25.3876699.mp4`, two per-scenario
`*_attitude(2).csv`, two `*_receipt(2).log`, two nav stress `(2).log`,
and `assisted_damage_flights_summary(2).csv`. Both failure injections are
`status=APPLIED` on real installed `ship_engine_L`.

- `aft-50`: approximately 82.2 simulated seconds, stress
  `COMPLETE/exit=0`. Planner returned `job_ready=1`,
  `job_plan_valid=1` (twice); an accepted 12-curve
  `route_valid=1`, `phase=4` executed through
  `BLUE_ENTER` and `BLUE_EXIT`. The planner/follower infrastructure
  works in the reduced-but-balanced configuration.
- `aft-failed`: approximately 137.16 simulated seconds, stress
  `GAME_EXIT/exit=3`; **every** sampled `phase=0`, `autopilot=0`,
  `workers=0`, `job_ready=0`, `route_valid=0`, `failed=0`, while
  `pending=1,serial=1`. This is a **PRE-PLANNER admission/authority
  wait**, not evidence of an invalid planner result or a follower-rejected
  route. In particular, a terminal `fail(...)` from the 3D motor-basis
  guard would have set `failed=1`, which is NOT observed. Source
  `SpaceState::updateDockingAdvisory` has an early `return`
  when `MotherComputer::assistedFlightEnvelope` is not ready/valid
  and only prints an `awaiting authoritative motor telemetry` note to
  stderr. The existing stress file omitted `m_dockingStatusWarning`
  and MotherComputer's specific gate facts, so the *precise* refusal cause
  is not yet measured.
- Physical `aft-failed` CSV still confirms
  `axis_emergency=1`, direction
  `(-0.76049403,0,-0.64934492)`, offset `49.50776982°`,
  left motor thrust 0, right motor active. Before request admission
  the fully damaged ship already produces large yaw torque and
  angular velocity despite `main_cmd=0`; this is an independent
  physical/control instability to investigate AFTER unblocking/identifying
  the entrance guard. Do not attribute it to planner or invent thrust.

**Confirmed additional SOURCE defect (not yet observed as direct second-run
failure):** after the MotherComputer authority gate, `SpaceState.cpp`
performed a 3D feasibility check with
`forward=liveAssistedAuthority.negative[2]`, which is **pure hull -Z**
and can be zero under a physical offset axis. It ignored the existing
`workingForwardAccelerationMps2` from the same signed physical matrix.
Commit `146b8fd` now uses the work-axis acceleration for emergency mode
and retains pure -Z for healthy. It still checks real lateral,
steering, and reverse authority (NO artificial authority / bypass).
A rejection now logs the exact signed magnitudes into the normal
`fail()` reason.

**Targeted missing instrumentation on main:** `bac6a937` and
`062e0fb5` extend the existing `NavigationStressHarness` observation
and `SpaceState` reporting, **not** another logger/test or an alternate
state source. Existing `SAMPLE/EVENT` lines now contain
`authority_status`, `mother_current`, `mother_envelope`,
`mother_motors`, `mother_doctrine_valid`,
`mother_doctrine`, `mother_server_axis`,
`mother_client_axis`, `mother_hull_forward`,
`mother_working_forward`, `mother_steering`,
`mother_lateral`, `mother_reverse`.
`5c4d360b` and `28c5326c` additionally expose
`mother_model_match` and `mother_axis_dot` (server vs client
unit-vector dot product, -2 means unavailable); this distinguishes an
authority-model mismatch from an otherwise valid signed motor basis
whose selected doctrine is rejected by mismatched physical axes.
This makes a pending/idle/failed=0 admission wait identifiable;
the worker result is logged by the existing `job_result` only
after it has actually run.
**No C++ build or game test after these source edits is available yet.**

**Immediate next action:** Windows pull/build BOTH game/server, then
use the **existing** runner's new `-OnlyPreset aft-failed` selector
(commit `0e2b6aa5`) with `-AssistedIOnly` to run just the broken case.
Omitting `-OnlyPreset` keeps the original two/six-case matrix.

```bash
cd /d/__elite/work
git pull --ff-only origin main
bash build_mingw64.sh
MSYS2_ARG_CONV_EXCL='*' powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$PWD/scripts/run-assisted-engine-damage-flights.ps1")" -AssistedIOnly -OnlyPreset aft-failed -ReportDirectory 'D:\\tmp\\elite_engine_out_gate'
```

The required deliverable for this one-case run is the NEW
`D:/tmp/elite_engine_out_gate/01_Assisted_I_aft_failed.log`,
plus its summary and receipt; numbering is 01 when only one case runs.
Determine whether `mother_current=0`, envelope/motor missing, doctrine
invalid from disagreement, or 3D actuator-basis deficit. If the gate
becomes ready, check actual `workers`, `job_result`,
`route_valid` and `phase` before altering planner math. Do NOT
reinterpret `job_stage=-1` as a planner failure. Keep healthy Y/X
smoothing and the installed-engine physics unchanged until supported
by more flight evidence.

## 0. 2026-10-23 — Explicit construction invariants and universal working-axis steering (SOURCE)

**User clarification:** A real installed CR-1/ER-5 cannot "lose its ID" as equipment
damage. The selected I/Y/X assembly has fixed slot-to-module identity, valid
installed equipment passports and a separately mutable runtime health record.
An empty/duplicate engine or reactor ID, absent module definition, or wrong
binding count means **invalid ship construction**, not a damaged motor. The
earlier fail-closed zero-thrust fallback protected against fabricated healthy
motors but was not a sufficient assembly-invariant check.

**Changes on main:**
- `574ddd51` validates ALL I/Y/X installed Cobra engine and reactor
  bindings at profile creation: correct slot/group counts, nonempty unique IDs,
  actual CR-1/ER-5 module passports. Violations raise a direct construction
  error before such a profile is activated. `8f68f0df` then reuses this same
  already-declared topology (avoids a duplicate local declaration).
- `38ac7751` rejects a partially populated installed actuator binding
  entirely in both runtime and replicated capability projections. No fictitious
  healthy motor AND no pretending that an invalid construction is ordinary
  engine damage. Entirely binding-free explicit isolated diagnostics remain
  separate legacy behavior, not a production Cobra installation.
- `0712a3fa` makes AssistedAutopilot always transform
  `assistedWorkingThrustAxisBody` from ship body to world; the offset
  orientation branch depends on vector geometry, **not** a damage flag.
  Healthy symmetric hardware supplies body `(0,0,-1)` and retains the
  existing nominal attitude controller. `7cf1efa4` applies the same
  universal vector rule to manual HOME/INSERT/END alignment in
  `ShipController`. The already corrected safe-course executor used the
  physical working-axis body vector unconditionally.
  `cca73c68` and `540175a5` also prevent invalid/zero/NaN
  Assisted working-axis vectors from silently becoming the healthy
  hull-nose direction in normal autopilot or safe-course entry.
- Existing `EngineThrustHudPresentationTests`, `HullPoseGuidanceTests`
  and `ShipControlBehaviorOracleTests` now directly exercise only these
  failure modes (invalid binding, off-axis vector even with lagging emergency
  flag); no extra testing system, engine fake or new contract.

**Geometry and its limits:** The 49.50776982° offset is NOT a constant.
`ShipCapabilityBuilder::applyMountedEngineAxialAuthority` uses each motor's
normalized installation coordinate * the physical half-extents in metres
minus the current ship CoM. `analyzeWorkingThrustGeometry` checks the
feasible balanced centered axis, otherwise selects a surviving motor's
torque-neutral force direction `-arm/length(arm)` inside its gimbal cone
and computes `acos(-force.z)`. For surviving right aft I: arm
`(+13,0,+11.1)` m, hence force `(-0.760494,0,-0.649345)` and
`atan2(13,11.1)=49.5078°`. Different installed slot positions, logical
physical extents or CoM produce another result **when those physical
parameters are updated**. Merely changing the drawn 3D mesh without
updating physical installation geometry does not change physics.

**Critical semantic boundary:** This is the *feasible reference forward
working thrust ray* used for torque-neutral drive/course alignment, not
a continuously sampled instantaneous vector of `sum(executed motor forces)`
during steering, reverse burn and attitude correction. Those can differ in a
maneuver; the amber HUD currently depicts the working reference. Do not
mislabel it as the instantaneous total-force vector.

**Verification:** All above are SOURCE / GitHub changes only. A post-write
source scan caught and removed a duplicate local declaration in the construction
validator before final handoff; this does NOT substitute for native compilation.
 The agent cannot
compile the Windows MinGW client/server or observe a new flight in this
environment. Do not claim acceptance before the focused native tests and
50%/100% Assisted-I flights. The accepted smooth healthy Assisted regulator
gains and navigation route geometry were NOT retuned.

## 1. Immediate task and acceptance criteria

Continue fixing the Assisted ship's engineering HUD and engine-out control.
**ONE mutable truth** for installed equipment is server
`ShipCore::ObjectModuleRuntime` (module identity, attachment, damage,
functional availability, thermal and operating state). The server derives
real motor authority and forces from it. Replication transports facts;
`MotherComputer` reads this one equipment graph and physical authority;
autopilot requests **ordinary pilot controls only**, never arbitrary
engine forces; HUD presents the same facts and no independently invented
healthy motors.

The forward *working thrust axis* is the real physically feasible total-force
direction through the current centre of mass (when torque-neutral). It is
NOT axiomatically the ship's body `-Z` nose direction. Healthy symmetric
Assisted may use `-Z` as the special case. A failed offset bank must use the
physically selected body-space vector in its plant, capabilities, flight
doctrine, safe-course entry and moving automatic attitude guidance. Do not
rotate the geometric route 49 degrees: rotate the HULL so real working thrust
aligns with the requested course. At zero thrust / terminal hold the desired
hull pose can be a separate docking reference.

The cockpit always retains its fixed cyan hull boresight. An amber
`THRUST AXIS` sight must identify **any meaningful off-nose working thrust
direction**, including an off-screen direction (edge arrow + angular offset).
If the true working axis coincides with the nose, no separate offset cue is
needed. The sight's direction belongs to the server-equipment/MotherComputer
tick, not to the interpolated render pose or a synthetic descriptor. CHECK
and each engine's yellow available/red delivered/grey unavailable bars must
agree with actual installed equipment including internal functional failure.

DO NOT retune accepted smooth Assisted I/Y/X regulators or rewrite planner
curves merely to conceal an installed-motor, replication, BITE or HUD defect.

## 2. Observed baseline — separate fact from hypothesis

**GAMEPLAY, 2026-10-22:** user reported former high-frequency pitch/yaw
chatter fixed, Assisted-I/Y/X now moving smoothly in normal operation.
Source path: `HullAttitudeControl.h` damped pitch/yaw and
`AssistedAutopilot.h` uses 0.50 s response. Prior 90.02 s Assisted-I trace
corroborated this; does not prove damaged-engine stability, Newtonian or
successful docking.

**GAMEPLAY/LOGS, 2026-10-23, BEFORE latest fixes:** two Assisted-I
preset flights, `aft-50` and `aft-failed`, injected functional damage into
the real installed `ship_engine_L` before route planning. Server physical
samples show:
- 50% case: motor 0 capacity at most 0.50; flight runner exit code 0.
- 100% case: motor 0 delivered thrust zero throughout, motor 1 continues
  delivering thrust; runner exit code **3**, so emergency route admission /
  recovery / corridor result is NOT accepted.
- Engine-out server reports `axis_emergency=1`,
  `axis_body=(-0.76049403,0,-0.64934492)`,
  `axis_offset_deg=49.50776982`, `axis_motor_index=1`.
  Body geometry X half extent 13 m, Z half extent 11.1 m.
- Nevertheless the user's HUD showed normal capacity, CHECK remained dark,
  and amber offset cue did NOT appear. Server actuator physics success does
  NOT establish that module records reached the remote client.

**Last failing client evidence:** user video
`20261023-1255-37.3977633.mp4`, 50% / failed per-run flight CSV and
summary. These were built at `fbfa722fc9e7` **before** the binding fix.
Do not claim that video tests the later corrections.

## 3. Audited implemented architecture (source on main)

- `ShipCore` owns `ShipState` (derived control/actuator state) and
  `ObjectModuleRuntime` (authoritative writable installed equipment).
  `injectEquipmentFailure` changes functional internal damage, preserves
  all other damage histories, bumps module revision. The server uses real
  mounted CR-1 capabilities to execute the physical wrench/engine torque.
- Installed Cobra Assisted configurations I (4 CR-1 / 2 ER-5),
  Y (6 CR-1 / 3 ER-5), X (8 CR-1 / 4 ER-5) have distinct persistent
  module identities. Ctrl+F9 switches the active installation without
  healing inactive damaged engines. Newtonian retains its distinct 8-node,
  24-nozzle RCS architecture; no synthetic Assisted fallback may stand in
  for installed hardware.
- `GameSimulation::buildReplicationSnapshot` -> per-module
  `ObjectModuleSnapshot` including `internalFailureDamage01` ->
  `ClientWorldState` `ship.modules` -> `collectReplicatedShipFacts` ->
  `MotherComputer` -> cockpit, CHECK, engine bars and Assisted authority.
  Compact module rows are now sent on module revision changes also to
  motion-lab probes; the larger structural/repair graph remains omitted.
- `EliteCobraMk1.cpp` `ELITE_SHIP_PHYSICAL_TEST_MODEL=Assisted-I/Y/X` now
  binds `installedAssistedEngineModuleIds(model)` rather than calling
  `slotModuleIds.clear()`. This fixed a **confirmed source cause**:
  a client descriptor with an empty binding was silently reconstructed as
  four healthy test motors despite damaged server modules (commit
  `dd8702c`). Not yet verified in new Windows gameplay.
- `ReplicatedInstalledAssistedDescriptor.h` selects actual installed
  equipment passports; `ShipCapabilityBuilder` derives their available
  forces and torque-neutral working thrust ray;
  `WorkingThrustAxisAnalysis.h` selects a valid COM ray from physically
  viable aft motors when central `-Z` is infeasible.
- `FlightControlProfile.h` / `MotherComputer.h` select the damage-offset
  doctrine from physical capability and measured motor authority, rejecting
  incompatible server/client references rather than pretending the nose
  is a healthy forward-axis fallback.
- `AssistedAutopilot.h` **already** uses working-axis-to-course pitch/yaw
  guidance during engine-out; it no longer adds a contradictory nose-to-course
  goal. `AssistedSafeCourseExecutor.h` **already** receives
  `effectivePhysics.assistedWorkingThrustAxisBody` from `SpaceState` and
  aligns the working ray, not the nose. The prior state text saying safe-course
  still uses nose unconditionally is OBSOLETE relative to current source.
- `collectCockpitView` selects a tick-matched MotherComputer working-axis
  vector; `SpaceState` passes it to `FlightVectorIndicatorRenderer`,
  which has an amber offscreen-direction sight. Normal cyan boresight
  stays separate. HUD CHECK/bars derive selected installation and BITE,
  with observed live capability capped by installed damage projection.

## 4. This 2026-10-23 source-audit slice, published on main

1. Historical `f0d2a192` replaced an invented healthy motor with
   zero authority; this behavior was **superseded by `38ac7751`**:
   partially empty installed bindings now reject the entire invalid
   configuration. Entirely binding-free isolated diagnostic topology
   is a separate explicit case, never normal installed Cobra hardware.
2. `fd39c175` — `MotherComputerClientAdapter.h`: the HUD now treats the
   replicated physical working-axis *vector* itself as evidence of an off-nose
   sight when the emergency boolean lags; validates a finite, nonzero vector.
   This is DISPLAY ONLY, not another engine state or an actuator override.
   If no server offset vector exists, the existing installed-capability
   projection remains available. Physical/autopilot laws unchanged.
3. `876ac36c` — expanded the **existing**
   `EngineThrustHudPresentationTests.cpp` to require that a blank single
   installed ID never becomes healthy and that the off-axis server vector
   remains visible even if the separate emergency flag is false. No new
   testing infrastructure or test-only production paths.
4. `d7b792ed` — `ShipController.cpp`: the legacy velocity-alignment
   path (Assisted HOME/return from free rotation and damaged
   flip-and-burn END) was STILL aligning the bare hull nose to velocity,
   independently of the already corrected navigation autopilot and
   safe-course executor. It now compares the actual body-to-world working
   thrust ray to velocity or anti-velocity, leaving physical body control
   axes and healthy/Newtonian behavior intact. Invalid off-axis reference
   fails closed instead of reverting to a fictional nominal nose direction.
   This is SOURCE ONLY, not accepted gameplay.
5. `9c55d026` — extended existing `ShipControlBehaviorOracleTests.cpp`:
   nose on velocity with 49.5-degree off-axis thrust must request yaw;
   counter-rotated hull with thrust on velocity must finish alignment;
   nominal nose-on-velocity behavior stays intact.
6. `2d6b9e3b`, `c7500d1a` — fixed `AGENTS.md` and
   `REPOSITORY_SOURCE_OF_TRUTH.md` instructions that previously told
   agents to maintain three parallel living status files. Only
   `PROJECT_STATE.md` now owns current project status; normative architecture
   documents remain independently authoritative on their actual contracts.

**VERIFICATION LEVEL: SOURCE REVIEW ONLY** for this slice (including
manual alignment correction) and previous `dd8702c` diagnostic-descriptor fix. GitHub writes succeeded; no MinGW
compilation, CTest, native Windows binary or new in-game video/trace has
confirmed the changed behavior. Do not mark engine-out or HUD accepted.

## 5. Next focused verification — no scope expansion

On user's Windows MinGW64 checkout, pull the current `main`, rebuild
**client and server**, and run focused existing tests first:

```bash
git pull --ff-only origin main
bash build_mingw64.sh
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_runtime --target engine_thrust_hud_presentation_tests mother_computer_tests working_thrust_axis_analysis_tests assisted_safe_course_executor_isolation_tests hull_pose_guidance_tests ship_control_behavior_oracle_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R "^(engine_thrust_hud_presentation|mother_computer|working_thrust_axis_analysis|assisted_safe_course_executor_isolation|hull_pose_guidance|ship_control_behavior_oracle)$" --output-on-failure
```

The test directory above is explicitly configured first, then only the six relevant existing targets are built and run.
Then rerun ONLY the existing two Assisted-I game-level scenarios:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-assisted-engine-damage-flights.ps1 -AssistedIOnly -ReportDirectory 'D:\tmp\elite_assisted_I_2runs'
```

Keep each run's original receipts, navigation stress report, video/screenshot
and separate attitude CSV. Especially inspect:
`D:/tmp/elite_equipment_state_aft-50.csv`,
`D:/tmp/elite_equipment_state_aft-failed.csv`, per-run
`*_attitude.csv` and `D:/tmp/elite_engine_damage_preset.log`.

Expected client observations (NOT yet confirmed):
- 50% `ship_engine_L`: replicated `internal_failure=0.5`,
  projected motor 0 <=0.5, CHECK/partial yellow bar; central thrust axis
  can remain feasible by balancing surviving aft motors.
- 100%: `internal_failure=1.0`, projected motor 0=0, zero red/yellow
  bar on neutral-grey track, CHECK, server and client offset vector
  agree within 49.5° tolerance and amber THRUST AXIS direction is visible
  either onsite or at screen edge.
- Compare `graph module_count`, module presence and active model with
  physical envelope and cockpit. If modules have not replicated, fix ONLY
  the server->client graph edge. If they have but bar/CHECK lies, fix
  ONLY MotherComputer/HUD consumption. Never create a healthy fallback.
- Independently check Assisted HOME and damaged END/flip-and-burn:
  the actual world-space working thrust axis must align with velocity
  (or anti-velocity for aft-main braking), not the hull nose.
- If HUD is finally correct but full-out flight still fails, use per-run
  acquired/route/admission data and yaw/torque/nozzle logs to isolate the
  actual physical feasibility, control allocation or safe-course phase.
  A ready route planner worker is not evidence that corridor execution
  was entered. Do not retune the healthy Assisted controller blindly.

## 6. Longer-lived constraints / deliberately open questions

- Autopilot is a virtual pilot issuing the same ordinary controls as manual;
  physics/server selects actual engines, force, moment, power and heat.
  Planner must use available acceleration and deceleration, not nominal wishes.
- Ordinary Assisted: motion/velocity tends toward the commanded nose/course;
  its smooth tracking was gameplay accepted. Newtonian: main aft thrust,
  turn-around braking; stabilization of fine RCS inputs is separate.
- Route remains exact planner geometry; corridor is visible in manual/auto.
  Fly with attainable lead turns, brake before infeasible corners, do not
  invent acceleration. Safe-course entry and terminal docking have distinct
  execution phases.
- Generic engine/damage state must cover partial or total loss, no healing by
  changing I/Y/X layouts, real thermal/electrical limitations and actual
  actuator/nozzle execution. A full 6DOF combined controllability certificate
  and time-bounded thermal/power feasibility are still NOT implemented;
  do not claim they are verified.
- Full engine-out corridor and docking acceptance remains OPEN despite
  server's physically correct torque-neutral 49.5° axis. Newtonian and
  other HUD issues should not obscure this precise current gate.

**Resume point:** first obtain one fresh Windows 50%/100% I test after the
actual `main` updates; compare server installed module vs client module,
MotherComputer capability/BITE and rendered sight, then correct the
first failed edge only. Continue recording findings HERE, not in parallel
status or task files.
 --output-on-failure`.
All production ship physics, HUD, F-key diagnostic toggle, and commands
remain unchanged.

## 2026-10-24 — Emergency manual thrust aiming, diagnostic F-key and keypad semantics (SOURCE, WINDOWS GATE OPEN)

**User screenshot:** `image(20261024-073337).png` shows off-center
amber emergency working-thrust marker near right-side tunnel at ~46–50°,
fixed cyan nose sight in center. User cannot reasonably fly a narrow
manual corridor using ONE static displaced sight, reports
`Ctrl+Alt+Shift+F` disables engines but never restores them, and keypad
translation remains referenced to hull nose.

**Verified actual architecture and semantics:**
- Working thrust ray comes from server-installed `ObjectModuleRuntime` /
  `WorkingThrustAxisAnalysis`, passes through CoM for selected
  survivable motor, appears in MotherComputer and `ShipPresetEngineManeuvers`.
  Engine-out preset executes this exact selected motor and torque-neutral
  vector first, then residual 6-axis control. The offset is a physical
  **acceleration reference**, NOT the inertial flight path.
- `FlightVectorIndicatorRenderer::renderEmergencyThrustAxis` originally
  assumed `focal=0.48*min(viewport widths/heights)` and zero camera
  attachment orientation; actual `PlayerShipView` cockpit uses vertical
  70° FOV and attachment/world transform. Amber marker was therefore
  not an accurate line-of-sight projection; steering by it can miss.
- Keypad `KP8/KP2` longitudinal, `KP6/KP4` lateral,
  `KP9/KP3` vertical are **body-axis pilot controls**. This is
  correct independent of damaged main-thrust axis: the damaged engine
  does NOT redefine "up/right/forward" directions of the cockpit.
  Assisted executes these through installed main-engine vectoring;
  actual attainable acceleration may be reduced/asymmetric.
- `Ctrl+Alt+Shift+F` was NOT power off: it was irreversible in-session
  `InjectEquipmentFailure`, and consecutive presses broke successive
  healthy main motors. `Ctrl+Alt+Shift+I` independently toggles the
  propulsion-reactor lifecycle (including 10-s restart startup).

**Source changes on main, NOT Windows compiled/gameplay verified:**
- `6bbb502d`/`da92e447`: ObjectModuleRuntime now has
  `debugRestoreInternalFailure(moduleId, previousInternalFailure01)`,
  restoring ONLY its internal-failure field and revising module capability.
  Other cause channels (impact/laser/thermal/radiation/wear), emitter
  history and structural damage remain untouched.
- `8a629122`/`ceb1ea62`/`3c3dda7d`: GameServer tracks which
  installed motor F explicitly faulted for each player ship. Next F
  *restores that same test fault* instead of destroying the next drive;
  authoritative status is logged in
  `D:/tmp/elite_equipment_failures.log` as
  `APPLIED` or `RESTORED`. Existing network message/type unchanged,
  and the state-of-truth is still ObjectModuleRuntime.
  `dab77264` extends existing equipment-damage pipeline test to ensure
  test fault can restore with impact/wear preserved and correct thrust.
- `357eb8cd`/`4c98f772`/`30187ba1`: amber working axis sight
  now uses **actual cockpit camera view matrix**, actual
  ship presentation orientation, viewport dimensions and camera vertical
  FOV. Body-space ray remains the installed authoritative module fact.
  Offscreen edge pointer remains for rays outside optical field.
- `4e783b0b`/`651ce683`/`01fa4c83`: emergency mode now also draws
  a distinctive GREEN `V` flight-path marker projected from live
  hub-local measured velocity through current ship reference frame and
  exactly the same camera. Amber = physical thrust/acceleration axis;
  green V = actual velocity vector; center cyan = optical hull nose.
  It is not physically correct to draw the off-axis ray at center
  without rotating the actual view. A fully thrust-centered
  *electronic view* would require a separate deliberate camera mode,
  not a misleading relocation of the marker.

**Next verification:** native build and existing equipment-damage
pipeline CTest; gameplay undamaged -> press F -> one engine fails,
press F again -> SAME engine recovers and original physical damage
(if any) remains; press I twice -> normal reactor restart with 10-s
delay. Compare F labels/telemetry & module state against
`D:/tmp/elite_equipment_failures.log`.
Test manual emergency with amber thrust sight and green V flight path;
confirm both projections follow the rendered cockpit camera when
pitch/yaw/roll changes. Confirm keypad relative body semantics and
actual attainable acceleration in 3 axes. Do not change healthy
Assisted roll law, already user-accepted.

## 2026-10-24 — Windows/gameplay roll acceptance, follow-up telemetry audit (LOG-VERIFIED)

**User observation:** normal Assisted-I now flies smoothly; only slight corrections
in authored turns. New user logs after `9cfcda52@main/clean`:
`elite_attitude_jitter(8).csv` (HEALTHY 90.02 s, 4501 ticks),
`elite_attitude_jitter(9).csv` and
`01_Assisted_I_aft_failed_attitude(3).csv` (same emergency run, 90.02 s),
`01_Assisted_I_aft_failed(3).log` (~120 s scenario),
`elite_docking_planner(20261024-072609).log`,
`elite_equipment_state_aft-failed(2).csv`,
`assisted_damage_flights_summary(6).csv` (exit code 0).
This confirms actual gameplay physics behavior, NOT a native CTest result.

**Healthy vs prior healthy, quantitative:**
- Healthy `axis_emergency=0`, installed 4 Assisted-I motors,
  `program_valid=1`, `engine_sample_valid=1`,
  `reactor_on=1` on all 4501 ticks.
- Before (`elite_attitude_jitter(7).csv`, 50 s): roll pilot `|manual_roll|>0.95`
  on 67.85% frames, max `|roll_rate|=0.5275 rad/s`,
  p95 `|roll_rate|=0.4050 rad/s`, peak roll torque=37.24 MN·m.
- Now: **0% full-scale roll**, max `|manual_roll|=0.0153`;
  p95 `|roll_rate|=0.03537 rad/s` (11.4x lower),
  max `|roll_rate|=0.03679 rad/s`,
  peak roll torque=1.376 MN·m. Requested and actual roll accelerations
  nearly identical (corr=0.9999), p99 absolute discrepancy ~0.0012 rad/s².
  Roll remains low through 20–90 s: no recurring limit cycle.
- Healthy yaw/pitch physical tracking remains sound: correlation requested
  vs actual angular accelerations yaw=0.9711, pitch=0.9824, and
  no opposite-sign requested/executed rate-acceleration demand above
  0.02 rad/s². Initial acquisition has transient
  `|manual_yaw|max=0.936` around t=10.54s,
  `|yaw_rate|max=0.2837 rad/s` around t=10.68s; no yaw full saturation
  and for t=20-40s p95 `|yaw_rate|≈0.007 rad/s`.
  Route turns still command small finite corrections, which is expected.
- Normal-speed overshoot still observed: peak 405.635 m/s around
  t=20.30s vs ~400 m/s plateau (1.4%). Not a blocker, but don't call
  speed regulation mathematically perfect.
- This healthy file has attitude/motor telemetry, NOT pose-vs-route
  cross-track/collision metrics; it cannot establish exact lateral path
  error or final docking precision.

**Emergency separate:** axis=49.50777°, one aft engine fails
`internal_failure=1`, `live_available=0`, other equipment intact.
Peak `|roll_rate|=0.5744 rad/s` and brief `manual_roll=+/-1`
  near t~41s while adopting the route, but t=60-90s
  p95 `|roll_rate|≈0.073 rad/s`, p95 `|manual_roll|≈0.019`.
  This is transient emergency behavior, not evidence of remaining
  healthy roll instability.
  Stress report confirmed single safe-course correction, valid route
  adopted at ~40.8 s after FOUR late-entry connector retries;
  blue mandatory corridor both entered/exited, `failures=0`,
  `emergency_stops=0`, `blue_entered=blue_exited=1`,
  `status=COMPLETE` for the **scenario**, not proof of physical dock.
  Early post-rebind cross-track peak 204.657m, max course error
  33.483° near t=45s, later settles to cross-track 8.94m
  and course error 0.373° on scenario termination, under blue corridor.
  Planner valid continuous route (12 curves rebind); reactor remained on.
  No source changes needed from this audit; preserve accepted normal flight.
  If future changes needed: isolate initial damaged-route handoff
  transient (cross-track ~205m) without retuning healthy controllers.

## 2026-10-24 — Healthy Assisted route roll limit cycle (SERVER LOG VERIFIED, SOURCE CORRECTED, WINDOWS GATE OPEN)

**Request:** New user videos
`20261024-0656-24.2369226.mp4`,
`20261024-0701-04.7465595.mp4` and associated failed-engine
flight receipts/logs (scenario exit code 0) show pronounced hull rocking
even in NOMINAL/healthy Assisted mode. This is unacceptable in healthy mode.
Read server actuator CSV `elite_attitude_jitter(7).csv` and damaged
`01_Assisted_I_aft_failed_attitude(2).csv`, plus planner report and
scenario log. This does not concern remote interpolation.

**Root cause (CONFIRMED BY SOURCE + USER'S SERVER CSV):**
- The healthy trace has `axis_emergency=0`, `model=Assisted-I`,
  `engine_count=4`. After ~10.68 s, `manual_roll` repeatedly jumps
  to `+1/-1` with a ~0.35-0.40 s half-cycle; e.g. at 25.02, 25.26,
  25.62 s, commands reverse sign. At 38-39 s, the same healthy roll
  limit cycle continues. `request_roll_acc=+/-2.451663 rad/s²`
  and `actual_roll_acc` often reaches identical extreme magnitudes,
  with roll rate excursions ~0.5 rad/s. This is a *REAL server-executed
  pilot/autopilot torque*, not a camera interpolation issue.
- The same damaged scenario includes `manual_roll=+/-1` later in
  flight (from ~40 s onward), but focus of the fix is making HEALTHY
  undamaged flight quiet, not suppressing realistic failure mechanics.
- `DedicatedClientRouteAutopilot` correctly uses ONE
  `HullTunnelRollGuidance` source for live tunnel roll phase and
  `desiredRollRateRadPerSec`: the latter ALREADY includes the
  signed two-point pose error divided by actuator response time plus
  tunnel phase rate. `AssistedAutopilot` previously set
  `HullAttitudeControl::pitchYawResponseSeconds=0.50` but did NOT
  set any roll response. So `HullAttitudeControl::predictiveAxisInput`
  converted desired angular rate into acceleration using
  `(desiredRate-currentRate)/(authority*dt)` at dt=0.02,
  saturating even small changes to full +/- roll thrust every tick.
  This is an independent single-axis implementation oversight; no
  need to modify tunnel geometry, hardware, pitch/yaw gains, or netcode.

**Targeted source fixes on main (not Windows-tested):**
- `eeb42643` adds an OPTIONAL `rollResponseSeconds` to
  `HullAttitudeControl::Request`. For callers that set it,
  `dampedRollInput` follows the already-computed desired roll rate on
  a finite physical response horizon (bounded `(targetRate-rate)/(T*authority)`),
  rather than on one dt. Only when no external desired rate is supplied
  does it derive one from the pose error. Preserves legacy behavior if
  `rollResponseSeconds=0`, and preserves separate roll vs pitch/yaw
  authority channels.
- `f2aa6da2` sets `rollResponseSeconds=0.50` ONLY in
  `AssistedAutopilot`, consistent with existing accepted pitch/yaw
  0.5-s response. Zero changes to engine matrix, server physics,
  `HullTunnelRollGuidance`, route geometry or Newtonian pilot.
- `561f7812` and `f5b9e1e7` extend existing
  `HullAttitudeControlTests` to ensure mild roll does not saturate;
  the roll rate is arrested after acquisition; a bounded rigid-body
  integration converges to both STATIC and steadily ROTATING tunnel
  roll phase/rate without new +/- full-torque oscillations.
  No native C++ tests/gameplay were executed in tool environment.

**Required next Windows gate:** update main, build with
`bash build_mingw64.sh`, run navigation runtime CTest
`hull_attitude_control` and relevant Assistant/navigation tests.
Play intact Assisted-I using ordinary route; capture new
`D:/tmp/elite_attitude_jitter.csv`. Check especially
`axis_emergency=0`, `manual_roll`, `request_roll_acc`,
`actual_roll_acc` and `angular_after_roll` from seconds
10-50: no repeating +/-1 roll commands, angular rate must settle
around moving tunnel phase rate. Repeat damaged test only AFTER
healthy pass, compare allowed degraded response. Avoid disguising
remaining real actuator limits by visual smoothing.

## 2026-10-23 21:00+ — Client/server dataflow and clock regression audit (SOURCE, WINDOWS GATE OPEN)

**User request:** examine all command/physics/replication/render stages after
severe player motion and visible nearby ship jitter in
`20261023-2054-07.1108583.mp4`. Choose one physical authority, remove
duplicated calculations. Reviewed latest user files
`01_Assisted_I_aft_failed(1).log` (build
`486de16ff7f0@main/clean`),
`01_Assisted_I_aft_failed_attitude(1).csv`,
`elite_attitude_jitter(6).csv`, equipment status and receipt.

**LOG-VERIFIED:**
- PREVIOUS pre-planner server/client axis mismatch IS FIXED:
  `mother_current=1`, `mother_doctrine_valid=1`,
  `mother_server_axis=1`, `mother_client_axis=1`,
  `mother_axis_dot=1.000000`; physical working-forward=36.774938 m/s².
  Planner worker runs, finishes `job_ready=1`, `job_plan_valid=1`.
- Route adoption is STILL BLOCKED by never-completing physical
  `SecuringCourse`, not planner failure:
  `phase=5`, `safety_active=1`, `route_valid=0` until stop.
  Real SERVER angular yaw begins ~0 and then repeatedly changes direction
  up to ~1 rad/s after safe-course control begins. It is not *only*
  client visual artifact. Server `manual_yaw` (automatic pilot command)
  swings near -1/+1, actuator delivers about 0.7 rad/s² in typical
  periods, while requested acceleration approaches 2.25 rad/s².
  **Source of excessive steering FOUND:** the ordinary Assisted pilot
  already selects `HullAttitudeControl::dampedPlanarInput` with its
  accepted `pitchYawResponseSeconds=0.50`, whereas
  `AssistedSafeCourseExecutor` left that field at zero and therefore
  selected legacy `predictivePlanarInput` with dt^-1 saturation.
  `f81282e2` switches ONLY the Assisted safe-course stage to the
  established 0.50-second damped inner attitude law. This reuses a proven
  controller, does NOT change healthy route follower or engine physics.
  `4c730bce` adds a focused regression to the EXISTING isolation tests:
  a five-degree yaw correction must not demand full-scale +/-1.
  Server safe-course stability and 3D route handoff still require a
  fresh native/gameplay run; never claim the change is sufficient until then.
- User reports nearby ship jitter. SOURCE demonstrates another plausible
  presentation discontinuity: low-fidelity remotes were extrapolated from
  `itOld` alone; `itNew` position was ignored until next snapshot-pair
  switch. Across that boundary the drawn position could jump even with
  correct network clocks. The 0.2-second remote presentation delay versus
  local predicted-player timeline is an independent *deliberate* epoch
  difference; must quantify its effect rather than allege broken wire protocol.

**Complete physical/data path audited:**
`GameClient::sendAndPredictFixedStep` sends numbered
`ShipControlState` in `ClientMessage`; `GameServer` consumes
one command per fixed simulation step from
`FixedStepControlQueue` and calls
`GameSimulation::update` -> `ShipCore` physical execution (real installed
`ObjectModuleRuntime`, motors, power, torque, collision and NPC world).
`GameSimulation::buildReplicationSnapshot` publishes
`ShipSnapshot::transform` plus per-tick
`controlAuthorityTelemetry`; `GameServer` stamps server time/tick
and last acknowledged input. Existing `WireDataSchema.h` serializes
these fields and was NOT EDITED by our recent commits;
`GameClient::acceptSnapshot` monotonic-tick-fences and
`ClientWorldState::applySnapshot` hydrates.
`ClientPresentationClock` selects one delayed remote render epoch;
`ClientWorldState::update` samples remote snapshots, while local player
uses latest locally extrapolated state for low-latency controls.

**ARCHITECTURE DECISION (source):** SERVER owns equipment state, damaged motor
allocation, physical forces/torques, rigid-body/collision, thermal/reactor,
world time and NPCs. CLIENT owns pilot/autopilot guidance, route planning,
HUD, commands and strictly kinematic presentation (position/orientation
from replicated velocity/rotation). Client's outgoing packet remains
numbered pilot controls; accepting client-authored position/engine
force as truth would violate authoritative collisions/equipment/NPC
in this existing world, so not adopted. Avoid any second client motor
allocator in runtime.

**SOURCE patches on main, not yet Windows/gameplay-verified:**
- `72aa299b`: add shared `advanceReplicatedHubKinematics` using current
  server-replicated velocity and angular velocity only, with frame position
  conversion. No active control, motor program, acceleration or fake damage.
- `f8cfecfd` identified/fixed a regression in prior `c5805530`:
  off-axis fixed prediction was guarded, but the fractional render path
  still re-ran physical `MountedEngine`-less slot approximation each frame.
- `4cc1bb90`: remove ALL motor and angular-control calculations from
  ACTIVE `ClientWorldState::predict` AND
  `prepareLocalPredictedPresentation` for every ship flight family.
  Both fixed and fractional paths use one kinematic provider.
- `acc79cba` and `05add0cb`: old compatibility prediction helpers,
  including the fractional helper used by architecture tests, now
  exclusively call the same kinematic provider; no executable
  client-side actuator physics remains in these client helpers.
  `3442b78e` extends existing fractional contract test to ensure
  pilot throttle/yaw cannot synthesize thrust or angular acceleration
  in the client and to check fixed/fractional positional continuity.
- `d7f89b17` + `2174fa3c`: low-fidelity remote rendering now uses
  both adjacent canonical snapshots and endpoint velocities (Hermite)
  instead of old-only extrapolation, with one shared delayed playhead
  for all remote ships. Boundary velocity/position are continuous;
  `f110babf` extends existing remote presentation contract tests.
  High-fidelity remote rendering and server network codec unchanged.
- This removes *duplicated* physics, but **does not yet solve**
  the server's physically unstable Assisted safe-course; it may
  reduce visual player-vs-neighbor artifacts but the video outcome
  must be verified after a new native build.
- No changes to `WireDataSchema.h`, `ShipControlState` network DTO or
  `GameServer` command consumption; hence no wire-layout regression
  introduced by this change series. This is SOURCE inspection, NOT proof
  against runtime clock starvation or lost snapshots.

**Next concrete gate:** Windows user `git pull --ff-only origin main`,
`bash build_mingw64.sh`, build/run EXISTING
`local_predicted_presentation_contract_tests` and
`remote_ship_presentation_contract_tests` in
`build/tests/architecture_contracts` using their CTest names,
and existing `assisted_safe_course_executor_isolation` +
`assisted_safe_course_recovery` in nav runtime. Repeat single
`-AssistedIOnly -OnlyPreset aft-failed` matrix,
check `phase=5` acquisition, yaw request/actual acceleration,
render jitter, and valid plan admission. If phase 5 persists,
inspect server plant simultaneous wrench and safe-course braking
using the same existing D:/tmp stress+jitter traces before changes.

## 2026-10-23 late — Exact pre-planner rejection + false client yaw (LOG-VERIFIED, SOURCE PATCH PENDING WINDOWS)

**Fresh Windows artifacts from user:**
`Вставленный текст(20261023-201529).txt` contains actual full client/server
MinGW builds PASS, `mother_computer` and `working_thrust_axis_analysis`
PASS; however `hull_pose_guidance` FAIL and
`ship_control_behavior_oracle` FAIL. The new
`01_Assisted_I_aft_failed.log` is build
`23407d1e0f1c@main/clean` (prior to the changes below);
the single-engine-failure matrix still exits 3, no route.
Video `20261023-2013-15.6001157.mp4`, equipment state CSV,
`elite_attitude_jitter(5).csv` and scenario
`01_Assisted_I_aft_failed_attitude.csv` are associated evidence.

**Exactly observed route gate:** `pending=1, serial=1` but
`workers=0, job_stage=-1, phase=0, failed=0`.
The planner and follower NEVER START. Snapshot on that same ship/tick:
`mother_current=1`, `mother_envelope=1`,
`mother_motors=1`, `mother_model_match=1`, but
`mother_doctrine_valid=0`, `mother_server_axis=0`,
`mother_client_axis=1`, `mother_axis_dot=0.649345`,
`mother_working_forward=36.774938 m/s²`,
`mother_hull_forward=28.353333 m/s²`,
`mother_steering=2.25`, `mother_lateral=55.162407`,
`mother_reverse=73.549875`.
Log explicitly says `authority_status="rejected physical flight doctrine"`.
`MotherComputer::observe` intentionally rejects a client emergency
working-axis candidate whose server boolean remains nominal.
This is **axis provenance/replication disagreement**, not absence of
real acceleration and not a geometric planner failure.

**Exactly observed yaw discrepancy:** the actual SERVER player
`ShipCore` physical 50-Hz CSV (90 s) shows
`axis_emergency=1`, angle `49.50776982°` constant,
`manual_yaw=0`, `nav_angular=0`, angular yaw rate
~`-0.000124` down to `-0.0000034 rad/s` and
physical residual yaw torque from ~-2307 N·m briefly to ~+10 N·m,
with controlled thrust after 10 s near zero.
The stress CLIENT world observed `omega≈0.66..0.78 rad/s`,
`main_torque_yaw_nm≈124,299,000 N·m`,
`main_alpha_yaw≈4.908 rad/s²`, even with `autopilot=0`
and `main_cmd=0`. These are **not matching physical states**.
This is why a new large controller-gain correction or a one-shot
instantaneous hull rotation would be wrong; the SERVER already
arrests angular velocity by actual motor torque.

**Confirmed code source of two truths:** local
`ClientWorldState::predict()` calls
`ShipControlExecutionGateway::integrateAttitude` and then
`predictHubTacticalMotion` with projected `EngineSlotState`
and NO actual installed MountedEngine/operating/thermal/power state.
The server `ShipCore` uses real motor passports and accepted
six-axis `AssistedManeuverPrograms`. Local client prediction
therefore fabricates an unrelated off-axis force/torque. Its
yaw acceleration can saturate at 4.908 rad/s² despite correct
server motion. This explains the client/server yaw disagreement
more directly than an endlessly compensating autopilot:
the failing request never activates the autopilot.

**Source changes published on main, WINDOWS UNVERIFIED:**
- `e8a06ec2` in `GameSimulation::buildReplicationSnapshot` now stamps
  the authoritative Assisted working axis and emergency condition
  from the same `ShipCore::effectivePhysics()` that commands real
  motors, instead of a potentially stale last motor-tick field
  in the presentation transform. This should let `MotherComputer`
  compare matching server/client physical axes.
- `c5805530` in `ClientWorldState::predict` disables only the
  impossible *off-axis Assisted* local actuator/attitude predictor.
  It uses the existing server-replicated ship motion + render
  smoothing, not imaginary slot-only forces. Healthy centered
  Assisted and Newtonian local prediction are unchanged.
  Check whether losing damaged-mode client prediction produces
  visual smoothing/lag before designing a future real motor-powered
  predictor. Do NOT synthesize engines.
- `638ad118` reports pre-planner gate cause only on changed reason,
  with source ship/tick, server/client emergency flags, axis dot,
  motor model, and rejects unknown data conservatively.
  `d8879888` maps precise known warning categories into
  HUD terminal rather than generic log message.
  `94767dbf` adds translation keys in existing five-language
  cockpit localization file. `041d886f` also surfaces a
  FINISHED BUT INVALID planner job's existing failure category in
  the same cockpit terminal (no duplicate planner/logger), while full
  worker details remain in `job_result` and planner log.
- Existing two source tests previously FAILED for unrelated fixture
  mistakes. `307bd36d`: healthy-on-course test incorrectly demanded
  a nonzero yaw command while nose/course were already aligned; now
  expects ZERO yaw. `1c651ce5`/`9901720a`: HOME test inspected
  `ship.yawInput`, but `SharedShipPhysics::evaluateControl`
  explicitly clears input after a tick; test now inspects
  actual `ship.yawRate` and terminal mode. These change no
  physics/control parameters. Native rerun is required.

**Next Windows acceptance gate:** pull main, full
`bash build_mingw64.sh`; rebuild focused
`hull_pose_guidance_tests`, `mother_computer_tests`,
`working_thrust_axis_analysis_tests`,
`ship_control_behavior_oracle_tests`; run selected CTest entries.
Then one existing Assisted-I `-OnlyPreset aft-failed` run
into `D:/tmp/elite_engine_out_gate`.
Key checks: `mother_doctrine_valid=1`,
`mother_server_axis=1`, `mother_client_axis=1`,
`mother_axis_dot≈1.0`, then
`workers>0`, `job_result`, `route_valid`.
At the same time compare player rendered yaw with physical server
`elite_attitude_jitter.csv` yaw: if discrepancy remains,
inspect temporal/replication path, not another gain change.
Even a built and accepted planner does NOT prove damaged ship
can complete docking. No claim of new gameplay pass yet.

## 2026-10-23 — Axial-zero audit and mesh coordinate conventions (SOURCE, WINDOWS GATE OPEN)

**Request:** review every meaningful flight/navigation prerequisite that equates
"flyable" with nonzero motor acceleration along hull local -Z. DO NOT
replace geometric Z coordinates, pitch/yaw body bases, signed motor-wrench
telemetry, or Newtonian rear-main conventions with a new "thrust axis" position
coordinate. They have different meanings.

**Audited active flow:** Cobra installed engine slots and `ShipCore` real
motor allocator -> `ShipCapabilityBuilder` geometry/CoM working axis ->
`ShipControlAuthorityTelemetry` signed acceleration matrix ->
`MotherComputer` flight doctrine and envelope ->
`SpaceState::updateDockingAdvisory` admission ->
`ManeuverCapabilityAdapters` / trajectory programmer ->
`AcceptedManeuverProgramBuilder` feasibility ->
`DedicatedClientRouteAutopilot` response horizon ->
`AssistedAutopilot` -> `ShipController` -> runtime actuator executor.
Examined Newtonian primary-main selector, physical-maneuver compiler,
route geometry and safe-course separately to avoid incorrectly replacing
body axes with course axes. Source results:
- `b9568691`: real six-axis actuator telemetry now ALWAYS publishes
  `workingForwardAccelerationMps2`: healthy symmetric bank gets
  `negative[2]`; off-axis case gets selected CoM-aligned motor's attainable
  acceleration. `applyAuthorityToControlParams` ALWAYS uses working-forward
  magnitude to set Assisted forward availability/rating, irrespective of
  potentially delayed emergency status. `negative[2]` is retained as
  distinct pure hull -Z *diagnostic* or wrench basis; it is not necessarily
  physically useful course authority.
- `6a02c239`: the docking pre-planner gate now ALWAYS checks actual
  `workingForwardAccelerationMps2`, no mode-conditional pure -Z.
  It still requires bidirectional lateral, pitch/yaw and reverse basis.
  Those are separate feasibility conditions: full engine-out still might be
  rejected as a 3D docking craft even when working-forward thrust exists,
  and those constraints must be evaluated against reachable manoeuvres;
  do NOT remove or fake them merely to make a test pass. The exact failing
  magnitudes are logged via `DockClient::fail`.
- `d593011f`: `ManeuverCapabilityAdapters::makeManeuverExecutionAuthority`
  no longer grants a fictitious 0.1 m/s² to forward, reverse or lateral
  Assisted when physically absent; minimum is 0.0, so validation correctly
  reports impossible movement rather than adopting a fake manoeuvre.
- `3b851ddc`: the active Assisted route follower's extra off-axis
  angular alignment allowance uses actual body working-axis geometry,
  not solely a damage boolean; centered healthy behavior remains unchanged.
  An invalid working axis has no healthy-nose fallback.
- `62886924`, `2eb43012` extend EXISTING MotherComputer tests:
  healthy basis provides a working-forward component; missing pure -Z
  with a valid working ray preserves control authority even when status
  boolean lags; nonzero -Z with zero actual working authority grants none.
  `c0f1d38a` and `08c5ab6d` align physics field comments with this
  established separation (working-forward vector vs body -Z).
  SOURCE commits only; tests not compiled in this environment.
- A **remaining deliberate boundary**: direction-tagged sample
  `forwardMap` in `AcceptedManeuverProgramBuilder`, follower and
  trajectory planner is a WORLD/ROUTE tangent, not the ship's body -Z.
  Its acceleration projections are necessary and should remain. Newtonian
  `choosePrimaryMainBurn` still steers a physically rear main using
  an explicit hull turnaround model; this is a distinct flight law.
  No blind global substitute `z -> workingForward` is permitted.
- **Potential further requirement beyond this audit:** true loss of
  lateral or reverse *pure-body* basis does not alone prove a curved route
  impossible if the craft can reorient an angled main and execute
  sequential burns. That is a reachable-set/time-feasibility question,
  distinct from the now-correct basic working-forward admission. Must
  read new authority log before changing planner/reachability assumptions.

**Ship body coordinates remain a stable, immutable convention:**
`+X right, +Y up, -Z geometric nose, +Z stern`. Body coordinates
describe slot positions, mesh dimensions, inertia and torques; they never
need to swivel with a damaged propulsion axis. The working thrust ray is
a **dynamic vector expressed in that fixed body frame**, transformed
through the current ship attitude into world/navigation coordinates.
E.g. healthy `(0,0,-1)`; emergency `(-0.760494,0,-0.649345)`.
No new coordinate system or alternate ship "front" is created.

**Authoring/import:** `IObjectDescriptor::meshToLogicalBasis4()`
already converts the object's authored `meshForwardAxis` and
`meshUpAxis` into the canonical logical body frame. The
`AssemblyMeshLibrary` applies that matrix to OBJ mesh vertices,
module positions, pivots and rotation axes. Other ecosystems are
different: Blender mesh default Z-up and a common art-facing -Y
front, whereas this Cobra OBJ specifically has authored
`meshForwardAxis=(-1,0,0)` and `meshUpAxis=(0,1,0)`; its actual
right is -Z. `1bee56d6` corrected a contradictory comment
in `EliteCobraMk1.cpp`; NO visual/physics basis changed.
`visualBasisRotationDegValue` remains a legacy fallback and
`AssemblyMeshLibrary` currently copies authored
`module.localRotationDeg` without basis conversion: verify the
full asset-animation path for future new OBJ model orientations
before shipping them. Keep physical mounts and visual orientation
consistent through their intended authored/logical binding, not
by changing the world flight law.

**Verification:** all listed modifications are SOURCE ONLY. The last
uploaded native run was pre-audit `71bf09e8e001`, with one 50%
successful route and one 100% pre-planner blocked request.
Run a fresh MinGW64 client+server build, existing `mother_computer`
and navigation tests, then the existing single-case
`-AssistedIOnly -OnlyPreset aft-failed` flight. Look for
`mother_current`, `mother_doctrine_valid`,
`mother_axis_dot`, `mother_working_forward`, separately
`mother_hull_forward`, and `mother_lateral/steering/reverse`
to identify any *remaining* true gate. Do not claim pass yet.

## TOP PRIORITY — 2026-10-23 evening: damaged-I docking never starts planning (LOG-VERIFIED; SOURCE PATCH UNVERIFIED)

**Fresh target Windows evidence from build `71bf09e8e001@main/clean`:**
User sent `20261023-1820-25.3876699.mp4`, two per-scenario
`*_attitude(2).csv`, two `*_receipt(2).log`, two nav stress `(2).log`,
and `assisted_damage_flights_summary(2).csv`. Both failure injections are
`status=APPLIED` on real installed `ship_engine_L`.

- `aft-50`: approximately 82.2 simulated seconds, stress
  `COMPLETE/exit=0`. Planner returned `job_ready=1`,
  `job_plan_valid=1` (twice); an accepted 12-curve
  `route_valid=1`, `phase=4` executed through
  `BLUE_ENTER` and `BLUE_EXIT`. The planner/follower infrastructure
  works in the reduced-but-balanced configuration.
- `aft-failed`: approximately 137.16 simulated seconds, stress
  `GAME_EXIT/exit=3`; **every** sampled `phase=0`, `autopilot=0`,
  `workers=0`, `job_ready=0`, `route_valid=0`, `failed=0`, while
  `pending=1,serial=1`. This is a **PRE-PLANNER admission/authority
  wait**, not evidence of an invalid planner result or a follower-rejected
  route. In particular, a terminal `fail(...)` from the 3D motor-basis
  guard would have set `failed=1`, which is NOT observed. Source
  `SpaceState::updateDockingAdvisory` has an early `return`
  when `MotherComputer::assistedFlightEnvelope` is not ready/valid
  and only prints an `awaiting authoritative motor telemetry` note to
  stderr. The existing stress file omitted `m_dockingStatusWarning`
  and MotherComputer's specific gate facts, so the *precise* refusal cause
  is not yet measured.
- Physical `aft-failed` CSV still confirms
  `axis_emergency=1`, direction
  `(-0.76049403,0,-0.64934492)`, offset `49.50776982°`,
  left motor thrust 0, right motor active. Before request admission
  the fully damaged ship already produces large yaw torque and
  angular velocity despite `main_cmd=0`; this is an independent
  physical/control instability to investigate AFTER unblocking/identifying
  the entrance guard. Do not attribute it to planner or invent thrust.

**Confirmed additional SOURCE defect (not yet observed as direct second-run
failure):** after the MotherComputer authority gate, `SpaceState.cpp`
performed a 3D feasibility check with
`forward=liveAssistedAuthority.negative[2]`, which is **pure hull -Z**
and can be zero under a physical offset axis. It ignored the existing
`workingForwardAccelerationMps2` from the same signed physical matrix.
Commit `146b8fd` now uses the work-axis acceleration for emergency mode
and retains pure -Z for healthy. It still checks real lateral,
steering, and reverse authority (NO artificial authority / bypass).
A rejection now logs the exact signed magnitudes into the normal
`fail()` reason.

**Targeted missing instrumentation on main:** `bac6a937` and
`062e0fb5` extend the existing `NavigationStressHarness` observation
and `SpaceState` reporting, **not** another logger/test or an alternate
state source. Existing `SAMPLE/EVENT` lines now contain
`authority_status`, `mother_current`, `mother_envelope`,
`mother_motors`, `mother_doctrine_valid`,
`mother_doctrine`, `mother_server_axis`,
`mother_client_axis`, `mother_hull_forward`,
`mother_working_forward`, `mother_steering`,
`mother_lateral`, `mother_reverse`.
`5c4d360b` and `28c5326c` additionally expose
`mother_model_match` and `mother_axis_dot` (server vs client
unit-vector dot product, -2 means unavailable); this distinguishes an
authority-model mismatch from an otherwise valid signed motor basis
whose selected doctrine is rejected by mismatched physical axes.
This makes a pending/idle/failed=0 admission wait identifiable;
the worker result is logged by the existing `job_result` only
after it has actually run.
**No C++ build or game test after these source edits is available yet.**

**Immediate next action:** Windows pull/build BOTH game/server, then
use the **existing** runner's new `-OnlyPreset aft-failed` selector
(commit `0e2b6aa5`) with `-AssistedIOnly` to run just the broken case.
Omitting `-OnlyPreset` keeps the original two/six-case matrix.

```bash
cd /d/__elite/work
git pull --ff-only origin main
bash build_mingw64.sh
MSYS2_ARG_CONV_EXCL='*' powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$PWD/scripts/run-assisted-engine-damage-flights.ps1")" -AssistedIOnly -OnlyPreset aft-failed -ReportDirectory 'D:\\tmp\\elite_engine_out_gate'
```

The required deliverable for this one-case run is the NEW
`D:/tmp/elite_engine_out_gate/01_Assisted_I_aft_failed.log`,
plus its summary and receipt; numbering is 01 when only one case runs.
Determine whether `mother_current=0`, envelope/motor missing, doctrine
invalid from disagreement, or 3D actuator-basis deficit. If the gate
becomes ready, check actual `workers`, `job_result`,
`route_valid` and `phase` before altering planner math. Do NOT
reinterpret `job_stage=-1` as a planner failure. Keep healthy Y/X
smoothing and the installed-engine physics unchanged until supported
by more flight evidence.

## 0. 2026-10-23 — Explicit construction invariants and universal working-axis steering (SOURCE)

**User clarification:** A real installed CR-1/ER-5 cannot "lose its ID" as equipment
damage. The selected I/Y/X assembly has fixed slot-to-module identity, valid
installed equipment passports and a separately mutable runtime health record.
An empty/duplicate engine or reactor ID, absent module definition, or wrong
binding count means **invalid ship construction**, not a damaged motor. The
earlier fail-closed zero-thrust fallback protected against fabricated healthy
motors but was not a sufficient assembly-invariant check.

**Changes on main:**
- `574ddd51` validates ALL I/Y/X installed Cobra engine and reactor
  bindings at profile creation: correct slot/group counts, nonempty unique IDs,
  actual CR-1/ER-5 module passports. Violations raise a direct construction
  error before such a profile is activated. `8f68f0df` then reuses this same
  already-declared topology (avoids a duplicate local declaration).
- `38ac7751` rejects a partially populated installed actuator binding
  entirely in both runtime and replicated capability projections. No fictitious
  healthy motor AND no pretending that an invalid construction is ordinary
  engine damage. Entirely binding-free explicit isolated diagnostics remain
  separate legacy behavior, not a production Cobra installation.
- `0712a3fa` makes AssistedAutopilot always transform
  `assistedWorkingThrustAxisBody` from ship body to world; the offset
  orientation branch depends on vector geometry, **not** a damage flag.
  Healthy symmetric hardware supplies body `(0,0,-1)` and retains the
  existing nominal attitude controller. `7cf1efa4` applies the same
  universal vector rule to manual HOME/INSERT/END alignment in
  `ShipController`. The already corrected safe-course executor used the
  physical working-axis body vector unconditionally.
  `cca73c68` and `540175a5` also prevent invalid/zero/NaN
  Assisted working-axis vectors from silently becoming the healthy
  hull-nose direction in normal autopilot or safe-course entry.
- Existing `EngineThrustHudPresentationTests`, `HullPoseGuidanceTests`
  and `ShipControlBehaviorOracleTests` now directly exercise only these
  failure modes (invalid binding, off-axis vector even with lagging emergency
  flag); no extra testing system, engine fake or new contract.

**Geometry and its limits:** The 49.50776982° offset is NOT a constant.
`ShipCapabilityBuilder::applyMountedEngineAxialAuthority` uses each motor's
normalized installation coordinate * the physical half-extents in metres
minus the current ship CoM. `analyzeWorkingThrustGeometry` checks the
feasible balanced centered axis, otherwise selects a surviving motor's
torque-neutral force direction `-arm/length(arm)` inside its gimbal cone
and computes `acos(-force.z)`. For surviving right aft I: arm
`(+13,0,+11.1)` m, hence force `(-0.760494,0,-0.649345)` and
`atan2(13,11.1)=49.5078°`. Different installed slot positions, logical
physical extents or CoM produce another result **when those physical
parameters are updated**. Merely changing the drawn 3D mesh without
updating physical installation geometry does not change physics.

**Critical semantic boundary:** This is the *feasible reference forward
working thrust ray* used for torque-neutral drive/course alignment, not
a continuously sampled instantaneous vector of `sum(executed motor forces)`
during steering, reverse burn and attitude correction. Those can differ in a
maneuver; the amber HUD currently depicts the working reference. Do not
mislabel it as the instantaneous total-force vector.

**Verification:** All above are SOURCE / GitHub changes only. A post-write
source scan caught and removed a duplicate local declaration in the construction
validator before final handoff; this does NOT substitute for native compilation.
 The agent cannot
compile the Windows MinGW client/server or observe a new flight in this
environment. Do not claim acceptance before the focused native tests and
50%/100% Assisted-I flights. The accepted smooth healthy Assisted regulator
gains and navigation route geometry were NOT retuned.

## 1. Immediate task and acceptance criteria

Continue fixing the Assisted ship's engineering HUD and engine-out control.
**ONE mutable truth** for installed equipment is server
`ShipCore::ObjectModuleRuntime` (module identity, attachment, damage,
functional availability, thermal and operating state). The server derives
real motor authority and forces from it. Replication transports facts;
`MotherComputer` reads this one equipment graph and physical authority;
autopilot requests **ordinary pilot controls only**, never arbitrary
engine forces; HUD presents the same facts and no independently invented
healthy motors.

The forward *working thrust axis* is the real physically feasible total-force
direction through the current centre of mass (when torque-neutral). It is
NOT axiomatically the ship's body `-Z` nose direction. Healthy symmetric
Assisted may use `-Z` as the special case. A failed offset bank must use the
physically selected body-space vector in its plant, capabilities, flight
doctrine, safe-course entry and moving automatic attitude guidance. Do not
rotate the geometric route 49 degrees: rotate the HULL so real working thrust
aligns with the requested course. At zero thrust / terminal hold the desired
hull pose can be a separate docking reference.

The cockpit always retains its fixed cyan hull boresight. An amber
`THRUST AXIS` sight must identify **any meaningful off-nose working thrust
direction**, including an off-screen direction (edge arrow + angular offset).
If the true working axis coincides with the nose, no separate offset cue is
needed. The sight's direction belongs to the server-equipment/MotherComputer
tick, not to the interpolated render pose or a synthetic descriptor. CHECK
and each engine's yellow available/red delivered/grey unavailable bars must
agree with actual installed equipment including internal functional failure.

DO NOT retune accepted smooth Assisted I/Y/X regulators or rewrite planner
curves merely to conceal an installed-motor, replication, BITE or HUD defect.

## 2. Observed baseline — separate fact from hypothesis

**GAMEPLAY, 2026-10-22:** user reported former high-frequency pitch/yaw
chatter fixed, Assisted-I/Y/X now moving smoothly in normal operation.
Source path: `HullAttitudeControl.h` damped pitch/yaw and
`AssistedAutopilot.h` uses 0.50 s response. Prior 90.02 s Assisted-I trace
corroborated this; does not prove damaged-engine stability, Newtonian or
successful docking.

**GAMEPLAY/LOGS, 2026-10-23, BEFORE latest fixes:** two Assisted-I
preset flights, `aft-50` and `aft-failed`, injected functional damage into
the real installed `ship_engine_L` before route planning. Server physical
samples show:
- 50% case: motor 0 capacity at most 0.50; flight runner exit code 0.
- 100% case: motor 0 delivered thrust zero throughout, motor 1 continues
  delivering thrust; runner exit code **3**, so emergency route admission /
  recovery / corridor result is NOT accepted.
- Engine-out server reports `axis_emergency=1`,
  `axis_body=(-0.76049403,0,-0.64934492)`,
  `axis_offset_deg=49.50776982`, `axis_motor_index=1`.
  Body geometry X half extent 13 m, Z half extent 11.1 m.
- Nevertheless the user's HUD showed normal capacity, CHECK remained dark,
  and amber offset cue did NOT appear. Server actuator physics success does
  NOT establish that module records reached the remote client.

**Last failing client evidence:** user video
`20261023-1255-37.3977633.mp4`, 50% / failed per-run flight CSV and
summary. These were built at `fbfa722fc9e7` **before** the binding fix.
Do not claim that video tests the later corrections.

## 3. Audited implemented architecture (source on main)

- `ShipCore` owns `ShipState` (derived control/actuator state) and
  `ObjectModuleRuntime` (authoritative writable installed equipment).
  `injectEquipmentFailure` changes functional internal damage, preserves
  all other damage histories, bumps module revision. The server uses real
  mounted CR-1 capabilities to execute the physical wrench/engine torque.
- Installed Cobra Assisted configurations I (4 CR-1 / 2 ER-5),
  Y (6 CR-1 / 3 ER-5), X (8 CR-1 / 4 ER-5) have distinct persistent
  module identities. Ctrl+F9 switches the active installation without
  healing inactive damaged engines. Newtonian retains its distinct 8-node,
  24-nozzle RCS architecture; no synthetic Assisted fallback may stand in
  for installed hardware.
- `GameSimulation::buildReplicationSnapshot` -> per-module
  `ObjectModuleSnapshot` including `internalFailureDamage01` ->
  `ClientWorldState` `ship.modules` -> `collectReplicatedShipFacts` ->
  `MotherComputer` -> cockpit, CHECK, engine bars and Assisted authority.
  Compact module rows are now sent on module revision changes also to
  motion-lab probes; the larger structural/repair graph remains omitted.
- `EliteCobraMk1.cpp` `ELITE_SHIP_PHYSICAL_TEST_MODEL=Assisted-I/Y/X` now
  binds `installedAssistedEngineModuleIds(model)` rather than calling
  `slotModuleIds.clear()`. This fixed a **confirmed source cause**:
  a client descriptor with an empty binding was silently reconstructed as
  four healthy test motors despite damaged server modules (commit
  `dd8702c`). Not yet verified in new Windows gameplay.
- `ReplicatedInstalledAssistedDescriptor.h` selects actual installed
  equipment passports; `ShipCapabilityBuilder` derives their available
  forces and torque-neutral working thrust ray;
  `WorkingThrustAxisAnalysis.h` selects a valid COM ray from physically
  viable aft motors when central `-Z` is infeasible.
- `FlightControlProfile.h` / `MotherComputer.h` select the damage-offset
  doctrine from physical capability and measured motor authority, rejecting
  incompatible server/client references rather than pretending the nose
  is a healthy forward-axis fallback.
- `AssistedAutopilot.h` **already** uses working-axis-to-course pitch/yaw
  guidance during engine-out; it no longer adds a contradictory nose-to-course
  goal. `AssistedSafeCourseExecutor.h` **already** receives
  `effectivePhysics.assistedWorkingThrustAxisBody` from `SpaceState` and
  aligns the working ray, not the nose. The prior state text saying safe-course
  still uses nose unconditionally is OBSOLETE relative to current source.
- `collectCockpitView` selects a tick-matched MotherComputer working-axis
  vector; `SpaceState` passes it to `FlightVectorIndicatorRenderer`,
  which has an amber offscreen-direction sight. Normal cyan boresight
  stays separate. HUD CHECK/bars derive selected installation and BITE,
  with observed live capability capped by installed damage projection.

## 4. This 2026-10-23 source-audit slice, published on main

1. Historical `f0d2a192` replaced an invented healthy motor with
   zero authority; this behavior was **superseded by `38ac7751`**:
   partially empty installed bindings now reject the entire invalid
   configuration. Entirely binding-free isolated diagnostic topology
   is a separate explicit case, never normal installed Cobra hardware.
2. `fd39c175` — `MotherComputerClientAdapter.h`: the HUD now treats the
   replicated physical working-axis *vector* itself as evidence of an off-nose
   sight when the emergency boolean lags; validates a finite, nonzero vector.
   This is DISPLAY ONLY, not another engine state or an actuator override.
   If no server offset vector exists, the existing installed-capability
   projection remains available. Physical/autopilot laws unchanged.
3. `876ac36c` — expanded the **existing**
   `EngineThrustHudPresentationTests.cpp` to require that a blank single
   installed ID never becomes healthy and that the off-axis server vector
   remains visible even if the separate emergency flag is false. No new
   testing infrastructure or test-only production paths.
4. `d7b792ed` — `ShipController.cpp`: the legacy velocity-alignment
   path (Assisted HOME/return from free rotation and damaged
   flip-and-burn END) was STILL aligning the bare hull nose to velocity,
   independently of the already corrected navigation autopilot and
   safe-course executor. It now compares the actual body-to-world working
   thrust ray to velocity or anti-velocity, leaving physical body control
   axes and healthy/Newtonian behavior intact. Invalid off-axis reference
   fails closed instead of reverting to a fictional nominal nose direction.
   This is SOURCE ONLY, not accepted gameplay.
5. `9c55d026` — extended existing `ShipControlBehaviorOracleTests.cpp`:
   nose on velocity with 49.5-degree off-axis thrust must request yaw;
   counter-rotated hull with thrust on velocity must finish alignment;
   nominal nose-on-velocity behavior stays intact.
6. `2d6b9e3b`, `c7500d1a` — fixed `AGENTS.md` and
   `REPOSITORY_SOURCE_OF_TRUTH.md` instructions that previously told
   agents to maintain three parallel living status files. Only
   `PROJECT_STATE.md` now owns current project status; normative architecture
   documents remain independently authoritative on their actual contracts.

**VERIFICATION LEVEL: SOURCE REVIEW ONLY** for this slice (including
manual alignment correction) and previous `dd8702c` diagnostic-descriptor fix. GitHub writes succeeded; no MinGW
compilation, CTest, native Windows binary or new in-game video/trace has
confirmed the changed behavior. Do not mark engine-out or HUD accepted.

## 5. Next focused verification — no scope expansion

On user's Windows MinGW64 checkout, pull the current `main`, rebuild
**client and server**, and run focused existing tests first:

```bash
git pull --ff-only origin main
bash build_mingw64.sh
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_runtime --target engine_thrust_hud_presentation_tests mother_computer_tests working_thrust_axis_analysis_tests assisted_safe_course_executor_isolation_tests hull_pose_guidance_tests ship_control_behavior_oracle_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R "^(engine_thrust_hud_presentation|mother_computer|working_thrust_axis_analysis|assisted_safe_course_executor_isolation|hull_pose_guidance|ship_control_behavior_oracle)$" --output-on-failure
```

The test directory above is explicitly configured first, then only the six relevant existing targets are built and run.
Then rerun ONLY the existing two Assisted-I game-level scenarios:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-assisted-engine-damage-flights.ps1 -AssistedIOnly -ReportDirectory 'D:\tmp\elite_assisted_I_2runs'
```

Keep each run's original receipts, navigation stress report, video/screenshot
and separate attitude CSV. Especially inspect:
`D:/tmp/elite_equipment_state_aft-50.csv`,
`D:/tmp/elite_equipment_state_aft-failed.csv`, per-run
`*_attitude.csv` and `D:/tmp/elite_engine_damage_preset.log`.

Expected client observations (NOT yet confirmed):
- 50% `ship_engine_L`: replicated `internal_failure=0.5`,
  projected motor 0 <=0.5, CHECK/partial yellow bar; central thrust axis
  can remain feasible by balancing surviving aft motors.
- 100%: `internal_failure=1.0`, projected motor 0=0, zero red/yellow
  bar on neutral-grey track, CHECK, server and client offset vector
  agree within 49.5° tolerance and amber THRUST AXIS direction is visible
  either onsite or at screen edge.
- Compare `graph module_count`, module presence and active model with
  physical envelope and cockpit. If modules have not replicated, fix ONLY
  the server->client graph edge. If they have but bar/CHECK lies, fix
  ONLY MotherComputer/HUD consumption. Never create a healthy fallback.
- Independently check Assisted HOME and damaged END/flip-and-burn:
  the actual world-space working thrust axis must align with velocity
  (or anti-velocity for aft-main braking), not the hull nose.
- If HUD is finally correct but full-out flight still fails, use per-run
  acquired/route/admission data and yaw/torque/nozzle logs to isolate the
  actual physical feasibility, control allocation or safe-course phase.
  A ready route planner worker is not evidence that corridor execution
  was entered. Do not retune the healthy Assisted controller blindly.

## 6. Longer-lived constraints / deliberately open questions

- Autopilot is a virtual pilot issuing the same ordinary controls as manual;
  physics/server selects actual engines, force, moment, power and heat.
  Planner must use available acceleration and deceleration, not nominal wishes.
- Ordinary Assisted: motion/velocity tends toward the commanded nose/course;
  its smooth tracking was gameplay accepted. Newtonian: main aft thrust,
  turn-around braking; stabilization of fine RCS inputs is separate.
- Route remains exact planner geometry; corridor is visible in manual/auto.
  Fly with attainable lead turns, brake before infeasible corners, do not
  invent acceleration. Safe-course entry and terminal docking have distinct
  execution phases.
- Generic engine/damage state must cover partial or total loss, no healing by
  changing I/Y/X layouts, real thermal/electrical limitations and actual
  actuator/nozzle execution. A full 6DOF combined controllability certificate
  and time-bounded thermal/power feasibility are still NOT implemented;
  do not claim they are verified.
- Full engine-out corridor and docking acceptance remains OPEN despite
  server's physically correct torque-neutral 49.5° axis. Newtonian and
  other HUD issues should not obscure this precise current gate.

**Resume point:** first obtain one fresh Windows 50%/100% I test after the
actual `main` updates; compare server installed module vs client module,
MotherComputer capability/BITE and rendered sight, then correct the
first failed edge only. Continue recording findings HERE, not in parallel
status or task files.
