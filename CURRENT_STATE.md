## 2026-10-03 — player navigation/autopilot moved fully to client

Architecture boundary changed after the live Manual/Automatic ownership race.

Player docking/navigation is now client-owned:
- SystemMap/UI creates only local DockingRouteRequest intent.
- SpaceState client state machine owns Stabilizing -> Planning -> RouteReady -> Executing.
- temporary route-planning stabilization uses ClientRouteAutopilot::stabilize -> PredictivePilot -> ordinary ShipControlState.
- Manual "SHOW ROUTE" stops/stabilizes through ordinary controls, plans from authoritative replicated ship/world state, publishes the tunnel, then stops overriding Human input.
- Automatic uses the SAME client RoutePlanner product, adapts executionGates to AcceptedManeuverProgram SpatialCorridor, then executes RouteFollower V2 -> PredictivePilot V2 -> ordinary ShipControlState.
- client virtual-pilot control is submitted with the exact normal GameClient::submitInput path; map/UI state does not interrupt autopilot sampling.
- authoritative server snapshots remain the feedback source for real position, velocity, attitude and angular rates.

GameServer is deliberately blind to player navigation intent:
- no Begin/Cancel Automatic docking commands;
- no docking-guidance preparation commands;
- no DockingAutomaticRuntime;
- no player docking RoutePlanner/TrajectoryGenerator/AcceptedManeuverProgram/RouteFollower/PredictivePilot;
- no replicated automatic route/result/authority state.
GameServer receives ordinary numbered ShipControlState, applies it through the normal fixed-step control/physics path and publishes authoritative state.

ClientShipCommand docking task payload was removed. WireProtocolVersion is 13. SimulationSnapshotWireSchemaVersion is 11.

Server-side NPC navigation remains a separate authoritative AI concern and may still link shared navigation libraries; it is not player autopilot ownership.

New regression: client_route_autopilot proves RoutePlan -> SpatialCorridor -> RouteFollower V2 -> PredictivePilot -> ordinary ShipControlState with direct navigation demand flags disabled.

## 2026-10-03 — live map exposed stop-speed terminal hairpin and docking-action hitbox mismatch

Live Hub-map evidence showed a terminal docking arc with an absurd ~pivot-sized radius after stop-and-settle. Root cause: DockingAdvisoryPlanner derived preferred terminal radius from initialSpeedMps; Automatic deliberately stops before planning, so the radius collapsed to the hull floor (~69 m for Cobra).

Fix:
- terminal geometry now uses a designTurnSpeedMps = max(initial speed, 0.5 * planned max speed), bounded by max speed;
- minimum authored radius is max(100 m, 20 * hull radius);
- lateral/angular radius terms are derived from design turn speed;
- stopping before planning no longer shrinks the authored curve;
- regression requires the stopped and moving-origin fixtures to preserve the same design turn when both are below design speed.

The same live run contained no [DockRequest] ui-action=start_docking / [DockAuto] request lines, proving Automatic never reached SpaceState/server ownership. A concrete UI mismatch was found: map action buttons were rendered at 23 px but hit-tested at 22 px. Hit testing now exactly matches 23 px and SystemMapBehaviorTests covers the final visible half-pixel of the lower start_docking button.

Next Windows evidence:
1. docking_advisory;
2. system_map_behavior + object-overlay contract;
3. automatic-docking architecture contract;
4. EliteGame live run. The new route should have a visibly broad terminal arc and clicking STЫKOVKA must emit [DockRequest] ui-action=start_docking immediately.

## 2026-10-03 — navigation test suite realigned to PredictivePilot V2

Full navigation-test audit completed after the V2 autopilot split.

Current production/V2 acceptance path is now:
- AcceptedManeuverProgram;
- RouteFollower V2;
- PredictivePilot V2;
- ordinary ShipControlState;
- SharedShipPhysics / DynamicMotionSystem::applyLocalFrameInput.

Changes:
- navigation_runtime_control is V2-only; legacy Bridge/direct-demand and old Assisted-helper cases were deleted from that gate.
- ManeuverProgramSamplerTests now uses public RouteFollower V2 for follower completion checks.
- new navigation_v2_tunnel_proving_ground executes RouteFollower V2 + PredictivePilot V2 through ordinary ship controls and checks accepted-tunnel containment, <=3 s Assisted slip, physical progress and learned angular authority.
- old NavigationCompositeProvingGround and matrix/execution labs are retained only as legacy_navigation_lab comparison/oracle tests.
- legacy_navigation_lab targets are EXCLUDE_FROM_ALL and skipped by the default navigation-runtime runner.
- legacy standalone navigation-tool architecture contracts are opt-in via ELITE_RUN_LEGACY_NAVIGATION_LABS=1.
- verify_docking.sh now gates on the V2 tunnel proving ground rather than legacy maneuver_tracking/composite execution.
- tests/run_all_mingw64.sh now includes the navigation-runtime PredictivePilot V2 suite.
- full architecture runner now includes automatic-docking and docking-module V2 contracts.
- BUILD_TEST_LAYOUT.md updated to the V2 target/test names and binary marker.

Windows compile/runtime verification is pending.

## 2026-10-03 — PredictivePilot V2 second layer: adaptive dynamics + centerline-locked corridor following

Production automatic docking now uses only PredictivePilot V2. ShipControlAdapter and direct navigation actuator demand are forbidden in the production server path.

Second-layer pilot state is persistent per DockingAutomaticRuntime and learns from measured response:
- effective pitch/yaw/roll authority from observed delta-omega after ordinary pilot inputs;
- Assisted forward-speed response from measured delta-V after +/- input;
- Assisted course-response time from measured VREL-to-nose error closure.
These learned values are bounded by ShipParams and feed the next control decision.

Attitude control now evaluates discrete ordinary-control candidates (-1,-0.5,0,+0.5,+1) over a short prediction horizon and penalizes predicted angular overshoot/braking debt. It no longer assumes the configured angular authority is exact.

SpatialCorridor execution no longer delegates to legacy TrajectoryFollower. RouteFollower V2 samples the accepted program directly. There is no 50-250 m private look-ahead point and no steeringRay. The accepted centerline is the only path. Course correction is constructed from:
- tangent of the CURRENT accepted centerline segment;
- bounded inward velocity component from the real craft to the closest point on that same segment.
The correction preserves route-speed magnitude and is limited to 35% lateral share plus the accepted program's feedback/braking authority. It cannot target a point beside the corridor or skip across a corner.

Architecture test now fails if V2 RouteFollower contains TrajectoryFollower, lookAhead or steeringRay, or if GameServer contains ShipControlAdapter.

DockAutoTrack logs now expose learned_pitch_alpha, learned_yaw_alpha, learned_roll_alpha, assisted_course_tau_s and assisted_speed_response_mps2.

Windows compile/test verification pending.

## 2026-10-03 — production autopilot is now a virtual pilot, not an actuator controller

Architecture reset after repeated tunnel regressions.

Production automatic docking no longer writes navigationAccelerationDemandValid / navigationVelocityTargetValid and no longer commands DynamicMotionSystem through navigation-only acceleration/velocity helpers. GameServer now converts Follower guidance into ordinary ShipControlState pilot inputs via src/game/navigation/autopilot/ShipControlAdapter.h.

New ownership:
- Planner owns route geometry, speed schedule, STOP points and physically feasible reference.
- Follower measures route error and produces guidance/correction intent.
- ShipControlAdapter is the autopilot pilot: pitch/yaw/roll, targetSpeedRate, keypad RCS and ordinary END/BrakeToStop only.
- ShipController + LocalFlightControlStateMachine + DynamicMotionSystem::applyLocalFrameInput exclusively own how those controls become physical motion and engine usage.

Control-law semantics:
- Assisted: autopilot changes the same persistent forward-speed target as player +/- input. Direction remains hull nose; the ship's ordinary Assisted law handles acceleration/braking and VREL-to-nose stabilization.
- Newtonian: autopilot commands ordinary positive primary-main throttle; explicit stop uses ordinary END/autobrake (turn-and-burn); no reverse-throttle fantasy is injected.
- Authored STOP with small residual motion (including cases such as 0.045 m/s) uses ordinary keypad RCS through forward/strafe/lift inputs. Measured near-zero speed is not itself a maneuver-state selector.

The composite proving ground was migrated to the same pilot-input path so synthetic success can no longer depend on a different actuator API than production docking.

DynamicMotionSystem::applyNavigationAssistedFlightModel was restored to the last known-good implementation from commit 737852c8ff76346eb8a90c5c3d8d2bc57c1dc0e9. It remains only for legacy diagnostics/labs while those seams are migrated; production automatic docking does not call it.

Architecture guard: tests/architecture_contracts/check_automatic_docking.py now fails if GameServer automatic docking writes direct navigation demand fields or calls direct navigation propulsion helpers.

New implementation marker:
dock-auto-20261003-pilot-input-controls

Windows compile/test verification pending.

## 2026-10-03 — Assisted scalar speed decoupled from steering vector direction

Latest Windows evidence: navigation_runtime_control and maneuver_tracking_controller pass. The low-speed bypass START fix no longer produces tracking failure; the planner safely holds through several AdjustedClear iterations until the moving hazard clears. The remaining composite failure is now portal_102 Assisted: the portal program begins tangent to the hull, but execution stalls with final position error ~47.36 m, final velocity error ~6 m/s, max course error ~124.8 deg and 500 tracking ticks.

Root cause in production DynamicMotionSystem::applyNavigationAssistedFlightModel: scalar Assisted speed was computed as max(0, dot(targetVelocity, currentHullForward)). Spatial Follower uses targetVelocity direction as a look-ahead steering direction. On a sharp bend, once that target points more than 90 deg away from the current nose, the projection becomes <=0 and the flight law commands zero forward speed. The ship then stops exactly when it needs to keep moving and rotate, causing spatial-progress deadlock.

Current main now defines Assisted command semantics explicitly:
- scalar speed request = |targetVelocitySystemMps|;
- travel direction = actual current hull nose;
- Follower/attitude loop owns steering direction;
- lateral target velocity remains non-authoritative for propulsion, preserving the prior nose-coupling fix;
- a new navigation_runtime_control regression uses a target vector 120 deg away from the current nose and requires targetForwardSpeedMps to remain 6 m/s, positive main-engine thrust, and zero precision RCS.

Windows verification pending.

## 2026-10-03 — low-speed dynamic bypass now uses explicit moving START

Latest Windows evidence after the Assisted nose-coupling production fix: navigation_runtime_control and maneuver_tracking_controller pass; Assisted replacement and first continuation now keep max continuous >8 deg slip at 0.0 s. The remaining composite failure moved to dynamic_bypass_3 after a full braking stop: an 8 s continuation accumulated 105 tracking-envelope ticks despite zero excessive slip.

Root cause: test-side fitAuthorityBoundedReplacement still authored low-speed continuations directly from the measured stopped state (v0=0) while makeCurve also imposes a0=0. For a TimeScheduled moving program that creates the same STOP/START ambiguity already fixed for portal_102: the reference clock advances while the real vehicle has no explicit non-zero launch state.

Current main applies the same explicit contract to low-speed bypass reauthoring:
- if measured start speed is below the transit floor, the authored moving start uses v0=0.25 m/s along the actual hull forward axis;
- makeCurve and all dense physical/dynamic proof sampling use that authored start, so proof and execution describe the same moving boundary condition;
- feed-forward acceleration at t=0 remains zero by the quintic construction;
- ReplacementFit records authoredStartSpeedMps and authoredStartTangentToHullDeg;
- continuation logs now include start_ref_speed_mps, start_tangent_to_hull_deg, max_env_pos_m, max_env_vel_mps and max_env_course_deg.

Windows verification pending.

## 2026-10-03 — Assisted controller: remove lateral target-velocity equilibrium

Latest Windows evidence: Newtonian remains fully green. Assisted physically completes the constrained portal and final capture with tracking_exceeded_ticks=0, but the new temporal guard correctly exposed a real control-law issue: nose/VREL slip above 8 deg persists beyond the agreed 3-second window.

Root cause in production DynamicMotionSystem::applyNavigationAssistedFlightModel: after the ordinary nose-coupled Assisted law had already damped measured lateral VREL toward zero relative to the current hull, autopilot code added a second lateral velocity equilibrium derived from targetVelocitySystemMps. Because Follower target velocity points toward a future look-ahead direction while the hull is still turning, VREL could chase that future direction instead of the current nose.

Current main fixes this at the flight-law layer:
- targetVelocitySystemMps no longer creates a lateral velocity equilibrium; direction remains owned by the actual current hull nose.
- trajectory lateral feed-forward may only assist an already-existing Assisted nose-alignment acceleration and is suppressed when it would oppose alignment or create sideways VREL from an aligned state.
- ordinary route execution still uses Assisted stabilization, never precision RCS.
- navigation_runtime_control regression coverage now proves that lateral target velocity alone cannot manufacture slip, same-direction feed-forward may assist alignment, opposite feed-forward cannot weaken alignment, and aligned VREL cannot be pushed sideways by feed-forward.
- composite phase logs now print max_continuous_assisted_slip_s so any remaining >3 s violation is localized immediately.

Windows verification pending.

## 2026-10-02 — Assisted composite now judged by velocity-alignment lag duration, not peak slip angle

Latest Windows evidence: Newtonian remains fully green. Assisted now traverses portal_102 with tracking_exceeded_ticks=0, hull half-width 17.293 m inside the 19 m portal, safe dynamic clearance, sub-meter terminal position error, and successful final capture. The only remaining failure was the legacy aggregate assertion maxSlipDeg <= 8.0. That metric records the single largest instantaneous angle between hull nose and VREL over the whole scenario, so it rejects legitimate transient turning even when the velocity catches up promptly.

Current main retains maxSlipDeg as a diagnostic but replaces the pass/fail criterion with the actual agreed Assisted behavior: while speed >0.25 m/s, a material nose/VREL mismatch above 8 deg may exist transiently, but one continuous exceedance may not persist beyond 3.0 seconds. ExecutionMetrics records maxContinuousAssistedSlipSeconds per accepted program and CompositeMetrics carries the worst phase. If this new duration guard fails, the controller itself is genuinely too slow; do not loosen it merely to pass the test.

Windows rerun pending.

## 2026-10-02 — Newtonian composite green; Assisted route-loss course moved from hull attitude to actual VREL

Latest Windows evidence:
- maneuver_tracking_controller passes;
- Newtonian now completes the entire composite proving ground: portal start tangent matches the hull (~0 deg), portal hull half-width is 17.276 m inside the 19 m limit, tracking_exceeded_ticks=0, portal terminal errors are sub-meter/sub-0.1 m/s, and final capture succeeds.
- Assisted also physically completes portal_102 and final capture with safe dynamic clearance and hull width, but the synthetic route-loss counter reported 70 ticks solely because max_env_course_deg reached 55.6 deg while cross-track position/velocity remained small.

Root cause: SpatialCorridor course loss for Assisted was measured as hull-forward vs authored local tangent. TrajectoryFollower deliberately rotates the hull toward a look-ahead point on a bend, so that commanded steering lead could invalidate its own accepted corridor.

Current main separates control attitude from route geometry:
- Assisted SpatialCorridor route course is measured from actual vehicle velocity (VREL) against the accepted tangent;
- hull attitude remains the steering target and continues receiving angular feedback toward the look-ahead reference;
- Newtonian remains exempt from hull/course route loss because hull/velocity slip is valid doctrine;
- regression tests prove (a) a 60-degree Assisted hull steering lead with VREL still tangent is not route loss, and (b) a 40-degree actual VREL course departure does invalidate the corridor even when the cross-track speed magnitude is still inside its independent envelope.

Windows rerun pending; do not mark Assisted composite green yet.

## 2026-10-02 — narrow tunnel start tangent now follows actual hull heading

Latest Windows evidence: STOP/RCS/START unit contracts pass, but composite Newtonian still failed portal_102 with tracking_exceeded_ticks=0, final speed near zero, final position error ~48.7 m, and actual hull half-width ~61.5 m against the 19 m portal. Root cause was tunnel authoring, not tracking: after STOP the real start velocity is exactly zero, so makeCurve() had no start-direction derivative; the first geometric segment fell directly toward the portal. A later post-hoc velocity rewrite pointed sample[0] along that already-authored segment, while Newtonian also used OrientationMode::FixedStart. Thus the accepted tunnel direction and hull heading diverged by construction.

Current main fixes the agreed contract:
- zero spatial velocity is now unambiguously STOP; the old TrajectoryFollower heuristic that auto-launched from a v=0 lower sample has been removed;
- portal_102 moving START is authored in the curve itself with non-zero start reference velocity along the actual start.basis.forward and a_ff(0)=0;
- the tunnel therefore begins tangent to the actual hull heading, then curves toward the portal;
- constrained passage orientation now uses VelocityAligned for Newtonian as well as Assisted so the hull can rotate along the bend instead of remaining frozen at the incoming bypass attitude;
- the obsolete regression test that required auto-launch from a stopped control point was replaced by explicit STOP and moving START tests;
- COMPOSITE-PORTAL-PROOF now logs start_ref_speed_mps and start_tangent_to_hull_deg; the latter should be ~0.

Windows verification pending.

## 2026-10-02 — explicit STOP(v=0) / START(v>0,a=0) semantics and physical RCS trim

User clarified the desired control contract: if an authored point requires zero velocity but the craft arrives with a small residual velocity, physically remove that residual with manoeuvre/RCS thrust; do not reinterpret near-zero as the start of a moving segment. Conversely, a new moving segment must begin with non-zero reference speed while its boundary feed-forward acceleration may be zero.

Implemented on main:
- Navigation control intents now carry precisionTranslationOnly through NavigationFrameBoundary and NavigationRuntimeControlBridge into ShipControlState.
- DynamicMotionSystem::applyNavigationPrecisionVelocityTrim removes measured velocity error with physical manoeuvre-thruster authority only; main engines and Assisted stabilization remain off.
- GameSimulation routes precision navigation samples through that RCS-only executor before ordinary Assisted/Newtonian allocation.
- TrajectoryFollower automatically requests precision RCS trim when the authored reference is exactly v=0 and measured speed is below the explicit 0.50 m/s precision-stop entry threshold.
- Spatial moving START is distinct: current composite portal authoring seeds sample[0] with the first moving sample's non-zero speed while keeping sample[0] feed-forward acceleration exactly zero.
- The composite braking/hold helper now macro-brakes first, switches to physical RCS below 0.50 m/s, and only declares STOP inside ShipParams::stopSpeedEpsilonMps before snapping numerical residue to exact zero.
- Regression coverage added for RCS-only STOP and STOP-vs-moving-START classification.

Windows compile/runtime verification pending; do not mark green yet.

## 2026-09-29 — spatial composite harness now obeys accepted validity window

Latest composite run shows the production spatial fixes working for Newtonian: portal_102 reports tracking_exceeded_ticks=0, max envelope position/velocity/course error all 0, and the 19 m hull constraint remains satisfied. The remaining failure is phase completion, not route loss.

Root cause in the synthetic composite harness: executeProgram still bounded every non-partial phase by nominalEnd + capture-overrun, even for SpatialCorridor. Production NavigationExecutionReplanPolicy instead treats AcceptedManeuverProgram::validUntilUniverseTimeSeconds as the hard execution boundary and requests SegmentExpired replanning only there. The harness now mirrors that contract: SpatialCorridor may execute until validUntil; TimeScheduled phases retain nominal/capture-overrun timing. PORTAL-ACTUAL now also prints final P/V error, simulated duration and validity budget.

Windows rerun pending; do not mark composite green yet.

## 2026-09-28 — production spatial handoff and Newtonian drift contract fix

Latest composite evidence exposed two real production contract mismatches after portal_102 was switched to SpatialCorridor:
- ManeuverPhaseGate::ScheduledMoving still advanced purely at nominal program time, so a spatial tunnel phase could hand off while the craft had not physically completed the corridor. This is now fixed: TimeScheduled phases keep clock handoff; SpatialCorridor phases advance only when TrajectoryFollower reports Complete, including early physical completion.
- ManeuverTrackingController treated hull/course angle as spatial route loss for every control law. That is correct for AssistedVelocity but wrong for NewtonianMainEngine, where large velocity/hull slip is a valid drift/braking state. Newtonian spatial route loss now remains geometric cross-track position/velocity; attitude feedback still operates but hull slip alone no longer invalidates the tunnel.

Regression tests were added in ManeuverPhaseGateTests and ManeuverTrackingControllerTests. Windows verification is pending; do not mark the production contract green until those two tests and composite pass on target.

## 2026-09-28 — Assisted portal tracking failure traced to wrong reference mode

Latest composite evidence: Newtonian now passes fully. Assisted also keeps the 19 m portal geometry and moving-hazard clearance safe (planned 1.759 m, actual 1.802 m, hull half-width 17.401 m) but reported 81 tracking-envelope exceed ticks during portal_102.

Root cause: the constrained portal program was still using AcceptedManeuverProgram::ReferenceMode::TimeScheduled by default. That makes tunnel execution a race against nominal time, while the intended tunnel/canyon contract is SpatialCorridor: real vehicle position advances the reference and route loss is geometric cross-track/course error.

Current candidate fix on main:
- portal_102 explicitly uses SpatialCorridor for both control laws;
- Assisted portal metadata uses AssistedVelocity; Newtonian uses NewtonianMainEngine;
- the synthetic execution harness now preserves a monotonic spatial-segment cursor and passes it into TrajectoryFollower, matching the production RouteFollower contract;
- portal diagnostics now print max envelope position, velocity, course and angular-rate errors separately;
- final StateCapture remains TimeScheduled because it is a terminal-state maneuver, not a tunnel-following segment.

Windows rerun pending; do not mark composite green yet.

## 2026-09-28 — composite long-segment dynamic proof

Latest target evidence still had only `navigation_composite_proving_ground` red. The new log showed safe dynamic clearance during replacement (4.747 m), continuation (22.821 m), and portal-entry settle (30.597 m), so the remaining failure had to occur in the later long accepted segments.

Root cause: the fixture's runtime planner horizon is 4 s, but it then executed a 16 s narrow-portal program and an 18 s final-capture program without a whole-program moving-hazard proof. `NominalClear` for the bounded horizon was being misused as authority for a much longer future segment.

Current candidate fix:
- added dense whole-program dynamic-clearance proof using `ManeuverProgramSampler` against the authoritative moving hazard;
- both `portal_102` and `final_capture` require that proof before Follower execution;
- if the future portion is unsafe, the craft remains in a safe stopped/settled hold while the hazard continues moving, then the program is re-authored from actual state and re-proved;
- the 19 m portal is unchanged and the hazard is never disabled;
- logs now expose `COMPOSITE-PORTAL-PROOF/ACTUAL` and `COMPOSITE-CAPTURE-PROOF/ACTUAL`.

Windows rerun pending; do not mark composite green yet.

## 2026-09-28 — composite portal settle after 5/6 targeted rerun

Target-machine evidence: 5 of the 6 audited tests now pass; only `navigation_composite_proving_ground` remains red. The narrow 19 m passage itself now passes, exposing a later aggregate dynamic-clearance failure.

Root cause in the synthetic proving-ground authoring: stretching the narrow quintic from 10 s to 16 s while preserving non-zero entry velocity increases the `v0*T` boundary term and can enlarge geometric overshoot. The fix keeps the hazard active, settles residual velocity before entering constrained topology, then authors the narrow precision segment from a stable state. `executeActiveBraking` now uses the production-style `stepVehicle(..., targetVelocity=0)` path so Assisted braking reaches the real fore/reverse main engine.

Commit candidate: `b69160aabcdc4e5899c0b05d9fc98371161b06d3`. Windows rerun pending; do not mark the suite green yet.

## 2026-09-28 — runtime-suite stale harness cleanup after Windows evidence

Windows full navigation-runtime CTest completed 29 tests with 6 failures while the current docking-specific chain remained green. The failures were audited instead of weakening production tolerances.

Current candidate corrections on main:
- NavigationRuntimePlanner provenance fixture gets a non-degenerate local-bypass search envelope; its purpose is conflict identity, not testing the exact grid boundary.
- Corner-family, rigid-body, chained-limit and composite labs now execute Assisted through the production-style `stepVehicle -> applyNavigationAssistedFlightModel` path instead of the retired acceleration-only harness path.
- Those comparison fixtures now model the real Cobra Assisted front main engine and Assisted stabilization authority, while Newtonian comparison fixtures keep reverse-main unavailable.
- PhysicalManeuverSearchCoordinator no longer treats Assisted as an unsupported law; its shared-state-blocker test now uses a genuinely shared unsupported initial angular state.
- Chained attitude seam uses a physically meaningful 0.01 degree threshold rather than a sub-float 1e-6 degree false hard-fail.
- Composite narrow-passage half-width remains 19 m; traversal duration was increased instead of widening the corridor.

Target-machine rerun is pending. Do not claim the six tests are green until Windows evidence confirms them.

## 2026-09-28 — authoritative build layout + speed-aware Automatic docking

The current Automatic docking baseline is now stop-and-settle before planning.
Do not restore the earlier moving-origin/coast contract from older sections below.

Current implementation facts:
- Automatic preparation commands BrakeToStop and waits for stable linear/angular motion before planning;
- RoutePlanner receives the measured planning-origin speed as `initialSpeedMps`;
- Assisted terminal-turn radius is no longer derived from `0.8 * maxSpeed`; it is derived from actual planning-origin speed plus vehicle lateral/angular authority, with only a hull-scaled geometric floor;
- ordinary transit fillets no longer fail merely because a cruise-speed radius does not fit; the dense speed profile slows for curvature;
- the selected docking target uses its semantic dock clearance rather than the generic 80 m Hub-infrastructure clearance;
- Automatic log marker: `impl=dock-auto-20260928-stop-speed-aware-turn-semantic-clearance`;
- `NavigationCompositeProvingGroundTests.cpp` was updated to the current 4/5-argument TrajectoryFollower API after a Windows compile failure.

Build/test command ownership is documented in root `BUILD_TEST_LAYOUT.md`.
Before issuing commands, inspect the current owning `CMakeLists.txt` and canonical scripts; never assume a standalone test target belongs to root `build/`.

Latest Windows evidence before the composite-test compile fix showed an old executable was still being run, so no new live-flight conclusion is accepted yet.

## 2026-09-28 — moving-start docking + rotating HOLD failure fixed in code

A pre-module-split live run exposed two independent Automatic docking defects.

Observed live evidence:
- starting Automatic while the ship already had VREL forced `BrakeToStop`;
- the geometrical route was available, but AcceptedProgram rejected the
  boundary-valued Assisted trajectory as
  `assisted-motion-envelope-infeasible`;
- at the HOLD endpoint the ship was ~0.05 m from the reference at ~4.38 m/s,
  but zero reference speed plus opposite hull attitude produced a false
  ~179.5 degree course loss;
- recovery then rebuilt ApproachHold from almost the endpoint and generated a
  tiny reverse Assisted sample (`-0.5 m/s`), which was correctly rejected but
  should never have been requested.

Current candidate fixes:
- ApproachHold preparation is now COAST: zero translational acceleration,
  preserved VREL, bounded angular-rate damping only;
- async planning projects the start position by
  `startVelocityMps * PlanningLeadSeconds` so the program starts where the
  coasting ship will actually be;
- FinalIngress still deliberately uses `BrakeToStop`;
- Planner/Trajectory use 90% of the already reserve-reduced execution
  acceleration envelope, leaving headroom before AcceptedProgram validation;
- HOLD capture now uses physical stopping distance inside the accepted
  position envelope instead of the retired fixed speed threshold;
- a zero-speed SpatialCorridor reference has no translational course, so hull
  attitude no longer creates a false route-loss angle;
- entering FinalIngress resets the stage-local tracking failure budget;
- Assisted envelope rejection now logs page/sample/time and demanded vs
  available longitudinal/lateral acceleration.

Native Windows verification is still required.

## 2026-09-28 — first hard Planner/Autopilot/Traffic/Landing module split candidate

The first implementation slice of
`src/game/docking/DOCKING_INFRASTRUCTURE_ARCHITECTURE.md` is now on `main`.
This is an architecture candidate pending Windows compile/test evidence; it is
not yet live dispatcher/parking functionality.

New public navigation boundaries:
- `src/game/navigation/planner/RoutePlannerApi.h` exposes generic
  `RoutePlanRequest -> RoutePlan`;
- `src/game/navigation/autopilot/RouteFollowerApi.h` exposes route execution,
  page selection, reference diagnostics and attitude acquisition;
- production `GameServer` no longer includes or names
  `DockingAdvisoryPlanner`, `TrajectoryFollower`,
  `ManeuverTrackingController`, `ManeuverProgramSampler` or
  `ManeuverProgramTimeline`;
- manual `SpaceState` route planning also consumes `RoutePlannerApi`, not
  the legacy docking planner directly;
- `AcceptedManeuverProgram` remains the explicit immutable execution DTO
  allowed across Planner -> Autopilot. Private sampling/tracking state does not
  cross that boundary.

The current proven route geometry/control algorithms are intentionally retained
behind adapters:
- legacy `DockingAdvisoryPlanner` is a private RoutePlanner backend for this
  migration slice;
- legacy `TrajectoryFollower`/Sampler/Timeline/TrackingController are private
  RouteFollower backend components.
This preserves the user's successful SpatialCorridor flight while changing
ownership first.

New physically separate docking infrastructure:
- `src/game/docking/model/DockFacilityDescriptor.h`;
- `src/game/docking/traffic/DockTrafficControllerApi.h/.cpp`;
- `src/game/docking/landing/DockLandingControllerApi.h/.cpp`.
Traffic and Landing are compiled in their own `EliteDockingInfrastructure`
library with no navigation link dependency.

The first Traffic implementation already pins the critical policy seam:
- remote/far requests are provisional and cannot mutate the queue or obtain a
  queue token;
- controlled-horizon requests can obtain stable queue positions;
- physical fit cannot be overridden;
- normal class allocation is exact-class;
- emergency allocation may use a larger compatible class, never a smaller one.

The first Landing implementation is intentionally only the boundary/state
skeleton. A valid `LandingHandoff` enters terminal alignment and the public
contract forbids main-engine authority. RCS settle, gear/contact/latch physics
are not implemented yet.

New native/API and architecture gates:
- `route_planner_api`;
- `route_follower_api`;
- `docking_infrastructure_api`;
- `check_docking_module_boundaries.py`.
`verify_docking.sh` runs all of them.

Next evidence must be the Windows MinGW `verify_docking.sh` gate and standalone
`EliteGame` compile. Do not wire live Traffic/Landing into Automatic until
this boundary compiles cleanly.

## 2026-09-28 — docking architecture split fixed before further FinalIngress work

The long SpatialCorridor is now a successful reusable navigation primitive and
must remain separate from terminal landing. The next docking work is no longer
allowed to grow inside one Automatic docking state machine.

Authoritative design is now recorded in
`src/game/docking/DOCKING_INFRASTRUCTURE_ARCHITECTURE.md`.

Required ownership split:
- `RoutePlanner`: generic route geometry/dynamic profile only;
- `RouteFollower/Autopilot`: follows an accepted route and emits vehicle-level
  navigation intent;
- `DockLandingController`: separate terminal pad alignment, manoeuvring-thruster
  parking, landing gear, contact/latch and undock handoff;
- `DockTrafficController`: separate physical/class compatibility, pad
  allocation, arrival/departure queue and conflict-resource reservations.

These modules must be physically separated in source files/directories and
communicate only through explicit public DTO/API headers. Architecture-contract
tests must forbid private include/dependency leakage, hidden mutable cross-module
state, Planner access to queue/pad state, Autopilot access to landing/latch
state, and Landing/Traffic access to Planner/Follower internals.

Dock assignment has two independent filters:
1. physical fit is a hard geometry rule and can never be overridden;
2. S/M/L class allocation is policy. Emergency may assign a larger compatible
   dock, never a smaller one.

Long-range docking requests are provisional. A request made five minutes away
must not reserve a pad or queue position and block ships already near the
station. Hard queue admission occurs only inside a configurable controlled
approach horizon, preferably ETA-based. Queue order is based on arrival
readiness at that horizon, with explicit emergency priority.

Initial holding model deliberately avoids a complex free-form staging yard.
Each portal has one authoritative EntryHoldPoint/final-approach lane. The
dispatcher maintains a virtual queue; only one ship is admitted to that near
lane. Other queued ships remain outside it and may stop at a Planner-generated
safe staging point on their existing approach route until cleared forward.

This architecture supersedes treating "dock" as one mesh/box. A DockFacility is
semantic infrastructure: KinematicFrame, AccessPortal, EntryHoldPoint,
TrafficConflictZone, InternalLane and ParkingPad. Art/collision geometry may
later change completely without changing Planner/Autopilot/Traffic/Landing APIs.

No claim is made that this split is implemented yet. The next implementation
slice is API + architecture-contract separation before more terminal behavior is
added.

## 2026-09-28 — live spatial corridor strong; internal dock ingress candidate

Fresh standalone EliteGame visual evidence from the user is strongly positive for
the rewritten SpatialCorridor execution:
- ship remains inside the displayed authoritative corridor;
- turns are smooth;
- hull up/down orientation relative to the dock is maintained very well;
- the previous corridor-loss behavior is no longer the active problem.

One remaining presentation/control-quality issue is small continuous straight-line
"hunting". Root cause is the per-fixed-step look-ahead ray reacting to tiny
cross-track position noise even when the craft is already centered. The current
candidate adds a small center deadband: inside it Follower holds the exact current
segment tangent; outside it the existing look-ahead recovery remains active.
Route speed is unchanged and turn geometry is untouched.

The live run also confirmed that Automatic stops before entering the dock. Code
inspection shows this was intentional in the old runtime: FinalIngress ended at
an exterior pre-capture point and returned Human authority with
`pre-capture-envelope-complete`.

The dock itself also had mismatched visual/collision truth. The guidance OBJ has
a real 190x110 (cube) / 200x120 (cylinder) through aperture, but monolithic
`ObjectRuntimeHitBuilder` previously produced one solid whole-object OBB. The
candidate fixes the source:
- `IObjectDescriptor` can author logical collision boxes for monolithic objects;
- guidance docks author four wall OBBs around the visible opening;
- the shared HitComponent therefore exposes a real through passage to both
  navigation and collision consumers;
- docking semantic anchors now author `capture_depth_m` (450 m cube, 600 m
  cylinder), explicitly locating the internal capture point;
- FinalIngress is now HOLD -> exterior pre-capture -> entrance plane -> internal
  capture point;
- the exact shared hit volumes must prove the segment through the target. There
  is no target-object collision whitelist. A blocked opening fails as
  `final-capture-corridor-blocked`;
- successful FinalIngress now reports `capture-envelope-complete`, not exterior
  pre-capture completion.

This candidate intentionally does NOT yet add permanent dock latch/undock state.
The next live gate is physical ingress through the real aperture and stopping at
the internal capture point. Only after that evidence should authoritative latch
ownership be added.

## 2026-09-28 — compile cleanup after retired slowdown policy

Native Windows build exposed one stale validation reference in `ManeuverProgramSampler.cpp` to the already removed `spatialSlowdownStartFraction` field. That stale check is removed. The unused `finite(glm::dvec3)` helper in `TrajectoryFollower.cpp` was also removed. Code HEAD for this cleanup: `64e0a665b1fbcc5f8f20d9905674e6df140d2e25`. Windows gate must be rerun; candidate is still unaccepted pending native tests/build/live flight.

## 2026-09-28 — candidate: one authoritative Automatic corridor; Windows gate pending

The previous live observation `[DockAdvisory] left ... gate=4` exposed an
architectural ambiguity rather than proving that Follower had left its own
accepted route: the cockpit could still display a client-side
`DockingAdvisoryPlanner` result while Automatic stopped the real ship and ran
the server planner again. The visible tunnel and the executed route therefore
were allowed to differ.

The current `main` candidate removes that split for Automatic docking:

- `START DOCKING` no longer runs the client/manual advisory planner as a
  preflight. The client sends `BeginAutomaticDocking` directly.
- The server builds Automatic geometry once with `DockingAdvisoryPlanner`.
  `TrajectoryGenerator` receives that geometry with
  `pathGeometryAlreadyAuthored=true`; its role is dynamic feasibility and
  speed/orientation/acceleration compilation, not a second geometric route.
- The resulting `AcceptedManeuverProgram` is the execution source of truth.
  Its Hub-local points, course basis, speed and tracking tolerance are
  replicated in the per-session Automatic route snapshot.
- Automatic HUD/Hub-map guidance is built directly from that replicated
  AcceptedProgram. Sparse 500 m / terminal 250 m cockpit frames are only a
  presentation sampling of the same accepted route. During stabilization or
  replan, when no AcceptedProgram exists, the old tunnel is withdrawn instead
  of being displayed as stale truth.
- Spatial Follower no longer reduces route speed as cross-track error grows.
  The retired `spatialSlowdownStartFraction` policy has been removed.
  Follower keeps the accepted local speed and steers the hull/velocity target
  toward a bounded 50–250 m look-ahead point on the same ordered path.
- Steering angle and route-loss course angle are now separate values.
  Angular control follows the inward look-ahead ray, while route-loss checks
  compare the hull against the authored corridor tangent. A corrective turn
  therefore cannot invalidate the route merely because it is a large turn.
- Automatic docking no longer passes Follower commands through
  `NavigationRuntimeControlBridge/PilotSkillExecutor`. Follower writes the
  vehicle-level target velocity and acceleration demand directly into
  `ShipControlState`; Assisted/Newtonian flight law and ship physics remain
  the physical capability authority. PilotSkill remains available for NPC/human
  skill simulation, not the docking computer.
- Spatial progress remains ordered and position-driven through
  `ManeuverProgramSampler::sampleSpatial` plus the monotonic segment cursor;
  accepted samples/page boundaries cannot be skipped by elapsed time or by a
  globally nearest future segment.

Candidate code baseline before these documentation commits:
`f60e30ce2afb098856ef12f3f10dab463d2611b7`.

No Windows native build, `verify_docking.sh`, or live flight has yet been
observed on this candidate. Do not mark the slice accepted until those gates
pass and a live run shows the ship following the same corridor that HUD renders.

## 2026-09-27 — vehicle-motion candidate published; Windows gate pending

GitHub `forzub/Elite` main now contains the three docking/control/test changes,
merged without changing the intervening space-games research document. Remote
merge commit `c3b5dc2` has the same tree as the locally verified candidate.
The updated docking static gate and local native builder, sampler and runtime
control tests passed before publication. No Windows build or actual flight on
this revision has been observed. Reproduce the earlier 10.0222 m HOLD failure
and observe stabilization, final ingress and pre-capture before accepting the
runtime slice. The wider ten unrelated static-contract failures remain open.

## 2026-09-27 — audit of docking test contracts before Windows commands

The vehicle-motion rewrite is still a local candidate. Reviewed the docking
and navigation test sources for the retired stepProgram/throttle/actuator
interface. The broad navigation API-purity check still required the old
Planner-owned throttle schedule; its ownership document and assertion have
been corrected. `verify_docking.sh` now includes the accepted-program sampler
native test and the API-purity check. All static checks used by the updated docking gate now pass; script
syntax and git whitespace checks pass. The overlay check was corrected from
an obsolete comment to the actual preserved Hub-local route-start invariant. Previous local builder, sampler and
runtime-control native executables passed. No current Windows build or live
flight result exists.

A wider scan of 30 non-benchmark navigation/docking Python checks reports 10
red contracts in other subsystems (foundation docking marker, NPC, replication,
local avoidance, map, space, stage-12 compiler fixture, stress-field basis and
moving-gap documentation). None of those checks changed in this candidate;
the docking marker is also absent at the previous verified baseline. They are
not evidence that the new autopilot is working. The aggregate docking gate
still needs the user's Windows run. The local branch contains two commits
not present on origin/main; a previous push was rejected by automatic review.

## 2026-09-27 — candidate: vehicle-motion autopilot interface

The live docking failure at 10.0222 m and 0.995736 m/s included residual
angular-rate error 0.280841 rad/s. The previously committed spatial HOLD
capture and local speed keyframes have no Windows flight acceptance yet.

The new candidate removes engine/throttle commands from the live autopilot
boundary. Follower samples a reference velocity and full attitude; PilotSkill
filters acceleration and angular demand; ShipControlState carries a system-frame
velocity target; GameSimulation delegates propulsion to the ship flight law
using actual measured velocity. Assisted retains its real nose-coupled speed
controller and bounded lateral stabilizer. Newtonian applies the vehicle's
physical acceleration allocator. AcceptedManeuverProgram no longer stores
per-engine throttle segments; Newtonian acceptance checks body-axis motion
capability and rejects transverse route acceleration. HOLD capture still
requires a stopped fresh state before FinalIngress.

Linux G++ syntax checks, native builder/sampler/runtime-control tests, and
four architecture checks passed. This is a
candidate, not Windows native/live acceptance; physical dock contact and
in-plane terminal trim remain open.

## 2026-09-26 — live gate: freeze gone; propulsion entry mismatch fixed; manual route nose-first

Fresh Windows/live evidence:
- canonical client/server build succeeded;
- the restored docking tunnel remains visible;
- the old ~350 ms Automatic fixed-step/state-update freeze no longer appears on START DOCKING;
- Automatic still returned Human authority after a one-shot plan failure:
  `accepted-program-propulsion-program-infeasible`.

Propulsion diagnosis:
- AcceptedManeuverProgramBuilder decomposes every planned acceleration into the
  real forward/reverse main banks plus the real manoeuvre/RCS authority;
- Cobra's accepted-program lateral actuator is the physical 2 m/s² manoeuvre
  thruster, not the much larger Assisted velocity-stabilization controller;
- after angular kinematics became physical, the first trajectory samples could
  accelerate along the route while the authored body attitude still started at
  the arbitrary stopped hull attitude;
- that required impossible sideways RCS and the builder correctly rejected it.

Current Automatic fix:
- the first planned program declares a required route-entry attitude from the
  first advisory gate direction;
- roll is chosen by projecting the current up axis onto that forward plane;
- planned entry angular velocity is zero;
- the existing bounded `Aligning` phase physically rotates the real hull to
  this entry attitude;
- alignment consumes real time/state, so the stale program is discarded and
  Automatic stabilizes/replans from the now-aligned authoritative hull before
  execution;
- the physical propulsion feasibility gate remains strict. No Assisted-only
  lateral authority is smuggled into the executable actuator program.

Manual docking correction:
- DockingAdvisoryRequest now supports an optional initial-forward launch lead;
- manual planning feeds the real hull-forward axis transformed into the Hub
  local frame;
- the visible route starts 500 m ahead of the nose (or ten hull lengths,
  whichever is greater);
- if that exact ray is obstructed, Planner may shorten the lead but may not
  rotate it sideways; if even the minimum forward segment is blocked, planning
  fails explicitly with `initial forward corridor blocked`.

HUD:
- cockpit HUD now includes a fixed center boresight representing the hull/nose
  optical axis;
- it is deliberately independent of the velocity-vector/navigation marker.

Regression coverage now pins the nose-first manual corridor, Automatic
route-entry alignment/replan contract, and fixed screen-center boresight.

Fresh Windows native/build/live verification of these newest changes is pending.


# CURRENT STATE

## 2026-09-26 — Automatic freeze moved off fixed-step; angular program is now physical

Fresh live evidence:
- the restored docking tunnel is visible again;
- Automatic attempts still failed before movement with
  `accepted-program-angular-kinematics-infeasible`;
- each START DOCKING still produced a single ~365–370 ms state-update stall;
- after that failure the server correctly restored Human authority, which made
  the UI visibly return to manual guidance.

Freeze diagnosis:
- manual docking advisory already snapshots its inputs and executes
  `DockingAdvisoryPlanner::plan` on a worker thread;
- Automatic performed advisory planning + Ruckig trajectory generation +
  AcceptedManeuverProgram construction synchronously from
  `applyAutomaticDockingControls` in fixed-step.

Current fix:
- Automatic now snapshots immutable planning inputs after stabilization;
- heavy planning runs outside fixed-step in a worker;
- the ship stays stopped while the job is pending;
- the job plans against a short future execution epoch so the returned absolute
  maneuver clock is not stale when installed;
- fixed-step only polls the atomic result, installs it, then enters Aligning or
  Executing;
- old synchronous retry-loop remains forbidden.

Angular failure diagnosis:
- TrajectoryGenerator previously authored geometric quaternions while
  AcceptedManeuverProgramBuilder interpreted consecutive quaternions as a
  physical angular program;
- sharp geometric orientation changes could therefore imply omega/alpha beyond
  the real ship envelope even though translation was valid.

Current angular contract:
- Automatic supplies real initial hull forward/up and measured angular velocity;
- TrajectoryGenerator compiles orientation + omega together under vehicle
  max-angular-speed and max-angular-acceleration limits;
- rotating terminal omega remains an exact requested boundary;
- the trajectory marks its angular state as planner-authored;
- AcceptedManeuverProgramBuilder preserves authored omega and still independently
  checks angular feasibility.

UI:
- active map-card actions are now bright green; inactive actions retain the
  existing style. This applies generically to dock and ship card actions that
  already publish `active` state;
- cockpit docking mode text is state-driven rather than inferred from tunnel
  visibility;
- added localized AUTOMATIC DOCKING MODE text for en/ru/zh-Hans/es/ja.

New native regression:
`trajectory_generator_angular` checks a real initial attitude -> bounded turn ->
rotating terminal pose/omega and proves every adjacent sample stays within
configured omega/alpha limits.

Fresh Windows compile/live verification is pending.


## 2026-09-26 — Assisted accepted; tunnel/autopilot regression isolated and corrected

Fresh live result:
- Assisted handling is accepted by the user.
- manual docking guidance lost the previously successful visible tunnel;
- pressing START DOCKING removed the route, caused one heavy freeze, produced no
  ship motion, then logged
  `phase=plan-failed reason=accepted-program-build-failed action=restore-human`.

Corrections now on main:
- Automatic enables the same LocalGuidance/HudGuidanceCorridor presentation as
  manual Guidance;
- an already calculated docking corridor is retained across Autopilot takeover
  and Human hand-back instead of being deleted;
- AcceptedManeuverProgramBuilder exposes the exact rejection class;
- the rotating dock terminal angular velocity is now part of
  TrajectoryGenerationRequest;
- trajectory orientation is time-varying near a rotating terminal: it is
  propagated backwards from the exact capture pose using terminal omega, so
  the final AcceptedManeuverProgram no longer receives an instantaneous angular
  velocity jump at its last sample.

This specifically targets the observed accepted-program construction failure
without weakening angular feasibility checks. Fresh Windows build/native/live
verification is still required.


## 2026-09-26 — live test: Assisted course lag + Automatic fixed-step planner freeze fixed

Fresh live evidence exposed two production defects and one positive result.

Positive:
- manual `CALCULATE TRAJECTORY` produced a route, so the user-visible guidance
  planner path is alive.

Defect 1 — Assisted course lag:
- Assisted was still effectively main-first during a sharp hull turn.
- A large new forward-speed error could consume the complete shared linear-load
  envelope with main thrust, leaving zero authority for cancelling the old
  sideways VREL.
- This made FA-on feel Newtonian after attitude changes.

Fix:
- ordinary Assisted now treats velocity-direction coupling as the primary
  sharp-turn objective;
- manual keypad RCS remains a small real gas-limited actuator;
- automatic Assisted lateral stabilization removes old sideways VREL first;
- longitudinal main thrust consumes the remaining shared load envelope;
- no direct velocity rewrite was introduced;
- Cobra automatic Assisted lateral authority now uses the existing 7.5 g
  linear envelope;
- native contract added: a 90-degree course change from 100 m/s must converge
  within 3 seconds to <=5 degrees course error without collapsing into a stop.

Defect 2 — Automatic docking freeze:
- live log showed `phase=plan-retry` every ~0.5 s;
- each retry consumed ~280–300 ms in fixed simulation, causing the observed
  cyclic hub-map freeze and effectively frozen game field.

Fix:
- removed the fixed-step retry timer/state;
- a failed Automatic plan is now one-shot for that stabilized state;
- Planner records a concrete failure reason;
- server logs `phase=plan-failed reason=... action=restore-human`;
- Human authority is restored instead of synchronously hammering Planner.
- execution-time recoverable tracking failures may still perform a controlled
  stabilize/replan; if the subsequent plan itself fails, it also hands back
  rather than entering a retry storm.

Fresh Windows compile/tests for these changes are pending.



## 2026-09-26 — focused Automatic docking gates all PASS

Fresh target-machine evidence after correcting the stale runtime-control
fixture:
- canonical MinGW build PASS;
- `navigation_runtime_control` PASS;
- `maneuver_tracking_controller` PASS;
- `docking_advisory` PASS;
- `accepted_maneuver_program_builder` PASS;
- `verify_docking.sh` fully green.

The Automatic docking implementation is therefore accepted at the compile/native
contract level and is ready for live in-game validation. Live evidence is now
the gating item: stabilize -> optional align/replan -> execute accepted program
-> collision-free pre-capture completion -> Human handback.



## 2026-09-26 — navigation_runtime_control failure classified as stale test expectation

Fresh Windows failure:
`navigation_runtime_control: remaining vector must clamp at manoeuvre-thruster authority`.

Root cause is the test fixture, not production physics. The fixture set
`maxLinearGs = 1g` while also expecting a full 1g main-engine acceleration
PLUS 2 m/s^2 manoeuvre/RCS. Production correctly gives main priority and then
reduces secondary RCS so the combined acceleration remains inside the common
linear-load envelope.

The test is corrected to prove both valid cases separately:
- installed rear main = 5 m/s^2, leaving load headroom -> manoeuvre authority
  may reach the configured 2 m/s^2;
- rear main saturates the full 1g load envelope -> secondary manoeuvre demand
  must be reduced to zero.

No production DynamicMotionSystem behavior was changed for this failure.



## 2026-09-26 — Windows build PASS; automatic execution gate 3/4 PASS

Fresh target-machine evidence:
- canonical MinGW build PASS;
- `maneuver_tracking_controller` PASS;
- `docking_advisory` PASS;
- `accepted_maneuver_program_builder` PASS;
- `navigation_runtime_control` FAIL.

Therefore the new Automatic docking code is compile-integrated and the geometry,
accepted-program builder, angular tracking (including 180-degree correction) and
program paging gates are green. Automatic docking is NOT yet accepted as working,
because the live execution/control seam from accepted Follower output through
ShipControlState into physical propulsion still has one failing native contract.

The user-provided CTest summary does not include the assertion text from
`navigation_runtime_control`, so the exact defect cannot yet be classified as
production behavior vs stale test expectation. Do not change production
semantics by guess; rerun only that test with `--output-on-failure` and use the
first emitted assertion as the next fix target.



## 2026-09-26 — Automatic docking execution ownership closed; Windows compile gate pending

The Automatic docking slice has been tightened before target-machine build.

Implemented/fixed:
- Automatic no longer enters the solid target module under a planner-only collision exemption. Current navigation ends at a collision-free pre-capture center; physical contact/latch remains a separate later authority/state transition.
- Client and server Hub obstacle inflation now consume one shared `HubNavigationClearancePolicy`.
- `AcceptedManeuverProgramBuilder` derives angular velocity/acceleration across the full trajectory rather than resetting angular state at storage-page boundaries. Rotating-target terminal omega is explicit and capability-checked.
- `TrajectoryFollower` has one executable product only: `AcceptedManeuverProgram`; the old RuntimeLab `AcceptedShortSegment` can reach it only through the diagnostics-only adapter.
- Planner-owned actuator allocation is now carried explicitly:
  `AcceptedManeuverProgram -> Follower rear/fore/RCS + bounded feedback -> NavigationRuntimeControlBridge::stepProgram -> ShipControlState -> DynamicMotionSystem::applyNavigationActuatorProgram`.
  Physics no longer re-infers the nominal engine split from one net acceleration vector for accepted-program execution.
- Bounded Follower feedback may use only remaining main authority plus remaining manoeuvre/RCS authority. Normal speed/load/gas physics remains authoritative.
- `ManeuverTrackingController` attitude error now uses exact shortest-arc quaternion error, so a 180-degree hull mismatch does not collapse to zero correction.
- Automatic execution has a physical `Aligning` phase. If the stabilized hull is outside the first accepted attitude tolerance, Autopilot rotates the real ship, discards the now-stale program, stabilizes, and replans from the new state/time. The tracking envelope is not widened to mask the mismatch.
- Local prediction suppression waits for authoritative Autopilot ownership, so a rejected Automatic request cannot freeze pilot controls.
- Wire protocol remains version 11 for Automatic docking target identity; SimulationSnapshot schema remains version 10.

New/expanded gates:
- native `accepted_maneuver_program_builder`;
- native `navigation_runtime_control`;
- native `maneuver_tracking_controller`, including exact 180-degree attitude regression;
- static `check_automatic_docking.py`;
- static `check_navigation_live_runtime_control.py`;
- `verify_docking.sh` now builds/runs all of the above plus the manual advisory test.

Static source/token sweeps over current main pass. Full Windows/MinGW compilation is still pending and is the next required evidence.



## 2026-09-26 — Automatic docking production execution slice wired; Windows gate pending

Automatic docking is no longer a disabled UI stub.

Implemented production ownership/execution path:
`START DOCKING -> DockingRouteRequest::Automatic -> ClientShipCommand::BeginAutomaticDocking -> GameServer Autopilot authority -> BrakeToStop/stabilize -> server-side DockingAdvisory geometry + TrajectoryGenerator -> AcceptedManeuverProgramBuilder -> TrajectoryFollower -> NavigationFrameBoundary -> NavigationRuntimeControlBridge -> ShipControlState -> shared physics`.

Important ownership changes:
- `TrajectoryFollower` now has exactly one executable input type:
  `AcceptedManeuverProgram`. The old `AcceptedShortSegment` overload was removed.
- The old Stage-12 runtime lab may still produce `AcceptedShortSegment`, but that historical product is isolated behind
  `NavigationRuntimeLabAcceptedProgramAdapter`; it cannot enter Follower directly.
- Manual docking preparation and Automatic docking are mutually exclusive server control lifetimes.
- Disconnect/reset clears both docking runtimes.
- Automatic request identity carries system/module/semantic-anchor across wire protocol version 11.
- Client only requests/cancels and observes authoritative Autopilot ownership; it does not own an executable automatic route.
- Local prediction is suppressed only after the server snapshot confirms Autopilot, so a rejected request cannot freeze human input.

Physical additions:
- `AcceptedManeuverProgramBuilder` converts a collision-checked trajectory into immutable program pages, compiles actuator intervals, reserves explicit tracking authority, derives angular kinematics, and rejects angular capability violations.
- rotating docking targets supply terminal angular velocity; the current diagnostic cube's local spin therefore cannot be ignored.
- new native gate: `accepted_maneuver_program_builder`.
- new static gate: `check_automatic_docking.py`.
- `verify_docking.sh` now gates manual advisory + automatic accepted-program execution.
- `verify_modes.sh` now includes the wire protocol round-trip because ClientShipCommand changed.

The earlier SystemMapRenderer `m_mode` compile regression is now guarded by `check_mode_state.py`, which scans both renderer inline implementation files for the retired field.

No Windows/MinGW build has yet validated this Automatic slice.


## 2026-09-26 — canonical game build exposed stale coordinate acceptance harness; corrected

After the focused mode-state gate, the canonical game build reached
`ClientAcceptanceHarness.cpp` and failed because
`testCoordinateDisplayHotkeyContract()` still called the removed
`CoordinateDisplayService::cycle()` API.

This was stale acceptance code, not a production coordinate-mode regression.
Production already routes Ctrl+F11 through
`Application::cycleCoordinateDisplayFormat()`:
`ClientModeState` owns `coordinateDisplayFormatId`,
`nextCoordinateDisplayFormat()` selects the next value, and
`CoordinateDisplayService::setFormat()` only projects that selected state.

The harness now verifies the same ownership rule:
`ClientModeState transition -> CoordinateDisplayService projection`, including
three transitions, wraparound, and state revision increments. The removed
service-owned `cycle()` was not restored.

Fresh Windows verification/build is pending.


## 2026-09-26 — mode-state refactor implemented; first Windows gate found only a test expectation error

Mode/state refactor now present on main:
- one shared default local flight law, Assisted;
- persistent flight-law/alignment/Assisted-target transitions are owned by
  `LocalFlightControlStateMachine`; `DynamicMotionSystem`,
  `SharedShipPhysics`, and `ShipController` no longer write those mode
  fields directly;
- Assisted entry captures longitudinal speed instead of total |VREL|;
- neutral angular damping is Assisted doctrine; Newtonian neutral rotation
  preserves angular inertia unless an explicit alignment state owns attitude;
- Assisted automatic lateral stabilization is a separate capability/state from
  the pilot gas-limited RCS. Cobra uses the configured aggregate
  `strafeAccel` capability, bounded by the common ship load envelope;
- the new Assisted stabilizer state is replicated; SimulationSnapshot wire
  schema is now version 10;
- global client modes (UI locale, constellation visibility, sky culture,
  coordinate format) are owned by `ClientModeState` and persisted as
  projections;
- coordinate formatter/renderer no longer owns a hidden coordinate mode;
- Galaxy/System/Detail/Hub renderer submode is now owned by `MapModeState`.

First `verify_modes.sh` native run:
- client preferences contract PASS;
- local-flight contract failed only because its new lateral-stabilizer test
  expected raw 20.0 m/s² while `makeParams()` has maxGs=2, so the production
  capability correctly returned 19.6133 m/s² (2g).
The test now expects
`assistedLateralStabilizationAccelerationLimitMps2(params)`, i.e. the same
central load-bounded capability as production.

Wire-data static contract was also updated from schema 9 to 10 and now requires
`assistedStabilizationAccelerationMps2`. Fresh Windows rerun is pending.

## 2026-09-26 — mode/state ownership refactor implemented; Windows verification pending

The requested mode-switch audit found and corrected several independent hidden
state paths.

### Local flight
- Assisted is the single shared default via
  `defaultLocalFlightControlLaw()`; DynamicMotionState, ShipControlState and
  the client pending command latch no longer carry copied literal defaults.
- Persistent flight transitions are owned by
  `LocalFlightControlStateMachine`.
- SharedShipPhysics, DynamicMotionSystem and ShipController are forbidden by
  static contract from directly writing `localControlLaw`,
  `velocityAlignmentMode`, `assistedTargetSpeedHold` or
  `assistedThrottleTrimWasActive`.
- Assisted entry captures longitudinal forward VREL, not total velocity
  magnitude, so Newtonian side-slip is not reinterpreted as a new forward
  setpoint.
- Neutral angular damping is now doctrine-specific: Assisted damps released
  rotation; Newtonian preserves angular inertia unless an explicit
  alignment/autobrake state owns attitude.
- Assisted lateral velocity stabilization now has a separate explicit actuator
  budget `assistedStabilizationAccelerationMps2`, sourced through central
  ShipDynamics from `strafeAccel`. Manual keypad RCS remains the separate
  gas-limited `manoeuvreThrusterAccel` path.
- The new DynamicMotionState field is replicated; simulation snapshot wire
  schema was bumped from 9 to 10.
- Native regression proves identical side-slip is actively cancelled in
  Assisted but preserved in Newtonian, without consuming keypad RCS gas.

### Global client modes
`ClientModeState` is authoritative for:
- UI locale;
- constellation visibility;
- sky-culture/constellation type;
- coordinate display format.

Preferences are storage projections only. Localization, SceneRenderer and
CoordinateDisplayService are projections/formatters; renderer/services no
longer own hidden transitions. SystemMapRenderer no longer overwrites the
coordinate format during init.

### Map submode
Galaxy/System/Detail/Hub selection is now owned by
`game::system_map::MapModeState`. SystemMapRenderer keeps `setMode()` only
as the transition/side-effect API and reads the selected mode from the state.
The old loose `Mode m_mode` field was removed.

Root `verify_modes.sh` now configures the standalone architecture-contract
build, runs native local-flight/client-preference contracts and the relevant
static state/localization checks.

No Windows compile/test result is claimed yet.

## 2026-09-26 — Automatic docking status: execution components exist, production owner is missing

Current main still does NOT provide working automatic docking.

Existing production-capable pieces:
- `DockingRouteRequest::Mode::Automatic` exists;
- server control registry can transfer the player ship to
  `ControllerKind::Autopilot`;
- `AcceptedManeuverProgram`, `TrajectoryFollower`,
  `NavigationRuntimeControlBridge`, and the explicit
  `ShipControlState::navigationAccelerationDemand...` seam exist;
- shared ship physics remains the authority that applies real vehicle limits.

Missing production integration:
- `SystemMapRenderer` hard-rejects `start_docking`;
- `SpaceState::updateDockingAdvisory()` processes Guidance only and clears any
  Automatic request;
- server docking ownership currently exists only for the temporary
  BrakeToStop preparation phase;
- after route publication `finishDockingGuidancePreparation()` restores Human
  ownership;
- no server-owned player lifetime currently stores/advances an accepted maneuver
  program through TrajectoryFollower -> NavigationRuntimeControlBridge every
  simulation step.

Therefore automatic docking is blocked by one architectural seam, not by lack of
planner/follower primitives. After the current manual docking route gate passes,
the next implementation milestone is explicitly the server-owned accepted-program
executor, followed by wiring Automatic request/UI and live docking acceptance.

## 2026-09-25 — native docking gate exposed boundary-join defect; clearance retreat implemented

Fresh standalone native evidence reached the real planner and failed the
far-preferred-axis regression:
`no collision-free geometric path shortened=1 final_axis_m=8390`.

This proves soft-axis shortening itself worked, but the accepted join point was
the last epsilon-clear point immediately adjacent to the inflated obstacle.
That endpoint is geometrically legal for the one-dimensional axis-clear probe
yet is a poor visibility-graph goal: support-node chords can graze/intersect the
same obstacle and leave the graph unable to connect to the goal.

Current main fixes that boundary mismatch:
- after binary-searching the longest clear final-axis prefix, the join retreats
  at least `max(50 m, 4 * hullRadius)` into already-clear space;
- if nominal geometric search still cannot reach that shortened join, Planner
  progressively retreats the join farther toward the mandatory ingress
  (100, 200, 400 ... m) and retries before declaring no route;
- mandatory close-in ingress remains the lower bound and retains its hard
  collision check;
- regression now requires at least 25 m actual clearance between the shortened
  join and the inflated far-axis blocker.

This is a route-recovery fix, not a relaxation of collision geometry.
Fresh Windows verification through `bash verify_docking.sh` is pending.

## 2026-09-25 — repeated stale verification output isolated; canonical verify script added

The repeated user output again showed:
- Ninja searching for `docking_advisory_tests` in canonical `build/`;
- CTest running against canonical `build/`;
- the pre-fix static-check message
  `preferred full docking-axis lead became a hard failure again`.

That combination proves the executed verification path was still stale; it was
not the corrected standalone navigation-runtime gate.

To remove command ambiguity, public main now contains root entrypoint
`verify_docking.sh`. It always:
1. configures `tests/navigation_runtime` into
   `build/tests/navigation_runtime`;
2. builds target `docking_advisory_tests` there;
3. runs CTest `docking_advisory` from that same tree;
4. runs the static manual-docking checker.

The static checker now prints revision
`20260925-soft-axis-v2` on PASS. Seeing that marker proves the working copy is
new enough to contain the corrected soft-axis semantics.

No new planner acceptance evidence exists yet; rerun through
`bash verify_docking.sh` after pulling current main.

## 2026-09-25 — verification failure was two gate/tooling defects, not evidence that route fix failed

The first verification attempt did not execute the docking native test:
- `cmake --build build --target docking_advisory_tests` failed because the
  canonical game build directory does not configure `tests/navigation_runtime`;
- `ctest --test-dir build` therefore reported no tests.

The target does exist in the standalone navigation-runtime CMake project:
`tests/navigation_runtime/CMakeLists.txt` defines executable
`docking_advisory_tests` and CTest name `docking_advisory`.
The correct build tree is now explicitly
`build/tests/navigation_runtime`.

The static manual-docking checker also produced a false positive. It rejected
any source occurrence of `if (!clear(align,stop))`, but current Planner uses
that probe to detect obstruction of the soft 9 km preference and then shorten
the accepted lead. The checker now rejects only the retired hard-failure
control flow / `dock alignment blocked` failure and separately requires both
`terminalApproachShortened=true` and the dedicated
`dock mandatory ingress blocked` close-in failure.

No native result from the user's failed command sequence is accepted as evidence
for or against the planner fix. Fresh standalone test configure/build/run is
required.

## 2026-09-25 — live 9 km final-axis regression fixed: preferred lead is no longer hard geometry

Latest live run failed three requests with:
`[DockAdvisory] ... failed=dock alignment blocked`.

Root cause was introduced by the broad Assisted slice: the planner made the full
preferred 9000 m docking-axis lead a semantic hard segment and called
`clear(align, stop)` before any reroute search. Therefore any unrelated
obstacle anywhere on the 9 km ray cancelled the route before geometric planning
could begin.

Current main fixes the ownership/semantics:
- mandatory near-port ingress is only `max(700 m, 3 * standoff)`;
- that short ingress remains hard geometry and may legitimately reject docking;
- the requested 9000 m Assisted lead is preferred geometry only;
- if the preferred lead is obstructed farther out, Planner binary-searches the
  longest collision-free prefix of the docking axis, records
  `terminalApproachShortened=true`, and continues route/reroute selection;
- preferred 6000 m terminal radius is still attempted first; if the shortened
  available axis cannot fit it, normal reroute-before-tighten policy applies.

Live route diagnostics now add:
`final_axis_m=<accepted>` and `final_axis_shortened=0|1`.

Regression coverage now distinguishes:
1. obstacle on far preferred 9 km lead -> valid route, shortened final axis;
2. obstacle in mandatory near-port ingress -> real
   `dock mandatory ingress blocked` failure.

Fresh Windows/native/live evidence for this fix is pending.

## 2026-09-25 — broad Assisted docking/default/recovery slice implemented; Windows gate pending

The new live requirement from the station-map screenshot is now implemented on
public main.

Manual Assisted guidance profile is deliberately much broader:
- final docking-axis lead: 9000 m;
- terminal fillet segment fraction: 0.85;
- preferred terminal turn radius: 6000 m.

These values remain preferences, not route-existence floors. The existing
reroute-before-tighten sequence remains authoritative: nominal broad arc,
alternate pre-alignment ingress directions with full obstacle search,
expanded-clearance/full-obstacle reroute, and only then a tighter collision-free
arc. The accepted plan still reports detour/radius-relaxation diagnostics.

Manual tunnel lifetime is now materially more forgiving without changing the
nominal tunnel display:
- nominal transit tolerance remains 60 m and still drives warning/critical HUD;
- release cross-section is nominal + max(100% of nominal, 30 m), so the normal
  60 m transit release bound is 120 m;
- sustained outside-release grace is 1.00 s instead of 0.35 s;
- longitudinal release receives the same 100% / 30 m expansion rule.

Fresh flight state now defaults to Assisted across authoritative motion,
ShipControlState defaults and client discrete-law latches. Newtonian remains
explicitly selectable.

DockPrep Assisted stop semantics are regression-locked: target VREL becomes zero
immediately; a healthy fore/reverse main bank is commanded at full physical
authority without a 180-degree hull flip. The server begin diagnostic now prints
the active law by name plus forward/reverse main acceleration authority. This is
needed because the reported slow live stop may have been the previous Newtonian
default rather than the Assisted stop controller.

Native/static tests were updated for the 6 km curve, 120 m release band,
1.00 s grace, Assisted default and full-authority Assisted stop. Fresh Windows
compile/runtime evidence is still pending; no acceptance is claimed yet.

## 2026-09-25 — live screenshot rejects current arc; Assisted becomes default and stop semantics are tightened

Fresh live visual evidence shows the currently published manual docking turn is
still far too sharp to be useful. The accepted requirement changes again:
- manual Assisted must prefer a substantially broader station/final turn than
  the current 3000 m / 1500 m profile;
- if broad geometry is obstructed, Planner must reroute first and then reduce
  radius only as much as necessary; route existence must not be confused with
  one preferred arc candidate;
- manual guidance lifetime must tolerate a materially larger excursion outside
  the nominal tunnel before the route is cancelled.

The current authoritative default local control law is still Newtonian in
DynamicMotionState. This explains why a fresh ship/session can enter DockPrep
with Newtonian END semantics unless the player explicitly switches mode.
Assisted is now the requested default law.

The existing Assisted BrakeToStop implementation already sets target VREL to
zero immediately and, with a healthy fore main bank, commands real reverse-main
braking without a hull flip. Therefore the live report of very slow takeover
deceleration must be distinguished from the intended Assisted physics. The next
slice will make Assisted the default and add stronger regression/diagnostics so
DockPrep proves which law is active and that healthy Assisted braking uses the
fore main at full physical authority rather than a slow throttle ramp.

## 2026-09-25 — manual docking now reroutes before tightening the terminal arc

The previous Assisted rule was wrong: a requested 1500 m terminal radius was
treated as a hard route-validity floor. If obstacle clearance or segment room
forced a smaller circular fillet, the whole manual navigation task was cancelled.

Current main changes the semantics:
- Assisted still requests a 3000 m final docking-axis approach, 0.75 terminal
  fillet fraction and a preferred 1500 m human-flyable terminal radius;
- the preferred radius is not a task-failure threshold;
- Planner first tries the nominal route at the preferred radius;
- if that candidate fails, Planner samples 12 alternate pre-alignment ingress
  directions around the final docking axis; each candidate uses the full
  obstacle set, so it may route far around station/structure geometry;
- if those terminal-direction candidates still fail, Planner retries the
  original topology with expanded obstacle clearance and the full obstacle set;
- only after preferred-radius rerouting is exhausted may the circular terminal
  turn shrink; the fallback starts from the preferred/dynamic radius and
  tightens only as clearance requires;
- failure is now reserved for the case where no collision-free rounded docking
  route can be produced, or the mandatory final docking-axis ingress itself is
  blocked.

DockingAdvisoryPlan now reports whether a detour was used, whether terminal
radius was relaxed and the accepted terminal radius. SpaceState logs these
values. Native regression coverage now contains both required cases:
1. an obstacle that blocks the preferred arc must produce a detour while
   retaining the preferred radius;
2. a final-axis geometry that physically cannot fit 1500 m must still return a
   valid tighter arc instead of cancelling the task.

Fresh Windows compilation/runtime evidence for this new policy is pending.

Automatic docking remains a separate execution boundary. The repository already
has DockingRouteRequest::Mode::Automatic, AcceptedManeuverProgram,
TrajectoryFollower and NavigationRuntimeControlBridge, but the player docking
production path does not yet own a server-side accepted-program executor.
SystemMapRenderer therefore still keeps START DOCKING disabled and SpaceState
still admits Guidance only. The temporary server Autopilot currently performs
only BrakeToStop preparation and restores Human authority afterwards. Do not
replace this missing layer with visual-gate chasing.

## 2026-09-25 — second final-density gate exposed sparse-step boundary crossing

Fresh Windows `docking_advisory` still failed the terminal density contract,
now with an exact 500 m interval:
`terminal advisory gate spacing too sparse: 500`.
The static manual-docking check continued to pass.

The previous "activate at 2250 m" rule was insufficient because cadence was
selected from the current frame. A current sparse frame at e.g. 2300 m could
still take a full 500 m step to 1800 m, crossing the activation boundary in one
jump.

The planner now treats the density boundary as an explicit anchor. While still
in sparse mode, if the next nominal 500 m step would cross
`terminalDenseDistance + terminalSpacing`, that specific interval is shortened
to the distance-to-boundary. Once the boundary frame is reached, all following
published intervals use terminal spacing. This removes the entire sparse-step
crossing class rather than shifting the threshold again. Fresh Windows rerun
is pending.

## 2026-09-25 — final-2-km density gate exposed one boundary-transition defect

Fresh Windows docking_advisory failed only the new terminal-density assertion:
`terminal advisory gate spacing too sparse: 490`.
The static manual-docking architecture check passed.

Root cause: published-frame compression switched to 250 m by inspecting the
candidate endpoint. A last sparse ~500 m chord could therefore cross the 2 km
boundary and land inside the dense zone before the shorter cadence activated.

The planner now activates terminal density one terminal interval early:
`2000 m + 250 m`. This guarantees the 2 km transition is bracketed by frames
no farther than the terminal spacing and prevents a 500 m chord from entering
the final zone. The regression keeps the strict <=250 m requirement inside the
last 2 km and also requires a frame to anchor the transition within one terminal
spacing. Fresh Windows rerun is pending.

## 2026-09-25 — live manual guidance works; turn geometry/visibility refined

Fresh Windows live evidence shows repeated guidance requests are active and
route cancellation now occurs on measured corridor departure rather than the
old dock-axis defect:
- request 1, gate 2: lateral -82.8289 m, vertical -59.6895 m against 75/75 m
  release bounds;
- request 3, gate 17: lateral 77.9429 m against 75 m release bound;
- request 4, gate 2: vertical -62.1068 m against a 61.1359 m release bound.

This is consistent with the user's observation that the sparse 500 m display
frames make the station turn hard to read/follow.

DockingAdvisoryPlanner has therefore been refined:
- geometric corner smoothing is now a true circular fillet, not a quadratic
  Bezier approximation;
- desired turn radius starts from v^2/a lateral capability, shrinks only when
  adjacent segments cannot contain it, and downstream speed limits remain
  responsible for a physically feasible smaller-radius turn;
- ordinary published frame spacing remains 500 m;
- within the final 2000 m, published frame spacing becomes 250 m;
- display-gate compression now uses along-route progress rather than straight
  chord length so curved geometry is not visually collapsed.

The preparation stop remains an authoritative gate: planning cannot start until
replicated Hub-relative speed is <= max(0.05 m/s, ship stop epsilon), angular
rate <=0.01 rad/s, continuously for 0.25 s. New diagnostics now print initial
DockPrep VREL/omega and the accepted settled VREL/omega so the next live run can
verify this numerically.

Full automatic docking is not yet connected end-to-end. The current
DockingRouteRequest has Automatic mode, but SpaceState's active docking path
still accepts Guidance only. SHOW ROUTE does already exercise temporary
Autopilot ownership for the physical stop phase.

## 2026-09-25 — canonical Windows game build hit a docking-header macro collision

After the focused navigation/propulsion gates passed, the canonical MinGW64
game build reached `EliteGame` compilation and failed in
`DockingAdvisoryCorridor.h` at `const auto near = ...`. This is a
Windows-header compatibility defect: `near` is not a safe identifier once the
Windows include stack has been processed. The same header compiled in narrower
test targets because that macro environment was absent.

The local helper has been renamed to `nearBoundary`. The manual docking
architecture check now explicitly rejects reintroduction of
`const auto near =` in the corridor header.

The winsock2 warning shown in the same build is non-fatal and unrelated to this
failure. A fresh canonical Windows game build is now the next gate.

## 2026-09-25 — dual-main target gates PASS; Newtonian runtime fallback completed

Fresh Windows MinGW64 evidence on `bb5ff1f8`:
- `local_flight_control_contracts`: PASS;
- `ordinary_physical_maneuver_compiler`: PASS;
- `ship_propulsion_state`: PASS;
- `docking_advisory`: PASS.
The three navigation-runtime gates were repeated and passed again with no rebuild
work, confirming deterministic native test behavior for this slice.

That acceptance exposed one remaining implementation asymmetry below Planner:
`ShipController` and B5 already selected the fore/nose main bank when the
aft/rear bank failed, but Newtonian `DynamicMotionSystem` BrakeToStop still
burned only the aft bank and ordinary Newtonian '+' still addressed only aft
main. This is now corrected on `main`:
- if aft main is alive, Newtonian behavior is unchanged;
- if aft main is dead and fore main survives, '+' produces fore-main thrust
  along `-hullForward`;
- BrakeToStop waits for nose-with-velocity attitude and then burns fore main
  opposite the nose to decelerate;
- Newtonian '-' remains a no-op and does not become a synthetic reverse throttle.

A native regression test and static architecture tokens now lock this fallback.
Fresh Windows verification of this final runtime completion is pending.

## 2026-09-25 — focused local-flight rerun blocked by test compile include

The next Windows rerun did not execute the updated local-flight contract binary:
compilation stopped because `LocalFlightControlContractTests.cpp` referenced
`game::ship::mainAccelerationLimitMps2()` without including
`src/game/ship/core/ShipDynamics.h`.

The subsequent CTest line repeated the old
`Assisted controller exceeded ship maxGs envelope` result because Ninja had
failed and the previously built executable remained in the build directory.
That repeated runtime failure is stale evidence and must not be attributed to
the current source.

`check_local_flight_control.py` passed and
`check_manual_docking_advisory.py` passed on this target. The missing include
is now fixed on public `main`; rebuild the local-flight target before drawing
any conclusion about the combined main+RCS envelope correction.

## 2026-09-24 — first dual-main Windows gate found two focused defects

Target MinGW64 evidence:
- `json_numeric_locale` passed;
- `local_flight_control_contracts` failed with
  `Assisted controller exceeded ship maxGs envelope`;
- `check_local_flight_control.py` passed;
- `check_manual_docking_advisory.py` failed with
  `speed label is still tied to arbitrary corner[1]`.

The flight failure was a vector-composition defect, not a reason to derate the
main engine. Assisted could command a main-engine acceleration already at the
linear envelope and then add perpendicular RCS, making the total vector slightly
larger than the allowed envelope. The correction keeps selected main thrust
authoritative and clips only secondary RCS so `|main + RCS|` remains inside
the shared linear limit.

The docking Python failure was a checker false positive. The actual speed label
already uses `projectedUpperLeft(projected.corners)`; the generic substring
check matched `projected.corners[1]` inside the separate semantic dock-bottom
marker. The check now inspects only the speed-label placement block and requires
the stable projected-frame anchor.

These corrections are on public `main`; target rerun is pending.

## 2026-09-24 — Cobra dual-main propulsion and failure fallback implemented

The authoritative Cobra descriptor now installs both longitudinal main-engine
banks: aft/rear thrust along ship forward and fore/nose thrust opposite ship
forward, each rated at the existing 7.5 g linear envelope. The fore bank is
represented by explicit damageable module identities and both banks are bound
through `ShipDescriptor::mainPropulsion`.

Runtime propulsion is binary. A bank is either operational at its full
descriptor authority or failed at zero; health is not converted into partial
main-engine thrust. Server physics, runtime navigation and client docking
planning derive effective `ShipParams` from current module state. Residual RCS
authority remains separate and cannot masquerade as a surviving main engine.

Assisted now uses a healthy fore main for ordinary nose-first braking. If the
fore bank fails, strong braking falls back to hull flip + aft main. If the aft
bank fails while the fore bank survives, Assisted reverses its working travel
direction and B5 may compile the fore bank as the primary main engine, including
the required hull attitude/time proof. The physical maneuver compiler now
supports Assisted and keeps explicit forward/reverse main authority separate
from body-axis/RCS authority.

Focused regressions were added for binary propulsion state, healthy Assisted
fore-main braking, fore-bank failure fallback and aft-bank failure reverse
working direction. These changes are published on `main` but have not yet
been compiled or run on the user's Windows target. Existing untracked trace
JSON/TXT files must be preserved.

## 2026-09-24 — live corridor gate isolates policy and Assisted-stop defects

The current Windows run reaches an active manual docking route and reports
`request=8`, then cancels at gate 12 with lateral 27.1992 m and vertical
60.1343 m against a 60/60 m nominal transit box. The old dock-axis failure is
not present in the supplied tail. A 0.1343 m nominal-bound excess is too brittle
as a one-sample route-destruction condition.

The implemented slice addresses the user's seven findings. Assisted
BrakeToStop now acquires tail-to-velocity attitude when no physical reverse
main exists, then uses the installed aft main; its stop demand targets the
fixed-step delta-v but remains actuator-clamped. Manual corridor tracking now
has nominal, warning/critical and release states: warning begins at 80% nominal,
release expands each lateral/vertical bound by max(25%,10 m), and cancellation
requires 0.35 s continuously outside release.

HUD docking frames now blink during warning, speed labels are limited to gates
within 500 m, displayed extent is explicitly ship extent plus twice the center
tolerance, and an outward marker on the semantic -up edge identifies dock
bottom. Native/static regression coverage was updated; Windows compilation and
live acceptance of this slice are pending.

## 2026-09-24 — target still runs pre-fix docking guard; retest published Hub-local fix

Latest target evidence is from Windows checkout `D:\\__elite\\work` at
`84d59f4d`. Its `build/EliteGame.exe` lacks the corrected
`[DockAdvisory] axis request=` marker and emits only the legacy
`failed=dock moved off approach axis` line after briefly showing the route.
That run therefore does **not** test the published Hub-local correction.

The verified corrected code baseline is commit `512b917c`; subsequent
documentation commit `046ba6e6` records publication. The corrected runtime
keeps the terminal gate and semantic dock/standoff in one tactical Hub-local
frame and emits a numeric axis delta before any axis cancellation. No new
navigation-math change is justified until the Windows target pulls, rebuilds,
proves the marker is in the executable, and reruns the live gate.

Next evidence must distinguish three outcomes: route persists for at least
45 s with server-confirmed Human hand-back; numeric axis/corridor diagnostics
identify a remaining live defect; or deployment/build provenance is still
stale. Preserve all existing untracked trace JSON/TXT files.

## 2026-09-24 — docking and JSON startup fixes published on GitHub

User explicitly authorized publication to public `forzub/Elite` `origin/main`.
The command-line `git push` could not authenticate in the execution container;
the authenticated GitHub integration instead published the exact verified
local file tree as commit `512b917c8d09bc22b479a1c82203bd6b003b7ccf`.
Read-only Git remote verification confirmed `refs/heads/main` points to that
commit. The local branch now tracks it, with its previous multi-commit history
retained under `elite-local-history`. Earlier sections describing blocked
publication are historical. Windows can now `git pull --ff-only origin main`
from `84d59f4d` without disturbing unrelated untracked trace files.
Focused docking, test-source and JSON numeric-locale checks passed locally;
the target build and live route/Human hand-back gates remain open.


## 2026-09-24 — direct GitHub publication requested, auto-review still blocks public main

The user explicitly rejected patch-file delivery and asked for code directly
on GitHub plus `git pull` commands. Read-only `git ls-remote origin
refs/heads/main` confirmed public `main` remains `84d59f4d`. A direct
`git push origin main` was again rejected by automatic approval review: it
requires explicit authorization of the exact public-main push, beyond the
user's request to apply code in git. No push or indirect publication occurred.
The corrected code remains local; focused docking, test-source and JSON-locale
checks pass. Await explicit authorization to publish the local commits to
`origin/main`. Do not claim `git pull` downloads these fixes yet.


## 2026-09-24 — target proven to run the unpatched docking executable

Target-machine evidence is decisive: checkout `D:\\__elite\\work` is
`84d59f4d` (`main`, `origin/main`), the `build/EliteGame.exe` does not contain
`[DockAdvisory] axis request=`, and its complete searched log contains only
`request=1 failed=dock moved off approach axis`. The observed disappearance
therefore belongs to the old tangent-extrapolation guard, not to the corrected
Hub-local implementation. The user has several untracked trace files; do not
delete or overwrite them. Apply the full mailbox patch based on `84d59f4`
with `git am`, rebuild the canonical executable, verify its axis marker, then
rerun the flight gate. Corrected-code live acceptance remains open.


## 2026-09-24 — repeated target route removal still lacks numeric axis record

The next Windows excerpt shows `[DockAdvisory] request=1 failed=dock moved off
approach axis` after the route briefly appeared. No `[DockAdvisory] axis`
measurement or `[Startup] LC_NUMERIC=C` was included in the excerpt. The
corrected guard writes axis measurements to `std::cerr` immediately before
that same failure, and the canonical build script compiles `build/EliteGame.exe`
from the checkout used to invoke it. An excerpt is not a complete log; the
remaining decisive checks are the target `git log`, presence of the marker in
the executable, and unfiltered `build/test-logs/docking-live.log`. Do not
change the 2 m axis threshold before obtaining those results. Human hand-back
and sustained route persistence remain unproved.


## 2026-09-24 — Windows startup JSON assertion after docking patch

New target run aborted in `nlohmann/detail/input/lexer.hpp` numeric conversion
with `endptr == token_buffer.data() + token_buffer.size()` while starting a
session. `src/main.cpp` previously applied the user's `LC_ALL` locale; on a
decimal-comma Windows locale, the JSON lexer accepts `1.25` but the C runtime
`strtod` consumes only `1`, triggering precisely this assertion. A preceding
`session-start-update` duration line marks a completed slow phase, not a
proven crash stack. Chromium's window-unregister message is a shutdown
consequence, not the JSON cause.

Startup now keeps the user's text locale but explicitly restores `LC_NUMERIC=C`
before constructing `Application` or starting worker threads. It logs the
numeric locale. A new native regression tries comma-decimal locales where
installed and verifies nlohmann parses a fractional and exponential JSON
number after the startup normalization. It passes locally, along with the
authentication-admission, test-source and manual-docking static checks. The
Windows runtime rerun and docking flight gate are pending. Remote publication
remains blocked by the prior automatic approval rejection; update the local
patch for target installation.


## 2026-09-24 — local docking fix complete; remote push blocked by review

The user requested applying the patch in the repository and requested
download/build/run commands. Local `main` contains the corrected docking
implementation through `90661a5`; focused docking and test-source architecture
checks passed and the worktree was clean. A direct `git push origin main` was
rejected by automatic approval review because it would publish multiple
commits to the public default branch without an explicit authorization of
that exact remote action. No alternate publication path is permitted. Remote
`main` still tracks `84d59f4` in this checkout. Deliver a local `git am`
patch and commands now; request explicit approval for remote public-main
publication if the user wants ordinary `git pull` to carry the changes.


## 2026-09-24 — repeated live axis failure: deployed-version check first

The new supplied live excerpt again shows only `request=N failed=dock moved
off approach axis`. The published `origin/main` source at `84d59f4` emits
exactly that line without a preceding measurement. The local corrected
`SpaceState.cpp` at `81175bb` emits `[DockAdvisory] axis request=... delta_m=...`
to the same `std::cerr` immediately before that failure. Therefore, if the
excerpt represents the unfiltered stderr stream, it was produced by an older
binary. An excerpt alone cannot prove which executable ran. Verify checkout
HEAD, binary contents/build provenance and capture full combined output before
changing docking math. The local code and 2 m guard have not been changed by
this observation. Live acceptance remains open.


## 2026-09-24 — one authoritative tick and Hub-local docking geometry (local verification)

The earlier current/render split is superseded for docking decisions. Planning
captures a single authoritative snapshot epoch: ship start, semantic port,
obstacles and route gates use tactical Hub-local meters. The port's authored
module offset is fixed relative to the Hub; rotation is evaluated from the
attachment at that same universe time. Its axial spin keeps the entrance and
approach axis fixed. Each subsequent server tick samples ship and target from
the same snapshot, checks timeline and Hub identity, then validates the final
gate and corridor entirely in local coordinates. The 2 m axis guard remains.

Presentation does not feed flight decisions. The cockpit world frames use the
player's single render reference frame/time; the Hub map projects the same
local gate positions directly in its local camera. Thus no map-specific Hub
orbit predictor can move a docking gate. The manual-mode indicator uses render
time too. New planning, frame mismatch, axis, corridor and hand-back logs carry
tick/time/frame or measured local errors. Native docking regression and manual
docking architecture/source-integrity checks pass locally. A full Windows
build, 45 s route persistence and server-confirmed Human control are pending.

Local commits must not be mistaken for remote `main`: direct public-main push
was rejected by automatic approval review. Apply the exported patch locally
for target verification until publication is explicitly authorized.

Verification boundary: local native docking test and two focused Python checks
pass. The Windows runner cannot start here (CMake/Ninja/CTest absent), and
full game syntax compilation lacks GLAD. Running its Python checks separately
gave 82/91 passes; nine failures are in existing runtime-control/NPC/DTO,
trajectory, editor, auth/wire and bootstrap checks outside this docking change.
They need independent triage on the configured target; no full architecture
gate pass is claimed.

## 2026-09-24 — docking coordinate/epoch boundary audit (local verification)

The SHOW ROUTE planner uses one Hub-local tactical frame for ship start,
semantic entrance/axis, static obstacle centers/bases and all generated gates.
World positions enter only at the snapshot adapter; published map and cockpit
frames are output projections of the same immutable local gate list.

Two additional mismatched-epoch reads were found after the earlier dock-axis
fix. Corridor tracking converted a ship world pose from its snapshot through
the Hub frame at the current display time. It now reads the ship's authoritative
or locally predicted `motion.localPositionMeters` after confirming Hub-frame
identity. Cockpit guidance previously transformed gates at estimated current
time against a player rendered on the delayed presentation frame. It now
resolves the dock and projects the gates through the player's actual
`renderReferenceFrame` and builds the HUD at that frame's universe time. The
map continues to project at its map time. A temporarily missing render sample
waits for the next coherent frame without cancelling the accepted route. HUD
marker stamping also uses the player's render-frame time.

Focused native regression passes: mixed current/render epochs in the fixture
displace a gate by over 100 m for a 0.1 s lag; coherent local/render transforms
preserve relative position. The separate Hub-map co-moving seed agrees with
the canonical orbital predictor to within 0.1 m through 120 s in the fixture.
The manual-docking architecture check and test-source integrity check pass.
Full game build/live persistence/Human hand-back are still unverified here.


## 2026-09-24 — dock-axis split fixed locally; Windows flight gate pending

Root cause is confirmed in source and a focused native regression: the fixed
terminal gate was advanced with the canonical orbital Hub frame, while the
resolved dock anchor was advanced by its instantaneous world velocity. The
latter traces a straight tangent and drifts even though the dock module's
authored Hub-local offset has zero linear velocity. The diagnostic cube at
`(3000,350,0)` has a 27-degree yaw and a 2-degree/s local Z spin; its entrance
center/forward lie on that spin axis. The far cylinder is stationary relative
to the Hub in both position and orientation.

`SpaceState` now retains the target's stable Hub attachment and semantic
definition at planning time, resolves the module again from the same predicted
Hub frame and universe time used for the gate, and resolves the anchor from
that module. The 2 m axis guard is unchanged. On a guard failure it logs both
world positions, error, times, frame and module IDs and Hub-local module speed.
The test covers 1 s and 45 s of orbital motion, zero authored module local
translation, axis-preserving spin, and rejection of off-axis spin. Focused
native test and manual-docking architecture check pass locally; full Windows
game build and observed flight/authority hand-back are still required.

The previous live observation remains valid: the line appeared briefly, so
takeover, settled snapshot, planning and publication were reached in that run.
The supplied log still does not prove the final server acknowledgement of
Human control.


## 2026-09-24 — live manual docking reaches publication, then fails dock-axis consistency check

Target game result on current docking slice:

```text
[DockAdvisory] request=1 failed=dock moved off approach axis
[DockAdvisory] request=2 failed=dock moved off approach axis
```

Observed behavior: the Hub-map/HUD route appears briefly and is then removed.

By current code ordering, a visible route proves that the request already passed
authoritative Autopilot ownership, Hub-relative speed settling, angular-rate
settling, fresh authoritative start snapshot capture, async
`DockingAdvisoryPlanner` completion and guidance publication. The failing check
runs only after a valid plan has become active.

The current blocker is therefore the post-publication terminal consistency
check. It compares the final fixed Hub-local advisory gate transformed through
the current predicted Hub frame against a separately predicted semantic dock
anchor:

`frame.localToWorldPosition(finalGateLocal)` versus
`predictHubSemanticAnchorAt(active.port, time).position +
forward * standoff`.

For the yawed/spinning diagnostic dock these two prediction paths are drifting
apart by more than the hard 2 m threshold. This is likely a prediction-model
consistency defect, not a planner or stabilization failure. Do not widen the
2 m tolerance without measuring the two states and unifying ownership.

Route publication sends Complete and begins hand-back, but the provided log
fragment does not contain the authoritative
`[DockPrep] published ... human_restored=1` /
`phase=manual human_control=1` evidence, so final Human restoration is not yet
recorded as target-proven.


## 2026-09-24 — test-source meta-check parser corrected before target run

Post-commit review found a defect in the newly added
`check_test_suite_source_integrity.py`: its extension regex matched the
`.c` prefix of `.cpp`, producing false missing-source names such as
`DynamicMotionSystem.c`.

The parser is corrected to prefer complete extensions and to inspect only two
repository-owned CMake source forms: explicit
`${ELITE_SOURCE_ROOT}/...` paths and test-local bare filenames. External
include probes/URLs are deliberately ignored. The previous navigation cleanup
itself remains unchanged.


## 2026-09-24 — navigation test layer audited and legacy guidance suite retired

Status: **TEST ARCHITECTURE CLEANUP IMPLEMENTED / TARGET RERUN PENDING**

The target architecture gate exposed another stale raw-symbol assertion:
`angularAccelerationEnvelope`. A full audit then covered:
- all 91 Python checks invoked by `tests/architecture_contracts/run_mingw64.sh`;
- all 16 `tests/*/CMakeLists.txt` source registrations;
- the navigation-focused test directories and their current/retired ownership.

Findings:
1. `check_local_flight_control.py` still expected the deleted private
   `angularAccelerationEnvelope/angularRateEnvelope` helpers. Current
   `ShipController` correctly consumes canonical `ShipDynamics.h` angular
   accessors. The check now verifies that boundary.
2. `check_navigation_live_replication_guidance.py` still opened deleted
   `DockingPathPlanner.cpp` and pinned snapshot wire schema 8. It now checks
   `DockingAdvisoryPlanner.cpp` and schema 9.
3. `check_navigation_foundation_lock.py` still required the pre-Stage-12
   client docking/tunnel stack and old `updateDockingGuidance(float dt)`.
   The obsolete expectations are removed; it now pins
   `updateDockingAdvisory()`, current authoritative planning snapshot usage,
   500 m gates and current runtime evidence.
4. `check_geometric_path_planner.py` still treated
   `DockingPathPlanner` as the docking composition layer. It now verifies
   `DockingAdvisoryPlanner` plus focused current geometric/runtime tests.
5. `tests/navigation_guidance` is a dead all-in-one legacy suite: its CMake
   explicitly compiled removed `DockingPathPlanner.cpp` and
   `GuidanceTunnel.cpp`. The complete directory is retired from the ready
   harness and removed.
6. Useful geometric coverage from that old suite is preserved in the new
   `tests/navigation_runtime/GeometricPathPlannerTests.cpp`.
7. A new `check_test_suite_source_integrity.py` validates every test CMake
   source path and runner script path, preventing deleted-source suites from
   silently surviving future refactors.

No production navigation/flight physics was changed by this cleanup.


## 2026-09-24 — local-flight architecture gate failure is stale static contract

Target architecture run stopped at:

`Local-flight-control architecture check failed: shared local motion law lost:
params.maxCombatSpeed`.

This is not a runtime flight-law regression. `DynamicMotionSystem.cpp` now
correctly consumes centralized `ShipDynamics.h` accessors:
`controlledSpeedLimitMps(params)`,
`forwardMainAccelerationLimitMps2(params)` and
`reverseMainAccelerationLimitMps2(params)`.

The stale Python check still required direct raw-field reads
`params.maxCombatSpeed`, `params.maxGs` and later
`params.maxLinearGs > 0.0f` inside DynamicMotionSystem. Those reads were
deliberately moved out of that layer.

The check is updated to prove both sides of the intended boundary:
DynamicMotionSystem must consume the common accessors, while ShipDynamics must
map those accessors to `maxCombatSpeed` and
`maxLinearGs` with fallback to `maxGs`. Native local-flight behavior tests
remain unchanged. Target rerun is required.


## 2026-09-24 — authority snapshot composition compile bug found and corrected

Static review of commit `61b5424608533c806672e21e77339be612dc0087`
found a patching error before target execution: the authority assignment was
inserted three times into `copySnapshotForSession()`, redeclaring
`controlledEntityId`, while hydrated and sparse copy paths were left without
the field.

The correction centralizes the query in
`controlledEntityAutopilotActiveForSession()` and makes all three production
copy paths call that helper exactly once. The focused commissioning check now
requires exactly three helper calls. The broken intermediate SHA must not be
used as target evidence.


## 2026-09-24 — authoritative docking hand-back acknowledgement implemented

Status: **CODE IMPLEMENTED / TARGET GAME GATE PENDING**

The initial manual-docking takeover had one remaining network race: after route
publication the client could resume Human prediction immediately after sending
Complete while the authoritative server still owned the ship as Autopilot for
one transport/fixed-step interval.

That edge is now closed by
`ClientSessionSnapshot::controlledEntityAutopilotActive`. Every per-session
full/hydrated/sparse snapshot composes the value from `ControlRegistry`.
Snapshot data-plane schema advances from 8 to 9.

Docking preparation now requires authoritative Autopilot=true before stop/settle
evidence is accepted. After the route is published, Complete is sent but local
prediction remains fenced until a newer accepted session snapshot reports
Autopilot=false. Expected successful order is therefore:

`phase=stabilizing -> [DockPrep] begin -> phase=planning ->
phase=handoff_wait -> [DockPrep] published ... human_restored=1 ->
phase=manual human_control=1`.

The existing native ControlRegistry contract on current main remains the
authority identity gate. No target-machine/game acceptance is claimed yet.


## 2026-09-24 — static audit after docking-preparation implementation

Post-commit GitHub inspection confirmed the active Hub travel frame stores
`matchedReferenceFrameId = hubFrame->hubId`, so the preparation stop tests
the correct Hub co-moving `localVelocityMps`. The existing shared physics
contract confirms `BrakeToStop` is valid under both control laws: Newtonian
rotates/brakes through its physical main-engine path, while Assisted drives its
target relative velocity to zero through installed main/RCS authority. A native
`ControlRegistryContractTests.cpp` now pins Human -> Autopilot -> Human
controller transitions while requiring player->ship identity to remain
unchanged. Execution on the Windows target remains pending.


## 2026-09-24 — manual docking preparation vertical slice implemented

Status: **CODE IMPLEMENTED / TARGET GAME GATE PENDING**

SHOW ROUTE now has a real temporary authority hand-off. The client sends an
authoritative begin-preparation command, suppresses only local human prediction,
and continues transmitting numbered input. The server changes the same
player-to-ship authority record from Human to Autopilot, discards queued human
samples from the preparation interval, and repeatedly drives the existing
physical `BrakeToStop` control primitive. Newtonian craft therefore rotate and
brake with installed forward main thrust; Assisted uses installed propulsion.
Neutral attitude axes retain normal bounded angular damping.

The client does not estimate a future start. It waits until canonical replicated
ship state has Hub-relative `localVelocityMps` below the descriptor stop
epsilon and combined pitch/yaw/roll rate below 0.01 rad/s for 0.25 s, then calls
`buildAuthoritativeHubSnapshot` at that exact accepted snapshot epoch. The
server continues holding Autopilot during async route calculation. Only after
Hub Map route and spatial HUD guidance are published does the client send the
completion command and re-enable local prediction; the server discards
preparation-era input and restores Human authority.

The advisory default and live request are now 500 m. Gate extraction does not
choose a chord longer than the configured spacing; the terminal remainder is
retained. HUD speed text is anchored to the actual projected upper-left gate
corner rather than a fixed vertex index.

`cockpit.docking.manual_mode` is in the unified localization catalog for
en/ru/zh-Hans/es/ja and renders as blinking cockpit-top text while manual
docking spatial guidance is active.

Retired checks that required deleted rolling GuidanceTunnel/DockingPathPlanner
runtime are removed. The general guidance architecture check is rewritten for
the active DockingAdvisory path and a focused manual-docking commissioning check
is added. No Windows/gameplay acceptance is claimed yet.


## 2026-09-24 — exact manual docking route commissioning contract

Status: **CONTRACT PINNED / IMPLEMENTATION INCOMPLETE**

The in-game manual docking test now has one exact lifecycle. Pressing the dock
card's SHOW ROUTE / calculate-route action does not immediately plan from a
moving client snapshot. It first requests temporary authoritative Autopilot
control. Autopilot physically stabilizes the ship to rest relative to the Hub
co-moving frame and settles angular rate through the normal ship-control and
physics path. No teleport, velocity clamp or control-law substitution is
allowed.

Only after stabilization is captured does navigation take a fresh authoritative
position/velocity/orientation/angular-velocity start state and calculate the
manual docking route. When the route is ready, the Hub map shows the planned
trajectory and the cockpit HUD shows fixed spatial tunnel gates. Normal gate
spacing is **500 m**; the terminal gate is always retained even when the last
interval is shorter. Every gate carries the recommended speed for the following
section and renders that speed at the gate's projected upper-left corner.

Once both map trajectory and HUD tunnel have been published, temporary
Autopilot authority is released and Human control resumes. While manual docking
guidance remains active, the cockpit displays a blinking localized
**MANUAL DOCKING MODE** status at the top of the cockpit. This string must come
from the unified `LocalizationService` catalog (all approved locales), never
from a renderer-local language branch or hardcoded Russian/English text.

Manual guidance is cancelled and all route/tunnel/manual-mode presentation is
cleared when either:

1. after valid corridor entry, the ship leaves the permitted tunnel
   cross-section; or
2. the selected dock information card is closed in Hub Map.

Cancellation during the preparation phase must also release temporary
Autopilot authority and return control to the human pilot.

Repository audit at this state:

- dock-card close cancellation is already implemented;
- post-entry corridor-leave cancellation exists, but still uses the provisional
  60 m transit / 700 m taper policy rather than proved corridor cross-sections;
- Hub Map route rendering exists;
- fixed spatial HUD gates and per-gate speed values exist;
- gate spacing is currently 350 m, not 500 m;
- the speed label currently uses a projected corner that is not guaranteed to
  be upper-left and formats `m/s` inside the renderer;
- spatial gate opacity still fades along the tunnel;
- no localized manual-docking-mode cockpit string/status exists;
- server `ControllerKind::Autopilot` exists as a type but temporary player
  takeover/release is not wired; current SHOW ROUTE remains client-owned;
- current architecture checks for the removed rolling
  `GuidanceTunnelBuilder` path are stale and must be replaced before they can
  serve as gates for the new advisory implementation.


## 2026-09-24 — docking start and corridor meaning revised

After the first live cancellation the user clarified that both dock-card
commands must establish a predictable ship state before planning. For manual
SHOW ROUTE the chosen target is rest relative to the Hub co-moving frame;
stabilization runs through bounded authoritative controls, then a fresh
authoritative pose/velocity/orientation/angular-rate snapshot is the route
start. Five to ten seconds is an example preparation window, not permission
to teleport, clamp speed or publish before physical capture. DOCKING uses the
same preparation before the separate automatic flight program. These
requirements are **specified, not implemented**: current SHOW ROUTE still
requests geometry from the moving ship, and server control authority currently
wires only Human. See the contract in `STAGE12_END_TO_END.md`.

An exit is defined against the physically clear, ordered swept-hull corridor
after entry and after an accepted start state. Merely being off an initial
polyline computed from a moving snapshot cannot count as leaving. The current
60 m transit/700 m taper is a temporary display/tracking policy, not a proof
of physical corridor clearance; the next implementation must derive permitted
center-region cross sections from the actual static hull clearance.

## 2026-09-24 — live dock map gate failed at corridor entry

The first Windows game run reached the new advisory path, but requests 1–6
immediately logged `ship left guidance corridor`; no line appeared on the Hub
map. The existing check applied dock-opening clearance to the entire route and
cancelled a plan before the ship had entered its first spatial gate. The client
now shows the planned trajectory while entry is pending, enforces exit only
after entry, allows a 60 m open-flight deviation, and narrows the corridor to
the fitted dock opening along the final 700 m. The map resolves the selected
dock route explicitly when other guidance products are present. Failed exits
now log signed lateral/vertical offsets and bounds. This correction requires a
new Windows in-game gate; local native checks alone do not establish visibility.

## 2026-09-24 — docking fixture native gate and rotating-axis correction

The station / near dock / yawed far dock static fixture now has a native
advisory test: its path clears all three obstacles, approaches along the port
normal and stops at the staging point. A separate semantic-anchor test verifies
the far dock's yawed 2 deg/s local-Z spin over 45 s. Both pass locally. The
isolated flight fixture no longer spawns the unrelated Stage-12 runtime NPC.

The test exposed a kinematics defect: module angular velocity had been mapped
from raw Euler rates into Hub axes, so a yawed spinning dock predicted rotation
about the wrong world axis. Simulation and `NavigationWorldPredictor` now share
the angular-velocity conversion for the authored Rx*Ry*Rz orientation. A
separate compilation blocker used the removed pilot-step constant; the NPC
control bridge now reads the configured maximum step from its pilot profile.

The native advisory gate and affected client/predictor/scene syntax checks
passed. Full game compilation and actual on-screen docking remain **unverified**
in this Linux checkout: CMake/Ninja, websocketpp headers and a graphical game
runtime are unavailable. DOCKING remains disabled pending a server-owned,
physically proved program, reservation and capture. No target-game acceptance
is claimed.

## 2026-09-24 — docking behavior replacement started

The diagnostic `HubDockingFlightTestScene` now leaves the station, Cobra and
both docks while removing the stress objects. The far dock is yawed 27 degrees
and spins at 2 degrees/s around its entrance axis. This is a fixture for a
future in-game acceptance test, not evidence of successful docking.

Removed the dormant 1200-line client docking computation, rolling manual tunnel,
`GuidanceTunnelBuilder` and `DockingPathPlanner` from the current checkout. A new
advisory-only `DockingAdvisoryPlanner` searches fixed geometry to a staging stop
before the port, rounds turns only if obstacle-clear, assigns backward braking
and curvature-based speed recommendations, then emits 350 m gates. The Hub map
shows the route, the cockpit shows spatial gates, and leaving the corridor or
closing the dock card cancels guidance. The native static route test passed. Static search runs on a copied request
outside the client frame; stale/timeline-discontinuous results are rejected.

The dock card has SHOW ROUTE and DOCKING. DOCKING remains disabled: the server
has no accepted physical program, dock reservation, Autopilot authority,
ingress or capture. This slice is not playable automatic docking; the new game
path has not passed live gameplay or complete client build validation because
this environment lacks the `websocketpp` header.


## 2026-09-24 — static A* and execution ownership clarified

The commissioned A* scope is one vehicle or group envelope against a fixed
static scene. Scheduled traffic, dynamic hazards, avoidance and dispatch do
not belong in A*. A geometric polyline is not an executable program: a later
separate compiler must author/prove the whole physical flight including final
state; execution consumes the immutable accepted program or cancels it.
Background commissioning is required but not yet connected to an executor.

Nominal routes now retain capability revision and reject mismatch. The
geometric planner checks candidate routes against the complete static request
even when the support-node search uses a capped obstacle subset; if that subset
omits a blocker, it reports failure rather than claiming success. This is a
subset limitation, not a proof of no path in the full scene.

## 2026-09-24 — stress fixture adequacy audited and corrected

The 500 original requests have unique start and unique goal coordinates, but
all lie on narrow common lines. They cannot represent 500 simultaneously
present Cobra hulls: the 100-barrel batch has 10,401 overlapping start pairs
and 10,077 overlapping goal pairs; the dogleg has 41,706 and 41,926. Earlier
500-query numbers measure independent throughput only, not concurrent traffic.

New `open_separated` control places 500 start/goal pairs on 25 × 20 parallel
3D lanes with 30 m spacing; no initial or terminal pair overlaps the current
13 m-radius geometric hull. Here all 500 geometry routes are direct, all 500
point-chain attempts enter the 8 m finish region, all 500 have correct +X
body orientation, and **0/500** satisfy terminal speed <=2 m/s. Final median
speed is 19.214 m/s. This isolates the current diagnostic's failure to plan
braking from traffic or scene complexity. It is not a failure proof for a
proper integrated planner and says nothing about A* in other state spaces.

## 2026-09-24 — chained B5 diagnostic fails terminal state in empty space

`benchmarks/navigation_route_stress` now runs a bounded, observer-only chain
through every visibility-graph point. Each chosen B5 terminal sample state
(position, velocity, basis and angular velocity) seeds the next query. The
diagnostic checks sampled center segments against every static obstacle and
requires 8 m final position, <=2 m/s final speed and +X hull direction within
10 degrees. All three scenes start at 20 m/s along +X.

All 500 starts received an unproved first B5 candidate, but complete terminal
capture is **0/500** in open space, **0/500** in the forced dogleg, and **0/500**
among 100 barrels. In open space, all 500 enter the final position sphere but
all miss final speed (median 5.110 m/s) and 448 miss attitude. Dogleg has 25
position reaches and 178 sampled collision rejections; barrels have 13
position reaches and 406 sampled collision rejections. Chained B5 p95 times:
0.345/1.047/3.884 ms; none represents successful proved route planning.

The bounded greedy waypoint frontier is diagnostic. Its failures do not prove
that a physically feasible route does not exist. They expose that the current
B5 point/capture interface does not author the required terminal state, while
point chasing at obstacles can drive short candidates into geometry. Replace
the waypoint with typed corridor/capture regions and backward terminal-state
admissibility, then prove the exact candidate before publication. These are
local Linux measurements, not MinGW or live-flight acceptance.

## 2026-09-23 — reproducible 100-obstacle / 500-ship route baseline

Correction and extension: the first baseline mistakenly used 5 m instead of
the Cobra's current 13 m geometric radius. With the corrected fixture and the
first B5 physical observer request, the 100-barrel run is 17,074.610 ms total,
31.941 ms p50, 63.243 ms p95, 500/500 geometrically clear, zero turns over
45 degrees and 500/500 **unproved initial-only** B5 candidates. A separate
four-wall dogleg forces 1,000 turns over 45 degrees (153 over 90 degrees) among
500 routes; geometry took 37.596 ms total, 500/500 clear, and the same
initial-only B5 probe still reports 500/500 unproved candidates. This is direct
evidence that initial B5 availability does not assess later route corners.
Neither fixture measures a complete physically executable route or 500 moving
actors colliding dynamically. The earlier 5 m and 15.33 s figures below are
superseded and must not be used for comparison.

The new `benchmarks/navigation_route_stress` fixture contains 100 deterministic
capsule obstacles and 500 distinct ship start/goal requests. Native Linux
g++ -O2 measurement of the current `GeometricPathPlanner` with its production
32-obstacle query cap: 15,330.817 ms sequential wall time, 30.025 ms per-ship
p50 and 57.749 ms p95. All 500 routes were subsequently checked against all
100 obstacles and were geometrically clear in this fixture. One identical
query with all 100 obstacles took 419.403 ms versus 42.002 ms with cap 32.

This measures neither physical feasibility nor actual flight time. It does not
validate the 32-obstacle cap for arbitrary scenes, and it is not MinGW target
evidence. Use the same generated requests and full-scene verification for the
next physically integrated planning candidate; record failure count, solve
latency, flight time and continuous collision proof before choosing architecture.

## 2026-09-23 — spatial target affects physical candidate eligibility

The first M4 spatial slice makes `geometricTargetPositionMapMeters` operational
instead of metadata. B5 retains only candidates whose sampled position moves
closer to the capture region without crossing its target plane outside the
explicit capture radius. Rejection is typed and carries starting and closest
distance; the coordinator may try another terminal with the same desired
velocity. Existing compiler fixtures that described velocity-only turns were
corrected to provide matching spatial targets.

Native g++/GLM focused tests pass for compiler and coordinator. MinGW E2E and
viewer were not run here. This local check cannot validate all points of a
continuous physical path, a corridor, or the next corner. No execution authority
has moved from the legacy runtime; the high-speed failure remains open.

## 2026-09-23 — target visual gate exposes the next physical-planner defect

Status: **OBSERVER VISIBLE / ROUTE-WIDE PHYSICAL SOLVE NOT YET PRESENT**

Exact remote commit `15f4c6c6f856cc9cf7974ef1527730315807c880`
built and ran on the MinGW target. The observer publishes one unproved
`LeadRotateMainBurn` candidate and the viewer renders its attitude/burn vectors
separately from legacy execution.

The 20.60 m/s Newtonian visual run fails at the first high-speed direction
change, not during a scalar speed increase. The retained route is collision-free
and has 19.04 m additional clearance, but the legacy author contains 50/801
infeasible actuator intervals. At about 5.03 s it requests lateral/RCS authority
the ship does not have; Follower invalidates tracking at 5.33 s on page 17. No
coarse static contact occurs.

The visual gate also exposes that the observer is only an initial-primitive
probe. `geometricTargetPositionMapMeters` is copied into B5 candidate metadata,
but current candidate dynamics are generated from velocity error only. Thus
`candidate_found_unproved` does not mean the route or next corner is physically
reachable. Extending this observer unchanged would produce a prettier lie.

The active correction is now a spatially bound receding-horizon physical solve:
candidate generation must consume target/corridor capture semantics, propagate
the exact terminal rigid-body state, and negotiate speed/time/terminal
alternatives at every direction change. Only that exact chain may proceed to
continuous proof and acceptance.

## 2026-09-23 — physical observer candidate implemented; target visual gate pending

Status: **OBSERVER DATAFLOW IMPLEMENTED / TARGET BUILD AND VISUAL INSPECTION REQUIRED**

`tools/navigation_runtime` now invokes the pure physical-search coordinator
after the retained route has produced its execution guide. The adapter passes
explicit measured state, canonical vehicle capability, control law, feedback
reserves, compiler shaping policy and four ranked arrival-time horizons.

The result is copied into a presentation-only trace DTO containing alternative
provenance, typed rejection reason/lower bound and sampled physical candidates.
Every candidate remains marked `requiresContinuousProof`; the adapter has no
path to `AcceptedManeuverProgram`, Follower or physics. JSON round-trip coverage
and E2E assertions lock that distinction.

The viewer now renders rejected alternatives in red and unproved physical
candidates separately in cyan/yellow, including body-forward and planned
acceleration vectors. Its title says `PHYS-OBS=... (НЕ ПРИНЯТО)`. Legacy Ruckig,
accepted reference and actual motion remain independent layers.

This checkout cannot compile C++ because CMake/GLM are absent. Target build,
focused tests and visual inspection of straight/corner/high-speed Newtonian
scenes are the active gate. No execution authority has moved.

## 2026-09-23 — coordinator accepted; visual observer is the next mandatory gate

Status: **BOUNDED COORDINATOR TARGET PASS / SYSTEM QUALITY NOT YET CLAIMED**

Exact commit `b506397ca30f223ee6cb29597c8673e0815626d1` built and passed
`physical_maneuver_search_coordinator` on MinGW64 (1/1, 0.04 s). The corrected
6.0 s alternative proves that bounded search advances past an impossible
horizon to a physically compilable candidate.

This accepts only the coordinator contract. It does not validate live route
quality, collision behavior, ship motion or viewer integration. The existing
viewer still shows the legacy translation-first execution path.

Because prior isolated tests were green while integrated visual behavior was
broken, the next mandatory gate is observer-only visualization of the new path:
ranked alternatives, typed rejections, physical candidate samples, attitude and
thrust phases must be rendered separately from legacy trajectory, proved tunnel,
accepted program and actual motion. Further acceptance/execution work is paused
until this visual seam exists and is inspected.

## 2026-09-23 — coordinator target fixture corrected; visual gate defined

Status: **SUPERSEDED — CORRECTION PASSED AT `b506397c`**

Exact commit `3492ca3ba314dcf250c5d3ebc03c6e8cc0c3dce6` configured and
compiled the new coordinator target. Existing physical compiler and chained
limit tests remained PASS. The coordinator test failed its first expectation.

The implementation did not stop after the first rejection: both alternatives
were physically rejected. The fixture requested a 135-degree Newtonian thrust
axis change but gave the supposed feasible alternative only 4.0 s. Its own
compiler policy/capability requires about 4.60 s for attitude acquisition before
any main burn. The corrected feasible horizon is 6.0 s; no production tolerance
or physical bound changed.

Visual status: the current viewer still displays the legacy runtime path, not
the new coordinator. After this focused gate, an observer snapshot may expose
frontier alternatives and typed rejections. Trustworthy visual flight requires
the next rigid-body/literal-actuator compiler slice and runtime integration;
unproved candidates must be visibly distinct from proved/accepted motion.

## 2026-09-23 — physical witness accepted; bounded coordinator candidate

Status: **FIRST M3/M4 CONTRACT SLICE ACCEPTED / COORDINATOR RETEST REQUIRED**

MinGW64 validated exact commit
`9af337c2e23a32d5f11d34a3e048ecd98842674d`. Both
`ordinary_physical_maneuver_compiler` and `maneuver_chained_limit_matrix`
passed (2/2, 0.12 s). The typed infeasibility witness and its Newtonian
rotate-before-burn boundary are therefore accepted on target.

The next candidate adds pure `PhysicalManeuverSearchCoordinator`. It consumes
an explicit ranked frontier of corridor/terminal/speed/arrival-time
alternatives, independent objective/frontier revisions, a resumable cursor and
a per-worker attempt budget. It
changes only target position, desired velocity and local program horizon before
calling the physical compiler; state, capability, law, reserves and compiler
policy remain common immutable inputs.

The coordinator returns `CandidateFound`, resumable `SearchPending`,
`FrontierExhausted`, `SharedStateBlocked` or `InvalidInput`. Every non-terminal
search outcome keeps objective ownership active. Rejection history retains
alternative provenance and typed witnesses; candidates remain unproved B5
products and cannot cross directly into `AcceptedManeuverProgram`.

## 2026-09-23 — direct physical-authoring replacement authorized

Status: **M3/M4 VERTICAL REPLACEMENT STARTED**

The migration no longer treats preservation of the translation-first physical
authoring block as a goal. Scenario I/O already has its explicit boundary; the
next vertical slice may delete or bypass legacy trajectory/program construction
where it conflicts with physical truth.

The first replacement seam is the physical compiler result. It must return
either bounded vehicle/control-law candidates or a typed
`InfeasibilityWitness` containing the limiting constraint and measured timing/
authority bounds. A failed attempt is coordinator feedback, not navigation
shutdown and not permission to publish an infeasible accepted program.

The candidate now implements that witness for invalid query/frame/law, missing
translation or attitude authority, insufficient horizon and numerical failure.
It also fails closed when initial angular velocity is non-zero because the
current candidate families do not yet propagate that state consistently; the
old behavior silently emitted fixed-attitude samples from a rotating state.
Already-aligned main-engine burns no longer require irrelevant angular
authority. This first slice is accepted by the target evidence above.

The old `TrajectoryGenerator -> attitude fit -> actuator annotation` chain may
temporarily remain for regression evidence and geometric-corridor extraction,
but it is not the target author of `AcceptedManeuverProgram`.

## 2026-09-23 — objective negotiation and persistent no-solution behavior

Status: **ARCHITECTURE CONTRACT CORRECTED / M2 CODE GATE RECORDED**

The high-speed diagnosis exposed wording that treated an attitude-infeasible
translation as a trajectory and suggested returning failure. That model is now
explicitly rejected. A trajectory belongs to a concrete vehicle/control law and
cannot demand thrust before its required thrust axis is reachable.

Mission/behavior owns `NavigationIntent`; a goal resolver owns the typed
`TerminalContract`; global planning returns ranked corridor/terminal
alternatives; the physical planner returns either proved candidates or a typed
infeasibility witness. The coordinator uses the witness to vary corridor,
portal, terminal sample, speed schedule or arrival time under bounded budgets.

No-solution does not disable navigation. The objective and planning state stay
active, the last still-proved short program may continue, and safe hold/coast/
attitude/braking alternatives are considered. If contact is unavoidable, a
separate physically feasible `UnavoidableContactMitigation` solve minimizes
impact consequences and explicitly predicts contact rather than claiming a
collision-free proof.

## 2026-09-23 — M2 slice 1 reaches runtime; known Newtonian physical-authoring defect reproduced

Status: **TARGET RUNTIME REACHED / M2 STRUCTURAL BEHAVIOR PRESERVED / M3-M4 FAILURE REPRODUCED**

The supplied MinGW64 excerpt confirms that the scenario-I/O extraction now
compiles and links far enough to run `navigation_runtime_pipeline`. The
non-identity frame product-chain marker remains PASS and the identity and
translated/rotated/moving fixtures remain numerically identical.

The later high-speed Newtonian fixture fails exactly at the already classified
physical-authoring boundary:

- 753 planned actuator intervals;
- 34 intervals marked physically infeasible;
- only one storage-page advance before tracking invalidation;
- `PROGRAM_INVALIDATED_TRACKING_LOSS` at 0.51 s;
- planned reference/velocity separation reaches 170.46 deg somewhere in the
  authored trajectory;
- the actually executed body remains within 1.00 deg of current velocity before
  execution aborts;
- no coarse static collision occurs.

Root cause remains the ordering defect, not the M2 I/O split:
`TrajectoryGenerator/Ruckig` first authors translational P/V/A using a scalar
acceleration envelope; reachable body attitude and installed propulsion topology
are fitted afterward. For the Newtonian Cobra, reverse main authority is absent
and manoeuvre/RCS authority is small, so braking that requires strong force
opposite velocity must reserve time to rotate the hull first. The current
trajectory does not co-time that rotation with translational demand.

`makeProgramPhase()` detects the mismatch via
`propulsionFeasible=false` / `actuatorProgramFeasible=false` but still emits
a valid executable program during the observe-only migration. Follower then
tracks an unreachable reference until the explicit 0.50 s tracking-loss window
expires and correctly invalidates the program.

Do not lengthen the timeout and do not weaken the high-speed test. M3 must make
infeasible programs non-acceptable; M4 must move Newtonian physical maneuver
authoring (rotate/coast/burn/flip-and-burn as required) ahead of final timing.

The provided excerpt does not include the checkout hash line requested by the
target-gate protocol, so no exact tested SHA is recorded from this run.


## 2026-09-23 — M2 slice 1 target compile defect diagnosed and corrected

Status: **M2 SLICE 1 CORRECTION IMPLEMENTED — TARGET RETEST REQUIRED**

The first MinGW64 target gate after extracting scenario I/O passed the
architecture contract, Stage-1 nominal-route test and follower component test,
then failed while compiling both `navigation_runtime_pipeline_tests` and
`navigation_runtime_viewer`.

The compile failures were all unresolved `normalizedOr()` calls plus one
unresolved `basisFromForwardUp()` call in
`NavigationScenarioRuntime.cpp`. Root cause: commit
`104544f8864e8ae26a7e668ceec72f5f08566d57` moved those two pure math helpers
together with the JSON parser into `NavigationScenarioIo.cpp`'s anonymous
namespace. Scenario I/O still compiled, but runtime calculation code could no
longer see helpers it continued to use.

Correction: `NavigationScenarioMath.h` now owns the two shared pure helpers.
Both scenario I/O and runtime composition include that header, and the private
duplicate was removed from `NavigationScenarioIo.cpp`. No route generation,
maneuver timing, Follower behavior, physics, pilot policy, risk semantics or
navigation geometry changed.

The same MinGW64 target gate must now be rerun. M2 slice 1 remains unaccepted
until compile/link and the required runtime evidence are observed.


## 2026-09-23 — M1 accepted; M2 composition-root split activated

Status: **M1 ACCEPTED / M2 SLICE 1 IMPLEMENTED — TARGET BUILD REQUIRED**

The fourth MinGW64 run produced the required non-identity-frame marker. Identity
and translated/rotated/moving-frame executions both advanced through all 90 page
boundaries, completed their 91-page programs and matched across 902 NavLocal
trace frames. This closes the frame/API/timeline exit gate for M1.

The pipeline still fails later in the high-speed Newtonian fixture. It reports
34 physically infeasible actuator segments, loses tracking at 0.51 s and has a
170.46-degree reference/velocity mismatch while the body remains aligned within
1 degree of velocity. That is the known M3/M4 authoring-order defect: an
unreachable translational reference is accepted before attitude/thrust
feasibility. It must not be hidden by increasing the tracking timeout.

M2 is now active. Its first behavior-preserving slice has extracted scenario
JSON and filesystem parsing from the 3849-line runtime monolith into
`NavigationScenarioIo.{h,cpp}` and the dedicated
`EliteNavigationScenarioToolIo` target. `NavigationScenarioRuntime.cpp` no
longer includes nlohmann JSON, defines the loader or owns input streams. The
viewer and E2E explicitly include the tool-I/O API, while Stage 1/2 continue to
receive the immutable `ScenarioDefinition` value.

All five available architecture contracts and `git diff --check` pass. Local
C++ build was not run because this environment has no CMake. MinGW64 must now
prove the new module compiles/links and preserves the accepted M1 marker and
the already-classified high-speed failure.

## 2026-09-23 — map/passage/risk/pilot semantics clarified

Status: **NORMATIVE REQUIREMENTS INTEGRATED / M1 TARGET GATE UNCHANGED**

The blueprint now explicitly separates:

- certified geometric traversability from coarse sphere/AABB broadphase;
- route corridor and typed portal geometry from actor-specific physical motion;
- `STANDARD`/`EXTREME` risk doctrine from constrained/Squeeze passage profile;
- `NEWTONIAN`/`ASSISTED` control law from installed hardware;
- nominal collision-free physical proof from robustness margin;
- pilot execution envelope and realized pilot error from vehicle capability;
- impossible route, robust-safe route, constrained-risk route and execution
  contact.

Regions may not treat their AABB as free volume, portal centers are not portal
semantics, and scalar clearance is a sufficient fast path rather than authority
to erase an orientation-traversable slit. A constrained-risk program must still
be nominally hull-clear and actuator-feasible; collision can result from pilot
envelope departure and must be attributed as such.

This is a documentation/contract iteration. The active code candidate remains
the canonical page timeline + M1 frame boundary awaiting MinGW64 validation.

## 2026-09-23 — M1 runtime reached; canonical page timeline added

Status: **RUNTIME DEFECT CORRECTED / TARGET REVALIDATION REQUIRED**

The third MinGW64 run compiled and linked the viewer and pipeline. Identity and
non-identity executions produced identical diagnostics and NavLocal telemetry,
but both failed at 50.30 s on page 1 with `PROGRAM_PAGE_BEFORE_START`.

The failure was caused by duplicate time ownership: runtime advanced by a
rounded previous-page end while Sampler judged the next page by its own start.
`ManeuverProgramTimeline` now owns page windows, active-page selection and
page-local elapsed time. Runtime, Sampler, Follower and phase gate use it.
Follower completion also now accounts for `sequenceStartOffsetSeconds`.

The frame E2E now compares the complete identity/non-identity trace and terminal
outcome rather than requiring both runs to finish a maneuver already known to
contain 78 infeasible actuator intervals. Physical success remains an M3/M4
gate; frame invariance remains M1. A new target run is required before M1 can be
accepted.

## 2026-09-23 — second M1 target attempt reached TrajectoryGenerator

Status: **SECOND COMPILE CORRECTION IMPLEMENTED / TARGET REBUILD REQUIRED**

The second supplied MinGW64 log confirms that the previous stale-pilot fix
worked: `NavigationScenarioRuntimeE2ETests.cpp` compiled. Architecture checks,
the Stage-1 nominal-route test and the maneuver-tracking-controller test also
passed on target.

The build then stopped in `TrajectoryGenerator.cpp`. This was a latent cleanup
defect, not a trajectory failure: the pure generator still copied the removed
`ruckigSolveMilliseconds` diagnostic and retained an unused blended-waypoint
counter after its only consumer—the internal filesystem perf-log—had been
deleted. The dead code is removed. `globalGuideSpeedLimit()` also no longer
accepts an unused arc table.

The API-purity checker now rejects both removed symbols. No motion behavior was
changed. Link and runtime were not reached, so the non-identity M1 fixture still
has no target verdict.

## 2026-09-23 — first M1 target attempt was compile-blocked

Status: **CORRECTION IMPLEMENTED / CLEAN TARGET REBUILD REQUIRED**

The supplied MinGW64 log is not an M1 runtime result. Compilation stopped in
`NavigationScenarioRuntimeE2ETests.cpp` because one fixture still assigned the
removed `ScenarioRunSettings::pilot` field. The same fixture already passes the
resolved `pilotExecutionProfile`, so the stale broad-field assignment has been
deleted and the API-purity checker now rejects its return.

The `ctest` command in that log ran after the failed build and therefore
executed an older binary. Evidence: its failure text asks for obsolete
reference-reacquisition diagnostics, while the current source requires the
monotonic-clock contract. It also never prints the new non-identity-frame PASS
marker. Those runtime lines cannot accept or reject M1.

The next target commands must be joined with `&&`. M1 remains pending until a
successfully rebuilt executable prints:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

## 2026-09-23 — M1 frame/API repair candidate

Status: **CODE CANDIDATE COMPLETE / TARGET MINGW64 VALIDATION REQUIRED**

The active `tools/navigation_runtime` execution boundary no longer copies
NavLocal intent vectors into System fields. Initial position, velocity, basis
and relative angular velocity now cross the canonical
`NavigationFrameBoundary`; Follower inputs, terminal comparisons and trace
telemetry cross back to NavLocal explicitly.

The execution harness now consumes an explicit `KinematicFrame` snapshot plus
epoch and advances its translation, acceleration, basis rotation and angular
velocity through the fixed-step run. A new product-chain E2E compares the same
route in identity and translated/rotated/moving/rotating frames.

The same test exposed a clock-boundary defect: the maximum execution deadline
was relative while `vehicle.timeSeconds` was absolute universe time. The
deadline now includes the explicit maneuver start epoch.

Static evidence on this checkout:

- navigation API purity PASS;
- Stage-1/two-stage ownership PASS;
- Stage-12 runtime-planner contract PASS;
- geometric path planner contract PASS;
- Python checker compilation PASS;
- diff whitespace validation PASS.

C++ evidence is not available locally: CMake and GLM headers are absent. This
is not an accepted M1 completion. The next state-affecting event must be the
target MinGW64 build/E2E result or a correction of a failure exposed by it.

## 2026-09-22 — scalable navigation architecture specified

Status: **NORMATIVE BLUEPRINT AUTHORED / IMPLEMENTATION NOT STARTED**

The complete audit, target semantic model, algorithms, APIs, multi-object
scheduling model, proof rules, tests and M0-M13 migration are now defined in:

```text
src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md
```

The audit concludes that the active tool chain is transitional and must not be
scaled as-is. Its geometry-first/scalar-timing-first order, post-fitted attitude
and propulsion, observe-only actuator segments and downstream reallocation mean
that the executed maneuver is not the maneuver that was accepted.

The target invariant is:

```text
accepted maneuver == capability-checked maneuver
                  == continuously collision-proved maneuver
                  == actuator program executed by physics
```

The first implementation stage is M1, because the active stand bypasses the
canonical coordinate boundary and current lexical purity checks miss that real
violation. The next code iteration must repair frame/API semantics before
physical compiler or topology work.

The architecture audit did not modify runtime code and did not claim a new
target-machine validation result. Existing test evidence below remains
historical baseline evidence only.

Local audit evidence on the unchanged code baseline:

- `check_navigation_api_purity.py` fails on the benign DTO declaration
  `WorldParams world {};`;
- `check_navigation_stage1_nominal_route.py` fails because it expects one exact
  source spelling/layout for terminal-orientation policy consumption;
- `check_navigation_stage12_runtime_planner.py` fails because it expects one
  exact source spelling/layout for acceleration feed-forward;
- those checks do not reject the actual private frame-conversion bypass.

This is the concrete reason M1 includes replacement of lexical confidence with
semantic boundary and non-identity-frame evidence.

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest actually tested checkout

```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Target evidence:
- architecture PASS;
- compile/link PASS;
- 17/19 runtime tests PASS;
- failures in runtime planner and final composite.

## Current unverified code baseline before state-sync commits

```
70dc7bb9c024a388c39b79d54a12774475ca8b32
```

## B4 contract corrected after first target failure

The former same-horizon two-segment requirement is rejected.

Canonical ordinary local behavior is now:

```text
accepted route / trajectory
    -> physical visible horizon
    -> predicted dynamic/static occupancy
    -> can a safe executable short segment avoid the obstacle?
         yes -> accept that short off-route segment and keep moving
         no  -> issue active braking intent and keep navigation alive
    -> next receding-horizon update
         -> continue bypass while required
         -> begin reacquiring nominal line when safe
         -> converge back under physical steering/acceleration limits
```

There is **no arbitrary distance at which the ship must return to the route**.
In particular, no 30 m merge requirement exists.

The on-route `mergeTargetMapMeters` / runtime mirror is currently only a
reacquisition reference. It is not a mandatory endpoint of the current local
segment and is not required to be reachable inside the same horizon.

## Navigation continuity on failure

`ConflictHold` does not switch navigation off.

Runtime behavior is command-producing:

```text
no safe current bypass
    -> ConflictHold
    -> holdIntent
    -> negative velocity demand / braking
    -> physics continues
    -> navigation planner remains active
    -> fresh world state is evaluated again
```

This matches the required behavior: if there is time/space to evade, evade; if
there is not, brake and use the space that remains while continuing navigation.

## Current bypass selection

For equal lateral clearance the local solver now prefers the farther forward
bounded bypass station. This reduces gratuitous sideways kinks and better
preserves the route direction.

The selected short segment is proved against:
- exact static geometry;
- projected moving occupancy;
- time-coupled dynamic separation.

Physical maneuver authoring/capability proof remains downstream ownership.

## Test corrections

The stale portal-attitude fixture was corrected:
- start moved to X=0;
- blocker moved to X=3.5;
- staging/reacquisition reference remains X=7.

This makes both endpoints outside the 2.75 m required dynamic separation while
the nominal path is still genuinely blocked.

The old regression `testReturnLegIsAlsoProvenAgainstExactStaticGeometry()` was removed.
It is replaced by `testBypassDoesNotRequireImmediateReturnToTrajectory()`, which
deliberately blocks an immediate return leg while leaving a valid short bypass available.

The no-space regression remains and requires:
- `ConflictHold`;
- `localBypassExhausted`;
- active emergency braking intent.

## Validation status

**UNVERIFIED on target MinGW64.**

The next run must verify the new receding-horizon contract. Do not promote B4
until the target gate is green and the final composite demonstrates physically
reasonable behavior.

## Documentation protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.

## Composite physical fallback

The final composite no longer treats `fit.valid == false` as an automatic
test failure. If the geometric B4 segment cannot be authored within the
vehicle's current physical authority, the test now executes active braking
through the real PilotSkill/physics path, keeps the hazard authoritative,
and then continues the receding-horizon replan loop.

This directly pins the required rule:
- can evade physically -> execute bypass;
- cannot evade physically -> brake;
- navigation ownership remains active in both cases.

## 2026-09-20 target run `navigation_test_20260920-182430.txt`

Important: the uploaded log does **not** contain the tested HEAD line, so the
exact checkout must not be inferred from the filename or current repository HEAD.

Observed evidence:
- Stage-12 architecture contract PASS;
- compile/link PASS;
- 17/19 runtime tests PASS;
- `navigation_runtime_planner` FAIL:
  `fixture must retain the future oriented portal as route context`;
- `navigation_composite_proving_ground` FAIL:
  `composite dynamic clearance lost for newtonian`.

### Planner fixture interpretation

The previous fixture repair moved the agent to X=0, exactly onto the minimum X
boundary of region 1 in `orientedPortalCaptureSpace()`. The new failure happens
before the actual bypass assertion: the test no longer retains the oriented portal
as route context. This strongly indicates the fixture was repaired in the wrong
place rather than a B4 regression. Use an interior start while preserving >2.75 m
separation from both start and staging endpoint (for example start X=1, blocker X=4, staging X=7).

### Composite interpretation

The B4 behavior itself improved materially:
- first solve: `AdjustedClear`, 29.38 m lateral offset, 22.5 m forward;
- physical replacement authored successfully;
- actual first replacement kept +4.747 m dynamic clearance and zero tracking violations;
- next receding-horizon solve again returned `AdjustedClear`;
- continuation kept +22.821 m actual dynamic clearance and zero tracking violations;
- next solve returned `NominalClear`.

The failure occurs **after** that successful B4 sequence. The composite then leaves
the receding-horizon planner loop and executes a fixed 10 s narrow-portal program
and then final capture while the dynamic hazard remains active. That violates the
new requirement that navigation/monitoring must remain active. A bounded segment
being nominal-clear does not prove the entire following scripted portal leg is clear.

Therefore the next fix should keep planner/monitor/replan ownership active through
the resumed topology leg instead of treating `NominalClear` as permission to run an
unmonitored long scripted phase.

### Visualisation

A visual trace is now justified. The useful first visualization should plot, in the
same 2D/3D scene, ship path, hazard path and inflated safety envelope, selected B4
targets, bounded nominal/reacquisition references, portal center, and replan points.

## 2026-09-20 navigation runtime 3D viewer

Implemented an independent diagnostic viewer under `tools/navigation_runtime/`.

Files:
- `tools/navigation_runtime/NavigationTrace.h/.cpp` — shared deterministic JSON trace schema/IO;
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp` — standalone GLFW/GLAD/GLM OpenGL 3D viewer;
- `tools/navigation_runtime/CMakeLists.txt` — isolated viewer build;
- `tools/navigation_runtime/run_mingw64.sh` — build/run helper;
- `tools/navigation_runtime/README.md` — controls and workflow.

The composite proving-ground test now writes the actual simulated run to
`tools/navigation_runtime/last_trace_<law>.json`. The writer is RAII-backed,
so the trace is retained when a later composite assertion throws.

Recorded data includes ship P/V/forward, dynamic-hazard P/radius, hull collision
envelope, planner safety envelope, actual dynamic clearance, phase/status,
selected B4 target, reacquisition reference, portal target and replan events.

Viewer presentation:
- topology route as a polyline;
- route turn/portal points as markers;
- actual ship trajectory;
- ship as an oriented wire rectangular box using Cobra half extents;
- explicit nose arrow;
- hazard trajectory and three wire envelopes;
- selected target / reacquisition / portal markers;
- all replan positions;
- current time/phase/status/clearance in the window title.

Controls: RMB orbit, MMB pan, wheel zoom, F fit, Space play/pause,
`[`/`]` frame step, R next replan, Esc close.

Validation state: source is committed but has NOT yet been compiled/run on the
target MinGW64 machine. Do not call the viewer accepted until that gate runs.

## 2026-09-20 target run on `c8972319390622a6b825bf0fa73e8a563c9d7068`

Verified on target MinGW64:
- navigation runtime configured and linked;
- 17/19 tests PASS;
- planner fixture still failed at future oriented portal route-context assertion;
- composite emitted `tools/navigation_runtime/last_trace_newtonian.json` with 770 frames;
- composite B4 sequence remained the same: two physically executed `AdjustedClear`
  segments, then `NominalClear`, then later dynamic-clearance failure;
- standalone viewer configure succeeded, but compile failed because the GLAD
  include root was wrong: source includes `<glad/gl.h>`, actual file is
  `glad/include/glad/gl.h`.

Fixes committed after that verified run:
- viewer CMake now includes `${ELITE_SOURCE_ROOT}/glad/include`;
- oriented-portal fixture now uses interior geometry start X=1, blocker X=4,
  stage X=7 while leaving global `baseAgent()` at its original X=0;
- these post-run fixes are UNVERIFIED until the next target run.

## 2026-09-20 viewer HUD iteration

Target feedback confirmed the standalone viewer now builds and opens, but the first
UI-less build was not self-explanatory: no visible buttons and no legend/state panel.

Viewer updated with an in-window diagnostic HUD:
- clickable PLAY/PAUSE, PREV, NEXT, NEXT REPLAN and FIT buttons;
- right-side live state panel with law/frame/time/phase/status/clearance;
- explicit `WHAT IS HAPPENING` explanation derived from trace phase/status;
- color legend for every diagnostic primitive;
- visible controls reminder;
- `portal_102` explicitly marked as the current known clearance-loss area.

The HUD uses a tiny built-in bitmap font and existing OpenGL primitives; no new UI
framework/dependency was introduced.

Validation state: previous viewer build/open is target-verified; the new HUD commit
still needs one target build/open check.

Same target run also showed planner fixture progress: the old route-context assertion
no longer fails; it now reaches `fixture must produce a safe adjusted target`. That
is a separate planner-fixture task and was not mixed into this HUD change.

## 2026-09-20 viewer physics/trace accuracy iteration

User clarified the viewer must remain an ordinary decorated Windows window, simply
maximized to the desktop work area; no exclusive/borderless game fullscreen.

Verified architecture of the current tool:
- composite test executes real Planner -> AcceptedManeuverProgram -> Follower ->
  runtime bridge -> SharedShipPhysics first;
- the viewer only replays the resulting JSON; it does not run planner/follower live;
- previous apparent low performance was primarily 0.10 s trace quantization (~10 Hz)
  with nearest-frame display, not evidence of a heavily loaded CPU.

Implemented, not yet target-verified:
- smooth interpolation between trace samples during playback;
- trace schema v2 records actual full ship basis forward/right/up (roll preserved);
- trace schema v2 records sampled AcceptedManeuverProgram reference P + full basis;
- viewer draws actual nose and program-reference nose separately and reports angular
  tracking error;
- viewer renders AcceptedManeuverProgram tracking.positionErrorMeters as a translucent
  tracking tube. This is explicitly NOT called a Planner volumetric corridor because
  NavigationRuntimePlanner::Result does not publish one;
- trace records hazard velocity + planner look-ahead;
- bottom-left `ГОРИЗОНТ КОБРЫ` inset projects the moving hazard and its predicted
  envelope tunnel onto Cobra's right/up plane perpendicular to actual forward;
- Russian HUD/Cyrillic bitmap support and Russian window title;
- interactive frame slider;
- ordinary GLFW window starts maximized.

Newtonian diagnostic correction:
- the composite test previously authored replacement/continuation programs with
  OrientationMode::VelocityAligned even under Newtonian law, making Newtonian motion
  visually look Assisted;
- Newtonian local bypass and the un-oriented portal-102 transit now preserve current
  body attitude (FixedStart); Assisted remains velocity-aligned;
- this change is unverified until the next target run.

Still pending and must not be faked in replay UI:
- pilot-skill selector (expert/average/loser);
- flight doctrine selector (standard/extreme);
- meaningful obstacle checkbox producing a different simulation, not merely hiding
  the obstacle;
- meaningful Assisted/Newtonian selector that runs/regenerates the scenario.
These require extracting/reusing a live scenario runner inside the tool or generating
distinct authoritative traces. Do not add cosmetic controls that leave physics unchanged.

## 2026-09-20 live navigation diagnostic stand

Architecture changed: `tools/navigation_runtime` is no longer a passive JSON trace
viewer. The JSON file is now an INPUT SCENARIO only. The executable calculates the
route and vehicle motion in-process when the user presses `РАССЧИТАТЬ`.

New production-chain runtime:
`scenario.json -> NavigationSpace/NavigationMap -> NavigationRuntimePlanner ->
AcceptedManeuverProgram -> TrajectoryFollower -> NavigationRuntimeControlBridge /
PilotSkillExecutor -> SharedShipPhysics / DynamicMotionSystem -> in-memory trace -> 3D`.

New files:
- `tools/navigation_runtime/NavigationScenarioRuntime.h`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tools/navigation_runtime/scenario.json`.

Real UI inputs:
- control law: Assisted / Newtonian;
- pilot: Expert / Average / Loser;
- flight style: Standard / Extreme;
- sudden-obstacle checkbox;
- `РАССЧИТАТЬ` button.

Pilot choice changes the actual PilotSkillExecutor profile (reaction delay, decision
rate, latency, response, slew and deterministic command error). Flight style changes
real cruise speed and planner horizon/cost aggressiveness. Control law changes the
actual local flight law and maneuver attitude semantics.

Sudden obstacle semantics:
- unchecked: sudden obstacle is never published;
- checked: it is absent from initial NavigationMap and appears only after
  `activation_time_s` during the simulation;
- therefore the initial route has no foreknowledge of the surprise obstacle;
- after activation normal receding-horizon replanning reacts to it.

`scenario.json` supports:
- start P/V/forward/up;
- optional forced ship route points (default empty);
- final position;
- optional final forward/up constraints;
- final constant speed requirement;
- static sphere/box/capsule obstacles;
- moving obstacles defined by velocity vector OR route points + speed;
- sudden obstacle, including spawn relative to live ship forward/right/up.

The default scenario leaves `ship_route_points` empty so Planner must calculate the
route around the static obstacle rather than replay authored portal points.

Calculated white route now records the live sequence of Planner `selectedTarget`
decisions. Adjusted targets are marked as turn/bypass points. Actual motion remains
a separate rendered path.

Static obstacles from JSON are carried into the calculated TraceDocument and rendered
in 3D. `last_calculated_trace.json` is still saved for diagnostics, but is an OUTPUT,
not an input to the stand.

Build architecture:
`tools/navigation_runtime/CMakeLists.txt` now links the production navigation/control/
physics stack directly instead of being an OpenGL-only replay executable.

Validation state: this live-runtime iteration is committed but NOT YET compiled/run on
the target MinGW64 machine. Do not claim acceptance until the target gate passes.

## 2026-09-20 target video diagnosis — live stand integration bug

User supplied a screen recording of the first live-stand behavior.

Observed on-screen:
- playback around t≈41 s showed phase `ТОРМОЖЕНИЕ` and status `ДАННЫЕ УСТАРЕЛИ`;
- Cobra made almost no meaningful route progress;
- right diagnostic panel visibly jumped/blinked because conditional rows (especially
  the one-frame `СОБЫТИЕ: ПЕРЕПЛАНИРОВАНИЕ`) were inserted/removed from flow layout;
- the default no-sudden-obstacle run therefore did not demonstrate routing quality.

Root cause found in `NavigationScenarioRuntime.cpp`:
`NavigationRuntimePlanner::plan(... dynamicResultAgeSeconds ...)` was called with
`vehicle.timeSeconds` (absolute simulation time) instead of the age of the freshly
queried NavigationMap snapshot. With `maxResultAgeSeconds=0.25`, every plan after
0.25 s became `StaleHold`, correctly triggering fail-closed braking.

Fix committed:
- live stand now passes `0.0` for the just-created dynamic snapshot age;
- this is the correct semantics for the current synchronous query->plan call.

HUD fix committed:
- right panel uses hard fixed Y slots for mode/frame/time/phase/status/orientation/
  clearance/event/calculation result/explanation/legend/controls;
- optional data displays `-` instead of adding/removing rows;
- replan indication may still change color/text, but cannot move any other text;
- calculation result is always visible during playback so a partial failed trajectory
  is not silently presented as a successful solution.

Interpretation of previous daytime tests:
- they were not fake: they exercised real production planner/follower/control/physics
  components and found genuine local failures;
- however the composite fixture staged the scenario in manually authored phases and
  handcrafted AcceptedManeuverPrograms, so it did NOT prove a free-running arbitrary
  start->world->finish orchestration loop;
- the live stand exposed exactly that missing integration gate.

Important remaining architecture gap:
- current live stand still converts Planner output to a locally authored quintic
  `makeShortProgram`; it does not yet drive every nominal/replan segment through the
  full production B5/B6/B7/B8 physical compile/proof/selection/acceptance chain;
- therefore the next task is to replace that local adapter with the actual production
  maneuver compilation/proof chain before treating live-stand success as navigation
  acceptance evidence.

Validation state: snapshot-age and fixed-HUD patches are committed but not yet target
recompiled/replayed after this video.

## 2026-09-20 architecture correction — global corridor vs local dynamic avoidance

User clarified the intended ownership and this matches the existing LocalAvoidancePlanner contract:

- the nominal global route/corridor is built once from start to finish;
- it remains authoritative while the destination and static navigation world remain unchanged;
- moving obstacles do NOT trigger global route reconstruction;
- dynamic snapshots are consumed only by a bounded local monitor/avoidance layer;
- local avoidance may temporarily leave the nominal corridor, then progressively reacquire it;
- follower/autopilot continuously executes the current accepted trajectory/control program;
- the global planner is invoked again only when the goal changes or the static navigation world/corridor revision becomes invalid.

`maxResultAgeSeconds` / the ~0.25 s freshness limit is therefore a dynamic-snapshot safety contract, NOT a global replanning cadence.

Current live stand violates this architecture because it calls NavigationRuntimePlanner::plan repeatedly and therefore recomputes the static corridor every short execution slice.

Important current capability gap:
`NavigationSpace::queryCostedCorridor` currently returns a region/portal corridor and portal centers. Exact static NavigationObstacle geometry is used to prove/reject local segments, but the global corridor search does not yet synthesize a full geometric path around arbitrary exact obstacles inside one coarse region.

Therefore the default one-region + wall live scenario cannot honestly demonstrate the requested `start -> finish` global corridor yet. A proper global geometric corridor product must be added/cached first, then local dynamic avoidance must operate against that immutable nominal corridor.

## 2026-09-20 clarification — corridor is not the physical collision tunnel

User clarified terminology:
- `corridor` is a navigation/test abstraction around the nominal route used to ask
  whether the ship can generally proceed along that route;
- it is NOT the authoritative physical swept volume of the Cobra;
- exact wall contact / aperture passage must be decided by the future physical
  `tunnel` product: the time-parameterized swept volume of the actual hull along the
  accepted trajectory, including body orientation;
- therefore corridor visualization may be approximate and route-centric;
- tunnel proof is where it becomes critical whether Cobra's real hull clips walls.

Implication:
- do not over-engineer global corridor generation as exact hull clearance geometry;
- global planning may retain a centerline/polyline + coarse corridor for navigation;
- exact physical feasibility remains downstream ownership of trajectory/tunnel proof
  against exact static geometry and dynamic occupancy.

## 2026-09-20 full-chain evidence audit — why green slices did not mean green navigation

Re-audit performed after the live 3D stand exposed bad end-to-end behavior.

### Core conclusion

The previous tests were not fictitious: many of them exercised real production
Follower, PilotSkillExecutor, propulsion/physics, exact HitVolume geometry,
NavigationMap, NavigationSpace and parts of NavigationRuntimePlanner.

However, the evidence boundary was repeatedly over-interpreted. Most green tests
proved a component or a pre-authored slice, not the autonomous full chain:

`world/objective -> one retained global route -> local geometric response -> physical
maneuver generation -> continuous tunnel/proof -> decision -> ACCEPT -> follower ->
pilot -> physics -> event-driven monitor/replan -> finish`.

### What the main green test families actually proved

`ManeuverProgramExecutionLabTests`, `ManeuverCorridorMatrixTests`,
`ManeuverFlyThrough3dTests`, `ManeuverRigidBodyCorridorTests` and much of
`ManeuverChainedLimitMatrixTests` construct AcceptedManeuverProgram objects in the
test itself, then execute them through the real Follower/Bridge/Pilot/physics stack.
They prove execution/tracking of an already-good authored program. They do not prove
that production planning can generate that program from arbitrary world truth.

`NavigationRuntimePlannerTests` use deliberately authored region/portal fixtures. For
example the forced detour is already encoded by three regions and two known portals.
This validly proves topology/portal selection and local planner semantics, but not
automatic route synthesis around arbitrary raw obstacle geometry.

`OrdinaryPhysicalManeuverCompilerTests` genuinely test production B5 Newtonian
maneuver generation. They also explicitly pin that Assisted is unsupported in the
current first B5 slice, and candidates still require downstream continuous proof.
Therefore successful Assisted execution matrices were execution proofs of authored
programs, not proof of a production Assisted B5/B6/B7/B8 chain.

`NavigationCompositeProvingGroundTests` uses real planner/follower/physics components
but glues them together with test-owned helpers. Its topology is pre-authored through
portals; `makeProgram()` authors quintic programs; `buildDoctrineChoice()` authors
candidate programs/decision annotations; `fitAuthorityBoundedReplacement()` is a
test-side physical fitter. The file itself documents that production general B5
authoring remained a migration gap. The composite therefore was a proving ground,
not a production orchestrator.

The authoritative GameSimulation NavigationRuntimeLab was a real live test and remains
strong evidence for its narrow scope: real world objects/HitVolumes, real map/space,
real planner/control/physics and replication. But its slit/tunnel topology, approach,
entry/exit portals, start and goal are deterministic authored fixture data. It proves
that the live runtime can execute that authored topology/local situation; it does not
prove arbitrary `start + raw obstacles + finish -> full route/program` synthesis.

### Existing repository documentation already warned about this

`NAVIGATION_PIPELINE_AUDIT.md` explicitly says a unit test passing inside one stage is
not enough and records P6 ordinary maneuver generation / P7 proof integration / P8
ordinary decision integration / P9-P10 handoff as incomplete or transitional.

`NAVIGATION_V2_BLOCK_ARCHITECTURE.md` states B3 is event-driven, B5 current production
compiler is Newtonian-only, B6 proof is mandatory before ACCEPT, and the follower may
only execute the already accepted maneuver.

The project mistake was therefore not lack of warnings in code/docs; it was promoting
slice-level green evidence to a broader 'system works' interpretation.

### Why the new live stand looked dramatically worse

The new stand attempted, for the first time in this workflow, to synthesize much more
of the chain from a raw JSON scenario inside one executable. That immediately exposed
missing orchestration and also introduced new stand-side integration defects:
- absolute simulation time was mistakenly supplied as dynamic snapshot age, causing
  StaleHold/fail-closed braking;
- the stand periodically called the combined NavigationRuntimePlanner, incorrectly
  recomputing global/static planning instead of retaining one global route;
- the stand still uses its own `makeShortProgram()` quintic adapter, bypassing the
  complete production B5/B6/B7/B8 compile/proof/selection/acceptance chain;
- its default one-region + wall scenario asks current topology machinery to do a more
  general raw-obstacle route-synthesis job than the earlier authored-portal fixtures.

Thus the bad video is not evidence that Follower/PilotSkill/physics were fake. It is
evidence that the missing orchestration/handoff layers were never fully proved, and
that the newly written stand-side glue was itself incorrect.

### Evidence policy from now on

No collection of component/slice tests may be described as full navigation acceptance.
A true end-to-end acceptance must start from world/objective input and use the same
production products/handoffs as the game without test-authored maneuver programs or
test-only route/physics adapters.

Every test/result should be classified as one of:
- component/unit proof;
- execution of authored program;
- planner/topology slice;
- authoritative authored-world integration;
- true autonomous scenario end-to-end.

Only the last category may support a statement that the complete navigation chain
works for the scenario being tested.


## 2026-09-20 — Stage split implemented: Stage 1 static nominal route

The navigation diagnostic workflow is now explicitly split into two stages.

### Stage 1 — current implementation

One retained nominal route is built once from start to finish through static obstacle
geometry.

New production-facing component:
- `src/game/navigation/NominalRoutePlanner.h/.cpp`.

It wraps the existing `GeometricPathPlanner` backend behind an explicit v2 ownership
contract and returns a sparse polyline only. It accepts optional required authored
checkpoints and a coarse navigation envelope/clearance.

Nominal-route validity is revision driven:
- goal revision changed -> rebuild;
- static-world revision changed -> rebuild;
- dynamic-world revision changed -> **do not rebuild**.

The dynamic revision is deliberately present in `ValidityQuery` and deliberately
ignored by nominal-route invalidation. A regression test pins this behavior.

The live diagnostic stand now runs only Stage 1 on `РАССЧИТАТЬ`:

```text
JSON start/checkpoints/finish
 + static obstacles
 -> NominalRoutePlanner
 -> retained static route polyline
 -> viewer
```

It no longer runs the old half-second Planner/Follower/physics loop during Stage 1.
Therefore the two previously identified stand defects are removed from the canonical
Stage-1 path:
- no periodic global route replanning;
- no stand-local `makeShortProgram()` presented as production maneuver execution.

The ship remains at the start pose in Stage 1. The viewer shows the calculated route
and static obstacles only.

### Corridor/tunnel boundary

`route_envelope_radius_m` and `route_clearance_m` are coarse route/corridor inputs.
They are not exact physical collision proof. Exact Cobra wall/aperture contact belongs
to the later time-parameterized swept-hull tunnel proof.

### Dynamic-ready boundary

The scenario schema still accepts moving obstacles and a sudden obstacle. Stage 1 does
not use them for route reconstruction. Stage 2 will consume them as a local dynamic
overlay against the retained nominal route.

### Validation state

Code is committed but the new Stage-1 target has not yet been compiled/run on the
user's MinGW64 machine. Do not claim target acceptance until that run is supplied.


## 2026-09-20 Stage-1 cleanup lock

The Stage-1 diagnostic boundary was tightened after the initial implementation:
- old stand-local `makeShortProgram`, periodic planner loop, Follower/PilotSkill/
  SharedShipPhysics execution and dynamic publication code were removed from
  `NavigationScenarioRuntime.cpp` rather than merely left dormant;
- `tools/navigation_runtime/CMakeLists.txt` now links only the nominal-route/static
  geometry core for Stage 1;
- moving/sudden obstacle records remain parseable in the scenario schema as reserved
  Stage-2 inputs, but cannot influence the Stage-1 route;
- a new architecture checker pins that Stage 1 cannot regress into execution/replan
  ownership;
- target validation is still pending on the user's MinGW64 machine.


## Architecture-gate compatibility fix

During pre-handoff audit, the existing `check_geometric_path_planner.py` was found to
encode an obsolete ownership assumption: it required the same geometric planner .cpp
to appear twice in root CMake. Current runtime architecture already compiles it once in
shared `EliteNavigationGeometry`, which both client/server navigation runtime reuse.
The checker now pins that shared-library ownership instead of duplicate compilation.
This is an architecture-test correction only; route behavior is unchanged.


## 2026-09-21 target Stage-1 validation result

User target machine checkout: `5da0be0d05ef91958a0e7dc9adda3b4eb8fdee29`.

Observed:
- new `nominal_route_planner` test PASS;
- architecture suite stopped on an obsolete pre-System-frame marker in
  `check_navigation_live_runtime_control.py`;
- old Stage-2 `navigation_runtime_planner` still fails its adjusted-target fixture;
- old Stage-2 composite still fails at the narrow-passage full-hull check after a
  Newtonian dynamic bypass;
- all other 18/20 runtime tests passed.

Interpretation:
- Stage 1 static nominal route code itself passed its new behavioral test;
- the two red runtime tests are Stage-2 execution/local-dynamic regressions and must
  remain visible, but they do not invalidate Stage-1 static route acceptance;
- the architecture failure was not a behavior regression: the checker still expected
  `...Map...` control-field names while production had already migrated to explicit
  `...System...` names.

Fixes committed after this run:
- updated `check_navigation_live_runtime_control.py` to the current System-frame field
  names and `applySystemAccelerationDemand` API;
- labeled `nominal_route_planner` with CTest label `navigation_stage1`;
- added `tests/navigation_runtime/run_stage1_mingw64.sh` as the focused Stage-1 gate.

The focused gate runs only:
1. shared geometric-path architecture contract;
2. Stage-1 nominal-route architecture contract;
3. `nominal_route_planner` behavioral test;
4. Stage-1 viewer build.

This is not hiding the old red tests. It prevents Stage-2 failures from being
misreported as Stage-1 route-construction failures.


## 2026-09-21 — Stage-1 viewer observability fix after user screenshot

User screenshot showed a blank 3D area before pressing `РАССЧИТАТЬ`. After calculation the scene/route appeared, but there was no movement, making it impossible from the UI to tell whether Planner or Follower had failed.

Clarification: Stage 1 is route-only by design; Follower is not linked or executed. The UI was misleading because legacy playback controls remained visible as ordinary controls and the pre-calculation scene had no sufficiently explicit authored-scene rendering contract.

Fixes now committed:
- `TraceDocument` carries explicit authored `sceneStartMapMeters` / `sceneFinishMapMeters` independent of route existence;
- `loadScenarioPreview()` and calculated results populate those endpoints;
- viewer renders a reference grid, a large green START marker, a large yellow FINISH marker/ring, static obstacle geometry and Cobra-at-start before calculation;
- `fitCamera()` explicitly includes authored scene endpoints, so preview framing no longer depends on a calculated route;
- fixed diagnostics panel is placed below the top controls and shows the chain state without vertical reflow;
- Stage-1 playback controls are visually labeled `ПОЛЁТ: ЭТАП 2` / `FOLLOWER: OFF` and cannot start playback when only the one route frame exists;
- `ScenarioRunResult` now exposes diagnostics;
- calculation writes console diagnostics plus `tools/navigation_runtime/last_route_plan.log`;
- diagnostic chain explicitly distinguishes `SCENE`, `PLANNER`, and `FOLLOWER: NOT RUN (STAGE 1)`;
- Stage-1 architecture checker now pins pre-calculation scene endpoints, preview loading and diagnostic ownership.

This does not start Stage 2. No Follower/physics execution was reintroduced.

Target validation pending for these viewer/diagnostic changes.


## 2026-09-21 — Follower restored as Stage 2 of the same diagnostic stand

User correctly rejected the previous interpretation where splitting work into two stages
removed Follower/physics from the viewer entirely. The intended split is by ownership,
not by executable:

```text
Stage 1: Calculate
  static scene -> NominalRoutePlanner -> retained route polyline

Stage 2: Execute
  retained route polyline -> time trajectory -> Follower -> PilotSkill -> physics
```

The same viewer now supports both stages.

### Stage 1 remains unchanged

`РАССЧИТАТЬ` calls only `NominalRoutePlanner`. It builds the static route once. The
user's target evidence already showed a valid four-point 323.75 m detour around the
wall. Control law, pilot skill and flight style do not alter this nominal route.

### Stage 2 restored correctly

After a successful Stage 1 the playback button becomes `ЗАПУСТИТЬ ПОЛЁТ`.

Stage 2 consumes `calculatedRoute.routePoints` directly and MUST NOT invoke
`NominalRoutePlanner::plan`, `NavigationRuntimePlanner::plan`, or any other global
route search.

Current static Stage-2 chain:

```text
retained Stage-1 polyline
 -> TrajectoryGenerator / RuckigRoutePlanner
 -> time-parameterized trajectory
 -> bounded AcceptedManeuverProgram chunks
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> SharedShipPhysics + DynamicMotionSystem
 -> playback trace
```

The former stand-local `makeShortProgram()` quintic shortcut is NOT restored.

### Execution modes

The existing viewer selectors now become real Stage-2 inputs:
- Newtonian / Assisted -> physical local flight law + reference-attitude policy;
- Expert / Average / Loser -> real PilotSkillExecutor profiles;
- Standard / Extreme -> Ruckig route speed request.

Pilot execution starts from neutral revision zero so the first real route intent exercises
the selected pilot's reaction-delay model.

### Static-only dynamic boundary

Dynamic/sudden obstacles remain reserved for the next pass. Stage 2 currently reports
`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`; it does not silently use the sudden
obstacle checkbox and does not rebuild the route.

### Diagnostics

Stage 2 writes:
- stdout lines prefixed `[NAV-STAGE2]`;
- `tools/navigation_runtime/last_execution.log`;
- `tools/navigation_runtime/last_execution_trace.json`.

Diagnostics separate Ruckig trajectory generation, Follower, Pilot bridge, final errors,
route deviation and coarse static contact.

### Physical-proof limitation

The current static execution path uses the coarse route envelope for static collision
checks and is useful for observing Follower/physics behavior. It is NOT yet final B6
oriented swept-hull/tunnel proof. Do not promote a visually successful run to full
navigation acceptance.

### Validation state

This restored Stage-2 execution path is committed but has NOT yet been compiled/run on
the user's MinGW64 target. The last target-verified fact remains: Stage-1 nominal route
planner passed and produced the four-point 323.75 m static detour.


## 2026-09-21 — immutable retained route for Stage-2 comparisons

Viewer state now keeps the successful Stage-1 `TraceDocument` as a separate immutable
retained-route snapshot. Stage-2 execution traces no longer replace the only copy of the
Planner result.

Changing control law, pilot skill, flight style or the reserved sudden-obstacle toggle
after an execution run restores the retained Stage-1 route and marks only Stage 2 stale.
The next execution run reuses the exact same route without calling Planner again.

This is required for clean comparisons such as Expert/Newtonian vs Average/Assisted:
route geometry is held constant while only the execution layer changes.


## 2026-09-21 — target gate false negative in Stage-2 architecture checker

Target run stopped before compilation with:

`[FAIL] static-route/two-stage navigation: Stage-2 execution path missing TrajectoryGenerator::generate`

This was a checker defect, not a runtime defect. The checker isolated only the body of
`executeCalculatedRoute()`, while the actual Ruckig call is correctly delegated to
`buildExecutionTrajectory()`:

```text
executeCalculatedRoute()
 -> buildExecutionTrajectory()
 -> TrajectoryGenerator::generate()
```

Fix committed in `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`:
- execution function must call `buildExecutionTrajectory()`;
- trajectory-builder slice must contain `calculatedRoute.routePoints` and
  `TrajectoryGenerator::generate`;
- global-planner prohibition is checked across the owned Stage-2 trajectory-builder +
  execution text.

No production navigation/runtime behavior changed in this fix.

Validation state: target compilation of the restored Stage-2 viewer still has NOT been
reached. Rerun the focused gate from latest main.


## 2026-09-21 — second Stage-2 architecture checker false negative

Target gate stopped with:
`Stage-2 execution path missing DynamicMotionSystem::applySystemAccelerationDemand`.

Runtime call is present. The checker failed because production formatting splits the C++
scope operator/call across lines:

```cpp
game::navigation::DynamicMotionSystem::
    applySystemAccelerationDemand(...)
```

The architecture checker previously searched an exact single-line string.

Fix:
- added whitespace-insensitive C++ token normalization via `compact_cpp()`;
- Stage-2 execution and trajectory-builder call checks now compare normalized token
  streams rather than raw formatting;
- runtime behavior is unchanged.

Validation state: target compilation of the restored two-stage viewer still has not yet
been reached; rerun the focused gate from latest main.


## 2026-09-21 — Follower first-chunk handoff bug + full initial kinematics

Target Stage-2 execution reached real runtime and produced:
- cached Planner route OK;
- Ruckig trajectory OK, 1521 samples;
- 102 AcceptedManeuverProgram chunks;
- Follower FAIL after only 24 viewer frames;
- max Follower position error only 0.02 m;
- final position still ~295 m from goal.

The failure signature localizes the problem to the first AcceptedManeuverProgram chunk
boundary, not route tracking quality. At Standard 10 m/s the Ruckig sampling interval is
0.05 s for this scenario. A 16-sample program therefore spans about 0.75 s, matching the
~24 trace frames at 30 Hz before failure.

Root cause found in Stage-2 program selection:

```cpp
vehicle.timeSeconds + 1.0e-9 >= next.acceptedAtUniverseTimeSeconds
```

This can activate the next program fractionally before its acceptance time. The canonical
ManeuverProgramSampler treats any negative elapsed time as `BeforeStart`, and
TrajectoryFollower maps `BeforeStart` to `InvalidInput`.

Fix committed:
- remove the positive epsilon from future-program activation;
- keep the current chunk until simulation time is actually >= next acceptedAt;
- pre-sample the active program before Follower and record exact failure reason/index/
  execution time/program acceptedAt in Stage-2 diagnostics;
- architecture gate now forbids the early-switch expression and pins the new diagnostics.

The user's video is consistent with this: Cobra moves only a few meters, then execution
stops at the first chunk handoff.

### Initial ship state correction

The scenario previously supplied only position, velocity and body orientation. That was
incomplete for a physically continuous Newtonian execution handoff.

Stage-2 initial kinematic state is now explicitly:
- position P;
- linear velocity V;
- linear acceleration A;
- body orientation (forward/up basis);
- pitch/yaw/roll angular rates.

New scenario fields under `start`:
- `acceleration`;
- `pitch_rate_rad_s`;
- `yaw_rate_rad_s`;
- `roll_rate_rad_s`.

Default current scenario remains:
- P = (0,0,0) m;
- V = (6,0,0) m/s;
- A = (0,0,0) m/s2;
- forward = +X;
- up = +Y;
- angular rates = 0.

`TrajectoryGenerationRequest` now owns `initialAccelerationMps2`, validates it, and
passes it into the initial Ruckig state instead of hardcoding zero. A focused Ruckig route
test now requires initial acceleration to be preserved at the first trajectory sample.

The execution vehicle initializes pitch/yaw/roll rate from the scenario. No global route
recalculation semantics changed.

Validation state: these fixes are committed but not yet target-rebuilt/re-run.


## 2026-09-21 — test-matrix audit: why old movement tests were green while viewer execution failed

A full audit of navigation execution tests found a real coverage gap.

### What old tests actually covered

- `RuckigRoutePlannerTests`: Ruckig route/trajectory generation and obstacle safety only;
  no TrajectoryFollower, PilotSkill or authoritative physics.
- `ManeuverProgramExecutionLabTests`: real Follower -> PilotSkill -> physics, but the
  test directly authors a small number of AcceptedManeuverProgram phases. A 100 m line
  is one 16-sample program spanning the whole ~20 s maneuver. The right-angle case is
  explicitly `leg1 -> rotate -> leg2`.
- `ManeuverCorridorMatrix`, `ManeuverFlyThrough3d`, `ManeuverCornerFamilyMatrix`,
  `ManeuverSpeedDoctrineMatrix`, `ManeuverChainedLimitMatrix`: real execution, but
  again with hand-authored bounded physical programs, not raw Ruckig output.
- `NavigationCompositeProvingGroundTests`: Planner + Follower + PilotSkill + physics,
  but each physical phase is hand-authored from the current actual vehicle state;
  Ruckig is not used for the executed phase.
- sampler/tracker/pilot tests are component tests only.

Therefore no previous test exercised the exact viewer chain:

```text
NominalRoutePlanner
 -> retained route
 -> Ruckig full trajectory
 -> route-to-AcceptedManeuverProgram adapter
 -> Follower
 -> PilotSkill
 -> physics
```

### Root integration mistake

The viewer introduced a new untested adapter that copied every 16 consecutive raw
Ruckig samples into a new AcceptedManeuverProgram. The current 1521-sample trajectory
therefore became 102 tiny ~0.75 s programs.

That interpretation was wrong. In the green execution tests, the fixed 16 samples are
reference knots spanning one meaningful physical phase; they are not a transport packet
of 16 adjacent 0.05 s Ruckig samples.

### Fix

- removed raw consecutive-sample microchunking;
- retained global route is still calculated exactly once;
- Ruckig still parameterizes that route once;
- trajectory is divided by retained coarse-route legs;
- each route leg becomes one bounded AcceptedManeuverProgram with up to 16 reference
  knots spread across the whole leg;
- phase advancement now uses the already-tested `ManeuverPhaseGate`;
- each next phase is accepted at the actual current simulation time;
- for the current four-point route the expected program scale is ~3 physical phases,
  not 102 micro-programs.

### New missing regression added

`tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp` now exercises the
same pipeline as the viewer from `scenario.json` through Planner -> retained route ->
Ruckig -> Follower -> PilotSkill -> physics -> authored finish.

`tests/navigation_runtime/run_stage1_mingw64.sh` now builds the viewer and runs this
exact E2E regression. Component tests alone are no longer sufficient evidence for viewer
execution.

Target validation is pending. Do not claim the route-phase fix works until the user's
MinGW64 machine passes the new E2E test.


## 2026-09-21 — retained-route Stage-2 target execution PASS

User-supplied target diagnostics prove that the route-leg phase correction works in the
real diagnostic stand. The supplied output does not contain `git rev-parse HEAD`, so
this evidence is intentionally not attached to an inferred commit SHA.

Stage 1 remained unchanged:
- START = (0,0,0) m with V=(6,0,0) m/s;
- FINISH = (300,0,0) m;
- one static obstacle;
- 4 retained route points;
- retained route length 323.75 m;
- static detour YES.

Observed Stage-2 runs:

| Pilot / style / law | Ruckig samples | phases / handoffs | final P error | final speed | max route dev | max follower error | contact |
|---|---:|---:|---:|---:|---:|---:|---|
| Expert / Standard / Newtonian | 1521 | 3 / 2 | 0.15 m | 0.10 m/s | 3.26 m | 1.08 m | NO |
| Expert / Extreme / Newtonian | 1416 | 3 / 2 | 0.10 m | 0.48 m/s | 3.26 m | 0.95 m | NO |
| Expert / Extreme / Assisted | 1416 | 3 / 2 | 0.10 m | 0.48 m/s | 3.26 m | 0.95 m | NO |

All three runs report:
- `ROUTE EXECUTION COMPLETE: YES`;
- `FOLLOWER: EXECUTED`;
- `FOLLOWER FAIL REASON: NONE`;
- `PILOT BRIDGE: EXECUTED`;
- `COARSE STATIC CONTACT: NO`.

This directly closes the previous 102-microprogram failure mode. The current four-point
retained route is executed as three meaningful physical route-leg phases with two real
ManeuverPhaseGate handoffs.

The exact retained-route E2E gate now exercises the previously missing chain:

```text
scenario.json
 -> NominalRoutePlanner once
 -> immutable retained route
 -> Ruckig
 -> route-leg AcceptedManeuverPrograms
 -> TrajectoryFollower
 -> PilotSkillExecutor
 -> SharedShipPhysics / DynamicMotionSystem
 -> authored finish
```

This is strong static integration evidence, not full Navigation-v2 acceptance. The
current static contact check is still coarse route-envelope geometry, not the final
oriented swept-hull B6 tunnel proof.

### Next active slice — dynamic local overlay on the retained route

Static retained-route execution is now sufficiently stable to start the previously
deferred dynamic layer.

Hard ownership contract:
- dynamic actors never rebuild the retained global/static route;
- moving/sudden obstacles are a bounded local monitor/avoidance overlay only;
- surprise obstacles become visible only after their authored activation time;
- if a physically executable short bypass exists, execute that bypass;
- do not require an immediate return-to-line segment inside the same horizon;
- if no safe executable bypass exists, issue active braking while navigation remains
  alive and continues evaluating fresh world state;
- after the obstacle clears, progressively reacquire the retained route under vehicle
  limits rather than teleporting or enforcing an arbitrary merge distance.

Initial dynamic acceptance should keep the same Stage-1 route immutable and first prove
Expert/Standard/Newtonian. Mode/style expansion follows after the local dynamic chain is
behaviorally correct.


## 2026-09-21 — video review: static maneuver quality blocker

User video review exposed two real quality defects that must be fixed before the
dynamic-overlay stage is promoted.

### 1. Shallow retained-route corners incorrectly become StopTurnGo

For the previous default retained route, the static planner produced approximately:

```text
(0, 0, 0)
 -> (110, -27, -45)
 -> (190, -27, -45)
 -> (300, 0, 0)
```

The two interior course changes are only about 25.5 degrees. An Expert pilot has no
behavioral reason to stop there.

The stop is caused by the current Ruckig waypoint-authoring fallback:
- `buildWaypointVelocities()` attempts a 25%-leg-length shortcut chord around each
  interior polyline point;
- on this geometry that chord intersects the inflated static wall;
- the waypoint is therefore left with zero target velocity;
- Ruckig then correctly solves a stop-at-waypoint trajectory;
- both Newtonian and Assisted inherit the same upstream StopTurnGo reference.

This is a trajectory-authoring defect, not a sensible Expert maneuver doctrine.

### 2. Newtonian hull/velocity semantics are not yet expert-quality

Low-level propulsion allocation is physically differentiated:
- Newtonian has aft-main thrust only for the main channel; reverse/main braking
  requires body reorientation, while lateral/vertical correction is bounded by RCS;
- Assisted has symmetric longitudinal main authority, with lateral/vertical correction
  still bounded by RCS.

However the Stage-2 reference is authored in the wrong order for a
main-engine-dominant Newtonian maneuver:
- Ruckig first creates a translational P/V/A history using a common vector
  acceleration budget;
- `buildReferenceAttitudes()` then points Newtonian body-forward toward the already
  requested acceleration whenever |A| > 0.35 m/s2;
- bounded RCS can therefore begin changing the velocity vector while the hull is still
  catching up;
- before a zero-speed waypoint, the acceleration vector becomes braking-oriented, so
  the body reference rotates toward the braking vector and then rotates again for the
  next leg.

The resulting motion is physically possible as an RCS-heavy correction, but it is not a
credible Expert Newtonian route maneuver. Large course changes must be authored as
body/thrust-aware physical primitives before ACCEPT, not derived by attaching attitude
after an arbitrary translational Ruckig curve.

Assisted also stops at these shallow corners because the zero waypoint velocity is
already baked into the common translational trajectory.

### 3. Standard vs Extreme is not yet a real flight doctrine

In the current diagnostic Stage 2, Standard vs Extreme primarily changes the requested
route speed (10 vs 18 m/s). It does not yet choose different physical maneuver families,
clearance use, slip tolerance or aggressiveness. Therefore visually similar maneuver
semantics are expected and are not sufficient implementation of the requested flight
styles.

### 4. Static route detour was unnecessarily long

The old box support graph had:
- 8 corners;
- 6 face centers;
- no 12 edge midpoints.

For the default wall, inflated/support half-extents are approximately
`(40, 27, 45)`. Because edge midpoints were missing, visibility A* had to pay
clearance on two transverse axes and selected the old 323.75 m route through
`(110,-27,-45)` and `(190,-27,-45)`.

The geometrically shorter same-clearance route is approximately:

```text
(0, 0, 0)
 -> (110, -27, 0)
 -> (190, -27, 0)
 -> (300, 0, 0)
```

with length about 306.53 m.

Candidate fixes committed:
- `e53312cc9b00119e69f1c7676edf14b5d21fea64` — add all box edge-midpoint support
  nodes to `GeometricPathPlanner`;
- `8e0d4c4c6ca7d4d7ae3e7cc71387ebff8b335302` — regression requiring a nearest-face
  one-plane box detour rather than unnecessary second-axis displacement;
- `ac30d441ebdafe232af0bf0fe7e8882da9f38767` — viewer uses a Cobra-like triangular
  prism, a much smaller translucent nose marker, and a thick current-velocity vector
  whose length is proportional to speed, with numeric speed at the vector tip;
- `c4c63c9751a4b5b381da290a570ee98775148eb6` — diagnostics now print every retained
  route point, interior retained-waypoint speeds and maximum body/velocity angle.

These candidates are not target-validated yet.

### Active priority change

Dynamic local avoidance is postponed, not discarded. First make the static retained
route execution behaviorally credible.

Next mechanism target:
- shallow/medium corners must remain moving when a physically safe maneuver exists;
- no zero-speed waypoint merely because one synthetic corner-cut chord is blocked;
- Newtonian Expert must use a body/thrust-aware maneuver (lead turn, drift/coast,
  RCS trim, main-burn as appropriate) rather than letting the translational vector
  rotate independently and asking attitude to catch up afterwards;
- Assisted Expert must also preserve speed on ordinary shallow bends;
- stop/flip is reserved for geometry, required terminal pose or braking physics that
  actually requires it;
- Standard and Extreme must differ by real maneuver doctrine/aggressiveness, not only
  by max-speed scalar;
- production B5/B6/B7/B8 ownership must be preserved. Do not turn geometric targets
  directly into accepted execution programs to make the viewer look good.

The already documented architecture gap remains decisive: ordinary B5 exists first for
Newtonian, but a generalized ordinary B6 continuous maneuver prover is still missing.
Do not bypass that proof boundary.


## 2026-09-21 — adaptive corner pass candidate after user retest

User retest confirms:
- the new nearest-face nominal route is logically acceptable;
- the full stops at the two shallow intermediate points remain;
- the program-reference arrow visibly leads the physical hull;
- the current velocity vector was not legible enough in the viewer.

The supplied performance log confirms the stop mechanism is still active: the latest
four-point / one-obstacle Stage-2 solves finish with `blended_waypoints=0`.
Therefore both interior waypoints are still authored as zero-speed points before this
candidate.

### StopTurnGo fallback changed

Candidate commits:
- `d2205f50259fdef05a6515fec3822055891c47d5`:
  `buildWaypointVelocities()` no longer tests only one 25%-of-leg corner chord.
  It searches from the widest proposed local blend toward a bounded smaller blend and
  keeps the widest collision-clear option. If a generated Ruckig leg still fails,
  through-speed is reduced progressively (70% steps) before falling back to zero.
- `641e6ef6aeb79d4dddcf58ce7601448723f79b9a`:
  removed the arbitrary global `0.65 * maxSpeed` waypoint cap. Curvature/available
  lateral acceleration and explicit speed limits now own the corner-speed limit.
- `20aef47942c675ae59b04c288b2ae4b3cb6de5e6`:
  focused Ruckig regressions now distinguish:
  1. wide corner chord blocked but tighter continuous turn available -> must keep moving;
  2. even minimum local blend blocked -> safe stop remains legal;
  3. the current default wall detour -> both shallow interior points must retain
     non-zero through-speed.

This makes the retained white polyline a geometric/topological intent rather than a
literal StopTurnGo execution prescription. Ruckig remains responsible for a continuous
time trajectory through the retained route; the candidate does not reintroduce the
retired custom B-spline backend.

If the target gate still collapses either default wall waypoint to zero, the next step
is to add explicit maneuver-space offset / corner-radius reserve to the execution path
rather than accepting StopTurnGo. Slightly lengthening the geometric route is allowed
when required to create real turn room.

### Viewer observability changed

Candidate commits:
- `cbdbcc2b2132f0ef29af9a73e3589d25c415a8f5`;
- `2ad3bf086c153895adefee64fb9f67bcabaa84da`;
- `f695a55454f5ccc5802c618347dfb400ec4d6bcb`.

Changes:
- speed number moved away from the ship into a fixed lower-right block;
- current velocity vector is now bright yellow, 6 px wide and 3.5 world-meters per
  1 m/s, so 10 m/s is about a 35 m arrow;
- short cyan arrow = actual physical hull nose;
- red arrow = Follower/program target nose;
- white line = retained geometric route;
- green line = actual flown trajectory;
- the lower-right block states these meanings explicitly.

Interpretation rule for the next video:
- red leading cyan by itself means the attitude reference is commanding ahead of the
  physical hull and is not automatically wrong;
- the important Newtonian fault is if the **yellow actual velocity vector** bends as
  though the requested thrust attitude were already achieved, rather than following
  the acceleration authority of the actual cyan-oriented hull/RCS system.

The deeper Newtonian architecture issue remains open: generic translational Ruckig P/V/A
is still authored before the final body/thrust-specific maneuver. The adaptive corner
candidate is intended to remove the clearly invalid zero-speed fallback first, not to
declare the complete B5/B6 Newtonian maneuver problem solved.

### Validation status

UNVERIFIED on target MinGW64.

Run the focused Ruckig route tests plus the two-stage viewer gate before promoting this
candidate.


## 2026-09-21 — target corner gate 7/8; convert Test 1 to moving-start/moving-finish

Target MinGW64 evidence from the focused Ruckig suite:
- straight route: PASS;
- diagonal stopped leg: PASS;
- clear corner keeps through velocity: PASS;
- blocked wide blend shrinks before stop: PASS;
- default wall shallow corners stay moving: PASS;
- initial acceleration preservation: PASS;
- impossible braking rejection: PASS;
- the only failure was the synthetic `truly blocked corner falls back to stop`
  assertion.

The important result is that the actual default four-point wall case now passes the
focused requirement that both shallow intermediate corners retain non-zero speed. The
single failure was not a safety failure: the test assumed that one tight fixture must
force zero speed. That assumption is stronger than the architecture contract. If the
solver proves a collision-free continuous passage, stopping only to satisfy the test is
wrong. The fixture is therefore retained as a swept-safety check, not as a mandatory
StopTurnGo assertion.

### Test-1 isolation change

For the default Standard stand, remove start/finish transients from the visual test:
- authored Standard max speed = 10 m/s;
- start velocity changed from 6 m/s to 10 m/s;
- finish speed changed from 0 m/s to 10 m/s.

The intent is a steady 10 m/s fly-through test:
```text
start already at 10 m/s
 -> shallow corner 1
 -> straight bypass leg
 -> shallow corner 2
 -> cross finish still at 10 m/s
```

This isolates the two turn maneuvers from startup acceleration and terminal braking.

### Moving terminal velocity support

The previous runtime had two artificial stop constraints:
- `executeCalculatedRoute()` rejected any non-zero authored finish speed;
- `RuckigRoutePlanner::plan()` unconditionally overwrote the final waypoint velocity
  with zero.

That is now replaced by an explicit exact terminal-velocity contract in
`TrajectoryGenerationRequest`:
- `hasTerminalVelocity`;
- `terminalVelocityMps`.

Point speed constraints remain upper bounds; the new terminal velocity is the authored
final inertial velocity.

Candidate commits:
- `c9a8e381531feee4516cafa436b67809fb2d753d` — request contract;
- `9372af939d2406768e530f9e2c02e05389f0df83` — Ruckig honors exact moving terminal;
- `82dc142e5c06f8be8e6c94aec810f16b8b47302f` — runtime accepts/authors moving finish;
- `75f11481979b83706861bbc98f8b6326341a07e0` — default stand start=10, finish=10;
- `d40e4fdc7cb1048d0f45daf2598788684e49affc` — focused tests use moving terminal and
  remove mandatory-stop assumption from tight corner;
- `bc5fbaa284663d7a986e2257b8d71f4a64a610ba` — focused straight Test 1 is a true
  steady 10 m/s transit;
- `7f57df3ef4a05d6601d05aa9c94de7121e0d9884` — retained-route E2E now requires
  crossing the finish at the authored 10 m/s.

These latest moving-terminal changes are not yet target-validated.

### Next target evidence

Run focused Ruckig first. Expected qualitative result:
- 8/8 focused tests;
- default wall shallow corners moving;
- straight Test 1 starts and ends at 10 m/s without an authored stop.

Then run Stage-1/Stage-2 viewer. For Expert/Standard/Newtonian, the useful visual is now
almost entirely the two corners because start and finish no longer require speed
transients.

After that, inspect yellow actual velocity vs cyan actual hull nose vs red target nose.
The deeper Newtonian body/thrust-aware maneuver-authoring issue remains open.


## 2026-09-21 — viewer-first maneuver inspection; moving finish is fly-through

Latest target output after switching the stand to start=10 m/s and finish=10 m/s showed:
- retained route remains 306.53 m with four points;
- Ruckig trajectory generated successfully;
- both interior retained waypoints are now exactly 10.00 m/s;
- no coarse static contact;
- the run failed only at the final phase with `FINAL_CAPTURE_TIMEOUT`;
- by timeout, final speed had collapsed to 1.01 m/s and final position error had grown to
  32.94 m;
- maximum body/velocity angle reached 178.94 degrees.

This failure was caused by a semantic mismatch in the stand, not by the two shallow
corner speeds. The last phase always used `ManeuverPhaseGate::StateCapture`, which is
correct for a parking/stopped terminal but wrong for an authored moving terminal. After
the nominal moving trajectory ended, the follower kept trying to capture a fixed final
position while also owning a non-zero terminal velocity reference. That produced the
artificial braking and eventual timeout.

Candidate fix:
- `9b1251e8601172ecd83ba4f656871f92f4669fb4` — a non-zero finish speed now makes
  the final phase `ScheduledMoving`; the terminal is a fly-through boundary rather
  than a parking capture.

Process change requested by user:
- current maneuver development is inspected visually in
  `navigation_runtime_viewer.exe`;
- the viewer preparation script must not auto-run the headless Stage-2 E2E and abort
  before the user can inspect the maneuver;
- `06513569f7861ac8b6e3104994ec72ffd0d891d1` removes that automatic headless
  Stage-2 run from `run_stage1_mingw64.sh`;
- focused/static architecture tests still run, and the viewer is still built;
- the headless E2E target remains available separately for later regression work.

Current visual objective:
```text
10 m/s at start
 -> corner 1 at moving speed
 -> bypass leg
 -> corner 2 at moving speed
 -> cross finish at 10 m/s
```

Do not interpret a red target-attitude arrow leading the cyan physical nose as a fault
by itself. The key Newtonian diagnostic remains whether the yellow actual velocity
vector changes consistently with the real cyan hull attitude and bounded RCS/main-thrust
authority. The 178.94-degree body/velocity diagnostic in the failed headless run is
strong evidence that this deeper maneuver-authoring issue remains.


## 2026-09-21 — expose the real calculated curve and replace exact-corner forcing with a rounded execution guide

User viewer evidence showed visible braking / path distortion on apparently straight
parts of the retained four-point wall route and requested that the actual calculated
"spline" be drawn.

Important correction: production runtime does **not** use the retired
`SmoothPathOptimizer`. The `SmoothPathPerf` records that can still appear in the
aggregate navigation performance log come from legacy/test compatibility runs. The
current runtime route-to-motion backend is the canonical Ruckig path. Therefore the
curve that must be inspected is the complete Ruckig reference trajectory, not a hidden
B-spline.

### Runtime geometry change

The previous Ruckig route authoring still forced each coarse Stage-1 support vertex to
be an exact target position while assigning a non-zero bisector velocity there. That
can create an unnecessarily awkward local state constraint: the solver must hit one
mathematical corner point and simultaneously leave it already rotated in velocity
space.

New candidate behavior:
- Stage-1 `routePoints` remain retained and unchanged.
- Stage-2 derives a separate collision-checked `executionGuide`.
- For every interior coarse vertex it computes a physically useful tangent reserve from
  authored speed and lateral acceleration.
- The coarse vertex is replaced locally by entry/exit guide points.
- If that useful corner does not fit, the local corner can be moved farther outward into
  free space in steps instead of tightening the curve or forcing StopTurnGo.
- Ruckig parameterizes this guide.
- The full guide is published in `TrajectoryGenerationResult::executionGuidePointsMeters`.

Candidate commits:
- `0b4212345b2c38b4f775937060a81ba56cace219` — expose execution guide;
- `939a7058ba58a244282d219e0c298150a72a410f` — rounded/widenable execution guide;
- `77f9640eb77267be6a258b0293720e00f089fbe6` /
  `f9b106f8c259c0f7a39ebde11f67c776e1a35c92` — guide diagnostics.

### Viewer change

The trace now carries two new Stage-2 products:
- `executionGuidePoints`;
- `calculatedTrajectoryPoints` (all Ruckig samples).

Viewer colors:
- white = retained coarse geometric route;
- blue = widened local execution guide;
- purple = complete calculated Ruckig curve;
- green = actual flown trajectory;
- yellow arrow = actual velocity;
- cyan short arrow = real hull nose;
- red arrow = program target nose.

Blue guide support points are also drawn as small crosses.

Candidate commits:
- `3ee684742afd8ad91307b99c0a1815bd30aea722` /
  `287ab085275873ffb1f991e09902d31706edb927` — trace schema/persistence;
- `33e3b228327cfb9c6031ecb09aa67847d16d2cda` — runtime publishes guide and full
  reference plus calculated min/max speed;
- `154d9e1a544dc957769efce127b70ee36dd8b0cc` — viewer renders the products.

### Focused regression

The default wall Ruckig test now additionally requires:
- execution guide has more support points than the four-point coarse route;
- calculated reference minimum speed stays >= 7.5 m/s in this steady 10 m/s stand;
- calculated reference does not geometrically backtrack along +X.

Commit:
- `68f4377aabae1fd894be4a882fb30b0cec762d89`.

This candidate is **not target MinGW64 validated yet**.

### Immediate diagnostic objective

In the next viewer run compare:
1. white coarse route;
2. blue execution guide;
3. purple calculated Ruckig curve;
4. green actual path.

Also inspect:
- `CALCULATED MIN SPEED`;
- `CALCULATED MIN SPEED POS`;
- `CALCULATED MAX SPEED`.

If purple is smooth and remains near 10 m/s while green brakes/loops, the fault is
downstream in Follower / body-thrust execution. If purple itself brakes or loops, keep
the fix in route-to-trajectory authoring and widen/reject the guide before ACCEPT.


## 2026-09-21 — purple reference itself bows at both rounded corners; replace chord-like guide with sampled C1 curve

User viewer evidence now isolates the defect conclusively:
- the purple calculated Ruckig reference itself has two visible lateral "barrels";
- they occur at the two rounded-corner transitions;
- therefore this is upstream of Follower/Pilot. The green actual path is not the primary
  cause of these two visible bulges.

Latest perf line for the new guide shows:
- coarse_points=4;
- guide_points=6;
- rounded_corners=2;
- expanded_corners=0;
- blended_waypoints=4;
- samples=2300;
- valid=1.

The six-point guide proved that merely replacing each coarse corner with one
`entry -> exit` chord is insufficient. Each Ruckig leg is solved with non-zero
endpoint velocities. When those velocity directions are not aligned with the leg chord,
Ruckig is free to generate a lateral polynomial bow while still satisfying endpoint
states.

### Mechanism fix

Each rounded corner is now represented by a densely sampled quadratic C1 curve:
```text
entry -> quadratic samples controlled by widened corner -> exit
```

Properties:
- the quadratic control point is the current local corner (including any outward
  expansion);
- its tangent at entry matches the incoming coarse leg;
- its tangent at exit matches the outgoing coarse leg;
- sample density targets about 2.5 m per segment, clamped to 4..24 segments per corner;
- the sampled curve itself is collision checked before acceptance;
- if the curve is not safe, the existing cut/expansion search continues.

This removes the long single Ruckig leg whose endpoint velocities were forcing the
visible barrel.

Commit:
- `3dccb8a36c8e8023083b21f317b944a8097fd6a0`.

### Small-angle speed fix

The old through-speed estimate clamped
`sin(angle/2)` to at least `0.15`. Once a curve is sampled densely, each local angle
is intentionally very small. That clamp falsely treated tiny smooth direction changes
as a much tighter turn and could create unnecessary braking.

The floor is now numerical only (`1e-4`), while the explicit max speed and lateral
acceleration remain the physical bounds.

### Focused regression

Default wall regression now additionally measures the full calculated Ruckig reference
against the published execution guide.

Requirement:
- maximum distance of any purple Ruckig sample from the blue execution-guide polyline
  must be <= 1.5 m.

Existing requirements remain:
- no major braking dip below 7.5 m/s in the steady 10 m/s experiment;
- no geometric backtracking along +X;
- collision-free;
- moving finish remains moving.

Commits:
- `517904389019fbf94ddbbabbe3248cdb7b6fab20` — visible-bow regression;
- `a056973c674194b109b8f37646f6d7a9e461c5b9` — explicit test include.

This candidate is not target MinGW64 validated yet.


## 2026-09-21 — target 7/8 after dense curve: functional speed-dip failure, not compile failure

Target MinGW64 evidence:
- `ruckig_route_planner_tests.exe` compiled and linked successfully;
- 7/8 focused tests passed;
- only failure:
  `default wall shallow corners stay moving`;
- failure reason:
  `default wall calculated curve contains a major unnecessary braking dip`.

This is a **functional trajectory test failure**, not a compiler/linker failure. The
test is intentionally retained: the steady 10 m/s wall stand must not hide a large
calculated speed dip.

### Root cause found

The sampled C1 execution guide introduced short local segments, but
`buildWaypointVelocities()` still computed corner speed from a heuristic
`blendDistance` proportional to local segment length.

That made speed depend on sampling density:
```text
same geometric curve
 + more guide samples
 -> shorter local segments
 -> smaller blendDistance
 -> lower turnSpeed
```

This is non-physical.

Candidate fix:
- use the three-point circumcircle curvature instead:
  `kappa = 2*|AB x BC| / (|AB| |BC| |AC|)`;
- local lateral-acceleration speed limit:
  `v_max = sqrt(a_lateral / kappa)`;
- this is invariant to guide discretization density;
- Ruckig perf log now records `min_speed_mps` and `max_speed_mps`.

Commit:
- `4dcebe77580e8abc7a0f9f4a22cec65f3ba985d5`.

### Remove start/finish heading transients from the steady stand

The prior "10 m/s start and finish" still had a hidden directional transient:
- start velocity was +X while the first retained route leg points
  `(110,-27,0)`;
- terminal velocity was +X while the final retained route leg points
  `(110,+27,0)`.

So the stand still asked Ruckig to perform extra heading changes at both boundaries.

The default scenario and focused wall fixture now align:
- start velocity and hull forward with the first route-leg tangent;
- finish velocity/forward with the final route-leg tangent;
- magnitude remains exactly 10 m/s.

Commits:
- `5c408b76b36a86bd6b4fecacc377142d80726796`;
- `3a22a4ae0aca037df4c9b8c76759506ed8a840c6`.

This makes the experiment genuinely:
```text
already moving along route at 10
 -> corner 1
 -> bypass
 -> corner 2
 -> leave along route at 10
```

### Viewer source revision control

Per user request, every viewer build now displays the exact Git source revision:
- CMake runs `git rev-parse --short=12 HEAD`;
- compile definition `ELITE_NAV_VIEWER_REVISION` is injected;
- the revision appears in both the native window title and the in-window HUD.

Commits:
- `2c1baf4854f083c2e3e44ed16136c70237dfafcd`;
- `1e84a9cac2f26da8e2802e1b97e8582f869f218d`.

The revision shown by the viewer is the HEAD that existed at CMake configure time. The
normal `run_stage1_mingw64.sh` configure/build flow refreshes it after every pull.

All changes after the reported 7/8 target run are currently unverified on target.


## 2026-09-21 — web review confirms dense 3-D Ruckig waypoint chaining is the wrong integration

Latest target evidence:
- focused binary still compiles/links;
- 7/8 tests pass;
- default wall test reports `min_speed=0.009 m/s`;
- viewer shows a large fan / convergence of purple calculated reference segments near
  the rounded route node.

The performance log explains the visual failure. After densifying the corner guide, the
runtime started feeding many close geometric samples to the 3-D state-to-state Ruckig
adapter. Recent lines show examples such as:
- `guide_points=52`, hundreds/thousands of Ruckig leg attempts;
- current default wall case `guide_points=18`, `legs=74`,
  `ruckig_ok=17`, `min_speed_mps=0.0094`.

This fan is therefore not a mysterious renderer artifact. It is the result of our own
integration: every close guide point became a new 3-D target state with its own target
velocity. Ruckig then solved many local polynomial state-to-state transitions and the
route nearly stopped at one of them.

### External verification

Official Ruckig documentation says:
- Ruckig's basic/open-source strength is state-to-state online trajectory generation;
- full local intermediate-waypoint trajectory calculation is a Pro feature;
- waypoint problems are significantly harder;
- Ruckig recommends as few waypoints as possible;
- it explicitly recommends filtering waypoint lists and prefers waypoints far apart.

This matches the target evidence exactly. The current dense chaining was a misuse of the
state-to-state solver.

The standard alternative architecture is also consistent with TOPP/TOPPRA:
```text
geometric path p(s)
    +
time parameterization s(t)
    =
trajectory p(s(t))
```

### Architecture correction implemented

Do not feed dense spatial samples to 3-D Ruckig.

New ownership:
```text
Stage-1 coarse route
 -> local rounded execution guide p(s)
 -> scalar jerk-limited Ruckig progress s(t)
 -> map s(t) back onto p(s)
 -> swept collision validation
 -> AcceptedManeuverProgram / Follower
```

Ruckig remains in the architecture, but in the role it is good at:
- true single-leg state-to-state solves still use the existing 3-D adapter;
- curved / multi-point routes use new one-dimensional
  `RuckigTrajectorySolver::solveProgress()`;
- dense guide points define geometry only and are never independent 3-D Ruckig targets.

Commits:
- `2e815ef993a31acd40596f170c67a124f9337bf9` — scalar progress API;
- `a50de59958887efbe50e2cdacee8c67623b2ebb8` — scalar Ruckig implementation;
- `9f166f4286dd03197705b0cd9e4ba63e33d000bc` — path-progress trajectory mapping;
- `3c58f4c3719d712e524e86e5f030820486af3102` — stop chaining dense 3-D legs;
- `c6f7b160878a3582362574691a12c96fd3ff3623` — keep stored guide sparse;
- `b9e7ce64182ce761edb45b4751d680950ab33ff5` /
  `3843194d999e302ca32171de3d49f3f21987de64` — contract docs corrected.

The current scalar timing uses one conservative path-wide speed cap for the current
slice (including curvature/range/positive point limits). This deliberately favors a
correct monotone reference over aggressive per-corner optimization. Later work may
introduce a sparse set of meaningful speed zones, but must never return to one Ruckig
3-D solve per geometric sample.

### Viewer revision and clutter

The previous revision stamp could be outside a cropped screenshot. Viewer now also
draws an always-visible in-window `NAV REV <sha>` badge below the top controls.

Per-sample cyan execution-guide crosses were removed. The BLUE guide line remains, but
internal geometric sampling no longer looks like dozens of commanded target states.

Commit:
- `1199a0c04ee7cf3a6938bef0ca9c3bb55d7bd4c8`.

### Validation state

This architecture correction has NOT yet been built or run on target MinGW64.
Do not call it PASS until the focused target gate and viewer confirm:
- no near-zero speed on the steady 10 m/s wall route;
- no purple fan/barrels;
- `NAV REV` visible and matches the current build revision.


## 2026-09-21 — excessive hull rotation isolated to Newtonian attitude authoring

Latest viewer/video evidence after scalar path-progress correction:
- purple calculated path is much cleaner;
- current wall-case perf reaches `min_speed_mps=10.0000`,
  `max_speed_mps=10.0000`, `guide_points=10`, one scalar Ruckig solve;
- remaining visually dominant defect is large hull rotation relative to the velocity
  vector during the shallow turn.

The cause is explicit in `buildReferenceAttitudes()`, not hidden physics:
```cpp
if (law == Law::Newtonian && acceleration > 0.35)
    requestedForward = normalize(sample.accelerationMps2);
```

For a constant-speed curved path, acceleration is predominantly centripetal and
approximately perpendicular to velocity. That rule therefore commands the ship to point
nearly broadside to its direction of travel even for a gentle turn.

This is especially wrong for the current stand because:
- `executionVehicleProfile.maxLateralAccelerationMps2` is already derived from
  `ShipParams::manoeuvreThrusterAccel`;
- Cobra test profile has `manoeuvreThrusterAccel = 2.0 m/s2`;
- the path curvature/speed combination is generated under that same lateral-authority
  envelope;
- therefore a gentle constant-speed turn that fits inside RCS authority does **not**
  require rotating the main engine into the centripetal acceleration vector.

The apparent sideways/out-of-plane motion in the video is primarily the commanded
broadside attitude viewed in perspective. The route and requested acceleration are
planar in this stand; the reference basis transport itself is designed to preserve the
previous roll orientation.

### Attitude fix candidate

Newtonian reference attitude now uses a minimum-cant allocation:

1. Default nose direction is velocity/path tangent.
2. If total requested acceleration is within manoeuvre/RCS authority, keep the nose on
   velocity.
3. If main-engine participation is required, calculate the smallest nose rotation such
   that:
   ```text
   positive main-engine thrust along nose
   + bounded omnidirectional RCS
   = requested acceleration
   ```
4. Do **not** point the nose directly at acceleration unless the physical demand truly
   requires that much rotation.

This keeps the hull close to the flight direction for ordinary gentle Newtonian turns,
while still allowing large cant/flip for demands that RCS cannot satisfy (hard braking,
high lateral acceleration, etc.).

Commits:
- `fcffa5bae3b4e3deab5d6f043d3c500137719dad` — minimum-cant Newtonian reference;
- `b943064d529935243863c30238ae0daf62f1c129` — report
  `MAX REFERENCE/VELOCITY ANGLE` separately from actual
  `MAX BODY/VELOCITY ANGLE`.

This candidate is not target MinGW64 validated yet.


## 2026-09-21 — viewer state authority, free-transit corridors, and main-engine visualization

User feedback after the minimum-cant Newtonian fix:
- visual behavior is now substantially better;
- UI appeared to disagree with actual control law after changing flight style
  (ASSISTED button still selected while motion looked Newtonian);
- exact 10.0 m/s tracking is undesirable for ordinary free transit: harmless
  ~10.1 m/s overshoot must not provoke a body rotation/braking manoeuvre;
- requested a visible indication of when the aft main engine is actually producing
  thrust.

Latest uploaded perf evidence remains healthy for the geometric/timing layer: the
steady wall cases at the tail of the log are one scalar Ruckig solve with
`min_speed_mps=10.0000` and `max_speed_mps=10.0000`. Therefore these changes belong
to execution/UI semantics, not the path generator.

### Redux-style viewer state

The native C++/GLFW viewer now uses one reducer-driven authoritative store:
- one `AppState`;
- explicit `ViewerActionType` actions;
- `reduceViewerState()`;
- all mode/pilot/style button clicks dispatch actions rather than directly mutating
  independent widget booleans;
- runtime control-law observations also dispatch into the same reducer.

This is intentionally Redux-style architecture, not the JavaScript Redux library.

Runtime truth is recorded in every execution frame:
- `hasRuntimeControlLaw`;
- `runtimeControlLaw`.

During playback the current frame is observed and, if a lower layer has actually changed
the law, the reducer updates `state.controlMode`. The ASSISTED/NEWTONIAN buttons are
therefore projections of the effective runtime state, not merely the last click.

Execution diagnostics now distinguish:
- `CONTROL LAW REQUESTED`;
- `CONTROL LAW EFFECTIVE`;
- `CONTROL LAW SWITCHES`.

The trace's textual law label is kept synchronized with reducer state, and effective-law
labels are localized in the viewer.

Relevant commits:
- `b503c3e35a9b8c2b0333b026b9251d6075a1afe2`;
- `1d17f9d208f9ef77a3dc8ac09753202aba7cd4c4`;
- `eedad40169c00509e01015f1a1777bd67f29965f`;
- `a60a215fde65505826b9b96c70edf511991da91f`.

### Free-transit speed/progress corridors

`AcceptedManeuverProgram::TrackingEnvelope` now has:
- `alongTrackSpeedDeadbandMps`;
- `alongTrackPositionDeadbandMeters`.

For `FreeTransit`, Follower decomposes position and velocity error into:
- cross-track components: still controlled normally;
- along-track components: ignored inside the configured corridor and corrected only
  by the excess outside it.

Current doctrine:
- STANDARD speed corridor: reference +/- 0.5 m/s;
- EXTREME speed corridor: reference +/- 1.0 m/s;
- STANDARD progress corridor: +/- 12 m;
- EXTREME progress corridor: +/- 16 m.

Thus a 10.1 m/s actual speed on a 10.0 m/s reference creates zero longitudinal speed
feedback in both styles. The vehicle is not required to chase exact schedule position
inside the progress corridor either.

This is deliberately limited to `FreeTransit`; precision docking/capture maneuvers keep
their stricter semantics.

The active speed corridor is persisted in trace frames and displayed in the viewer HUD
as e.g. `КОРИДОР V: 9.5 .. 10.5 М/С`.

Relevant commits:
- `92d65f86fe22ec1d0a404f952a0ebd0eb4935fb3`;
- `a171510889a2d235896e6ad567011c2b651ee3b7`;
- `023861170299dc7321975f2e73173af4b2547ca8`;
- `ad053356ada03d5212185b7d49d0b6aeb017ed6a`;
- `0b2e48b66744662e783b52b135ef26a714f43fb5`;
- `7a663da3b7f6bc8a92daa9433daeef40a10d41c9`;
- `3cadb29a840643524f2edafba3abb0b9091d795c`;
- `9e27bcd2392e7c18c36354374df33ab04882045f`;
- `3845b50910310494b394ec00820c86cc97b96aff`;
- `5299809e37f0a5061d56f066240619d56c6f27b4`.

### Aft-main-engine visualization

Each execution frame now records `mainEngineThrottle01` from the **actual physical**
`motion.mainEngineAccelerationMps2`, projected onto the current physical hull forward
axis and normalized by main-engine authority.

Viewer behavior:
- rear face of the diagnostic ship is filled orange only for positive aft-main thrust;
- intensity/alpha scale with actual main-engine throttle;
- HUD also shows `MAIN: N%`;
- RCS/manoeuvre-thruster acceleration does not light the rear face.

This directly answers whether a hull rotation was physically useful for main-engine
vectoring or was merely an attitude command with no aft-main thrust.

Relevant commits:
- `245bcc627fef5e0f79ad3e00e4b998c31e04b8db`;
- `2de51af96a3d9156fe7f3b5cb94a9f33d9f0e015`;
- `cbb58b16a7a15803cc8e56618d639916d748a53e`;
- `089d905f3207fa48afe3bba70935ce9413b145f6`;
- `163bee3c58443a5d0d6b4ad092ae7e6f970feef1`;
- `2e8692b68c670275e7f96654c4a990c473b885a9`.

### Validation state

These latest execution/UI changes are committed but are not yet user target-validated.
The next target viewer pass must explicitly compare STANDARD/EXTREME and
ASSISTED/NEWTONIAN while watching:
- effective button selection;
- `CONTROL LAW REQUESTED/EFFECTIVE/SWITCHES`;
- speed corridor text;
- `MAIN:%`;
- orange rear face.


## 2026-09-21 — body/engine telemetry + independent 5..50 m/s start/finish sliders

User feedback after reducer/corridor/main-engine visualization:
- current flight is visually much better, but a remaining unexplained hull somersault occurs
  near the low/straight part of the route while the main engine indicator is off;
- actual speed is about 10.1 m/s during the event;
- user asked whether logs contain body orientation changes and exact engine-on moments;
- user requested independent start and finish speed sliders, each 5..50 m/s.

### What the existing logs did and did not contain

`navigation_perf.log` is a trajectory-generator performance log. It contains Ruckig/path
timing facts only. The latest uploaded tail still shows the default wall reference as one
scalar solve with `min_speed_mps=10.0000` and `max_speed_mps=10.0000`; it cannot
explain a physical body flip.

Before this iteration, `last_execution_trace.json` already persisted:
- ship position;
- ship forward/right/up basis;
- ship velocity;
- effective runtime control law;
- actual positive aft-main throttle;
- accepted-program reference forward/right/up.

However, there was no compact human-readable time-series log correlating body attitude,
angular rate, main/RCS acceleration, and reference attitude.

### New execution telemetry

Each trace frame now additionally records:
- `shipAngularRatePyrRadPerSec`;
- `mainEngineAccelerationMps2`;
- `manoeuvreAccelerationMps2`;
- `engineAccelerationMps2`.

These fields are persisted in trace JSON.

A new file is emitted after every Stage-2 execution:
```text
tools/navigation_runtime/last_execution_telemetry.log
```

Every sampled line contains:
- time;
- phase;
- effective control law;
- position and speed;
- actual body forward/up;
- pitch/yaw/roll rates;
- main-engine throttle percent;
- main-engine acceleration vector;
- manoeuvre/RCS acceleration vector;
- combined engine acceleration;
- reference speed;
- reference forward/up;
- body-vs-velocity forward angle;
- actual-vs-reference forward angle;
- actual-vs-reference up angle.

It also marks exact sampled transitions:
- `MAIN_ON` / `MAIN_OFF`;
- `RCS_ON` / `RCS_OFF`.

This is the primary evidence for the remaining somersault. If main stays off while
`ref_forward/ref_up` themselves rotate, the problem is attitude authoring/program
sampling. If reference attitude is stable but actual body rotates, the problem is lower
in angular tracking/physics.

Relevant commits:
- `62c94992bfbde0983dbcb2d581c038f5939f890c`;
- `33854813be1d8299597a7aeb9e4bf40600403f16`;
- `72eeb3fde524136aff5edff1e33a683c9cf1e955`;
- `e8083bb2f27623e73753bb98a9b6137bfc5928c2`.

### Start/finish speed override seam

`ScenarioRunSettings` now has independent optional execution overrides:
- `startSpeedOverrideMps`;
- `finishSpeedOverrideMps`.

Negative values mean "use scenario.json authored value".

The runtime:
- preserves the authored start velocity direction and changes only its magnitude;
- applies the same effective start velocity both to trajectory generation and the actual
  simulated ship initial state;
- applies the effective finish speed to terminal Ruckig speed constraint/velocity,
  moving-terminal gate semantics, final-state validation, and diagnostics;
- Stage-1 static route geometry remains unchanged.

`ScenarioRunResult` exposes authored start/finish defaults so the viewer initializes
from scenario data, not a UI hard-code.

Relevant commits:
- `1932f80e1350a2631332b1092f7615bddc479dfe`;
- `0bebf41764177e6614efc10617ddfa034407f38b`.

### Viewer speed sliders

The reducer-owned viewer state now includes:
- `startSpeedMps`;
- `finishSpeedMps`.

Two independent sliders are visible under the main controls:
- `СТАРТ V`;
- `ФИНИШ V`;
- range 5.0 .. 50.0 m/s.

Dragging a slider dispatches `SetStartSpeed` / `SetFinishSpeed` through the same
Redux-style reducer. Changing speed after an execution restores the retained Stage-1
route and invalidates only Stage-2 execution, because speed is an execution setting and
must not force static route replanning.

Execution settings carry the current slider values into Stage-2.

Commit:
- `c199d0dbb042066075f1b320799d8e5dfa55b0a7`.

### Validation state

These telemetry and slider changes are committed but not target-MinGW64 validated yet.

Next evidence should include:
- successful runtime/viewer build;
- visible 5..50 start/finish sliders;
- one run that reproduces the unexplained somersault;
- the generated `last_execution_telemetry.log`.

Do not guess the remaining flip root cause from `navigation_perf.log`; use the new
execution telemetry.


## 2026-09-22 — control-chain audit, automatic Stage-2 refresh, speed-override validity, and somersault root cause

User raised two architectural/UX questions:
1. Who is actually controlling the ship during motion, versus merely animating a material point?
2. Do pilot/control-law/style/speed changes require another explicit Calculate press?

User also supplied `last_execution_telemetry.log`, which finally gives physical
attitude/propulsion evidence for the remaining hull somersault.

### Actual runtime control chain

The current Stage-2 execution is not a kinematic playback. The mutable ship state is a
real `ShipTransform` + `DynamicMotionState`, and the control chain is:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> TrajectoryFollower
 -> ManeuverTrackingController
 -> NavigationSystemControlIntent
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState navigation acceleration demands
 -> SharedShipPhysics / ShipController (angular)
 -> DynamicMotionSystem (main-engine + manoeuvre/RCS allocation and translation)
 -> ShipTransform / DynamicMotionState
```

So there is a concrete autopilot/controller pulling the virtual controls. Pilot skill
changes the delivered command through `PilotSkillExecutor`; body angular rates are
bounded by the actual ship parameters; translational demand is physically split into
main-engine and manoeuvre/RCS channels according to the active control law.

However, the user's concern is partly correct at the *authoring* level: the current
Stage-2 still starts from a kinematically-authored trajectory and then derives an
attitude/reference program around it. Full B5/B6 body/thrust-coupled maneuver authoring
is still not complete. The lower execution is physical; the accepted program is not yet
the final unified physical maneuver compiler/proof architecture.

### Telemetry proves the current somersault is an angular-control runaway

The uploaded telemetry shows:
- early flight: actual body basis equals reference basis, angular rate is zero, and
  both main/RCS translation channels are quiet;
- near the first turn, body and reference briefly become nearly coincident while the
  physical pitch rate is already about +0.75 rad/s;
- after that, the reference settles back toward the route tangent but pitch rate keeps
  increasing/saturating around +1.57 rad/s, so the physical body continues rotating
  *away* from the stable reference;
- during much of that runaway the main engine is OFF; manoeuvre acceleration is
  translational RCS, not evidence that the main engine needed the flip;
- a second runaway later reaches nearly 179 degrees body-vs-velocity/reference with the
  main engine still OFF, and main thrust starts only after the hull is already nearly
  inverted.

Therefore the main engine is not requesting the somersault. The angular command loop is.

### Root cause in the current accepted-program representation

`AcceptedManeuverProgram` is capped at 16 samples per phase. The runtime previously:
1. differentiated the dense reference attitude to angular velocity and angular
   acceleration;
2. downsampled those values into at most 16 program samples;
3. linearly interpolated `angularAccelerationFeedForwardMapRadPerSec2` between the
   sparse samples;
4. added that feed-forward directly to angular tracking feedback.

This can smear a short angular-acceleration pulse over a long interval. The result
matches the telemetry: the reference basis has already passed the desired orientation,
but positive angular acceleration/feed-forward can keep winding up the ship.

Additionally, the follower previously clamped only the angular *feedback reserve*, then
added angular feed-forward without a final clamp against the accepted physical angular
capability.

Candidate fixes:
- `018fd2c08874cb426a9d3fc8340c85ffb4539bad`
  - FreeTransit no longer publishes sparsely-sampled angular-acceleration feed-forward;
    body attitude is tracked using reference basis + reference angular velocity +
    closed-loop feedback.
  - explicit START/FINISH speed overrides expand the Stage-2 speed envelope so 5..50
    m/s slider values do not fail `validRequest()` merely because STANDARD nominal
    speed is 10 or EXTREME nominal speed is 18.
- `e8369c0fa21cc59f0429741540dd4903f7f556e8`
  - total follower angular acceleration demand is clamped to the accepted physical
    angular-acceleration capability after feed-forward + feedback composition.

These fixes are not target-validated yet.

### Automatic execution refresh after UI changes

The old UX did require a separate Stage-2 Execute after changing execution settings,
and the viewer could therefore continue showing the previous trace. That created the
correct impression that buttons/sliders did nothing until another explicit action.

New behavior:
- one initial `Calculate` obtains/retains Stage-1 static route and immediately queues
  Stage-2 execution;
- after a retained route exists, changing:
  - Assisted/Newtonian;
  - pilot skill;
  - Standard/Extreme;
  - sudden-obstacle toggle
  automatically queues a fresh Stage-2 execution;
- START/FINISH speed sliders queue one fresh Stage-2 execution on mouse release rather
  than once per dragged pixel;
- these execution-only changes do not re-run the static Planner.

Commit:
- `7a164a65292a47e41c19a01bdcb3f55b93d0110d`.

Therefore the intended UX is now:
```text
Calculate once for static route
 -> change any flight/execution setting
 -> Stage-2 recalculates automatically
 -> viewer plays the new trace
```

A new Stage-1 Calculate is only needed when static route facts/goal/world geometry change.

### Full control-chain telemetry

To answer "who pulled which lever" rather than infer it from hull motion, trace frames
now also store:
- Follower ideal linear acceleration command;
- Follower ideal angular acceleration command;
- PilotSkillExecutor executed linear acceleration command;
- PilotSkillExecutor executed angular acceleration command.

These are written to JSON and to `last_execution_telemetry.log` as:
- `ideal_lin_cmd`
- `ideal_ang_cmd`
- `exec_lin_cmd`
- `exec_ang_cmd`

Then the same line shows the downstream physical allocation:
- `main_a`
- `rcs_a`
- `engine_a`
- actual `pyr_rate`
- actual/reference body basis.

Commits:
- `a2ece66294ab0a8aa3c4258257dbcdbe0c72b086`;
- `4343b7d5289fca95e6989f3e7d21f691723361bc`;
- `9efe6934aa3f8141604e81e714319126a83afa6a`.

This makes the next target run able to distinguish:
```text
program/reference
 -> follower ideal command
 -> pilot-executed command
 -> physical allocator
 -> actual body/velocity
```

### Validation status

All changes in this section are committed but not yet target-MinGW64 validated.
Do not claim PASS until the user rebuilds/runs the viewer and returns the new telemetry.


## 2026-09-22 — FlightStyle = clearance doctrine; speed/style now invalidate Stage-1; Calculate has one explicit meaning

User clarified the intended semantics:

- FlightStyle must never own a nominal/cruise speed.
- STANDARD vs EXTREME answers only the risk/clearance question:
  - STANDARD: prefer more maneuver/safety room around static geometry;
  - EXTREME: accept a tighter pass when that is useful (for example to stay close to cover).
- START/FINISH speed changes are not execution-only. Higher inertia changes the
  geometric room needed for a credible maneuver, so the retained Stage-1 route may move.
- `РАССЧИТАТЬ` must have one unambiguous action:
  - enabled only when current inputs invalidate the displayed result;
  - one click performs every required calculation;
  - show a concise on-screen result log;
  - after the attempt, visibly disable the button until an invalidating input changes.

### Removed style-owned speeds

The runtime no longer contains or parses:
- `standardSpeedMps`;
- `extremeSpeedMps`;
- `standard_speed_mps`;
- `extreme_speed_mps`.

Those keys were also removed from `tools/navigation_runtime/scenario.json`.

The current test stand's trajectory speed envelope is derived only from the explicit
boundary requests:
```text
max(start speed, finish speed)
```

FlightStyle is no longer passed into FreeTransit program authoring and no longer changes
Follower speed/progress deadbands.

Relevant commits:
- `7e8c67bacfc6615c1d03faf8f771e6e5e65b66d3`;
- `75d0f50fc25c784d515f1e9f092c37600d4e6346`;
- `75c85b028b4c7b21a3ddce4c1dff3912b596c95f`.

### Speed-aware Stage-1 maneuver reserve

A first coarse speed-aware route reserve now exists in the diagnostic stand.

`routePlanningClearanceMeters()` uses:
- the larger requested START/FINISH speed;
- real ship angular acceleration/rate limits;
- a representative 30-degree turn time;
- a style-only clearance factor.

Current coarse formula:
```text
inertialLead = planningSpeed * characteristicTurnTime
additionalClearance =
    authoredClearance +
    inertialLead * styleReserveFactor
```

Current factors:
- STANDARD = 1.0;
- EXTREME = 0.35.

This is intentionally a coarse Stage-1 maneuver-space reserve, not the final B6
time-parameterized oriented swept-hull proof.

Practical consequence:
- increasing speed should usually keep the same obstacle-side topology but move support
  points farther away;
- if another passage becomes cheaper/feasible, topology/point count may also change;
- at the same speed EXTREME should route closer than STANDARD.

Stage-1 diagnostics now include:
- `ROUTE PLANNING SPEED`;
- `STYLE CLEARANCE`;
- `ROUTE ADDITIONAL CLEARANCE`.

A new E2E regression compares:
- STANDARD 10/10;
- STANDARD 40/40;
- EXTREME 40/40;
and requires the 40 m/s Standard detour to move farther from the wall and the 40 m/s
Extreme detour to cut closer.

Relevant commits:
- `7e8c67bacfc6615c1d03faf8f771e6e5e65b66d3`;
- `a8930d82b8379d26b73a1bd1f61b6f7dbdf66c5d`;
- `f3f4734d0ebc2d0a500d9607862319d26b3e6d44`;
- `70f099ccd77810da8c31356efd1951cbc0549e01`;
- `0c6700ae470b7758da014cf7fe78e719a3c61362`.

### Calculate UX/state contract

Viewer state now separates:
- `routeInputsDirty`;
- `executionInputsDirty`;
- `calculationInProgress`.

Invalidation:
- START speed -> route + execution dirty;
- FINISH speed -> route + execution dirty;
- STANDARD/EXTREME -> route + execution dirty;
- Assisted/Newtonian -> execution dirty only;
- Pilot skill -> execution dirty only;
- sudden-obstacle toggle -> execution dirty only.

No setting auto-runs a calculation anymore.

One `РАССЧИТАТЬ` click:
1. rebuilds Stage-1 only when route inputs are dirty (or no route exists);
2. executes Stage-2 from the resulting/retained route;
3. publishes a concise on-screen log with route point count/length, flight result and
   requested boundary speeds;
4. clears dirty state after the attempt.

The button is then rendered dark/dim and labeled `РАСЧЕТ ГОТОВ`. It cannot be clicked
again until an input marks the result dirty. Dirty state changes the short log to
`РЕЗУЛЬТАТ УСТАРЕЛ` and re-enables `РАССЧИТАТЬ`.

The obsolete second `UiAction::Execute`, `ЗАПУСТИТЬ ПОЛЁТ` path and SPACE-to-execute
behavior were removed. Playback controls now only control playback.

Relevant commits:
- `523d40866610f5b167ad30ffd6fbe598d91fa701`;
- `ba4b38c5a380bfbb238841fe5b4eee419730d653`;
- `8a013ea2f54ced80dc9495948196f3922a0aba8e`;
- `d254b18cf7bc7a70030c24041facf3fd73175124`;
- `e99d3be88d52283531d26cb9e631e03b018fa88f`.

Documentation was updated in `tools/navigation_runtime/README.md` to match this
one-button workflow and the new style semantics:
- `ef3c632d5e2dbbcdb51dd51dba3918de39a4e646`.

### Validation state

These changes are committed but NOT yet validated on the user's MinGW64 target.

Next target evidence must verify:
- architecture contract script passes;
- runtime E2E builds/runs;
- viewer builds;
- `РАССЧИТАТЬ` is enabled initially, visibly disabled after an attempt, and re-enabled
  only after a relevant setting changes;
- changing speed visibly changes white Stage-1 route coordinates/clearance;
- at equal speed EXTREME is closer to the obstacle than STANDARD;
- no style-owned nominal speed remains in behavior/diagnostics.


## 2026-09-22 — target build failure fixed; variable-speed invariant formalized

Target MinGW64 evidence from HEAD `c5a2ea68434a6743e034599e7c35134b0c3c1663` reached the viewer build and failed in
`NavigationRuntimeViewer.cpp` for two ordinary C++ integration mistakes:

1. `drawHud()` called `recalculationRequired(state)` before any declaration was visible.
2. The old speed-slider auto-refresh removal left a dangling
   `if (speedChanged)` with no statement before the closing brace.

These were not navigation-algorithm failures.

Corrections:
- forward declaration for `recalculationRequired(const AppState&)`;
- slider release now only clears `scrubbingFrames` and `speedSliderDrag`;
- unused viewer `orientationErrorDegrees()` helper removed;
- the unrelated unused `acceleration` warning in `buildReferenceAttitudes()` was also
  removed.

Relevant commits:
- `e2597a7714f5e7fee2200e7f47f9bfb9bafb2734`;
- `b15d0e97c6897d2b099e0c6e41d60830c45776b9`.

### Variable-speed contract

A new canonical rule is now recorded in
`src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`.

Speed is a state variable, not a FlightStyle constant and not a value that must remain
uniform across a route.

Rules:
- START/FINISH speeds constrain those boundary states only.
- Intermediate speed may vary freely inside physical, geometric, mission and safety
  limits.
- Any speed reduction must have a concrete reason local to the maneuver:
  curvature/turn authority, braking distance for an upcoming required state, collision
  or clearance proof, explicit local speed restriction, docking/formation precision,
  tracking recovery, or emergency avoidance.
- A difficult corner may require a major slowdown or a full stop.
- A clear segment may be flown substantially faster than the FINISH speed if later
  constraints can still be met.
- A local speed restriction must not silently cap unrelated route segments.
- Without a physical/geometric/mission/safety reason, the planner must not invent
  braking just to make the profile uniform.
- STANDARD/EXTREME remain clearance/risk doctrine only and never own a nominal speed.

Commit:
- `49009e6f480c7a70848f9a89c5cb4a0f7d4c8dab`.

### Concrete terminal-speed bug fixed

Two implementation points were corrected immediately:

1. `executionVehicleProfile.maxSpeedMps` is no longer
   `max(startSpeed, finishSpeed)`.
   START/FINISH are boundary states. The hard execution speed ceiling now comes from the
   vehicle capability (`ShipParams::maxCombatSpeed`), expanded only if a boundary state
   is already higher.

2. In the multi-point scalar path-progress backend, an exact moving terminal point-speed
   constraint is no longer folded into the route-wide maximum speed. Ruckig already
   receives terminal speed as `targetSpeedMps`; applying the same value globally was
   unjustified braking.

Relevant commits:
- `336b48a293dca0306847afbe3bc00e5787713a2e`;
- `16fe6a8918fa1a9f03fce3dd2f814987198f31e8`.

Focused regression added:
- three-point straight 500 m route;
- START = 10 m/s;
- FINISH = 10 m/s;
- vehicle max = 80 m/s;
- trajectory must rise above 12 m/s in the unrelated transit segment and still finish at
  exactly 10 m/s.

Commit:
- `22133bbe5c9ef61a52e839338e88b44a29061f22`.

Runtime README now states the same variable-speed rule:
- `30135fcd9db3a61b875ad0e4a4f5a4e98611e960`.

### Remaining known speed-profile defect

Do not claim the entire variable-speed contract is implemented yet.

The current multi-point scalar path backend still computes a single
`globalGuideSpeedLimit()` from the worst curvature found anywhere on the execution
guide. Therefore one difficult bend can still unnecessarily cap otherwise clear route
segments.

That behavior is safe but over-conservative and violates the new locality rule.

Required next trajectory-authoring correction after the target build is green:
- convert curvature / point / range speed restrictions into local speed-profile
  constraints along scalar progress;
- permit independent acceleration on clear segments;
- brake only early enough to satisfy the next local restriction;
- permit a local stop when genuinely required;
- accelerate again after the restriction when safe;
- never turn a local restriction into a whole-route cap.

This must stay inside the route-to-trajectory/B5-B6 authoring path. Do not hide it in
Follower behavior or FlightStyle.


## 2026-09-22 — Assisted selector bug fixed by separating requested vs effective runtime state

Latest user feedback:
- overall flight now looks materially more realistic;
- there is still some body oscillation while returning to the nominal attitude;
- ASSISTED appears not to switch at all;
- user requested an always-visible right-panel summary of current pilot, control type and
  behavior/style.

The uploaded execution telemetry begins with `law=NEWTONIAN` and the displayed run is
indeed a Newtonian execution. The important UI bug was found in the viewer state model,
not in the Assisted physics implementation.

### Root cause

The viewer previously used one field, `state.controlMode`, for two different products:

1. the **requested** user setting selected by the ASSISTED/NEWTONIAN buttons;
2. the **effective** control law observed from the currently displayed runtime frame.

Every render called:
```text
syncViewerStateFromRuntimeFrame(displayFrame)
 -> RuntimeControlLawObserved
 -> state.controlMode = frame.runtimeControlLaw
```

Therefore this sequence was broken:
```text
old trace = NEWTONIAN
user clicks ASSISTED
 -> requested state becomes ASSISTED
next render still displays old NEWTONIAN frame
 -> runtime observation writes NEWTONIAN back into state.controlMode
 -> ASSISTED button appears not to work
```

This also meant stale playback could modify the input that would be sent to the next
Calculate.

### Corrected state ownership

Viewer now has separate products:

```text
state.controlMode
    = requested control law for the next Calculate

state.effectiveRuntimeControlLaw
    = observed law of the currently displayed execution frame
```

`RuntimeControlLawObserved` is no longer allowed to assign `state.controlMode`.

The top buttons always show the requested input. The old trace may remain visible while
the result is dirty, but it cannot undo the newly selected input.

Commit:
- `3067ccfde2f260dcde30baa038ca82e400f233a8`.

### Right-panel mode diagnostics

The right diagnostics panel now contains a dedicated `ТЕКУЩИЕ РЕЖИМЫ` block:

- `ПИЛОТ`;
- `УПРАВЛЕНИЕ / ВЫБРАНО`;
- `УПРАВЛЕНИЕ / ФАКТ`;
- `ПОВЕДЕНИЕ`.

If settings changed and the displayed execution is still the previous trace, effective
control law is shown with:
```text
(СТАРЫЙ РАСЧЕТ)
```

This makes the intended interaction explicit:
- click ASSISTED -> selected line/button becomes Assisted immediately;
- before Calculate, effective line may still say Newtonian / old calculation;
- press Calculate -> fresh execution should show effective Assisted if the runtime really
  uses the requested law.

Pilot and Standard/Extreme behavior are also visible in the same block.

Architecture regression now forbids `RuntimeControlLawObserved` from writing
`state.controlMode`, and checks that the right-panel mode diagnostics remain present.

Commits:
- `d57cd3b285b0419f9bf8266887fb34811d57a592`;
- `12d91b3253f68907557308b9e27ee4fe6d871822`.

### Remaining body oscillation

The user still observes some oscillation while the hull returns toward the requested
attitude. Do not conflate this with the selector bug.

The control-chain telemetry already exposes:
- ideal angular command;
- pilot-executed angular command;
- actual P/Y/R rate;
- reference forward/up;
- actual forward/up.

After confirming ASSISTED really executes, use those channels to determine whether the
remaining oscillation is:
- reference motion;
- insufficient angular damping in ManeuverTrackingController;
- PilotSkillExecutor lag/overshoot;
- or ShipController/angular-rate dynamics.

Do not tune damping until the requested/effective control-law state is visibly proven on
the target viewer.


## 2026-09-22 — high-speed runaway root causes: underdamped hull loop + reference clock outrunning physics

Latest target evidence came from a 26.15 m/s -> 11.75 m/s Newtonian run.

Observed diagnostics:
- Ruckig trajectory itself was produced successfully;
- 3 accepted FreeTransit phases / 2 nominal handoffs;
- calculated speed range 11.38..26.15 m/s;
- old runtime claimed `ROUTE EXECUTION COMPLETE: YES`;
- but physical final position error was 68.49 m;
- max route deviation 62.24 m;
- max Follower position error 68.49 m;
- main engine could be zero while angular control and RCS remained active.

That proves two separate faults.

### 1. Hull "float" / oscillation

B10 default attitude feedback was:
```text
Kp = 2.0
Kd = 1.0
```

For the second-order attitude-error loop, critical damping for Kp=2 is about:
```text
Kd_critical = 2 * sqrt(Kp) ~= 2.83
```

So the old loop was intentionally/accidentally strongly underdamped. Telemetry matches
that: the body still carries substantial angular rate as forward-reference error approaches
zero, overshoots the requested attitude, then reverses correction like a pendulum.

Changed default B10 angular-velocity feedback to:
```text
Kp = 2.0
Kd = 3.0
```

This is near-critical/slightly overdamped and remains bounded by the accepted program's
angular feedback reserve and lower physical angular acceleration/rate limits.

Focused regression now requires default Kd >= 2*sqrt(Kp).

Commits:
- `e2b5270210b79c5b873a0437b2a4deb14c7973ae`;
- `32af480c8fee271a1f5f3f6396b02b6601a0e190`.

Important actuator semantics remain:
- hull rotation does NOT imply main-engine thrust;
- navigation angular acceleration is applied by the attitude/RCS torque path;
- main engine owns linear thrust;
- therefore `MAIN=0` while `exec_ang_cmd != 0` is physically legitimate.
The bug was unnecessary/underdamped rotation, not the independence of those actuators.

### 2. No physical return to the trajectory at high speed

The old FreeTransit execution was too schedule-driven.

`ManeuverPhaseGate::ScheduledMoving` advances when nominal phase time ends. At high speed
the reference could continue into the next phase while the physical craft had already
fallen far outside the intended track. The program clock therefore ran away from the ship.

This explains the previously contradictory diagnostic:
```text
ROUTE EXECUTION COMPLETE: YES
FINAL POSITION ERROR: 68.49 M
```

A reference-reacquisition policy is now implemented in the runtime fixture:

- `AcceptedManeuverProgram` remains immutable;
- runtime owns a separate `activeProgramReferenceDelaySeconds`;
- B9/Follower are sampled using:
  ```text
  programReferenceTime = physicalTime - referenceDelay
  ```
- when B10 reports `trackingErrorExceeded`, reference delay grows by one fixed step;
- therefore reference progress pauses while the physical ship catches/reorients/reacquires;
- phase gate sees the same delayed reference time, so it cannot hand off merely because
  wall-clock time elapsed;
- once the craft is back inside its envelope, reference time resumes;
- phase activation resets the local delay;
- accepted program timestamps are NOT mutated.

Viewer status while this happens:
```text
FOLLOWER ВОЗВРАЩАЕТСЯ В КОРИДОР
```

Diagnostics:
- `REFERENCE CLOCK HOLD FRAMES`;
- `REFERENCE CLOCK HOLD`.

The diagnostic harness overrun window was enlarged from +12 s to +30 s so a real recovery
attempt has time to occur instead of immediately ending the fixture.

Commits:
- initial implementation `4717263f59e9b50f90ad7696e927eb1f3f4696ba`;
- immutable accepted-program correction
  `c0378aa900518523a8f9ac9c51dd32563c7ed773`;
- viewer reacquisition status
  `4d2255366d4ff6ad330d82586b24a29214236b41`.

### 3. FreeTransit envelope now matches its own longitudinal deadbands

Previously FreeTransit could intentionally suppress harmless along-track lead/lag feedback
but still mark the same raw error as `EnvelopeExceeded`.

B10 now evaluates the tracking envelope using the same effective position/velocity errors
after longitudinal deadbands are applied. Real cross-track/velocity errors remain active;
accepted longitudinal slack no longer falsely pauses the reference.

A regression deliberately makes the raw along-track error exceed the nominal envelope
while remaining inside the accepted deadband and requires `Tracking`, not
`EnvelopeExceeded`.

Commits:
- `aca1d7af46a0cf5bdccc7e08629450bb217ced2b`;
- `cf963e972690e0f822146bdeb0ce96dbb8df166b`.

The runtime FreeTransit reacquisition envelope is now tighter:
- position 8 m;
- linear velocity 4 m/s;
- forward-angle 0.35 rad;
- angular-rate 0.8 rad/s.

Leaving this envelope does not disable navigation; it holds reference progress and
continues physical recovery.

Commit:
- `59aa0d404d02604e37f93955e083f18d0504b3cf`.

### 4. High-speed regression

Runtime E2E now reproduces the reported case:
- Newtonian;
- Expert;
- Standard;
- START 26.15 m/s;
- FINISH 11.75 m/s.

It requires:
- reference-reacquisition diagnostics are present;
- execution succeeds physically;
- final position error <= 5 m.

Commit:
- `41ea618fc089003410b6cba8f5b2c1db912c8cbc`.

This target regression is committed but NOT yet verified on the user's MinGW64 machine.

### 5. Completion diagnostics no longer conflate schedule and physics

Removed misleading:
```text
ROUTE EXECUTION COMPLETE: YES
```

It is now split into:
```text
PROGRAM PHASES COMPLETE: YES/NO
PHYSICAL TERMINAL STATE: REACHED/MISSED
```

Commit:
- `f6a8e68df19c9951ade574b38d7e4e08101aec2c`.

### 6. All current-run artifacts moved to repo root

Runtime logs now use process working directory. With supported commands launched from
repo root:
- `last_route_plan.log`;
- `last_execution.log`;
- `last_execution_telemetry.log`;
- `navigation_perf.log`.

Viewer traces also moved to root:
- `last_calculated_trace.json`;
- `last_execution_trace.json`.

Commits:
- `5aefdd85ec15b95e17f37834ab05e3458bf7a7ac`;
- `830069744134177f23ff829a8879032bdb0f4d4a`;
- docs `83fb94fc413290bcabf022cd53cd48ae840b7d86`,
  `a928e7e018567a8b1b27962aa7189f112c9ae575`.

### Validation status

Architecture guard updated to require:
- immutable-program external reference delay;
- reacquisition markers;
- root-log path ownership;
- Kd=3 attitude damping;
- effective FreeTransit envelope errors;
- high-speed E2E regression.

Commit:
- `baecf937643083e97d8af4a9e82ee10742a82f7d`.

Do not claim the new high-speed recovery or reduced oscillation is PASS until target
MinGW64 evidence is returned.

## 2026-09-22 — Assisted 10 -> 10 regression: frozen-reference derivative runaway

Fresh target evidence from the viewer run:

```text
control law                    ASSISTED
pilot                          EXPERT
flight style                   STANDARD
requested speed                10.00 -> 10.00 m/s
Ruckig calculated max speed    11.71 m/s
program phases complete        NO
physical terminal state        MISSED
final speed                    100.64 m/s
final position error           2411.14 m
max route deviation            2411.14 m
reference clock hold           47.70 s
max body/velocity angle        179.99 deg
coarse static contact          YES
```

This is **not** a Stage-1/Ruckig speed-profile failure. The retained route and
trajectory still request about 10-12 m/s. The failure is in physical
reacquisition after the new reference-clock hold activates.

Root cause:
- reference time was frozen on a **moving** FreeTransit sample;
- B10 kept replaying that frozen sample's non-zero linear feed-forward;
- the same frozen sample also retained non-zero reference angular velocity;
- bounded recovery feedback could not cancel the permanently replayed
  derivatives;
- the hull therefore accelerated away while trying to chase a fixed pose plus
  moving-sample derivatives, producing the observed speed runaway and tumbling.

Corrective candidate:
- outside the B10 tracking envelope, the frozen sample is treated as a
  geometric reacquisition target;
- linear/angular feed-forward derivatives are neutralized while outside;
- angular recovery damps actual hull angular rate toward zero instead of chasing
  the frozen sample's angular velocity;
- once the physical craft re-enters the envelope, the accepted moving reference
  and its derivatives become authoritative again;
- tracking authority remains bounded by the existing follower reserve.

Viewer correction:
- `fitCamera()` no longer includes every historical physical ship position;
- a runaway execution cannot make the authored route/obstacles microscopic;
- the execution trace remains rendered and can still be inspected with
  pan/zoom.

Regression added:
`testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives()`.

Code candidate before documentation commits:

```text
59ff756996229bf15a122eb0fe43cf0d9a14b245
```

Status: **TARGET MINGW64 VALIDATION REQUIRED**. Do not call this accepted until
the target machine passes both the Assisted 10 -> 10 viewer case and the
existing Newtonian high-speed reacquisition regression.

### 2026-09-22 follow-up — sparse attitude derivative alias also fixed

The frozen-reference failure exposed a second, earlier source of the tumbling:
FreeTransit programs keep at most 16 sparse samples, but were copying
`angularVelocity` from individual dense attitude samples. B9 then interpolated
those isolated dense rates across the much longer sparse interval.

In the failing trace the visible reference heading moved only by a few degrees
per second around the hold point while the controller was effectively chasing
a much larger sampled angular-rate target. That derivative was not consistent
with the sparse basis being rendered/executed.

Candidate now:
- never copies dense-source angular velocity directly into sparse FreeTransit
  samples;
- re-derives interior angular velocity from neighboring **sparse accepted
  bases and sparse sample times**;
- forces first/last sparse angular velocity to zero;
- clamps re-derived angular velocity to physical angular-rate capability;
- keeps angular acceleration feed-forward zero for sparse FreeTransit;
- retains the out-of-envelope derivative-neutralization recovery added in the
  previous correction.

New E2E regression:
`testAssistedLowSpeedDoesNotRunAwayDuringReferenceHold()` pins the exact
ASSISTED / EXPERT / STANDARD 10 -> 10 m/s case, requires physical success and
rejects physical speed above 25 m/s.

Updated code candidate before docs: `13ef6bd731ef6d8e78c75c72bf7a59f524b30bcb`.
Target MinGW64 evidence is still required.

## 2026-09-22 — higher-speed Assisted failure: propulsion/body coupling bug

Fresh target run:

```text
ASSISTED / EXPERT / STANDARD
START 20.90 m/s
FINISH 20.00 m/s
Ruckig max 20.90 m/s
program phases complete NO
physical terminal state MISSED
final error 175.20 m
final speed 7.45 m/s
max body/velocity angle 180 deg
reference hold 40.70 s
```

The exponential runaway is gone. The new failure is different: the physical
allocator and the reference attitude were inconsistent.

Observed telemetry showed the hull almost perfectly aligned with the accepted
reference while speed fell hard with `main_pct=0`. At ~10 s:
```text
body/ref error ~0.03 deg
main_pct 0
main_a = (-1.42, +0.39, 0)
speed 13.76 while ref speed 20.35
```
The old Assisted allocator allowed negative longitudinal **main** acceleration:
a virtual fore/nose main engine. Therefore the ship could brake through zero
and reverse its velocity while the hull did not turn. Around 19.2 s the body
was still fixed on the same reference attitude while body/velocity passed
through 90 deg; later it reached ~180 deg and the real aft-positive main command
began braking the now-backwards motion.

That behavior violates the documented physical contract.

Candidate correction:
- navigation main engine is aft-only in BOTH Assisted and Newtonian;
- reverse demand before hull rotation can use only bounded RCS;
- Assisted reference attitude is now propulsion-aware just like Newtonian:
  if RCS cannot realize the requested acceleration vector, the hull reference
  cants/flips toward the acceleration so aft main can contribute;
- control-law difference remains controller doctrine/slip behavior, not hidden
  propulsion;
- exact 20.90 -> 20.00 Assisted E2E regression added and rejects reverse main
  acceleration relative to hull forward.

Candidate code baseline before docs: `b833eddb7bd04b5c025b2be0fd8334c33f8824e6`.

Status: target MinGW64 validation required.

## 2026-09-22 — Newtonian still behaved Assisted: RCS was acting as primary propulsion

Fresh target run:

```text
NEWTONIAN / EXPERT / STANDARD
START 21.20 m/s
FINISH 21.20 m/s
route additional clearance 17.71 m
Ruckig min/max 20.73 / 21.20 m/s
program phases complete NO
physical terminal state MISSED
final error 181.42 m
final speed 8.17 m/s
max body/velocity angle 174.31 deg
reference hold 40.61 s
```

Stage-1 responded logically to the higher requested speed by widening the
detour. The execution failure was downstream.

Telemetry showed the actual problem clearly:
- Newtonian hull/reference became essentially fixed;
- main engine stayed OFF;
- manoeuvre/RCS supplied about 1.5 m/s^2 continuously;
- that RCS-only acceleration reduced physical speed from ~21 m/s to single
  digits while the reference still expected ~20.6 m/s;
- only very late, with body/velocity already ~174 deg apart, did aft main thrust
  begin to appear.

Why this happened:
`propulsionReferenceForward()` considered the Cobra's full physical
`manoeuvreThrusterAccel = 2.0 m/s^2` as ordinary route authority. The Ruckig
trajectory typically asks for ~1.5 m/s^2, so Newtonian concluded that RCS alone
could realize the trajectory and did not author a main-engine pointing
maneuver. Physically possible for a short RCS burst, but wrong doctrine for this
main-engine-dominant ship and effectively Assisted-like behavior.

Candidate correction:
- Assisted and Newtonian no longer share the same RCS attitude-authoring
  doctrine;
- Assisted may use the full real RCS envelope to preserve its coupled-flight
  behavior;
- Newtonian uses only the existing 0.35 m/s^2 precision slice when deciding
  whether the hull may stay on the travel tangent;
- material Newtonian acceleration therefore authors a real cant/flip so aft
  main propulsion participates;
- full RCS physical authority remains available downstream for recovery/trim.

Viewer diagnostic added at the bottom:
- fixed indicator `МАРШЕВЫЙ`: lights for positive aft-main acceleration;
- fixed indicator `ПЕРЕДНИЙ МАРШЕВЫЙ`: lights if any negative/fore main
  acceleration appears. The old aft-only baseline expected it dark; with the
  2026-09-24 physical fore bank it is now legitimate when that bank is used;
- fixed indicator `МАНЕВРОВЫЙ`: lights whenever manoeuvre/RCS acceleration is
  physically non-zero.

New regression:
`testNewtonianHigherSpeedUsesMainEngineDominantManeuver()` pins the exact
21.20 -> 21.20 Newtonian case and requires material main-engine participation
within the first 8 seconds.

Code candidate before documentation commits: `421ecb1b2721d54bc0c33737a520100a3c10fac9`.
Target MinGW64 validation pending.

## 2026-09-22 — planner/Ruckig architecture audit after Newtonian overshoot

No new physics patch in this iteration. The current architecture was inspected
before changing more executor behavior.

### What Stage 1 actually does

The static planner returns a coarse collision-free polyline. Its speed-aware
clearance reserve uses:
- requested start/finish speed;
- Cobra angular acceleration;
- Cobra max pitch/yaw/roll rate;
- STANDARD/EXTREME clearance doctrine.

It does **not** directly generate the final flyable 30 m/s arc.

### What Stage 2 geometry does

`TrajectoryGenerator::buildExecutionGuide()` rounds coarse corners locally
with sampled quadratic Bezier geometry. Radius/cut is estimated from:
`radius ~= v^2 / maxLateralAcceleration`.

For the current Cobra runtime profile, lateral acceleration is
`manoeuvreThrusterAccel = 2.0 m/s^2`.

This means the system can draw curved guide geometry, but the curve is still
constructed from a simplified lateral-acceleration envelope rather than from a
fully compiled hull-attitude + main/RCS maneuver.

### What Ruckig actually does on a multi-point route

For routes with >2 points Ruckig is **not** called once per user-visible route
point with full 3-D position/velocity/acceleration states.

Instead:
1. coarse route -> rounded execution guide p(s);
2. one scalar Ruckig solve computes path progress s(t);
3. trajectory maps scalar speed/acceleration back onto guide tangent/curvature.

So Ruckig currently decides only *when/how fast to move along an already fixed
curve*. It does not choose the arc and does not understand hull rotation,
main-engine pointing, RCS/main allocation, or lead-rotation time.

The scalar request uses one symmetric acceleration limit:
`min(maxForwardAcceleration, maxBrakingAcceleration)`.

### Cobra parameters: yes, but with two important problems

The runtime test harness uses a local hard-coded `cobraParams()` copy instead
of reading the authoritative `EliteCobraMk1Descriptor()`.

The copied values currently match the descriptor for the inspected fields:
- maxPitchRate 2.5 rad/s
- maxYawRate 2.5 rad/s
- maxRollRate 3.0 rad/s
- angularAccel 3.0 rad/s^2
- manoeuvreThrusterAccel 2.0 m/s^2
- maxLinearGs 7.5
- turnRadius 20 m
- maxCombatSpeed 500 m/s

However, the execution vehicle profile turns `maxLinearGs=7.5` into
~73.55 m/s^2 and exposes that as both forward and braking acceleration to the
trajectory layer. This is a load/linear envelope, not a propulsion allocation
model. Ruckig therefore sees a much stronger longitudinal capability than the
actual maneuver can necessarily realize before hull rotation.

The descriptor also contains `turnRadius=20 m`, but the current multi-point
execution-guide construction does not use that field as the authoritative turn
geometry.

### Architectural conclusion

The user's mental model is closer to the required end state than the current
implementation:
- planner/trajectory authoring should produce a physically flyable sequence of
  states/primitives with position, tangent velocity, acceleration requirement,
  and hull attitude/rotation schedule;
- a corner should become a real arc/clothoid-like maneuver sized by speed and
  available propulsion/rotation authority, not merely a polyline plus a global
  clearance padding;
- braking boundary must include hull lead-rotation time before aft-main braking;
- main and RCS authority must be compiled together before timing is accepted;
- Ruckig should time/bridge already physically feasible states or primitives,
  not be asked to compensate for missing propulsion geometry.

Next task is design/implement this contract rather than further follower tuning.

Repository baseline at start of this audit: `adfe567eefac994344d81c60b1f21e24f3d09077`.

## 2026-09-22 — current Ruckig reference / phase-target diagnostics added

Two world-space crosses are now rendered during execution:

- pink: the instantaneous `programReferencePosition` sampled from the active
  accepted Ruckig-derived program; this is the point B9/B10 is asking the
  follower to track NOW;
- violet: the endpoint of the currently active `AcceptedManeuverProgram`
  phase.

This distinction matters because the current multi-point backend does **not**
give Ruckig a "target point for the current route leg". It performs one scalar
path-progress solve to the end of the whole execution guide, then the resulting
trajectory is split afterward into accepted phases.

The new markers therefore expose both:
1. the actual moving reference handed to the follower;
2. the current phase endpoint created from the precomputed Ruckig trajectory.

This should make the next target run answer "what point is the executor chasing
when it begins to miss the turn?" without pretending that scalar Ruckig has a
per-leg target it does not actually have.

Architecture correction recorded in this iteration:
- turn geometry must NOT assume manoeuvre thrusters are the only source of
  lateral/normal acceleration;
- main + RCS + hull attitude form the reachable acceleration set;
- physical maneuver compiler/planner owns this decision;
- Ruckig is subordinate solver;
- follower/autopilot executes the accepted physical maneuver.

Current scenario terminal state:
- finish JSON contains both `forward` and `up`, so both are required;
- runtime accepts final position error <= 5 m;
- final speed error <= 1.5 m/s;
- final forward and up errors <= 0.25 rad (~14.3 deg);
- non-zero finish speed is converted into an exact terminal velocity direction
  along `finish.forward`.

Therefore current test requires an oriented moving fly-through, not merely
crossing the finish position. Braking is not intrinsically required when
start/finish speeds are equal and a sufficiently broad curve can achieve the
terminal velocity/orientation. Braking becomes necessary when geometry,
speed-limit/curvature, narrow passages or terminal speed demand it.

Code baseline before docs: `620b59ebbb6937727c5c3e3a47f78884ae3fd99f`.
Target validation pending.

## 2026-09-22 — canonical Planner/Autopilot contract fixed

Terminology is now explicit:

- **Planner** outputs the complete physical maneuver program.
- **Ruckig** is a helper used inside Planner calculations.
- **Autopilot/Follower** executes that program against real ship physics and
  watches for sudden hazards.

A Planner result is no longer conceptually "a set of route points".

It is a sequence of physical states plus the command interval to the next state.

State:
- time;
- position;
- velocity vector;
- acceleration vector;
- 3-D body attitude;
- angular velocity;
- angular acceleration.

Interval:
- duration;
- aft-main enable/throttle and throttle ramp;
- fore-main enable/throttle only when that hardware exists;
- manoeuvre/RCS force/command;
- attitude-control target/profile.

Important semantic correction:
"engine works for N seconds" belongs to the segment between two states, not to
the point itself.

The Autopilot does not choose a different nominal maneuver when tracking gets
hard. It may apply bounded correction and emergency safety action, then ask
Planner for a replacement program from the current state.

This is the architecture to implement next.

## 2026-09-22 — explicit Planner actuator intervals introduced

First implementation slice of the canonical Planner -> ManeuverProgram ->
Autopilot contract is now in code.

`AcceptedManeuverProgram` now contains two different things explicitly:

```text
ReferenceSample
    instantaneous physical target state

ActuatorSegment[i -> i+1]
    duration
    rear-main enable + throttle start/end
    fore-main enable + throttle start/end
    manoeuvre/RCS acceleration start/end
    propulsion-feasible witness
```

At that historical baseline Cobra planner compilation did not invent
fore-main hardware. The current descriptor now supplies a real fore bank; the
same invariant remains: Planner may use only descriptor/runtime hardware.

The existing Stage-12 trajectory/reference is now converted into these physical
actuator intervals in `makeProgramPhase()`:
- requested acceleration is decomposed against the planned body forward axis;
- positive body-forward component becomes rear-main throttle;
- remaining vector becomes manoeuvre/RCS command;
- RCS is clamped to real `manoeuvreThrusterAccel`;
- intervals that require more physical authority are marked infeasible.

This migration is deliberately observable before switching the live physics
path:
- B9 `ManeuverProgramSampler` samples the actuator interval directly;
- `TrajectoryFollower::Result` carries the sampled planner actuator command;
- trace records planned actuator segment/main/front/RCS/feasibility separately
  from actual engine telemetry;
- viewer shows a `ПЛАН SEG ... MAIN ... FRONT ... RCS ... FEASIBLE/SATURATED`
  line above the real engine lamps;
- telemetry log writes planned and actual propulsion on the same frame.

The actual runtime physics still uses the old net-acceleration execution path in
this slice. Diagnostics explicitly report:
`AUTOPILOT ACTUATOR EXECUTION: OBSERVE-ONLY MIGRATION`.

This is intentional: next target run first verifies whether Planner's new
physical program itself is sane before making Autopilot obey it literally.

New sampler regression pins that actuator intervals are interpolated directly
and no fore engine is synthesized when absent from the vehicle profile.

Code baseline before documentation commits: `7b60f875193334e20bc5d168d65df351b746754d`.
Target MinGW64 validation pending.

## 2026-09-22 — root cause of "Newtonian uses only manoeuvre thrusters" isolated

Fresh target run proved two separate facts.

### 1. Planner actuator schedule was not actually driving the ship

The run explicitly reported:

```text
TRAJECTORY SAMPLES: 693
PROGRAM PHASES: 3
PLANNED ACTUATOR SEGMENTS: 45
AUTOPILOT ACTUATOR EXECUTION: OBSERVE-ONLY MIGRATION
```

So the new Planner engine schedule was diagnostic-only. Actual engines were
still selected downstream from Follower/Pilot's net acceleration vector.

Current live engine selection rule is still:
- project executed world acceleration onto actual hull forward;
- positive forward component -> aft main;
- residual vector -> manoeuvre/RCS;
- no positive component -> main stays off.

Therefore a lateral/opposed tracking correction naturally lights only RCS even
if the intended nominal maneuver should have used main thrust.

### 2. The oscillation fix exposed a pre-existing sparse-program alias

The important regression is not the Kd change itself.

`e2b5270 Critically damp navigation attitude tracking` only changed angular
velocity damping from 1.0 to 3.0.

`56aca8a Derive sparse attitude rate from accepted basis samples` correctly
stopped copying dense angular derivatives into a <=16-key accepted program.

However, the entire long route leg was still compressed into <=16 uniformly
selected states. In the fresh run, 693 dense Ruckig samples became only 45
actuator intervals across 3 phases.

That means:
- P/V/A and body attitude were sampled too sparsely;
- B9 linearly interpolated between distant states;
- the accepted interval could have zero/incorrect feed-forward acceleration
  while its interpolated position/velocity reference still curved;
- Follower then created a large correction acceleration;
- because that correction was mostly lateral/opposed to current hull forward,
  downstream allocation used RCS almost exclusively.

Telemetry confirms exactly this signature at the start:
Planner interval reports MAIN=0 / RCS=0 while Follower/physics immediately
commands and applies RCS acceleration.

### Code correction

Commit `9d4b599c7339fc7dc30803a0aa3c57b7b543fd5a` removed long-leg sparse
compression. Route legs are now divided into consecutive dense chunks of at
most 16 source samples. Adjacent chunks share their boundary sample.

This preserves the actual dense Ruckig-derived P/V/A/attitude sequence instead
of inventing a different trajectory through sparse interpolation.

Additional invariant now reported:

```text
PLANNED ACTUATOR SOURCE COVERAGE: actual/expected COMPLETE
```

For a complete accepted route, planned actuator intervals must cover every
adjacent dense trajectory sample exactly once.

Code baseline before documentation commits: `b51b0abc22270104f75f198935d857582c9b4445`.
Target MinGW64 validation pending.

## 2026-09-22 — full architecture audit: current regression is a frozen-program deadlock

Fresh dense-source target runs are both failures. Source coverage is now
complete, but there are zero phase handoffs.

The key new diagnosis is independent of speed:

- accepted attitude can jump from zero angular rate to ~3 rad/s in one 8.33 ms
  sample because `buildReferenceAttitudes()` constrains angular speed but not
  angular acceleration;
- the 0.8 rad/s tracking envelope is therefore exceeded almost immediately;
- runtime freezes `programReferenceTime` by accumulating
  `activeProgramReferenceDelaySeconds`;
- the same frozen time is passed to `ManeuverPhaseGate`;
- nominal page end is never reached, so activeProgram remains 0 forever;
- outside-envelope B10 removes trajectory feed-forward and performs bounded
  recovery toward that frozen early reference;
- the craft therefore brakes, reaches zero speed and then returns toward an old
  point.

Dense chunking exposed this because the 16-sample objects are being treated as
semantic maneuver phases. They are actually storage pages of one continuous
program and must share one global timebase.

Architecture audit also confirms there is no single vehicle-dynamics source of
truth. Descriptor, runtime `cobraParams()`, NavigationVehicleProfile,
CapabilitySnapshot and manual Assisted propulsion currently disagree.

No further follower-gain tuning is justified before correcting:
1. continuous program/page semantics;
2. reference invalidation/replan behavior;
3. angular-acceleration-reachable attitude authoring;
4. vehicle dynamics SSOT.

The Stage-1 script is not an E2E execution gate. The actual
`navigation_runtime_pipeline` CTest must be run explicitly.

Repository baseline entering this audit: `5117857c8f31992c97f393e7f2ed16a630e8efc3`.

## 2026-09-22 — input/API normalization pass implemented

The architecture-cleanup pass requested after the emergency-state audit is now
implemented in code.

### Generic vehicle input

Navigation runtime no longer owns Cobra constants.

New canonical API type:

```text
game::navigation::VehicleDynamicsProfile
    ShipParams physics
    bodyHalfExtentsMeters
    capabilityRevision
```

The application/test fixture chooses a concrete vehicle descriptor and converts
it once at the outer boundary. Runtime functions receive the generic profile
explicitly:

```text
loadScenarioPreview(..., vehicle)
calculateScenario(..., settings, vehicle)
executeCalculatedRoute(..., settings, retainedRoute, vehicle)
```

No `cobraParams()` remains in NavigationScenarioRuntime.

Vehicle dimensions are no longer authored in scenario JSON. Collision-envelope
projection is centralized by
`conservativeCollisionRadiusMeters(VehicleDynamicsProfile)`.

### Derived dynamics have one projection layer

New common helpers:

```text
ShipDynamics.h
    forwardMainAccelerationLimitMps2
    reverseMainAccelerationLimitMps2
    manoeuvreAccelerationLimitMps2
    controlledSpeedLimitMps
    maximumAngularSpeedRadPerSec
    angularAccelerationLimitRadPerSec2
```

`NavigationVehicleProfileAdapters.h` is now the single ShipParams ->
NavigationVehicleProfile projection.

`ManeuverCapabilityAdapters.h` is now the single ShipParams ->
AcceptedManeuverProgram::CapabilitySnapshot projection.

Capability revision now comes from the vehicle capability revision, not from the
static-world revision.

### Explicit calculation policy

All stand parameters that materially affect calculations are now carried through
`ScenarioRunSettings::navigation` as `ScenarioNavigationPolicy`, including:
- integration dt;
- trace sampling dt;
- execution overrun window;
- speed-aware clearance doctrine;
- Newtonian RCS-as-primary threshold;
- low-speed direction threshold;
- terminal orientation blend;
- accepted-program tolerances;
- tracking envelope/deadbands/reserves;
- tracking-loss invalidation time;
- final capture timeout;
- final physical acceptance tolerances.

The policy validates at the public API boundary. Vehicle profile validates at
the same boundary.

This removes the previous file-scope `kExecutionDt`,
`kTraceSampleSeconds`, `kTrackingLossInvalidateSeconds` and duplicate
literal tolerances from the runtime calculation path.

### One maneuver clock / storage-page semantics

Already implemented in the same cleanup sequence:
- <=16-sample objects are storage pages, not maneuver phases;
- all pages share one accepted maneuver clock;
- page changes are transparent indexing;
- diagnostics now say PROGRAM STORAGE PAGES / STORAGE PAGE ADVANCES;
- the reference clock is monotonic.

Indefinite stale-reference homing is removed:
- short tracking loss is allowed as bounded correction;
- after the explicit tracking-loss timeout the accepted program is invalidated;
- the static stand reports `PROGRAM_INVALIDATED_TRACKING_LOSS` instead of
  freezing time and driving back to an obsolete sample.

### Attitude reachability

`buildReferenceAttitudes()` now integrates angular state using:
- actual initial angular velocity;
- angular acceleration limit;
- angular-rate limit;
- braking-aware target omega;
- average-omega orientation integration.

The old zero -> near-max-omega-in-one-sample reference jump is no longer
permitted.

### Architecture checker

The Stage-1/Stage-2 architecture checker was updated to pin:
- generic VehicleDynamicsProfile input;
- explicit ScenarioNavigationPolicy;
- common vehicle/capability projection helpers;
- monotonic program clock;
- absence of old frozen-reference state;
- absence of hard-coded Cobra runtime params;
- storage-page terminology.

Code baseline before documentation commits: `8b7134078fe1e7672e04085254c9f84c29ed1519`.

Target MinGW64 build/E2E validation is still required.

## 2026-09-22 target gate after API/clock normalization

Target machine results:
- architecture contract PASS;
- Stage-1 nominal route test PASS;
- follower corridor component test PASS;
- viewer/runtime build PASS;
- `navigation_runtime_pipeline` FAIL.

The first test assertion is stale: `testHighSpeedRunReacquiresInsteadOfOutrunningReference()`
still requires the old `REFERENCE CLOCK HOLD:` diagnostic even though reference-clock
freezing was intentionally removed. The runtime correctly reports
`REFERENCE CLOCK: MONOTONIC`.

There is also a real Stage-2 failure behind that stale assertion:
- high-speed Newtonian run: 743 trajectory samples;
- 742/742 actuator source coverage;
- 42 planned actuator segments are physically infeasible;
- tracking is invalidated at 0.51 s on storage page 1;
- max reference/velocity angle reaches 175.84 deg while actual body/velocity
  angle is only 0.94 deg.

Interpretation:
the new invalidation path is working. It now exposes that the authored
translation/reference program is still not physically co-timed with reachable
body attitude. Ruckig/guide timing can demand acceleration/braking whose force
direction requires a large hull rotation before that rotation can physically
occur.

Do NOT relax the 0.50 s invalidation timeout to hide this. Next fix is:
1. update stale E2E expectations to monotonic-clock/invalidation semantics;
2. move physical maneuver/attitude/thrust feasibility ahead of final Ruckig timing.

Baseline tested: `5cc0b668665f0adc11b160bad3bc2af314cdfe4d`.

## 2026-09-22 — second API-purity audit after stale E2E contract

The user's concern was valid: the first API cleanup was incomplete. A second
function-by-function audit of the active navigation path found concrete boundary
violations and hidden behavior.

### Fixed in this audit

1. **Stale E2E contract**
   - old "reacquire/reference hold" terminology is removed from active E2E
     assertions;
   - current contract is monotonic reference time + explicit invalidation.

2. **Accidental terminal-state reach-through**
   - a previous broad textual refactor had leaked an undeclared `terminal.*`
     alias into parser/orchestration code outside helpers that actually receive a
     terminal endpoint;
   - fixed;
   - API purity checker now explicitly rejects `terminal.*` outside functions
     whose API contains `const Endpoint& terminal`.

3. **Broad helper APIs narrowed**
   - `routePlanningClearanceMeters` no longer receives whole Scenario/Settings;
     it receives planning speed, authored clearance, style, ShipParams, policy;
   - `executionVehicleProfile` receives vehicle + resolved clearance/speed
     ceiling only;
   - `buildReferenceAttitudes` receives trajectory, initial basis/omega,
     terminal endpoint, law, vehicle physics and policy;
   - `makeProgramPhase/buildRoutePrograms` receive exact revisions/clearance
     rather than whole Scenario;
   - `ExecutionVehicle` receives an explicit `ExecutionVehicleInit`;
   - `executionTraceFrame` receives the selected target directly rather than
     the whole Scenario.

4. **One resolved kinematics snapshot**
   - start speed, finish speed, planning speed and start velocity are resolved
     once into `ResolvedRunKinematics`;
   - the old repeated `effectiveStartSpeedMps/effectiveFinishSpeedMps/
     effectiveStartVelocity` helpers are removed.

5. **Trajectory backend hidden behavior constants**
   - non-zero waypoint velocity threshold now comes from
     `minimumUsefulWaypointSpeedMps`;
   - lateral acceleration floor comes from
     `minimumAccelerationMps2`;
   - scalar speed floor comes from `minimumSpeedMps`;
   - path-capture threshold comes from `pathCaptureSpeedThresholdMps`.

6. **Production/transitional RuntimePlanner hidden doctrine**
   - `holdIntent(..., 0.5)` removed;
   - emergency threshold `urgency >= 0.75` removed from helper internals;
   - static/stale/conflict urgency and emergency threshold are explicit
     `NavigationRuntimePlanner::Policy` inputs.

### Classification after audit

Strict pure calculation kernels:
- NominalRoutePlanner
- GeometricPathPlanner
- TrajectoryGenerator/RuckigRoutePlanner backend
- RuckigTrajectorySolver
- OrdinaryPhysicalManeuverCompiler
- ManeuverProgramSampler
- ManeuverTrackingController
- TrajectoryFollower
- ManeuverPhaseGate
- NavigationExecutionReplanPolicy

Explicit deterministic stateful executors:
- NavigationRuntimeControlBridge
- PilotSkillExecutor
- DynamicMotionSystem

Orchestration boundaries allowed to compose values/I/O:
- loadScenarioDefinition
- makeScenarioVehicleParameters
- calculateScenario
- executeCalculatedRoute
- explicit diagnostic writers

`NavigationRuntimePlanner` remains a composition planner rather than a strict
math kernel because its API explicitly receives `StaticQueries&` and performs
bounded static queries. This is not hidden/ambient access: the service is an
explicit dependency. If we later require referentially pure planning, this seam
must be replaced by immutable precomputed query results/snapshots.

New normative document:
`src/game/navigation/NAVIGATION_API_CONTRACT.md`.

The architecture gate
`tests/architecture_contracts/check_navigation_api_purity.py` now checks the
active pipeline more broadly and pins the narrow helper/API contracts above.

Code baseline before documentation commits: `efb3999b71a18186c1e3622c6c51d9b0169b52be`.

No target-machine build/E2E has yet validated these new edits.

## 2026-09-27 — Assisted autopilot now uses the manual game-flight law

Latest Windows evidence before this change:
- `navigation_runtime_control`: PASS;
- `maneuver_tracking_controller`: PASS;
- `accepted_maneuver_program_builder`: PASS;
- `docking_advisory`: FAIL because the published manual corridor did not
  start along the hull nose;
- live Automatic returned
  `accepted-program-propulsion-program-infeasible` and restored Human.

Root cause is now explicit. Manual Assisted and Automatic Assisted were using
different motion laws:
- manual `DynamicMotionSystem::applyLocalFrameInput` bends VREL toward the
  hull nose through the dedicated automatic Assisted stabilizer;
- the navigation actuator path zeroed that stabilizer and tried to realize the
  same course change through physical `manoeuvreThrusterAccel` (2 m/s²).

That was the wrong model. The 2 m/s² manoeuvre/RCS system is precision
translation, not ordinary Assisted course authority.

Implemented replacement:
- `AcceptedManeuverProgram::TranslationMode` distinguishes
  `AssistedVelocity`, `NewtonianMainEngine`, and future `PrecisionRcs`;
- Assisted accepted programs publish no synthetic RCS actuator segments;
- Follower samples target forward speed from the accepted velocity reference;
- Bridge publishes `navigationAssistedFlightModel*`;
- `DynamicMotionSystem::applyNavigationAssistedFlightModel` calls the same
  `applyLocalFrameInput` used by manual Assisted flight;
- ordinary Assisted Automatic keeps physical manoeuvre/RCS at zero;
- small Follower lateral correction consumes the automatic Assisted
  stabilization budget, not keypad/RCS authority;
- production docking derives Planner lateral/course authority from
  `assistedLateralStabilizationAccelerationLimitMps2`, while generic legacy
  vehicle-profile callers retain their previous hardware-envelope semantics.

Newtonian is now a separate ordinary-transit doctrine:
- docking geometry requests piecewise-straight turns instead of aircraft-like
  fillets;
- ordinary Accepted programs may use hull rotation + primary main engine;
- they may not rescue a bad trajectory with sustained precision RCS;
- a trajectory that asks for lateral Newtonian route thrust is rejected as
  `newtonian-main-engine-program-infeasible`.

This intentionally exposes the remaining Newtonian compiler work: dedicated
`coast -> rotate -> burn` / `accelerate -> rotate -> brake` sequencing,
rather than hiding it behind RCS.

Manual nose-first failure was also corrected: the semantic initial-forward
segment is now protected from generic corner filleting, and the straight
Newtonian geometry path proves every segment collision-free.

New regressions pin:
- Assisted Builder accepts route course change independently of the 2 m/s²
  precision RCS limit;
- Newtonian ordinary transit cannot spend precision RCS as route thrust;
- Assisted Automatic uses the canonical game-flight law and realigns VREL to
  the nose within the accepted 3 s window while ordinary RCS remains zero;
- manual nose-first lead cannot be removed by filleting.

Fresh Windows `verify_docking.sh`, full MinGW build and live Automatic flight
are still required before this replacement is accepted.

## 2026-09-27 — rotating-terminal gate exposed a real angular planner bug

Fresh Windows `verify_docking.sh` reached the rotating-terminal trajectory gate
and failed:

```text
trajectory_generator_angular: FAIL
angular trajectory cannot reach requested terminal state
```

The test itself is physically valid: 90 degree body turn, `omega_max=1.0`,
`alpha_max=0.5`, terminal `omega=(0,0.2,0)`, and a 200 m trajectory with a
180 m terminal orientation blend. Do not weaken this test or its 5 degree /
0.05 rad/s acceptance envelope.

Root cause: `compileBoundedAngularKinematics` bounded each forward angular
acceleration step but treated terminal angular velocity as a final-sample
override. A feasible trajectory could therefore reach the penultimate sample
with too much angular-rate error to remove in the last dt, then reject itself.

Fix on main:
- every sample now reserves backwards-reachable angular authority for the exact
  terminal omega;
- invariant:
  `|omega(t)-omega_terminal| <= alpha_max*(T-t)`;
- if current omega has already left that cone, generation fails honestly;
- desired omega is projected into the next sample's terminal-reachable cone
  before the ordinary per-step alpha clamp;
- no terminal tolerance or angular capability was relaxed.

The native angular test now additionally asserts that the penultimate omega is
already reachable from the requested terminal omega within the last dt. Fresh
Windows rerun is pending.

## 2026-09-27 — native docking/trajectory gates are green; static checker typo fixed

Fresh Windows `verify_docking.sh` results:
- `navigation_runtime_control`: PASS;
- `maneuver_tracking_controller`: PASS;
- `docking_advisory`: PASS;
- `accepted_maneuver_program_builder`: PASS;
- `trajectory_generator_angular`: PASS;
- manual docking static contract: PASS.

This is the first fresh target evidence that the nose-first corridor regression
and terminal-angular reachability fix both pass natively.

The remaining failure was NOT production behavior. The automatic-docking static
checker mistakenly required the regression message
`angular planner deferred terminal omega correction to the final sample`
inside `TrajectoryGenerator.cpp`, although that message correctly belongs to
`TrajectoryGeneratorAngularTests.cpp`.

The checker now:
- pins `remainingBefore`, `remainingAfter`, `maxTerminalDelta` in
  `TrajectoryGenerator.cpp`;
- pins the penultimate-sample anti-snap assertion in
  `TrajectoryGeneratorAngularTests.cpp`.

No navigation/physics implementation changed after the green native result.
Full static verify rerun is still required before build/live acceptance.

## 2026-09-27 — automatic static checker made formatting-independent

Fresh target native gates remain green. The next verify failure was again static
only:

```text
[FAIL] src/game/server/GameServer.cpp:
missing automatic-docking token
'trajectoryRequest.hasTerminalAngularVelocity = true'
```

Production code was verified and is correct:

```cpp
trajectoryRequest.
    hasTerminalAngularVelocity = true;
trajectoryRequest.
    terminalAngularVelocityRadPerSecond =
        terminalAngularVelocityMapRadPerSec;
```

The same terminal omega is also passed to
`AcceptedManeuverProgramBuilder::Request`.

Root cause: the checker depended on exact C++ whitespace/line wrapping.

Fix:
- terminal angular check now uses semantic tokens rather than one exact formatted
  expression;
- the automatic-docking checker helper now falls back to whitespace-compacted
  token matching, so source formatting alone cannot produce another false
  architecture failure.

No production navigation, physics, Planner, Follower or trajectory code changed
after the already-green native run.

## 2026-09-27 — live gate exposed self-preflight and launch-geometry defects

Fresh live evidence after the native docking gates:
- cockpit boresight is useful and remains accepted;
- the visible docking tunnel did not look like a route the current ship would
  naturally enter: it showed a nose-forward line followed by a visually hard /
  displaced transition;
- pressing START DOCKING with no previously calculated manual route made the
  button green and showed AUTOMATIC DOCKING MODE, but the ship did not move;
- FramePerf showed `dock_request=1`;
- the captured log contained no `[DockAuto]` lifecycle line at all.

This proves the presentation request became active but the old Automatic UX was
not self-contained/observable enough.

Three concrete defects were found.

1. **Visible Assisted guidance still used the legacy generic vehicle profile.**
   That profile derives lateral authority from physical
   `manoeuvreThrusterAccel` (2 m/s²). This contradicted the newly accepted
   Assisted game-flight law. Visible manual/preflight routing now calls the
   explicit control-law overload of `makeNavigationVehicleProfile`; Assisted
   turn geometry therefore uses the same velocity-to-nose authority as
   Automatic/manual flight.

2. **The nose-first fix protected the entire 500 m lead from filleting.**
   That guaranteed the first segment direction but created a hard corner at
   `routeSearchStart`. Planner now protects only a visible straight prefix
   (normally half the accepted lead) and allows the remainder of the lead to be
   consumed by a tangent circular first-turn fillet. Initial/first-turn
   presentation cadence is also densified. SpaceState publishes normal docking
   guidance at 150 m cadence instead of the old 500 m sparse chords.

3. **START DOCKING depended on an already visible route for good UX.**
   Automatic is now self-contained:
   ```text
   START DOCKING
    -> route-preflight when no matching corridor exists
    -> temporary authoritative Autopilot stop/stabilize
    -> authoritative Hub snapshot
    -> visible advisory route/tunnel
    -> temporary authority hand-back
    -> BeginAutomaticDocking
    -> server Automatic planning/execution
   ```
   An already calculated route for the same dock is still reused.

Server Automatic now also supplies the actual stopped hull forward axis and
nose-first lead to its own DockingAdvisoryRequest and uses 150 m gate cadence,
so executable geometry starts with the same launch semantics as the visible
preflight.

Automatic request diagnostics are now flushed and every early server rejection
has an explicit `[DockAuto] rejected ... reason=...` line. A successful
client request logs `phase=requested`; a route-less start first logs
`phase=route-preflight`.

Native DockingAdvisoryPlanner regression was strengthened beyond the old
first-vector check. It now requires:
- multiple visible straight-prefix segments;
- then a real launch turn;
- no >30 degree discrete heading kink over the first 1200 m.

Fresh Windows verify/build/live evidence for these newest edits is pending.

## 2026-09-27 — GameServer compile regression corrected

Fresh MinGW build failed in `GameServer.cpp` because the new
`autopilot-authority-denied` diagnostic was accidentally inserted into
`beginDockingGuidancePreparation()`, whose API has `requestSerial` but no
`ClientShipCommand command`.

Correction:
- manual preparation logs `[DockPrep]` and uses `requestSerial`;
- Automatic logs `[DockAuto]` and uses `command.requestSerial`;
- the Automatic authority-denied diagnostic is now located in
  `beginAutomaticDocking()` where it belongs;
- successful Automatic begin output is flushed.

Post-edit source check:
- `beginDockingGuidancePreparation()`: zero `command.requestSerial`
  references;
- `beginAutomaticDocking()`: authority-denied diagnostic present.

No Planner, Follower, route geometry, flight law, physics or docking state-machine
semantics changed in this correction. Fresh target build is pending.

## 2026-09-27 — live Automatic reaches server Planner; angular timing is current blocker

Newest live evidence:

```text
[DockAuto] request=1 phase=plan-failed
reason=trajectory:angular trajectory cannot reach requested terminal state
action=restore-human
```

This materially narrows the failure. START DOCKING is reaching the server,
Autopilot authority is being acquired, stabilization/planning is running, and
the server is deliberately restoring Human only because no executable
trajectory passed the angular gate. This is no longer an authority-transfer or
"make the player ship an NPC" problem.

The same Ship entity remains authoritative throughout:
```text
Human controller -> Autopilot controller
same Ship / ShipDynamics / installed hardware / fixed-step physics
```
That is the intended NPC-like ownership model.

### Angular timing defect and correction

The translational scalar Ruckig solve previously chose the fastest
collision-free route time independently of hull attitude timing. The angular
compiler then required the same clock to reach exact terminal attitude and
terminal omega. If the hull physically needed more time, generation failed
instead of slowing translation.

Multi-point trajectory generation now retries the SAME safe geometry with lower
translation speed only when the failure is angular-terminal reachability.
Current speed scales:
`1.00, 0.80, 0.64, 0.50, 0.40, 0.32, 0.25, 0.20, 0.16, 0.125, 0.10, 0.08, 0.06, 0.05`.

No angular rate, angular acceleration, collision, terminal orientation or
terminal-omega tolerance is relaxed.

A successful slowed trajectory reports
`angular-speed-relaxed`. If even the slowest allowed solve fails, the
trajectory now reports the exact angular cause, including sample/reachable
omega or final pose/omega error instead of only the generic message.

A native regression now creates a route whose fastest translation is too short
for the authored hull rotation and requires Planner to increase route time.

### Manual docking-frame cadence is now a hard user contract

The user explicitly requires:
- ordinary manual docking corridor: **500 m between published frames**;
- terminal/final zone: **250 m**;
- do not change this again without an explicit user request.

All accidental densification has been removed:
- SpaceState publishes `gateSpacingMeters = 500.0`;
- SpaceState publishes `terminalGateSpacingMeters = 250.0`;
- the former launch-only 125 m cadence override is deleted;
- the former fractional one-off frame used to land exactly on the dense-zone
  boundary is deleted;
- terminal cadence starts up to one normal interval early instead.

The native launch regression requires the first published frame gap to be
approximately 500 m. Static contract rejects a return of 150 m visible cadence
or fractional transition logic.

To preserve a real 500 m straight nose-first frame and still leave geometry for
a tangent first arc, normal initial-forward lead is now at least 1000 m and the
first 500 m is protected from the fillet. This changes geometry length, not
published frame cadence.

### Docking-port picking

Live feedback also showed far docking ports were hard to select and parent
infrastructure often stole the click.

Root cause: a dock had high pick priority, but once a projected physical
`hitPolygonPx` existed it replaced the normal hit radius. At distant zoom the
physical opening can be only a few pixels, so overlay picking missed and the
subsequent infrastructure triangle picker selected the whole assembly.

Current fix:
- docking port is selectable by precise physical polygon **OR** semantic
  screen-space dock radius;
- dock hit radius: 22 px;
- dock pick priority: 1000;
- among overlapping docks, the one nearest the cursor wins;
- parent infrastructure remains the fallback only when no docking-port overlay
  consumes the press.

`verify_docking.sh` now also runs the object-overlay contract so docking-port
picking is part of the docking gate.

Fresh Windows verify/build/live evidence for these newest changes is pending.

## 2026-09-27 — Automatic docking restored to two physical stages

The latest live failure plus code review exposed an architectural regression:
the long Automatic transit appended the pre-capture point to the same trajectory
and required that one program to end with the rotating port's exact orientation
and angular velocity.

That contradicted the agreed docking sequence.

Current Automatic runtime now has:
```text
Stage::ApproachHold
Stage::FinalIngress
```

ApproachHold:
- uses the ordinary collision-free docking route;
- ends at the advisory standoff/hold gate;
- terminal translational velocity is zero;
- does NOT append pre-capture;
- does NOT require exact dock terminal orientation/omega.

When ApproachHold completes:
- Autopilot commands BrakeToStop;
- all transit program/control state is discarded;
- runtime logs `phase=hold-complete next=final-ingress`;
- runtime stabilizes again from the real authoritative ship state.

FinalIngress:
- recomputes the current/predicted docking-port pose;
- builds only the short hold-to-pre-capture maneuver;
- uses the complete port basis, so alignment includes yaw, pitch and roll;
- exact rotating-port terminal orientation and angular velocity are authored
  only here;
- generic angular-time relaxation may slow only this short ingress when needed.

The Ship entity does not become a separate NPC object. Human vs Autopilot is
controller ownership on the same physical Ship.

The prior docking_advisory native FAIL at a 36.08 degree change between
published 500 m corridor chords was a bad test criterion. Sparse display chords
are not the underlying curve-continuity proof. DockingAdvisoryPlan now exposes
`initialTurnPresent` and `initialTurnRadiusMeters`; the native test verifies
that the nose-first transition is an actual tangent circular fillet while
retaining the user-locked 500 m / 250 m frame cadence.

Fresh Windows verify/build/live evidence is pending.

## 2026-09-27 — current live stop is before server Automatic

Newest live log shows `dock_request=1` but no `[DockAuto]` or
`[DockAdvisory]` lifecycle line.

Therefore the observed no-motion case is currently before server planning:
the UI has a pending docking request, but there is no evidence yet that
SpaceState sent route-preflight or BeginAutomaticDocking.

A one-shot `[DockRequest]` trace is now emitted whenever a new pending serial
enters SpaceState. It reports mode, previous path serial, preparation serial,
Automatic serial, visible-route reuse and whether route-preflight is required.

No flight/planner/physics behavior changed in this diagnostic iteration.

## 2026-09-27 — docking UI claimed Automatic before authority existed

Code trace found that the cockpit selected `AUTOMATIC DOCKING MODE` from the
pending UI request, even before `BeginAutomaticDocking` was sent or the server
confirmed Autopilot ownership. A visible corridor is a client advisory product,
not evidence of an accepted maneuver. The sampled `dock_request=0` in FramePerf
reports only that frame; it does not establish the state at the button press.

Current diagnostic candidate logs one serial at UI request, SpaceState entry,
each client command, server receipt/rejection and request clearing. The cockpit
now says PREPARING AUTOMATIC DOCKING until the Automatic request has actually
observed authoritative server Autopilot ownership. No movement/planner law was
changed. Target build and a fresh clicked-button trace remain pending.
Local static Automatic contract and localization JSON parse passed. The full
`verify_docking.sh` could not start in this Linux workspace because `cmake` is
not installed; it must run on the target Windows toolchain.

## 2026-09-27 — Automatic corridor and failure round trip

Code audit found that the active Automatic branch returned from SpaceState
before refreshing the visual docking corridor. The server also sent only a
Boolean Autopilot ownership state back to the client; its failure reason was
logged server-side and lost to the cockpit. The current candidate keeps
refreshing the same Hub-local corridor while Automatic owns the ship, retains
the last route with an unsafe warning on advisory-only validation failures,
and carries a serialled terminal result/reason in the per-session snapshot.
The wire version is now 12; old client/server binaries cannot interoperate.
Failures in preparation, planning, handoff, and follower recovery now have
specific logs. New static contracts pass; native build and live evidence await
the target Windows toolchain. No physical maneuver acceptance was relaxed.

Publication: the complete diagnostic and corridor/result tree was published
to public `forzub/Elite` main as `4da95fa3` after explicit user approval.
GitHub verified the published tree SHA matches the tested local tree. Native
Windows verify/build/live acceptance remains pending.

## 2026-09-27 — live bridge rejection at automatic execution

Windows built both binaries and the live run reached server Automatic request 1.
Repeated `[DockAuto] phase=replan reason=control-bridge-step status=2
snapshot_valid=0` shows the program reached the Follower/bridge boundary but
never produced a physical control sample. `PilotSkillExecutor::Status::InvalidInput`
is 2. Code inspection found that the bridge was reset at a planned future
absolute universe epoch, then first stepped at a later real tick using only the
one-tick gameplay delta. The executor requires `stepTime - resetTime == delta`
within a strict tolerance; any planning/installation gap fails that check.
The large absolute epoch also makes strict double precision checks fragile.

Current correction gives the pilot executor a local clock reset to zero on
program installation and advanced by the exact gameplay delta on both Aligning
and Executing steps. Follower and trajectory sampling still use absolute
universe time. Bridge results now distinguish invalid intent, invalid actuator,
and executor rejection. Three repeated bridge failures terminate the request
with a server result instead of replanning indefinitely. Local static checks
pass; target Windows native and live gates are pending for this correction.
This corrected implementation is the next public-main candidate for the
target-machine gate; its publication alone will not imply runtime acceptance.

## 2026-09-27 — pilot clock check uses gameplay-scale tolerance

The executor uses elapsed time for decision/reaction/latency scheduling and
`deltaSeconds` for its command filter. A whole missing tick would make these
disagree, but nanosecond agreement is unnecessary. Keep the local pilot clock
from the live bridge fix and accept time/delta rounding up to the greater of
0.1 ms or 1% of the step; larger discontinuities still return InvalidInput.
The bridge regression covers harmless rounding and a missing tick. Windows
native and live Automatic verification remain pending.

## 2026-09-27 — live Automatic passes bridge, then rejects program pages

The latest Windows build started physical movement for request 1; repeated
`no-active-program-page status=0` then made the ship alternately accelerate and
brake as the server replanned. `status=0` is Timeline::InvalidInput, not a
network packet failure. Accepted pages use a 1998-based universe epoch of
hundreds of millions of seconds. Timeline checked their adjacency using two
separately rounded absolute timestamps within 1 ns; these can differ by about
0.12 microseconds even when local sample offsets are continuous. Selection now
checks adjacency in local maneuver time and retains the canonical absolute start
for activation. A genuinely invalid timeline ends the request with a server
result instead of looping; BeforeStart holds until the planned start. Windows
native and live flight for this correction remain pending.

## 2026-09-27 — live tracking-envelope loop after timeline correction

Latest Windows build no longer logs the invalid page timeline. It moves the ship
but repeatedly logs `tracking_error=1 propulsion_ok=1`; every rejection stops
and replans, producing visible start/stop motion. Which tracking component fails
is not present in the old log. The reporter also perceives a speed snap to zero.
Physics code only explicitly zeroes velocity in BrakeToStop at or below the
configured `stopSpeedEpsilonMps` (default 0.05 m/s); a larger jump is not
proven by the supplied log. Current candidate logs raw and effective envelope
position/speed errors, attitude/rate errors, limits, actual/reference speeds,
step delta and stop threshold. It flags any authoritative map-velocity change
above installed acceleration times elapsed time plus 0.5 m/s diagnostic slack.
Three repeated tracking failures end the request with a returned reason instead
of an endless restart. Windows native/live gate must locate the violated
envelope and confirm whether the reported speed snap is physical or visual.

## 2026-09-27 — live tracking failure identifies Assisted speed-response mismatch

Windows compiled client/server and ran two Automatic requests. Each reached
page 1 at elapsed 0.34 s with ship speed 6.155 m/s against planned 15.706 m/s.
Effective position error was <0.001 m (25 m allowed), angle <0.09 deg
(15 deg allowed), angular-rate error <0.05 rad/s (0.25 allowed). The only
violation was effective along-track speed error 8.55 m/s > 8 m/s, after the
existing 1 m/s FreeTransit deadband. No `physics-watch` discontinuity appeared
in the supplied console. User reported subsequent 143 m/s after Human handback.

Cause: the planner budgets installed forward main acceleration; Assisted
uses a first-order speed response with gain `throttleAccel=5 s^-1` on Cobra.
Passing only reference speed as target makes a ramp of ~46 m/s^2 lag by
roughly a/gain=9.2 m/s, reproducing the measured discrepancy even with
physically capable engines. The candidate offsets the Assisted target by
the PilotSkill-executed forward acceleration divided by the same response
gain; normal main-engine and speed caps still constrain actual acceleration.
On Automatic completion/failure the server explicitly requests bounded
BrakeToStop and clears persistent Assisted speed target before restoring
Human authority. The previously neutral handback could leave the old speed
target active. Focused native and Windows live confirmation are pending.

## 2026-09-27 — late Assisted turn exceeds vector tracking

Latest Windows build passes and Automatic flies through page 390 (~117 s).
Actual scalar speed 97.598 m/s nearly equals reference 97.714 m/s, but vector
velocity error reaches 9.918 m/s (effective 9.898 > 8 m/s). Position error
13.08 m is under 25 m; forward error 0.84 deg is under 15 deg. The user ended
the run after one recovery; this excerpt does not prove a terminal failure.

Trajectory samples budget lateral turn acceleration, but Assisted physics had
consumed the pilot-executed demand only longitudinally. This candidate adds
the executed lateral component to Assisted stabilization, subject to its
existing physical and load limits. New turn regression and recovery-vector
diagnostics require Windows verification. Scalar path-progress Ruckig also
uses the minimum local speed/curvature bound over the entire route. Thus a
local manual-corridor recommendation of 500 m/s and an executable ~98 m/s
route can coexist. Route-local speed scheduling remains open.

## 2026-09-27 — Windows native turn test exposed stale test capability

The Windows docking gate compiled, and maneuver_tracking_controller,
docking_advisory and accepted_maneuver_program_builder passed. The new
navigation_runtime_control test failed, but the supplied CTest summary does
not contain the failed assertion. Source inspection identifies a deterministic
fixture error: the test demanded 4 m/s^2 of lateral turn feed-forward while
capabilityParams() set strafeAccel=2 m/s^2. The production Assisted actuator
correctly caps the command at 2. The turn fixture now installs 8 m/s^2 of
lateral authority, checks that 4 arrives, then demands 100 and checks that
the actual actuator stays at 8. Production physics was not changed. Native
rerun and live dock turn are pending.

## 2026-09-27 — live arrival at approach hold, stage transition blocked

Latest Windows build/live flight reached the last ApproachHold page 422 at
126.82 s. The reference was stopped; actual speed was 1.0255 m/s and
position error 9.41455 m against the safe hold target. Tracking velocity
and forward errors were inside the 8 m/s and 15-degree limits, but angular
rate error 0.284315 rad/s exceeded 0.25. Follower rejected the expired
hold page before the server could enter the physical stop/FinalIngress state.
The subsequent long-approach replan from only ~9 m before HOLD failed with
`accepted-program-assisted-reverse-flight-not-supported`; automatic control
returned Human. The excerpt contains no `phase=hold-complete`, so it does not
prove a failed final-ingress program.

Current correction treats a final ApproachHold sample within 12 m and at
<=2 m/s as a bounded standoff capture, then physically brakes/damps and
replans the separate FinalIngress from fresh authoritative state. If an
ApproachHold recovery begins within 150 m of HOLD, it does not append the
normal kilometre nose-first launch that creates a loop near the dock. The
existing tracking and terminal tolerances remain unchanged. Reverse-flight
rejections now report page/sample/time/forward speed to distinguish any
future genuinely infeasible final-ingress plan. Windows native/live flight
and final pre-capture completion remain pending.

## 2026-09-27 — game-like correction for local docking errors

User rejected treating ~9 m hold offset and 0.284 versus 0.25 rad/s
angular deviation as a catastrophic failure. This matches the executable
chain: ManeuverTrackingController previously suppressed moving-program
feed-forward whenever any one tracking component crossed its envelope, and
the server discarded the program/replanned on angular-only errors.

Current candidate classifies isolated angular-rate drift when position,
velocity and forward direction remain in their envelopes. It preserves the
accepted translation and angular feed-forward, adds bounded angular-rate
feedback, and lets the server keep controlling the same ship inside 500 m
of the endpoint at <=5 m/s while the actual angular rate remains inside the
ship capability. Ordinary unsafe displacement/velocity errors, invalid
programs and propulsion failures still have their existing recovery. The
previous hold capture handles the 9.4 m standoff case. Native regression
checks that a 0.30 rad/s rate drift keeps a moving reference and produces
counter-torque, while a positional departure still invalidates feed-forward.

The current automatic docking objective ends at collision-free pre-capture;
physical port contact and latch are not implemented by this slice. Windows
native/live verification and the actual final-ingress outcome remain open.
## 2026-09-27 — route-local speed keyframes and HOLD spatial capture candidate

Fresh Windows flight request 2 on `7a81d58` did **not** dock: on page 436,
at 10.0222 m and 0.995736 m/s from HOLD, angular-rate tracking error
0.280841 exceeded the soft 0.25 rad/s limit. The controller entered recovery,
then a near-HOLD replan failed `accepted-program-assisted-reverse-flight-not-supported`
and returned Human control. The prior angular-only fix did not prevent this.
No `hold-complete` or FinalIngress was observed.

The route's scalar Ruckig backend applied its minimum curvature/speed constraint
to the entire route, suppressing speed on long straights. Current candidate
replaces this global scalar solve for multi-point routes with route-local speed
keyframes: cruise cap 0.8 of vehicle maximum, curvature/point/range limits,
backward braking and forward acceleration passes, and acceleration/cruise/brake
peaks on long straight intervals. Geometry and the independent physical
program acceptance remain intact. A numerical probe with a 10 km straight and
short 3D corner reached 400 m/s on the straight, <=70 m/s near the corner and
zero at the finish under a 20 m/s² acceleration limit.
The manual display gate recommendation now uses the same 0.8 cruise cap and
separate forward acceleration/backward braking limits. The builder's failure
helper previously accepted `const char*` while its detailed reverse-flight
diagnostic assembled `std::string`; that pre-existing compile error is fixed.

HOLD transition now measures distance from the physical ship to the actual
last program endpoint (not the current storage page endpoint), and can capture
a low-speed ship inside the 12 m safe HOLD volume before a trailing page expires.
It then physically brakes/damps and plans FinalIngress from a fresh state.
The approach trajectory uses the port's up vector as its route-frame roll
reference; visible corridor frames already use that same port-up reference.
Manual frame spacing remains 500 m / 250 m; the long protected launch lead is
unchanged to preserve the first nose-aligned display frame.

G++ syntax checks passed for the modified planner, trajectory generator and
server. Local automatic/manual architecture scripts passed. Full Windows
`verify_docking.sh`, client/server build and live request remain **pending**;
do not label this candidate accepted. The requested general control-interface
redesign, in-plane fine alignment and physical contact/latch are not complete.
## 2026-09-27 — live failure analysis and new diagnostic candidate

Published candidate `a0b5aa1` failed in user Windows flight. Small in-flight
angular-rate error caused an unnecessary recovery; later forward-speed lag
crossed the 8 m/s follower limit. Both translation and attitude were active:
forward-angle errors in the failed samples stayed under one degree. Manual
flight footage is a separate mode; it does not demonstrate Automatic command
output. A tight Assisted turn exists because the advisory planner could halve
non-terminal arcs below its speed-derived desired radius. Current local
candidate limits that tightening, allows bounded in-flight angular correction,
ramps manual tap torque, and emits `[DockAutoTrack]` command/physics samples.
Native advisory and targeted architecture gates passed; full Windows flight
still required, especially to diagnose the unresolved forward-speed lag.


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

For `SpatialCorridor`, `ManeuverProgramSampler::sampleSpatial` projects the authoritative vehicle position onto the current accepted path segment and derives P/V/A/attitude from that physical progress. The cursor is monotonic and sequential: a fixed step may advance at most one accepted segment, and a storage page may advance at most one page only after the real craft crosses the current endpoint plane while still inside that segment's proved cross-track tracking envelope. Crossing the plane far off-axis is treated as a corner cut, not progress. It never searches arbitrary future segments by nearest Euclidean distance, preventing a hairpin/canyon path from jumping through a wall to a nearby later branch. Page-local nominal timestamps do not advance or expire spatial execution; only the common accepted epoch gates initial start. Spatial completion is state-based and may complete early when the accepted terminal state is physically reached.

Trajectory geometry is sufficiently dense for this contract: scalar path progress is sampled every 0.02 s, about 8 m at 400 m/s, far denser than 500/250 m HUD frames. HUD cadence remains presentation-only and is not Follower geometry.

A corridor speed governor is now part of the accepted tracking policy via `spatialSlowdownStartFraction=0.50`. Inside the first 50% of the proved cross-track envelope, planned speed is unchanged. From 50% to 100%, along-path target speed is reduced continuously; at the edge it reaches zero so centering authority wins over forward schedule. Positive planned forward acceleration is reduced with the speed scale, curvature/lateral feed-forward scales with speed squared, and existing planned braking is never weakened. For SpatialCorridor the route-loss envelope is based on cross-track position/velocity plus attitude/angular state; deliberate along-track slowdown remains controlled but does not itself declare the tunnel lost.

Server runtime keeps `currentSpatialSegment`, selects spatial storage pages by real position, passes the monotonic cursor into Follower, and reports `reference_mode`, `ref_segment`, `ref_alpha`, `ref_distance_m`, and `spatial_speed_scale` in live tracking/recovery diagnostics. Regressions prove: physical progress beats nominal clock, cursor cannot jump backwards, hairpin branches cannot be skipped, a fixed step cannot skip multiple pages, spatial completion can occur before nominal time, and cross-track slowdown does not falsely trigger route loss.

## 2026-09-28 — SpatialCorridor checkpoint semantics: launch without clock and geometric route loss

Latest Windows live evidence exposed two defects in the first SpatialCorridor execution tick. The craft was physically at the first accepted point with zero speed, and spatial projection correctly stayed at segment 0 / alpha 0, but the interpolated reference therefore also stayed at zero velocity and zero feed-forward. Because spatial progress requires physical movement, the follower could never create the movement needed to advance the reference. The same sample was then rejected only because angular-rate error was about 0.517 rad/s against the 0.25 soft tracking limit while position, cross-track velocity and forward-angle errors were all inside their envelopes.

SpatialCorridor now treats accepted samples as spatial control points rather than a timetable. Position progress remains the exact monotonic projection of the authoritative craft onto the current accepted segment. If that projected reference is at a legitimate zero-speed lower point while the next accepted point has non-zero speed, Follower uses the current segment tangent plus the next point's accepted speed as the local launch velocity target. Universe time does not advance spatial progress. Once the craft physically moves, ordinary spatial interpolation resumes.

Spatial route loss is now geometric. Cross-track position, cross-track velocity and hull-forward angle remain hard tracking envelopes. Angular-rate mismatch is still measured and receives bounded angular feedback, but it no longer invalidates a SpatialCorridor while the hull course itself remains inside the accepted forward-angle cone. TimeScheduled maneuvers retain the stricter angular-rate envelope. New regressions lock both behaviors: a stopped first control point must produce forward demand without clock-driven position progress, and an angular-rate-only SpatialCorridor excursion larger than the previous recovery threshold must remain under Follower control.

Published candidate commits: `2608765` (spatial launch target), `5a2ca7d` (geometric spatial route-loss envelope), `3f199b7` (regressions). Windows native build and live flight are still required; this candidate is not yet accepted.



## 2026-10-04 — client autopilot executes one continuous accepted program

The client docking autopilot no longer treats fixed-capacity AcceptedManeuverProgram pages as semantic flight phases. At start it flattens all accepted pages into one monotonic spatial reference, removing the intentionally duplicated page-boundary sample. Runtime progress advances sequentially along this continuous centerline; storage page/segment indices are reconstructed only for capability/tolerance metadata and cannot reset or invalidate the flight reference.

The command target now comes directly from the authored program at current spatial progress: velocity, forward/up attitude and feed-forward acceleration. The previous client-side multi-page look-ahead steering heuristic is no longer part of the command path. Planner/TrajectoryGenerator therefore own how to fly the route; client execution only tracks that program.

Assisted cross-track recovery is a bounded servo around the program. It derives a capture velocity from the real lateral authority using v_capture^2 = 2*a_lateral*cross_track_distance, changes steering direction while preserving the authored scalar speed, and tends continuously to zero at the centerline. Small tracking-envelope errors no longer cause storage-page selection to discard the execution program.

Planner speed constraints are preserved down to zero; the former client floor max(0.5, gate.speedMps) was removed so an authored near-stop/full-stop checkpoint can survive compilation.

Published commits:
- 8b7f7607 — assemble and execute one continuous accepted route
- 71503579 — physical cross-track servo around continuous program
- 7c450f44 — remove storage-page selection from the control path
- 9f91cfac / 25952477 — regressions for continuous page boundaries and servo contract

Next gate: Windows MinGW build + client_route_autopilot CTest. Do not add checkpoint suffix re-generation yet; first establish that the full authored program plus bounded servo follows the live tunnel without page-driven resets. If this base is stable, the next layer is event/checkpoint re-anchoring of the remaining trajectory from the measured state.


## 2026-10-04 — checkpoint re-anchor updates only the remaining speed profile

Continuous client execution now keeps immutable authored geometry/orientation plus a mutable runtime speed profile. Sparse Planner/HUD gates are mapped once onto the continuous accepted path and act as execution checkpoints; their 500/250 m presentation cadence is not used as a steering look-ahead distance.

When real spatial progress crosses a checkpoint, the remaining runtime speed profile is re-anchored from the measured ship speed without rebuilding route geometry. A forward kinematic pass limits future speeds by available main-engine acceleration, and a backward pass preserves all future Planner speed constraints using braking authority. Runtime longitudinal feed-forward is reconstructed from v^2(s). The calculation uses the same feedback reserve and 0.90 planning authority fraction as initial client trajectory generation, so checkpoint correction cannot assume authority that was intentionally reserved for tracking.

Small/medium checkpoint error does not request a replan. If the measured checkpoint speed is already above the maximum speed from which the future constraints are physically reachable under braking, ClientRouteAutopilot marks replanRequired. SpaceState applies the current braking/control sample, stops that accepted execution and returns to the existing stabilization/replan path with reason=checkpoint-suffix-infeasible.

Live DockClientTrack telemetry now includes checkpoint=<index> and profile_rev=<revision>; the revision increments only when a checkpoint re-anchor actually occurs.

Published commits:
- 3377d208 — re-anchor speed profile at corridor checkpoints
- 729d462a — keep checkpoint re-anchor inside reserved authority
- a820bbdf — detect physically infeasible checkpoint suffix
- dcf1d643 — replan only for infeasible checkpoint suffix
- a99674b6 — trace checkpoint/profile revision in live telemetry
- 2a331b8e / 6a25f6c5 — regression coverage for recoverable vs infeasible checkpoint states

Next gate: Windows MinGW build and client_route_autopilot tests, then live automatic docking. Expected evidence: profile_rev increments at sparse frames; ordinary errors continue on the same route; checkpoint-suffix-infeasible appears only for genuinely unreachable future speed constraints.


## 2026-10-05 — stopped checkpoint semantics corrected: v=0 does not imply a=0

Live client evidence showed continuous execution parked forever at the first spatial sample with target_speed_mps=0, actual_speed_mps=0 and no translational speed command. The underlying issue was semantic, not geometric: code treated a zero-speed reference sample as if it also meant zero acceleration.

The motion contract is now explicit. A checkpoint speed is only a boundary condition. For two adjacent spatial samples with non-zero distance, longitudinal acceleration is derived from v1^2 = v0^2 + 2*a*ds. Therefore a stopped sample followed by a moving sample correctly has positive acceleration.

TrajectoryGenerator now preserves the first leg's acceleration on the t=0 sample instead of hardcoding initial acceleration to zero. ClientRouteAutopilot runtime profiles reconstruct longitudinal acceleration from adjacent speed states and path distance. The temporary stoppedCheckpointLaunch special case was removed.

PredictivePilot Assisted speed control now uses standard feed-forward + feedback:
a_cmd = a_ref + (v_ref - v_actual)/horizon.
Thus v_ref=0 at a boundary no longer deadlocks if the authored trajectory requires positive acceleration.

Live telemetry now reports target_accel_mps2. Regression test requires the first stopped accepted sample to retain v=0 while carrying positive forward acceleration and requires Assisted to emit positive targetSpeedRate from that state.

Published commits:
- c605a102 — preserve initial acceleration at stopped trajectory boundary
- 2cb6a96f — derive runtime checkpoint acceleration from adjacent speed states
- 0407c203 — execute Assisted speed with acceleration feed-forward
- a76f8242 / 758ae9d0 — expose target longitudinal acceleration in live diagnostics
- 707d0250 — regression for v=0 with non-zero acceleration

The previous stoppedCheckpointLaunch workaround is obsolete and removed.


## 2026-10-05 — route speed is a ceiling; overspeed brakes, missed gates stay missed

RouteGate.speedMps is now explicitly defined as a route-local maximum/recommended speed ceiling, not a mandatory vehicle state. Execution may be slower. If measured speed is above the ceiling, the correct response is braking while continuing forward along the accepted route.

TrajectoryGenerator no longer rejects an otherwise valid route because the measured initial speed is above vehicle/route limits or above the backward reachable speed for a future constraint. Measured initial speed is authoritative state. A forward physical repair pass propagates the fastest speed achievable under maximum braking until the authored ceiling can be caught again. Temporary unavoidable overspeed is therefore represented in the runtime profile rather than treated as route failure.

When entering a segment already above its ceiling, keyframed progress now begins braking immediately. The previous peak-speed construction could coast at the excessive speed and postpone braking; that behavior is removed.

Checkpoint suffix re-anchoring follows the same contract. Overspeed never sets a replan-required flag by itself. The remaining runtime profile is raised where necessary to the minimum physically reachable speed under braking, then continues converging toward the authored ceilings. SpaceState no longer leaves Automatic mode for checkpoint speed overshoot alone.

Spatial progress is monotonic. If the craft has already crossed one or more route/checkpoint planes, ClientRouteAutopilot advances through those missed gates and aligns to the future route. A later positional wobble cannot move currentContinuousSegment backward, so the ship is never commanded to return to an already missed gate.

Published commits:
- 076fd813 — recover initial overspeed by braking instead of rejecting route
- ed5eeb7b — keep checkpoint overspeed on route and brake through suffix
- 8e543ac0 — never abandon docking route for speed overshoot alone
- adb3572d — define RouteGate speed as ceiling, not state
- 75e8e4f3 — brake immediately when entering route overspeed
- 6e9a6c73 — regressions for overspeed braking and monotonic missed-gate progress
- 7ba5223f — initial overspeed must be accepted and command braking

Next gate: Windows native client_route_autopilot build/test, then live docking. Expected behavior: no checkpoint-suffix-infeasible recovery for speed alone; targetSpeedRate becomes negative on overspeed; checkpoint/profile revision advances forward; missed gates are skipped and never reacquired behind the craft.


## 2026-10-05 — trajectory-cleanup pass: damp lateral capture and stop at final guidance frame

Live automatic docking now traverses the route and speed profile, but showed two distinct execution defects. First, the position-only cross-track servo used v_capture=sqrt(2*a*d) without accounting for already accumulated lateral velocity. This could accelerate the craft toward center, cross the centerline with residual side speed, flip the error sign, and command the opposite turn. Second, runtime checkpoint repair could overwrite the authored terminal zero-speed constraint with a non-zero physically reachable fly-through speed, causing oscillation after remaining_m reached zero.

ClientRouteAutopilot cross-track capture now includes lateral stopping distance. It measures current cross-track closing speed, computes d_stop=v_in^2/(2*a_lateral), and only commands capture velocity from the remaining capture distance max(0,d-d_stop). Thus lateral steering is reduced before the centerline rather than after crossing it.

A zero terminal speed is now treated as a hard HOLD boundary. Checkpoint overspeed repair may raise intermediate speeds but must preserve nominalSpeedProfileMps.back()==0. When the final continuous segment is reached, PredictivePilot enters stop mode for an authored terminal stop. Automatic docking starts ClientRouteAutopilot with holdAtTerminal=true: after arriving/stopping it remains active instead of completing/releasing control.

Dock orientation alignment already uses the real docking-port up vector: SpaceState passes executionPort.up into ClientRouteAutopilot, and the HUD frames are built from the same renderPort.up. During terminal HOLD the ship therefore remains stopped while PredictivePilot continues forward/up attitude correction, including roll, until hull bottom aligns with the dock-bottom mark. Dock ingress itself is deliberately disabled for this phase.

Live telemetry now includes cross_closing_mps, cross_capture_mps, forward_err_deg, up_err_deg and terminal_hold so the next run can separate centerline-control error from angular-control error.

Published commits:
- 05e7a1b9 — damp cross-track capture and hold at terminal frame
- 012daf17 — automatic docking stops/holds at final guidance frame
- d3479adf — regressions for braking before center and terminal HOLD
- 75f64626 — HOLD continues dock-bottom roll alignment after translation stops
- 366b348e / 01f1b3be — diagnostic cross-track and attitude errors

Next gate: Windows MinGW client_route_autopilot test and live run. Do not implement dock ingress yet. Evaluate only centerline cleanliness, monotonic lateral damping, full stop at final frame, and continuing dock-up/bottom alignment.


## 2026-10-05 — cross-track correction moved from hull steering to lateral RCS

The latest live run proved that straight-line twitch was not primarily path geometry error. Cross-track was often only centimetres while forward error stayed below about one degree, yet pitch/yaw repeatedly saturated near full input. The cause was architectural: ClientRouteAutopilot converted fine cross-track correction into steeringForward, forcing the entire hull to rotate for centimetre-scale position trimming. Curves looked better because continuous tangent rotation masked this defect.

ClientRouteAutopilot now keeps hull desiredForward on the authored route tangent. Cross-track capture is executed through ordinary ShipControlState strafe/lift inputs using the ship's bounded lateral authority. The servo retains stopping-distance logic: it computes current cross velocity, target capture velocity, and reverses RCS before centerline overshoot. Fine trajectory correction therefore no longer pollutes pitch/yaw.

The same lateral RCS servo remains active at terminal targetSpeed=0, so terminal HOLD can center the craft in the final frame instead of freezing several metres off-center. terminal_hold telemetry now represents the actual final hold mode once the final stopped segment is reached, rather than only a fully satisfied completion tolerance.

The previous live run also showed the pre-HOLD storm: at ~14 m remaining, target speed ~39 m/s, forward error reached ~30 degrees while cross-track itself was only ~0.4 m. This was a direct consequence of nose-steering cross-track capture and is expected to disappear with the RCS split.

Published commits:
- 58955637 — move cross-track correction from nose steering to RCS
- 8dc2f89d — regressions require RCS correction without hull twitch
- 1ad1061c — log strafe/lift controls

Next gate: Windows client_route_autopilot test and live automatic docking. Evaluate straight-line hull stability, cross-track convergence, final-frame centering, and pre-HOLD behavior. Do not change Planner geometry or speed profile until this RCS split is validated.


## 2026-10-05 — Assisted steering now models nose-to-course lag; no manoeuvre-thruster path correction

The RCS cross-track experiment was rejected by live evidence. With the hull held almost perfectly on route tangent, strafe/lift saturated near full input while cross-track diverged from tens to hundreds of metres. Assisted route following therefore must be modeled primarily as nose direction driving velocity-course direction with finite lag, not as lateral translation.

ClientRouteAutopilot now uses the measured PredictivePilot::State.assistedCourseResponseSeconds (tau) as the steering horizon. Dynamic lead distance is L=v*tau using actual scalar speed. The follower predicts the craft's ballistic position after tau, samples authored route attitude at s+L, and samples an intercept point at approximately s+2L. If the predicted lateral miss is outside the centering band, the nose is aimed toward that future intercept; otherwise it follows the lead route tangent. This makes turn anticipation and correction depend on speed and measured course response instead of fixed 250/500 m look-ahead.

Fine centering no longer reacts to centimetres. The neutral band is defined as 10% of the already supplied docking tracking tolerance; with the current 20 m minimum tolerance this is a 2 m deadband. Sub-band error does not alter nose direction.

PredictivePilot angular execution is no longer time-optimal bang-bang for every attitude error. It now consumes the trajectory's authored angular velocity and angular acceleration feed-forward and tracks attitude/rate with a critically damped servo. Its response time is derived from configured angular rate limit divided by angular acceleration authority. This prevents fraction-of-a-degree errors on straights from commanding full pitch/yaw while preserving strong commands for large turns.

Automatic path correction and terminal HOLD no longer use strafe/lift/forward manoeuvre-thruster translation. HOLD arrests translational speed and continues attitude alignment only.

Published commits:
- 3d7946f2 — damped attitude/rate servo with authored angular-rate feed-forward
- 2301d127 — speed-and-tau-derived course lead with predicted intercept and meter-scale deadband
- a2962e3e — remove manoeuvre-thruster translation from terminal HOLD
- af7c84c5 — remove obsolete RCS authority from course follower
- 35c34334 — regressions for meter-scale deadband and predicted future miss
- c8a6b8b5 — require turn anticipation from measured course lag
- 0a29e2ae — log course_lead_m / predicted_cross_m / center_deadband_m

Next gate: Windows client_route_autopilot test and live run. Evaluate straight-line twitch, onset of pre-turn nose lead, predicted_cross_m versus center_deadband_m, and final dock-up alignment. Do not reintroduce lateral RCS for route tracking.


## 2026-10-05 — course lead may advance tangent, never replace a curve with an intercept chord

The first live run of speed-and-tau course prediction flew straights well but cut the inside of the first bend and exited the tunnel. Root cause: the follower sampled a future route state and then aimed directly at a still farther intercept point. On curved geometry that line is a chord, so the guidance law mathematically commanded shortcutting through the inside wall.

The course-lag model is retained, but its geometry contract is now strict: L=v*tau may advance the authored tangent/orientation reference only. It may never create a direct position target across a bend.

For position correction, the follower predicts the craft position after tau, measures cross-track error in the local plane of the future authored tangent, and adds only the lateral course-velocity component required to remove that local predicted error over one response time. Steering therefore remains tangent-following plus local normal correction; there is no future-point chord.

Published commits:
- f5e78af6 — preserve tunnel curvature in course-lag steering
- 02057ec6 — regression forbids chord cutting through a curved tunnel

Next gate: Windows client_route_autopilot test and live bend-only validation. Primary invariant: centerline tracking may use phase lead in attitude, but the commanded path must remain topologically inside the authored corridor and must not shorten curved sections with straight intercepts.


## 2026-10-05 — route geometry contract generalized beyond circular arcs

The Planner->Follower contract must not assume routes consist only of straight lines and circular fillets. Future route backends may author cubic Bezier, spline or other parametric curves. Follower must therefore execute authoritative parameterized geometry rather than reconstruct a route from dense sampled points.

RoutePlannerApi now exposes RouteCurveSegment with Line, CircularArc and CubicBezier kinds, explicit path-progress interval [startProgressMeters,endProgressMeters], speed ceiling, endpoint frame data, circular-arc parameters, cubic-Bezier control points, and optional arc-length inversion knots for non-uniform parameterizations.

RouteCurveSegment evaluates position, first/second derivatives, tangent and scalar curvature. Cubic Bezier curvature uses |p' x p''|/|p'|^3. Arc-length knots map physical progress s to Bezier parameter t; they parameterize the exact curve but do not define its geometry.

Published commits:
- 76c917fd — add generic parameterized route-curve contract
- 14836c42 — make route curves evaluable by progress and curvature

Important: this establishes the API only. Current docking Planner still publishes dense executionGates as the active execution geometry; routeCurves are not yet populated/consumed. Next implementation step is to preserve authored primitives from DockingAdvisoryPlanner into RoutePlan.routeCurves and make ClientRouteAutopilot use those authoritative curves for tangent/curvature/speed-limit queries, leaving dense gates only for HUD/collision/proof.


## 2026-10-05 — docking execution now carries and follows authoritative parametric route curves

The docking backend no longer discards exact curve geometry after sampling. RoundedCandidate now stores both authoritative routeCurves and sampled points. Straight sections are preserved as Line primitives. Each circular fillet is preserved before sampling with exact start/end, center, normal, radius, sweep and curve-local speed ceiling. The exact terminal quarter-circle is preserved the same way, followed by the final straight.

DockingAdvisoryPlan carries routeCurves and a production docking plan is no longer valid if that vector is empty. This prevents silent fallback to sampled-only geometry in the real docking path.

RoutePlanner propagates routeCurves unchanged into RoutePlan.

ClientRouteAutopilot stores those curves. Accepted maneuver samples still provide timing, speed profile, linear/angular feed-forward and terminal state, but when routeCurves exist the follower now takes spatial position, tangent and curvature from Planner's parametric geometry. Course-lag phase lead advances along the authoritative curve by s+v*tau; it does not infer shape from continuousSamples. Current/future cross-track planes are also based on the exact curve position/tangent. Curve-local maxSpeed is applied as an additional ceiling.

Because accepted trajectory progress is still measured on dense sampled geometry, current implementation maps sampled progress to exact curve progress by total-length scale. Dense sampling is ~10 m, so circle chord-length error is small; a future refinement may project directly onto the active primitive.

Compatibility-only RoutePlans that contain no routeCurves synthesize speed-neutral line primitives from executionGates; this path exists for old tests/generic callers only and never imposes a fake zero speed ceiling.

Live telemetry now reports curve=<index>, curvature_1pm and radius_m. Regression coverage constructs a sampled quarter-circle plus an exact CircularArc primitive and requires the follower to report the Planner radius/curvature rather than sampled chord geometry.

Published commits:
- c4090ebf — carry authored route curves through docking backend
- f7db8f6d — preserve exact lines and arcs before route sampling
- 63336f08 — propagate authored route curves through planner API
- 0cc00e0f — follow Planner curves instead of sampled route geometry
- 092bc28e — trace authoritative route curvature
- be5c7cf5 — regression requires exact Planner radius in follower
- c501e852 — keep sampled compatibility fallback speed-neutral
- 212d0fbe — require parametric geometry for valid production docking route

Next gate: Windows MinGW build plus docking_advisory and client_route_autopilot tests, then a live turn-only run. On a circular bend radius_m must remain the Planner-authored radius throughout the primitive and cross-track must remain bounded without chord-cutting.


## 2026-10-05 — live parametric-curve run exposed three execution-control defects

The latest live run proved that authoritative routeCurves reached ClientRouteAutopilot, but execution still failed for three independent reasons.

1. Curve-boundary phase error. Exact curve progress was temporarily obtained by one global scale between dense sampled route length and exact parametric route length. This shifted internal Line->Arc boundaries. Live evidence showed cross-track already ~13 m while still reporting curve=0, then ~53 m when curve=1 finally activated. ClientRouteAutopilot now builds a monotonic per-primitive correspondence between exact curve endpoints and continuous sampled progress. Route progress is mapped piecewise within each primitive, so an authored arc activates at its actual endpoint instead of after accumulated global length error.

2. Terminal stop had zero response margin. The live route stayed at ~288 m/s with 788 m remaining and still had ~102 m/s at 98 m remaining: almost exactly the ideal v^2/(2a) boundary, leaving no room for speed-controller/engine response. Terminal hold now applies a dynamic safe-speed ceiling obtained from d=v^2/(2a)+tau*v. This begins braking before the ideal mathematical boundary.

3. Angular execution was still consuming sampled angular-rate feed-forward even after spatial geometry moved to exact routeCurves. Live straights had curvature=0 and sub-degree orientation error but large yaw/roll commands. Non-terminal angular reference is now derived from exact curve geometry: desired course angular rate is omega=v*kappa along the curve's turn normal. Authoritative lines therefore request omega=0. Circular arcs request v/R; Bezier segments use the local binormal from p' x p''.

Assisted steering was also reformulated to use curvature phase lead instead of future-tangent look-ahead. Required steady nose lead on a curve is delta ~= tau*v*kappa. On a straight kappa=0, so the nose no longer starts changing course merely because a future arc lies v*tau metres ahead. Before a discontinuous Line->Arc transition, only hull ATTITUDE may prepare early; its preparation distance is derived from angular acceleration required to acquire the upcoming phase-lead angle. The actual desired course remains the authored straight until the curve begins.

Fine cross-track correction remains local and meter-scale; it does not create a new route.

Terminal HOLD now ignores trajectory angular feed-forward and locks to the final reference forward/up with zero desired angular rate/acceleration. A dedicated stopping-envelope attitude capture uses omega_target=min(omega_max,sqrt(2*alpha*angle)); therefore a 40-50 degree final error cannot decay to near-zero control while unresolved.

The fixed dock up vector passed from executionPort.up is now used directly as the non-terminal roll/up reference rather than sampled attitude-up states. This should make hull bottom alignment correspond continuously to the same dock up/down axis used to render the guidance frames.

New live telemetry includes exact_remaining_m, desired_omega_radps, actual_omega_radps and phase_lead_deg.

Published commits:
- 1b03506d — local exact-curve boundary mapping + response-margin terminal braking
- e3b25216 — GLM-core compatible curve mapping helper
- f8a46422 — physical large-error terminal attitude capture
- 9e8dc86e — terminal HOLD locks final dock pose
- 92494857 — exact-curvature angular-rate tracking
- b46be8bc — Assisted curvature/phase-lead steering, no future-tangent route shift
- 01d7d976 / fd50cd6e — exact remaining / angular-rate / phase-lead diagnostics
- 647615ef / df2355ab — regression coverage for exact curve entry, terminal response margin and strong HOLD capture

Next gate: Windows MinGW client_route_autopilot + docking_advisory tests, then live turn/terminal validation. Primary checks:
- curve index switches at the physical primitive boundary;
- on a straight desired_omega_radps=0;
- on an arc desired_omega_radps ~= actual_speed/radius;
- braking begins earlier than the old v^2/(2a) boundary;
- terminal HOLD reaches forward_err/up_err near zero with non-negligible rotation commands while errors are large.


## 2026-10-05 — terminal braking now uses measured longitudinal response and early full-brake trigger

Inspection of the real Assisted flight law showed that targetSpeedRate is not an instantaneous speed command. It slews the persistent targetForwardSpeedMps, and the Assisted flight controller then applies first-order speed-error feedback bounded by installed forward/reverse main authority and the shared linear-load envelope. throttleAccel is therefore a longitudinal response gain, not a direct thrust command.

The previous autopilot terminal logic only partially modeled this. It lowered target speed using a stopping envelope, but BrakeToStop itself still activated only after entering the last continuous segment. Live overshoot was therefore consistent with a controller that waited too long before bypassing the normal speed-following loop.

PredictivePilot now learns positive acceleration response and negative braking response separately from measured scalar-speed change per normalized ordinary control input. ClientRouteAutopilot uses the measured braking response conservatively (never above nominal braking authority) when available.

Terminal stopping now computes:

    d_required = v^2/(2*a_effective) + v*t_response

where t_response = 1/longitudinalResponseGain + dt. If d_required >= exactRemainingRouteMeters, terminalBrakeActive becomes true and BrakeToStop is requested immediately, even before the last continuous segment. The target-speed ceiling remains active as a softer outer layer.

This is equivalent to treating the final stop point as dynamically closer by the controller-response reserve, rather than multiplying all distances by an arbitrary fixed safety factor.

Mass/cargo audit: current propulsion descriptors expose main-engine authority in acceleration units (m/s^2), and ShipParams::massKg is not used to derate ordinary forward/reverse acceleration. Runtime engine damage changes availability but not thrust-to-mass ratio. No current cargo/payload-mass term was found in the effective propulsion path. Therefore loaded ships do NOT yet become systematically more inertial through cargo mass in ordinary local-flight acceleration; that is a separate physics-model task and should not be faked inside navigation. The measured-response braking estimator will nevertheless adapt to any future runtime mechanism that makes actual acceleration lower.

New telemetry:
- brake_a_mps2
- stop_need_m
- terminal_brake

Published commits:
- 4c36e4b4 — learn acceleration and braking response separately
- a00f0c60 — enter terminal brake from measured stopping envelope
- 030c3a6f — trace measured terminal stopping envelope
- 9eb1360f — regression requires early full brake at stopping envelope

Next gate: build/test client_route_autopilot, then live terminal run. Verify that terminal_brake=1 appears while exact_remaining_m is still greater than zero and before the old near-point overshoot region. Compare stop_need_m with exact_remaining_m at the trigger.


## 2026-10-05 — smooth Assisted turn entry, dynamic curve speed ceiling, damped cross-track recovery

Latest live evidence showed terminal braking now works, but turn execution remained novice-like: small bends triggered abrupt hull commands, actual angular rate overshot desired curve rate by 2–4x, cross-track then alternated sign and produced prolonged left/right shuttle. Speed reduction also began too late, frequently after the craft had already entered the curve.

The exact route geometry was correct. The remaining defects were in control dynamics.

1. Non-terminal attitude tracking is now paced by the measured Assisted course-response time, not the fastest mechanical hull-response time. PredictivePilot::Request carries angularTrackingResponseSeconds and the continuous route attitude servo uses max(actuator response, Assisted course response). Natural frequency is 1/responseSeconds. Desired angular-rate feed-forward carries the steady curve turn, while attitude error closes smoothly instead of commanding a near time-optimal stick pull.

2. Curve speed now has a dynamic Assisted limit in addition to Planner maxSpeed and hull angular-rate limits. For a curve radius R and measured course-response tau, one response interval bends the authored curve away from its tangent by a sagitta. The allowed sagitta is bounded by the existing docking tracking tolerance, yielding a speed ceiling from geometry and measured response rather than a hard-coded turn-speed percentage. On a straight before a curve, the follower solves the braking equation with controller response reserve to obtain the largest safe approach speed, so deceleration begins BEFORE curve entry.

3. The steady Assisted nose/course phase relation now uses the first-order vector relation sin(delta)=tau*omega rather than the small-angle approximation delta=tau*omega. Phase lead is therefore delta=asin(clamp(tau*v*kappa,-1,1)).

4. Pre-turn preparation ramps smoothly. The preparation distance considers both angle-capture time and time required to build the target angular rate from real angular acceleration. The blend uses smoothstep, giving zero slope at the start and end of the preparation interval. Desired hull angular rate ramps from zero to v*kappa instead of jumping at the curve boundary.

5. Cross-track recovery is now critically damped using both position error and measured closing velocity. The scalar model is e''+2/tau*e'+e/tau^2=0. The required lateral/course correction is converted into a nose/course correction angle through the Assisted response law. If the craft is already crossing toward center too fast, the correction reverses before centerline crossing, preventing the previous shuttle behavior.

New diagnostics:
- turn_vmax_mps
- turn_distance_m
- course_tau_s

Published commits:
- 04c6405c — smooth route attitude tracking on Assisted response time
- 6ea1508e — brake before curves and ramp Assisted turn rate smoothly
- 1deba040 — critically damp cross-track recovery and smooth turn entry
- 2d99e584 / 4c945772 — expose measured Assisted course-response time
- 9faf5e1a — regression: brake before dynamic turn limit
- 42d09688 — regression: pre-turn angular-rate ramp must be gradual

Next gate: Windows MinGW build/test and live first-turn validation. Expected telemetry before a bend: turn_vmax_mps below straight cruise when course dynamics require it, target_speed begins decreasing while turn_distance_m is still positive, desired_omega rises smoothly from zero, and actual_omega follows without 2–4x overshoot.


## 2026-10-05 — active corridor recentering and explicit dock-bottom roll tracking

Latest live run showed the authored curve is now followed substantially better, but the craft may exit a bend with a large lateral offset and then fly almost parallel to the tunnel for kilometres. Example: on final straight curve=8, cross-track reached ~88 m while cross-closing speed was only ~1-2.3 m/s at ~289 m/s forward. This is not a geometry failure; the cross-track controller was too soft after the earlier anti-oscillation changes.

Root cause: on an authoritative straight the controller requested desired_omega=0 even while asking for a non-zero course correction angle to return to center. The attitude servo therefore received "hold this offset heading" but also "angular rate zero", so it approached the new heading too slowly and allowed long parallel offset flight.

ClientRouteAutopilot now retains the critically damped cross-track position/velocity law but also publishes an explicit capture angular-rate component:

    omega_capture ~= delta_capture / tau_course

bounded by physical maximum angular rate. The non-terminal desired angular velocity is the vector sum of:
1. authored route curvature rate;
2. cross-track capture rate;
3. dock-bottom roll-alignment rate.

This gives the attitude controller the missing derivative target while preserving damping and avoiding bang-bang steering.

Dock-bottom alignment is now an explicit continuous roll task. Desired up remains the authoritative dock port up vector projected into the plane normal to the desired nose direction. ClientRouteAutopilot computes signed roll error around the desired nose axis and publishes a desired roll rate using the physical roll actuator response time (maxRollRate/angularAccelerationAuthority). This prevents dock-bottom alignment from disappearing inside the combined SO(3) error while pitch/yaw track the route.

HUD semantics were audited. GuidanceCorridorRenderer constructs corners[0..1] on the semantic -up edge and explicitly labels that edge as DOCK BOTTOM. Therefore aligning ship +up to port +up is the correct sign: the hull bottom (-up) then aligns with the HUD bottom marker. The remaining live defect was control authority/rate, not a sign inversion.

New live diagnostics:
- capture_angle_deg
- capture_omega_radps
- roll_err_deg
- roll_target_omega_radps

Published commits:
- 2c493e28 — actively capture corridor center and dock-bottom roll
- 402c1f81 — trace corridor capture and dock-bottom roll targets
- 0e974246 — regressions require active parallel-offset capture and continuous roll alignment

Next live validation:
- after an arc exit, capture_omega_radps must remain non-zero while cross_track is outside the centering band;
- cross_closing_mps should rise enough to recover centerline, then decay before crossing it;
- roll_err_deg should monotonically approach zero during cruise, not only after terminal HOLD;
- roll_target_omega_radps must remain non-zero while roll error is material;
- no manoeuvre-thruster strafe/lift may be used.
