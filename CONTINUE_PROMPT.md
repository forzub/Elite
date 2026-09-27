# CONTINUE PROMPT — Elite Automatic docking bridge clock

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
