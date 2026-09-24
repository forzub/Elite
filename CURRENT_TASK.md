# CURRENT TASK — manual docking advisory flight acceptance

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
