## Continue from 2026-10-03 Assisted scalar speed fix

Current main fixes the portal stall: applyNavigationAssistedFlightModel no longer projects Follower target velocity onto the current hull to derive speed. Assisted scalar speed is now the target-vector magnitude; the current hull nose owns travel direction and the attitude loop owns steering. This prevents sharp look-ahead turns from zeroing throttle. New runtime regression covers a 120-degree off-nose target while requiring preserved scalar speed and main thrust. Next evidence: runtime_control + tracking_controller + composite rerun.

## Continue from 2026-10-03 low-speed bypass START correction

Current main: production Assisted nose-coupling fix is holding (earlier phases show zero prolonged slip). Remaining composite failure was a low-speed TimeScheduled continuation authored from v0=0,a0=0 after a full STOP. fitAuthorityBoundedReplacement now uses explicit moving START v0=0.25 m/s along actual hull forward, with a0=0, and validates that exact curve. Continuation logs expose component envelope maxima. Next evidence: runtime_control + tracking_controller + composite rerun.

## Continue from 2026-10-03 Assisted nose-coupling fix

Current main removes the production bug where autopilot lateral target velocity established a second sideways Assisted equilibrium. VREL direction is again coupled to the actual current hull nose; lateral feed-forward can only assist an existing alignment correction and cannot create/oppose slip. New runtime-control regressions enforce this, and composite logs slip duration per phase. Next evidence: runtime_control + tracking_controller + composite Windows rerun; then full suite if green.

## Continue from 2026-10-02 Assisted lag-duration metric

Current main: Newtonian composite is end-to-end green. Assisted physically completes portal and final capture with zero tracking-envelope violations. The obsolete final rule maxSlipDeg<=8 was replaced by a temporal contract: with speed >0.25 m/s, continuous nose/VREL slip above 8 deg must clear within 3 seconds. Peak angle remains diagnostic only. Next evidence: tracking-controller + composite rerun, then full navigation-runtime suite if green.

## Continue from 2026-10-02 Assisted VREL-course correction

Latest Windows run proves Newtonian composite end-to-end green. Assisted physically traverses the portal and final capture but self-invalidated because route-loss compared hull heading to the local tangent while Follower intentionally aimed the hull toward a bend look-ahead point. Current main now measures Assisted SpatialCorridor course from actual VREL vs tangent; attitude steering remains independent. Two regressions cover steering lead vs genuine VREL departure. Next evidence: tracking-controller + composite rerun.

## Continue from 2026-10-02 tunnel-start tangent correction

Current main now enforces the user's original geometry rule: the constrained tunnel starts along the ship's actual current forward direction. The previous bug came from STOP leaving start.velocity=0, after which makeCurve had no heading derivative and authored the first segment directly toward the portal; post-hoc sample[0] velocity rewriting plus Newtonian FixedStart made hull and tunnel diverge. Zero-speed spatial samples are now STOP only; moving START is explicitly v>0,a_ff=0 along start.basis.forward; constrained Newtonian orientation follows the curve.

Next evidence: maneuver_tracking_controller + navigation_composite_proving_ground Windows rerun.

## Continue from 2026-10-02 STOP/START navigation contract

Current main implements the user's requested semantics: authored v=0 is a real STOP completed by physical RCS-only trim when residual speed is small; moving SpatialCorridor START has v>0 with a_ff=0. The precision semantic is wired through production GameSimulation, not only the composite fixture. New regressions cover actuator choice and STOP-vs-START classification. Next evidence required is a four-test Windows build/run (runtime_control, tracking_controller, phase_gate, composite).

## Continue from 2026-09-29 spatial validity-window correction

Current main: SpatialCorridor phase handoff is physical, Newtonian hull slip is not route loss, and the composite harness now executes spatial programs until AcceptedManeuverProgram::validUntilUniverseTimeSeconds rather than an obsolete nominalEnd+5.1 synthetic deadline. New portal diagnostics expose final P/V and actual execution budget.

Next required evidence: composite-only Windows rerun. Then run the two unit regressions if composite is green.

## Continue from 2026-09-28 production spatial-contract correction

Current main fixes two production issues found by composite: SpatialCorridor no longer advances by nominal ScheduledMoving time, and NewtonianMainEngine hull/course slip no longer counts as geometric corridor loss by itself. Regression tests were added for both. Next evidence required: maneuver_phase_gate + maneuver_tracking_controller, then composite.

Read BUILD_TEST_LAYOUT.md before issuing commands.

## Continue from 2026-09-28 SpatialCorridor portal correction

Latest target evidence: Newtonian composite is fully good; Assisted keeps geometry and dynamic clearance safe but had 81 tracking-envelope ticks because portal_102 was still TimeScheduled. Current main sets that constrained segment to SpatialCorridor and maintains the monotonic spatial cursor exactly as the production RouteFollower contract expects. Final StateCapture remains TimeScheduled.

Next required evidence: composite-only Windows rerun. Read BUILD_TEST_LAYOUT.md before giving commands.

## Continue from 2026-09-28 full accepted-segment dynamic proof

Composite root cause: 4 s local planner horizon was incorrectly treated as authority to execute 16 s/18 s accepted programs while a moving hazard remained authoritative. Current main now proves the entire portal and final-capture program against the hazard before execution; unsafe future occupancy causes hold + re-author + re-proof, never tunnel widening or hazard suppression.

Next evidence required: composite-only Windows rerun. Read BUILD_TEST_LAYOUT.md before issuing commands.

## Continue from 2026-09-28 composite-only rerun

Latest target evidence: navigation_runtime_planner, maneuver_corner_family_matrix, maneuver_rigid_body_corridor, maneuver_chained_limit_matrix, and physical_maneuver_search_coordinator pass. Only navigation_composite_proving_ground remains pending.

The latest fix does not widen the 19 m corridor or disable the moving hazard. It settles residual entry velocity before the constrained portal and routes Assisted active braking through the same velocity-target flight model used by production.

Read BUILD_TEST_LAYOUT.md before changing commands.

## Continue from 2026-09-28 six-test runtime audit

Latest Windows full runtime suite: 23/29 passed; six failures were audited. Docking-specific tests remained green. Candidate fixes are on main and need Windows rerun.

Important findings:
- several old simulation labs bypassed the current production Assisted path by using `NavigationRuntimeControlBridge::step` plus raw `applySystemAccelerationDemand`; they now use `stepVehicle` and `applyNavigationAssistedFlightModel`;
- Assisted comparison fixtures now carry the real Cobra fore/reverse main engine; Newtonian comparison fixtures do not;
- PhysicalManeuverSearchCoordinator's old "Assisted unsupported" expectation was obsolete;
- the chained seam threshold of 1e-6 degrees was below meaningful float-orientation precision and is now 0.01 degrees;
- composite narrow corridor remains 19 m; the test slows traversal rather than widening it.

Read `BUILD_TEST_LAYOUT.md` before issuing commands. Next evidence is a targeted rerun of the six failing CTest names, then full runtime suite if green.

## Continue from 2026-09-28 verified build-layout correction

Read root `BUILD_TEST_LAYOUT.md` before giving any build/test/run command.
For every non-trivial command, verify the current owning `CMakeLists.txt`, exact build target, build tree, and CTest name. Prefer existing canonical scripts when they already implement the gate.

Current build ownership:
- root `build/` owns `EliteGame`;
- standalone navigation-runtime targets live in `build/tests/navigation_runtime/` and are declared by `tests/navigation_runtime/CMakeLists.txt`;
- do not run `cmake --build build --target docking_advisory_tests` or similar standalone-test targets.

Latest code state:
- Automatic is stop-and-settle before planning;
- Planner carries `initialSpeedMps` and terminal radius is speed-aware instead of using `0.8 * maxSpeed`;
- target dock HitVolumes use semantic dock clearance;
- runtime marker is `impl=dock-auto-20260928-stop-speed-aware-turn-semantic-clearance`;
- Windows full navigation-runtime build exposed one stale 3-argument `TrajectoryFollower::follow` in `NavigationCompositeProvingGroundTests.cpp`; it is fixed on main, and all 30 navigation-runtime test sources were checked for the same stale call pattern.

Next step is Windows rebuild/test evidence, not more route tuning.

## Continue from 2026-09-28 moving-start/HOLD fixes

A pre-latest live log exposed and the code now addresses:
- ApproachHold incorrectly braking a moving ship before planning;
- Assisted route planned exactly on the AcceptedProgram acceleration boundary;
- false ~180-degree course loss at a zero-speed HOLD reference;
- recovery replanning a tiny reverse Assisted segment instead of entering
  FinalIngress.

Current contracts:
- ApproachHold = coast with zero translational acceleration + angular damping;
- planning origin = measured P + measured V * planning lead;
- FinalIngress = stop + rotate/align + fresh short plan;
- HOLD capture = physical stopping distance inside accepted envelope;
- zero-speed SpatialCorridor reference has no route-loss course;
- planner uses 90% of reserve-reduced acceleration envelope.

Next action is Windows `verify_docking.sh`, `EliteGame` build, then one live
run starting START DOCKING while already moving. Do not tune corridor geometry
unless that evidence specifically shows a geometry failure.

## Continue from 2026-09-28 first hard module split

A first concrete Planner/Autopilot/Traffic/Landing separation is on `main`.

New public seams:
- `src/game/navigation/planner/RoutePlannerApi.h`;
- `src/game/navigation/autopilot/RouteFollowerApi.h`.

New docking domain:
- `src/game/docking/model/DockFacilityDescriptor.h`;
- `src/game/docking/traffic/DockTrafficControllerApi.h/.cpp`;
- `src/game/docking/landing/DockLandingControllerApi.h/.cpp`.

Do not restore direct GameServer/SpaceState dependencies on
`DockingAdvisoryPlanner`, `TrajectoryFollower`,
`ManeuverTrackingController`, `ManeuverProgramSampler` or
`ManeuverProgramTimeline`.

The old algorithms remain private adapter backends for now because the user's
current SpatialCorridor flight is successful and must not be rewritten during
an ownership refactor.

Before wiring live queue/landing behavior, obtain Windows evidence:
`bash verify_docking.sh`, then build standalone `EliteGame`.

If that gate passes, next slice is authoritative DockTrafficController
integration only: remote provisional inquiry -> controlled-horizon queue ->
EntryHoldPoint/portal/pad clearance. LandingController remains a later,
separate handoff.

## Continue from 2026-09-28 docking infrastructure architecture decision

The user accepted the long SpatialCorridor behavior as very good. Preserve it.
Do not broadly retune Planner/Follower while refactoring ownership.

Read `src/game/docking/DOCKING_INFRASTRUCTURE_ARCHITECTURE.md` before the
next docking implementation.

Next work is architectural separation, not more monolithic FinalIngress code:

- generic RoutePlanner in its own files/module;
- generic RouteFollower/Autopilot in its own files/module;
- DockTrafficController in separate docking/traffic files;
- DockLandingController in separate docking/landing files;
- DockFacility semantic model separate from art mesh.

Enforce narrow public DTO/API headers and add architecture-contract tests for
forbidden cross-module includes/private state leakage.

Traffic policy:
- remote request does not reserve pad/portal/queue;
- committed queue begins only in a controlled ETA/distance horizon;
- one physical EntryHoldPoint per portal/final-approach lane initially;
- dispatcher queue is virtual; other ships wait outside the lane at safe planned
  staging stops;
- physical fit is mandatory;
- class policy may emergency-upgrade to a larger compatible dock only;
- smaller dock is never allowed;
- arrival and departure both reserve shared conflict resources.

Landing begins only at LandingHandoff and is not RouteFollower logic. It owns
RCS/manoeuvring-thruster pad alignment, gear, settle, contact, latch and later
undock handoff.

## Continue from 2026-09-28 live corridor success / dock ingress candidate

The user reports the long automatic corridor is now excellent: corridor held,
turns smooth, and dock-relative up/down orientation maintained. Do not retune
that successful behavior broadly.

Current candidate changes:
- small center deadband holds exact segment tangent to reduce straight-line
  hunting without changing route speed;
- guidance docks no longer use a solid whole-object hit box; they author four
  wall OBBs around the visible aperture;
- docking anchors load explicit `capture_depth_m`;
- FinalIngress now goes through the entrance to the internal capture point and
  must pass exact obstacle clearance;
- success reason is `capture-envelope-complete`;
- permanent latch/undock state is still intentionally unimplemented.

Next: Windows `bash verify_docking.sh`, build standalone EliteGame, run START
DOCKING, verify straight stability and physical passage inside. If planning
fails, use the exact `[DockAuto]` reason. In particular,
`final-capture-corridor-blocked` means the shared collision aperture is still
physically insufficient and must be fixed at geometry truth, never bypassed.

## 2026-09-28 — rerun Windows gate after compile cleanup

Pull through `64e0a665b1fbcc5f8f20d9905674e6df140d2e25`. The stale `ManeuverProgramSampler` validation reference to retired `spatialSlowdownStartFraction` is removed. Rerun `bash verify_docking.sh`, then build and run standalone `EliteGame`. Do not restore the retired slowdown field to satisfy old code.

## Continue from 2026-09-28 single-source Automatic corridor candidate

Work from `forzub/Elite` `main`. The candidate code baseline before this
documentation update is `f60e30ce2afb098856ef12f3f10dab463d2611b7`.

Do not restore the retired architecture:
- no client Automatic route-preflight;
- no separate visible route from the one Follower executes;
- no cross-track speed governor / `spatialSlowdownStartFraction`;
- no PilotSkillExecutor in docking Automatic;
- no time-driven or globally-nearest future-segment spatial progress.

Current intended chain:
`server Planner -> authored geometry -> dynamic compilation -> AcceptedProgram -> Follower -> ShipControlState -> flight law/physics`.
HUD/Hub-map Automatic corridor is a read-only presentation of the same
AcceptedProgram.

Next evidence is Windows standalone only:
`bash verify_docking.sh`, build `EliteGame`, then live START DOCKING with
`[DockAutoRoute]`, `[DockAutoTrack]`, `[DockAuto]`, `[DockResult]`
captured. Confirm inward steering keeps route speed, course error remains
geometric, and the ship advances through the visible accepted corridor.

# 2026-09-27 — published docking candidate; run Windows flight gate

GitHub `forzub/Elite` main now contains the rewritten vehicle-motion
Autopilot, route-local speed keyframes and spatial HOLD capture. Merge
`c3b5dc2` preserved the intervening research document. The updated
`verify_docking.sh` and local builder/sampler/runtime-control native tests
passed; Windows build/live docking are still pending. In MSYS2 MinGW64:
`git pull --ff-only origin main`, `bash verify_docking.sh`, build/run the
full navigation-runtime CTest suite, `bash build_mingw64.sh`, then launch
`build/EliteGame.exe` with console logging. Continue from the recorded
10.0222 m HOLD angular-rate failure. Do not claim physical latch accepted.

# 2026-09-27 — test audit and Windows delivery status

`verify_docking.sh` now includes maneuver_program_sampler and navigation API
purity. NAVIGATION_COMMAND_OWNERSHIP.md and its API-purity test were updated
to the current vehicle-motion contract. The overlay test was also updated
to check retained route start in Hub-local space. All six docking-gate static
checks pass; a broad
scan of 30 non-benchmark static checks found ten unrelated pre-existing red
contracts. Local branch is ahead of origin/main by two commits plus this audit;
the earlier automatic review rejected `git push origin main`, so the user's
Windows `git pull` cannot yet retrieve the rewritten autopilot. Do not state
otherwise or bypass review. Windows build/live HOLD/FinalIngress gate remains
open. See leading CURRENT_STATE/CURRENT_TASK/PROJECT_STATE/Stage-12 sections.

# 2026-09-27 — vehicle-motion boundary candidate (unverified on Windows)

After the earlier spatial HOLD/keyframe commit, the current local changes
rewrite the live Autopilot interface: AcceptedManeuverProgram stores motion
reference without throttle segments; Follower exposes target velocity;
NavigationRuntimeControlBridge::stepVehicle publishes system-frame velocity
plus filtered acceleration/angular demand; GameSimulation delegates Assisted
and Newtonian allocation to the ship flight law. The Assisted law uses measured
velocity, target forward/lateral velocity, and installed physical limits.
Newtonian program proof checks motion authority without issuing engine
commands. Old stepProgram/actuator control channels and physical executor
were removed. Local C++ builder, sampler and runtime-control tests plus
architecture checks pass; full Windows build and docking flight have not run.
The previous 10.0222 m near-HOLD failure must be reproduced with the new build;
pre-capture in-plane trim and physical contact/latch are still outstanding.
The local branch is ahead of origin/main; an earlier automatic push request
was rejected by auto-review. Do not try to publish by another mechanism.

# CONTINUE PROMPT — Elite Automatic docking bridge clock

LATEST 2026-09-27: Windows request 2 on `7a81d58` reached page 436,
10.0222 m/0.995736 m/s before HOLD; omega error 0.280841 > 0.25 triggered
recovery; near-HOLD replan failed Assisted reverse flight. The angular-only fix
did not work. Current unverified candidate implements route-local speed
keyframes (0.8 max ordinary cap, curvature/braking/acceleration, long-straight
peaks), port-up roll reference and spatial HOLD capture independent of storage
page. G++ syntax and local static checks passed; Windows gate/build/live remain
pending. User requires Autopilot to command observed ship behavior rather than
calculate/allocate individual engines; 3D tangent arcs, start/end safe sections,
in-plane stop/trim, bottom-to-bottom entry and eventual contact/latch. These
broader changes are not yet accepted/complete. Read the leading sections of
the four canonical state documents and Stage 12 before further edits.

Repository: public `forzub/Elite`, canonical `main`. Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, `src/game/navigation/STAGE12_END_TO_END.md` before behavior changes. Update all four state documents after state-affecting events. Earlier published implementation: `4da95fa3`, publication notes: `212d9614`.

Latest Windows build succeeded for client and server. Live START DOCKING request 1 reached server Follower/RuntimeControlBridge but repeatedly logged `phase=replan reason=control-bridge-step status=2 snapshot_valid=0`. `PilotSkillExecutor::Status::InvalidInput` is 2. The old bridge was reset at the planned future absolute universe epoch; its first actual control step arrived later but supplied only one gameplay tick as delta. Executor validates exact equality between step-time increment and delta, so this is a proven clock-domain mismatch. Large absolute epochs further erode the tight tolerance. M8E startup frame stalls were also present but are separate from the bridge rejection.

Current correction (not yet target-validated): pilot bridge resets at local clock zero upon accepted program installation; both Aligning and Executing increment local clock by the same gameplay delta passed to `step`/`stepProgram`. Follower samples still use absolute universe time. Bridge failure kinds distinguish invalid intent, invalid actuator and executor rejection. After three repeated failures the server returns a terminal result and reason rather than looping. A native bridge test verifies clock diagnostics. The executor now permits harmless clock/delta rounding up to max(0.1 ms, 1% of the step) while rejecting a missing tick; its delay/decision schedule and filter must share elapsed time. Do not relax actuator, tracking, safety or terminal tolerances. Preserve visible corridor in Automatic, manual visible frame spacing 500 m / 250 m, and two-stage ApproachHold -> stop -> FinalIngress.

After publication, Windows procedure: `git pull --ff-only origin main`, `bash verify_docking.sh`, `bash build_mingw64.sh`, `build/EliteGame.exe 2>&1 | tee build/test-logs/docking-live.log`. Press START DOCKING once. Verify no repeating status 2; record `failure_kind`, `clock_s`, `delta_s`, actuator values if any failure. Observe actual motion, corridor, stop and final ingress, and both `[DockResult]` directions. Target verification remains pending.

Latest Windows result: physical motion began, then repeated
`no-active-program-page status=0` with start/stop oscillation. This is
Timeline::InvalidInput: page adjacency used separately rounded absolute
universe epoch sums with a 1 ns tolerance, while the epoch is hundreds of
millions of seconds. Current correction compares page-local maneuver offsets,
adds a live-scale-epoch regression, holds for BeforeStart and terminates
corrupted timelines with a serialled result. Pull fresh main once published,
rerun Windows gate and verify sustained motion rather than restart loops.
Full Automatic acceptance remains pending.

Latest Windows live after page fix: actual motion begins, then repeated
`tracking_error=1 propulsion_ok=1` with perceived abrupt speed reset. Cause of
tracking error is unknown because old logs lacked each error and limit; physics
only explicitly zeroes velocity at <= default 0.05 m/s in BrakeToStop. Current
candidate adds raw/effective error and limits telemetry, speed and physical
velocity-discontinuity watch, and terminates after three tracking recovery
cycles with `tracking-envelope-exceeded`. Windows run must provide first
`phase=recovery` line and any `phase=physics-watch` line, and separate physical
velocity change from client snapshot presentation. Do not widen the envelope
until exact failed dimension and origin are known.

New Windows evidence localizes failure: at page 1 t=0.34 s, actual speed
6.15515 m/s versus planned 15.7056 m/s; effective velocity error 8.55049 >
8 m/s. Position and attitude were fine; no reported `physics-watch` warning.
Cobra Assisted speed response gain is 5 s^-1, which explains ~9 m/s lag for
a feasible ~46 m/s^2 ramp. Candidate feeds pilot-executed acceleration/gain
forward into canonical Assisted target speed, retaining real acceleration and
speed caps. Automatic finish now issues BrakeToStop and clears sticky speed
setpoint before Human handback (user observed later speed 143 m/s). Native
Windows verification and live flight remain pending; do not mark accepted
until motion, corridor, hold/final ingress and handback pass.

Latest Windows flight reached page 390 at t=117.06 s: actual/planned speeds
97.598/97.714 m/s, but vector error 9.918 m/s (8 limit); position/attitude
were within limits. Assisted physics discarded planned lateral acceleration.
Candidate feeds pilot-executed lateral demand to physically clamped Assisted
stabilization, with native turn test and velocity-vector recovery diagnostics.
Scalar Ruckig backend takes whole-route minimum speed (~98 m/s), so corridor
local 500 m/s is not executable speed. Next: Windows gate/live turn, hold and
final ingress. Route-local speed scheduling remains open.

Windows build and 3/4 native docking tests passed after lateral-turn fix;
navigation_runtime_control failed. Source inspection found the new turn test
asked for 4 m/s^2 but configured only 2 m/s^2 assisted lateral authority.
Fixture now sets strafeAccel=8 and checks both 4 delivered and 100 clamped
to 8. Production physics unchanged. Next rerun verify_docking.sh; if still
red collect ctest -R ^navigation_runtime_control$ --output-on-failure,
then test live turn/hold/ingress.

Latest Windows live run reached ApproachHold page 422 t=126.82 s, 9.41455 m
before HOLD at 1.0255 m/s; angular-rate error .284315 exceeded .25,
triggered recovery and a near-HOLD full-approach replan rejected as Assisted
reverse flight. No final ingress started. Current candidate captures last
HOLD sample within 12m/2mps then physically stops/damps before fresh
FinalIngress; near-HOLD ApproachHold recovery avoids a 1000m forward launch;
reverse-flight failure now includes sample details. Next Windows verify,
client/server build, live stage=final-ingress then pre-capture success.

User rejects abort/replan for minor angular/hold deviations; wants game-like
physical steering. Candidate classifies angular-only tracking excess while
translation/heading remain valid, preserves moving trajectory feed-forward,
adds bounded angular correction, continues within 500m of target at <=5mps
and within ship angular capability. Existing unsafe translation/actuator
errors still recover. Prior change captures hold inside 12m/2mps. Native
regression added. Next: Windows verify/build/live hold, final ingress,
correction and result. IMPORTANT: current objective stops at pre-capture;
physical dock contact/latch is still missing, do not claim full docking.
# 2026-09-27 — live trajectory-following diagnosis in progress

The published Automatic candidate failed the Windows run: a tiny angular
rate excess (0.255151 versus 0.25 rad/s) triggered recovery at 68 m/s;
later the ship trailed the reference speed by 9–10 m/s at 250–300 m/s.
The manual cockpit video and corridor-exit log are a separate manual flight.
Local changes allow bounded in-flight angular correction, prevent very tight
ordinary Assisted route fillets, ramp manual tap torque without changing
Autopilot torque, and add `[DockAutoTrack]` acceleration/rotation samples.
Run `bash verify_docking.sh`, build on Windows and capture every `[DockAuto]`
and `[DockAutoTrack]` line. Determine whether speed lag originates in pilot
response or ship acceleration before claiming flight acceptance.


## 2026-09-27 — route/topology and physical-trajectory correction

Live request logs showed the failure is linear tracking, not attitude: heading error stayed below one degree while velocity lag reached about 9–10 m/s. The accepted reference itself contained acceleration spikes of roughly 74–176 m/s^2. Root causes were found in production code: DockingAdvisoryPlanner accepted the first valid/shortest topology and only searched alternate station sides after failure; Automatic execution then rebuilt sparse displayed advisory gates into a second execution curve; path-progress curvature feed-forward used a 1 m tangent-difference probe over a sampled curve, producing artificial curvature/acceleration spikes; Assisted accepted-program validation checked speed/reverse motion but not whether linear acceleration fit the installed Assisted envelope.

Current main changes: compare valid docking detours by estimated physical traversal time and require a broader non-terminal Assisted radius; expose dense execution gates from the same accepted advisory route and feed those gates to Automatic trajectory generation; replace curvature impulse estimation with interpolated circumcircle curvature consistent with speed limiting; reject path-progress samples outside forward/braking/lateral vehicle authority; add an Assisted accepted-program motion-envelope proof; give [DockAutoTrack] its own cadence so tracking telemetry is emitted independently of other docking diagnostics. Native Windows verification and live flight are still required. The next evidence must include [DockAutoTrack] lines and confirm planned acceleration remains inside ship authority before changing follower tolerances.


## 2026-09-28 — immutable docking geometry contract

Follow-up review found that feeding dense advisory points into TrajectoryGenerator was still insufficient because buildExecutionGuide could round/expand them again. TrajectoryGenerationRequest now has pathGeometryAlreadyAuthored. Automatic docking sets it after DockingAdvisoryPlanner accepts the route, so route geometry p(s) is immutable below that boundary; trajectory generation may only parameterize time/speed and compile attitude. A regression test verifies executionGuidePointsMeters exactly preserve every authored point. Live planned diagnostics now report execution_points, detour, initial_turn_radius_m and max_planned_accel_mps2; [DockAutoTrack] has an independent cadence. Next gate is Windows verify/build/live. Do not loosen follower tolerances until the new log proves the reference geometry and acceleration are physical.


## 2026-09-28 — cruise-radius and curvature-envelope correction

Windows gate exposed two self-inflicted planner failures after the route/topology work. docking_advisory failed because the new minimum Assisted transit radius was derived from absolute maxSpeed even though ordinary authored cruise is 0.8*maxSpeed; the 1000 m nose-first launch fixture cannot fit that over-constrained radius while retaining its straight first frame. The minimum comfort radius now uses the authored ordinary cruise speed, remaining materially broader than the old rule without making valid launch geometry impossible. Automatic docking then failed before execution with path-progress acceleration exceeds vehicle envelope. Root cause: interpolated sampled-curve curvature could contain a small tangential component; adding that fake component to real longitudinal acceleration exceeded the directional envelope. Curvature acceleration is now explicitly projected normal to the instantaneous route tangent. Envelope failures now report sample index, along/lateral demand, limits and speed. A new authored circular-route regression checks forward/braking/lateral acceleration bounds.


## 2026-09-28 — terminal arc is now a primitive, not a squeezed fillet

User identified the correct geometry rule: final-straight start and turn radius are independent. Previous planner conflated the final-straight start ('align') with the virtual sharp vertex consumed by a circular fillet, so a large preferred radius was incorrectly required to fit inside the final straight and route topology was chosen before the turn plane. Preferred Assisted docking now authors a 90-degree terminal circular primitive first. The virtual corner is placed one accepted radius upstream of align so the arc exits exactly at align; align->HOLD remains an untouched semantic straight. The incoming tangent/turn plane is rotated around the docking axis through 36 candidate sectors, and GeometricPathPlanner routes to each candidate before physical traversal-time comparison. A short final straight no longer forces radius relaxation by itself. Radius may relax only after rotated candidates fail. terminalArcRotationDegrees is published and logged as arc_rotation_deg for live proof of side selection. The terminal lead before the arc is 2R so the preceding fillet and terminal fillet cannot consume overlapping parts of the same segment.


## 2026-09-28 — exact terminal arc primitive and actionable diagnostics

The failing short-final-straight regression exposed that the supposedly rotated 6000 m terminal arc was still passed through generic roundGeometry(), where terminalTurnSegmentFraction=0.85 silently limited tangentDistance against the outgoing segment and collapsed the selected radius to 765 m. A second hidden coupling also remained: generic rounding injected request.preferredTerminalTurnRadiusMeters even when called with required radius zero for an unrelated transit join. Both behaviors are removed.

Preferred Assisted docking now constructs the terminal turn directly as an exact commanded quarter-circle primitive: route -> pre-entry straight -> entry -> circular arc(R) -> align -> HOLD. Generic rounding is allowed only before the arc, to settle the preceding topology onto the entry tangent; it cannot resize the terminal arc. The 36-way search rotates the exact-R primitive around the docking axis and routes to each pre-entry. Radius relaxation remains only an explicit fallback after all exact-R rotated candidates fail.

Planner diagnostics now publish requested radius, selected radius, rotation, candidates tested, routeable/rejected counts, transit-ready/rejected counts, collision-rejected count, accepted-candidate count, relaxation flag and last rejection. The docking_advisory test prints this breakdown on relevant failures. GameServer carries the same trace into planned logs and also appends it to downstream trajectory/accepted-program failures so failure logs no longer lose the route-selection context.


## 2026-09-28 — preserve authored vertices through dense execution sampling

The terminal-arc diagnostic proved Planner itself had selected the commanded R=6000 arc successfully: all 36 rotations were routeable, transit-ready, collision-free and accepted, with no relaxation. The remaining failure (`terminal arc consumed or bent the final straight`) was introduced after planning by dense execution resampling. The old global-distance sampler stepped every 10 m over the whole polyline and could place one sample just before an authored semantic vertex (terminal align) and the next just after it; connecting those samples created a chord that cut the arc->final-straight junction. This meant a correct planner product was still being geometrically modified downstream.

Dense/execution sampling now subdivides each authored segment independently and always emits the exact authored endpoint before moving to the next segment. Therefore entry, every exact terminal-arc sample, align and HOLD are preserved as execution vertices; no resampler may skip across them. The docking regression now requires the exact align vertex (1e-6 m) to survive into executionGates and prints the offending segment coordinates/direction dot if the semantic final straight bends again.


## 2026-09-28 — execution sampling uses equal segment subdivision

Dense execution spacing is not a physical 10 m grid. Each authored segment is now divided into N equal subsegments with N=max(1,floor(length/desiredSpacing)); the actual step is length/N. This preserves every authored endpoint exactly and avoids a short remainder fragment or any cross-vertex resampling. The nominal 10 m value remains only an approximate sampling density for curvature/speed evaluation.


## 2026-09-28 — final-straight regression fixed to follow route order

The latest docking_advisory failure was a test-classification bug, not a planner failure. Diagnostics proved all 36 exact-R=6000 terminal arc rotations were routeable, transit-ready, collision-free and accepted with no radius relaxation. The failing segment had x≈-3870 m while ALIGN lies on x=0, so it was an earlier detour crossing the plane through ALIGN, not a segment of the semantic final straight. The regression had inferred 'after ALIGN' from only the signed projection onto the final-axis direction. It now locates the exact preserved ALIGN execution-gate index and validates only subsequent gates in route order, also checking cross-track distance to the docking axis. Any future failure after ALIGN prints align_gate_index, gate_index, direction dot, cross-track and segment coordinates.


## 2026-09-28 — authored-arc envelope regression used wrong frame at zero speed

The rotating-terminal trajectory gate failed with `authored arc exceeded lateral acceleration`, but TrajectoryGenerator itself had already passed its internal directional envelope validation and returned Ready. Root cause was the regression test: for zero-speed samples it hard-coded world +X as the tangent. The terminal sample of the quarter-circle is stopped, so its braking acceleration is aligned with the final route tangent/orientation, not necessarily world +X; the test therefore misclassified longitudinal braking as lateral acceleration. The test now uses velocity direction when moving and the sample orientation forward axis when stopped, exactly matching the generator's physical decomposition contract. Failure diagnostics now include sample index, speed, along/lateral acceleration, acceleration vector and tangent.


## 2026-09-28 — static docking contracts updated to exact-arc architecture

The full docking verification reached the static-contract stage with all native docking/runtime/trajectory tests green, then failed only because check_manual_docking_advisory.py still required the retired token `preferredTerminalRadius*clearanceScale`. That token belonged to the old strategy of expanding clearance around an already selected route. The manual contract now requires the exact terminal-arc architecture instead: terminalPrimitiveRadius, 36 rotational ingress samples, explicit preEntry/center/entry construction, exact selected radius/rotation diagnostics, route-to-entry generic rounding only, and accepted-candidate accounting. It also explicitly rejects reintroduction of clearance-scaling or handing the exact terminal arc back to generic corner rounding.

check_automatic_docking.py was proactively updated in the same pass: execution attitude must come from executionGates rather than sparse display gates; pathGeometryAlreadyAuthored/buildAuthoredExecutionGuide are required; automatic docking must retain the arc decision trace through downstream failures; and the planner contract now requires the 36-way exact terminal primitive plus per-authored-segment execution subdivision.


## 2026-09-28 — Hub map now renders dense authored docking geometry

Live screenshot showed the visible docking route still looked like a small broken polyline despite exact R=6000 planner tests passing. Root cause was presentation ownership: SpaceState stored only DockingAdvisoryPlan.gates (sparse 500/250 m cockpit frames) and published those same points as the Hub-map trajectory. SystemMapRenderer then correctly connected the supplied sparse points with straight segments, so the map visually reconstructed the exact circular arc as coarse chords. DockAdvice now stores separate mapRouteGates. Planner.executionGates (dense exact authored geometry) feeds the full map route, while Planner.gates remains the sparse cockpit/HUD corridor. Live DockAdvisory diagnostics now print requested/selected terminal radius, arc rotation/candidates/accepted, execution_points and hud_gates; handoff logs print hud_gates and map_points. Architecture contracts require this dense-map/sparse-HUD split.


## 2026-09-28 — exact terminal arc ownership, hidden-HOLD fix, and hitch tracing

Live Automatic docking exposed `selected_radius_m=288.887261` followed by `accepted-program-assisted-motion-envelope-infeasible`, even though 36 exact-R=6000 arc rotations had been attempted. The main cause was a stale hidden ownership bug inside `roundGeometry`: callers passed `finalPoint=entry` to build only the transit route to the exact terminal arc, but the helper still unconditionally appended global docking `stop`. The actual candidate became `route -> entry -> HOLD -> exact arc -> ALIGN -> HOLD`. `roundGeometry` now ends only at the explicit caller-supplied `finalPoint`; a regression forbids HOLD from appearing before terminal ALIGN.

Assisted preferred terminal radius is now a hard authored geometry contract. Planner may rotate the exact quarter-circle through 36 sectors and, only if the whole ring fails, slide ALIGN along the docking axis using deterministic offsets while keeping the same radius. It may move inward so the arc peels away before a far-axis blocker or outward when the local ring is crowded. Silent fallback from R=6000 to a small generic fillet has been removed. If no exact-radius solution exists, Planner returns `no collision-free exact-radius terminal arc` with canonical diagnostics rather than inventing a different maneuver.

Diagnostics now identify the concrete blocker: requested/selected radius, final-axis length, axis extension, axis passes, rotation, candidate counts, route/transit/collision rejection counts, accepted count, dominant obstacle id/hits/center/half-extents/radius, and last rejection. Client preflight and server Automatic share `dockingAdvisoryPlanDiagnosticSummary` so the same failure is described consistently.

SpaceState no longer owns or rewrites route geometry. DockAdvice stores the complete `DockingAdvisoryPlan` intact; SpaceState reads `plan.gates` for sparse cockpit frames and `plan.executionGates` for dense Hub-map presentation. It has no sparse-to-dense fallback. Automatic server execution likewise consumes only `advisoryPlan.executionGates`; plan validity now requires both sparse HUD gates and dense execution geometry. Architecture contracts explicitly reject route-geometry ownership returning to SpaceState.

Light periodic freezes reported in live play were not attributable to DockingPlanner from the supplied sample (`dock_ms` stayed sub-millisecond). Runtime hitch instrumentation now lowers the slow-phase threshold to 20 ms when `ELITE_TRACE_RUNTIME` is enabled, lowers frame-gap reporting to 50 ms in that mode, and separately times `game-ui-presentation`, `webview-commands`, `presentation-commit`, existing state update/render, and swap-buffers. The next live log should identify the actual hitch phase instead of guessing.


## 2026-09-28 — terminal docking radius is vehicle-derived, not hard-coded

Production Assisted docking no longer supplies a fixed `preferredTerminalTurnRadiusMeters = 6000`. Client preflight and authoritative server now pass the vehicle's actual lateral acceleration, maximum angular velocity and maximum angular acceleration into `DockingAdvisoryRequest` and set `deriveTerminalTurnRadiusFromVehicle=true`. Planner owns the turn-speed policy (`v_turn = 0.8 * maxSpeed` in the current ordinary profile) and derives the minimum terminal radius as `max(20 m, v_turn^2/a_lateral, v_turn/omega_max)`. The optional explicit radius field remains only for deterministic fixtures/backward-compatible generic callers and production leaves it zero.

Angular acceleration does not directly determine steady circular radius; it determines how much approach distance is needed to ramp hull angular rate to `omega_turn=v_turn/R`. Planner now derives `angularRampDistance = v_turn * (omega_turn/alpha_max)` and includes that in the pre-arc tangent lead. Ship dimensions continue to enter geometric feasibility through `VehicleGuidanceEnvelope::conservativeSafetyRadiusMeters()`, so a larger hull does not arbitrarily multiply the dynamic radius in open space but does enlarge collision/swept-volume clearance and can force a different side/axis position or larger feasible geometry around obstacles. Diagnostics now report turn speed, lateral-derived radius, angular-derived radius and angular ramp distance.

Regression coverage now proves the radius from the vehicle equations rather than locking a numeric 6 km constant. Architecture contracts reject reintroduction of the fixed 6000 m production policy.


## 2026-09-28 — docking advisory test build fix

The vehicle-derived-radius regression edit accidentally left the anonymous helper namespace open, placing `main()` inside it and producing `expected '}' at end of input`. The namespace is now closed immediately after `printTerminalArcDiagnostics`, before global `main()`. The previous CTest PASS observed after the failed build was a stale previously-built executable and is not considered a valid gate result.


## 2026-09-28 — static ALIGN token updated after axial search refactor

Full native docking/runtime/trajectory gates were green. The remaining verify failure was static-only: the architecture checker still required the retired literal `point=align`, while the exact terminal primitive now ends at the selected sliding-axis endpoint `candidateAlign`. Production already enforced `if(j==arcSegments) point=candidateAlign;`; the static contract now checks that actual invariant and identifies it as the selected ALIGN endpoint.


## 2026-09-28 — docking obstacle geometry now uses authoritative HitVolumes

Live vehicle-derived radius planning failed with `turn_speed_mps=400`, `lateral_radius_m=2175.394636`, `angular_radius_m=255.463965`, so the new radius calculation itself was valid and lateral authority was the governing limit. All exact-radius candidates were instead rejected by `blocker=object:1`, whose obstacle was the entire Station01 logical envelope as one Box (`half=(2000,2044.78,2510.69)`, bounding radius about 3805.88 m). This erased every station hole/pass-through and made real docking approach geometry look like solid station material.

Docking planning no longer rebuilds descriptor-wide obstacles for static infrastructure. Authoritative server planning copies each StaticObject HitComponent into the async planning snapshot and uses `NavigationHitVolumeAdapter::buildObstacles` at the predicted Hub pose. Client preflight uses the replicated hit-volume cache retained by ClientWorldState and the same adapter; if authoritative hit-volume geometry is genuinely unavailable it fails closed with `ObstacleGeometryUnavailable` instead of inventing a coarse Station box. SpaceState remains outside geometry ownership.

NavigationHitVolumeAdapter now also accepts replicated DebugHitVolumeSnapshot vectors so client/server use the same per-volume OBB semantics. A new `navigation_hit_volume_adapter` native gate proves that two solid module volumes preserve a real navigable gap, still block their actual solid regions, and remove destroyed volumes from navigation. `verify_docking.sh` builds/runs this gate, and architecture contracts reject a return to `makeNavigationObstacleForObject` in docking planning.


## 2026-09-28 — automatic tunnel lifecycle and in-place tracking recovery

Live video/log showed three separate failures after exact station HitVolumes made route planning viable. First, the client presentation intentionally kept Automatic HUD gates visible after `DockingAdvisoryTrackingResult::Left` and latched `m_noSafeDockingGuidanceSolution=true`; this made the tunnel remain visible/red even after the tracker later returned Inside. Second, HUD progress stayed tied to the old sparse segment while the craft was outside, so a downstream re-entry could still be measured against an obsolete gate (live request 2 reported about 5.9 km lateral error at gate 9). Third, server recovery discarded the accepted program on small dynamic envelope excursions. The decisive live sequence was page 466 at the stopped HOLD end (24.68 m position error inside the 25 m spatial envelope, 1.5 m/s speed, only angular-rate error), followed by a fresh page 5 plan and later page 353; this explains the observed turn-back/corner-cutting behavior.

SpaceState now tracks `corridorDeparted` explicitly. A real Automatic departure hides only the spatial `:frames` HUD tunnel while preserving the Planner route; re-entry into the nominal corridor restores it and clears only the departure-specific warning. Departure no longer latches global no-safe red state. HUD progress is monotonic but can advance to the nearest future sparse segment, allowing a craft that rejoins downstream to be judged against the route segment it actually re-entered. Native tracker regression now proves Left -> Inside re-entry.

Automatic execution now reserves 20% of the weakest forward/braking/lateral vehicle authority for Follower correction instead of the old hard cap of 0.5 m/s^2. For the current Cobra authority (~73.55 m/s^2), that is ~14.71 m/s^2 of correction reserve rather than 0.5, while TrajectoryGenerator plans with the remaining authority. This directly addresses the long parallel-to-tunnel drift: a large cross-track error can now be corrected on a game-relevant timescale without fake RCS authority. The planning log reports `linear_feedback_reserve_mps2` and `angular_feedback_reserve_radps2`.

Recovery is also less brittle without weakening the spatial safety envelope. If position and hull-forward errors remain inside their accepted limits, modest velocity/rate excess is treated as an in-place control correction instead of a stop/replan. HOLD capture uses the accepted tracking position envelope and half the velocity-error envelope (minimum 2 m/s), so the observed 24.68 m / 1.5 m/s near-terminal state transitions to FinalIngress stabilization rather than rebuilding a kilometre-scale Stage-1 route. As a second guard, a Stage-1 recovery already within 150 m of HOLD disables the long Assisted terminal arc/9 km approach policy and plans only the short positioning maneuver. A new `DockingAutomaticRecoveryPolicy` centralizes and tests these rules.


## 2026-09-28 — SpatialCorridor execution: tunnel/canyon geometry owns progress

Live Automatic docking proved that the HUD departure was real (for example request 1 left at gate 4 with lateral 61.65 m, vertical -165.32 m while nominal/release cross-sections were 60/120 m). Root cause analysis found a deeper execution mismatch: `ManeuverProgramSampler::sample()` and storage-page selection were driven by universe time, so the accepted reference could move far ahead while the real ship lagged or drifted. The Follower was then correcting toward an escaping time point rather than flying the accepted tunnel. This is unacceptable for future tunnels, canyons and narrow passages.

`AcceptedManeuverProgram` now has an explicit `ReferenceMode`: `TimeScheduled` or `SpatialCorridor`. Automatic docking Stage 1 (ApproachHold) is `SpatialCorridor`; FinalIngress remains `TimeScheduled` because the final target can be moving/rotating and timing is part of that short maneuver. The builder preserves and validates the mode on every fixed-capacity storage page.

For `SpatialCorridor`, `ManeuverProgramSampler::sampleSpatial` projects the authoritative vehicle position onto the current accepted path segment and derives P/V/A/attitude from that physical progress. The cursor is monotonic and sequential: a fixed step may advance at most one accepted segment, and a storage page may advance at most one page only after the real craft crosses the current page endpoint plane. It never searches arbitrary future segments by nearest Euclidean distance, preventing a hairpin/canyon path from jumping through a wall to a nearby later branch. Page-local nominal timestamps do not advance or expire spatial execution; only the common accepted epoch gates initial start. Spatial completion is state-based and may complete early when the accepted terminal state is physically reached.

Trajectory geometry is sufficiently dense for this contract: scalar path progress is sampled every 0.02 s, about 8 m at 400 m/s, far denser than 500/250 m HUD frames. HUD cadence remains presentation-only and is not Follower geometry.

A corridor speed governor is now part of the accepted tracking policy via `spatialSlowdownStartFraction=0.50`. Inside the first 50% of the proved cross-track envelope, planned speed is unchanged. From 50% to 100%, along-path target speed is reduced continuously; at the edge it reaches zero so centering authority wins over forward schedule. Positive planned forward acceleration is reduced with the speed scale, curvature/lateral feed-forward scales with speed squared, and existing planned braking is never weakened. For SpatialCorridor the route-loss envelope is based on cross-track position/velocity plus attitude/angular state; deliberate along-track slowdown remains controlled but does not itself declare the tunnel lost.

Server runtime keeps `currentSpatialSegment`, selects spatial storage pages by real position, passes the monotonic cursor into Follower, and reports `reference_mode`, `ref_segment`, `ref_alpha`, `ref_distance_m`, and `spatial_speed_scale` in live tracking/recovery diagnostics. Regressions prove: physical progress beats nominal clock, cursor cannot jump backwards, hairpin branches cannot be skipped, a fixed step cannot skip multiple pages, spatial completion can occur before nominal time, and cross-track slowdown does not falsely trigger route loss.

## 2026-09-28 — continue from spatial checkpoint launch candidate

Current main contains the SpatialCorridor correction from the latest Windows live log. Stage-1 docking is position/progress driven, not nominal-time driven. A stopped first control point no longer deadlocks at `ref_segment=0 ref_alpha=0 target_speed_mps=0`: if the lower spatial sample is stopped and the next accepted control point is moving, Follower takes the current segment tangent and next-point speed as its local launch target without advancing position by time.

Spatial route loss is geometric: cross-track position/velocity and forward-angle envelopes remain authoritative. Angular-rate mismatch is corrected with bounded angular feedback and does not by itself invalidate a SpatialCorridor. FinalIngress remains TimeScheduled.

Published implementation/test commits: `2608765`, `5a2ca7d`, `3f199b7`. Documentation synchronization follows on main. Next action is target Windows evidence: pull, run maneuver tracking + full docking verification, rebuild client/server, then live START DOCKING. Capture all `[DockAutoTrack]`, `[DockAuto]` recovery/result lines. Do not mark accepted until the live craft actually leaves the first checkpoint, follows the tunnel course, and angular-rate-only error no longer causes recovery.

