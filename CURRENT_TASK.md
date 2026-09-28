## 2026-09-27 — fetch published docking candidate and fly

On the Windows MSYS2 MinGW64 checkout: fetch/pull `origin main`, verify the
published merge commit `c3b5dc2` is an ancestor, run `bash verify_docking.sh`,
then build the full navigation-runtime suite and run its CTests. After those
gates, run `bash build_mingw64.sh` and launch `build/EliteGame.exe` while
capturing `[DockAuto]` output. Observe actual HOLD capture, stop, FinalIngress
and pre-capture. Physical dock contact/latch and in-plane terminal trim are
not accepted by this candidate.

## 2026-09-27 — Windows gate after test-contract audit

The updated `verify_docking.sh` gates docking advisory, accepted-program
builder, sampler, runtime control, tracking, angular trajectory, manual and
automatic architecture, flight control, API purity, and system-map overlay.
Run it on the canonical MSYS2 MinGW64 checkout only after the local candidate
becomes available there. Then rebuild with `bash build_mingw64.sh`, launch
`build/EliteGame.exe`, and capture every `[DockAuto]` line through HOLD and
FinalIngress. `origin/main` does not yet contain commits 91a8a57/3defe45 or
the test audit; `git pull` alone cannot deliver them. Separate failures of
non-gated broader static checks require their own diagnosis and correction;
do not silence them to claim docking acceptance.

## 2026-09-27 — verify motion-level autopilot candidate

Rebuild and run the native navigation/docking gates, then reproduce START
DOCKING through ApproachHold and FinalIngress with a free compatible port.
Confirm that the ship stops within 12 m of HOLD at <=2 m/s without a long
reverse-flight replan, stabilizes angular rate, and completes collision-free
pre-capture. Capture every `[DockAuto]` line and visible motion. Inspect the
new velocity-target control path for Assisted and Newtonian; ship physics
must decide installed propulsion. A successful pre-capture is not a physical
contact/latch. Current local changes need target-machine verification.

## 2026-09-26 — verify nose-first tunnel + physical Automatic entry alignment

Assisted remains accepted; do not retune it.

Pull current main, run the docking gate, rebuild and launch.

Acceptance:
1. Manual `CALCULATE TRAJECTORY`:
   - route/tunnel remains visible;
   - after returning to the cockpit, its first segment is directly in front of
     the ship along the center boresight, not immediately off to one side.
2. Cockpit HUD:
   - a fixed center boresight is always visible with the cockpit HUD;
   - it represents hull/nose direction, not VREL/flight path.
3. Automatic `START DOCKING`:
   - no old synchronous ~350 ms planning freeze;
   - expected first planning log is `phase=planning-async`;
   - if the hull is not already on the route-entry attitude, expect
     `planned ... phase=aligning`, then `phase=aligned-replan`, then a fresh
     asynchronous plan;
   - after physical alignment/replan, expected state is
     `planned ... phase=executing`.
4. The previous
   `accepted-program-propulsion-program-infeasible` must not recur merely
   because the stopped hull initially points away from the route.
5. Builder/physics must remain strict about real main/RCS capability. Do not
   substitute Assisted stabilization authority for physical manoeuvre thrust.
6. On successful execution the ship must physically move along the retained
   route/tunnel toward pre-capture.
7. Any genuine one-shot planning failure still restores Human authority safely.

If Automatic still fails, capture every `[DockAuto]` line and the exact reason.


# CURRENT TASK — manual docking advisory flight acceptance

## 2026-09-26 — Windows gate: no docking freeze + physical Automatic start

Assisted remains accepted; do not retune it.

Pull current main and run `verify_docking.sh`. The aggregate gate now includes
the new `trajectory_generator_angular` native test.

Then rebuild/run the game.

Acceptance:
1. CALCULATE TRAJECTORY still shows line + tunnel.
2. Selected action in dock/ship map cards is visibly bright green while inactive
   actions retain the normal style.
3. Manual guidance shows localized MANUAL DOCKING MODE; START DOCKING switches
   the blinking mode label to localized AUTOMATIC DOCKING MODE.
4. START DOCKING must not cause the old ~350 ms map/game freeze. Expected log is
   `phase=planning-async`; rendering/simulation should remain responsive while
   the worker plans.
5. The ship remains stopped during Planning, then receives a `planned ...`
   line and enters either Executing or Aligning.
6. The previous `accepted-program-angular-kinematics-infeasible` failure should
   be gone. If planning still fails, capture the exact new reason.
7. On successful planning, verify physical movement along the retained visible
   route/tunnel and continue through optional align/replan to pre-capture.
8. Any genuine one-shot planning failure must still restore Human authority;
   never reintroduce a synchronous retry storm.


## 2026-09-26 — verify restored tunnel and Automatic execution after terminal-spin fix

Assisted is accepted; do not change it in this iteration.

Pull current main and verify:
1. `CALCULATE TRAJECTORY` shows the docking route AND the previously accepted
   guidance tunnel/corridor.
2. With that route visible, `START DOCKING` must keep the same route/tunnel
   visible while server Autopilot owns the ship.
3. Automatic may stabilize first, but after planning it must begin physical
   movement along the route; no route deletion and no retry storm.
4. The observed generic `accepted-program-build-failed` should be gone.
   If AcceptedManeuverProgram still rejects the trajectory, capture the new
   exact reason such as `accepted-program-angular-kinematics-infeasible` or
   `accepted-program-propulsion-program-infeasible`.
5. Keep checking that Human authority is restored cleanly on any one-shot plan
   failure.

The new terminal-spin contract must not be bypassed or weakened: rotating-dock
omega is authored into the trajectory itself, then independently checked by
AcceptedManeuverProgramBuilder.


## 2026-09-26 — verify Assisted course capture and non-freezing Automatic failure diagnostics

Pull/build and run the mode + docking gates.

Acceptance:
1. Assisted manual flight: after a substantial yaw/pitch course change the
   velocity vector follows the new nose direction quickly; expected practical
   convergence is within roughly 2–3 seconds for the new 100 m/s regression.
2. `CALCULATE TRAJECTORY` must remain functional.
3. `START DOCKING` must never enter the old `phase=plan-retry` fixed-step
   loop.
4. If Automatic planning still fails, the game must remain responsive and emit
   exactly one useful `phase=plan-failed reason=...` followed by Human
   handback. That reason becomes the next docking fix target.
5. If Automatic planning succeeds, continue observing stabilize -> optional
   align/replan -> execute -> pre-capture completion.



## 2026-09-26 — live Automatic docking validation

Launch the freshly built game and test `START DOCKING` against a
compatible/free docking port.

Capture console output containing all `[DockAuto]` lines and report the visible
ship behavior.

Expected lifecycle:
- `phase=stabilizing`;
- either direct `phase=executing`, or `phase=aligning` followed by
  `phase=aligned-replan` and a fresh plan;
- no navigation shutdown on recoverable tracking loss;
- final current-stage success:
  `approach-complete ... reason=pre-capture-envelope-complete`;
- Human control restored.

Do not judge physical station latch yet; latch/contact is the next separate layer.



## 2026-09-26 — rerun corrected runtime-control gate

Pull current main and rerun the single corrected test first:

```bash
cd /d/__elite/work
git pull --ff-only origin main

cmake --build build/tests/navigation_runtime \
  --target navigation_runtime_control_tests \
  -j 8

ctest --test-dir build/tests/navigation_runtime \
  -R "^navigation_runtime_control$" \
  --output-on-failure
```

If it passes, run:

```bash
bash verify_docking.sh
```

The canonical game build already passed on the previous Windows run, so another
full build is only needed if the corrected native test exposes a real source
compile dependency.



## 2026-09-26 — isolate the one remaining runtime-control failure

The full game/server build now passes. Do not rebuild everything first.

Run only:

```bash
cd /d/__elite/work
ctest --test-dir build/tests/navigation_runtime \
  -R "^navigation_runtime_control$" \
  --output-on-failure
```

Use the first assertion/error from that output as the next fix target.

Do not claim Automatic docking accepted until:
1. `navigation_runtime_control` passes;
2. `verify_docking.sh` is fully green;
3. live `START DOCKING` demonstrates
   stabilize -> optional align/replan -> executing -> pre-capture handback.



## 2026-09-26 — compile/test the completed Automatic execution seam

Run the canonical target-machine gate from `D:\__elite\work`:

```bash
cd /d/__elite/work

git status --short
git pull --ff-only origin main
git log -1 --oneline

bash verify_modes.sh
bash verify_docking.sh
bash build_mingw64.sh
```

Do not run an old executable if any command above fails.

If all three pass:

```bash
build/EliteGame.exe
```

Automatic live acceptance:
- compatible/free port exposes `START DOCKING`;
- request produces `[DockAuto] ... phase=stabilizing`;
- if hull attitude differs materially, expect planned log with `phase=aligning`, then `phase=aligned-replan`, then a fresh plan;
- otherwise plan may enter `phase=executing` directly;
- planned log must include pages, trajectory duration, pre-capture depth, terminal omega and initial attitude error;
- accepted translation must use Planner-owned rear/fore/RCS schedule through `stepProgram`, with Follower correction separate;
- no direct position/velocity/orientation writes and no target-obstacle bypass;
- tracking/propulsion/frame failure returns to controlled stabilization/replan;
- successful current slice ends with `approach-complete ... reason=pre-capture-envelope-complete` and Human authority restored.

Send the first compiler/test failure exactly, or if green send the `[DockAuto]` live log.



## 2026-09-26 — Windows gate for Automatic docking + Planner/Follower execution

Pull current main and run the focused gates before changing behavior again:

```bash
cd /d/__elite/work
git status --short
git pull --ff-only origin main
git log -1 --oneline

bash verify_modes.sh
bash verify_docking.sh
bash build_mingw64.sh
```

Only if all three pass, launch:

```bash
build/EliteGame.exe
```

Live Automatic acceptance:
- `START DOCKING` is enabled for a compatible free/allowed port.
- after press, server takes Autopilot authority and ship visibly brakes/stabilizes;
- client does not continue local prediction after authoritative Autopilot ack;
- server logs `[DockAuto] begin`, then `[DockAuto] planned`;
- the planned log includes page count, trajectory duration and `terminal_omega_radps`;
- motion is produced through `AcceptedManeuverProgram -> TrajectoryFollower -> NavigationRuntimeControlBridge -> ShipControlState`, not through direct position/velocity writes;
- tracking/propulsion failure must return to controlled stabilization/replan, not disable navigation;
- final completion returns Human authority.

If compilation fails, fix the first real compiler error without restoring any retired `m_mode`, `CoordinateDisplayService::cycle()`, or `TrajectoryFollower(AcceptedShortSegment)` compatibility path.


## 2026-09-26 — rerun mode gate and canonical build after acceptance-harness repair

The canonical game build exposed one stale test harness call:
`ClientAcceptanceHarness.cpp` still used the removed
`CoordinateDisplayService::cycle()`.

That harness is corrected to exercise the current ownership path without
reintroducing hidden state:
`ClientModeState -> nextCoordinateDisplayFormat -> projection setFormat`.

Run from `D:\__elite\work`:
`git pull --ff-only origin main`
`bash verify_modes.sh`
`bash build_mingw64.sh`

If both pass, launch `build/EliteGame.exe` and perform the Assisted/Newtonian
live acceptance already defined below. Do not change production coordinate mode
ownership unless fresh evidence shows a real runtime failure.


## 2026-09-26 — rerun one canonical mode-state gate

Mode/state implementation is complete enough for a focused Windows gate. Run:

`git pull --ff-only origin main`
then:
`bash verify_modes.sh`

The previous 19.6133-vs-20 failure was a bad test expectation: 19.6133 m/s² is
the correct 2g load-bounded authority in the test fixture. The test now derives
the expected value from the production capability accessor.

`verify_modes.sh` now checks:
- native local-flight contract;
- native client-preferences contract;
- flight state ownership;
- global client mode ownership;
- subsystem mode-state ownership including system-map mode;
- localization boundary;
- wire schema consistency for the new replicated Assisted stabilizer state.

If this passes, run the canonical full game build and live-test Assisted versus
Newtonian. Then resume the Automatic docking executor milestone.

## 2026-09-26 — verify mode/state refactor, then return to Automatic docking

Immediate Windows gate:

`git pull --ff-only origin main`
`bash verify_modes.sh`

If green, rerun the independent docking gate:
`bash verify_docking.sh`

Then build/run the game:
`bash build_mingw64.sh`
`build/EliteGame.exe`

Live flight acceptance:
- fresh ship reports/behaves as Assisted by default;
- turning the hull in Assisted actively bends/cancels lateral VREL;
- released angular rotation damps in Assisted;
- Newtonian keeps side-slip and angular inertia without explicit alignment;
- Assisted automatic lateral stabilization does not consume manual RCS gas;
- Ctrl+F10 changes law through the state-machine path rather than a loose flag.

Client-mode acceptance:
- locale, constellation visibility, sky-culture and coordinate-display changes
  survive restart through ClientModeState/preferences;
- map Galaxy/System/Detail/Hub transitions still behave identically after
  replacing the renderer flag with MapModeState.

After these gates, resume the already-defined production Automatic docking
executor milestone. Do not continue broad mode refactoring unless a concrete
remaining shadow-state is found by tests or live behavior.

## 2026-09-26 — finish current route gate, then wire real Automatic docking

Do not spend another open-ended cycle only polishing manual guidance after the
current `verify_docking.sh` gate is green.

Next implementation sequence:
1. add a server-owned player navigation execution lifetime that retains
   Autopilot authority after preparation;
2. store/advance a proved `AcceptedManeuverProgram`;
3. each fixed step run
   `TrajectoryFollower -> NavigationRuntimeControlBridge -> ShipControlState`;
4. on completion restore Human ownership; on tracking/proof/capability
   invalidation request replan/controlled stop rather than killing navigation;
5. let `DockingRouteRequest::Mode::Automatic` enter this path;
6. only then enable `START DOCKING` in the map UI;
7. live-test approach, terminal alignment, stop/handoff and replan behavior.

The current manual docking verification remains the immediate gate because
Automatic must consume a valid route/program rather than hide planner defects.

## 2026-09-25 — rerun docking after shortened-axis endpoint clearance fix

Fresh native verification reached the planner and failed after successful soft
axis shortening: the join landed at about 8390 m, effectively on the inflated
obstacle boundary, and the visibility graph could not connect to it.

Current source backs the join away from that boundary by at least 50 m or four
hull radii. If geometric routing still cannot reach the shortened join, Planner
retreats it progressively toward the mandatory ingress and retries.

Run:
`git pull --ff-only origin main`
then:
`bash verify_docking.sh`

Expected result: the far-axis blocker case remains valid with
`terminalApproachShortened=true`; only a blocker in mandatory close-in ingress
may hard-fail.

## 2026-09-25 — use one canonical docking verification command

Do not manually mix the canonical game build tree and navigation-runtime test
tree.

From repository root:
`git pull --ff-only origin main`
then:
`bash verify_docking.sh`

Expected script stages:
- configure standalone navigation-runtime tests;
- build `docking_advisory_tests`;
- run native CTest `docking_advisory`;
- run static manual-docking checker.

A valid fresh static PASS includes revision
`20260925-soft-axis-v2`.

Only after this script passes, run `bash build_mingw64.sh` and live
`build/EliteGame.exe`.

## 2026-09-25 — rerun docking verification from the standalone runtime-test build tree

Configure the navigation-runtime test project explicitly; do not use the
canonical game `build/` tree for its test-only targets.

From repository root:
- configure `tests/navigation_runtime` into
  `build/tests/navigation_runtime` with Ninja;
- build target `docking_advisory_tests` there;
- run CTest `docking_advisory` from that same test build tree;
- run the corrected
  `python tests/architecture_contracts/check_manual_docking_advisory.py`.

Only after both native/static docking gates pass should the canonical game be
rebuilt and live SHOW ROUTE retested. Preserve the final-axis shortening
diagnostics and Assisted/default fast-stop checks.

## 2026-09-25 — verify preferred final-axis shortening restores live route calculation

Pull current main and run the focused docking gate.

Expected behavior:
- the previous `failed=dock alignment blocked` must disappear for obstacles
  that intersect only the far preferred 9 km final-axis lead;
- accepted route log must show either
  `final_axis_shortened=0 final_axis_m=9000` or
  `final_axis_shortened=1 final_axis_m=<largest clear prefix>`;
- only an obstacle inside the near-port mandatory ingress may produce
  `failed=dock mandatory ingress blocked`;
- reroute-before-tighten remains active after shortening, so a shortened axis
  may still produce detour geometry and/or `radius_relaxed=1` rather than
  cancelling navigation.

Run docking_advisory native/static tests first, then canonical build and live
SHOW ROUTE. Also retain the previously requested Assisted-default / fast-stop
checks in the same live run if compilation is clean.

## 2026-09-25 — target verification of broad Assisted route and hard stop

Pull current public main and verify the newly implemented slice before changing
Automatic-docking ownership.

Focused gates:
1. rebuild/run `local_flight_control_contract_tests`, then CTest
   `local_flight_control_contracts`;
2. run `python tests/architecture_contracts/check_local_flight_control.py`;
3. rebuild/run `docking_advisory_tests`, then CTest `docking_advisory`;
4. run `python tests/architecture_contracts/check_manual_docking_advisory.py`;
5. build the canonical game.

Live acceptance:
- fresh ship reports Assisted without requiring Ctrl+F10;
- DockPrep begin prints `law=ASSISTED` and non-zero
  `reverse_main_mps2` for a healthy Cobra;
- VREL drops quickly to the settle threshold instead of spending a long time in
  a Newtonian turn/brake sequence;
- SHOW ROUTE reports preferred radius 6000 m unless geometry forces a
  `radius_relaxed=1` fallback;
- the visible station bend is broad enough to fly by eye;
- crossing the nominal tunnel raises warnings, but the route survives recoverable
  excursions up to the new 120 m transit release envelope / 1 s grace.

If live Assisted still brakes slowly while the log proves healthy reverse-main
authority, capture VREL plus engine-acceleration evidence next; do not weaken
physics or fake velocity assignment.

After this gate, continue the server-owned Automatic docking executor through
AcceptedManeuverProgram -> TrajectoryFollower -> NavigationRuntimeControlBridge.

## 2026-09-25 — widen manual curve, relax tunnel release, make Assisted default, verify hard Assisted stop

Implement and then verify four coupled changes:
1. Increase the manual Assisted final-axis lead and preferred terminal radius
   enough that the map trajectory is visibly broad rather than a compact corner.
2. Preserve reroute-before-tighten semantics: a blocked broad arc searches other
   ingress geometry and may go around the whole station before radius is reduced.
3. Increase manual corridor release tolerance/grace substantially while keeping
   nominal warning/critical presentation unchanged.
4. Make Assisted the default local flight law for newly initialized motion and
   default control requests.

For DockPrep, verify the existing Assisted stop path behaves as an immediate
zero-VREL command: target speed becomes zero at once, healthy reverse/fore main
is used without hull rotation, and deceleration is limited by installed
propulsion rather than by the ordinary 5/s throttle response.

Required target-machine gates after code lands:
- local-flight native contract + static local-flight architecture check;
- docking_advisory native test + manual-docking architecture check;
- canonical game build;
- live log must show DockPrep law=Assisted and a rapid VREL collapse before
  phase=settled.

## 2026-09-25 — verify reroute-before-tighten, then build the Automatic executor seam

First pull current main and prove the corrected manual-planning contract on the
Windows target:
- rebuild/run the native `docking_advisory` test;
- run `python tests/architecture_contracts/check_manual_docking_advisory.py`;
- if both pass, rebuild the canonical game;
- run Assisted SHOW ROUTE and inspect the new
  `route=nominal|detour terminal_radius_m=... radius_relaxed=...` diagnostic;
- specifically verify that an arc obstruction can select another pre-alignment
  direction around the final axis rather than merely shrinking the same turn;
- verify the task no longer disappears merely because the preferred 1500 m arc
  is obstructed or cannot fit; the route may become much longer, including a
  route around station geometry.

Keep the semantic final docking-axis segment strict: if the port ingress itself
is occupied, that is a real unavailable-dock result rather than a reason to
invent a different final approach direction.

After this focused gate, continue Automatic docking at the real ownership
boundary. Required next implementation slice:
- introduce a server-owned player docking execution lifetime that can retain
  Autopilot authority after preparation;
- its executable input must be an AcceptedManeuverProgram (or the completed
  proved-program coordinator output), never DockingAdvisoryGate display frames;
- execute through TrajectoryFollower -> NavigationRuntimeControlBridge ->
  ShipControlState -> shared physics;
- cancellation, invalidated proof/capability, or tracking-envelope failure must
  request replan/stop without disabling the navigation task globally;
- only enable the START DOCKING UI when that server execution path is actually
  present.

Do not bolt the existing transitional NPC immediate-intent controller onto the
player docking button as a shortcut.

## 2026-09-25 — rerun docking advisory after explicit transition-frame anchoring

Pull current main and rerun only `docking_advisory` plus the static manual
docking check. The prior exact 500 m failure came from a sparse frame jumping
across the 2250 m activation boundary. Planner now shortens the crossing step
itself and places an anchor at/just before that boundary.

If the gate passes, rebuild EliteGame and proceed with live inspection of the
circular station turn, final 250 m frames, and DockPrep begin/settled VREL logs.

## 2026-09-25 — rerun docking advisory after 2 km transition anchoring fix

Pull current main and rerun only `docking_advisory` plus
`check_manual_docking_advisory.py`. The failed 490 m interval was the sparse
chord crossing into the final 2 km; terminal density now starts one 250 m step
early so the boundary is already inside the dense cadence.

If both pass, rebuild EliteGame and inspect the circular final turn, final-2-km
frame density, and new DockPrep begin/settled VREL diagnostics.

## 2026-09-25 — verify circular turns, final 250 m frames, and authoritative zero-speed stop

Pull current main and run the focused docking advisory native/static tests,
then rebuild EliteGame. In the live SHOW ROUTE run verify:
- the station/final turn is visibly circular rather than a coarse polyline;
- published frames are approximately 250 m apart inside the final 2 km and
  remain 500 m in open transit;
- console contains DockPrep begin with initial `vrel_mps` and
  DockAdvisory `phase=settled vrel_mps=...`;
- the settled number is <=0.05 m/s (or the ship's larger configured stop
  epsilon) and omega <=0.01 rad/s before planning;
- Human control is handed back after route publication.

Do not enable full DOCKING by routing the advisory gates through an ad-hoc
waypoint controller. Automatic mode must consume the accepted physical maneuver
program / TrajectoryFollower architecture rather than create a second control
system.

## 2026-09-25 — pull Windows-safe docking header and rebuild EliteGame

Pull current `main`, rerun `check_manual_docking_advisory.py`, then rebuild
with `bash build_mingw64.sh`. No navigation math or propulsion change is
required for the reported compiler failure.

If `EliteGame.exe` links successfully, proceed to live SHOW ROUTE acceptance:
autopilot takeover, physical stabilization, route/corridor publication, and
authoritative hand-back to Human. Preserve all untracked trace/log artifacts.

## 2026-09-25 — verify final Newtonian fore-main runtime completion, then build game

Pull current `main`. Rebuild and run `local_flight_control_contracts`, which
now includes the aft-main-failed Newtonian runtime case. Also rerun
`check_local_flight_control.py` to lock the implementation structurally.

If both pass, the dual-main propulsion slice is ready for canonical
`bash build_mingw64.sh` and live SHOW ROUTE acceptance. Existing Planner,
propulsion-state, and docking-advisory runtime tests already passed on the
target machine and need not be repeated unless the game build exposes a shared
compile issue.

## 2026-09-25 — rebuild local-flight test after header fix

Pull current `main`, rebuild only `local_flight_control_contract_tests`, and
run only `local_flight_control_contracts`. Do not treat the previous CTest
failure as current evidence: that invocation ran the old executable after Ninja
failed to compile the new source.

If the rebuilt contract passes, continue directly to the pending
`ship_propulsion_state`, `ordinary_physical_maneuver_compiler`, and
`docking_advisory` runtime tests. The two Python architecture checks already
passed in the latest Windows run.

## 2026-09-24 — rerun only the two corrected focused gates first

Pull current `main`. Rebuild and rerun
`local_flight_control_contracts`, then run
`check_manual_docking_advisory.py`. The first must prove that Assisted keeps
the selected main-engine acceleration intact while reducing only RCS as needed
to fit the combined linear acceleration envelope. The second must pass while
retaining the semantic dock-bottom marker and the <=500 m speed-label rule.

If both pass, continue with the already requested propulsion/compiler tests:
`ship_propulsion_state`, `ordinary_physical_maneuver_compiler`, and
`docking_advisory`, then build the canonical game. Preserve all untracked
trace artifacts.

## 2026-09-24 — verify dual-main propulsion and failed-engine trajectory fallback

Pull current `main` and first run the focused propulsion/trajectory/control
regressions. The required new gates are `ship_propulsion_state`,
`ordinary_physical_maneuver_compiler`, and the local-flight-control contract.
Then run the existing docking-advisory/manual-docking checks and build the
canonical game.

Acceptance semantics:
- healthy Cobra Assisted brakes with the physical fore main without a 180 deg
  flip;
- fore-bank failure removes reverse main completely, leaves RCS intact, and
  strong braking compiles/executes flip + aft-main burn;
- aft-bank failure removes forward main completely and promotes the surviving
  fore main as primary propulsion with reversed working hull direction;
- an operational bank always retains full descriptor thrust; no proportional
  health-based derating is allowed;
- Planner/B5 must distinguish main-bank authority from residual RCS authority.

After those gates pass, repeat SHOW ROUTE from non-zero Hub-relative speed in
Newtonian and Assisted and continue the current corridor-warning acceptance:
fresh request serial each press, physical stabilization, planning/publication,
authoritative Human hand-back, recoverable nominal-bound excursions and
cancellation only after sustained departure beyond the release envelope.

Do not weaken geometry or propulsion truth to make a test pass. The new fore
engine modules currently have runtime/damage identities but no invented mesh
hit-volume; do not bind them to unrelated Cobra mesh parts merely to create a
damage target.
## 2026-09-24 — immediate gate: install and prove the published fix

Do not modify docking geometry again from the bare legacy failure. On the
Windows target, first fast-forward from `84d59f4d`, then run the focused
numeric-locale and docking tests, rebuild the canonical executable, and verify
the executable contains `[DockAdvisory] axis request=`. Run SHOW ROUTE with
combined stdout/stderr captured.

Acceptance evidence:
- startup reports `[Startup] LC_NUMERIC=C`;
- preparation reaches planning/publication and then authoritative Human
  hand-back;
- the route remains visible for at least 45 seconds while valid;
- any cancellation includes the new numeric `[DockAdvisory] axis` or
  `[DockAdvisory] left` record.

If the marker is absent after the build, stop navigation diagnosis and repair
the checkout/build/run path. If the marker is present and the route still
vanishes, diagnose the measured local delta/tick/frame rather than widening
the 2 m guard.

## 2026-09-24 — pull published code and run Windows gates

Public `origin/main` contains the exact corrected local tree at `512b917c`.
On `D:\\__elite\\work`, run `git pull --ff-only origin main`, verify source
and executable markers, run focused `docking_advisory` and
`json_numeric_locale` CTests, build with `bash build_mingw64.sh`, and run the
canonical `build/EliteGame.exe` with stdout/stderr piped through `tee` into a
new `build/test-logs/docking-live-fixed.log`. Confirm startup locale, route
persistence and acknowledged Human hand-back; if the guard fires, use the
numeric `[DockAdvisory] axis` line. Preserve untracked trace artifacts.


## 2026-09-24 — public main publication approval gate

User wants fixes in GitHub and no patch files. Automatic approval review
rejected the exact `git push origin main` attempt as consequential public
default-branch publication without sufficiently explicit approval. The next
required input is express authorization to publish these local docking and
JSON-locale commits to `forzub/Elite` public `origin/main`. After authorization,
verify remote HEAD and provide only `git pull`, focused tests, canonical build
and log-capturing run commands. Do not bypass with another push route.


## 2026-09-24 — install confirmed-missing patch on target checkout

The target is at `84d59f4d` and its binary lacks the fixed axis guard. Apply
the complete exported patch with `git am` (do not remove the user's untracked
trace files), run the focused JSON-locale and docking tests, build using
`bash build_mingw64.sh`, and verify the rebuilt binary contains
`[DockAdvisory] axis request=`. Capture combined console/file output, then
check route persistence and server-acknowledged Human hand-back.


## 2026-09-24 — inspect target checkout, binary and complete game log

On Windows print `git log -1 --oneline`, inspect `src/game/SpaceState.cpp` for
`[DockAdvisory] axis request=`, check the built `build/EliteGame.exe` for that
same literal, and search the complete combined `docking-live.log` for
`[Startup]`, planning, axis and failure lines. If the binary lacks the literal,
apply the corrected patch and rebuild in that checkout. If the binary has it
and the full log includes numeric axis error, diagnose that measurement.


## 2026-09-24 — rerun after numeric JSON locale correction

Install the refreshed local patch on the Windows checkout. Run the focused
`json_numeric_locale` and `docking_advisory` CTest targets, build the canonical
game, and capture combined output with `tee`. The new `[Startup] LC_NUMERIC=C`
line proves the corrected startup path ran. First verify startup no longer
asserts in the nlohmann numeric lexer; then proceed to SHOW ROUTE and capture
the docking axis/hand-back lifecycle. This crash is a separate gate from the
Hub-local navigation fix. Do not claim Windows runtime acceptance from the
local native test.


## 2026-09-24 — deliver target verification while remote push is blocked

Automatic approval review rejected publishing the four local docking commits
to public `origin/main`. Provide the refreshed `git am` patch for target
installation, exact focused tests and canonical build/run commands. Record
stdout/stderr both on screen and in `build/test-logs/docking-live.log` using
`tee`. Remote push requires explicit approval identifying `origin/main`.


## 2026-09-24 — prove the executable contains the local fix

Repeated `failed=dock moved off approach axis` without an `[DockAdvisory]
axis` line matches the old remote source. First confirm the target checkout
contains the corrected commits and the launched `build/EliteGame.exe` was
rebuilt from them. Capture unfiltered stdout and stderr together, including
planning and axis lines. If the corrected binary produces an axis measurement,
diagnose its numeric local delta/tick/Hub ID; if it does not, repair the
deployment/build path. Do not infer new dock physics from an excerpt lacking
the executable identity.


## 2026-09-24 — verify single-epoch local docking route on Windows

Apply the local change set on the target checkout, run architecture and focused
docking tests, build the client and start the game. SHOW ROUTE must stop the
ship, plan from one authoritative tick, retain fixed Hub-local gates for over
45 s, and acknowledge Human control after route publication. Collect
`phase=stabilizing`, `phase=planning` (source tick/time),
`phase=handoff_wait` (validated tick/render time),
`[DockPrep] published ... human_restored=1`, and
`phase=manual human_control=1`. If cancelled, collect the complete
`[DockAdvisory] axis` or `left` measurement. The code is locally checked but
full-game compilation and live acceptance are open. Prior text asking to pull
remote `main` alone is obsolete: these edits have not been published there.
The full architecture runner is blocked in this environment by missing build
tools; its separate Python portion passes 82/91, with nine unrelated failures
requiring target-side triage. Record these separately from the docking gate.

## 2026-09-24 — Windows gate after complete docking frame audit

In addition to the port/Hub orbital correction, local code now removes two
current-versus-render epoch mixes: corridor tracking uses ship Hub-local
position, and cockpit gates are projected through the player's actual render
Hub frame. Focused tests pass locally, including mixed-epoch displacement and
map-frame agreement. Build the updated `docking_advisory_tests` and game on
Windows. Verify route persistence while stationary and moving relative to the
Hub, no orbital drift in cockpit/map for 45+ s, and confirmed Human hand-back.
If the route clears, capture `[DockAdvisory] axis` or `[DockAdvisory] left` plus
the complete preparation phase sequence. Full target acceptance remains open.


## 2026-09-24 — verify canonical dock-axis prediction on Windows

The split prediction is corrected in code and its orbital/axial regression
passes locally. On the Windows target: pull current `main`; run the
architecture gate and `docking_advisory_tests`; rebuild the game and press
SHOW ROUTE while moving in the isolated Hub fixture. Verify physical stop,
route persistence (including beyond 45 s), and the log sequence
`phase=stabilizing -> phase=planning -> phase=handoff_wait ->
[DockPrep] published ... human_restored=1 -> phase=manual human_control=1`.
Check Human controls after that confirmation, then card-close and post-entry
corridor reset. If `dock moved off approach axis` recurs, provide its new
`[DockAdvisory] axis` numeric line. This gate is not yet passed by an in-game
run; DOCKING remains disabled.


## 2026-09-24 — diagnose/fix dock-axis prediction split after successful publication

Current live blocker: route is successfully calculated and published, then
`dock moved off approach axis` immediately clears it.

Next:
1. log the final-gate world position, predicted dock standoff point, delta,
   dock position/forward, source/current universe times and Hub frame identity;
2. confirm the diagnostic spinning dock is being predicted through two
   different kinematic paths;
3. replace the split prediction with one canonical Hub-attached module/anchor
   prediction source;
4. keep the fixed 500 m advisory geometry and do not relax the 2 m threshold
   merely to hide model drift;
5. rerun and capture the complete hand-back sequence
   `handoff_wait -> [DockPrep] published human_restored=1 ->
   phase=manual human_control=1`.


## 2026-09-24 — use corrected meta-check HEAD

Do not use `91f24d13726cd2192d7b240277aa5a659622caf3` as target evidence:
the navigation cleanup is valid, but its new test-source meta-check has a
false-positive extension parser.

Pull the next corrected HEAD and rerun the architecture suite from the start.


## 2026-09-24 — rerun cleaned architecture gate, then docking-focused native gates

The next target action is verification from the cleanup HEAD.

First run `tests/architecture_contracts/run_mingw64.sh`. It must now pass the
local-flight angular policy boundary and the new test-suite source-integrity
check without touching production physics.

Then build/run focused current navigation targets:
- `geometric_path_planner`;
- `docking_advisory`;
- wire protocol/data-plane authority tests.

Do not run or restore `tests/navigation_guidance`; that directory represented
the deleted DockingPathPlanner/GuidanceTunnel architecture and has been retired.

If a new architecture failure appears, treat it as a candidate stale contract
first: compare the check against current ownership/API before changing runtime
code.


## 2026-09-24 — rerun architecture gate after stale local-flight check repair

The previous target run did not reach docking-specific compilation. It stopped
in a stale grep-style local-flight architecture check.

Rerun the same chained gate from the corrected HEAD. Expected next evidence:
`Local-flight-control architecture check passed.`

Do not modify DynamicMotionSystem physics to satisfy the old raw-field tokens;
the centralized ShipDynamics ownership is the intended architecture.


## 2026-09-24 — rerun from corrected authority-copy HEAD only

Do not test `61b5424608533c806672e21e77339be612dc0087`; it contains the
statically detected duplicate declaration in one snapshot-copy function.

Use the next corrected HEAD and first run the architecture suite / compile gate.
Only after those pass proceed to the in-game SHOW ROUTE lifecycle test.


## 2026-09-24 — verify acknowledged docking hand-back

Before gameplay inspection verify the current focused gates:
- `control_registry_contracts`;
- `wire_protocol_contracts`;
- `wire_data_plane_contracts` with snapshot schema 9;
- `docking_advisory`;
- `check_manual_docking_advisory.py`.

In game, start with non-zero linear and angular motion, press SHOW ROUTE and
confirm the exact log order:
`phase=stabilizing`, `[DockPrep] begin`, `phase=planning`,
`phase=handoff_wait`, `[DockPrep] published ... human_restored=1`,
`phase=manual human_control=1`.

Do not accept the slice if Human input becomes effective before the final
authoritative hand-back confirmation.


## 2026-09-24 — verify manual docking commissioning in game

Implementation is present. Next acceptance is target evidence, not more redesign:

1. run the focused architecture, control-authority and docking-advisory native tests;
2. rebuild the canonical Windows game;
3. enter the isolated Hub docking fixture and accelerate/rotate the Cobra;
4. press SHOW ROUTE;
5. verify the ship physically turns/brakes to rest relative to the Hub and
   visibly damps pitch/yaw/roll rather than snapping;
6. verify route calculation starts only after the stable authoritative state;
7. verify Hub Map line and fixed cockpit gates appear, with nominal 500 m
   spacing and speed in each gate's upper-left;
8. verify localized blinking MANUAL DOCKING MODE appears;
9. verify manual controls work again only after publication;
10. verify dock-card close cancels guidance and a post-entry tunnel departure
    cancels guidance.

Capture `[DockPrep]` and `[DockAdvisory]` lines if any stage fails. Automatic
DOCKING remains disabled.


## 2026-09-24 — implement playable manual docking commissioning slice

Status: **ACTIVE — AUTOPILOT PREP -> 500 M STATIC GUIDANCE -> HUMAN HAND-BACK**

Implement one vertical in-game path for the dock-card SHOW ROUTE action:

1. route the request through an authoritative docking-preparation command;
2. temporarily switch the controlled ship from Human to Autopilot authority
   without losing the player-to-ship identity binding;
3. physically brake to zero velocity relative to the Hub co-moving frame and
   settle angular rate using normal bounded controls/physics;
4. any material human flight input during preparation cancels preparation and
   restores Human authority;
5. after bounded settle, capture one fresh authoritative rigid-body start state;
6. calculate the existing static docking advisory from that captured state;
7. publish the route to Hub Map and fixed HUD tunnel gates;
8. use 500 m nominal gate spacing and always retain the terminal gate;
9. render each recommended speed at the projected upper-left of its gate;
10. show a blinking localized MANUAL DOCKING MODE cockpit status through the
    unified localization catalog;
11. release Autopilot authority only after the map route and HUD guidance are
    published;
12. after entry, leaving the valid tunnel cross-section resets the advisory;
13. closing the selected dock card resets the advisory at any stage and restores
    Human authority if preparation still owns control.

Do not enable automatic DOCKING in this slice. Do not reuse the removed rolling
`GuidanceTunnelBuilder` or `DockingPathPlanner`. Do not plan from a moving
pre-takeover snapshot. Do not make gate presentation time-rolling.

Replace the stale manual/live docking architecture checks that still require the
deleted rolling tunnel with checks for `DockingAdvisoryPlanner`, the
authoritative preparation lifecycle, static 500 m gates, localization ownership,
card-close reset and post-entry corridor-exit reset.

Target acceptance is an actual Windows game run, not only native tests:
press SHOW ROUTE while moving; observe bounded physical stabilization; observe
the route line in Hub Map and the fixed 500 m cockpit tunnel with speed labels;
observe the localized blinking manual-mode status; confirm Human control is
returned after publication; then separately confirm card-close and corridor-exit
reset behavior.


## 2026-09-24 — prepare start before requesting docking geometry

Contract updated after the user's corridor/start clarification. Next code
slice: server-owned, physically bounded stabilization for SHOW ROUTE (stop in
the Hub co-moving frame), explicit control takeover and cancellation on pilot
input, capture a fresh authoritative state after bounded speed/angular-rate
settling, then run the existing async static planner from that captured state.
DOCKING must use the same start-state capture, but remains unavailable until
the full proved program/dispatch/ingress path exists. Replace the provisional
60 m/700 m corridor exit rule with a center-region envelope derived from
swept-hull static clearance and an ordered entry/progress/exit state. Do not
treat an estimated point on a moving ship's path as an already entered gate.

## 2026-09-24 — retry Hub docking map after failed live gate

Target result: six SHOW ROUTE attempts reported `ship left guidance corridor`
before a line could appear. The implementation now keeps guidance visible
until the ship enters the first gate, uses a broad transit corridor and
reduces it toward the real opening in the last 700 m. Verify on Windows:
build and run `docking_advisory`, rebuild the canonical game, request the far
dock route, confirm the map line appears and persists before entry, then check
that closing the card and leaving an entered tunnel cancel it. If it still
fails, capture `[DockAdvisory] left gate=...` measurements and the failure
reason. Automatic docking remains disabled.

## 2026-09-24 — next docking flight gate

Local native far-dock geometry and 45 s yawed dock-spin checks pass. The
spinning-module angular velocity is now derived from its authored Euler pose
in both server simulation and prediction. The isolated diagnostic scene skips
the unrelated Stage-12 runtime NPC. An unrelated compilation blocker in NPC
stepping is corrected by reading the profile-owned step limit.

Next target-machine gate: build `docking_advisory_tests` and the game, run the
test, open the isolated Hub fixture and select the far dock's SHOW ROUTE in the
map. Inspect the static map trajectory and cockpit gates/speeds; confirm that
the spinning port does not cancel its approach axis, closing the dock card or
leaving the corridor cancels the advisory. The automatic DOCKING action is still
disabled until a proved server-owned execution program, reservation, spin
alignment, ingress and capture have been implemented and tested in game.

## 2026-09-24 — docking rewrite, advisory first

The diagnostic `HubDockingFlightTestScene` now leaves the station, Cobra and
both docks while removing the stress objects. The far dock is yawed 27 degrees
and spins at 2 degrees/s around its entrance axis. This is a fixture for a
future in-game acceptance test, not evidence of successful docking.

The old disabled client route/tunnel chain is removed. The new advisory mode
builds a fixed stop-before-port corridor with speed labels and cancellation on
leaving the corridor or closing the dock card. Its native static test passes; full client build and live Hub fixture remain
unverified because this environment lacks `websocketpp`. DOCKING stays disabled until a server
flight program with verified actuator/hull feasibility, reservation, spin
alignment, ingress and capture can safely execute without changing the ship's
fixed control law. No dynamic obstacle avoidance belongs in static A*.


Date: 2026-09-24

## 2026-09-24 — static route commission, immutable execution program

Scope revised by user: A* plans a static geometric route for one craft or a
group modeled by a single envelope; scheduled traffic/dispatcher and all
dynamic behavior are outside this component. After separate physical authoring
and full proof, execution either follows its accepted program or cancels it.
New planning following cancellation is a separate scheduled request. Docking,
salvage and formation capture require a terminal state, not merely a point.

Implemented here: nominal route carries vehicle capability revision and rejects
stale provenance; capped A* cannot publish a route blocked by an omitted static
obstacle. No async runner or physical program acceptance is claimed.

Next: design and implement a nonblocking commissioning job with revision checks,
full-program physical authoring and continuous static hull proof; only then
allow the resulting accepted program to be executed. The preceding observer
point-chain direction below is superseded by this static/offline boundary.

## 2026-09-24 — input contract corrected after 500-ship review

The original batch has distinct goals but physically overlapping start/goal
positions; retain it solely as 500 independent planning-load queries. The
new parallel-lane empty-space control has 500 unique, 30 m separated starts
and goals. It still gives 0/500 acceptable final speeds, despite 500/500
position captures and 500/500 correct body orientations. Diagnose this as
missing backward terminal-speed authoring in the observer chain, not a traffic
dispatcher problem or an intrinsic A* property.

Before comparing planners, distinguish independent request throughput,
simultaneous multi-actor traffic and corridor execution. The latter two have
not been benchmarked; no global architecture winner is established.

## 2026-09-24 — point-chain measurement completed; authoring correction next

The optional 500-ship diagnostic now chains B5 terminal sample states across
each geometric waypoint under bounded speed/horizon attempts. Required final
position, speed and hull attitude: open 0/500, dogleg 0/500, barrels 0/500.
Empty-space failure isolates missing terminal-state authoring; clutter also
exposes waypoint chasing and sampled collision. Initial 500/500 candidate
availability was a misleading local-only signal.

Next implementation: replace exact A* support-point capture with a corridor
and portal-region contract; propagate admissible terminal position, velocity,
attitude and angular rate backward; author forward maneuvers that preserve
that contract. Compare total planner cost only after full-route candidates
pass actuator and continuous hull proof. The greedy benchmark is not a
production algorithm to tune until it happens to succeed.

## 2026-09-23 — navigation scaling experiment baseline

Correction: use `benchmarks/navigation_route_stress` with the 13 m Cobra
radius, not the superseded 5 m result below. The final 100-barrel throughput
run took 17.075 s for 500 geometric routes and has no turns over 45 degrees.
The added four-wall dogleg generated 1,000 turns over 45 degrees yet all 500
initial B5 probes still returned unproved candidates. The next gate must
propagate state through **every** dogleg corner and report full-route proof or
typed failure. Initial-candidate counts cannot serve as route success counts.

The deterministic 100-barrel / 500-ship route benchmark is implemented and
run locally. At the current cap of 32 considered obstacles, geometric search
alone takes 15.33 s for all 500 sequential requests (p95 57.75 ms per ship).
All returned paths clear the full 100-barrel fixture, but this does not prove
physical executability. A single all-100-obstacle query took 419.4 ms.

Next benchmark gate: run the alternative physical/corridor planner on the same
500 start/goal/obstacle inputs and record total route+physics time, success and
rejection counts, actual simulated arrival quality and continuous collision
proof. Only then compare designs; do not infer that post-checking is cheaper.

## 2026-09-23 — spatial eligibility slice implemented locally

The B5 compiler now consumes the target point and an explicit capture radius.
It rejects primitives that never approach the capture region or cross its
perpendicular plane outside the region, returning
`SpatialTargetNotApproached` with distance evidence. The bounded coordinator
can retry a different terminal at unchanged desired velocity. Focused native
compiler and coordinator tests pass; MinGW target and viewer gates remain open.

Next: model a typed corridor cross-section and terminal capture condition,
carry the *exact* terminal position/velocity/attitude/angular velocity into the
next local solve, then show each leg and rejection in the observer. A mere
distance decrease is not proof of capture, geometry, actuator execution or
route completion. Keep B5 observer-only until continuous swept-hull proof.

Status: **VISUAL GATE FAILED USEFULLY — INITIAL-ONLY PHYSICAL PROBE MUST BECOME A CHAIN**

## Target visual result

Commit `15f4c6c6f856cc9cf7974ef1527730315807c880` built and displayed
the observer layer. At 20.60 m/s the legacy route reaches the first corner with
50 infeasible actuator intervals and loses tracking at 5.33 s. The observer's
initial rotate/burn candidate is visible and remains explicitly unaccepted.

This proves the viewer seam works and proves that one successful initial B5
primitive says nothing about a later corner.

## Active implementation task

Replace the initial-only observer request with a pure spatial local-maneuver
task and a bounded receding-horizon chain:

- target is a typed capture region/corridor cross-section, not metadata;
- candidate scoring/compilation must account for position and velocity boundary
  conditions together;
- the next solve starts from the exact terminal position, velocity, attitude
  and angular velocity of the previous candidate;
- a corridor leg advances only after its capture condition is reached;
- corner failure returns a typed witness and varies speed, arrival time or
  terminal sample without cancelling the objective;
- no candidate enters Follower before continuous swept-hull proof.

First regression: when a changed spatial target changes progress/capture
constraints while desired velocity is preserved, candidate eligibility or
outcome must change or return a typed rejection. Identical short primitives may
remain legal for targets on the same unconstrained ray; silently ignoring a
capture plane, corner or terminal volume is not legal.

## Implemented observer candidate

The runtime now builds one observer-only initial receding-horizon frontier from
explicit API inputs. Four ranked alternatives vary arrival/program horizon;
the coordinator exposes attempts, witnesses and any unproved physical candidate
samples through `TracePhysicalSearch`.

The trace JSON and viewer preserve four separate meanings:

- legacy retained route/Ruckig reference;
- rejected physical alternatives;
- unproved B5 physical candidate set with attitude and acceleration vectors;
- accepted reference/actual motion from the still-legacy execution chain.

There is deliberately no B5-to-B8 conversion and no physical observer input to
Follower or physics.

## Active gate

Build `navigation_runtime_pipeline_tests` and `navigation_runtime_viewer` on the
MinGW target, run the pipeline and open the viewer. Inspect at least straight,
corner and high-speed Newtonian starts. Confirm red rejection markers, separate
cyan/yellow candidate curves/vectors, and the window-title marker
`PHYS-OBS=... (НЕ ПРИНЯТО)`.

The legacy high-speed terminal failure remains expected until the replacement
path gains continuous proof and execution authority. Do not weaken that test.

## Accepted coordinator evidence

Exact commit `b506397ca30f223ee6cb29597c8673e0815626d1` passed
`physical_maneuver_search_coordinator` on MinGW64 (1/1, 0.04 s). Combined with
the earlier exact `9af337c` physical compiler/limit pass, the pure compiler and
bounded-search contracts are accepted.

This is not acceptance of the integrated navigation system.

## Implemented visual slice contract

The new physical search is connected to `tools/navigation_runtime` in
observer-only mode. It does not steer the ship or publish
`AcceptedManeuverProgram`.

Required viewer layers:

- legacy coarse route and legacy Ruckig reference, clearly labeled;
- ranked physical alternatives and their provenance;
- rejected alternatives with typed witness/reason;
- selected unproved physical candidate samples;
- candidate attitude axis and planned acceleration/thrust phase;
- later, proved swept tunnel, accepted reference and actual path as distinct
  products.

The observer adapter owns conversion from scenario/vehicle/runtime values into
the pure coordinator request. The coordinator and compiler remain free of
viewer, JSON, filesystem and OpenGL dependencies. All behavior-affecting
observer inputs must come through explicit policy/API values.

Exit gate:

- viewer builds and displays the new path independently of legacy execution;
- an unproved candidate cannot be mistaken for a proved or accepted program;
- visual inspection is performed on straight, corner and high-speed Newtonian
  cases before the replacement is allowed to control physics.

## Historical first target result

Exact commit `3492ca3ba314dcf250c5d3ebc03c6e8cc0c3dce6` configured and
compiled all three targets. The existing two tests passed; the new coordinator
test failed because its allegedly feasible second alternative used a 4.0 s
horizon for a 135-degree rotation whose computed attitude-acquisition lower
bound is about 4.60 s.

Correction: use a 6.0 s second horizon so the test actually contains a physical
burn window. The coordinator correctly rejected the original pair; no planner
logic, timeout or authority limit is relaxed.

## Accepted prerequisite

Exact MinGW64 commit
`9af337c2e23a32d5f11d34a3e048ecd98842674d` passed:

- `maneuver_chained_limit_matrix`;
- `ordinary_physical_maneuver_compiler`.

Result: 2/2 tests passed in 0.12 s. The typed physical infeasibility witness is
accepted. The aggregate high-speed runtime remains intentionally unresolved
until the replacement path reaches accepted-program publication.

## Implemented slice

`PhysicalManeuverSearchCoordinator` is a pure bounded search layer above
`OrdinaryPhysicalManeuverCompiler`.

Input ownership:

- mission/goal/route code owns the ranked alternative frontier;
- every alternative carries corridor, terminal, speed-schedule and
  arrival-time provenance IDs;
- the common query owns measured state, vehicle capability, control law,
  reserves and compiler policy;
- an alternative may vary only target position, desired velocity and local
  program horizon;
- an explicit policy owns the maximum attempts per worker slice;
- independent objective/frontier revisions and a cursor preserve progress
  between slices without turning a rebuilt frontier into a new mission goal.

Output states:

- `CandidateFound` — one alternative produced unproved physical candidates;
- `SearchPending` — budget ended and the cursor can resume later;
- `FrontierExhausted` — caller must produce another frontier or proved safe
  fallback, while the objective remains active;
- `SharedStateBlocked` — changing terminal alternatives cannot repair the
  shared input/law/state problem;
- `InvalidInput` — revision/frontier/policy API data is invalid.

No state accepts a maneuver, changes ship capability or disables navigation.

## Accepted focused evidence

Build and run:

```bash
cmake --build build/tests/navigation_runtime \
  --target physical_maneuver_search_coordinator_tests \
           ordinary_physical_maneuver_compiler_tests \
           maneuver_chained_limit_matrix_tests && \
ctest --test-dir build/tests/navigation_runtime \
  -R "^(physical_maneuver_search_coordinator|ordinary_physical_maneuver_compiler|maneuver_chained_limit_matrix)$" \
  --output-on-failure
```

Required behavior:

- a short-horizon rejection advances to a later feasible alternative;
- a one-attempt budget returns `SearchPending` and resume does not repeat work;
- exhausted alternatives preserve witnesses and objective ownership;
- shared control-law/state blockers do not waste the remaining frontier;
- stale objective or frontier revisions fail before physical compilation.

## Work after visual acceptance

Add literal actuator phases and consistent rigid-body propagation, including
non-zero initial angular velocity. Then introduce continuous proof over the
exact candidate. Only after both layers may the replacement publish an
`AcceptedManeuverProgram` and displace the legacy translation-first author.

## 2026-09-27 — active gate: shared manual/automatic flight mechanics

Immediate target task is no longer to tune the 2 m/s² RCS allocator.

Run the complete docking gate on Windows/MSYS2. It now includes the local-flight
architecture contract.

Acceptance requirements:

1. `docking_advisory` must pass the nose-first regression. The first published
   manual tunnel segment must remain aligned with the stopped hull nose.
2. `accepted_maneuver_program_builder` must prove:
   - Assisted ordinary transit uses `TranslationMode::AssistedVelocity` and
     emits zero synthetic actuator/RCS segments;
   - Newtonian ordinary transit rejects lateral route thrust rather than using
     precision RCS.
3. `navigation_runtime_control` must prove Automatic Assisted runs through the
   same `applyLocalFrameInput` game law as manual Assisted and reorients VREL
   to the hull nose within 3 s with ordinary RCS = 0.
4. Full MinGW client/server build must succeed.
5. Live START DOCKING in Assisted must no longer fail with
   `accepted-program-propulsion-program-infeasible` merely because route
   curvature requires course change.
6. Expected live sequence is async planning, optional physical alignment/replan,
   then `phase=executing`; retained corridor remains visible and the ship
   actually moves.
7. Preserve the center boresight; user reports it materially improves control.

Do not weaken Builder, angular limits, tracking limits or collision proof to
make a test green.

Newtonian parking is now a separate algorithmic branch. Its target doctrine is:
long mostly-straight legs, coast/rotate/main-burn, accelerate then rotate and
brake, large maneuvering volume, and a later slow precision-placement phase
using stronger class-specific RCS and/or tugs. Do not force the Assisted curved
parking algorithm onto Newtonian craft.

Current implementation establishes this separation and refuses fake-RCS
Newtonian transit. A dedicated Newtonian stop/rotate/burn maneuver compiler is
still future work and is not claimed complete in this gate.

## 2026-09-27 — immediate gate: rerun rotating-terminal trajectory

Pull current main and rerun `bash verify_docking.sh`.

The first required result is now `trajectory_generator_angular: PASS`.
If it still fails with terminal-state reachability, inspect terminal pose error
versus terminal omega error separately; do not loosen the 5 degree orientation
or 0.05 rad/s omega thresholds.

Only after the complete docking verify is green proceed to
`bash build_mingw64.sh` and live Assisted docking.

## 2026-09-27 — immediate gate: complete verify after static-checker correction

Pull current `main` and rerun `bash verify_docking.sh`.

Expected already-proved native results remain green. The corrected automatic
static contract should now pass without requiring test-only text in production
source.

If the full verify passes, proceed immediately to:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

Then run live Assisted START DOCKING and capture all `[DockAuto]` lines.

## 2026-09-27 — immediate gate after checker hardening

Pull current `main` and rerun `bash verify_docking.sh`.

The production terminal angular handoff has been manually verified in
`GameServer.cpp`; this iteration changes only the static checker.

Expected result: full `[DOCK-VERIFY] PASS`.

After that, run the canonical MinGW build and live Assisted docking test.

## 2026-09-27 — active gate: START DOCKING from a cold route state

Run the complete docking verify on current main. The new native launch fixture
must prove straight prefix -> smooth first arc rather than merely checking the
first vector.

After a green verify/build, test this exact live sequence:

1. Start the game in Assisted.
2. Do **not** press CALCULATE TRAJECTORY.
3. Select a docking port and press START DOCKING directly.
4. Expected logs:
   ```text
   [DockAuto] request=N phase=route-preflight
   [DockAdvisory] request=N phase=stabilizing ...
   [DockAdvisory] request=N phase=settled ...
   [DockAdvisory] request=N phase=planning ...
   [DockAdvisory] request=N route=...
   [DockAdvisory] request=N phase=handoff_wait ...
   [DockAdvisory] request=N ... human_control=1
   [DockAuto] request=N phase=requested ...
   [DockAuto] begin ... phase=stabilizing
   ...
   [DockAuto] request=N phase=planning-async
   ...
   phase=executing
   ```
5. The cockpit tunnel must visibly begin in front of the boresight with several
   frames on the current hull axis, then bend smoothly into the route, then
   continue toward the dock.
6. The ship must actually acquire server Autopilot authority and move after
   planning/alignment.

If Automatic is still inert, the new flushed client/server logs must show
whether it stopped at route-preflight, request dispatch, server rejection,
authority acquisition, planning, alignment or execution.

Do not restore a dependency on pressing Manual first and do not fall back to the
2 m/s² RCS profile for Assisted guidance.

## 2026-09-27 — immediate gate after GameServer compile correction

Pull current main and rerun the canonical MinGW build.

If build succeeds, run the cold Automatic live test:
press START DOCKING without first pressing CALCULATE TRAJECTORY.

The route-preflight / straight-prefix -> launch-arc changes remain the code under
live validation. This iteration only repairs the misplaced diagnostic compile
error.

## 2026-09-27 — immediate gate: angular-time relaxation then real movement

Hard UI contract: manual docking corridor is 500 m normal / 250 m terminal.
Do not alter those values or add special launch/transition frame cadences unless
the user explicitly asks.

Next target sequence:

1. Pull current main and run `bash verify_docking.sh`.
2. The new angular native regression must prove translation is slowed when
   bounded hull rotation needs more time.
3. The docking-advisory native regression must prove the first published launch
   frame is ~500 m and the later first turn is still smooth.
4. Object-overlay contract must pin dock semantic hit radius/priority and
   nearest-dock arbitration.
5. Only after full verify PASS run `bash build_mingw64.sh`.
6. Live: START DOCKING directly in Assisted. The old generic failure
   `trajectory:angular trajectory cannot reach requested terminal state`
   should be replaced by a valid planned program, potentially reporting a
   longer `trajectory_s`.
7. Expected next meaningful lifecycle is `planned ... phase=aligning` or
   `phase=executing`, followed by actual ship motion.
8. If angular planning still fails, use the new detailed reason verbatim; do
   not widen angular limits or tolerances.

Also validate clicking a distant docking port: its card must win over the parent
module/assembly when the cursor is within the dock marker hit area.

## 2026-09-27 — next gate: hold stop must execute before final ingress

Run full docking verification on current main.

Expected native/static contracts:
- manual corridor remains 500 m normal / 250 m final;
- nose-first launch has a real tangent circular fillet;
- Automatic runtime contains ApproachHold and FinalIngress;
- long approach cannot append pre-capture;
- exact terminal angular matching is final-ingress-only.

After build, live START DOCKING without manual pre-calculation.

Expected lifecycle should now include:
```text
stage=approach-hold phase=planning-async
planned ... stage=approach-hold phase=...
... physical transit ...
stage=approach-hold phase=hold-complete next=final-ingress
stage=final-ingress phase=planning-async
planned ... stage=final-ingress phase=aligning|executing
```

At the hold point the ship must actually stop before the final short ingress.

If FinalIngress angular planning fails, use the detailed angular diagnostic;
do not reattach pre-capture to the long transit and do not widen angular limits.

## 2026-09-27 — immediate diagnostic gate

Pull/build/run and press START DOCKING once.

Capture the first `[DockRequest]` line plus any following
`[DockAuto]`/`[DockAdvisory]` lines.

The first missing transition will identify whether the break is:
UI request mode -> SpaceState, route-preflight, preparation command,
Automatic command dispatch, or server acceptance.

## 2026-09-27 — live diagnostic: request versus authority

Build current main, start Assisted, press START DOCKING once without pressing
CALCULATE TRAJECTORY. Capture the combined console from before the click through
the first result. Follow one serial through `[DockRequest] ui`, `serial`,
`client-send`, `server-recv`, plus `[DockAdvisory]` and `[DockAuto]`. If the
request disappears, inspect `ui-clear` / `client-clear`. The cockpit must show
PREPARING while route preflight runs, and Automatic only after server authority
is confirmed. Native code gate has not replaced this live validation.
The local static contract passed; this workspace has no `cmake`, so the full
verify/build remain target-machine gates.

## 2026-09-27 — next gate: full corridor and server result

Build client AND server from the same protocol-12 commit, run
`bash verify_docking.sh`, then `bash build_mingw64.sh`. In Assisted, press
START DOCKING directly and capture the full combined console from click to
server completion. Check the same request serial across UI, client send,
server receive, `[DockAuto]`, `[DockResult] server`, and `[DockResult] client`.
The cockpit/map corridor should remain visible and refresh while Automatic
executes. A server rejection or planner failure must display its reason and
retain the visual route when it is still representable. An invalid/stale
advisory must carry an unsafe warning. Actual ship motion and endpoint still
need live acceptance; static checks alone do not prove either.
Publication is complete on `main` at `4da95fa3`; pull the latest `main`
before running the target-machine gate.

## 2026-09-27 — next gate: local pilot clock

Pull the correction after publication, run full `bash verify_docking.sh`,
rebuild both binaries with `bash build_mingw64.sh`, then press START DOCKING once
in Assisted. The previous repeating `status=2` bridge rejection must disappear.
If the bridge still rejects a step, record `failure_kind`, `clock_s`, `delta_s`,
and actuator fields from the new log. Three consecutive failures now end with
`[DockResult]` server and client reasons. If physical execution begins, evaluate
motion, corridor persistence, hold stop, and final ingress separately. Startup
M8E frame stalls in the supplied excerpt are independent of this bridge
rejection and remain an open performance issue.
## 2026-09-27 — verify tolerant pilot clock with live Automatic docking

Run Windows native docking verification and client/server build on the new
main. START DOCKING should pass the pilot bridge, move physically along the
visible corridor, and report any server outcome to the client. The executor
permits 0.1 ms / 1% clock rounding but still rejects missing gameplay ticks;
log `failure_kind`, `clock_s`, `delta_s` on any bridge failure. Do not mark the
Automatic runtime accepted until live movement and final result are observed.

## 2026-09-27 — verify local-time program page selection

The new Windows trace proves bridge execution and physical movement, then
repeated Timeline::InvalidInput for multi-page maneuvers. Pull/build/test the
local-offset page-continuity correction, press START DOCKING once and confirm
no repeated `no-active-program-page status=0` or accelerate/brake oscillation.
If selection genuinely fails, expect one terminal `[DockResult]` with
`invalid-program-page-timeline` on server and client. Continue to verify
corridor persistence, hold stop, final ingress and actual terminal outcome.

## 2026-09-27 — diagnose tracking limit and observed speed discontinuity

Run Windows native verification and live Automatic once. Capture the first
`phase=recovery reason=follower-rejected` line with raw/effective position and
velocity errors, attitude and angular-rate errors, corresponding limits, actual
and target speeds and `stop_snap_epsilon_mps`. Watch for
`phase=physics-watch reason=velocity-discontinuity`. A repeated failure now
ends after three cycles with `[DockResult] tracking-envelope-exceeded`. Diagnose
the actual failed dimension before changing the physical envelope, trajectory
or follower; distinguish a server velocity discontinuity from client snapshot
presentation. Full Automatic docking is still not accepted.

## 2026-09-27 — verify Assisted ramp compensation and safe handback

Run native navigation runtime control test, full docking gate and Windows
client/server rebuild. On live START DOCKING verify the page-1 reference
velocity no longer outruns actual speed beyond the 8 m/s envelope during
a feasible acceleration. Preserve the exact position/attitude limits and
monitor physical velocity continuity. If the request fails, verify the
server returns Human control with `BrakeToStop`, and the prior Autopilot
setpoint cannot keep accelerating toward 143 m/s. Inspect hold stop/final
ingress separately. This slice needs target-machine proof before acceptance.

## 2026-09-27 — immediate gate: Assisted turn feed-forward

Automatic reached page 390 at 117.06 s. Scalar speed tracks the program, but
vector speed error exceeded the 8 m/s envelope during the approach. Run the
Windows native docking gate and live flight after the lateral feed-forward
change. Inspect reference/actual velocity vectors on any recovery; confirm
the ship follows the turn, reaches hold stop and final ingress, and aligns
roll to the dock basis. Peak executable speed vs ship limit is now logged.
The scalar solver's whole-route minimum speed remains an open issue.

## 2026-09-27 — rerun corrected native turn fixture

Windows compiled the candidate; three native docking tests passed and
navigation_runtime_control failed. The turn test itself configured only
2 m/s^2 of lateral authority while asserting 4. Rerun `verify_docking.sh`
with the corrected 8 m/s^2 fixture and its explicit clamp check. If the test
still fails, collect the one-test `ctest -R ^navigation_runtime_control$ --output-on-failure` assertion. Then rebuild and check the live approach turn.

## 2026-09-27 — verify HOLD capture then FinalIngress

Run Windows `verify_docking.sh`, rebuild client/server and retest START
DOCKING. The late HOLD approach should log `phase=hold-complete
capture=standoff-stop` (or terminal-accepted), remain under Autopilot while
braking/angular damping, then log planning/execution for `final-ingress`.
Confirm final ingress succeeds and returns `pre-capture-envelope-complete`.
If a new plan fails, keep the exact `reason=` including page/sample/forward
speed; do not confuse a failed second-stage plan with the previous late
ApproachHold error. Controlled docking contact/latch is still a separate
stage outside this pre-capture contract.

## 2026-09-27 — native/live test for angular correction in final ingress

After updating main, run `verify_docking.sh`, rebuild game/server and fly
Automatic docking. Confirm `phase=hold-complete`, then `stage=final-ingress`
and `phase=correcting-attitude` if the angular-rate error alone crosses the
tracking envelope. Ship should remain under Autopilot and continue to use
bounded angular control rather than restart the whole route. Preserve any
plan/physics-watch/recovery/result lines. Native completion at the
pre-capture point is only the current objective; a separate dock-contact
and latch authority/state transition is still needed for actual docking.
## 2026-09-27 — live gate for route-local speed and HOLD handoff

User contract: Autopilot operates the ship through control inputs and measured
vehicle response; physics/engine allocation remain owned by the ship. Starting
from a safe state, stabilize angular rates, align the hull to travel direction
if moving, and use a safe launch section. Build spatial tangent arcs from the
final approach backward, give route nodes speed/acceleration and a frame-up
marker, cap ordinary cruise at 0.8 vehicle maximum, and insert acceleration
and braking phases on long straights. At the final-section entrance stop, damp,
align laterally in the entry plane and match dock bottom/up/spin before ingress.
Small safe errors must be controlled locally instead of invalidating the route.

Current implementation slice removes global minimum-speed timing and the
storage-page condition that caused the reported 10 m HOLD recovery. Run the
Windows native gate and rebuild client/server, then START DOCKING and collect
the planned peak speed, page/velocity/position and `hold-complete` through
FinalIngress and result. If an error remains, diagnose the first actual
failure; do not assert acceptance based on static tests. Further work must
replace engine-specific planning decisions where they conflict with the
control-interface contract, finish in-plane hold alignment and contact/latch.
## 2026-09-27 — investigate live trajectory-following failure

Windows traces show Automatic angular-rate rejection at 0.255151 versus
0.25 rad/s near 68 m/s, then repeated longitudinal velocity lag around
9–10 m/s at roughly 250–300 m/s. The manual video mostly shows MANUAL
DOCKING MODE; its corridor-exit record has lateral 5.43 m and vertical
-2.36 m, inside the published release dimensions, so it cannot by itself
prove a failed Automatic steering command. Candidate fixes allow a bounded
angular correction while translating, enforce a minimum Assisted transit
fillet radius, and soften tap torque without limiting navigation torque.
`[DockAutoTrack]` now samples planned, pilot-commanded and ship-executed
accelerations and rotation every 60 ticks to distinguish command lag from
actuator saturation. Next gate: run Windows build and Automatic flight with
all `[DockAutoTrack]` and `[DockAuto]` lines through HOLD. Do not declare the
9–10 m/s speed lag resolved until that flight is measured.


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
