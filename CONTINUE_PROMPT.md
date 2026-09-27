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
