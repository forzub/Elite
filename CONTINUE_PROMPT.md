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
