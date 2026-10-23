# ELITE — CURRENT PROJECT STATE (2026-10-22, 22:30 Europe/Kyiv)

> **Authoritative continuation point.** Maintain **this file only** for Elite project context. Update this top summary and, if needed, the historical log below after each substantive code/flight finding. Do not create parallel state/status files. Work directly on `main`; preserve working Assisted behavior while investigating other modes. Keep diagnostic logs under `D:/tmp`. Distinguish code-confirmed, numerical/log-confirmed, gameplay-confirmed, and still unverified outcomes; no tests merely for the sake of tests.

## 2026-10-22 — Generic sudden equipment failure + flight injection (source, not yet native-built)

**Latest code:** Generic `EquipmentDamageKind::InternalFailure` and `EquipmentDamageState::internalFailure01` have been added to the common equipment damage layer. Five existing environmental histories remain independent; internal fault is a sixth functional loss cause, **not** fake collision/impact, structural destruction or an invented engine-only property. Generic device `evaluateEquipmentFunction` and CR-1/ER-5 `totalAccumulatedDamage01` now include this cause. `ObjectModuleRuntime::injectEquipmentFailure(moduleId,targetDamage01)` changes the installed module's damage to at least a specified TOTAL damage level, is idempotent and never heals existing causes. Runtime module revision invalidates capability cache.

**Player test control:** `Ctrl+Alt+Shift+F` sends `ClientShipCommand::InjectEquipmentFailure` to the authoritative server, which resolves the **active installed** Assisted engine slot. Automatic slot `index=-1` chooses the first not-yet-failed main motor (aft slots first), then subsequent presses select successive motors; `amount=1.0` completely disables thrust and TVC without deleting the module. Server ignores noninstalled diagnostic layouts, missing/detached modules and invalid severity. A per-key event is recorded in the server console and on Windows in `D:/tmp/elite_equipment_failures.log`. The shortcut is also displayed in `TemporaryShortcutStrip` with five cockpit localizations.

**Scenario test:** Existing `ELITE_SHIP_DAMAGE_PRESET=aft-50|aft-80|aft-failed` now injects the same INTERNAL functional fault on the first actual installed aft motor *before route planning*, rather than falsely recording impact; Assisted-Y/X real installations are no longer excluded merely because `ELITE_SHIP_PHYSICAL_TEST_MODEL` is specified. Existing `D:/tmp/elite_engine_damage_preset.log` is preserved for this startup receipt. No random auto-failure scheduling has been enabled; trigger is explicit at present.

**Replication/BITE:** Added `ObjectModuleSnapshot::internalFailureDamage01` to server snapshot builder, canonical wire DTO schema and client `ShipCapabilityBuilder`; `ShipBitePresentation` consumes it so physical failure and diagnostic HUD agree. **Snapshot wire schema 19 -> 20**: build matching client/server binaries. Control-message decoder now accepts all current `ClientShipCommand` values including the new injection type. Added specific equipment-damage, wire-data and wire-command roundtrip regressions, plus shortcut/localization check.

**Status:** Source changes committed directly to `main`; **native C++ compilation, Windows build and in-game flight are NOT verified** here (the environment has no local Elite checkout and cannot resolve GitHub for a clone). Acceptance test: build client and server; confirm I/Y/X smooth baseline, press `Ctrl+Alt+Shift+F` on Assisted-Y/X while AUTO flies and verify each press fails one real installed motor, autopilot reads reduced attainable acceleration, BITE indicates failure, `D:/tmp/elite_equipment_failures.log` has exact module IDs, and switching I→Y→X→I does not heal the fault. Separately boot `ELITE_SHIP_DAMAGE_PRESET=aft-50` and `aft-failed` to verify partial and complete loss. **Repair of latched internal faults is a separate workflow**, not implicitly performed by structural module reattachment. Avoid changing the accepted Assisted autopilot just to compensate for failure injections.

## 2026-10-23 — One aft CR-1 completely failed: axis/HUD/autopilot regression audit (SOURCE; flight acceptance pending)

**User's acceptance goal:** During normal Assisted autopilot flight, fail exactly one active aft motor by `Ctrl+Alt+Shift+F` (first press) or `ELITE_SHIP_DAMAGE_PRESET=aft-failed`. The real surviving motor(s) must deliver physically feasible force/moment, the nose-to-working-axis offset must be displayed as a **distinct amber HUD reticle**, and autopilot must adapt its course/attitude/acceleration without invented torque or fallback healthy motors.

**Static physics findings:** Aft COM geometry has half extents X=13 m, Z=11.1 m. Assisted-I left-aft failure leaves right-aft, whose proven torque-neutral force ray is `normalize((-13,0,-11.1))` in BODY axes: **49.5078° off the nose**, nozzle deflection inside the CR-1 75° cone. A pure axial surviving outboard engine would inject parasitic yaw torque; its force ray through COM is the safe emergency choice. Assisted-Y's two remaining asymmetric aft motors may likewise require an offset. Assisted-X's surviving triangle can retain a central axis using a nonnegative opposite diagonal pair, so X is **not expected always to display an offset marker**. Existing `WorkingThrustAxisAnalysis.h`, `ShipCapabilityBuilder::applyMountedEngineAxialAuthority`, `AssistedManeuverPrograms::execute`, `MotherComputer::selectFlightDoctrine` and `AssistedAutopilot` contain emergency-axis handling; `FlightVectorIndicatorRenderer::renderEmergencyThrustAxis` and `SpaceState.cpp` ALREADY render a separate amber marker when server publishes `motion.assistedWorkingThrustAxisEmergency`. No new HUD marker or autopilot regulator retuning was needed at this step.

**Important defect FIXED on main:** Client `ship.descriptor` remains the native Assisted-I catalog even after server installed-layout switching to Y/X. `MotherComputerClientAdapter`, `ReplicatedAssistedControlAuthority` and direct `EngineThrustHudPresentation` previously treated `ship.physicalControlModel != ship.descriptor->physicalControlModel` as synthetic diagnostics and silently reconstructed HEALTHY Y/X slots/reactors, hiding real failed motors from client autopilot and HUD. New `ReplicatedInstalledAssistedDescriptor.h` resolves the actual per-layout CR-1 and ER-5 IDs *only if all immutable module passports exist*, and binds snapshot motor damage/power/HUD to the selected real equipment. A missing installed module does not trigger healthy fallback. Replicated motor-authority reconstruction now also consumes `internalFailureDamage01`, matching server six-cause functional damage. Native-I working fast path remains unchanged.

**Focused tests added (not executed here):** `EquipmentDamageFailurePipelineTests` checks full failure of installed left-aft I motor, selection of right-aft slot, 49.5° torque-neutral center-of-mass ray, disabled failed motor and nonzero surviving propulsion. `EngineThrustHudPresentationTests` checks that installed Y/X snapshots with a full internal fault are not mistaken for healthy diagnostics and the correct 6/8-slot HUD raises an alarm. Existing `MotherComputerTests` also verifies `DamageOffsetAxis` transfer to autopilot and rejects discrepancy between client and authoritative server axes. Check only relevant tests on local Windows build: `ctest --test-dir <build-dir> -R "equipment_damage_failure_pipeline|engine_thrust_hud_presentation|mother_computer|working_thrust_axis_analysis|installed_engine_actuator" --output-on-failure` (use the user's actual test build directory).

**Flight capture improved:** Existing bounded `D:/tmp/elite_attitude_jitter.csv` now appends columns `axis_emergency,axis_body_x,axis_body_y,axis_body_z,axis_offset_deg,axis_motor_index` after the original per-motor telemetry columns, without changing previously named/indexed columns. Log captures up to 90 seconds after first active attitude/nav command; run one clean game process and trigger engine failure early during autopilot flight so both pre/post events are captured. `D:/tmp/elite_equipment_failures.log` supplies exact failed installed module/selection timestamp context. User should send both logs plus video/screenshot of the amber marker; evaluate course deviation, angular response, nozzle deflection, main-generator budget and actual path tracking.

**IMPORTANT ACCEPTANCE BOUNDARY:** This environment can inspect and edit GitHub `main` but cannot resolve github.com from local container, so **no game binary, Windows compilation, CTest execution, or actual aircraft-in-flight/autopilot outcome has been observed in this response**. Source logic and tests are ready, NOT proof of successful damaged-engine corridor passage. Before changing existing smooth Assisted controller, inspect captured real flight first. Maintain single state file on `main`.

## 2026-10-23 — Requested two-run Assisted-I aft-engine damage acceptance

**2026-10-23 PowerShell parser fix:** First user attempt from MSYS2 successfully reached the batch script but PowerShell rejected two warning strings containing the bare interpolation `$preset:` (line 81/93). Fixed in `main` commit `911c19e` by using `${preset}:` in both lines, keeping all scenario settings unchanged. No game flight executed in that attempt; after `git pull --ff-only origin main` from `/d/__elite/work`, rerun the existing batch with a Windows path from `cygpath -w` (or `D:/tmp` forward slashes in the ReportDirectory argument). The fix changes only the script; do not rebuild binaries for this parse correction.



**Request:** Exactly two identically configured game-level navigation stress runs on Assisted-I: first `aft-50` (50% FUNCTIONAL damage of left aft installed CR-1), then `aft-failed` (100% INTERNAL functional failure of the same physical motor). Both use the existing simple-stress diagnostic scene with hot-start 300 m/s, nose offset 0°, unchanged autopilot and path. Do not run six I/Y/X combinations or use the isolated ideal flight-matrix test as proof of live damage handling.

**Runner update:** `scripts/run-assisted-engine-damage-flights.ps1` now accepts optional `-AssistedIOnly` switch, narrowing the existing six-case matrix to the two named I cases without changing the default six-case behavior. Correctly sets `ELITE_SHIP_PHYSICAL_TEST_TOTAL=2`; checks newly appended `D:/tmp/elite_engine_damage_preset.log` receipts for the exact requested preset, `status=APPLIED`, `module=ship_engine_L`; preserves the original navigation stress report plus **separate** copied `D:/tmp/elite_attitude_jitter.csv` per flight before the next run overwrites it. Writes per-flight receipts and combined `assisted_damage_flights_summary.csv`. No fresh receipt or no fresh flight trace cannot receive a successful status, even if runner reports COMPLETE. Restores original process environment after the batch.

**Expected physics (hypotheses to verify):** 50% left-aft: the two remaining operational aft CR-1s can preserve central torque-neutral forward thrust by throttling the healthy right motor, though maximum forward acceleration falls. No amber off-axis marker should be required merely from 50% loss. 100% left-aft: right-aft is sole aft candidate; physical CoM ray is offset ~49.5° toward body-negative X, amber THRUST AXIS sight should appear away from the fixed nose boresight; the ship computer must select the engine-out flight doctrine, never attempt a fixed -Z burn using phantom balancing torque. Autopilot must reflect sharply reduced achievable force and be judged by actual corridor/stability/termination evidence rather than forcing a pass.

**Windows operator command** from the updated Elite repo root after building BOTH game and server:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-assisted-engine-damage-flights.ps1 -AssistedIOnly -ReportDirectory 'D:\tmp\elite_assisted_I_2runs'
```

Collect the resulting two distinct `*_attitude.csv`, two `*.log` scenario reports, receipt files and summary CSV; evaluate `axis_emergency`, `axis_offset_deg`, `axis_motor_index`, executed yaw torque/angular velocity, motor thrust/nozzle angle, and autoplanner corridor error. `D:/tmp/elite_equipment_failures.log` belongs to live keypress events and need not have entries in these PRESTART scenario runs.

**Verification status:** Runner source prepared and reviewed, **the two executable/game runs have NOT been performed inside ChatGPT's Linux container** (no local Windows EliteGame.exe, PowerShell or repo build). Do not claim pass/fail before actual logs. Keep this as one focused acceptance batch, no changes to healthy Assisted regulator or broader navigation experiments.

## Current focus and result

**Assisted-I autopilot pitch/yaw jitter is FIXED IN OBSERVED GAMEPLAY**, as reported by user on 2026-10-22: ship movement now smooth, no visible tremor. Independently, the newly supplied 90.02-second server flight capture strongly corroborates that outcome. This closes the specific **high-frequency pitch/yaw full-authority bang-bang** diagnosis; it does **NOT** certify every Assisted-Y/X layout, Newtonian behavior, exact docking completion, or all flight envelopes.

- Implementation on `main`: `3ee3209` added physically bounded damped planar PD input to `src/game/navigation/autopilot/HullAttitudeControl.h` (`pitchYawResponseSeconds`, separate from default legacy predictive path); `7afe8dc` opted **AssistedAutopilot only** into T=0.50 s via `src/game/navigation/autopilot/AssistedAutopilot.h`. Direct GitHub source inspection confirms both paths and `pitchYawResponseSeconds = 0.50`.
- Previous `b4620b0` separated course/velocity guidance from true **hull nose-to-lead** pose error, preserving the outer velocity-course loop and a bounded 10° nose lead. Both fixes were necessary: the first corrected the reference; the later PD inner servo corrected excessive 50-Hz gain.
- New user flight evidence `elite_attitude_jitter(2).csv`: 4501 samples, dt=0.020 s, 90.02 s, Assisted physical model `model=0`, 4 powered main motors, `reactor_on=1`, valid motor program/sample for all frames, generator ~10,000 MW, maximum measured velocity **412.07 m/s**. Emulated pitch+yaw control vector at saturation (norm >=0.95): **0%** at 10–20, 70–80, and 80–90 s (previous affected trace: **98.6%, 95.0%, 97.0%** in those windows). Over the 90s, pitch command sign reversals (ignoring |command|<=0.1) **2** and yaw **1**, compared with earlier **588** and **722** in the prior automatic trace. Only 27/4501 frames (0.6%) had control-vector saturation; all were in the initial 0–10 seconds. This is direct evidence that sustained bang-bang has disappeared, **not** a claim of literally zero fine-grained angular acceleration changes.
- Companion `elite_docking_planner(20261022-192708).log`: 36 route results, all with `failure=""`; route-construction log, **not** a per-frame actuator trace and not proof that docking was completed.
- User feedback is **qualitative gameplay acceptance** for smoothness; no CTest matrix or independent full Windows build was executed by assistant this turn. Do not accidentally revert this to `unverified jitter` when reading older entries below.

## 2026-10-22 — Assisted I/Y/X topology audit after smooth Y/X gameplay

**GAMEPLAY REPORTED BY USER:** Assisted-Y and Assisted-X now fly as smoothly as Assisted-I (2026-10-22). This establishes observed smoothness for all three, NOT independent Y/X frame-log measurements, completed docking, engine-out or damage acceptance. The previous 90.02 s `elite_attitude_jitter(2).csv` is **Assisted-I only**, model=0, four motors.

**SOURCE-CONFIRMED (main, no physics code changes):** They do NOT secretly execute an Assisted-I four-engine matrix. `CobraPropulsionPresets.h` defines Assisted-I 2 aft+2 fore (4), Y 3+3 (6) triangular and X 4+4 (8) radial, with distinct normalized moment arms. `ShipPhysicalControlModels.cpp` dispatches by selected model; `AssistedManeuverPrograms::build/execute` derives a six-axis force/torque wrench per selected topology, uses `count=mounted.size()`, and executes each motor separately. `ShipControlExecutionGateway.cpp` sums actual individual motor forces and moments `r × F` using that model, publishes `engineTelemetry.count`, and applies common power-feasible Assisted scaling. One shared generic allocator/flight law is **intentional**, not a fallback to the I geometry.

**Important diagnostic-vs-installed boundary:** The Cobra descriptor's ordinary constructed ship is **native Assisted-I**, with four actual CR-1 engine module IDs and two actual paired ER-5 reactors. Server `ClientShipCommand::SelectPhysicalControlModel` (Ctrl+F9) always calls `ShipCore::selectPhysicalConstructionForDiagnostic`; `ShipState::selectConstructionForDiagnostic` marks `DiagnosticOverride`, EVEN when selecting I again. The selected diagnostic topology gets healthy synthetic CR-1 engine *instances* in `ShipCore::updateTranslationControl` and `controlAuthorityTelemetry`, indexed as 4/6/8. Those synthetic motors still deliver physically calculated Newton forces, `r × F` torques, individual nozzle vectors, electrical demand and actuator telemetry, but are **not persistent/damageable installed ship modules**, and their thermal/damage/aging data do not come from distinct real Y/X modules. `CobraPropulsionPresets::installationSlots` declares generator group layouts of 2/3/4; diagnostics use 5000 MW synthetic fallback for unavailable groups, while some group indices may still resolve the native I reactor records. Do **not** mislabel the current Y/X diagnostic power/temperature behaviour as a fully installed production Y/X ship. `ELITE_SHIP_PHYSICAL_TEST_MODEL` is also a test-only descriptor override.

**Practical continuation:** Preserve accepted smooth Assisted control. If full Y/X ship construction is wanted, install 6/8 actual engine modules with matched IDs, 3/4 genuine associated ER-5 groups, and wire persistent damage/heat/power, **without changing the shared six-axis actuator solver or retuning the working autopilot**. Source review is enough to settle the present four-engine-scheme question; no additional tests or log collection are needed just for this audit.

## 2026-10-22 — Proposed replacement of Ctrl+F9 diagnostic synthesis with installed Assisted hardware (NOT IMPLEMENTED)

User asked whether Assisted I/Y/X can use genuine installed CR-1/ER-5 modules on Ctrl+F9 switching, rather than synthetic diagnostic motors/reactor group budgets. **Feasible and recommended; request was a feasibility question, not yet authorization for a code patch.** Current repo unchanged in this discussion.

Code dependencies verified:
- Native Cobra descriptor `EliteCobraMk1.cpp` defines four genuine engine module IDs and the reactor IDs for its selected construction; the global `ShipDescriptor` is a static immutable type definition, and `ObjectModuleRuntime::init` initializes module states from the descriptor once. Current `ShipState::selectConstructionForDiagnostic` explicitly activates `DiagnosticOverride`.
- `ShipCore::updateTranslationControl` then creates synthetic CR-1 passports and 5000 MW fallback for diagnostic I/Y/X. In contrast, production motor binding uses `descriptor.physicalControl.slotModuleIds` and `ObjectModuleRuntime`; `ShipCore::controlAuthorityTelemetry` must share the same selected bindings. `ShipCore::updateSystems` advances **real** engine and ER-5 temperature, life, electricity usage only when `source == ModuleRuntime`, and reads descriptor-global engine/reactor bindings; `ShipCore::getCoreStatus` also reads the bound reactor IDs. Merely changing model ID or making fake descriptors is NOT enough.
- Proposed path: explicit per-layout equipment definitions/bindings with unique IDs for Assisted I (4 motors/2 ER-5), Y (6/3) and X (8/4), all backed by `ObjectModuleRuntime` installed state; select and activate exactly one set per ship without resetting world pose, linear/angular velocity, pilot mode or the already accepted Assisted inner/outer control. Preserve distinct heat, accumulated damage, component age and repairs for each installation; do not reset a damaged engine to healthy by changing the layout. Ensure inactive configurations do not generate force/power or take unrelated thermal loads. Update capability, physical motor executor, reactor groups, thermal service loops, snapshots/HUD and damage/hit binding to resolve through the SAME active equipment selection. Respect changed mass/CoM/inertia if configurations have different installed masses.
- Remove synthetic CR-1 creation and generator 5000 MW fallback from the normal Ctrl+F9 path. If a selected installation is incomplete or destroyed, report reduced/zero authority rather than secretly reverting to Assisted-I or creating phantom thrust. Keep any truly isolated test harness explicitly separate. Preserve `main`-only policy and verify I/Y/X actual `engineTelemetry.count=4/6/8`, active groups 2/3/4, real damage/thermal persistence across I→Y→X→I, no regression in accepted smooth flight.

## 2026-10-22 — Installed Assisted I/Y/X switching implementation (source only, runtime validation pending)

**New source change:** Cobra now defines persistent CR-1/ER-5 equipment for all three Assisted layouts (I:4+2, Y:6+3, X:8+4). The Cobra module runtime initializes real module states once; immutable installed descriptor views bind the selected model to its true engine/reactor IDs. `Ctrl+F9` now rebinds Assisted layouts with `ModuleRuntime` rather than `DiagnosticOverride`, preserving position, velocity, attitude, module damage, reactor thermal state and accrued operating life across switches. Installed Assisted path invokes the existing actual engine actuator matrix, group power limits, per-motor thermal updates, native reactor thermal updates, HUD and capability telemetry, without new Assisted autopilot tuning. No 5000 MW synthetic group budget or synthetic CR-1 motor objects are taken on this Assisted Ctrl+F9 path. The legacy diagnostic fallback remains only for explicitly selected non-installed Newtonian/test configurations.

**Subsequent source correction:** `ShipCore` now retains the native descriptor for Newtonian diagnostic switching, avoiding use of the previously active Assisted-Y/X reactor bindings (`006b952`). Assisted-Y/X additional logical engine modules have persistent real damage/thermal state but no dedicated authored exterior mesh parts; therefore spatial hit-volume acceptance for these extra motors is **not verified** and should not be claimed complete.\n\n**Important validation boundary:** Changes are source-level, **NOT native-built or game-accepted** in this environment. Check I→Y→X→I in a Windows build: `source=ModuleRuntime` all three, `engineTelemetry.count=4/6/8`, ER-5 status arrays `2/3/4`, no synthetic "SIM ER-5", no accidental revival of a destroyed engine after switching, physical force/torque and thermal response; verify Assisted smoothness remains accepted. If equipment mass is introduced into module passports later, update mass/CoM/inertia per active configuration; current three presets deliberately preserve the same baseline hull mass/CoM.

## Architecture and non-negotiable boundaries

1. **Autopilot substitutes pilot controls**, never teleports state or commands engines outside the same physical actuator pathway. Server computes installed motor/RCS forces and torques with real thrust/authority/power/thermal/damage limitations; planner/controller must work from real achievable acceleration envelopes, not desired performance.
2. Maintain **strict Assisted vs Newtonian flight-law isolation**: independent planners, autopilots, associated helpers/control doctrine. Working Assisted behavior is baseline; avoid changes to Assisted to solve Newtonian faults.
3. Assisted outer guidance is about **velocity vector following the route**; hull attitude is an inner controller, with modest course-dependent nose lead. Do not conflate nose-to-course angle with velocity-to-course error. Preserve continuous authored route curves, lead-turn, tunnel visualization, physical stopping/braking and corridor constraint.
4. MotherComputer is an orchestration/telemetry/presentation consumer of the capability model, not an alternative physical actuator. Separate raw ship plant and its timed inputs/outputs from advisor, planner, guidance, HUD and diagnostic layers. Handle missing/damaged modules by capability, not one-engine-out special cases.
5. Current real propulsion architecture includes ER-5 reactor-backed CR-1 main thrusters, thermal pads and separate services/coolant concepts; six I/Y/X + Assisted/Newtonian layouts and physical Newtonian RCS modules have been under development. Existing HUD/thermal/synthetic-layout work is described in dated entries below; a source patch without gameplay verification is not a PASS.
6. Keep changes small and local; no redundant tests/contracts, no side branch, no additional state files. Preserve all other prior decisions below as historical context, but treat this summary as the most recent outcome where they conflict.

## Open work / next practical verification

- **Do not retune the fixed Assisted-I attitude loop without new evidence.** If needed, use actual in-game smooth flight, first turn and approach to confirm corridor behavior; never declare complete docking based only on a route-builder result.
- **Assisted-Y/X**: user game-confirmed smoothness before installed-hardware migration. Source now implements 6/8 **installed** CR-1 motor identities and real 3/4 ER-5 sets on Ctrl+F9, but Windows build and flight validation are OPEN. Do not retune accepted controller. Check true damage/thermal persistence and authority after switching.
- **Newtonian I/Y/X and RCS**: separate capability, self-stabilizing release, real installed thrusters/power, engine-out/damage and turn/braking behavior still require mode-specific flight acceptance; this Assisted trace cannot validate Newtonian.
- Thermal/ignition/cooling/damage/HUD: check against observed snapshots and real server readings as separate work; do not promote source-only fixes to runtime PASS.
- On future failure: collect **minimal targeted** frame-level actuator/attitude data in `D:/tmp`; compare requested controls, physical executed angular acceleration, angular rate, motor power and render motion before changing controller gains.

## Current repository anchor

Repository: `forzub/Elite`; branch: `main`. At review, head was `96aeaa1` (state-document update after code commits `3ee3209` and `7afe8dc`). This snapshot records the **subsequent user gameplay confirmation and new 90-s telemetry**, not a new physics change. The remaining sections are preserved project history, **including entries with superseded open/unverified jitter status**.

---

## 2026-10-22 — Post-fix flight confirmation, no code changes

User: “все, движется как по попке младенца. гладко.” The final trace above confirms pitch/yaw control now behaves without nearly permanent full-stick reversal. Future continuation should begin with the real smooth-flight result, not resume the already-solved jitter diagnosis. The files `elite_attitude_jitter(2).csv` and `elite_docking_planner(20261022-192708).log` were analyzed in this conversation and **are not committed to GitHub**. Keep diagnostics in `D:/tmp`.

---

## 2026-10-22 — 19:05 flight: measured AUTOPILOT pitch/yaw bang-bang; fix the inner attitude loop (main ONLY)

User supplied `elite_attitude_jitter(1).csv` (4501 complete 50Hz server frames, 90.02 s, Assisted-I physical model 0, four main engines, generators continuously on at 10,000 MW), console `Вставленный текст(20261022-190553).txt`, and `elite_docking_planner(20261022-190605).log`. This is a new AUTO docking run after earlier `b4620b0` course/hull separation, not manual data. Console confirms `[AttitudeJitterTrace] BEGIN` and `COMPLETE rows=4501`; dock follower tracks and advances; no planner failure. At times 10–20 sec, automatic pitch/yaw input vector magnitude is >=0.95 for **98.6%** of frames, despite very small steering angle reported in follower; 70–80 sec 95.0%, 80–90 sec 97.0%. Across the trace pilot-emulated automatic inputs switch sign directly 588 times for pitch and 722 times for yaw (nonzero >0.1 threshold). Actual motor-executed pitch/yaw accelerations also alternate sign (577 and 771 flips respectively); servo instability is REAL physical torque, NOT just render/camera. Example at 12.10–12.24 sec measured command pitch -0.187 -> +1.0 -> -1.0 with yaw +0.982 -> near zero -> +0.003; physical pitch angular acceleration follows sign. The common 10 GW engine budget never disappeared; primary root cause NOT reactor ignition and NOT planner geometry.

Root source: `HullAttitudeControl::predictivePlanarInput` uses desired angular rate derived from stopping distance sqrt(2 * authority * poseError), then derives attitude control as `(targetRate - currentRate)/(authority * deltaSeconds)`. With deltaSeconds=0.020s it saturates at full positive or negative command on tiny attitude error, unlike a damped regulator. The earlier outer course-to-hull fix did not eliminate this high-gain 50-Hz bang-bang. The real executor follows those commands, so tuning motor thrust is NOT the remedy.

**Targeted fix DIRECT ON MAIN:**
- Commit `3ee3209` adds optional `pitchYawResponseSeconds` to `HullAttitudeControl::Request` and separate `dampedPlanarInput` PD inner servo. It computes bounded physical angular acceleration `a=error/T²+2*(desiredRate-actualRate)/T` with T>0 and scales by actual angular acceleration authority; shared pitch+yaw request vector limited to unit magnitude; no integrator, no impulses. The legacy predictive law remains the default at responseSeconds=0 for other callers and Newtonian. Roll channel and tunnel roll remain unchanged.
- Commit `7afe8dc`: ONLY AssistedAutopilot opts in to smooth inner pitch/yaw response T=0.50s. Outer VelocityCourseGuidance and max-10deg nose steering lead from previous fix preserved. No changes to navigation planner, ship server physics, engine authority, thruster damage, Newtonian or manual input.
- Simple numerical check, at angle 0.4° and rate 0: new input ~0.011 (not +/-1); at angle 15° and rate 0: new input ~0.427; 45° can still saturate legitimately. For a 0.4° step the linearized PD settles monotonically instead of alternating full authority. Native C++/Windows compile and game verification NOT performed.
- User has already provided sufficient data for root-cause evidence; next: pull/build current main and do AUTO Assisted-I straight + first turn, capture another `D:/tmp/elite_attitude_jitter.csv`. Compare pitch/yaw sign changes and fraction of saturated input against above measured baseline. If physical CSV smooth but picture trembles, inspect snapshot/render interpolation separately. Existing CSV re-truncates on game launch; copy if retention desired.

Keep only `PROJECT_STATE.md` for state; do not create branch, new protocols, broad tests, or alter Newtonian.

---

## 2026-10-22 — Autopilot jitter: velocity-course error was fed to hull attitude controller

Evidence: user uploaded `elite_attitude_jitter.csv` (1340 records, 26.8 seconds of **manual** Assisted-I input; model=0; source=0; no navigation-acceleration requests, nav_angular=0) and `Вставленный текст(20261022-184822).txt` console from AUTO docking. Manual data: pitch held to 1.82s, roll held 3.90–5.80s, yaw 7.44–9.78s; actual engine angular acceleration matches desired (|delta| <1e-4 rad/s²) for all three axes, and power group budget remains 10,000 MW. Motors accelerate up to 1.56578 rad/s pilot speed limit, then decelerate smoothly after release. Thus manual-installed-wrench allocator cannot alone explain autopilot-specific visible twitch.

Dock client log has 12 decimated `DockClientTrack` records: hull rotation rate ~0.09–0.36 rad/s under automatic flight, `pitch/yaw` alternating from ~-0.98 to +0.91 and back while route course error already ~0.1–0.8 degrees, often much smaller than hull rate; `motor_angular_a_radps2` sampled near zero (1e-6...1e-5), not enough to reconstruct fine-tick torque. The logged `pitch/yaw` are pilot *controls* (not directly angular acceleration), and the 12 diagnostic records alone do not prove fine-tick switching frequency. Console `[AttitudeJitterTrace] BEGIN` proves existing server CSV capture also triggers during autopilot, but uploaded CSV is from manual run, not same automatic run.

Source diagnosis: `AssistedAutopilot.h` previously fed `VelocityCourseGuidance::pitchYawErrorLocalRad` **directly** into `HullAttitudeControl` as if it were hull nose error. That value is explicitly the error between actual VELOCITY direction and route direction, which changes with **lag** (thrust/velocity), while `HullAttitudeControl` closes fast loop on BODY angular rate with predictive acceleration and dt. Thus sustained hull rotations can be demanded even after the hull has already moved ahead, followed by abrupt pilot-control reversals as velocity finally catches up. This is a specific controller-layer mismatch, not evidence against velocity-vector-first guidance.

**Code fix on main** `b4620b0`: existing `VelocityCourseGuidance` remains authoritative OUTER loop. It derives a bounded max-10-degree nose steering lead from the actual velocity vector toward the authored desired route course; INNER hull loop now uses `HullPoseGuidance` to compute ACTUAL nose-to-lead angle before passing that to `HullAttitudeControl`. Lead fades continuously as course error vanishes; pitch/yaw remains separate from hull roll. Emergency working-thrust-axis compensation and terminal attitude-hold path remain as before. No autopilot->engine bypass, planner change, reactor-power change, or direct angular impulse. Root-cause explanation is well supported by code, but improvement **NOT YET native-built or confirmed in gameplay**. Effect on smooth high-speed curved flight also needs verification: avoid unqualified PASS.

Next: pull/build main, perform automatic docking in Assisted-I, allow enough flight for the trace and send `D:/tmp/elite_attitude_jitter.csv` and console log. Compare actual commanded pitch/yaw flip rate with executed torque and hull angular velocity at full 50 Hz. If jitter persists despite smooth motor rates, inspect client snapshot interpolation, camera. Continue to store only `PROJECT_STATE.md`; no side branches. Do not alter Newtonian flight law.

---

## 2026-10-22 — Attitude jitter diagnosis, real server CSV added (main only)

User supplied `20261022-1820-16.6154822.mp4` (~122 s, 1770x946, 30 fps) and `elite_docking_planner(20261022-182303).log` (~322 lines). Video shows cockpit movement/scene changes; there is no time-correlated server motor command recording in video. Log ONLY has `DockPlannerFile/Request/Geometry/Stage/Result` lines (33 route results) and contains NO executed motor thrust, torque, yaw/pitch angular rates or stabilization controller commands. Therefore it **cannot prove the origin of the visible shaking**. Specifically, previous guessed cross-reactor group power scaling was NOT sufficient to remove the shake; do not state shake fixed.

Source-followed current Assisted attitude path:
1) `ShipCore::updateMotionControl` samples pre-evaluation angular rate, calls `ShipControlExecutionGateway::evaluateAttitudeControl` (ShipController rate law), converts hypothetical rate delta to `motion.requestedMainAngularAccelerationBodyRadPerSec2`, restores real old rates;
2) `ShipCore::updateTranslationControl` performs motor-program wrench solve (`AssistedManeuverPrograms::execute`), scales motor output by common reactor electrical budget, recomputes physical force/torque and applies real angular delta. The joystick/button RELEASE creates a discontinuous transition from held-input acceleration to rate-damping demand, and signed maneuver program combination can switch active motor sets around zero. Both are plausible sources of visible twitch; actual cause NOT yet measured. Camera/render interpolation is a separate plausible source.

**Instrumented in main:** `ShipCore.h` remembers `m_attitudeRatesAtTickStart` from the controller tick (commit `8c8eeed`). `ShipCore.cpp` (commit `d8a167c`) writes `D:/tmp/elite_attitude_jitter.csv` automatically for the **player ship only**; starts at first manual pitch/yaw/roll or nonzero executed attitude/navigation command, records the next **90 real fixed-step seconds**, flushes every 30 records, closes automatically. First open truncates prior run file. Logs tick, delta t, model, source, manual pitch/yaw/roll, nav flag, reactor state, pre/post angular rates, requested/actual angular acceleration, physical x/y/z torque, main linear acceleration, velocity, generator available total MW, motor-program valid, motor sample valid/count, actual 8 motor thrust levels, nozzle yaw/pitch angles, motor electrical MW. Server console logs `[AttitudeJitterTrace] BEGIN` and `COMPLETE`; writes to D:/tmp without requiring any environment setting. Does not affect physics, controller, autopilot or HUD. **Not compiled or runtime-verified yet.**

Next immediate user repro: pull/build current main; choose **Assisted-I** (known working; avoid conflating Assisted-Y energy defect); autopilot OFF; press/hold W or S for ~2 s then release and watch the braking jerk; repeat A/D or Q/E, then travel straight ~10 s. Send `D:\tmp\elite_attitude_jitter.csv`. From timeseries, compare demanded accel with measured motor torque and observed rate derivative: if actual motor torque oscillates while demand smooth -> executor/power/solver; if requested accel oscillates -> ShipController damping/sampling; if actual rate smooth but video shakes -> renderer/camera interpolation. Only then change the relevant behavior. Keep project state in THIS FILE ONLY. All development commits directly on main, no side branches.

---

## 2026-10-22 — User correction: Assisted-I works; defect specific to Assisted-Y; jitter OPEN

User clarified screenshot shows **ASSISTED-Y**, while **I layout functions normally**. Hence previous description of universally disabled main propulsion is incorrect. The non-native Y diagnostic topology has 3 fore/aft ER-5 powered groups (vs I with 2), plus synthetic CR-1 motors; group-to-reactor identity mismatch explains the unpowered Y HUD. The targeted diagnostic synthetic-group power-budget fix is committed in main (`c8d591f`), NOT game-verified. Critically, **ship shaking persists**, including after past equal-factor Assisted power-limit correction; do not mark the shaking as fixed or conflate it with the Y no-thrust defect. Next required work: game-build verification of Y vs I physical thrust, then capture time series of angular requested/actual acceleration, motor forces/torque and rates during release to identify controller chatter. Do not tune damping blindly. No additional feature branches.

---

## 2026-10-22 — Fix blank ER-5 gauges, diagnostic Assisted-Y dead thrust, ignition acknowledgement

User screenshot showed `ASSISTED-Y` thrust HUD with `-- K` on every ER-5, plus unrelated legacy services `REACTOR` widget at 3500 K. Ignition chord appeared to do nothing; RCS moved but main propulsion only twitched; docking/status terminal emitted no start/off messages.

**Confirmed source defects and main-only fixes**:
1. Snapshot binary `WireDataSchema.h` serialized `ShipCoreStatus` WITHOUT `engineReactors` or actual `newtonianRcsExecuted01`; these server fields were silently lost every replication. Added `EngineReactorStatus` nested schema, their full list + 24 RCS values + ignition fields. Snapshot schema version 18 -> 19.
2. `ShipCore::getCoreStatus` gated core-temperature visibility on thermal load/lifetime even though a running idle ER-5 at 900K is valid. Now report finite positive temperature immediately, including idling.
3. `ASSISTED-Y/X` use synthetic test motor layouts on top of native Cobra I. `updateTranslationControl` attempted to find matching real ER-5 IDs for all synthetic groups and set unbound groups to 0 MW, disabling physical motor wrench despite motor widgets. Diagnostic layouts now have a synthetic 5000 MW per group when switches enabled and restart countdown complete; **native models still require actual physical reactor modules**. UI shows `SIM ER-5` instead of `-- K` for synthetic reactors: do not pretend their synthetic temperature is a real reading.
4. Added server-side `propulsionSwitchRevision` increment each toggle and `propulsionReactorsOn` / startup remaining seconds into replicated `ShipCoreStatus`; mothercomputer cockpit view carries state to HUD; tiny per-reactor ignition lamps: GREEN=online, YELLOW=10-second restart, DARK=off. No unconfirmed green on client keypress.
5. Existing docking diagnostic terminal now opens on a received ignition transition and prints OFF, STARTING countdown, READY, DIAGNOSTIC 5GW simulation, or ERROR no functional physical ER-5. If command does not arrive, no revision: terminal does not fabricate acknowledgement.
6. Service ReactorSystem power had been forced to 0.8 on all normal server ticks after restart by previous prototype, independent of prior load. Now preserves/restores actual pre-shutdown service throttle; the old `REACTOR` HUD at 3500K is **services reactor, NOT an ER-5**. Its heat model not rewritten.
7. Main plus RCS are gated by ignition state already; do not imply RCS can move on reactor off intentionally. Remaining jerks with power on still need in-game tracing, not treated as solved.

Changes committed **directly to main**; no new branches, no in-game build/compile completed by assistant. Validation sequence: build; inspect `ASSISTED-Y` simulated generator indicators, powered rotation, `Ctrl+Alt+Shift+I` OFF->DARK, no main torque/RCS; ON->YELLOW 10s->GREEN, terminal messages; then actual native layout measured ER-5 temperatures/G-P-S and physical damage limits. Check fresh game startup and existing snapshots schema version 19 both client/server. Log captures to D:/tmp. Never claim full game verification before run.

---

## 2026-10-22 — Power-toggle scope correction

Follow-up in main `9ae2ddc`: all-reactor switch also sets existing ship-services `ReactorSystem` throttle to zero on OFF and throughout 10-second ON startup delay; restores 0.8 normal idle on startup completion. Unlike propulsion ER-5, ship-service power feeds the current `PowerBus`. Newtonian RCS 24-nozzle executor now presents its physically mounted nozzle availability as false when all reactors are off/starting. Executor outputs zero force/torque and replaces nominal fake maneuver demand with executed zero result. The earlier saved reactor switch name `TogglePropulsionReactors` remains a protocol label but its actual behavior is all reactors. This is a deliberate game-wide safety stop, not just motor-specific.

The 10-second sequence currently gates electrical authority, not fully simulates a cold-core temperature ramp/cooldown. Preserve the future cold-start/stop machinery boundary. No native build/flight test executed.

---

## 2026-10-22 — ER-5 power toggle + Assisted independent hull mode (MAIN only)

User-requested commands wired into existing single codebase:
- `Ctrl+Alt+Shift+I`: send `ClientShipCommand::TogglePropulsionReactors` to server. `ShipCore` initially starts with installed ER-5 active at idle (900 K; main engine CR-1 450 K). First press disables the propulsion-reactor electrical group budget (all primary motors receive 0 MW); next press starts a temporary **10-second** delay before generator electrical authority returns. Countdown advances on server systems ticks, no five-minute start yet. No new branch. This is an on/off electric authority prototype, NOT a fully implemented thermal cool-down / cold-restart sequence.
- `Backspace` in gameplay Assisted: send `ClientShipCommand::ToggleAssistedFreeRotation`. Server-owned state `DynamicMotionState.assistedFreeRotation` replicated in `WireDataSchema.h`. While enabled, the ordinary Assisted speed-setpoint and lateral velocity servo are bypassed; + and - have no authority; pilot pitch/yaw/roll controls and numeric-keypad translation still drive installed main motors through existing physically budgeted 6D allocator. No invented manoeuvre force. Newtonian ignores this toggle.
- On disabling free rotation, server starts bounded `VelocityAlignmentMode::ForwardToVelocity` in Assisted; the existing ShipController aligns the nose to the real relative velocity direction, with body rotation still executed by physical main motors; longitudinal speed servo remains disabled until alignment completes. Neutral/manual attitude does not interrupt the return alignment. At tiny/zero speed alignment finishes directly.
- `FlightVectorIndicatorPresentation` transports free-rotation mode to `FlightVectorIndicatorRenderer`; ring becomes 4 px thick and green when active, normal thin blue otherwise.
- Temporary bottom shortcut strip describes `Backspace FREE ROT` and `Ctrl+Alt+Shift+I REACTORS`, localized in five languages in `src/assets/localization/ui/cockpit/flight.json`. SystemMap Backspace continues to mean map BACK.

Code touched: `ClientShipCommand.h`, `DynamicMotionState.h`, `WireDataSchema.h`, `ShipCore.h/.cpp`, `GameServer.cpp`, `DynamicMotionSystem.cpp`, `LocalFlightControlStateMachine.h`, `SharedShipPhysics.cpp`, `SpaceState.cpp`, `ClientHudPresentation.h/.cpp`, `FlightVectorIndicatorRenderer.cpp`, `TemporaryShortcutStrip.h`, locale JSON.

**Verification:** Source-level presence checks performed; native MinGW build/runtime test NOT performed by assistant. Need compile + manual game test: press chord off: main engine thrust zero, NumPad RCS Newtonian works independent of ER-5; restart: exact 10 s inactive then power returns; Assisted free-mode: rotate without automatically changing local velocity, +/- no response, numeric keypad translation works; Backspace again: bounded physical nose-to-velocity turn, ring returns blue, ordinary Assisted throttle resumes after alignment. Observe return alignment during near-zero velocity and under reactor shutdown. WARNING: restart countdown currently gates electrical output; it is not a 10-second temperature warm-up, and there is no actual reactor shutoff cooling state yet.

---

## 2026-10-22 — No automatic cold start; correct unpowered Assisted torque leak

User clarified: game always spawns ER-5 and main CR-1 engines ALREADY AT IDLE TEMPERATURE, and no cold-start countdown or shutoff mechanic is active yet. Startup/shutdown remains future explicit machinery. **Main commits** `00eb1f4` restores engine operating temperature initialization to descriptor nominal idle (CR-1 450 K); ER-5 previously fixed to nominal idle (900 K).

User rightly reported residual rotation despite cold ER-5 zero power. Root cause from `ShipCore::updateMotionControl`: `evaluateAttitudeControl` mutates pitch/yaw/roll rates BEFORE installed engine wrench runs; code rolls this mutation back and transfers it into `requestedMainAngularAccelerationBodyRadPerSec2` only inside an overconstrained Assisted topology predicate. If the predicate fails, virtual angular acceleration survives without engine power. **Main commit** `0b3356f`: all valid HubTactical Assisted constructions unconditionally translate the hypothetical controller delta into a motor angular-demand and RESTORE actual old rates before engine allocation. Engine power 0 therefore cannot produce angular torque via this path. Newtonian RCS path remains unchanged. No global damping bypass or free control torque added.

No native Windows build/game confirmation; validate zero-power engine gives zero Assisted turn while independent RCS still physically runs, and powered neutral pitch/yaw/roll control stops. If any residual, instrument and inspect the actual server tick motor moment.

---

## 2026-10-22 — CRITICAL FIX: main thrust dead on fresh spawn (main)

User video `20261022-1731-45.2307400.mp4` reports entire main flight/attitude control disabled while keypad RCS still works. Root-cause confirmed from source: ER-5 `EngineReactorState` defaulted to 2.725 K while `ShipCore::updateTranslationControl` scales generator electrical authority by `ready=clamp((T-2.725)/(idle-2.725),0,1)`. New game ships spawned directly into tactical flight without running a reactor startup procedure. Every main motor therefore initially had zero available electrical power; RCS uses its separate real executor. No pilot-input regression or missing bindings required to explain symptom.

**Fixed in main** `49ae1be`: `ObjectModuleRuntime::init` initializes each installed `engineReactor` at its descriptor idle temperature (ER-5 = 900 K). The existing continuous thermals/power/overheat limiting remain, but ships no longer spawn with disabled generators. A future cold-start mechanic MUST be activated by explicit reactor stop/start semantics; do not tie ordinary loaded ships to an uncontrolled 7-minute no-thrust period. Existing independently initialized main-engine temperatures left unchanged. Validate native build and manual pitch/yaw/roll/thrust along with Newtonian RCS; no game build executed through connector.

---

## 2026-10-22 — Assisted release jerk: preserve cross-reactor wrench balance

User reports manual center-of-mass attitude rotation brakes after key release, then visibly jerks into an unrelated acceleration, likely also shaking autopilot. Source analysis found in `ShipControlExecutionGateway::evaluateTranslationControl`: an Assisted six-axis program calculates a balanced motor wrench, but a subsequent per-ER-5 independent power-budget scaling changed fore/aft and across-generator engine forces by different factors and recomputed actual torque. New 7-minute cold-start/thermal power authority makes these group ratios naturally unequal, so this violated the physical command's precomputed balanced torque and added cross-axis moments. **Commit `0820bd2` in main** applies one conservative common minimum power scaling factor to ALL Assisted motors, preserving resultant force/torque direction and cross-axis cancellation while respecting every group MW limit; Newtonian retains independent power-group allocation. No free angular impulse, hidden attitude override, or damping changes. This is a targeted probable root-cause fix, **not native-built/game-verified**. Validate holding/releasing pitch/yaw/roll in Assisted during cold warm-up and at full generator readiness, and autoguidance straight flight; actual remaining oscillation may need a separate trace. Keep all work only in main.

---

## 2026-10-22 — Simplified balanced ER-5/CR-1 pad thermals (main; native build unverified)

User authorized replacing complex thermal calculations with a readable game model. Updated **main only**:
- `src/game/physics/UniverseThermalRadiation.h`: universal propulsion pad dissipation `PropulsionPadCoolingMWPerM2 = 6.0` MW per intact m² at design temperature; below design temperature cooling rises linearly from zero at the equipment idle temperature. Legacy generic ship radiators still use their original Stefan-Boltzmann law. No second propulsion pad multiplier.
- ER-5 passport: cold start ~2.725 K, idle 900 K, 420-second exponential warm-up to ~99% idle, rated electrical 5000 MW, efficiency 90%, heat capacity 600 MJ/K, safe 2100 K, critical 2400 K. **Dedicated square pad 11×11 m = 121 m² = 726 MW rated sink.** Generator hot standby =2 thermal MW but **zero electrical MW** without consumer demand. At steady full output, reactor waste is 555.6 MW and linear balance predicts ~1818 K, with ~23.5% rated pad spare capacity.
- CR-1: initially cold ~2.725 K, idle 450 K, exponential 300-second warm-up to 99%, rated 4850 MW and 98.55% efficiency; at full thrust ~70.3 MW heat, standalone round pad Ø5m =19.635m² x 6 = 117.81 MW sink. Safe 1250 K, heat capacity 70 MJ/K. Full-load equilibrium predicted ~927 K. No duplicated coolant sink for main motor heat. Thermal damage still uses existing damage layer and failure thresholds.
- `ShipCore::updateSystems`: ER-5 now reads actual physically executed engine electrical draw from its powered-engine slots and **dedicated reactor pad**. No longer subtracts available power from motor pads as if same emitter. Reactor thermal clock stores measured generated/pad/stored heat; no separate transfer to shared ship coolant in this simplified propulsion model.
- `ShipCore::updateTranslationControl`: actual power-group authority reduced by cold start readiness, heat above safe threshold, reactor equipment damage and age. The executor's existing power-limited engine allocator reduces delivered force physically, rather than only HUD color. ER-5 thermal tick computes same power authority.
- The propulsive full-load nominal thermal state is stable **by construction**; after pad damage/loss, cooling drops and equipment can overheat.
- No GUI or flight-law rewrites. Thermal balance uses deterministic and bounded formulas only.

**Important limitations / next test:** No Windows compilation, ctest or gameplay was possible through GitHub connector. Main-engine/ER-5 pad *independent physical hitboxes* and damage injection for the new reactor square pad are not yet implemented: existing reactor equipment failure/damage acts on electrical output, while its pad currently defaults healthy. The ReactorTick's thermal damage state still needs canonical runtime damage synchronization across ticks; do NOT claim this full damage loop is complete. Existing generic service coolant still operates for other consumers, but the simplified propulsion thermals intentionally use their own pads. Verify with native build: cold launch/idle warm-up, >10min 100% thrust, broken pad behavior when implemented, and HUD G/P/S in Assisted/Newtonian I/Y/X. Then check compile/runtime regressions and right-sized UI. Logs to D:/tmp.

---

## 2026-10-22 — ER-5 thermal balance and thrust-angle HUD follow-up; straight-flight shake still unresolved

User reported ER-5 temperature fixed, heat dissipation/overheat risk invisible, maneuvers only looking like accelerate/brake, gimbal angle not visible, and visible straight-flight ship shaking. Attached `elite_docking_planner(20261022-144221).log` is a ROUTE BUILD diagnostic, not per-frame actual control/pose trace: contains route-worker-ready and long route geometry but no nozzle/yaw/pitch/angular-rate or thruster state samples; cannot attribute observed shake to planner or claim it fixed.

**Implemented on main:** added factual per-ER-5 `lastPadHeatMW`, `lastCoolantHeatMW`, `lastStoredHeatMW` persisted in installed reactor state after thermal tick; exposed via `ShipCoreStatus.engineReactors`, `MotherComputerCockpitView`, and engine HUD projection. Active HUD displays `G` generated MW, `P` radiated via main-engine pads MW, `S` stored MW (positive heat accumulation risk) and safe-temperature margin. Enlarged instrument slightly to make room and increased visible nozzle angular magnitude text. Crucial correction: `collectCockpitView` now reads executed motor thrust from `ship.transform.motion` instead of interpolated `ship.renderTransform.motion`, matching existing actual-gimbal-angle source; never infer thrust by speed change. **No changes to physics damping/autopilot or planner** because runtime cause of straight-flight shake is not established. For Assisted with two healthy solid 5m pads, 2500 K can legitimately remain constant if P >= G: heat-flow figures now explain this. Existing ER-5-to-common-coolant flow is still zero by design until transport is physically modeled; pad thermal margin and genuine overheat derating remain incomplete.

This is CODE ONLY: no MinGW game build, CTest or runtime reproduction performed. Next: compile, inspect HUD at I/Y/X including crowded four-group rows; capture time-series executed per-motor thrust/yaw/pitch, angular rate/accel, requested angular accel, stabilizer demand and interpolation/camera motion on a STRAIGHT run (with autopilot on/off); diagnose and fix actual shake based on those traces. Do not declare shake fixed on current evidence. One branch only: main.

---

## 2026-10-22 — MotherComputer engine telemetry: executed thrust vector, real RCS firing, ER-5 thermal tick (source, NOT compiled)

Commits on **main only**: `5311db0`, `1581033`, `bb102d7`, `95ed34d`, `aaa04d5`, `89e2b17`, `fee260e`, `494c5ac`, `5ad8b4e`.

- Assisted HUD left/right main bars continue to display **actual executed thrust**, INCLUDING angled/vectoring maneuvers. Under an actively thrusting bar, display the measured absolute nozzle deviation angle in degrees from `EngineLiveTelemetry.nozzleYawRad/nozzlePitchRad`. Angle is a concise magnitude, not signed yaw/pitch. No reconstructing thrust from pilot controls.
- Newtonian RCS HUD: use actual 24-nozzle `allocateNewtonianRcs` executed `m_newtonianRcsDelivered01`, exposed via `ShipCoreStatus.newtonianRcsExecuted01[24]` and validity. Aggregate eight **physical** node IDs from `CobraNewtonianRcsNozzles.h`, each three consecutive nozzle motor identities; independently measure per-module damage (minimum functioning nozzle capacity is conservative station health) and show an orange activated lamp when any of the station's three real nozzles executes >0.005 duty. Supersedes erroneous 4/6/8 generic physical-control-slot grouping for instrument. The *separate control-slot topology* remains unchanged for now; no physical Newtonian flight-law modification was made.
- ER-5 updateSystems now runs `advanceEngineReactor` on each installed generator state, using measured **executed motor electrical MW**, healthy/damaged pad emission rating less motor waste heat as conservatively reserved capacity, and existing finite thermal capacity. Hot standby generates heat even at idle. getCoreStatus already forwards resultant reactor temperature, power and generated thermal MW through existing motherboard status; reactor HUD shows measured core temp K, orange until safe limit, red above safe, dark if unmeasured/faulted. No fabricated ship-services generator identification.
- **Known physical/telemetry limits:** ER-5 to common coolant transport is deliberately passed as **0 MW** until real separate generator-to-coolant transfer capability is authored; remainder heats its own finite thermal mass. The reactor operating history thermal damage currently is not reconciled into canonical ObjectModuleRuntime equipment damage; overheating therefore DOES NOT YET derate supplied electrical authority. Pad color thermal-load margin remains unknown; do not present green/yellow/red pad thermal margin as completed. No complete thermal-path energy audit, MinGW compilation, or game verification performed. In addition, grouping RCS firing uses latest `ShipCoreStatus` generated by system-tick from most recent actuator output; may be decimated for distant ships.
- Follow-up: run Windows build; correct any compile errors; check assisted angle with turning, 8 Newtonian nodes light individually on translational/rotational RCS, ER-5 core values at idle and load; implement physically bounded coolant transfer and thermal derating separately. Logs in D:/tmp. No additional tests for their own sake.

---

## 2026-10-22 — Single mainline policy (user's binding directive)

**Only `main` is authorized for all subsequent development and commits.** Do not create additional feature, test, diagnostic, release or restore branches. Do not claim any side branch is the current working branch. All normal work, including ER-5 physics/telemetry and cockpit widgets, remains on `main`.

GitHub ancestry audit on 2026-10-22: recent `feature/assisted-engine-out-axis-2026-10-21` is already **fully contained** in `main` (zero commits ahead). All tested prior feature, restore, test and baseline branches likewise have zero unique commits. TWO obsolete experimental branch tips retain distinct commits not in `main`: `diagnostic/assisted-safe-course-frozen-ab-2026-10-16` (2, modifies recovery algorithm and adds diagnostic probe), `experiment/traffic-route-graph-2026-10-08` (22, changes outdated planner). Their heads are **not merged**: replaying these old algorithms would replace newer, accepted navigator behavior. They are retained as historical records, not development targets. No branch force update or deletion was performed. A safe history-only merge with unchanged main tree would require authoritative tree SHA; connector's create_tree rejects using a commit SHA as base. Do not claim all branch histories are merged until that is resolved, but do not resume work outside main.

---

## 2026-10-22 — Per-ER-5 telemetry relay to MotherComputer; live source still pending

Commits `49ce46b` and `da4c464` on main: `ShipCore::getCoreStatus` now publishes one `EngineReactorStatus` per bound installed ER-5 module via existing `ShipCoreStatus.engineReactors`, and the existing `MotherComputerClientAdapter::collectCockpitView` looks up matching moduleId to feed the reactor square's genuine temperature/safe limit. No new RPC, duplicated status bus, dummy temperature or ship-services reactor confusion. Source remains guarded: zero/uninitialized status must not be interpreted as real ER-5 core heat.

**NOT COMPLETE:** This only connects the telemetric pathway. `ShipCore::updateSystems` still does not invoke `advanceEngineReactor`; no continuous, live temperature or output readings yet. The preexisting `EngineReactorState` runtime stores electrical output and heat only WHEN actually ticked, and the HUD should therefore retain `-- K` until the real generator service tick is integrated. Thermal emitter headroom and pad temperature-based color are also not yet measured. Avoid presenting this wiring alone as delivery of reactor telemetry. Next: authoritative per-installed ER-5 service tick using executed motor electrical output, real residual pad radiative capacity, bounded common coolant overflow; then validate build/game. Latest edits not compiled locally through GitHub.

---

## 2026-10-22 — ER-5 missing HUD telemetry diagnosed; partial source groundwork only

User confirmed drive reactor temperature does not reach HUD. Root cause verified: `ShipCore::updateSystems` still does not advance installed ER-5 thermal state; `ShipCore::getCoreStatus` reports only the 70MW ship-service reactor; `MotherComputer` has no actual ER-5 temperature. Do NOT represent an ER-5 nominal 2500K value as measured temperature.

Commits `23cdda8`, `b25e052`, `d80d43a` add per-installed engine reactor mutable state in `ObjectModuleState`, a separate `ShipCoreStatus::engineReactors` DTO, and last measured output/heat fields in `EngineReactorState`. **This is incomplete wiring, not working new telemetry.** A further attempt to update `ShipCore.cpp` was not applied (tool rejected request). The renderer should continue to mark ER-5 temperature unknown until the complete authoritative chain is connected.

Next: once only in `ShipCore::updateSystems`, tick each installed ER-5 from delivered `motion.engineTelemetry.electricalMW`, calculate remaining pad radiation headroom after actual motor waste, and handle finite reactor-to-shared-coolant heat transfer without introducing fictional extra pump. Then publish per-reactor status in `getCoreStatus`, map module IDs to `MotherComputerCockpitView.drives` and to the actual HUD reactor squares. Confirm Windows build/game behavior. Newtonian single pad cannot reject nominal full generator waste. No native build run. Work on main only.

---

## 2026-10-22 — Assisted HUD motor sides fixed; Newtonian eight display positions

Commit `0dbbf4f` modifies only `src/render/cockpit/EngineThrustIndicatorRenderer.cpp` on `main`. Assisted fore/aft drawing corrected to user convention: **aft on LEFT**, **fore on RIGHT** of each ER-5 square, with both thrust fills starting at the square and proceeding outward. Newtonian instrument reserves **eight RCS circles** across I/Y/X; positions beyond the actual installed maneuver clusters display dark/unavailable, NOT fabricated working nozzles. Actual `CobraPropulsionPresets.h` defines four `ManoeuvreCluster` slots for Newtonian-I, six for Newtonian-Y, eight for Newtonian-X. Those are distinct from 24 detailed Newtonian nozzle units; no physics or equipment counts were changed. A future design decision is needed if eight truly independent indicators are required for every construction rather than eight visual locations. No build/game verification performed via GitHub; ask user to compile and send screenshot.

---

## 2026-10-22 — User's actual paired-reactor propulsion HUD implemented (source; native validation pending)

Replaced legacy vertical engine thrust bars' ACTIVE motherboard-driven rendering with the user-specified horizontal installation diagram, committed DIRECTLY to main in `a5f8cfb`, `af0df38`, `2c958fa`, `77b34cc`, `7f2da83`. Updated ONLY MotherComputer cockpit DTO/adapter, existing engine presentation adapter, and existing `EngineThrustIndicatorRenderer`; no extra telemetry channel, service, standalone widget, autopilot or ship physics introduced.

- Header: existing engine fault symbol LEFT and physical Assisted/Newtonian I/Y/X text on same line; panel wider (260 reference pixels) with fixed 134 reference-pixel height, bottom aligned. Companion ship-services REACTOR/COOLANT widgets shift right of this width; they are NOT the drive's ER-5s.
- One compact horizontal row PER ACTUAL `CobraInstallationSlots::engineReactors` powered-engine group: Assisted I/Y/X = 2/3/4 ER-5s, fore and aft motor bars extend LEFT/RIGHT from centered reactor square; Newtonian I/Y/X = 1/3/4 ER-5s, reactor on RIGHT with one left-extending motor bar. Bars shorten with the already-reported effective motor availability and show measured executed thrust starting at reactor toward outer end. Per-engine pad circles and their percentage use replicated `emitterIntactArea01` on native installed modules. Reactor square fills proportionally to observed accumulated five-cause reactor equipment integrity; destroyed/missing/diagnostic unbound groups cannot fabricate health.
- Newtonian bottom dots represent ACTUAL current `ManoeuvreCluster` topology: I=4, Y=6, X=8. Earlier user's generic wish of eight circles cannot be made factual for I/Y without inventing extra actuators. Available replicated module damage drives fill; unavailable data is dark/unclassified.
- **Telemetry limitations:** Installed ER-5 live core temperature NOT yet replicated or computed in actual service tick. The dashboard prints `-- K` and uses noncommittal dark-blue reactor body until this exists, rather than fictitious 2500 K/working-orange. The true pad **heat dissipation margin** is also not replicated; its outline remains neutral/amber, and percentage is actual pad surface integrity, NOT an invented green/yellow/red thermal-balance status. Once ER-5 runtime and its authoritative temperature/heat-availability telemetry are implemented, drive the agreed dark-blue/orange/red/black reactor modes and green/yellow/red/black pad modes strictly from physical data.
- Existing `MotherComputer` is the ONLY client cockpit feed; new DTO facts are collected in `MotherComputerClientAdapter.h` from matched ship modules and immutable installation topology. MotherComputer already receives physical motor delivered output, heat/cooling systems and BITE; no fresh network RPC.
- Source changes committed; no Windows MinGW build, CTest or screenshot/game test available through GitHub. Validate compiler first, then Assisted I/Y/X and Newtonian I/Y/X visually, ensuring no overlap at 1080p, reactor groups track installed topology and Ctrl+F9 diagnostic layouts clearly show unknown engine-reactor damage/temperature. Correct genuine build/runtime failures only; logs D:/tmp.

---

## 2026-10-22 — HUD geometry updated, existing MotherComputer feed retained (native validation pending)

Commits `ecf9c75`, `9141bdd`: minimal edits only to `src/render/cockpit/EngineThrustIndicatorRenderer.cpp`. Engine fault icon is now left of the Assisted/Newtonian I/Y/X model label on the SAME header row; width accounts for both and the original bar baseline/panel height remains. The thermal-system renderer's engine-width estimate was updated identically, retaining the adjacent REACTOR and COOLANT layout. Reactor chain (core→pump→coolant) now scales horizontally to its available width, with reactor and radiator warning lamps BELOW the pump and coolant glyphs instead of at the far right. Cooling panel, actual engine fill colors and source telemetry were not modified.

**Computer connection:** This is already implemented: `SpaceState.cpp` publishes a cockpit snapshot into the sole client `MotherComputer` and renders the engine HUD from `buildEngineThrustHudPresentation(cockpit)`, thermal HUD from `cockpit.systems`. No new UI data provider, duplicate BITE pipeline, engine controller or network fields introduced. IMPORTANT: current system reactor HUD refers to the separate legacy 70 MW ship-services reactor, NOT the installed 5 GW ER-5 generators, whose per-reactor thermal telemetry is not yet available. Do not display fictional ER-5 temperature/efficiency.

**Validation boundary:** Source edits committed directly to `main`; no Windows MinGW build or in-game screenshot was run. Inspect alignment for Assisted I/Y/X and Newtonian I/Y/X and warning icon placement at native screen resolution. Reactor ER-5 live thermal integration remains separate pending work.

---

## 2026-10-22 — Stage 4 preparation: ER-5 stored-heat recovery (source only)

Commit `efd77c7` updates only `src/game/equipment/reactor/EngineReactorUnit.h`. Existing `advanceEngineReactor` previously stored un-rejected reactor heat but never released it when engine load fell. It now returns above-operating-baseline stored heat through ONLY the remaining engine-pad radiative headroom and bounded shared-coolant transfer headroom, without spending sink capacity twice. The reactor state keeps finite thermal capacity and cannot radiatively cool below its authored operating baseline by this discharge. Generator operating conditions, engine/ship control, and HUD were not changed.

**Validation boundary:** Code committed directly to authoritative `main`; no native compilation or dynamic game test was run. Crucially, this fixes the standalone thermal primitive ONLY. `EngineReactorState` is NOT yet persisted per installed `ObjectModuleState`, and `ShipCore::updateSystems` does NOT yet call `advanceEngineReactor`; reactor thermal generation/temperature still has NO live effect on ship coolant, propulsion derating or HUD. Do not call Stage 4 complete.

**Next focused step:** Persist one reactor operating state on each installed module, advance it once per service tick using actually delivered per-motor electrical telemetry, subtract the engine's own emitted heat before assigning remaining pad capacity, deposit bounded reactor overflow into the existing shared `ThermalSystem` before `CoolingSystem::update`, and make authoritative power allocation read thermally available output. Resolve the Newtonian single-pad heat deficit without inventing heat disappearance. Preserve one main branch and single root state file. Tests should check energy balance at load, overload and recovery; logs in D:/tmp.

---

## 2026-10-22 — Stage 3: real installed propulsion-reactor power limits on main thrust

Commits `d5546965` (gateway power group definition), `c9b9ecd4` (enforce budgets on real physical motor force/torque/telemetry), `548b11ce` and `b7941b41` (construct powered groups by actual installed reactor modules and fail closed for missing bindings).

`ShipCore::updateTranslationControl` now resolves `CobraInstallationSlots::engineReactors`, each exact set of powered slot IDs, plus corresponding `ShipDescriptor::powerThermalInstallation.engineReactorModuleIds` and live `ObjectModuleRuntime` reactor state. For a non-attached/destroyed/missing reactor output = 0. For attached reactor max electrical power is the installed ER-5 passport 5000MW times the common equipment damage function's authority, calculated from its five accumulated damage causes. Healthy Assisted-I has 2 shared groups (fore/aft); Assisted Y/X 3/4; Newtonian I/Y/X each main has its own generator. The installed-power group array is passed into `ShipControlExecutionGateway::evaluateTranslationControl` main motor execution overload.

Inside gateway after regular Assisted maneuver program or Newtonian engine-matrix execution, delivered main motor samples sharing a reactor have their simultaneous requested electrical MW summed and scaled proportionally to available actual reactor MW, then **actual force vectors, six-DOF rigid-body force/torque and accelerations, engine delivered thrust, electrical MW, heat telemetry** are recomputed using physical slot lever arms and inertia. This power limiting runs before rates/acceleration are applied to ship motion, not after telemetry. No extra physics layer or new autopilot inserted. Missing reactor bindings enforce zero group output (fail closed), not free power. This is a main-motor authoritativeness seam only; RCS continues using independent ship control.

**Known limitations / next steps:** ER-5 reactor temperature, hot standby, per-reactor operating age and heat generation are NOT advanced in runtime yet. Available output is damage-based, not yet thermally de-rated. Generated heat has not yet been transferred through paired emitters into cooling system or retained in reactor heat capacity. Note currently there are no reactor-specific spatial hit volumes, and actual runtime damage response should be checked. Legacy ship-services reactor is now 70MW and world radiation law x10, with engine housing coolant conductance fixed to 0.14004 MW/K. No build/game test possible through GitHub connector; do not claim tested. Next step: persistent `EngineReactorState` per installed module, advance heat/recovery exactly once per service tick from actual delivered `engineTelemetry.electricalMW`; cap spare pad radiative capacity after engine heat, bounded common coolant transfer. Need ensure heating/cooling behaves at idle and cold start before implementing temperature-authority in motion execution. USER REQUIREMENT: change only what is needed, no invented parallel test/contract stacks. HUD and Assisted free-attitude deferred.

---

## 2026-10-22 — Minimal stage 2 prerequisites: common radiation and independent ship-service reactor

Commits `e41566c6`, `68c6330d`, `32cff214`. Global `UniverseThermalRadiation::RadiationEffectiveness` increased x2.5 -> x10 as agreed; affects BOTH pads and panels via single law. Fixed the CR-1 engine's housing-to-coolant conductance to explicit 0.14004 MW/K rather than recomputing it from emitter radiated capacity (which would make it ZERO after x10). The existing `ShipDescriptor::reactor` (old singleton ship-services reactor actually consumed by `PowerBus`) changed from 10000 to 70 MW nominal/peak. ER-5 modules were instantiated in prior stage 1, but this stage does **not** connect them to actual engine power allocation or thermal ticks yet.

Critical caveat: existing main propulsion force is executed before the engine-life service tick, where `mainEngineDeliveredThrust01` is only *observed*. Power allocation MUST be connected in execution gateway / main-force allocator rather than falsely clamping power post-hoc. Do not claim full energy/thermal mechanics works. Current 70MW ship-services bus load may need prioritization and pump power limits; test live behavior. Under x10, healthy 5m 2500K emitter ~352.3MW each, 2/pair ~704.5 MW; 5GW ER-5 at 90% loses 555.6MW plus 2MW standby and at most one full CR-1 loses ~70.325MW. For Newtonian one emitter cannot dump all heat: independent engineering decision required. No native build/test run. User asked minimal localized changes without test-for-test.

Next: trace main physical executor and install per-pair power allocation there, using actually installed damageable ER-5 modules; only then add generated heat/radiator headroom/finite overflow to coolant, and HUD. Avoid changing Assisted control law accidentally. Then investigate Free Attitude mode per requirement: stabilized Assisted should drive VELOCITY toward NOSE, not nose toward velocity.

---

## 2026-10-22 — Implementation stage 1: install reactor modules from six layouts

Implemented in Cobra descriptor `EliteCobraMk1.cpp` commit `0e5ab1a0`: after selecting physical construction including test-only I/Y/X override, `installationSlots(model)` instantiates a distinct `ModuleDescriptor` for each 5GW Cobretty Cobra ER-5 propulsion generator slot; these have stable `ship_<slotID>_reactor` IDs, generic CB frame parent only (NOT frame geometry overhaul), true reactor damage hit zone, internal equipment type, normal failure and maintenance passports, and optional `engineReactor` equipment passport. Binds each generated module ID into `desc.powerThermalInstallation.engineReactorModuleIds` in exact slot order. Two ship-service placeholders `ship_service_reactor` and `ship_common_cooling` similarly materialized and bound. All six layout cases thus have module identities in object module runtime, without pretending a reactor is a frame.

**Only stage 1 is authored; cannot claim runtime ER-5s generate or allocate power/heat yet.** `ShipCore` still constructs legacy single 10GW reactor for ship services, existing engines continue without a hard allocated electrical power limit. Thermal overflow into coolant, per-generator hot standby, 70MW ship reactor replacement, common universe x10 radiation factor, power-driven engine clipping, switchable Assisted velocity-to-nose mode, and requested reactor/engine HUD refresh remain TODO. Explicitly do NOT duplicate parallel autopilot or main engine control models. Module descriptors have no individual authored hit volumes/mesh locations yet; until those are added, do not promise reactor projectiles physically resolve to these devices. Structural frame geometry intentionally unchanged per user.

Next order: 1) real per-installed ER-5 runtime power/heat (model current engine power demand and per-pad headroom, call unit advance, accumulate coolant and stored heat with recovery), 2) 70MW separate ship reactor and coherent PowerBus requests/allocation, 3) universal x10 radiation and validated heat balance for Newtonian one-pad arrangement, 4) gameplay derating and hot standby, 5) DTO/HUD, 6) Free Attitude Assisted without cross-family law switches, 7) end-to-end in-game scenario. Native MinGW build not run for stage 1 yet; ask user to pull and compile, and inspect any errors.

---

## 2026-10-21 — COBRA ENGINE REACTOR EQUIPMENT TYPE + ER-5 PRESET (NOT YET INSTALLED)

Created `src/game/equipment/reactor/EngineReactorUnit.h`: reusable dedicated engine-generator passport/state/tick. Per-installation state has temperature, equivalent operating hours, the same `EquipmentDamageState` five causes (laser, impact, thermal, radiation, wear). `advanceEngineReactor` caps real electrical output by damage, age and failure; applies a separate modest efficiency-loss law, produces rated-load waste heat as electrical output*(1/efficiency-1), has 2MW baseline hot-standby thermal loss, no time to start/shutdown. It allocates thermal output to explicitly supplied *remaining* engine pad radiation headroom first, then through bounded overflow to common coolant, retaining the rest in finite reactor thermal capacity and accumulating heat damage. It is a standalone function, NOT yet invoked in ShipCore physical runtime. Thermal exchange modelling still needs correct runtime engineering and safe recovery of stored heat during low-load periods; automatic high-temperature shutdown/de-rated power policy not yet complete.

Created `src/game/equipment/reactor/presets/CobraCobrettyER5PropulsionReactor.h`, rated 5000 MW, .90 conversion efficiency, 2MW standby heat, 2500K operating, 2750K safe and 3200K critical, 180MJ/K, 30k equivalent-hour overhaul and 50k end of life, damage/life/repair policy. Added optional `engineReactor` passport to `ModuleDescriptor`, so module-installed damageable reactor identity can reuse the same existing damage pipeline. Renamed Cobretty CR-1 **display** to `Cobretty Cobra CR-1 Vector Main Drive`; its stable type ID and file path unchanged.

**Thermal-budget issue that MUST be solved before activation:** a 5000 MW electric generator at 90% efficiency makes 555.56 MW of heat plus 2 MW hot standby when loaded. One 5m 2500K engine pad at current x2.5 law radiates 88.07 MW capacity; two radiate 176.14 MW capacity, BEFORE subtracting engine's own 70.325 MW per fully driven engine. Even if universe factor changes to x10, two 5m pads radiate 704.5 MW total *but* at peak motor thrust one engine itself generates 70.325 MW, so available excess ~634 MW and generator's 557.6 MW fits at rated pad temperature only for Assisted fore/aft pairs. In Newtonian ONE engine pad at x10 radiates 352.3MW total, insufficient to dispose of reactor's ~555.6MW + motor waste. This is a physics limit, not a reason to fake disappearance of heat: Newtonian requires bigger pad/other emitter, better reactor efficiency, power derating, or additional sink. The proposed global radiation factor x10 has NOT been committed into UniverseThermalRadiation.h and current actual factor remains 2.5. Avoid presenting the new equipment as playable before wiring it to layout module slots, real Engine::execute power consumption, damage runtime and cooling buses.

Commits 0a16b371, a54652f9, cdf97877, 13006b12. No native build/test executed. Do not manufacture test claims. User wanted one engine reactor per Assisted fore/aft pair and one per Newtonian main, plus independent 70MW ship reactor, but current ShipCore still has previous 10 GW legacy reactor until a separate physical integration step.

---

## 2026-10-21 — SIX LAYOUTS: ENGINE REACTOR AND COMMON SERVICES SLOTS

Architecture decision: physical layout model is the functional installation skeleton WITHOUT hull frames. Each ship descriptor selects one of six Cobra I/Y/X Assisted/Newtonian models and binds actual equipment to the layout slots. Remaining ordinary equipment stays descriptor-defined and connects via standard PowerBus and cooling circuit.

Added to existing `CobraPropulsionPresets.h` alongside all six canonical propulsion layouts: `CobraInstallationSlots`, with `EngineReactorSlotDefinition` (stable slotId, normalized body position, powered main-engine slot IDs), plus `shipReactor` and `commonCooling` service slots. `installationSlots(PhysicalControlModelId)` covers Assisted I/Y/X with 2/3/4 fore/aft shared-reactor pairs; Newtonian I/Y/X with 1/3/4 dedicated aft-main-engine reactors, no reactor assigned to RCS clusters. The L/R, top/bottom pairings are explicit. All positions are relative to current mass center when converted to physical distances; provisional normalized placement, no frames rearranged.

Added `ShipPowerThermalInstallationBinding` to `ShipDescriptor.h`: engineReactorModuleIds ordered by existing installation slot topology, plus shipReactorModuleId and commonCoolingModuleId. Blank strings mean uninstalled/not yet functional; this phase does not invent actual reactor equipment.

Design intent confirmed: each engine-reactor module feeds only its assigned engine(s). Reactor plus engines reject waste heat through the associated emitter pads; if radiator pad is degraded or overloaded, surplus heat must go through a *bounded physical transfer* into the shared ship coolant. Other ship equipment connects to the separate 70 MW ship reactor and common cooling using descriptor/equipment slots.

Commits f0f5ac5f, f09885da. IMPORTANT: **ONLY topology and descriptor binding declared so far. No real EngineReactor runtime devices, per-reactor generated electrical power, thermal transfer equations, bus arbitration, or installed damageable module IDs have yet been created.** Existing legacy single `ReactorSystem` in ShipCore remains and carries the interim 10 GW tuning from before this design; this needs conversion to ship services 70MW plus layout-specific generators. Never describe the new reactors as live. No build/game test yet.

---

## 2026-10-21 — SOLID 5m CR-1 HOT PADS / ASSISTED-I REACTOR EFFICIENCY REBALANCE

User authorized Cobretty CR-1 motor emitter replacement: full SOLID circular 5.0m disk, 0m aperture, existing 2500K nominal emitter surface, emissivity .9 and viewFactor .9; universe radiative effectiveness 2.5 unchanged. Per full-rated engine, electrical input 4850MW, .9855 nominal efficiency => 70.325MW waste. At 2500K new 19.635m² radiating disk has 88.07MW radiative *capacity* (NOT actual waste generated); formerly annular 10.996m² and 49.32MW capacity. Nominal engine direct radiation now limited to its 70.325MW heat produced, therefore zero conduction to shared coolant for healthy steady operation and ~42.012MW relieved from coolant for two full-thrust Assisted-I aft motors. **IMPORTANT** Preserve existing finite engine-to-coolant thermal conductance based on PREVIOUS annulus surplus: if dynamically recomputed as max(0,waste-new-radiator)/deltaT it would become ZERO, making damaged emitter incapable of heating coolant. Preset explicitly computes previous fallback conductance while assigning new solid disk. Thermal path and damage behavior must still be native-tested. The physical rendering asset ID stays `engine_hot_emitter_annulus` pending visual mesh update; physical geometry updated but art may still show aperture.

380m² panels at 1050K nominal, emissivity .9, universe x2.5 reject 58.93MW; at 10GW reactor rated electric max, choose efficiency values: cold boundary 99.60%, optimal 99.65%, hot boundary 99.55%. Reactor waste at 10000MW = 40.16 / 35.12 / 45.20MW respectively, leaving ~18.77/23.81/13.73MW for other equipment and pump at 1050K assuming zero heat to coolant from healthy 100%-thrust engines. Efficiency is a FICTIONAL gameplay balancing parameter, NOT physics of actual nuclear generators. Does not certify 10GW electrical bus supply to engines; engine energy clipping is not connected yet. Panel degradation, hot reactor loss, pump failures, damage, low coolant temperature and simultaneous additional motors can eliminate this reserve.

Commits `65a0a5b7`, `60aec4b8`. Changes authored remotely; **not compiled, not in-game tested, no PASS**. Ask user to pull branch, build EliteGame and inspect nominal thermal IN/OUT, pad degradation scenario, and visual emitter geometry. Reactor temp initially 3500K as previously committed. No second radiator pump.

---

## 2026-10-21 — Reactor damage efficiency runaway corrected

At 10 GW electrical capacity, previous ReactorSystem::getEfficiency01 multiplied conversion efficiency by structuralIntegrity. Even 1% damage changed 99.95% to ~98.95% and raised full-load waste heat from ~5 MW to ~105 MW, exceeding 380 m2 radiator capacity. Fixed in commit 6e7d1d69: integrity still derates available electric output; independent small bounded damage-related conversion penalty (up to 0.0003 absolute efficiency loss at full structural damage) replaces proportional efficiency multiplication. Temperature curve remains independent and can reduce efficiency, and hot panels can degrade. This fixes a catastrophic mathematical coupling, not a certified thermal or electrical supply simulation. Build and playtest pending. The 10 GW main-engine consumer is not yet wired into PowerBus. Never claim thermal equilibrium without real load measurements.

---

## 2026-10-21 — HUD TEXT BOTTOM ANCHOR FIX (SCREENSHOT FOLLOW-UP)

User screenshot showed REACTOR and COOLANT textual telemetry overlapping graphics after compact thermal HUD update. Changed `src/render/cockpit/EngineThrustIndicatorRenderer.cpp` at commit `a24797c7`: BOTH widgets' status text Y positions now derive from common actual panel height `y+h-offset*scale`, not fixed `y+offset*scale`. Reactor last 3 lines at h-52/h-34/h-16; coolant 4 lines at h-63/h-47/h-31/h-15. Moved reactor chain and radiator drawing higher to leave text-only bottom area. No physics or telemetry changed. Needs actual screenshot and MinGW compilation verification. Run git pull and EliteGame build as before.

---

## 2026-10-21 — REACTOR / COOLANT COMPACT HUD, PHYSICAL RADIATOR OVERHEAT, REPEATING ALARMS (SOURCE ONLY)

User game screenshot showed working tiny ASSISTED-I/REACTOR/COOLANT HUD and requested: panel graphic on COOLANT with captions below, panel & coolant temperatures, functional panel percentage, heat IN and OUT; REACTOR panel reactor-column→line→square pump (amber when energized)→line→coolant square, real power and temperature/damage-dependent efficiency, distinct reactor vs radiator alarm lights; all active system fault texts repeat in docking terminal every 10 seconds, recovery message when resolved; radiator panel damage when exceeding material critical temperature.

Implemented in branch:
- `CoolingSystem::update`: uses radiator material catalogue `damageOnsetTemperatureK` and sustained dt to reduce each panel's health/efficiency, hence area and Stefan–Boltzmann rejection. Degradation gameplay tuning provisional rate 0.0001 health/second at onset scaled by excess (do not state validated). Model panel surface temperature still assumes coolant temperature, not a finite-panel separate node.
- `CoolingSystem`: exposes `isPanelThermalCritical` and `getPanelDamageOnsetK()`. `ShipCoreStatus::CoolingStatus` carries panel health percent (average effective surface health, not literal integer count of fully intact plates) and critical flag. `ShipCore::getCoreStatus()` produces radiator thermal damage warning from identical material limit, plus reactor damage/low-temperature warning; preexisting high reactor/coolant warnings retained.
- Reactor `getEfficiency01()`: explicit interpolated 3-point efficiency curve vs currently active runtime operating range, reduced by structural damage; same live efficiency now applied to `getHeatGeneration()`. Passport efficiencyMin/Peak/Max sourced from Cobra ReactorDescriptor, BUT baseline runtime power remains hardcoded 70MW and active temperatures 1300/1400/2000K, NOT migrated to descriptor 2500/3500/4500K or 3200MW peak. This avoidance of unsanctioned power rebalance must be explicit, and full power-consumer bus integration still pending.
- `EngineThrustIndicatorRenderer.cpp`: tiny reactor icon→connector→pump square→connector→coolant box; pump turns amber only if requested/allocated. Blink lamps for radiator overtemperature/damage and reactor damage/thermal faults. Bottom text actual reactor K, generated electric MW, dynamic efficiency %. COOLANT displays expanded single radiator, approximate surface `P` and actual coolant `C` K, average panel effective health % `OK`, and measured coolant IN/OUT MW. No independent panel thermal mass.
- `SpaceState.cpp`: opens system terminal when BITE or systems.alerts are active, injects reactor/coolant/power/radiator alerts into canonical diagnostic stream.
- `DockingStatusTerminal.h`: fault messages immediately when first seen, repeat while active at >=10s, announce recovery once; no repeated cleared alarms. This operates in existing terminal presentation layer, not on server.
- Commits include `f5223bb0`, `858274a8`, `00153dad`, `85527e31`, `212ad34d`, `52e8a096`.

**NO BUILD OR IN-GAME TEST OF THESE CHANGES THIS TURN.** User: `git pull --ff-only origin feature/assisted-engine-out-axis-2026-10-21` and `cmake --build build --target EliteGame -j 8`; screenshot and compare panel temp/crit alarm, panel OK percent decline above material onset, real coolant IN vs OUT, terminal reminders. Engine electric MW still not a powered-bus consumer, so current 70 MW is NOT proven sufficient for main engines; do not simply inflate peak power without real electrical allocation. Reactor operating thresholds not fully descriptor-driven. NPC simplified plant still future. Do not add a second radiator pump; only one independent reactor pump.

---

## 2026-10-21 — REACTOR PUMP / RADIATOR TOPOLOGY CORRECTION + LIVE COOLANT HEAT BALANCE (SOURCE, NOT YET BUILT)

User clarified there is ONE special external reactor heat pump, with hysteresis controlled by reactor temperature. Installed engines/equipment transfer heat using their internal cooling loops into ONE common circulating coolant, which directly bathes the radiator panels. There is NO second radiator pump to model. Separate pump #2 proposed earlier is explicitly superseded. The reactor's pump belongs conceptually and visually in REACTOR HUD, while COOLANT HUD displays only a large aggregated panel pictogram, effective panel/coolant temperature, incoming heat and heat radiated (MW).

Fixed `CoolingSystem.cpp` so panel radiation no longer incorrectly depends on reactor-pump `pumpCapacity`; it is now bounded by panel radiation material/area and current sensible coolant energy. Corrected `pumpHeatMJ` to record the pump's electrical loss heat, not heat transport. This preserves the reactor pump's control of reactor→coolant only and keeps radiator radiation active when the reactor pump is off.

Extended `ShipCoreStatus::CoolingStatus::heatIntoCoolantMW` to read the LAST systems tick's observed positive thermal additions `ThermalSystem::getHeatVolume()/CoolingSystem::getDT()`; reflects energy delivered INTO the coolant (reactor pump delivery + internal equipment heat + electric pump losses), not the full instantaneous waste heat physically produced and stored INSIDE reactor and engine units. `radiatedPowerMW` was already measured by CoolingSystem. Do not label coolant input as total whole-ship waste-heat generation. It is actual COMMON COOLANT HEAT IN, exactly comparable to panel OUT for explaining coolant temperature trends.

`EngineThrustIndicatorRenderer.cpp`: moved reactor-pump pictogram into the compact reactor gauge, removed pump from coolant gauge, expanded one aggregate radiator rectangle, showing effective radiator temperature (current approximate surface=fluid assumption), `IN x MW` and `OUT x MW`. Kept compact 3-gauge sizing and model-dependent layout. Reactor HUD still lacks authoritative live efficiency, engine cooling-pad temps still unavailable. Changes committed through `403ac89a`.

**NOT BUILT OR GAME TESTED THIS TURN**. Native MinGW build and screenshot required; do not claim successful compilation. The reactor pump is still represented through old CoolingSystem controller, without independent EquipmentDamageLayer-backed module; user also wants its temperature-hysteresis and damage to be fully represented eventually. Full whole-ship generation measurement would require reactor generated heat + retained individual equipment heat before conductivity, with source ledger designed to avoid counting both production and coolant deposition; do not invent that number.

---

## 2026-10-21 — COOLING HYSTERESIS AUDIT AND SAME-TICK TELEMETRY/FLOW FIX (UNVERIFIED BUILD)

User reports coolant temperature rising, MW jumping, panel -- K, expects radiator pump to switch on/off by coolant temperature hysteresis. Source audit definitively shows `CoolingSystem::calcRequestedPower()` hysteresis is on REACTOR temp `workTemp±delta`, not coolant/radiator demand. Critical coolant-temperature latch actually shuts off the only current pump to avoid overtemperature; no independent radiator pump currently exists. One common allocated power drives both reactor→coolant circulation and coolant→radiator removal. Thus the intended two independently controlled pump architecture remains UNIMPLEMENTED and rise in coolant K can be caused by inadequate radiator capacity and/or current coupling.

Changes:
- `CoolingSystem::prepareTransport()` computes the present-tick pump heat transport from newly allocated bus power; called after `m_powerBus.update()` and before `m_reactor.update()` instead of previous tick's `pumpCapacity` (which previously lagged one step). `CoolingSystem::update()` shares this preparation.
- `ReactorSystem::update` now caps heat transferred into coolant by physically available reactor−coolant gradient and reactor thermal capacity (no heat pumping backwards / unlimited subcooling). Does NOT rebuild the reactor descriptor or complete actual pump efficiency and thermal network.
- `ShipCoreStatus::CoolingStatus` adds `reactorPumpActive`, `coolantOverheatLatched`, `radiatorPotentialMW`, `effectiveRadiatorTemperatureK`, all populated from existing live `CoolingSystem`. Effective radiator temperature is MODEL assumption equal to shared coolant temperature used directly in Stefan–Boltzmann equation, not a real independently modelled panel temperature; HUD says `PANEL ~... K`, never an unqualified measurement. HUD coolant MW now labeled `P:` for pump electric draw.
- NOT IMPLEMENTED: separate #2 radiator pump, its independent coolant-based hysteresis, individual damage/heat/energy, complete reactor config/KPД, main/RCS bus clipping, MotherComputer pump command path, finite panel heat capacity. Power fluctuations may still come from old proportional/hysteretic request + bus control and should not be hidden as verified fix.
- **No build/tests run in current environment**. User must `git pull --ff-only origin feature/assisted-engine-out-axis-2026-10-21` and `cmake --build build --target EliteGame -j 8`. Check coolant/radiation/pump figures and relay state in-game. For actual two-pump behavior, next implement reactor and radiator pump equipment separately and appropriate controller.

---

## 2026-10-21 — COMPACT THERMAL HUD SIZING UPDATE (SOURCE ONLY)

User game screenshot confirmed first compact reactor/coolant widgets rendered but were shorter/narrower than engine panel, labels cramped, and position must follow engine panel width for I/Y/X. Updated `src/render/cockpit/EngineThrustIndicatorRenderer.cpp` commits `0c9bc59b`, `ddc28d7e`. Reactor and coolant frames now use exactly the engine widget height expression (padding/icon/iconGap/label/labelGap/bar/padding), 142px-at-reference auxiliary width vs previous 110px, 9px gap, larger fonts and separated captions. Reactor X now computes actual content-derived engine width from groups, per-bar widths/gaps, padding, icon and measured text width (not estimated constants), then coolant follows reactor width. Top and bottom align through same bottom anchor; Assisted I/Y/X adaptation applies without duplicating engine widget UI. **Not yet built or screenshot-tested by user after these changes.** Verify with `git pull --ff-only origin feature/assisted-engine-out-axis-2026-10-21`, `cmake --build build --target EliteGame -j 8`; inspect all I/Y/X and collect screenshot. Do not claim verified before runtime screenshot.

---

## 2026-10-21 — COMPACT ENGINE / REACTOR / COOLING HUD SOURCE WIRED (BUILD/SCREENSHOT PENDING)

User specifically rejected full-sized cinematographic panels; HUD must match existing tiny `ASSISTED-Y` lower-left amber bordered engine gauge, with two small neighboring widgets and colored thin strips beside EACH thrust bar.

Source modifications:
- `EngineThrustHudPresentation.h` adds per-bar measured engine operating temperature and safe-temperature threshold; pads/emitter are explicitly marked unavailable (no actual emitter temperature measurement in replicated `ObjectModuleSnapshot`).
- `MotherComputer.h::MotherComputerEngineHudSlot` + `MotherComputerClientAdapter.h` pass actual native installed engine `module.engineTemperatureK` and descriptor safe temperature via MotherComputer cockpit view; synthetic diagnostic slots remain unknown, never fake measured values.
- `EngineThrustIndicatorRenderer.cpp/.h`: draws thin track left of each motor bar for pad/emitter (dark until independent authoritative pad temperature exists), right of each motor bar for engine operating temperature with blue→green→red fill. Uses existing amber widget scale/background, preserves original yellow thrust capacity/red active thrust bars. Adds two adjacent compact rounded amber widgets: reactor temperature fill and output MW; coolant flask→pump→single aggregate radiator pictogram, coolant K and allocated pump MW. Reactor efficiency and panel temperature show `--` because `ShipCoreStatus` doesn't supply reactor instantaneous efficiency or panel material temperature. Widget position accounts for engine group width of I/Y/X to avoid overlap, and hides when insufficient free lower-left width.
- `SpaceState.cpp` calls `renderThermalSystems(cockpit.systems, engineThrustInstrument, vp)` in cockpit pass. Same MotherComputer verified ship systems snapshot, no direct server reads.

**SOURCE CHANGES ONLY: NOT COMPILED, NO SCREENSHOT OF ACTUAL GAME, NO IN-GAME HUD PASS.** The user needs MinGW build `cmake --build build --target EliteGame -j 8` and in-game visual check. If build fails, get compiler error. Future work: add reactor instantaneous efficiency and aggregate effective panel temperature to authoritative `ShipCoreStatus`, plus a real individual emitter-pad temperature telemetry value; do not invent values. Panels intentionally tiny, no new widget framework. Changed no physics or NPC simulation.

---

## 2026-10-21 — NEWTONIAN PHYSICAL RCS EXECUTION WIRED (FIRST UNCOMPILED INTEGRATION)

In isolated branch only, implemented first server-physical Newtonian 24-nozzle execution path:
- `ShipCore.h`: tracks Newtonian controller's requested RCS angular acceleration and actual 24-nozzle duty from the preceding physical-control tick.
- `ShipCore.cpp`: for native module-runtime Newtonian with ALL 24 physical RCS modules present, `updateMotionControl` converts the legacy Newtonian attitude controller's desired angular-rate delta into a torque request and RESTORES the angular rate (prevents free attitude acceleration). `updateTranslationControl` after existing main-drive gateway maps current requested legacy manoeuvre acceleration into physical body-force demand, calls `allocateNewtonianRcs` with 24 actual `MountedEngine` views, then replaces the legacy manoeuvre contribution to actual motion with the returned force and applies only produced angular torque. If hardware graph is inconsistent, zeros legacy manoeuvre acceleration rather than granting fictional RCS. Does not modify Assisted. Existing main-engine law retained.
- `ShipCore::updateSystems`: advances each installed RCS `EngineOperatingState` through existing `ObjectModuleRuntime::advanceEngineOperatingLife` based on actual physical throttle, including conducted heat, coolant and wear; destroyed nozzle has no delivered output. Uses one shared existing life/damage model, not special RCS damage.
- `tests/navigation_runtime/NewtonianRcsPhysicalTests.cpp` + CMake registration: one direct physical-nozzle test, initial normal impulse vs one unit fully impact-failed, verifies failed nozzle commands/output power/heat zero and wrench finite. This was not yet compiled or run.
- Commits `d1baecfa`, `26d9ed98`, `7e810fa6`, `b1edb3e5`, `37994441`, `c51faf3e`.

**CRITICAL RISKS / NOT FINISHED:** This is FIRST uncompiled native-only integration; must compile on user's Windows MinGW with CMake/Ninja and fix any errors. Must then check real runtime with selected native Newtonian (physical models constructed from chosen descriptor), not Ctrl+F9 synthetic diagnostic overrides; otherwise new branch intentionally not active and cannot test nozzle failure. Newtonian old `physicalControlSlots` still has 4/6/8 `ManoeuvreCluster` virtual authority; new 24-nozzle execution is a separate materialized bank, not yet fully replacing their capability-planner projection. Those virtual slots may overstate capability under damage, so proper signed control envelope and optional RCS telemetry still need finishing. Main engine array remains <=16, RCS separate bank. No per-nozzle electrical bus power clipping; MW generated for instrumentation only; no separate RCS HUD graphic. Existing `ShipBitePresentation` sees 24 additional descriptor engines and passes BITE via MotherComputer to terminal if canonical conditions are met, but UI behavior not tested. Collision hit volumes for each tiny nozzle are not yet mapped; direct damage through moduleId works. Thermal operating state and RCS updated under systems cadence rather than per-motion tick. No claims of fully verified six schemes or gameplay PASS.

**USER TEST START:** `git pull --ff-only origin feature/assisted-engine-out-axis-2026-10-21`; `cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja`; `cmake --build build/tests/navigation_runtime --target newtonian_rcs_physical_tests -j 8`; `ctest --test-dir build/tests/navigation_runtime -R '^newtonian_rcs_physical$' --output-on-failure`; then `cmake --build build --target EliteGame -j 8`. Only if green investigate real Newtonian engine and RCS damage, then six-configuration runtime matrix. Capture failure logs D:/tmp; never infer PASS from code edits.

---

## 2026-10-21 — NEWTONIAN COBRA DESCRIPTOR NOW INSTALLS 24 RCS MODULES (INTEGRATION PARTIAL)

Commit `b27b5993`: `EliteCobraMk1.cpp` includes `CobraRcsEquipmentAssembly.h` and adds all 24 unique `ModuleDescriptor` entries via `addNewtonianRcsEquipmentModules(desc.modules)` for each constructor-selected Newtonian I/Y/X BEFORE normal module-class/default normalization and server `ObjectModuleRuntime::init`. Duplicate IDs fail loudly. Assisted descriptors untouched. As a result Newtonian descriptor graph now declares real independent motor identities accessible to existing equipment damage/BITE scan when installed module snapshots are populated. Runtime validation is still pending; these modules have no physical hit volume association yet.

**Do not call work complete:** `physicalControlSlots(Newtonian*)` still uses legacy 4/6/8 omnidirectional clusters, `ShipCore` still excludes those from mounted motor execution/thermal wear, `DynamicMotionSystem` still generates old synthetic scalar RCS forces, and the new 24 nozzle allocator is not called by physics. Each RCS node's body coordinate is in the new preset but does not yet participate in live flight. Neither assembly, CTest nor game flight has run. Next integration step: route Newtonian commands to a 24-nozzle server-mounted RCS actuator bank with authoritative force/torque, disable old aggregate RCS acceleration/rate path, add thermal tick and independent telemetry and acceptance. A diagnostic physical-model override is NOT proof of damaged engine flight unless actual engine modules have been installed and bound.

---

## 2026-10-21 — PHYSICAL NEWTONIAN 24-NOZZLE ALLOCATOR ADDED; SIX-GAME VERIFICATION NOT DONE

Added `src/game/ship/plant/NewtonianRcsAllocator.h` commit `cf540490`. This new PURE bounded fixed-iteration 24-nozzle allocator takes actual `MountedEngine` pointers, physical demands for Fx/Fy/Fz/Mx/My/Mz, fixed centre-of-mass-relative hull half-extents in metres, returns individual throttle, real `Engine::execute` force and torque, actual MW and thermal MW. Capabilities are limited by each individual available engine's accumulated equipment damage. Algorithm bounded to 32x24 coordinate descent operations and zero per-tick dynamic allocations. NO new parallel damage engine. Pure geometry rank of 6x24 signed wrench matrix checked offline with numpy: rank 6, both positive/negative contributions available for each of 6 dimensions. **This is only algebraic reachability, not tested engine execution.**

**Critical unfinished integrations** after careful source audit:
- Existing `ShipCore` has no mounting of the 24 ModuleDescriptors into the actual live ship descriptor/runtime, skips RCS when creating `MountedEngine` motor views and when advancing engine operating life. `CobraRcsEquipmentAssembly.h` must be called by the actual Cobra descriptor factory before `ObjectModuleRuntime::init`, with real hit geometry/hull mounting. No dynamic descriptor ownership rewrite without auditing object lifecycle.
- `ShipControlExecutionGateway` still allows Newtonian scalar `motion.manoeuvreAccelerationMps2`/attitude RCS through legacy `DynamicMotionSystem`; new allocator is **NOT** connected to server, so **RCS in live game is NOT yet independently physically constrained by this new implementation**. Must divert execution to this physical wrench and suppress duplicated old artificial RCS forces once wired.
- `EngineLiveTelemetry` and `ShipEngineControlMatrix` have <=16 motor samples; new 24 RCS measurements need separate fixed-size RCS DTO, including per-nozzle BITE identity, not an array overrun.
- BITE only scans module definitions with `module.engine`. Once real 24 `ModuleDescriptor` entries are attached and replicated, existing `buildShipBiteState` can report them; MotherComputer cockpit/terminal currently consumes BITE, but per-nozzle HUD is a future renderer.
- Engine operating lifetime must be advanced from allocated RCS throttle with actual coolant, worn/damaged through existing `ObjectModuleRuntime`. Same physical engine passport retains electrical/heat budgets, but current reactor bus doesn't enforce motor demand. No claim of energy-constrained flight.
- **No access to user's Windows game runtime; no compiled game/CTests and no six successful in-game flights. Do not present these as finished or PASS.** A local numpy-only 6D geometric rank sanity check is NOT a game test. Do not change Assisted.
Record this before subsequent work; the current step is incremental source development, not completion.

---

## 2026-10-21 — RCS MODULE DESCRIPTOR ASSEMBLY; SERVER EXECUTOR MIGRATION BLOCKERS

Added `src/game/ship/presets/cobra/CobraRcsEquipmentAssembly.h` (commit e2a0643) to assemble **24 unique ModuleDescriptor objects** for Newtonian Cobra RCS into the existing module graph using the universal `EngineUnitDescriptor` + 5-cause `EquipmentDamageLayer` path. Each nozzle ID from the fixed-direction Cobra RCS mount catalog becomes a separately damageable, repairable installed equipment unit; no second damage system, no fake hit volumes. Helper deliberately requires caller to invoke it BEFORE ObjectModuleRuntime::init and to provide actual hit geometry; it does NOT mutate a live server ship behind the construction API. **No actual Cobra construction caller wired yet, so this does not yet make RCS damage live.**

Critical executor blockers verified in source: `ShipEngineControlMatrix.h::EngineMatrixResult::MaxEngines = 16` (24 RCS plus main engines exceeds it); `ShipCore.cpp` currently skips every `EngineSlotRole::ManoeuvreCluster` both in mounted-engine setup and per-engine operating-life/thermal update. `NewtonianPhysicalControlModel` still calculates RCS as scalar omnidirectional-cluster authority, and `ShipControlExecutionGateway` splits Newtonian manoeuvring acceleration from main matrix. Thus simply replacing slots with 24 nozzles would lose control correctness or silently reject execution. Proper next step must replace cluster/scalar RCS controller and 16-main telemetry with one physically bounded RCS allocation/separate telemetry path while keeping Assisted untouched; then wire assembled 24 modules to the native descriptor and physical damage exposure. CURRENT code is an incremental data/assembly change only; neither compilation nor live flight checked.

---

## 2026-10-21 — NEWTONIAN RCS NOZZLE GEOMETRY + ENGINE PASSPORT (PARTIAL IMPLEMENTATION, NOT YET IN PHYSICAL EXECUTOR)

Created `src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h`: explicit 8 nodes (fore/aft x upper/lower x left/right), each 3 outward fixed-axis nozzle units X/Y/Z with UNIQUE IDs (24 total), normalized coordinates and signed directions. Same map intended for Newtonian I/Y/X; independent of aft main count. Created `src/game/equipment/engine/ManoeuvreEnginePreset.h`: fictional electric RCS nozzle hardware passport via EXISTING `EngineUnitDescriptor` (12 kN, 0.12 MW input, 0.94 nominal energy-conversion efficiency, thermal capacity/conductance, 390 K nominal / 450 K safe, 4000/8000 equivalent operating hours, overtemperature damage, zero vector steering). All hardware values are provisional game-tuning, NOT observed flight figures. Standard EquipmentDamageLayer / EngineOperatingState models already support Laser, Impact, Thermal, Radiation and Wear and EngineUnit evaluation; no duplicate damage system authored.

**CRITICAL HONEST STATUS:** These are catalog/preset declarations only. Existing `CobraPropulsionPresets.h` Newtonian physical slot lists still contain legacy 4/6/8 `ManoeuvreCluster` omnidirectional RCS groups; Newtonian physical executor and actuator allocator still use old grouped RCS. The 24 new nozzle entities have NOT been mounted into ShipDescriptor as individual ModuleDescriptors with unique ObjectModuleRuntime IDs; physics damage, real per-nozzle power/heat/time degradation and BITE/terminal have NOT been wired for these 24. Neither compilation nor in-game tests have been performed. Do not report the 24 new actuators as operational or claim completed six-frame migration. Continue with installed-module assembly, real force/torque solver, thermal/energy tick and RCS BITE integration; preserve current Assisted physics. Single state file only.

---

## 2026-10-21 — NEWTONIAN INDIVIDUAL RCS DAMAGE REQUIREMENT; CURRENT GAP

User requires engine damage simulation to include independent Newtonian maneuver nozzles. Common Newtonian-I/Y/X design is 8 mount nodes x 3 independently actuated fixed-direction nozzles = 24 damageable units, distinct from aft main drive modules. Damage to a nozzle must persist in the SAME authoritative ObjectModuleRuntime as main engine damage, propagate through real physical RCS actuator execution, reflected six-axis force/torque capability, BITE, MotherComputer, HUD/terminal, and conserve/inherit damage state through installation changes. Cluster-level damage only if explicitly modeled as damage to a shared node, not substitute for per-nozzle wear/fault.

AUDIT: Current Cobra propulsion presets STILL encode 4/6/8 `ManoeuvreCluster` omnidirectional slots rather than 8 mount nodes and 24 individual directional nozzles; `ShipCapabilityBuilder::slotStates` treats any empty `slotModuleIds` binding as healthy, which hides individual damage; `applyMountedEngineAxialAuthority` explicitly skips ManoeuvreCluster. DO NOT claim per-nozzle damage works already. First build physical 24-nozzle plant with correct oriented forces/moments and compatible installed module bindings; only then map authoritative equipment damage into each nozzle's usable thrust. Validate signed 6D authority with failure cases and real server execution; never just blink BITE or multiply one global RCS coefficient. Preserve existing Assisted physics and avoid modifying Newtonian autopilot until plant is valid.

This is recorded as required work, NOT implemented by this note; no native runtime verification.

---

## 2026-10-21 — COBRA SIX PROPULSION PRESETS EXTRACTED (SOURCE ONLY, BUILD PENDING)

User selected cheap cinematic fixed-CoM ship skeleton: reactor/fuel center is the reference, hull mass plus three principal inertias, nominal engine mount coordinates (normalized, scaled by existing hull bounds) and installed motor units. Requested Cobra folder with six interchangeable schemes, including separate Newtonian maneuvering units, while finishing Assisted before Newtonian controller changes.

Created `src/game/ship/presets/cobra/CobraPropulsionPresets.h`, ONE source of the existing six native slot topologies with identical order, IDs, positions, nominal force directions and freedoms. `AssistedPhysicalControlModel.cpp` and `NewtonianPhysicalControlModel.cpp` now consume these shared immutable presets rather than owning duplicate topology arrays. The existing `ShipDescriptor::physicalControlModel` + `physicalControl.slotModuleIds` remains the descriptor selection/binding seam; no additional ShipDescriptor type, no physical law or autopilot control change. Three Newtonian presets contain individual RCS CLUSTER units: I = 1 main + 4 RCS, Y = 3 main + 6 RCS, X = 4 main + 8 RCS. Crucial limitation: existing RCS are `OmnidirectionalCluster` with zero nominal vector; these are individually positioned *clusters*, NOT physically resolved independently aimed nozzle units, and the design is not yet a full RCS engine-equipment passport. Do not claim Newtonian RCS simulation complete.

**Not yet complete:** the ship descriptor still has to bind/install a matching real engine module for EVERY slot if damaged Y/X native ships are to participate in BITE/HUD. Do not confuse current diagnostic topology override with a mounted engine inventory. Source changed only; no Windows build, CTest, six in-game damaged flights, dynamic reconstruction or damage scenarios validated. Run compilation/real-case acceptance before calling migration complete. The temporary new preset file intentionally stores no second set of physical engine laws. Logs D:/tmp only.

---

## 2026-10-21 — ASSISTED DAMAGE SIX-FLIGHT RUNNER; HUD AXIS & CONTROL STATUS

Created `scripts/run-assisted-engine-damage-flights.ps1` on isolated feature branch (commit `3560b33`), using existing `run-navigation-stress.ps1` and environment presets `ELITE_SHIP_DAMAGE_PRESET=aft-50|aft-failed` for Assisted I/Y/X. Runs all six even after individual failure, restores environment, logs at `D:/tmp/elite_engine_out/` with CSV summary. **Not executed/compiled locally, no PASS assertion.** Requires authoritative fixture to actually honor each preset; inspect per-engine damage and measured thrust in the game logs before accepting an engine-out result. This runner itself does NOT yet implement ideal reactor/cooling, which are still pending separate, isolated fixture overrides.

Verified `SpaceState.cpp` production cockpit render already calls `renderBoresight` unconditionally and `renderEmergencyThrustAxis` from `cockpit.renderTransform.motion.assistedWorkingThrustAxisBody` when emergency flag is true. Therefore the existing emergency thrust-axis reticle is hull-fixed, not a moving instant-force arrow. The geometrical working-axis analyzer proves a conservative ZERO-parasitic-torque CoM ray when possible; it does NOT yet solve the globally minimal nonzero moment among all feasible multi-engine allocations. May remain center-aligned for damage compensated by other motors. No HUD graphics source changed in this commit.

**Architecture answer:** local client MotherComputer is already the read-only authoritative-data aggregation / operational doctrine source for own-ship HUD and Assisted autopilot envelope; the server still applies manual/autopilot controls source-agnostically through `ShipControlExecutionGateway` / `ShipCore` and validates physical motor actuation. Not all flight commands are physically routed THROUGH MotherComputer, and its energy/thermal/mobility providers are unfinished. Do not describe full migration as complete. Newtonian remains untouched.

---

## 2026-10-21 — ASSISTED-ONLY ENGINE-OUT ACCEPTANCE AND TELEMETRY PRIORITY (USER DECISION; NOT YET EXECUTED)

**Scope:** Finish Assisted I/Y/X first. Do NOT change Newtonian planner/autopilot/RCS or use its unfinished flights as a gate. User observed three healthy Assisted game corridor flights behaving well; Newtonian-I yaw/side-slip and broken-segment route remain out of scope.

**Immediate flight:** run Assisted I (4 mains), Y (6 mains), X (8 mains) through SAME game tunnel with one selected AFT main at (a) partial damage, (b) complete failure/disconnected state. Preserve their healthy runs as baselines. Damage MUST be applied to authoritative installed equipment/module runtime used by Engine and real server allocator, not merely the client HUD or an isolated slot/capability fixture. Keep initial motion/route comparable; report actual availability, course following, saturation and whether the route is revalidated/rejected safely. A failed/infeasible flight is valid evidence, not permission to fabricate force or weaken collision constraints.

**Temporary test assumption:** reactor has unlimited available electrical budget; radiator/cooling has unlimited heat rejection, ONLY for these first damage-flight probes. Engine nominal electrical MW, generated heat MW, damage, efficiency and temperature/degradation instrumentation must remain observable. Do not globally rewrite reactor/cooling laws; this is an isolated diagnostic override, later removed.

**Per-engine actual output to observe:** stable motor module identity and fore/aft slot, attached/enabled/operational state, damage health and reason, aging/degradation, effective emitter/thrust coefficient, delivered/available/nominal thrust, nozzle vector yaw and pitch (electronic phased-field steering, no servos), max allowable cone, instantaneous force and reaction torque, consumed electrical MW, generated waste heat MW, engine/emitter temperature and remaining thermal limits. Tag unavailable values as unavailable, not fake zero. Aggregate ship pose/velocity/angular velocity, signed six-axis authority and working-thrust axis, total demanded vs delivered electrical MW, total generated vs actually rejected heat, reactor and cooling status when those systems are later integrated. Prefer authoritative EngineLiveTelemetry/EngineOutput + module runtime snapshots; avoid a second state owner or HUD-derived estimates.

**Code audit at decision time:** Engine::execute already returns actual force, delivered/available thrust, available vector limits, electrical MW and waste heat MW. EngineLiveTelemetry exposes actual thrust, availability, nozzle yaw/pitch, forces and power/heat by slot (capacity 16), but does NOT itself carry damage/temperature/engine efficiency or whole-ship cooling budget. EngineSlotState is explicitly a reduced projection that excludes power/fuel/thermal limits. ShipControlAuthorityTelemetry publishes six signed authority axes and revision; it is NOT a full engine/energy/thermal snapshot. Need an authoritative aggregation seam for MotherComputer and a real-time HUD, without inventing undocumented existing connections.

**Sequence:** (1) Implement isolated installed-engine damage fixture + 6 Assisted game flights and logs exclusively to D:/tmp; (2) repair any real controller/allocator limits revealed; (3) wire actual engine damage/age/efficiency/temperature and reactor/cooling telemetry to one per-tick authoritative ship-systems snapshot and replicate to HUD; (4) integrate finite energy/thermal budgets and repeat the same flights; (5) no extra tests/contracts except those demonstrating real behavior. New HUD must present every existing ship-system measurement as real-time authoritative state and distinctly indicate missing measurements. Existing Newtonian stays unchanged. No game flights, build or engine-damage injection completed by this design update.

---

## 2026-10-21 — PATCH RECAP: ACTUAL COMPILATION ERROR AND RELEASE ASSERTIONS

User-provided MinGW/Ninja build log `Вставленный текст(20261021-133721).txt`: compiler stops in `WorkingThrustAxisAnalysis.h` on undefined `determin`; because `ShipCapabilityBuilder.h` includes that header, both `working_thrust_axis_analysis_tests` and `engine_thrust_hud_presentation_tests` fail to compile. Exact symbol fixed to `determinant` (`71cdbe48`). Build process stops before CTest; no testing PASS evidence at all. In `ShipPhysicalControlModelsTests.cpp`, 98 former `assert` calls were disabled by the user's `-DNDEBUG` Release configuration; replaced with always-live `CHECK` that throws a detailed failure (`70da4b1d`) and corrected introductory comment/include order (`e68651a0`). This changes NO physical game logic and adds NO synthetic test suite. Next user `git pull --ff-only` and rerun original PowerShell script from native PowerShell or via properly quoted MSYS2 path, then fix any actual compiled/runtime failure. No success may be claimed until then.

---

## 2026-10-21 — USER DIRECTIVE: GENERAL CONTROL-PLANT INPUT/OUTPUT/TIME ANALYSIS; DO NOT GIVE SERVER CLIENT MOTHERCOMPUTER; NO TESTS FOR TESTS

**User insists:** Never special-case "one aft engine", even by stacking more ifs. Enumerate ALL actual control inputs, dynamic states, outputs, temporal delays, and disabled/degraded variations, and derive flight controllability from the complete available plant. Clarify whether server needs MotherComputer at all. Every test and interface must protect genuine executable flight behavior or a dangerous boundary; do not add abstract mocks, redundant files, contracts, or acceptance ceremony for its own sake. Keep ONE permanent state file PROJECT_STATE.md.

**Correct responsibility split (agreed):**
- AUTHORITATIVE SERVER FIXED-STEP PLANT: maintain mass + actual CoM + full inertia, motor hardware state, geometry, individual cone/rates, real energy allocator and cooling, collision/environment; receive PILOT CONTROL SIGNALS (manual and autopilot identical), turn these signals into force and torque via same actual engine/RCS actuators and integrate velocities/attitude; publish actual motion/engine execution. Server must never trust client-declared 'engine operable', maneuver plan, axis, physical thrust or time budget. It does NOT need full client MotherComputer, BITE UI, route planning, or per-ship asynchronous diagnostic threads. NPC server AI can consume the SAME compact deterministic flight-capability calculation if/when NPCs need it; no full motherboard per NPC.
- SHARED PURE PHYSICAL EVALUATION: given an EXACT read-only snap of actual installed actuator plant and a proposed bounded 6D demand, predict physically executable motor commands `(throttle_i,nozzle_i,rcs_i)` and actual `(F,M,energy,heat)` plus their dynamic response limits, with identical mathematics to server executor. Prefer refactor existing motor plant solver/executor into shared preview+apply rather than clone allocator math or invent a new parallel physical law. Nominal plant and failure case pass through SAME input/output equations. Current `WorkingThrustAxisAnalysis.h` proves only a conservative torque-neutral straight direction; it is NOT a general solution for all 6 axes at once; no new false certification.
- CLIENT MotherComputer: combine replicated facts and physically feasible joint-force/torque candidates into operational doctrine, working thrust reference, finite maneuver/cooling/energy horizon, and conservative full/limited/emergency/inoperable status. Fast same-tick synchronous authority never depends on async worker; worker computes optional expensive forecast. HUD/BITE/terminal consume its coherent state. A chosen working axis is a POLICY among PHYSICALLY PROVEN references, not a new force vector in server.
- AUTOPILOT: never sees engine layout, nozzle mechanics, damage percentages or separate power/cooling logic. Receives feasible control envelope, reference axes and explicit time-to-acquire/brake/rotate/status; computes path/speed/corridor/turn lead and issues SAME manual control signals, not motor bypass. Server ultimately constrains execution.
- NETWORK: transmit actual movement state on existing tick; transmission of expensive capability or installed geometry only on source revision (damage/installation/CoM/energy/coolant relevant revision) or when quantitative envelope changes meaningfully. Source timestamp/revision+validity required; no new chatty RPC loops. Camera interpolation stays client.

**Explicit genuine input/state inventory (units/types from current code):**
1) Pilot / autopilot command: main forward/reverse speed/throttle, requested linear acceleration vector in body frame m/s², requested angular acceleration `(pitch,yaw,roll)` rad/s², direct RCS/manoeuvre channels, braking/alignment selection, controller law Assisted/Newtonian, command tick & step `dt` (source `EngineMatrixDemand`, `ShipControlState`, `DynamicMotionSystem`).
2) Installed actuator matrix: layout/slot ID & role (Assisted I/Y/X 2/3/4 aft and 2/3/4 fore; Newtonian I/Y/X 1/3/4 aft plus 4/6/8 manoeuvre clusters), slot body-space lever arm m, nominal force direction, engine nameplate thrust N, fore/aft polarity, per-engine damage and attachment/health, absolute thrust availability, TVC cone angle rad, TVC max angular speed rad/s (authored in `EngineUnitDescriptor`); intermittent thrust irregularity sampled in `Engine::instantaneousAvailableThrustNewtons` with tick/4 hold.
3) Rigid-body & environment: dynamic effective mass kg, CoM local m (cargo shifts), pitch/yaw/roll inertias kg m², current position/orientation, linear velocity m/s, angular velocity rad/s, external impulses and obstacles, speed/load/crew envelope, `ShipParams`.
4) Reactor + electric: reactor current vs peak MW, status and damage, consumer list and priority, power bus requested/allocated MW, actual engine consumedElectricalMW, possible accumulator stored MJ and maximum discharge MW (do NOT assume source exists or is connected), emergency critical-load policy.
5) Thermal: engine nominal efficiency/damage conversion, MW waste and nozzle-emitted radiation, engine internal heat capacity & conductance, coolant temperature K, thermal mass MJ/K and critical K, pump requested/allocated power and transfer MW, radiator area/emissivity/health and actual rejected MW, transient accumulated MJ. No double-count directly radiated nozzle heat.
6) Control dynamics & latencies: discrete authoritative physics `dt`, command/network age and latest revision; Assisted speed response `throttleAccel` / `fallbackThrottleResponsePerSecond`, `strafeDamping`, controller angular damping; Newtonian gimbal slew `mainGimbalRateRadPerSec` in `DynamicMotionSystem::updateNewtonianMainGimbal`; electric bus service tick vs physics tick; motor thermal progression/wear over time; angular slew + angular-arrest time from available SIGNED torque; ballistic distance during latency. **Important model defects identified:** the generic `Engine::execute` reports gimbalRateRadPerSec but Assisted `ShipPresetEngineManeuvers` and `EngineMatrixDemand` explicitly allow instantaneous nozzle angle changes each control tick (NO servo dynamics); engine thrust throttle spool-up physical lag is not modeled independently of controller speed-response gains; `PowerBus::update` registers other systems but NOT installed engine actual power demand so engine draw does not derate physical thrust; `ThermalSystem::update` is empty but `addHeat/removeHeat` and `CoolingSystem.update` do change thermal state. **Do not publish physical maneuver time/certifications until actual executor and estimator share same delays and motor MW clipping.**

**Actual physical outputs (per control tick):** per-engine commanded/attained throttle fraction, gimbal pitch/yaw and rate response, per-engine actual force `F_i` N, actual moment `r_i × F_i` N m, summed 3D linear acceleration `ΣF/m` m/s², angular acceleration from `I^{-1}ΣM` rad/s², measured/predicted energy MW/thermal MW, pressure/heat remaining, actual achieved angular rates and main/RCS drift; measurable deviation between demanded and achieved wrench; condition flags unknown/infeasible/partial/safe, guaranteed horizon and timestamp. These values must feed future MotherComputer profile; no 'full authority' based only on max of independent axes.

**General degraded combinations to evaluate with ONE solver, not if-matrix:** lost/partial engine thrust, asymmetric bank and missing fore reverse thrust, failed or fractionally degraded TVC (fixed-axis motor may still accelerate but induces torque unless compensated), failed/depleted RCS and one-direction torque, changed CoM/inertia, constrained reactor or unavailable actuator power, worn radiator/pump or excess heat, intermittent motor thrust, thermal slowdown, command+telemetry delay and rate-limited turning, failure to arrest spin, ability to coast/flip/burn vs controlled docking. For each state compute physically feasible NONNEGATIVE motor allocation respecting cone and caps, signed achievable accelerations/torques simultaneously, minimal controllable complete 180deg turn+arrest+velocity braking time (and NO arbitrary promise if impossible), resource bound and corridor safe-speed. One-motor-only is simply another subset and does not guarantee yaw/pitch/roll separability. 'Unverified' remains distinct from 'Inoperable' until missing power/thermal providers actually exist.

**Critical test evidence just received:** local user ran `scripts/run-assisted-engine-out-gate.ps1` through actual MSYS2 build, failed during **compiler** before any test: `WorkingThrustAxisAnalysis.h:84` used undeclared `determin` instead of `determinant`. Corrected on feature branch in commit `71cdbe48`. Also compiler warnings on `ShipPhysicalControlModelsTests.cpp` showed assertion operands unused; direct inspection proves 98 `assert(...)` checks disappear under `-DNDEBUG` Release (O3), so old test could falsely claim PASS without checking anything. Replaced real existing assertions with always-live CHECK macro (throw on failure with file line/condition) in commit `70da4b1d`. This is NOT a new test; it makes existing physical invariants effective. Previous dozens of tests were NOT executed; user must pull feature branch and rerun. Keep tests directly tied to executed physics: healthy I/Y/X unchanged, signed thrust/torque for damaged I/Y/X, real 180deg + arrest timing and resource-bound emergency flight in game, and motherboard data coherence. NO more mocked classification tests that pretend to prove actual engine authority. Existing pure classifier tests show only classifier arithmetic, NOT motor feasibility.

**Progress and limits:** Pure `WorkingThrustAxisAnalysis` presently offers NO full joint 6DoF/temporal envelope. Although current `MotherComputer` selects a candidate doctrine, `out.mobilityEvidence={}` intentionally remains UNVERIFIED; no all-conditions motor/energy/thermal provider is installed. The user asks to SOLVE GENERALLY; do not mark finished. Next real integration must start with real actuator dynamic response and true electricity allocation, then use same preview/execution source to compute actual reachable wrench and cost over time. Avoid new abstract parallel 'contracts'. Server does NOT need full MotherComputer, only accurate authoritative plant and if relevant same read-only computation for NPCs. `main` and restore point remain untouched.

---

## 2026-10-21 — FLIGHT CAPABILITY RESPONSIBILITY REFACTOR: PURE PHYSICAL AXIS PROPOSAL + MOTHERCOMPUTER DOCTRINE (NOT YET COMPILED)

**User architecture rule (agreed):** autopilot does NOT inspect per-engine damage/motor/nozzle topology. It receives achievable CONTROL capabilities, physically reachable thrust reference, transition/turn/brake durations and permitted flight doctrine via a single MotherComputer facade. Physical actuator plant (authoritative server) uniquely maps ordinary pilot/autopilot controls to real Newtons/torques, and reports possible signed wrench/axis candidates. MotherComputer (client) selects preferred operational doctrine / route strategy from PHYSICALLY PROVEN candidates and combines electrical + thermal + BITE constraints. Flight planner chooses route/speed/lead, never invents motor force. HUD and terminals read motherboard only. Family Assisted vs Newtonian is an installed physical law, not magically switched due to damage. Network server must validate executed motor commands with same shared, deterministic physical policy; MotherComputer worker cannot modify server physics, and no new server RPCs were added.

**Implemented this turn, branch `feature/assisted-engine-out-axis-2026-10-21` only:**
- NEW `src/game/ship/capability/WorkingThrustAxisAnalysis.h` physically grounded, PURE layout-independent 3D CoM geometry analyzer. It accepts each aft installed motor's ID, CoM-relative arm, actual usable Newtons and gimbal cone. Proves whether a conventional body -Z torque-null force can be attained via nonnegative throttle allocation for one centered motor, an opposite motor pair, or any triangle enclosing the projected CoM (Caratheodory). Thus unequal capacity with both aft still live can retain nose=axis via trimming, and arbitrary Assisted I/Y/X arrangements are supported. If not, conservatively proposes a SINGLE specific gimbaled surviving aft motor with line of force -arm/|arm| through CoM, within physical nozzle cone; choose maximal forward-component usable Newtons; never infer a safe axial burn from merely 'some engine is alive'. If no safe CoM ray within cone, declare no proven main axial force and leave attitude possibilities separate. Side motor's exhaust/nozzle points OUTWARD, its REACTION force inward via CoM.
- `ShipCapabilityBuilder.h`: removed special 'survivingAft == 1' trigger for axis choice, now universally queries analyzer for native Assisted layouts, retains healthy/partially asymmetric bank's nominal -Z whenever geometrically balanceable. When off-axis, caps planned main forward force at selected motor's usable force; when no safe neutral ray exists, zeros claimed safe straight-ahead authority. Owns only PHYSICAL candidate generation, not autopilot/route decision. Some existing scalar bank capability approximations remain, particularly balancing weighted thrust for healthy multi-bank layouts; not a full multi-engine optimal thrust allocator yet.
- `ShipParams.h`: `assistedWorkingThrustMotorIndex` to identify the exact gimbaled motor behind the fallback axis; essential for general Y/X rather than implicitly firing the first aft motor.
- `ShipPresetEngineManeuvers.h`: emergency motor allocation now selects that exact slot index and avoids secretly using the wrong motor; program cache matches selected slot/emergency mode. Existing zero-unrequested-angular-acceleration emergency guard retained.
- `ShipControlAuthorityTelemetry.h`: emergency forward available acceleration is attributed ONLY to the selected physical aft motor (instead of summing all surviving aft motors falsely).
- `MotherComputerClientAdapter.h`: for all native Assisted layouts it derives identical physical candidate axes, rather than only 1-survivor condition. Identical shared pure analysis on client and server; no extra server requests.
- NEW `src/game/ship/computer/FlightControlProfile.h`: `PropulsionDoctrine::{AwaitingPhysicalEvidence,NominalNoseAxis,DamageOffsetAxis,BallisticFlipBurnCandidate,NoSafeAxialThrust}`. Pure synchronous strategy policy over real `ShipParams`/installed signed motor authority; 'candidate' ≠ certified safe 180°/braking route. `MotherComputer.h::observe` now selects this doctrine synchronously and includes it in same-revision snapshot; Assisted flight-envelope reader gives motherboard's selected working-axis reference to client autopilot. The LOCAL projection is checked against SERVER-PUBLISHED executed working-axis flag and vector; any model/coefficient/CoM mismatch FAILS CLOSED rather than handing autopilot a wrong gimbal heading. Server itself continues to validate actual physics and motor commands. Async worker remains non-authoritative.
- NEW `tests/navigation_runtime/WorkingThrustAxisAnalysisTests.cpp`: verifies healthy I/Y/X nose axis, 50% asymmetrical but throttle-balanced pair nose axis, destroyed left aft Cobra 13m/11.1m -> 49.5° outward nozzle/inward force, unbalanced Y two-aft fallback, opposite diagonal X pair remains nose, insufficient gimbal rejected, all aft destroyed rejected. Registered `working_thrust_axis_analysis` in `tests/navigation_runtime/CMakeLists.txt`, included in `scripts/run-assisted-engine-out-gate.ps1`. `MotherComputerTests.cpp` now checks emergency selected index/reference, no-fore candidate flip-burn, missing authority fails closed, no safe thrust status, server/client matching axis required.
- `main` and return-point checkpoint unchanged. No compiled executables or actual CTest/gaming results were produced from current connector-only source access. Do NOT state verified/pass.

**CRITICAL STILL-MISSING TO CALL FULL SHIP-OPERABILITY ASSESSMENT 'FINISHED':**
1. `src/game/ship/computer/MotherComputerClientAdapter.h` explicitly sets `out.mobilityEvidence = {}`: all 3 verified control/electric/thermal proof providers are STILL MISSING. Therefore `assessShipMobility` returns `Unverified`; flightProfile doctrine is a conservative preview, NOT a safety/return certificate. No automatic eligibility to return to hub or selection of actual route based on this incomplete classification.
2. `ShipCore::updateSystems` calls `PowerBus::update` without mounted main-drive `engineTelemetry.electricalMW` included as a real bus consumer and allocated motor power bound. Must implement physical engine electricity request/allocated MW and engine thrust derating under overload BEFORE issuing verified endurance status; current observed MW not sufficient to certify future burn.
3. `ThermalSystem`/radiator/coolant thermal endurance remains time-and-duty-cycle dependent; evaluate actual conducted engine heat vs radiated, panel damage and safe K/mass budget for each proposed long maneuver, otherwise thermal proof false.
4. Scalar per-axis signed programs are NOT sufficient to prove simultaneously achievable 6D force/torque under damaged motors, nor 180° slew + stop-spin + hold + flip-burn. Build real bounded signed-wrench sequence certifier at install/damage/revision time, no expensive frame-rate optimizer. Proven combined limits/time should be supplied by physical plant to MotherComputer and autopilot, replacing direct planner checks of installed fore/aft counts/axes.
5. `MotherComputer` currently selects strategy from physically analyzed reference but its server-side executor still obtains same shared selection on server: this is intentional authoritative validation, not a redundant second operational decision. Future optional additional client policy may choose among multiple motor-combination profiles, never bypassing physical constraints. Real pilot/autopilot commands must always use the same execution gateway.
6. Existing client/sim current CoM may differ because client has descriptor projection without latest cargo-dependent mass-properties; explicit server working-axis coherence gate prevents contradictory recommendations, but server should eventually replicate minimal physical capability DTO (CoM validated preferred axis, feasible signed forces and torques, revision) rather than infer dynamic mass client-side.
7. URGENT: user has not yet supplied a successful compile/test log. Prior failed attempt was Bash quoting `-File .\\scripts\\run-assisted-engine-out-gate.ps1` (slashes swallowed). In MSYS2 Bash use `powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\\scripts\\run-assisted-engine-out-gate.ps1'`; PowerShell native `.\\scripts\\run-assisted-engine-out-gate.ps1`. Run `git pull --ff-only` first. Collect `D:\\tmp\\elite_engine_out\\assisted_engine_out_gate.log`; fix compiler/test failures, then healthy/aft50/aftfailed and Y/X damaged combinations. DO NOT MERGE `main` until all accepted.

**Architectural boundary:** Correct response to user: 'The physical layer maps requests to actual dynamics and emits feasible actuation candidates; MotherComputer owns operational doctrine and joins BITE/power/heat; autopilot plans against the published feasible CONTROL API. The analysis is not yet COMPLETE, because the verified resource and jointly controllable time/capability providers still do not exist. Do not invent them or let asynchronous computer control physical actuators.'

---

## 2026-10-21 — MOTHERCOMPUTER NOW CENTRALIZES PLAYER HUD, COCKPIT AND TERMINAL PRESENTATION DATA (CODE WRITTEN; COMPILATION UNVERIFIED)

**User instruction accepted:** Player HUD, thrust bars, flight-vector display, BITE and docking status terminal, and player engineering HTML terminal MUST read from ONE ship-specific MotherComputer data plane; no duplicate per-widget engine damage/BITE interpretation or server requests. MotherComputer is a CLIENT flight/computer data node; does not render graphics or take authoritative physics from server. World object/navigation marker positions remain owned by the navigation/world presentation stream (not raw ship engineering), but any own-ship facts are to pass through MotherComputer.

**Implemented branch-only work:**
- `src/game/ship/computer/MotherComputer.h`: added `MotherComputerCockpitView` (independent factual read-only per shipInstanceId+serverTick, interpolated client `ShipTransform`, style/maxspeed, grouped motor slot capacities and actual delivered thrust, shared `game::bite::State`, existing `ShipCoreStatus`), plus `publishCockpit`/`cockpitSnapshot` guarded by the same ship/tick and mutex. New `MotherComputerTerminalView` and `publishTerminal`/`terminalSnapshot` isolate terminal from raw physics/BITE and ensure matched ship/tick. New physical observation tick invalidates previous cockpit/terminal state. Diagnostics remain separate latest-wins asynchronous worker; UI/flight reads never wait for the worker.
- `src/game/ship/computer/MotherComputerClientAdapter.h::collectCockpitView`: only one presentation-side adapter reads replicated ship module topology/state and actual `EngineLiveTelemetry` / `ShipTransform` into a small DTO, attaches identical BITE and systems from the motherboard authoritative input. Engine slot view covers Assisted/Newtonian I/Y/X including physical role and individual yellow available/red delivered fractions; fault flags derive from the SHARED BITE `hudAttention`, not ad hoc fault thresholds. For diagnostics/layout overrides synthetic healthy slots remain their prior accepted behavior.
- `src/game/presentation/EngineThrustHudPresentation.h`: added overload `buildEngineThrustHudPresentation(const MotherComputerCockpitView&)` that only groups/renders already authoritative factual slots (retains correct Assisted-I fore/aft bar order, Y/X and Newtonian exclusion of fore bars); the original `ClientShipState` overload remains for existing tests/fallback API but the production `SpaceState` HUD uses ONLY MotherComputer overload.
- `src/game/presentation/ClientHudPresentation.h/.cpp`: factored velocity/coordinate HUD and flight-vector presenter to use common `ShipTransform` implementation; added MotherComputer cockpit overloads. Original standalone `ClientShipState` API preserved for existing tests; production calls motherboard.
- `src/game/SpaceState.cpp`: publish motherboard cockpit once immediately after `GameClient` receives each new authoritative player ship tick, then refresh only client presentation/interpolated transform when rendering; world coordinates, speed/flight law, flight indicator and emergency working thrust-axis reticle now read from motherboard. Thrust bars via motherboard. Docking/BITE terminal uses `cockpit.bite` exactly, composes existing local route/planner terminal details into `MotherComputerTerminalView` and consumes final snapshot FROM computer. `pushShipCoreState` HTML diagnostic of OWN controlled player ship now reads motherboard `systems`; inspecting arbitrary NPC engineering is explicitly separate client-world inspector until per-NPC motherboard coverage exists.
- `tests/navigation_runtime/MotherComputerTests.cpp`: tests HUD/terminal same source BITE/engine colors, ship/tick mismatch, invalidation on next replication tick, stale terminal rejection. `tests/navigation_runtime/EngineThrustHudPresentationTests.cpp`: tests motherboard-based Assisted-I grouping, fault display, yellow capacity and actual red thrust. `scripts/run-assisted-engine-out-gate.ps1`: adds engine thrust HUD test to motherboard, mobility, motor and autopilot test suite. Logs `D:\\tmp\\elite_engine_out\\assisted_engine_out_gate.log`.

**Strict limitations:** No actual local MSYS2 GCC/Ninja/EliteGame compilation, CTest or 3-flight in-game acceptance in tool environment. DO NOT assert PASS. This is FIRST UNVERIFIED integration; compilation/runtime fixes may be needed when user posts logs. Terminal workflow/planner strings are STILL composed from local navigation state in `SpaceState` and merely transmitted via motherboard terminal channel: to achieve strict single-source logic, next move event production to structured MotherComputer navigation inputs (without turning motherboard into route planner or duplicating navigation algorithms). Player cockpit ship-centric facts DO use one node. External nav/world marker positions and target data remain legitimate external inputs, not an extra ship engineering reader. Selected NPC engineering panel remains an inspector until multiple motherboard instances are implemented.
**User run update:** User switched locally to `feature/assisted-engine-out-axis-2026-10-21` but ran `powershell -File .\\scripts\\run-assisted-engine-out-gate.ps1` from likely Bash/MSYS2, whose unquoted backslashes were swallowed, producing `.scriptsrun-assisted-engine-out-gate.ps1`. Actual tests DID NOT START. Correct MSYS2 invocation from Elite root: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\\scripts\\run-assisted-engine-out-gate.ps1'` (Bash single quotes) or, from native PowerShell, `.\\scripts\\run-assisted-engine-out-gate.ps1`. User must `git pull --ff-only` before testing most recent motherboard HUD/terminal commits.
**Repo constraints:** restored `main` remains unchanged and is return point; only isolated feature branch edited. Update this file only; do not create parallel states.

---

## 2026-10-21 — MOTHERCOMPUTER CLIENT-LOCAL CENTRAL DATA PLANE; ASYNC ADVISORY + SYNC PHYSICAL AUTOPILOT INPUT (CODE WRITTEN, BUILD UNVERIFIED)

**User-directed architecture:** Introduce ONE local ship "mother computer" centralizing previously scattered BITE, physics, motor authority, system diagnostics, power/thermal telemetry and future operation/route feasibility. Explicit separation: server owns authoritative fixed-tick physics, hull/engine forces, damage, energy and heat, collisions/docking validity, world identity/time and persistence. Client MotherComputer owns combined READ-ONLY instrument view, nonauthoritative system diagnosis, maneuver/resource proofs, route planning and cockpit presentation. Client may not demand new server RPCs/data for provisional motherboard work. Audit existing replication payload first. Server's CURRENT payload is not asserted bandwidth-optimal and not yet reduced.

**Implemented module and integration on ONLY** `feature/assisted-engine-out-axis-2026-10-21`:
- `src/game/ship/computer/MotherComputer.h`: per-client MotherComputer lifetime managed by `SpaceState`; client-owned `std::thread` with `std::condition_variable` latest-wins one-input mailbox (no per-tick thread creation), immutable COPIED input, no access to server/ShipCore pointers/transport, deterministic `stop()` join; result source shipId+serverTick+internal generation must equal current sample. Concurrency split: (a) SYNCHRONOUS fast lane `physicalFlightEnvelope(shipId,tick)` for both laws and `assistedFlightEnvelope(shipId,tick)` only with valid matching REAL signed installed motor authority; (b) asynchronous advisory `snapshot()` with `analysisCurrent`, provisional signed motor axis capabilities, optimistic yaw half-turn **LOWER BOUND ONLY**, observed electric/coolant margin, and `ShipMobilityAssessment` status. No automated engine/direct world actions by worker.
- `src/game/ship/computer/MotherComputerClientAdapter.h`: builds motherboard input only from already replicated `ClientShipState` (installed motor authority, existing physical/control layout, current CoM descriptor geometry, replicated engine live vectors/thrust/electric MW/heat MW, reactor/bus/cooling actual, fully copied `ShipCoreStatus` BITE data, current velocity, requested and delivered angular acceleration, working thrust axis), respecting ship identity and server revision. Passes actual `game::bite::State` computed with existing `buildShipBiteState` in client. Asymmetric one-aft working-axis projection retained ONLY for Assisted; healthy/50% Assisted and healthy Newtonian-I original baselines deliberately not modified by this adapter.
- `src/game/SpaceState.h/.cpp`: one MotherComputer member; after every NEW client-replicated `serverTick` of controlled ship, copy source facts ONCE into motherboard. Active Assisted docking/autopilot now reads live matching installed motor authority and physically bounded `ShipParams` through motherboard fast lane, not direct `player.controlAuthorityTelemetry`; Newtonian/Assisted nominal physics is read via common `physicalFlightEnvelope`, with original planner-built fallback retained until motherboard sample available. Engine BITE/HUD terminal preferentially reads motherboard's same-ship/tick BITE snapshot, with original view fallback only during initial sync. No changes to normal manual control path, physics simulation tick or server commands.
- `tests/navigation_runtime/MotherComputerTests.cpp`: checks synchronous motor authorities don't wait for async worker, installed motor bound not inflated by descriptor, wrong ship/tick/stale sample rejected, BITE faults copied with source revision, latest-wins 150 samples, same-server-tick generation disambiguation, Newtonian generic envelope does NOT invent Assisted motor authority, worker's missing verified resource providers yield UNVERIFIED not auto-route permission, worker stops safely.
- Registered `mother_computer_tests` in `tests/navigation_runtime/CMakeLists.txt` with `Threads::Threads`; updated `scripts/run-assisted-engine-out-gate.ps1` (D:\\tmp\\elite_engine_out\\assisted_engine_out_gate.log), compiles/tests mother + mobility + core physical + client autopilot, then EliteGame.

**Critical honesty and incompleteness:**
1. The new MotherComputer is an IMPLEMENTED CENTRAL CLIENT DATA PLANE, NOT YET A VERIFIED COMPLETE PHYSICAL OPERABILITY COMPUTER. `ShipMobilityEvidence` from adapter is deliberately EMPTY/unverified for electrical, thermal and full joint-6DOF maneuver proofs until providers are written. Advisory half-turn time is optimistic lower bound from signed independent yaw-axis values and NEVER a certified ability to execute/stop a 180° maneuver. Ship Mobility Assessment remains `Unverified`, even for visually healthy ships, until resource data can be proved. This is safer than manufacturing false emergency docking/return permission, and has not been wired as an unconditional controller hard stop (to preserve existing baseline).
2. Existing `PowerBus` does NOT allocate engine `electricalMW` as part of its real energy consumers, even though engine actuator telemetry exposes MW. Must add motor electrical load and enforce available power physically BEFORE a real `PowerMobilityProof`; then model actual battery peak discharge MW and usable stored MJ if present.
3. Cooling is modeled and engine exhaust heat partition exists, but total sustainable MW, safe stored MJ and run-time heat time-to-failure must be validated as a CONTROL-demand-dependent forward envelope; only then `ThermalMobilityProof` verified. Never double-count exhaust heat directly radiated in the nozzle.
4. Existing cached motor programs are individually signed per axis but do NOT prove arbitrary simultaneous force+moment or 180° spin/arrest/dock feasibility. Need generalized installed-motor wrench + timing/thermal/endurance trajectory certifier for **all I/Y/X and 4/3/2/1 installed working-engine subsets**, not the previous exactly-one-aft temporary axis candidate. Only after this can MotherComputer classify Factory/Limited/Emergency/Inoperable and advertise route reachability.
5. `MotherComputer` uses existing replicated facts; NO SERVER-BANDWIDTH REDUCTION has been performed. Minimal server protocol optimization must be measured and designed; do not strip authoritative physics or needed collision/hub facts based on guesses.
6. User's full policy: if at least one source can produce controlled thrust AND real motors can perform a complete half-turn one way, brake their OWN angular momentum, hold required burn attitude, brake translational velocity, and all this fits electric+thermal budget, then emergency straightforward route to nearest PHYSICALLY REACHABLE hub may be attempted. Nearest by range alone not enough; no automated return-to-hub until a route-specific separate proof demonstrates course, obstacles, stopping, remaining resource and docking/rendezvous constraints.

**Status:** GitHub source changes committed only in isolated feature branch; no GCC/MSYS2 or CTest runtime performed in this tool context. Do NOT claim tests PASS or MotherComputer validated in game. Current untouched restore baseline remains `main`. Next locally run `git fetch origin; git switch feature/assisted-engine-out-axis-2026-10-21; git pull --ff-only; powershell -NoProfile -ExecutionPolicy Bypass -File .\\scripts\\run-assisted-engine-out-gate.ps1` and provide D:\\tmp log on failure. After green build verify motherboard async BITE and motor live state plus healthy/50%-aft/full-aft-failed flight matrix; then implement missing physical proof providers and resource linkage before merging anything.

---

## 2026-10-21 — ARCHITECTURE CHANGE ACCEPTED: UNIVERSAL SHIP MOBILITY/OPERABILITY ASSESSMENT, NOT ONE ENGINE-OUT SPECIAL CASE

**User correction, architecturally mandatory:** The current engine-out 1-aft Cobra-specific axis is NOT a general capability assessment. Ship may have Assisted I/Y/X or Newtonian I/Y/X, several failed/diminished aft/fore engines, altered CoM, insufficient electrical energy or damaged cooling. Introduce topology-INDEPENDENT live assessment of *whether and how* ship can navigate, based on evidence rather than engine count, control-law enum or scalar axis capacities. Factory thrust axis coincides with nose only if physically controllable; working thrust direction comes from feasible installed-engine force/torque allocation, not "exactly one intact aft engine". The former single-aft selector in `ShipCapabilityBuilder.h` is explicitly a temporary candidate, not the correct final architecture. This must be generalized before acceptance/merge.

**Four user-defined operational categories:**
1. FACTORY: all mandatory attitude/translation/braking abilities and factory maneuver envelope physically achievable; sustainable power and cooling for continuous operation.
2. LIMITED: full directional course and attitude control & braking remain feasible, but speeds/acceleration/maneuver durations/turn radius must be reduced; sustainable continuous power/cooling.
3. EMERGENCY: controlled translation, at least one COMPLETE signed-attitude half-turn (180°) **plus arrest angular velocity, hold burn orientation and brake**, have a time-bounded verified electrical and thermal capacity to complete maneuver; only simple reduced-speed physically revalidated route / safe-coast is eligible. May coast + flip + burn, even with just one usable main motor if maneuver authority is PROVEN; one motor by itself is NOT proof.
4. INOPERABLE: essential steps of controlled propulsion/stabilization/braking or energy/thermal operating window impossible; safe coast, distress/rescue, no autopilot false promise. Separate `UNVERIFIED` is an EVIDENCE STATUS not a "damage grade": absent/out-of-date power/thermal/controllability proof MUST NOT authorize automated return.

**New shipped experiment (first pure headless contract, not yet integrated):**
- `src/game/ship/capability/ShipMobilityAssessment.h` pure `assessShipMobility(const ShipMobilityEvidence&)` returns `ShipMobilityAssessment`: `MobilityClass::{Unverified,Factory,Limited,Emergency,Inoperable}`, reason bitflags, thermal/electrical sustainable booleans and remaining seconds, verified safe-maneuver duration and net acceleration, `candidateForSimpleReturnRoute` only when evidence permits it; explicitly does NOT imply ANY actual hublink reachable.
- Inputs have three independent AUTHORITATIVE proof contracts: `ControlMobilityProof` (signed, combined physically possible 6-axis motor program + complete 180° turn, rotational arrest, attitude hold, reverse velocity braking in chosen state; credible conservative upper-bound maneuver duration); `PowerMobilityProof` (continuous bus MW, essential + real motor MW load, usable stored MJ **AND max battery discharge MW**); `ThermalMobilityProof` (coolant/waste heat MJ/s, sustainable radiative heat rejection MW and remaining safe thermal MJ). No arbitrary number/percentage thresholds: feasibility uses actual actuator sequence and duration.
- `tests/navigation_runtime/ShipMobilityAssessmentTests.cpp` tests nominal, sustainable limited, certified 1-engine-like emergency, unverified power/thermal/control, inability to arrest turn, absence of braking, insufficient continuous/battery instant MW, battery energy running out, heat saturating before maneuver, finite valid emergency battery and NaN measurement. Registered in `tests/navigation_runtime/CMakeLists.txt` as `ship_mobility_assessment`; included in `scripts/run-assisted-engine-out-gate.ps1`, logs under D:\\tmp\\elite_engine_out.
- **Do not claim PASS**: files committed via connected GitHub; NO compiler, CTest or live-game execution performed in this environment yet.

**Existing real subsystem evidence and GAPS discovered:**
- `ShipPhysicalControlModel.h` already carries partial `RuntimeFlightRegime` categories and slot states, and `ShipCapabilitySnapshot.h` has `ControlHealthState`, but their criteria are based on alive/disabled engine bank/maneuver counts and aggregate scalars, **NOT joint signed 6D force-torque maneuver reachability, power/cooling/endurance**. `AssistedPhysicalControlModel.cpp` declares any alive aft+fore bank merely degraded vs full, any one whole bank absent `ThrustAxisFallback`; Newtonian chooses `SurvivalControl` if one family missing. Those categories are NOT safe return certification.
- `ShipCore.cpp` has reactor `getCurrentOutput,getMaxOutput,getStatus`, `PowerBus::availablePower/overloaded`, thermal coolant temperatures/critical, cooling `getLastRadiatedPower,getPumpCapacity,getAllocatedPower`, and motor `EngineLiveTelemetry` reporting **electricalMW** and heat each physical tick. However `ShipCore::updateSystems` registers life support, avionics, radiation shielding, pump, radar, transmitters etc as `PowerBus` consumers and does **not** register installed primary engines as PowerBus loads. Engine operating life and heat enter the common thermal volume, but the power-bus source can overpromise main-burn endurance. Therefore runtime `PowerMobilityProof.verified` MUST remain FALSE until the engine MW and power allocation are physically integrated. Do not confuse an existing available-power counter with proof motor draw can be supplied.
- Cooling should account real heat deposited into coolant (NOT direct nozzle radiated watts again); sustainable heat-rejection rate, radiator panel state, thermal margin and pump power are available in pieces but have no unified guaranteed flight-duration proof yet. Thermal verified MUST remain FALSE until connected.
- Attitude 180° alone is insufficient; need actual stop-and-hold under signed torque with current inertia and engine gimbal cones, plus reachable anti-velocity braking and resource endurance. The `AssistedManeuverPrograms` cached signed single-axis basis and 1/3 concurrency heuristic do NOT prove reachable combined command; apply a physically bounded wrench evaluator/controller/short-horizon maneuver certificate for any subset of installed actuators including RCS.
- Route eligibility pipeline must select candidate actual hubs, solve geometry/obstacle/collision corridor and check power, thermal and time budgets over the WHOLE route, including velocity braking, arrival attitude and docking capability (if docking impossible, safe rendezvous rather than automatic docking). "Nearest" geographically may not be nearest physically reachable. Recheck certificates if health/power/cooling/CoM changes, and fall back to safe coast/distress rather than demanding imaginary controls.
- Keep independent source-level physical maneuver checker and unified read-only assessment. Healthy Assisted code path unchanged by this new pure module. Never auto-switch damaged Assisted to Newtonian artificial physics; emergency doctrine may LOOK Newtonian because it uses coast/flip/burn through real installed motor controls.

**NEXT ACTION:** Compile `ship_mobility_assessment_tests` and full gate on user's Windows 10/MSYS2 checkout of `feature/assisted-engine-out-axis-2026-10-21`. Then implement adapters linking real combined-wrench reachability, real power bus incl installed main engines, and temperature/time-correct thermal envelope. Publish authenticated operational class/reason codes to ship state, HUD, planner and AI, conservatively UNVERIFIED until complete. Test 4/3/2/1 actual motors, all six topologies, fore/aft failure combinations, low-energy/heat, turn+arrest, no-false-PASS. Only then consider simple return-to-hub behavior. Original restored `main` checkpoint remains untouched.

---

## 2026-10-21 — POST-IMPLEMENTATION DELTAS; STILL UNVERIFIED

- Added explicit emergency heading slew time to the Assisted-only route speed/corner preview (`DedicatedClientRouteAutopilot.h`): transform damage-derived working thrust axis into world coordinates; angular rest-to-rest time `2*sqrt(actual_axis_to_route_tangent_angle/angular_accel)`, plus current pitch/yaw angular-rate arrest time `hypot(pitch_rate,yaw_rate)/angular_accel`. Add this to measured response lag and turnaround speed braking lead. No hard-coded velocity fraction. Healthy and aft-50 branches unchanged.
- Implemented ballistic coast during navigation working-axis capture (`DynamicMotionSystem.cpp`), while attitude motors continue to rotate the hull; once within the existing `brakeAlignmentCosine` physical alignment tolerance, resume actual translation. The separate manual pilot retains ordinary input semantics.
- Localized emergency thrust axis marker in `src/assets/localization/ui/cockpit/flight.json` (en, ru, zh-Hans, es, ja).
- `PROJECT_STATE.md` remains ONE canonical journal on working branch. Current branch's implemented code was written through connected GitHub. It has **not** been compiled/tested here; no GitHub Actions workflows found, and the container cannot fetch the private GitHub repo. User must execute build script on local Windows 10 / MSYS2 Ninja environment before any PASS claim.
- Acceptance sequence: `git switch feature/assisted-engine-out-axis-2026-10-21`, `git pull --ff-only`, `powershell -NoProfile -ExecutionPolicy Bypass -File .\\scripts\\run-assisted-engine-out-gate.ps1`. Gate logs `D:\\tmp\\elite_engine_out\\assisted_engine_out_gate.log`. Then manually repeat three flights: unset `ELITE_SHIP_DAMAGE_PRESET`, `aft-50`, `aft-failed`, save engine/docking logs from `D:\\tmp`. Expected healthy/50 cases are regression invariants; failed aft case should show no parasitic yaw `motor_angular_a` during straight full-thrust, emergency HUD marker offset to side, and actual direction correction with ballistic reorientation first. If headless test/build fails, fix before game acceptance.
- Remaining limits: No evidence yet that complete damage case can dock, that single-engine multi-axis angular control is available for arbitrary commands, or that the fixed-source HUD perspective is pixel-perfect. Torque bound checks are fixed-cache preview, not an exact general 6DOF simultaneous optimizer. Branch is experimental, NOT production-merged.

---

## 2026-10-21 — ENGINE-OUT ASSISTED WORKING AXIS IMPLEMENTATION: FIRST EXPERIMENTAL PASS (NOT YET COMPILED / NOT GAME-ACCEPTED)

**Branch and return point:** Implemented ONLY on `feature/assisted-engine-out-axis-2026-10-21`, branched from isolated `test/engine-damage-three-flights-2026-10-20` (which itself derives from the restored pre-regression `main`). Do not merge/cherry-pick into `main` until actual build, headless tests and THREE real-game flights succeed. Native healthy Assisted and partial aft-50 deliberately retain existing behavior when no full aft loss.

**Implemented physics and policy:**
- `ShipParams.h`: emergency `assistedWorkingThrustAxisEmergency`, body-space `assistedWorkingThrustAxisBody`, `assistedWorkingThrustAxisAngleRad`; canonical ship +X right/+Y up/-Z nose.
- `ShipCapabilityBuilder.h::applyMountedEngineAxialAuthority`: only for one surviving aft engine, derive unit force direction `-arm/|arm|` from live slot placement (Cobra 26m left/right → 13m, 11.1m longitudinal lever) and current center of mass; verify cone including damage vectoring fraction before enabling. Healthy/50% leave `(0,0,-1)`.
- `ShipPresetEngineManeuvers.h::execute`: in emergency seed the exact primary thrust vector of the single intact aft motor through CoM, before mixing residual linear/angular basis motor commands; source forces/torques still `ΣF`, `Σ(r×F)` and physical installed actuators, NO fake vector force. Preview predicted actual body angular output from fixed wrench matrix, reject parasitic off-axis moment >0.01rad/s² when no angular demand, reject wrong-sign angular response, bound motor-driven angular magnitude and combined pitch/yaw to nominal safe load, reduce real motor commands instead of snapping rate. No expensive per-tick numerical optimizer. Angles: nozzle/exhaust outward ~49.5078°, thrust force inward through CoM, remaining outward cone ≈25.492°.
- `DynamicMotionSystem.cpp`: manual Assisted velocity stabilization and longitudinal response use live working thrust direction rather than hull nose when emergency. Navigation velocity-target path likewise projects targets onto working axis and allows ballistic translation while it is farther than existing brake alignment tolerance from desired course; attitude motors remain active. Assisted END with no fore/reverse bank now coasts while physically flip-aligning to anti-velocity, then brakes with actual surviving aft propulsion rather than inventing reverse thrust. Newtonian branch unmodified.
- `AssistedAutopilot.h`: retain ordinary velocity-course navigation as primary; in engine-out case additionally align actual physical working thrust axis to desired course, not nose; identify course response against effective working axis. Terminal docking pose remains hull-attitude-targeted.
- `ShipControlAuthorityTelemetry.h`: add true physically reachable `workingForwardAccelerationMps2` for emergency rather than treating impossible pure body -Z thrust authority as the only forward axis; include in loss revalidation. Shared authority passed to navigation.
- `DynamicMotionState.h`, `ShipControlExecutionGateway.cpp`, `WireDataSchema.h`: publish and REPLICATE current emergency flag and body working axis to cockpit; explicit wire schema modified to avoid dropping new field.
- `FlightVectorIndicatorRenderer.h/.cpp` + `SpaceState.cpp`: retain true center hull boresight and original factual velocity indicator; add a separate side-offset orange emergency thrust-axis reticle and localizable label `cockpit.emergency.thrust_axis` (fallback THRUST AXIS); mark offscreen location at viewport edge. Projection uses fixed HUD tangent scaling (approximate, NOT camera calibrated).
- `tests/navigation_runtime/AssistedAuthorityContractTests.cpp`: new engine-out hardware fixture asserts 26m width/49.5° CoM line, significant positive aligned acceleration on surviving right aft motor, no parasitic pitch/yaw/roll, nozzle angle within 75°; 50% baseline original tests retained.
- `scripts/run-assisted-engine-out-gate.ps1`: isolated Ninja navigation compile/test gate for `assisted_authority_contract`, `ship_physical_control_models`, `ship_control_execution_gateway`, `client_route_autopilot`, then full EliteGame build including HUD/wire. Log `D:\\tmp\\elite_engine_out\\assisted_engine_out_gate.log`.

**CRITICAL STATUS:** Connector-backed GitHub edits performed and source inspected; **NO actual C++ compilation, CTest, or real-game run has occurred in the tool environment** (repo is accessible through GitHub connector but source not locally cloned and container has no GitHub DNS). Do NOT claim tests PASS. User's Windows/MSYS2 local environment must first checkout feature branch and run `powershell -NoProfile -ExecutionPolicy Bypass -File .\\scripts\\run-assisted-engine-out-gate.ps1`. Fix any compiler/test errors before running live. Then run healthy, aft-50, aft-failed game cases as in the original log collection, inspect `D:\\tmp` logs and verify emergency marker/CoM-neutral thrust; compare cross-track, angular rates and true delivered torque. Important remaining: full combined 6DOF feasibility/timing guarantee, robust roll/one-engine maneuver control, camera-FOV-correct reticle projection, 3-flight acceptance, and navigation replan/low-speed doctrine when reverse fore bank damaged. This is experimental, not accepted completed work.

---

## 2026-10-20 — USER ACCEPTS DAMAGE-AWARE WORKING THRUST AXIS AS EMERGENCY ASSISTED DOCTRINE; MANEUVER-TIME AND ROLL LIMITS

**Accepted design direction (not yet implemented):** Define a physically computed WORKING THRUST AXIS from live installed engine capacities/geometric mounts + actual center of mass, with physically attainable zero/off-target torque and translational force. In fully healthy and moderately degraded Cobra it normally coincides with hull nose; when one aft engine is completely lost it MAY deviate from nose, with the straightforward single-survivor neutral line about 49.5078° and outward EXHAUST nozzle tilt. Physically valid axis determines assisted velocity/acceleration stabilization, emergency heading HUD marker, and flight-control achievable wrench. Do not confuse shift in thrust axis with shifting the real hull axis, actual velocity vector or center of mass. Damage change, handoff, and HUD marker should be stable/continuous where feasible, including real cargo/CoM variations. Real velocity marker must remain factual. Manual/autopilot must share physical control surface.

**Important geometry nuance:** CR-1's healthy nozzle has a **3D 75° thrust cone** from its nominal axial direction. Nominal emergency zero-moment position uses yaw-plane offset 49.5078°; nearest outward yaw boundary is 25.4922° further but 25.5° is NOT a separate independent ±25° allowance for BOTH pitch and yaw. A tilt away from the exactly CoM-aligned line produces (useful or unwanted) torque and changes acceleration direction. The two intact FORE engines in the 1-aft +2-fore topology can generate pitch, yaw and roll moments, including an approximately pure roll couple from opposite vertical gimbal force components with matching axial forces. With ONE surviving engine at `r=(x,0,z)`, `M=r×F`: `Mx=-z Fy,` `Mz=x Fy`, so its vertical-vectoring-induced pitch and roll are coupled and a free independently commanded pure roll cannot be guaranteed. One fixed off-centre engine thrust vector cannot in general deliver arbitrary 6DOF force+moment simultaneously, and zero-thrust coasting cannot create new attitude torque unless other actuators remain. Avoid claiming safe full 6DOF control on one engine.

**Time-to-maneuver rule:** Autopilot must use motor-specific simultaneous-wrench feasibility, actual angular acceleration and time to RAMP/ROTATE/BRAKE angular rate, plus linear velocity change (course capture) and ballistic drift. Body orientation is distinct from course-vector steering; if possible overlap controls instead of always summing durations. Ideal rest-to-rest fixed-angle angular slew with symmetric constant +/-α, ignoring latency and engine coupling, `t_min=2sqrt(theta/alpha)`. For theta=49.5078°=0.86407 rad: α=2.45166rad/s² => ~1.19s; α=1.121rad/s² (approx single healthy aft nozzle 10° offset from zero-torque trim at 100% thrust) => ~1.76s; α=0.562rad/s² (5° offset) => ~2.48s. At 400m/s, 2.48s coast = ~992m forward along previous velocity. A 49.5° offset maneuver should NOT be assumed mandatory on every course turn; it is the geometry of initial/terminal thrust-axis reconfiguration and not automatically a course change. Gimbal in this game model is nearly instantaneous, but manual angular-input ramp 0.25s and physically achievable angular acceleration, braking, actuator saturation are not.

**Fore-engine damage:** Without fore/reverse propulsion, ordinary nose-forward Assisted deceleration capability can collapse; doctrine may necessarily become NEWTONIAN-LIKE (inertial coast, controlled flip 180°, aft-main braking) even though installed topology remains Assisted-family and no arbitrary programmatic mode toggle is required. With one healthy engine spacecraft may still coast and make slow, cautious thrust/attitude maneuvers, subject to severe coupling, absent pure steering torque and lack of full attitude controllability. Autopilot must not promise terminal docking or any mandatory corridor motion unless controllability analysis proves them physically possible; controlled safe-coast/degraded abort otherwise.

**Future tests if user authorizes implementation:** 4/3/2/1 installed engines, damaged aft vs fore, working axis stability vs load, torque-null alignment, independent yaw/pitch/roll and sign symmetry including pure roll couple with fore pair, rest-to-rest angular slew with live motor/braking limits, 400mps coast distances and early lead, turning trajectory vs body rotation, reverse/flip-burn if fore unavailable, HUD triple markers, zero-torque straight engine-out flight; all preserve healthy and 50% LIVE PASS. Do not change engine dynamics or HUD in the present analysis turn. Only `PROJECT_STATE.md` is modified on isolated test branch; restored main is unchanged.

---

## 2026-10-20 — USER CORRECTION: OUTWARD NOZZLE + DAMAGE-AWARE VIRTUAL THRUST HEADING; MANEUVER FEASIBILITY (ANALYSIS ONLY)

**Important physical sign correction from user:** For surviving RIGHT AFT CR-1 at right x=+13m, aft z=+11.1m relative to CoM, the ENGINE EXHAUST/NOZZLE is tilted OUTWARD (starboard/right) ~49.5078° from the aft direction; the reaction thrust FORCE points INWARD (port/left) and FORWARD and passes through CoM: `F_dir=(-13,0,-11.1)/hypot(13,11.1)`. Prior narrative conflated nozzle and thrust directions. To keep force along desired world course, rotate nose/hull RIGHT relative to course by 49.5078° (in horizontal flight). Nominal CR-1 max 75° remains enough, 25.492° outward margin. For left aft survival the signs are mirrored. This is a source-backed geometry with current 26m left/right pair separation and 22.2m longitudinal hull length.

**User-requested physics/HUD doctrine, proposal not yet approved for implementation:** Damage-aware PHYSICAL FLIGHT LAW should derive attainable virtual 'working forward / propulsive reference' axis from installed motors and CoM rather than assume nose always equals heading. Pilot/autopilot still use ordinary ShipControlState; damaged Assisted stabilizer must stop forcibly chasing VREL against the HULL nose when trim axis is intentionally offset. HUD course/AIMING reference should shift off-center to display the reachable propulsive heading in cockpit; retain separate unchanged true velocity-vector marker and hull-nose marker; show angle and authority. This is fundamentally a degraded physical plant mode, not tricking the autopilot or moving the ship center mass.

**Maneuver analysis:** Pure main thrust through CoM generates translation with zero yaw torque, hence cannot itself change hull angular rate. To rotate (or arrest spin), deliberately deviate nozzle from neutral, or use the TWO still-alive FORE CR-1s, subject to combined acceleration/torque constraints; both angular control and coarse trajectory change remain physically possible. Exact surviving single main engine `alpha_yaw = -(F*sqrt(13²+11.1²)/I_yaw)*sin(delta)` where `delta` = nozzle angle relative to zero-torque 49.5078°; `F=9.56148375MN,I_yaw=25.3248667Mkgm²` coefficient 6.45395rad/s². Thus nozzle +/-5° from neutral produces ~-/+0.562rad/s² controlled yaw (at full instantaneous power); +/-10° -> ~-/+1.121rad/s²; outward adjustment limited to +25.492° before 75° cone; opposite side has 49.508° room to 0. These steering deflections also tilt the TRANSLATIONAL thrust force, and rotational braking needs deliberate opposite moment. Single motor zero-torque full-force along desired world acceleration gives 36.77494m/s² before shared load limits, but immediate velocity is inertial: if thrust points tangent to existing velocity, it changes speed only; bend/capture requires normal acceleration vector. Hull may rotate on COAST while actual velocity direction stays inertially fixed; no need to stop moving, but physical attitude changes on Assisted-I need engine firings (no magical RCS), which may create residual dV. To avoid excessive combined demand, suspend main aft thrust during attitude slewing, use available fore engines for pitch/yaw/roll and angular damping, then re-establish torque-neutral thrust line and resume acceleration. Not necessary to switch nose back to pre-damage axis for ordinary turns; possible for collision clearance/docking/other constraints. Pre-existing spin at full engines-off persists without control torque. Required flight reference on curve is the feasible acceleration vector (tangent acceleration + normal v²κ + correction), NOT blindly current velocity direction.

**Architecture implication:** Physical allocator should optimize feasible 6DoF wrench (force+torque) from all surviving motors and current CoM; zero-moment thrust-axis is feasible emergency mode, not necessarily the only or globally optimal 3-engine allocation. Damage-mode transitions and HUD marker should be smooth, remain consistent with persistent true velocity indicator, and retain obstacle swept-volume and terminal docking attitude. Need test steering moments ± yaw, angular stop time, coasting transition, braking via fore motors, corner tracking and HUD with unchanged healthy / 50%-damaged acceptance baselines.

**Action:** Discussion + geometry/math verification only. NO source or runtime changes. `main` restored checkpoint unchanged, state maintained only in this diagnostic branch's PROJECT_STATE.md.

---

## 2026-10-20 — PROPOSED ENGINE-OUT CENTER-OF-MASS THRUST AXIS (FEASIBILITY VERIFIED; NOT IMPLEMENTED)

**User concept:** With the left aft engine destroyed in Assisted-I, yaw/rotate the entire hull and gimbal the remaining right aft engine until the force line of action passes through the ship's current center of mass AND points along the desired travel direction. Treat that offset main-thrust line as the emergency propulsion heading, rather than requiring nose = travel direction. This eliminates the right engine's parasitic yaw torque without fake angular-rate clamps or discarding usable thrust.

**Exact source-backed geometry:** Assisted-I aft slots in `AssistedPhysicalControlModel.cpp`: left (-1,0,+1), right (+1,0,+1). Cobra logical hull 26.0m width ×22.2m length with centered CoM and `ShipCapabilityBuilder::applyDescriptorActuatorGeometry`: right aft engine offset `r=(+13,0,+11.1)m`. For zero torque from that engine, thrust FORCE (not exhaust plume) direction must be `-r/|r| =(-0.760494,0,-0.649345)` in ship local right/up/aft axes. Relative to ordinary aft push forward (-Z), required gimbal deflection is `atan(13/11.1)=49.5077708°`. Healthy CR-1 `CobrettyCR1VectorMainDrive.h` maximum cone is 75°, so feasible with 25.492° range margin; the surviving right aft engine is healthy when the LEFT is 100% failed. Turn ship hull heading ~49.5° to the right of desired travel heading (check exact signs in world transform) so oblique engine force maps to travel tangent. 9.56148375MN force /260000kg gives 36.7749375m/s² world-course acceleration at max engine output with zero moment from THIS engine. If hull is not rotated, components along nose and toward the destroyed engine are ~23.8796 and 27.9671m/s² respectively, still with zero moment. The centered force itself cannot slew/reorient hull, so use remaining fore motors for initiating/changing attitude and damping preexisting angular motion.

**Important limitations and architectural requirements:** `DynamicMotionSystem::applyLocalFrameInput` explicitly uses nose-based `desiredWorldVelocity = assistedTravelForward * targetForwardSpeedMps` and cancels lateral velocity relative to the nose; thus current manual-Assisted stabilizer would FIGHT a deliberate ~49.5° stable flight slip unless the physical flight law adopts a degraded thrust-axis reference (manual and autopilot alike). `VelocityCourseGuidance` navigation works against velocity-vector course rather than nose direction, so retain that; adjust actuator/Assisted stabilization semantics, not redefine planned route tangent as hull heading. `ShipPresetEngineManeuvers` currently mixes precomputed signed 6-axis basis programs rather than prescribing a continuous offset-CoM aligned thrust vector as its primary axial program. An emergency propulsion mode should calculate feasible zero-moment net wrench, not bypass the engine matrix. Current native ship has 3 surviving CR-1 motors (1 aft +2 fore), so a coupled multi-motor allocator may be able to trim yaw while keeping the hull closer to course and may be preferable to permanently flying yawed 49.5°. Steering velocity along curved route requires acceleration toward turn normal, not only along tangent: choose achievable acceleration FORCE vector and hull/thrust-axis alignment as appropriate; use other motors to generate/stop deliberate angular hull rotation while preserving torque bounds. At zero resultant torque a pre-existing hull spin persists until active physically executed damping. Distorted hull swept collision envelope and final docking attitude must be accounted for. Gimbal angle, thermal damage, CoM shifts must be live constraints; 75° is undamaged surviving-engine maximum.

**Action:** Feasibility analysis only, no flight source/HUD/planner/autopilot tests changed. Keep main at restored checkpoint. This is a candidate strategy, not accepted implementation nor successful runtime result.

---

## 2026-10-20 — FIXED 26 M ENGINE SEPARATION; DISTINGUISH STEERING FROM UNBALANCED THRUST (ANALYSIS ONLY)

**User-frozen geometry:** For Cobra Assisted-I the LEFT-RIGHT distance between paired main engines is EXACTLY **26 metres**; each is at lateral **x=±13 m** from centered nominal CoM. This is a deliberate hard physical modeling constraint for subsequent actuator force/torque and HUD work. It matches ship logical width and current `AssistedPhysicalControlModel.cpp` normalized ±X slots and `ShipCapabilityBuilder.h::applyDescriptorActuatorGeometry`. Do not confuse the 26m separation with each engine's torque arm: the arm is 13m.

**Critical correction of interpretations:** Two healthy aft main engines firing forward at equal thrust have opposite yaw moments, +4.908 and -4.908 rad/s², net ZERO (although deliberately commanded differential/fore-aft vectoring can create intentional moments). Loss of left aft produces uncompensated moment from right aft if it fires at full thrust. Damage at 50% produces raw mismatch approx 2.454rad/s² if right full and left half; total loss produces approx 4.908rad/s² if right full, before motor allocation compensates. This DOES NOT mean a one-engine ship has stronger commanded steering than a two-engine ship; it means the cancellation was removed. For all-failed-case runtime logs, both **`main_a_mps2=36.7749`** and **`motor_angular_a_radps2=4.90817`** repeatedly match EXACT theoretical unbalanced one-CR1 full-thrust values: F/m=9561483.75/260000=36.7749375m/s², F*r/Iy=9561483.75*13/25324866.6667=4.908191rad/s². This is strong circumstantial evidence of *uncanceled aft-engine moment* in these episodes, not conclusive axis-signed wrench evidence.

**Control-law requirement under discussion, NOT YET IMPLEMENTED:** The *physical actuator allocator/controller* must not allow actuator-generated angular acceleration that exceeds nominal safe commanded acceleration, `angularAccelerationLimitRadPerSec2` ~2.4516625rad/s² for Cobra, regardless of asymmetric damage. Must also actively aim for NEAR ZERO resultant yaw torque when commanded yaw is zero; merely capping accidental 4.908 to 2.452 still spins up indefinitely, so ZERO TORQUE TRIM comes first. No post-physics artificial angular-rate reset/clamp; adjust actual motor fractions/nozzle vectors, defer/reduce forward/lateral thrust when real simultaneous wrench is infeasible, prioritize predictable angular stability. Impact/external torque is separate physics and can legitimately produce >nominal rate; should be damped physically by actuators, not deleted. With NO other compensating moment and a need only to cap accel, single engine throttle would have to be <=2.4516625/4.908191≈0.4995; to remain at zero yaw while traveling straight, additional compensation or further reduction may be needed.

**Reaction time and stopping-envelope:** `ShipController::updateControlRates` has manual angular stick ramp 0.18→1 over 0.25sec. Both manual and Assisted autopilot using pitch/yaw input via `ShipControlState` share this path. After release, neutral angular damping demands `-rate*angularDamping` (`angularDamping=2.5s^-1`) subject to actual installed motor torque. Actual motor/nozzle reaction in current game is modeled instantaneous per physics tick once commanded; there is no independent physical gimbal servo lag for Assisted. `HullAttitudeControl` does use predictive braking `omega_target≈sqrt(2*a_available*remainingAngle)`; however it relies on scalar/pilot-learned angular authority, not measured signed available force+moment for concurrent dynamic acceleration/braking with damage. At omega=1.974 rad/s and optimistic decel 2.4517 rad/s², ideal braking lasts ≥0.805s and consumes ≥0.795rad ≈45.6deg, BEFORE extra command latency or reduced simultaneous brake authority. Hence course aiming must lead the turn by realistic motor-dependent reaction+braking distance/angle, not aim at the center and then wait.

**Next step only if user authorizes source edits:** one explicit six-DOF requested-vs-delivered signed force/torque telemetry lane, motor allocation that meets commanded yaw moment and rate/acceleration bounds before accepting commanded forward/lateral thrust, and autopilot turn/capture speed based on physically feasible combined lateral + angular correction with response delay. HUD needs nose/velocity slip, signed yaw rate, delivered vs requested angular acceleration/torque, required vs available lateral acceleration, motor effort/saturation and expected time to arrest spin. Preserve healthy/50% successful baseline and production restore branch; NO implementation or tests performed as part of this discussion.

---

## 2026-10-20 — PHYSICAL SOURCE OF 4.908 RAD/S² ANGULAR INSTABILITY IDENTIFIED (ANALYSIS ONLY)

User observes manual pilot with damaged engine: hull turns normally but velocity vector aligns ever more slowly to new nose; 50% damage still controllable, full engine loss feels transiently Newtonian. Asked why rapid hull rotation and what manual pilot experiences; no implementation requested.

**New exact numeric source-to-telemetry match:** Cobra CR-1 passport `nominalThrustNewtons=9561483.75`. Cobra logical dimensions width=26m, `applyDescriptorActuatorGeometry()` sets physical actuator half-extents (13m,2.5m,11.1m); Assisted-I `aft_left/right` have normalized X +/-1; `desc.physics.yawInertiaKgM2=25324866.6666667`. Therefore ONE full-strength off-center aft engine contributes `alpha_yaw=F*x/I=9561483.75*13/25324866.6666667=4.908191240888396rad/s²` if its yaw moment is not canceled by other engine vectors. `elite_assisted_failed.log` reports repeated `motor_angular_a_radps2=4.908` at sustained ~+/-1 yaw commands, a highly specific match; this identifies the physical thrust-lever mechanism for the observed magnitude but logs only the magnitude, so without signed per-engine yaw torque cannot prove exactly which motor(s) did/not cancel it in each frame.

**Command vs executed authority gap:** Cobra nominal `angularAccel=3.0rad/s²`, `maxGs=5`, `turnRadius=20m`. `ShipDynamics::angularAccelerationLimitRadPerSec2` gives `min(3, 5*9.80665/20)=2.4516625rad/s²`; `angularLoadRateLimitRadPerSec=sqrt(5*9.80665/20)=1.56577856rad/s`. `ShipController::applyRequestedAngularAcceleration` bounds the VIRTUAL desired angular change within these limits, but `ShipCore::updateMotionControl` derives a physically requested motor angular delta, resets rate to prior, and `ShipControlExecutionGateway.cpp` finally ADDS the actual motor torque/inertia to `transform.yawRate` with no second rate/acceleration clamp. The observed `hull_omega_radps=1.974` also exceeds nominal global controlled angular-rate limit 1.566. This is PHYSICAL angular acceleration from installed engines including coupled off-axis contributions, not proof that autopilot directly bypasses pilot control.

**Damage semantics from source:** `EquipmentDamage::evaluateEquipmentFunctionAtDamage01` sets authority=max(0,1-totalDamage) until profile fail. At isolated 50% impact on `ship_engine_L`, rated available max thrust of that ONE engine halves (4.78074MN ideal other constraints equal), and its 75-degree CR-1 vectoring angle also halves to 37.5deg (gimbal separate failure >=0.90). At 100% it provides ZERO thrust, gimbal failed, three other motors still work. It does NOT automatically switch the flight law to Newtonian. `DynamicMotionSystem::applyLocalFrameInput` in Assisted demands lateral correction `lateralVelocityError*strafeDamping`, prioritizes it over longitudinal throttle, but physical actuator allocator can only deliver within available combined 6DOF thrust and torque. Thus manual pilot feels increasingly long nose-vs-velocity slip (slower vector alignment) and worse asymmetric directional response, while hull can still turn rapidly thanks to remaining motors. Assisted manual angular neutral damping uses `-angularRate*angularDamping` as motor request, NOT instantaneous stopping. Normal manual stick has ~0.25s ramp to full angular command in `ShipController`; autopilot uses the same pitch/yaw/roll `ShipControlState` channel and, with repeated near-full yaw to chase course, holds high requested moment.

**Important unresolved:** Distinguish motor controller mixing of forward/main plus yaw demands vs uncommanded residual off-axis torque by signed per-engine forces/torques and desired/delivered angular acceleration; `motor_angular_a_radps2` scalar magnitude alone is insufficient. Current engine HUD provides capacity/actual thrust per motor, not instantaneous six-axis available authority, hull-vs-velocity slip, signed rotational torque and capture-time estimates. No source changes made; keep diagnostic branch separated and restored `main` untouched.

---

## 2026-10-20 — THREE RESTORED ASSISTED-I LIVE FLIGHTS ANALYZED; SOURCE UNCHANGED

**User's direct observation:** healthy and one aft-main with 50% impact damage are PASS; 100% failure is conditionally acceptable as convincing rough flight EXCEPT recurrent severe/uncontrolled hull rotations; request is ANALYSIS AND THEORIES ONLY, no controller/planner edits.

**Exact diagnostics:** Same native Assisted-I dock scene; preset validation `aft-50 status=APPLIED module=ship_engine_L impact_damage=0.500000`, `aft-failed status=APPLIED module=ship_engine_L impact_damage=1.000000`. Three supplied user logs: `elite_assisted_healthy.log`, `elite_assisted_aft50.log`, `elite_assisted_failed.log`, supplementary appended `elite_docking_planner(20261020-180833).log`, `elite_engine_damage_preset(7).log`. Parsed EVERY `[DockClientTrack]` sample (159 / 161 / 111). Each began with ~15.24° nose/course vs first tunnel tangent after selected `[AssistedSafeCourse] turn_deg=15`; this residual mismatch also occurs in the two completed flights and is NOT unique to engine-out.

| Observable | healthy | aft-50 | aft-failed |
| --- | ---: | ---: | ---: |
| max cross_track_m | 8.156 | 10.108 | 1134.160 |
| last remaining_m | 7.148 | 5.611 | 25517.1 |
| last actual_speed_mps | 0.0 | 0.0 | 61.312 |
| peak hull_omega_radps | 0.205 | 0.331 | 1.974 |
| peak hull_err_deg | 15.24 | 15.24 | 169.596 |
| peak motor_angular_a_radps2 | 0.338 | 0.338 | 4.908 |
| number samples abs(yaw)>0.95 | 11/159 | 12/161 | 86/111 |
| installed_turn_a_mps2 | 73.550 | 64.356 | 55.162 |
| max recorded required_curve_a_mps2 | 21.446 | 18.154 | 16.470 |

**Chronology of aft-failed:** At route page ~81: v=402.4 m/s, cross=38.9m, yaw command 0.36, hull angular speed 0.342 rad/s, required curve a=4.29m/s2 (< installed lateral basis 55.16). At page 89: v=398.2, cross=23.35, yaw=1.0, hull error 47.55°, motor angular acceleration=4.908 rad/s2, auxiliary lateral acceleration at max 55.162. At page 93: v=386.76, cross=91.40m (64m tracking corridor exceeded), hull speed 1.291rad/s, hull error66.27°, yaw+1.0, motor angular acceleration4.908. At page101: v=407.15, cross=334.43m, motor angular acceleration4.908. Page113: cross=501.90m, v398.4, target v400, turn limit 500 (straight/nearly straight). At page~143: hull speed peaks 1.974rad/s (113°/s); rotation fluctuates rather than increasing monotonically. At page238: hull error reaches169.60°, cross878.5m; flight ends page239 still ~25.5km from destination, cross1134m. Logs provide unsigned scalar angular-acceleration magnitude only, so they CANNOT prove the physical moment always had same sign, or distinguish intended yaw-actuation vs uncompensated off-axis motor reaction; do not assert either as established root.

**Verified relevant source behavior:** `DedicatedClientRouteAutopilot.h` computes Assisted bend speed as `sqrt((executionAuthority.lateralAccelerationMps2/3)/curvature)`, bounded by course response, angular rate and spline speed, with braking preview. This addresses BASELINE circular-arc requirement v²κ only. The `InCorridorCenteringGuidance` operates within tracking tolerance; outside it `CourseCaptureGuidance` selects capture direction, but source does NOT impose a separate recovery-speed ceiling using physically achievable required recapture vector + angular stabilization + braking. At long shallow curve with speed ceiling 500 and target 400, the corridor-occupancy blending cannot slow down a ship already hundreds of meters outside. `AssistedAutopilot.h` commands yaw/pitch via `HullAttitudeControl`, learns each angular authority from observed rates vs previous stick and speed response separately; this scalar identification cannot represent signed coupled yaw+translation authority lost through asymmetric engines. `ShipPresetEngineManeuvers.h` has distinct signed basis patterns, scales every motor when combined demands reach physical caps; this maintains feasibility but can underdeliver demanded combined force/torque. `ShipControlExecutionGateway.cpp` physically adds torque from installed motor allocator to angular rates, not a free/nozzle-bypassing autopilot.

**Most likely working theory (NOT PROVEN):** engine-out yields yaw-control/translation coupling + saturated yaw feedback. At 400m/s even nominal 55m/s² lateral authority turns velocity only ~0.138rad/s; meanwhile hull rotates ~0.45–1.29 and peaks 1.97rad/s, so nose/velocity separation rapidly becomes unmanageable. First xtrack blowout occurs before steep bend; thus not simply missing `v²κ` bound. With yaw saturated, 4.908rad/s² angular command/response and 55m/s² available lateral axis saturate simultaneously. Existing turn governor considers geometric curvature but not coupled 6-DOF recovery and heading slew under damaged allocator, and speed may stay 400 beyond 64m. Recovery mode then chases tunnel while physically rotating too aggressively, amplifying excursion. Directional moment, per-engine wrench delivery, and signed body-rate errors would be necessary to distinguish whether motor matrix introduces unintended torque vs pilot commanding excessive intentional yaw.

**Status:** Analysis only. No source, tuned settings, physics, tests, or safety-mode edits. Preserve restored `main`; keep the diagnostic branch isolated. User should choose next step only after theory discussion. If later authorized, address coupling/authority model and physically computed maximum speed including recapture plus attitude, without touching known-good healthy/aft50 baseline; first make observable signed demand vs delivered wrench rather than changing arbitrary gains.

---

## 2026-10-20 — THREE REAL-ENGINE FLIGHT COMPARISON, CHECKPOINT PRESERVED (TEST RUNS PENDING)

**Restored production baseline:** main at `ad084a1f0a91e08115728c4db1388ddefaabc230` has exact tree of checkpoint `596fd41114f524fd95330d458fd1fd75d900af7a`, no later damaged-engine autopilot fixes retained. User requires three comparable real-flight trials with Assisted-I: one healthy installed engine, left aft engine impacted at 50%, same engine failed at 100%. No more controller/planer redesign or test-for-test detours.

**Isolated temporary branch:** `test/engine-damage-three-flights-2026-10-20`, based on restored `main`. Only functional changes: re-added known historical test-only selector in `src/game/ship/debug/DamagedEnginePreset.h` and its opt-in GameServer bootstrap hook before planning. Env `ELITE_SHIP_DAMAGE_PRESET` omitted => healthy; `aft-50` => 0.50 impact damage on ONE actual installed aft engine; `aft-failed` => 1.00. The hook reports status=APPLIED|FAILED and installed module ID to D:/tmp/elite_engine_damage_preset.log; use real `ShipCore.moduleRuntime().addEquipmentDamage`, not a fake physics override. No autopilot, planner, HUD, ship physics or production branch changes. Verified by GitHub diff: exactly 2 changed source paths prior to this PROJECT_STATE.md entry. This branch is purely for reproducible live diagnostic and must NOT be merged into main.

**No native game run, no native build/CTest by assistant.** On Windows MSYS2:
```bash
cd /d/__elite/work
git fetch origin
git switch --track origin/test/engine-damage-three-flights-2026-10-20
bash build_mingw64.sh
mkdir -p /d/tmp
unset ELITE_SHIP_PHYSICAL_TEST_MODEL ELITE_ASSISTED_ROUTE_PRESET
unset ELITE_SHIP_DAMAGE_PRESET
./build/EliteGame.exe 2>&1 | tee /d/tmp/elite_assisted_healthy.log
ELITE_SHIP_DAMAGE_PRESET=aft-50 ./build/EliteGame.exe 2>&1 | tee /d/tmp/elite_assisted_aft50.log
ELITE_SHIP_DAMAGE_PRESET=aft-failed ./build/EliteGame.exe 2>&1 | tee /d/tmp/elite_assisted_failed.log
```
Run one program at a time, perform SAME dock route and same Assisted-I scene in each, close before next. Observe cross track, route hold/turn, abnormal angular oscillations, premature safety stops and finishing. Check damage preset `status=APPLIED` for the two damaged cases; if it says FAILED stop, as no valid damage test occurred. Main branch must remain at checkpoint. After comparison return via `git switch main` or `git switch -C main origin/main` ONLY if local main has no uncommitted work; do not force-reset.

---

## 2026-10-20 — SAME-SNAPSHOT STANDARD VS EXTREME: MEANINGFUL GEOMETRY DIFFERENCE; SPARSE HUD MAY HIDE IT

**User observation:** Extreme looks visually indistinguishable from Standard. User supplied `elite_docking_planner(2).log` (EXTREME only) and `elite_docking_planner(3).log` (same identical EXTREME record followed by STANDARD). Treat as ONE Extreme and ONE Standard experiment, not three independent runs.

**Facts directly from user logs:** Both start (-10000,2500,0), goal (668.255,350,2659.507), start speed=0, vmax=500, acceleration/lateral braking constraints 17.652, direct 11202.997m, fixed 700m capture + 1200m BLUE volume + 150m release, terminal approach 900m, inbound hard goal 7138.013651m. Actual `assisted_preset` and optimizer `preset` agree per run.

STANDARD: total 48579.862490m; inbound 34133.101035m, ETA 100.812542s; last 12396.761455m, ETA 75.745003s; composed minimum sampled radius 955.047950m; 13 curves, 105 HUD gates, 669 execution gates, worker_ms 10340.614.
EXTREME: total 41782.855758m; inbound 29314.755329m, ETA 90.152742s; last 10418.100429m, ETA 62.116780s; composed minimum sampled radius 223.262971m; 12 curves, 92 HUD gates, 640 execution gates, worker_ms 11055.136.

DIFFERENCES: -6797.006732m (-13.9914%) total length, -4818.345706m inbound, -1978.661026m terminal, ~4.28x tighter reported composed minimum radius. Nominal ETA incl same mandatory 2050/220 = 185.875727s Standard vs 161.587704s Extreme (-24.288023s, -13.07%) using local optimizer ETA, NOT an actual flown full-global speed profile. Worker runtime not significantly faster; EXTREME was ~0.715s slower to calculate.

**Why visually similar despite math:** BLUE path and authored straights are unchanged; free legs share mandatory traffic portal topology. More importantly, `RoutePlanner.cpp::buildPresentationGates` still samples `routeCurves` at fixed `gateSpacingMeters=500m` and `terminalGateSpacingMeters=250m`. `GuidanceHudPresentation.h` uses exactly those sparse HUD frames without further interpolation, and `GuidanceCorridorRenderer.cpp` draws their corners/connectors. Therefore EXTREME curves with ~223m assembled sampled radius (and ~62.1045m MINIMUM inbound optimizer sample radius!) can be poorly resolved by 500m HUD gate spacings; visual comparison of rib tunnel alone is insufficient to assess mathematical curvature. Separate dense execution gates exist but do not directly become the displayed tunnel. Do NOT change cinematic standard spacing or constraints as part of analysis. Potential opt-in Extreme-only adaptive HUD sampling (not implemented) could make shape differences obvious, keeping geometry unchanged.

**Important diagnostic mismatch:** `inbound_detail min_radius_m=62.104519` (optimizer sample) vs assembled `segmented min_radius_m=223.262971`. These are DIFFERENT discretization metrics: the latter samples each compiled curve only 32 times with `minimumSampledRadiusMeters`, potentially misses narrow curvature peaks. Before safety/visual claims, verify actual differential curvature maxima at high-resolution sampling and the entire execution gate speed profile. In EXTREME `peak_lat_util=1.047810` inbound and `1.001753` final, despite declared feasibility; this is a segment-level calculated authority utilization above 1 and should be investigated rather than claiming all authority limits strictly respected in execution. Hard validator `out.feasible` checks `maxViolation`, which does not directly reject these `peak_lat_util` summary numbers. Leave physics unchanged until precise cause is verified.

**Changes in this turn:** No source code/physics/planner modifications; this analytical note only. User may share equal-camera screenshots to compare geometry if still visually identical. For future work, consider a visual audit of gate spacing in Extreme and curvature sampling validation, but preserve Standard's visual 500/250m rule and default behavior.

---

## 2026-10-20 — COMPLETE DOCKING TERMINAL LOCALIZATION + PRESET LABEL (SOURCE COMMITTED; NATIVE TESTS PENDING)

**User request:** Always display the active Standard/Extreme planner preset on the in-game docking terminal; every visible terminal message must use the canonical localization catalog, with no hardcoded human-readable text. Do not modify planner geometry, ship physics, flight laws or experimental preset behavior.

**Changes committed on sole authoritative `main`:**
- Canonical `src/assets/localization/ui/cockpit/flight.json` now contains localized templates for all docking terminal stages, planner metrics, explicit STANDARD / EXTREME labels, ship/control-law/engine group identifiers, safety status and handoff, BITE fault severity/detail/suffixes and classified navigation failure reasons. All five enabled locales: `en`, `ru`, `zh-Hans`, `es`, `ja`. Existing global locale setting remains authoritative; no parallel translation table.
- `src/game/presentation/DockPlannerTerminalTelemetry.h` now accepts an injected UI translation lookup and substitutes actual numbers into catalog templates. All visible labels/units including live elapsed time, V0/Vmax, braking/lateral/acceleration authority, obstacle count, true completion time, route length, curves, per-free-leg worker times, ETA, evaluations, and BOTH preset names are translated. Parsing stable machine-readable `diagnosticSummary` identifiers (`eta_s=`, `inbound_detail`, etc.) remains internal and untranslated by design.
- New `src/game/presentation/DockTerminalBitePresentation.h` translates typed BITE faults on the UI side. Existing `BiteApi::terminalLines` legacy English implementation remains in domain API for compatibility/tests but is no longer used by cockpit terminal; engine ID is a stable technical identifier, not a generated English sentence.
- `SpaceState.cpp` builds ONLY translated terminal DTO text: planner preset line is visible during an active solve and after adopting an accepted route; physical model family label is localized while immutable I/Y/X layout code is retained; flight law and actuator group labels are localized. Existing project statuses and warnings use localization keys instead of hardcoded fallback strings. Source technical diagnostics remain unchanged in normal console/`D:/tmp` log; terminal maps known failure reasons to translated actionable categories, falling back to localized “see log” for unclassified errors rather than leaking untranslated backend text. Pseudo-random `CALC` was removed in previous step and stays removed.
- `DockingStatusTerminal.h` no longer owns English `DOCKING` or `BITE NOMINAL` fallback strings; all texts are supplied through localized DTO. `localeId` triggers history/typing reset on language switch so old-language and new-language lines do not mix.
- `tests/navigation_runtime/DockingStatusTerminalTests.cpp` updated for new localization-aware formatter interface and checks Russian Extreme label, translated numeric lines, translated BITE faults and failure category, and live locale switch handling.
- `tests/architecture_contracts/LocalizationContractTests.cpp` asserts catalog translations for presets in all five active languages and scans EVERY `cockpit.terminal.*` catalog entry to ensure non-empty values in `en/ru/zh-Hans/es/ja`, bypassing fallback masking.

**Validation boundary:** GitHub source re-read and static key coverage audit PASS: 64 unique terminal/flight-law keys used by terminal source and adapters are present with nonempty translation in all five locales; no decorative CALC, old BiteApi UI call, raw SHIP/ACT/ROUTE MODE English assembly, direct untranslated guidance error or untranslated status warning in the visible terminal block. **Windows MinGW build, navigation_runtime CTest, architecture_contracts localization CTest and live font/layout verification have NOT been run**. Preserve an honest PENDING status until user reports these results.

**Recommended MSYS2 MinGW64 checks:**
```bash
cd /d/__elite/work
git pull --ff-only origin main
cmake --build build/tests/navigation_runtime --target docking_status_terminal_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^docking_status_terminal$' --output-on-failure
cmake --build build/tests/architecture_contracts --target localization_contract_tests -j 8
ctest --test-dir build/tests/architecture_contracts -R '^localization_contracts$' --output-on-failure
bash build_mingw64.sh
```
Run the SAME docking route first without `ELITE_ASSISTED_ROUTE_PRESET` and then with `ELITE_ASSISTED_ROUTE_PRESET=extreme`. Inspect localized preset label during and after planning. Switch global UI locale (existing Ctrl+Alt+F12) while terminal is visible and check no stale language remnants or glyph rendering. Never alter Standard route geometry to fix a terminal string.

---

## 2026-10-20 — ASSISTED ROUTE STANDARD/EXTREME EXPERIMENT + REAL DOCK TERMINAL TELEMETRY (SOURCE COMMITTED; NATIVE BUILD PENDING)

**User intent:** Keep the existing Assisted planner unchanged in standard production. Make one opt-in `Extreme` experiment removing cinematic/visual minimum turn radius and comfort preferences to measure how much route geometry/time depend on visual corridor design. Never bypass real ship capability, hull/obstacle collision, mandatory BLUE transit or authored 700m entry/150m exit. Also replace decorative random-looking CALC hex numbers in the cockpit docking terminal with actual calculation parameters/results.

**Implementation committed on the sole active `main` branch:**
- `src/game/navigation/planner/RoutePlannerApi.h`: explicit `AssistedRoutePreset::{Standard,Extreme}`, default Standard; passed by value across the existing Assisted-only pipeline and segmented free legs. Newtonian planning receives no new logic.
- `RoutePlanner.cpp::planContinuousAssisted`: Standard keeps the SAME `requiredGuidanceTunnelRadiusMeters(500m gate, hull length)` floor (~955m) and SAME original objective weights. Extreme sets `minimumComfortRadiusMeters=0` and zeroes the SOFT reserve/comfort/curvature/angular smoothness penalties, retaining travel-time and path-length objectives and ALL hard constraints in the unchanged `ContinuousAssistedPathOptimizer::evaluate`. This is an experiment; Extreme may yield ugly/unreadable tight tunnel curves, hence Standard is the default.
- `SpaceState.cpp`: launch-time environment `ELITE_ASSISTED_ROUTE_PRESET=extreme` (or uppercase EXTREME) opts the Assisted client into Extreme per docking request. If absent, Standard. Compare **same dock/stationary start** in two processes, without a permanent setting or changes to pilot/autopilot execution logic. Output includes `continuous-assisted preset=STANDARD/EXTREME` and `[DockPlannerRequest] assisted_preset=...` in the same `D:/tmp/elite_docking_planner.log`. Planner input speed/acceleration/obstacle clearance and authored BLUE constraints are unchanged.
- Removed the former `SpaceState.cpp` presentation-only pseudo-hex `CALC XXXX XXXX...` activity stream, which was derived from request serial/stage/progress/time, NOT real solver values. A new pure `src/game/presentation/DockPlannerTerminalTelemetry.h` derives live stage from actual async worker state (PREPARE / GEOMETRY / FRAMES / CONTROL / COMPLETE), real monotonically elapsed wall time (3-second UI update buckets to avoid typewriter backlog), input V0/VMAX, actual accelerating/braking/lateral acceleration and obstacle count, plus chosen preset. After adoption, terminal displays actual worker duration, route km and curve count, per-free-leg solve times, optimizers' per-leg `eta_s` and `evals` parsed from the existing `RoutePlan::diagnosticSummary`. The ETA displayed is explicitly two free-leg estimates (not a guaranteed full execution ETA). `DockAdvice` retains measured worker milliseconds and the request preset.
- `DockingStatusTerminal.h` adds independently updated `plannerDetails` lines to its presentation DTO, separate from BITE faults. No terminal-to-planner calls or flight-state effects.
- Source tests extended: `tests/navigation_runtime/RoutePlannerApiTests.cpp` checks default Standard and explicit Extreme, zero extreme HUD radius floor and intact final docking endpoint on a synthetic physical free leg. `tests/navigation_runtime/DockingStatusTerminalTests.cpp` checks formatting of preset, real elapsed seconds, actual acceleration limits, per-leg worker timings, ETA and evaluation counts. No new parallel state file.
- Connector re-read static source checks: all required integration markers PASS (correct request declaration order, no old `nextHexWord`/hex CALC, log preset, real monotonic time, Standard default, unchanged optimizer hard constraint integration). **No native Windows build/CTest or game visual preview has been run yet. Do not claim runtime success or any Extreme route length/ETA until user reports it.**

**Minimal MSYS2 MinGW64 native validation:**
```bash
cd /d/__elite/work
git status -sb
git fetch origin
git pull --ff-only origin main
cmake --build build/tests/navigation_runtime --target route_planner_api_tests docking_status_terminal_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^(route_planner_api|docking_status_terminal)$' --output-on-failure
bash build_mingw64.sh
# First Standard baseline, with preset unset:
unset ELITE_ASSISTED_ROUTE_PRESET
./build/EliteGame.exe
# Then relaunch from the same stationary scene:
ELITE_ASSISTED_ROUTE_PRESET=extreme ./build/EliteGame.exe
```
Each time request the same dock, inspect the drawn route and the appended `D:/tmp/elite_docking_planner.log`. Full docking acceptance is NOT necessary for visual/ETA comparison; a route appearance screenshot and that log suffice. If the test fails, return FIRST native compilation/CTest error instead of skipping. Compare stage lengths (inbound/BLUE/terminal), full length, calculated ETA, minimum radii, objective and feasibility/rejected candidates. If Extreme fails constraints or draws too-tight HUD tunnel, do not relax hull or physical gates and do not alter Standard baseline. Active development + context remains solely `main` and root `PROJECT_STATE.md`.

---

## 2026-10-20 — REJECTED ROUTE CANDIDATES: FIRST REAL WINDOWS REPORT

New user log `elite_docking_planner(1).log`, second report: selected total 48728.543 m. Inbound free leg 34281.782 m, ETA 101.064 s; mandatory traffic 2050 m, approximately 9.318 s at 220 m/s; final free leg 12396.761 m, ETA 75.745 s. Nominal total ETA 186.127 s. The old report in the same file predates candidate instrumentation.

Shortest feasible candidates: inbound 31187.522 m, ETA 114.017 s; final 12235.242 m, ETA 76.354 s. Combined approximate total 45472.764 m and 199.689 s, 3.256 km shorter but 13.562 s slower than selected.

Fastest feasible candidates: inbound 34880.584 m, ETA 99.509 s; final 12515.286 m, ETA 75.636 s. Combined approximate total 49445.870 m and 184.463 s, 717 m longer and 1.664 s faster than selected. Selected weighted objectives are lower: inbound 2.673129 vs fastest 2.686599; final 6.792346 vs fastest 6.808122. This supports user claim that longer paths can be faster; the selected solution trades ~0.9% ETA for reserve/shape preferences. All comparisons are made with optimizer ETA, not independent final execution-profile validation.

Terminal feasible evaluations: 806/4726 (17.1%); inbound 2979/4814 (61.9%). Crucial rejected independent terminal search branch: 8891.814 m, ETA 56.413 s, violation=1.0, infeasible. The current optimizer sets violation=1.0 when sampled geometry intersects navigation obstacles; this suggests collision, but exact cause and blocker remain unlogged. Two other terminal branch outcomes at 12087.017m and 12306.405m also violate constraints (0.044247 and 0.039610). Do not count any of these as feasible improvements.

No planner source, weights or physics changed during this analysis. Next diagnostic: log per-reason constraint failures and blocking obstacle identifiers for each rejected independent branch, then compare executable ETAs with the same final speed cap. Final docking acceptance remains unproven.

---

## 2026-10-20 — INSTRUMENT REJECTED ASSISTED CANDIDATE ETA WITHOUT CHANGING THE SEARCH (SOURCE COMMITTED; NATIVE TEST PENDING)

**User idea:** Since selected-route ETA (~186s) agreed reasonably with the observed near-complete docking flight, inspect feasible routes discarded during optimization to see whether any were faster, rather than deducing inefficiency from length or guessing about alternative geometry. A longer route may truly be faster. **The OLD `D:/tmp/elite_docking_planner.log` did NOT retain rejected candidate ETAs**, only the selected route and aggregate 4,682/4,726 evaluations. Old candidates cannot be recovered from that text log; a new same-scene route calculation is required.

**Changes on authoritative `main` (no new persistent state document):**
- `src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.h/.cpp`: passive `CandidateAudit` records all pre-existing objective evaluations (13 seed evaluations plus 11-dimensional pattern-search evaluations; no extra `evaluate()` calls). Counts feasible/infeasible objective evaluations. Retains **5 shortest feasible**, **5 fastest-ETA feasible**, and **5 lowest-objective feasible** candidates, each with evaluation id, length (m), predicted ETA (s), objective, minimum radius and accumulated curvature. Also retains 4 independently optimized seed-branch end results (rank, feasibility, length, ETA, score, violation), to distinguish genuinely different optimized starting branches from nearly duplicate parameter tweaks. Infeasible candidates are NOT treated as valid competitors.
- `src/game/navigation/planner/RoutePlanner.cpp` appends the above bounded data to existing `diagnosticSummary`. Consequently the existing `src/game/navigation/debug/DockPlannerFileLog.h` sink writes them in `[DockPlannerResult]` inside the **same** `D:/tmp/elite_docking_planner.log`. Keys: `feasible_candidate_evals`, `infeasible_candidate_evals`, `shortest_feasible_candidates`, `fastest_feasible_candidates`, `lowest_score_feasible_candidates`, `optimized_seed_branches`. Candidate tuples are `evaluation-id:length-m:eta-s:objective:min-radius-m`. Branch tuples are `rank:feasible:length-m:eta-s:objective:violation`. This is metadata only, not full alternative line geometry.
- `tests/navigation_runtime/ContinuousAssistedOptimizerLabTests.cpp`: added audit consistency, feasible candidate ordering, candidate-count conservation, upper bound of 5 retained per metric and branch count 1–4, for reversed-heading and high-speed test problems.
- Static source audit via GitHub re-fetch **PASS**: all three original evaluation call sites observed, unchanged total of 3 calls to `evaluate` in source (plus function definition), two unchanged `patternOptimize` invocations, unchanged objective weights (time 1.0, path length 0.35), diagnostics included in existing disk log path. **No native compiler, CTest, live optimizer execution or user game's disk log rerun has been performed by assistant; behavior PASS is NOT claimed.**

**Interpretation caveat:** Candidates are compared with the SAME existing optimizer ETA predictor (`designSpeedMps`, normally 500 m/s); the composed execution route subsequently imposes a 0.8 cruise cap (400 m/s). Logged alternatives allow a fair comparison WITHIN this predictor, but do not prove the real selected route beats those alternatives under the final capped global speed profile. If results are close, recompute shortlisted candidates through the identical final execution speed-profile model before considering any ranking change. Some discarded feasible candidates may be faster but more expensive in other weighted safety/comfort penalties; inspect ALL columns.

**Minimal user verification (MSYS2 MinGW64):**
```bash
cd /d/__elite/work
git status -sb
git fetch origin
git pull --ff-only origin main
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime --target continuous_assisted_optimizer_lab_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^continuous_assisted_optimizer_lab$' --output-on-failure
bash build_mingw64.sh
./build/EliteGame.exe
```
Request/show exactly the same dock route; no complete flight required. Examine the new report appended to `D:/tmp/elite_docking_planner.log` for the six candidate audit keys inside `inbound_detail` and `final_detail`. Delete or rename the old file before the run only if a clean one-request file is desired. Send the new log to compare best shortest vs fastest vs selected and the four search branches, per leg. If tests/build fail, return the FIRST native compiler/CTest failure, do not claim native PASS.

**Next engineering decision, NOT IMPLEMENTED:** Based on observed candidate ETA/length data, either leave time-first objective alone if chosen ETA really wins the physically executable comparison; or isolate a ranking/search deficiency with a same-snapshot feasible counterexample. Do not remove BLUE physical containment, 700m/150m authored straights, or change startup control modes in this diagnostic step.

---

## 2026-10-20 — CORRECTION: BLUE 220 m/s IS TRAFFIC DATA, NOT A PREPLANNING MODE; FLIGHT TIME CHECKED AGAINST LIVE TRACK

**User clarification (authoritative design):** Three startup/preplanning cases are distinct from traffic speed limits: (1) begin at rest, remove accelerations and rotation; (2) manual-control preparation, cancel accelerations/rotations and brake to rest; (3) automatic moving-start preparation, cancel acceleration/rotation and coast at existing velocity. None includes a user-specified fixed 220 m/s speed. Prior explanation improperly mixed these concepts; do not repeat that assumption.

**Where 220 m/s ACTUALLY comes from:** `src/assets/data/navigation/hub_traffic_route_graph.json` authors `cylinder_b.blue_lane.max_transit_speed_mps=220` and portal crossing max 220. `SegmentedRouteComposer.cpp` converts this maximum to `handoffSpeed` and prescribes `legRequest.terminalSpeedMps=handoffSpeed` and `currentSpeed=handoffSpeed` after mandatory stage. This treats an infrastructure speed CEILING as an exact free-leg boundary speed in local optimization, though the later composed global speed profile can still reduce speed. This is not a preparation policy. Whether the authored lane needs a 220 m/s cap is a separate gameplay decision; no user approval for this constant has been established. The inbound hard goal straight 7138.013651m is the inherited max-speed (500) to handoff-speed (220) `requiredTerminalApproachMeters` calculation, not a measured braking requirement for this initially stationary ship.

**Important correction on route quality:** Small path-length penalty and preference for ETA do NOT establish that longer route is slower or suboptimal. A longer feasible route CAN be fastest; ranking quality requires an actual competing physically feasible candidate with a common executable speed model. No such alternative is recorded in this log. Both-leg optimizer ETA estimates are real outputs; their accuracy can be tested against observed flight progression but not against an unknown shorter candidate.

**Measured live time for selected route:** Reexamined uploaded `Вставленный текст(20261019-211248).txt` (155 `[DockClientTrack]` samples from 48604m remaining to 53.5956m remaining). Numerically integrating successive route progress delta / average `actual_speed_mps`, interpolating at exact stage boundaries, yields approximately: inbound 34,157.205m at t=105.82s; BLUE capture+transit+release 2050m at t=115.15s (9.33s for block); final 12,396.761m from t=115.15 to t=184.42 at last observed 53.6m remaining (69.26s so far), last 53.6m conservatively only approximated at ~2–3 additional seconds with final deceleration. Therefore near-complete live route ~187s. Optimizer ETA inbound=101.129s, final=75.745s, plus mandatory block minimum ~2050/220=9.318s, estimated sum ~186.19s. Overall duration agrees approximately (~1%), but individual legs compensate (inbound live ~+4.7s vs ETA, terminal live ~-3-4s vs ETA); closing window means FINAL HOLD or docking success remains UNPROVED.

**Potential speed-profile consistency gap:** `ContinuousAssistedPathOptimizer::speedLimitForCurvature` uses `request.designSpeedMps=base.maxSpeedMps=500` in candidate ETA, while composed `recomputeGlobalSpeeds` applies `cruise=0.8*base.maxSpeedMps=400` to final execution gates. Accuracy of total observed ETA here does not validate consistent candidate RANKING under different caps. Before tuning weights or shortening straights, log and compare candidate execution-profile ETA using same downstream 400 cap, acceleration/braking, BLUE limits, dynamic curvature/angular restrictions. Candidate comparison must include shortest FEASIBLE, fastest FEASIBLE, actual selected and safety margins.

**Geometry insight:** A broad constant-radius turn is not automatically faster under lateral acceleration ceiling: for angle θ, length θR, maximum turn speed sqrt(a_lat R), minimum idealized turn time θ sqrt(R/a_lat), which increases with R if no other constraints intervene. Avoid unsupported claims that large loops must save time.

**Changes:** Analysis and root `PROJECT_STATE.md` only. No planner runtime code, 220 m/s rule, route geometry, mode logic or tests modified.

---

## 2026-10-20 — PLANNER PATH-LENGTH DIAGNOSIS FROM FIRST PERSISTENT WINDOWS REPORT (ANALYSIS ONLY)

**Evidence:** User uploaded `D:/tmp/elite_docking_planner.log` with real `[DockPlannerResult]`. Current `main` source inspected at `cc3ae6b` plus state-only commits. Report: ship start (-10000,2500,0), dock HOLD (668.255,350,2659.507), direct distance 11202.997m, initial speed 0m/s, nominal max speed 500m/s, planner braking/lateral 17.652m/s². Valid route 48603.966m with stages: inbound free 34157.205m; mandatory BLUE capture 700m, KeepInside transit 1200m, release 150m; terminal free 12396.761m. Geometry preview/live file logging works; no completed landing acceptance claimed.

**Hard diagnosis #1 — inherited maximum-speed terminal brake bound in inbound free leg:** `src/game/navigation/traffic/SegmentedRouteComposer.cpp` computes the inbound virtual goal hard straight with `OrdinaryRoutePolicy::requiredTerminalApproachMeters(terminalRequest.maxSpeedMps, handoffSpeed, terminalRequest.brakingMps2,...)`. Logged `inbound_terminal_hard_m=7138.014` equals 1.25*(500²-220²)/(2*17.652). The craft actually starts this test stationary and enters BLUE at prescribed 220m/s (not necessarily at 500 on preceding free leg). The optimizer already has global forward/backward physically feasible speed profiling. This multi-km deceleration reserve is being converted into a mandatory geometric straight before an additional 700m mandatory portal-entry straight. Treat as a suspected planner-input design mistake; do NOT simply remove speed feasibility or mandatory 700m portal clearance. Compare physically validated alternatives with virtual inbound goal hard straight determined by actual portal alignment/clearance, not max-speed braking distance.

**Hard diagnosis #2 — objective conflicts with stated ordinary policy:** `src/game/navigation/OrdinaryRoutePolicy.h` explicitly prioritizes valid/comfortable geometry then shortest distance, using time only as tie-breaker. The ACTIVE `src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.cpp` instead minimizes normalized ETA (weight 1.0) plus modest squared path stretch (weight 0.35) and curvature/control penalties. Measured score decomposition from logged `eta_s`, `lower_bound_m`, `stretch`, `total_turn_rad` and defaults: inbound score 2.675, ETA term 1.942 (72.6%), length penalty 0.034 (1.3%), turn penalty 0.408; final score 6.792, ETA term 5.535 (81.5%), length penalty 0.231 (3.4%), turn penalty 0.639. Thus long, fast smooth routes can outrank shorter slower ones, contradicting declared policy. First evaluate shortest FEASIBLE vs fastest FEASIBLE vs selected candidate, report Pareto alternatives. Do not blindly multiply path-length weight without checking physical collision/curvature/longitudinal feasibility.

**Other confirmed bounds:** per-leg optimizer diagnostics inbound length 34157.2 vs geometric chord-with-protected-segments lower bound 26036.67 (stretch 1.3119), total unsigned curvature 4.083rad (~234°); final 12396.76 vs lower bound 6842.22 (stretch 1.8118), accumulated turn 6.395rad (~366°). The lower bounds are not globally collision/topology-feasible minima. Each optimal candidate saturates geometric radius floor ~954.93m, imposed by 500m HUD gate spacing and 30°/frame policy, NOT simply by available propulsion. Two cubic Beziers and optional middle straight optimized using 13 seeds and 11-dimensional coordinate local search; optimality is local, not certified and self-loops may persist.

**Traffic and protected line caveats:** `src/assets/data/navigation/hub_traffic_route_graph.json` and `SpaceState.cpp` force the cylinder_b entrance/transit/exit for cube_a dock-front in this test. Therefore global direct start-to-dock line 11.2km is not a valid comparison or a substitute for traffic constraints. The compiled mandatory outbound 150m straight is already included before `currentStart=blockEnd`; `finalRequest.initialProtectedStraightMeters=150` adds another 150m after this, yielding 300m post-portal alignment. This is a small double-protection candidate to audit, not the multi-km root cause.

**Logged diagnostic flag caveat:** `[DockPlannerRequest] continuous_assisted=0` shows the original client request, while `AssistedPlanningPipeline::segmented` clones it with `useContinuousAssistedOptimizer=true`. Both per-leg `continuous-assisted ready` diagnostics prove the active optimizer DID run. This flag must not be taken as evidence the continuous optimizer is off.

**Planning cost:** 11,388ms total worker; inbound_ms 5,300.94; final_ms 5,678.55; free-leg total_ms 10,979.50; evals 4,682 and 4,726 respectively. Optimization consumes most measured worker time. Do not mix path-length goal with computation-time goal; first diagnose geometry then benchmark bounded search.

**Recommended next controlled work (not executed):** Preserve source and logs; instrument leg start/goal, portal boundary points and tangent vectors, blocked candidate reasons, top-k feasible candidate length/ETA/reserve/turning, and score components. Prove with exact-snapshot A/B that the first leg can safely shed maximum-speed-derived 7138m hard straight without violating 220m/s BLUE arrival, 700m ingress, 150m egress, clearance, 955m radius or all movement/speed limits. Then change ranking to match agreed policy and broaden topological candidate family only if the shorter feasible set is still inaccessible. Validate 3D route geometry before any full flight test. No production changes or Windows tests performed in this diagnostic analysis.

---

## 2026-10-20 — MAIN IS NOW AUTHORITATIVE (FAST-FORWARD, NO HISTORY REWRITE)

**Confirmed GitHub operation:** `main` was fast-forwarded from `52ec1372706e067193807be35e1a1b04e98ed1f2` to `cc3ae6b7819409757483d334f913bf7701b287d8`, the exact then-current HEAD of `feature/propulsion-authority-matrix-2026-10-16`. Compare reported **1336 commits ahead / 0 behind** before the update; post-update both refs pointed to `cc3ae6b`. No force push, merge-conflict resolution, squash or rebase occurred. All code, tests and this project's existing state entries moved into the default `main` branch unchanged.

**Repository workflow decision:** The project is developed as one linear stream. **`main` is the single active development and project-state source**. New implementation and updates to the sole root `PROJECT_STATE.md` belong on `main`; do not continue development in stale feature branches by default. Before high-risk experiments, make a named recovery branch or immutable tag without changing active development ownership. User's local Windows worktree may still be switched to the old feature branch; switching locally to `main` is a separate action requiring attention to uncommitted changes.

**Other branches inspected:** 15 older feature branches are ancestors of the new `main` (0 unmerged commits); `feature/propulsion-authority-matrix-2026-10-16` is identical to new `main` before this documentation commit. **Exception:** old 2026-10-11 experimental `feature/assisted-optimizer-planner-v3-2026-10-11` diverged (15 independent commits, 1125 commits behind) and adds its own `AssistedRoutePlanner` v3 backend; it was deliberately NOT merged into current production because blindly injecting this obsolete experimental implementation risks replacing the newer continuous Assisted optimizer. Keep it as an archival experimental reference, not a second active development line. Recovery branches also remain untouched; do not forcibly rewrite or delete them.

**Current verification boundary:** The prior recorded `D:/tmp/elite_docking_planner.log` already demonstrates that planner file logging ran in the user's Windows build; this GitHub branch advancement changes no runtime logic. For further geometry/runtime diagnosis, use main's `PROJECT_STATE.md` and continue with route screenshots + `D:/tmp/elite_docking_planner.log`.

---

## 2026-10-20 — AUTOMATIC DOCK PLANNER REPORT TO D:\\tmp (SOURCE COMMITTED; WINDOWS BUILD NOT YET RUN)

**User asks:** save planner diagnostics automatically into D:\\tmp to upload after future route-only tests; no need to redirect all game stdout or complete a docking flight. Preserve single root `PROJECT_STATE.md` and keep generated runtime logs OUTSIDE repository.

**Source changed in active branch** `feature/propulsion-authority-matrix-2026-10-16`:
- New `src/game/navigation/debug/DockPlannerFileLog.h` is a small Windows-only, best-effort C++17 file sink on the CLIENT orchestration boundary, not in pure `RoutePlanner`/`ContinuousAssistedPathOptimizer` and not on server physics/control path. It creates D:/tmp if needed, opens `D:/tmp/elite_docking_planner.log` in APPEND mode, writes timestamp/serial/status, full worker ms and final-connector solve ms, input start/goal/speed/acceleration/initial lead, route length and straight-line distance, one row per semantic BLUE/terminal stage with length and ownership, plus `RoutePlan::diagnosticSummary` including per-leg `inbound_ms`, `final_ms`, bound straight lengths, optimizer evaluation statistics. Flushes and closes after every report to survive immediate window closing; mutex serializes reports. Does not run on a simulation frame hot path; failed logging only warns to stderr once, never changes flight state.
- `SpaceState.cpp` invokes `appendDockPlannerReport()` **once at completion/adoption processing of each async route worker** (including invalid/retryable planner results), after a purposely future-dated projected connector has finished waiting, before route validity/timeline/handoff checks. Event `route-worker-ready`, `connector-worker-ready` or `route-worker-failed` denotes worker result, not necessarily accepted/autopilot-executed route. Old console diagnostics preserved; file avoids lost `std::cout` due buffer on immediate window exit.
- `tests/architecture_contracts/check_assisted_docking_entry_contract.py` enforces file sink location, flush/append mode, semantic stage/result output and orchestrator hook.

**Status:** GitHub source updated only. **No native Windows build, CTest or live file existence test run by assistant**; user must update branch and build `EliteGame` to activate. No changes to planner cost/search, geometry, BLUE constraints, controller or restoring branches. Previous live geometry comparison stationary start: ~79.72km total before versus ~49.14km after, terminal ~43.51km before versus ~12.40km after, worker still 11.54s. Single frozen restore point remains `restore/terminal-optimizer-contract-before-2026-10-19`.

**Minimal next verification (MSYS2 MinGW64):**
```bash
cd /d/__elite/work
git status -sb
git fetch origin
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
python tests/architecture_contracts/check_assisted_docking_entry_contract.py
bash build_mingw64.sh
./build/EliteGame.exe
```
In game request/show route and close after route appears. Verify `D:\\tmp\\elite_docking_planner.log` exists and includes `[DockPlannerFile]`, `[DockPlannerRequest]`, `[DockPlannerStage]`, `[DockPlannerResult]`, `inbound_ms=`, `final_ms=`. User can upload that ONE file plus route screenshot. It APPENDS across runs (delete file manually before clean A/B test if necessary). If log missing, check warning `[DockPlannerFile] unable to write D:/tmp/elite_docking_planner.log` and share build/runtime error. No game flight acceptance required for this inspection.

---

## 2026-10-19 — FIRST USER GAME GEOMETRY PREVIEW AFTER ASSISTED TERMINAL HARD-STRAIGHT FIX

**User evidence:** screenshot `image(20261019-204419).png` and full posted runtime/build log `Вставленный текст(20261019-204432).txt` (103 lines). Full EliteGame + EliteServer native MinGW builds LINK successfully; no blocking compile errors. `[DockRequest]` start, `[AssistedSafeCourse]` selects an alternate course (initial `speed_mps=0`, `current_safe=0 escape_found=1 turn_deg=15`), `[RouteStage]` and `[DockRouteStage]` show a valid five-stage topology: initial free, 700m BLUE capture, 1200m KeepInside, 150m BLUE exit, terminal dynamic. `[DockLateEntry]` OnPrefix, `forecast_on_prefix=1 rebound=0 worker_ms=11535.8`. `[DockClient] phase=executing control_path=ShipControlState`; user stopped early by closing GLFW as agreed (geometry-only preview, no completion claimed).

**Measured NEW stage progress:** Free initial 0–34695.7m = 34.6957km; capture 34695.7–35395.7 = 700m; BLUE 35395.7–36595.7=1200m; exit 36595.7–36745.7=150m; terminal 36745.7–49142.5m = **12.3968km**; total 49.1425km. Compared against PREVIOUS STATIONARY-start run from 2026-10-19, initial ~34.154km, BLUE 2.05km, terminal ~43.5115km, total ~79.7155km. Thus initial section **+0.5417km**, terminal section **-31.1147km**, total **-30.573km (~38% shorter)**. This is solid evidence removal of the artificial terminal hard-straight produced a much more compact route. The initial free leg was NOT changed by this experiment. Prior 500m/s-start baseline (~47.37km initial/36.13km terminal) is **not apples-to-apples** with this new 0m/s run; repeat same 500m/s setup separately if desired. Screenshot subjectively shows one loop shrink and one lengthen; actual stage lengths confirm relatively minor initial free-leg elongation versus dramatic terminal shrink. Nonzero safe course chosen 15deg is a PLAUSIBLE reason the initial leg differs; causality unproven without identical snapshot A/B.

**Open diagnostics:** Worker still ~11.54s: no evidence optimization speed improved. Posted run has `[RouteStage]` and `[DockRouteStage]` but lacks `[DockClient] phase=route-ready planner_diagnostics=` line, which currently emits via `std::cout`; therefore `inbound_ms`, `final_ms`, start/goal hard distances and evaluation counts are NOT observed in this log. Need ensure emitted reliably (e.g. flush/error stream) before making per-leg performance claims. Only geometric preview was requested; no new native flying test or terminal HOLD acceptance. Current first-stage offset cannot be asserted an algorithmic regression from this comparison.

**Next:** Keep physically valid BLUE/hull/manual frame constraints and existing controller unchanged. Check whether terminal 12.4km is near reasonable lower bound with per-leg velocity bounds (actual BLUE-exit speed), local obstacles and manual-frame min radius; use per-leg timing to target 11.5s search cost. If user wants next geometry iteration, do exact same snapshot A/B comparison or repeat 500m/s preview; avoid shortening first ballistic leg mechanically. Single state file remains root PROJECT_STATE.md.

---

## 2026-10-19 — FIX OBSOLETE SEGMENTED_ROUTE_COMPOSER TEST EXPECTATION (NATIVE RERUN PENDING)

**User's actual MSYS2 output after Assisted terminal-geometry experiment:** CMake configure/build succeeded, `route_planner_api` PASS (0.24s), `ordinary_route_policy` PASS (0.04s), `segmented_route_composer` FAIL (0.05s) with `SEGMENTED ROUTE COMPOSER TESTS: FAIL: terminal free stage missing`; CTest 2/3. Python `check_assisted_docking_entry_contract.py` PASS, although its final printed handoff sentence was obsolete and has now been corrected.

**Diagnosis verified in source and frozen pre-experiment restore branch:** The exact same `SegmentedRouteComposerTests.cpp` (same Git SHA `63b6d943...`) on both active and `restore/terminal-optimizer-contract-before-2026-10-19` expected last semantic `stages[4].kind == RouteStageKind::FreeTransit`. This expectation predates the established `TrafficRoutePlanAdapter::buildStageContract` code, which *intentionally* synthesizes last `RouteStageKind::TerminalApproach`, `RouteStageFramePolicy::DynamicTerminalFrame`, `RouteStageRefreshPolicy::LiveDuringStage` on free geometry after BLUE release, so docking frames may co-rotate with the dock. It does **not** erase the final free geometry; this stage describes semantic frame ownership, not whether geometry may move through free space. `request.roundTurns=false` in failing legacy test, so the new Assisted continuous optimizer terminal start/goal hard-straight opt-in could not have run in this test. This is stale test contract, **not evidence of regression in this experimental planning change**.

**Fix now committed to active branch, restore point untouched:** `tests/navigation_runtime/SegmentedRouteComposerTests.cpp` expects final semantic `TerminalApproach`; now additionally checks live dynamic frame ownership, `LiveDuringStage`, no hard BLUE containment leak, positive actual geometry length, coverage to final curve progress and final endpoint equals authored docking HOLD point. Existing BLUE KeepInside/semantic/order checks preserved. `tests/architecture_contracts/check_assisted_docking_entry_contract.py` now asserts adapter's dynamic terminal contract + test assertions and replaces stale line 'one projected entry bind based on first-turn travel time' with correct measured connector-worker latency and forecast prefix validation.

**Status:** User's original two positive C++ tests remain PASS. This source fix and strengthened test have **NOT YET BEEN NATIVELY RUN**; source contract/branch compare only. Do not state 3/3 until user's rerun proves it. No physical system, route optimizer, BLUE topology or actor controls modified during this fix; still follow user request for ONLY planner geometry preview after tests.

**Minimal user rerun (MSYS2 MinGW64):**
```bash
cd /d/__elite/work
git status -sb
git fetch origin
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake --build build/tests/navigation_runtime --target segmented_route_composer_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^segmented_route_composer$' --output-on-failure
python tests/architecture_contracts/check_assisted_docking_entry_contract.py
```
On green, full game build `bash build_mingw64.sh`, then in game just SHOW ROUTE and save log `D:\\tmp\elite_terminal_geometry_preview.log`, screenshot, no need to fly. Check `planner_diagnostics`, `final_start_hard_m=150`, `final_goal_hard_m~900`, `final_ms`, `inbound_ms`, and terminal stage length vs previous ~36.13km. If failing, send FIRST native failure; do NOT skip it.

---

## 2026-10-19 — ASSISTED BLUE-EXIT TERMINAL GEOMETRY EXPERIMENT: FIX HARD-STRAIGHT INPUTS, PLANNER PREVIEW ONLY (SOURCE COMMITTED, NATIVE PENDING)

**User authorization:** "хорошо. давай пробовать. прогон устраивать не обязательно, можно только смотреть результат работы планеровщика." Fix overly wide ~36.13km terminal loop **only in geometry**, inspect new live-planned cyan tunnel immediately after it appears; a full docking/flight acceptance is NOT required for this experiment. Preserve earlier steady flight stack.

**Immutable new recovery branch CREATED** `restore/terminal-optimizer-contract-before-2026-10-19` at `279ce5586aab2d7d177151eddc0f4ede6ab11e44`. Older previous frozen restore branch `restore/assisted-handoff-before-lead-fix-2026-10-19` remains at `c4429788de640d5487334733f60095bb117db570`. Work branch: `feature/propulsion-authority-matrix-2026-10-16`. Do NOT update either recovery branch or reset the user's worktree.

**Verified root cause before change:** Production Assisted segmented planning DOES use `ContinuousAssistedPathOptimizer` on both free legs. The optimizer inherited `protectedStartStraight = max(initialProtectedStraightMeters, initialForwardLeadMeters)`, so its ~150m exact protected BLUE release became >1km (potentially much more) hard start. More importantly, the terminal final tangent `terminalApproachLengthMeters` came from `requiredTerminalApproachMeters(maxShipSpeed,0,smallConservativeBraking,..)`, making 500m/s hypothetical braking length about 10km a mandatory straight even though optimizer computes velocity/braking across the whole free leg. The old 1000m pre-arc straight and 36-angle old `DockingAdvisoryPlanner` are NOT active for this route. The new optimizer's 13 seeds / 11 parameters / thousands of full evaluations are expensive, hence independent per-leg measurement matters.

**Experiment source changes (only Assisted final free leg):**
- `OrdinaryRoutePolicy.h`: new pure `assistedHardStartStraightMeters(lead,protected,exactPortalRelease)`. Ordinary/hot initial entry remains `max(lead,protected)`. After a semantic BLUE exit, the protected geometry is EXACT `protected` (150m), no conversion of turn/braking budget into an additional hard line. New pure `requiredDockingAxisMinimumMeters(ref,shipLength) = max(700m,3*ref,10*shipLength)`; existing speed-dependent `requiredTerminalApproachMeters` and its previous callers still exist unchanged and now delegate their geometric minimum to this helper.
- `RoutePlannerApi.h`: new per-request opt-in `assistedUseExactProtectedStart=false`, default fully backward compatible.
- `SegmentedRouteComposer.cpp`: ONLY `finalRequest` opts into new exact protected start when `assistedCompactStartLead && useContinuousAssistedOptimizer`. Mandatory 150m after BLUE exit unchanged, mandatory BLUE entry/transit/exit untouched. Assisted final goal hard straight uses only dock axis geometry minimum (~900m for reference=300m, Cobra length~30m), not braking distance computed from route max 500m/s; physical feasibility still enforced by continuous optimizer's entire-leg speed profile (signed motor-derived accel/braking/curvature/omega) and route validation. Legacy/Newtonian request behavior untouched. Inbound free leg untouched. No new speed caps or hidden radius reductions.
- `RoutePlanner.cpp::planContinuousAssisted`: reads new opt-in via `OrdinaryRoutePolicy::assistedHardStartStraightMeters`; *no changes to optimizer internals*, `minComfortRadius` from 500m frames/30deg still strict. The old initial ballistic moving ship still receives full physically justified protected lead.
- **Planner-only diagnostics:** `SegmentedRouteComposer` measures actual wall-clock `inbound_ms`, `final_ms`, `free_leg_total_ms`; records hard-start and hard-goal extents, final post-BLUE handoff speed, total length and straight-line (loose) lower bound plus each free leg's `ContinuousAssistedPathOptimizer` evals/seeds/min-radius/length/stretch/time objective. `SpaceState.cpp` prints this in `[DockClient] phase=route-ready planner_diagnostics="..."`; semantic stages already logged as `[DockRouteStage]` for individual lengths. No separate new state/log files in repo. Redirect runtime output to D:\\tmp on user's Windows machine.
- **Tests added/strengthened:** `OrdinaryRoutePolicyTests.cpp` asserts 8km original moving initial lead unchanged, BLUE-exit hard 150m, dock HOLD hard axis 900m and separate original nominal cruise-braking estimate; `RoutePlannerApiTests.cpp` adds an actual continuous optimizer scenario with manoeuvre lead 8km but 150m hard start and 900m hard goal, requiring exact terminal point and first line length. `tests/architecture_contracts/check_assisted_docking_entry_contract.py` checks source integration, Assisted-only flag, untouched inbound and diagnostics. Static cross-source content audit of 8 relevant files PASS (NOT compilation or CTest). Test scenario may expose feasibility bugs; fix on exact error rather than claiming PASS.

**Next user action: planner visual comparison ONLY.** In MSYS2 MinGW64:
```bash
cd /d/__elite/work
git status -sb
git fetch origin
git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime --target ordinary_route_policy_tests route_planner_api_tests segmented_route_composer_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^(ordinary_route_policy|route_planner_api|segmented_route_composer)$' --output-on-failure
python tests/architecture_contracts/check_assisted_docking_entry_contract.py
mkdir -p /d/tmp
set -o pipefail
bash build_mingw64.sh 2>&1 | tee /d/tmp/elite_build.log
./build/EliteGame.exe 2>&1 | tee /d/tmp/elite_terminal_geometry_preview.log
```
On 500m/s reference scene select dock/route, WAIT FOR `phase=route-ready`, visualize cyan tunnel and screenshot without completing the flight; compare old BLUE->dock ~36.13km to new last semantic `[DockRouteStage]` span. Include `planner_diagnostics` to see real final start 150m, final goal ~900m, `inbound_ms` vs `final_ms`, `free_leg_total_ms`, optimized length. A new short red-like trajectory is NOT automatically 3D safe; all hard collision and KeepInside validators still required. If optimize fails, report first compiler/CTest failure or `[DockPlanFailure] diagnostics` rather than loosening safety. If code compiles and route is shorter, subsequent eventual flight test still required to declare actual control PASS, but user explicitly deferred that.

**Rollback:** `git switch restore/terminal-optimizer-contract-before-2026-10-19` after safeguarding local changes; do not hard-reset user's workspace. Root `PROJECT_STATE.md` remains the ONE persistent project context file.

---

## 2026-10-19 — CORRECTION: CONTINUOUS ASSISTED OPTIMIZER *IS ACTIVE* FOR BOTH FREE LEGS; ROOT CAUSE PROTECTED-STRAIGHT CONTRACT AND SEARCH COST

**User asks why the 1000m straight, why the route worker needs ~12.8s if only 36 candidates, and whether the earlier optimized planner regressed.** Full current GitHub call-graph + relevant commits inspected. **Important correction to immediately previous section of this document and prior assistant statements:** those concluded the live BLUE route uses the LEGACY 36x9 terminal fixed-circle `DockingAdvisoryPlanner`. This is WRONG for the ACTIVE Assisted segmented route. The older planner DOES contain `preArcStraightMeters=max(1000,angularRampDistance,2*hullRadius)` and 36 angles over 9 axis offsets, but those routines are BYPASSED for both live free legs described below. Do NOT base a patch or performance estimate for this live scene on the legacy 36 candidates.

**Verified exact active call chain:** `SpaceState.cpp` calls `AssistedPlanningPipeline::segmented`, which explicitly sets `request.useContinuousAssistedOptimizer=true` and invokes `SegmentedRouteComposer::plan`. Composer isolates the hard BLUE volume stage, then clears `legRequest.requiredViaPointsMeters` + `mandatoryTangentStraights` for inbound and likewise for `finalRequest`; retains the true optimizer flag from original `terminalRequest`. Both `RoutePlanner::plan(legRequest)` and `RoutePlanner::plan(finalRequest)` therefore satisfy the `useContinuousAssistedOptimizer && roundTurns && empty vias/mandatory` gate and invoke `planContinuousAssisted` -> `ContinuousAssistedPathOptimizer::optimize`. The hard BLUE transit stays unchanged between the two. History: optimizer source actively developed Oct 11-12; Oct 14 `2ceaa900c` isolation explicitly routes Assisted segmented planning to that backend. **Not switched off.**

**REAL integration mistake: geometric planning budget converted to HARD protected straight.** `SegmentedRouteComposer` sets `finalRequest.initialProtectedStraightMeters=150` specifically to release the hull from the BLUE passage and allow early curvature. But `RoutePlanner.cpp::planContinuousAssisted` computes `request.protectedStartStraightMeters = max(input.initialProtectedStraightMeters, input.initialForwardLeadMeters)`, so the potentially 8-12+ km `initialForwardLeadMeters` becomes a non-negotiable initial straight in the optimizer, OVERRIDING the 150m override. The same happens for initial inbound leg and makes the ship fly a long straight after plan appears. `requiredAssistedStartLeadMeters` was recently reduced from the legacy double budget but STILL uses a 1000m baseline for this independent source; the legacy `preArcStraightMeters=1000m` is irrelevant to live Assisted segment. A fix must distinguish operational ballistic prefix from available braking/turn-search region and enforce hard straight only where truly necessary.

**Second HARD bound:** `RoutePlanner::planContinuousAssisted` sets `request.protectedGoalStraightMeters = input.terminalApproachLengthMeters`; `SegmentedRouteComposer` inherits `terminalApproachLengthMeters` computed by `OrdinaryRoutePolicy::requiredTerminalApproachMeters(maxRouteSpeed, maxDockEntrySpeed, plannerBraking, ...)`, from route MAX rather than actual release-of-BLUE speed and with 1.25x braking margin. With maxSpeed 500m/s and conservative 15m/s² braking, this gives ~10.4 km compulsory last straight; more if braking lower. The optimizer cannot remove either long protected end. User's RED shorter tangent path is not within the input feasible family until these artificial hard portions are relaxed/represented correctly; red is NOT 3D collision-certified.

**Actual cost:** Not 36 trials. `RoutePlanner.cpp` configures `samplesPerBezier=96, maxPatternIterations=160`. Optimizer creates 13 initial seeds; ranks 4 of them; optimizes each for 40 iterations and performs another 80 refinement iterations. Each iteration tries +/- perturbations of **11 parameters**, i.e. up to `13+4*40*22+80*22 ~= 5293` calls to `evaluate` PER free leg (may terminate earlier). TWO such legs -> order 10k full evaluations. Each evaluation samples ~192 cubic positions (plus straights), performs many per-segment collision checks against world obstacles, curvature and lateral/angular/braking speed passes, and cubic turn-shape analysis. The 12,810.9ms logged `worker_ms` covers BOTH route optimizations AND validation, stage composition, trajectory/autopilot compile; current log has **no per-leg wall-clock timings**, so cannot prove exact bottleneck fraction or whether 12.8s was entirely the path search. Older `[SegmentedLeg]` diagnostic logging was removed by Oct 12 `b63e80d8f` to keep segmented composer geometry-only; no present per-leg benchmark.

**Conceptual verdict:** The new optimizer is ACTIVE and richer than old fixed-quarter-circle planner (two cubic transitions + optional straight; 13 seeded patterns, time/reserve/length objective); the output is overly large because adapter converts manoeuvre planning lead to hard straight and max-speed terminal braking margin to another hard straight. This is a **planner-to-optimizer contract/design regression**, not evidence of optimizer deactivation. The search itself is expensive due thousands of repeated numerical/collision evaluations, not tiny 36-trial enumeration.

**Safe engineering steps pending approval:** (1) Add per-leg timing+evaluation count, route-boundary inputs/actual start-goal protected lengths and trajectory compile time (logs ONLY D:\\tmp). (2) For ordinary first route, require just physical balloon/real safe ballistic continuation before first curve, keep dynamic braking room as SOFT search budget; avoid removing initial straight where 500m/s early braking is proven necessary (last 500m/s run had turn_need>turn_distance at handoff). (3) Define final-axis hard length from actual viable BLUE exit speed, terminal HOLD and ship clearance, with advance braking planned anywhere before final; separate preferred axis length from mandatory docking approach. (4) Re-run same snapshot with old/new A/B physically feasible, collision clearance and >=3 frames per 90deg at 500m gates, check no Assisted/Newtonian cross leak. (5) Investigate bounded optimization improvements: fast direct no-obstacle candidates, coarse screening vs full sampled evaluation, reuse collision cache/near obstacles, prune seeds and per-leg budget; retain optimization of velocity + comfort instead of reintroducing arbitrary hard 90deg circle. Do not change production algorithms while responding to this diagnostic question; only this sole root PROJECT_STATE.md updated.

---

## 2026-10-19 — LIVE 500 M/S TERMINAL LOOP FORENSICS; ROUTE SEARCH IS CONSTRAINED-FIRST-FEASIBLE, NOT SHORTEST (ANALYSIS ONLY)

**User data:** screenshot `image(20261019-185825).png` with proposed shorter RED arc vs actual CYAN, 99-second video `20261019-1853-55.6206548.mp4`, live console `Вставленный текст(20261019-190007).txt` (457 lines). Video confirms wide terminal loop around hub; image is 2D and cannot establish whether the red alternative is 3D collision-free or respects BLUE dock clearance. This source analysis, NOT user acceptance of route changes; NO production code modified.

**Live current branch CONFIRMED BUILD+EXECUTION by user:** both EliteGame and EliteServer MinGW build finish; `[AssistedSafeCourse]` 500m/s initially safe; `[DockLateEntry] disposition=2 OnPrefix along=6479.75m prefix=11613.4m remaining=5133.68m forecast_on_prefix=1 rebound=0 worker_ms=12810.9`; `[DockClient] phase=executing control_path=ShipControlState`. Route total 85,542.4m, stages: free 47,366.6m; BLUE entry 700m; BLUE lane 1200m; BLUE exit 150m; terminal leg **36,125.8m**. 271 `[DockClientTrack]` rows: peak cross track=49.0969m during late handoff, later low; max course error=2.085deg, peak curved required a=20.89m/s² versus reported installed lateral 73.5499m/s² (planner uses conservative fraction). End speed ~1.15e-5m/s, remaining ~3.61m, cross-track ~6.42m. This is consistent with user-requested pre-dock STOP (not yet proof of rotating-port relative HOLD).

**Exactly why CYAN loop is wide (verified in current GitHub source):**
1. `src/game/navigation/DockingAdvisoryPlanner.cpp` chooses design turn speed `max(initialSpeed,0.8*maxSpeed)` for terminal even when actual post-BLUE arrival will be slower. With max=500, v_design=400m/s; Assisted conservatively budgets ~one-third real force, i.e. <=24.52m/s² even before other reserves. Mandatory geometry radius is `max(150,30*hullRadius,designV²/plannerLateral,designV/maxAngularVelocity)`, thus **at least order 6.5km** for terminal turn. One compulsory 90° arc itself has length >=10km. During execution the craft CAN/DO slow on arcs; maximum initial cruise geometry is an arbitrary optimization assumption, not a collision/visibility law.
2. `DockingAdvisoryPlanner.cpp` exact terminal primitive is `route to preEntry -> mandatory pre-arc straight >=1000m -> exact 90° circle fixed R -> ALIGN -> final docking-axis straight -> stop`. It CANNOT consider a smooth alternative composed of changing-curvature curves or another turn angle/shorter feasible radius. It separately forces docking-axis final straight >=max(700m,3*standoff). User's proposed red curve is not represented in this candidate family.
3. `OrdinaryRoutePolicy::requiredTerminalApproachMeters()` estimates final approach as `max(geometricMinimum,1.25*(v_max²-v_entry²)/(2*a_brake))`; `SpaceState.cpp` calls it with route `maxSpeedMps`, not actual semantic exit-of-BLUE arrival speed. The same request maxSpeed is carried into the segmented final free leg. With v_max=500 and conservative planner braking ~one-third of nominal, this can reserve several-to->10km of unnecessary final axis before arc.
4. `DockingAdvisoryPlanner.cpp` samples 36 orientations about docking axis, and `OrdinaryRoutePolicy::preferCandidate` chooses shortest among those of the FIRST feasible axis offset; then `if(selected.valid) break;` prevents searching other axis-offset factors `0,-0.5,+0.5,-1,+1,-2,+2,-4,+4` times terminal R. Thus global feasible minimum is NOT searched even within its fixed-radius primitive family; an inward station may yield much shorter route but is never tested.
5. `SegmentedRouteComposer` splits at hard BLUE volume transit, planning independent ingress/terminal legs with fixed portal geometry and shared speed ceiling, without global alternate topology optimization. Active `RoutePlanner::plan` uses legacy DockingAdvisoryPlanner backend (flag `useContinuousAssistedOptimizer` defaults false); the existing experimental continuous Assisted optimizer is not used by this production traffic route. Keeping hard BLUE zone and manual gate spacing (500m, 250m near finish, >=3 frames per quarter-turn) is correct.

**Shortening the INITIAL straight carries distinct risk.** At route adoption live 500m/s and `[DockClientTrack]` first turn_distance=7285.67m while turn_need=8184.64m, a ~899m shortfall in follower's advance-brake diagnostic. This later recovered and flight remained stable, but warns the previous compact lead change cannot be reduced blindly; reconciled planner/follower speed-envelope and latency is required.

**Architectural evaluation:** Separation `topology (hard mandatory BLUE portals) -> geometric search -> physically achievable speed profile -> virtual-pilot execution` is correct. Current terminal search family and candidate ranking are NOT sufficient for efficient routes: hard-coded design speed, rigid circular 90° turn, long nominal-vmax docking axis, first-feasible-axis exit, and failure to compare alternative radius/approach choices. 2D red sketch NOT certified collision-free. DO NOT lower safety/BLUE constraints, dock-relative HOLD, minimum visually readable frame count, or physical motor envelope to make the picture shorter.

**Proposed next design, subject to user approval and measurable tests:** (i) choose candidate terminal arrival/turn speeds with predicted post-BLUE arrival and braking distance, (ii) variable legal turn radii bounded below by `OrdinaryRoutePolicy::requiredGuidanceTunnelRadiusMeters(500m,...)`, angular/simultaneous motor authority and collision clearance, (iii) alternate safe G1/G2 fillets and appropriate short docking-axis final segment, (iv) bounded multi-axis offset/radius/rotation candidate exploration, coarse-to-fine/pruned (not exploding worker), score all accepted by total length + feasible time with hard constraints, (v) record candidate radius, stage lengths, reject reasons and best-vs-current length in `D:\\tmp`, (vi) replay same BLUE/rotating-dock scene comparing candidate actual world samples and follower track; never deem user red shape feasible from projection alone. Preserve original restore branch `restore/assisted-handoff-before-lead-fix-2026-10-19` at `c4429788de640d5487334733f60095bb117db570`, keep only this PROJECT_STATE.md file as permanent context.

---

## 2026-10-19 — RESTORE POINT + ASSISTED BALLISTIC HANDOFF / INITIAL-LEAD EXPERIMENT (SOURCE COMMITTED; NATIVE PENDING)

**Permanent GitHub restoration branch** `restore/assisted-handoff-before-lead-fix-2026-10-19` pinned at pre-experiment commit `c4429788de640d5487334733f60095bb117db570`. Leave immutable, DO NOT move/reset user's worktree automatically; active development `feature/propulsion-authority-matrix-2026-10-16`.

**Preserved user contract:** Current dock behavior is STOP BEFORE port entrance then maintain position and relative orientation as dock rotates, NOT enter port. Manually visible tunnel needs at least three frames per 90-degree turn (500m normal, 250m near finish); large physically feasible arcs are intentional. Preserve mandatory BLUE KeepInside transit, real motor allocation, Assisted/Newtonian isolation, server-agnostic virtual pilot input. Do not globally optimize distance if it makes curves too sharp for manual control.

**Source changes implemented (not C++ runtime tested):**
- `OrdinaryRoutePolicy.h::requiredAssistedStartLeadMeters`: for Assisted only, one physical braking distance + one comfortable turn radius, rather than generic `2*(braking distance+turn radius)`. Min 1km preserved, geometric turn radius / tunnel frame cadence unchanged. Existing `requiredStartLeadMeters` remains UNCHANGED for Newtonian/legacy.
- `RoutePlannerApi.h::assistedCompactStartLead` explicit opt-in default false; `SpaceState.cpp` sets only in Assisted. `SegmentedRouteComposer.cpp` propagates this differentiated start-lead across BOTH initial and post-BLUE free flight legs, preserving hard topology.
- NEW `planner/BallisticEntryHandoffPolicy.h`: accept original route without rebind when live AND predicted ballistic positions both remain on its first authored straight. Prediction = livePos + liveVel * expected FINAL connector computation latency, NOT time to first curve. When predicted craft would leave that straight, trigger bounded connector rebinding early.
- `SpaceState.cpp/.h`: record `DockAdviceJob::entrySolveDurationMs` for actual bind solver, and remember measured full late-connector worker latency INCLUDING autopilot compile as `m_dockObservedConnectorWorkerSeconds` (distinct from expensive first-route worker). Forecast uses 0.75s bootstrap before measurement, afterwards max(0.75s, 1.25*last connector-worker seconds). No more `leadSeconds=max(firstTurnTravelSeconds, fullWorkerSeconds)`; flight time to first turn remains only for diagnostics. Connector retry budget remains 4; safe course, real control law and physics unchanged. `[DockLateEntry]` now logs `first_turn_remaining_m`, `start_lead_policy_m`, `forecast_distance_m`, `connector_solve_ms`, `connector_forecast_s`, `forecast_on_prefix`, `worker_ms` and `rebound`.
- Added regression cases in `OrdinaryRoutePolicyTests.cpp` (v500 nominal Assisted compact lead, Newtonian untouched, stopped minimum; pre-existing HUD visibility test) and `RouteEntryBinderTests.cpp` (actual v500 second-run 6129.79m along 23226.9m prefix should reuse, near-end/v500 predicted PastPrefix must rebind, lateral departure, bootstrap/observed latency). Updated Python `check_assisted_docking_entry_contract.py` to enforce distinct forecasts, Assisted-only start lead and original safety invariants.
- A read-only Github source consistency scan of 10 files was PASS. **No native MinGW64 C++ compilation, CTest, Python contract run or game flight were performed for these changes.** All remain pending user execution; previous flight stability is pinned at restore branch.

**Next MSYS2 MinGW64 commands**, save uncommitted edits before switch:
```bash
cd /d/__elite/work
git status -sb
git fetch origin
git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime --target ordinary_route_policy_tests route_entry_binder_tests assisted_authority_contract_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^(ordinary_route_policy|route_entry_binder|assisted_authority_contract)$' --output-on-failure
python tests/architecture_contracts/check_assisted_docking_entry_contract.py
mkdir -p /d/tmp
set -o pipefail
bash build_mingw64.sh 2>&1 | tee /d/tmp/elite_build.log
./build/EliteGame.exe
```
Stop on first error. In 0m/s and 500m/s safe-ballistic docking compare `[DockEntryPolicy] forward_lead_m`, `[DockLateEntry] first_turn_remaining_m/forecast_on_prefix/connector_forecast_s/rebound`, `[DockClientTrack] cross_track/course_err`. Count visible frames through turns and test dock-relative HOLD. If needed, roll back by `git switch restore/assisted-handoff-before-lead-fix-2026-10-19` after saving local changes; do not reset shared working branch. Logs only D:\\tmp; ONE permanent state document is this root PROJECT_STATE.md.

---

## 2026-10-19 — USER REASSERTS DOCK HOLD, VISIBLE TUNNEL TURN SPACING; SECOND 500 M/S BALISTIC HANDOFF AUDIT

**Canonical requirements (user clarification):** At present automatic docking is intended to STOP just before the port entrance and then HOLD position/orientation **relative to a rotating docking port**; actual ingress/occupancy is future work. Thus a stationary craft ~3.55 m from the route end is NOT, by itself, a docking failure. Keep frame cadence 500 m ordinary, 250 m near finish, and minimum visually readable number of frames through *every* turn (current OrdinaryRoutePolicy::requiredGuidanceTunnelRadiusMeters enforces 30° per characteristic 500 m interval). Physically flyable and hand-flyable arcs outrank minimum geometric distance. Do not blindly minimize terminal arc/radius or erase BLUE mandatory transit.

**Second 500m/s run, attached log `Вставленный текст (2)(4).txt`:** request speed=500 m/s, `[AssistedSafeCourse] current_safe=1`, worker=12115 ms, `[DockLateEntry] disposition=2 OnPrefix, along=6129.79 m, prefix=23226.9 m, prefix_remaining=17097.1 m, cross=43.8855 m, rebound=0`. Thus the craft traveled ~6.13 km ballistically in the ~12.1 s worker; the original first straight was **still valid**, so code ADOPTED it with NO projected connector recompute. First `[DockClientTrack]` reports `turn_distance_m=20762.5`, `turn_need_m=8227.53`, ~12.54 km slack to first turn beyond estimated required braking/turn margin. After it settles near 400m/s, turn_distance~16482.4 m vs turn_need~5008.0 m (~11.47 km slack), and braking becomes active near turn_distance~4963 m. The long post-tunnel straight is REAL authored geometry, not merely the ~12 s spent waiting for compilation. The accepted route need not actually hit its first turn immediately on worker completion.

**Source audit:** `SpaceState.cpp` uses `RouteEntryBinder::inspectBallisticPrefix(...)`; when `OnPrefix` it immediately adopts the old route, without invoking `RouteEntryBinder::bind`. `m_dockLateEntry` projection/rebind runs only when prefix is OffAxis/BeforeStart/PastPrefix. In that exceptional branch `leadSeconds=max(firstTurnTravelSeconds,wholeWorkerSeconds)`; initial connector project is `start + velocity * leadSeconds`. If future connector finishes early the system can also intentionally coast until that predicted time. There is no independent final/entry-leg compute-time observation; `job->workerDurationMs` measures total plan/compile worker duration. The user's requested canonical scheme is more precise: heavy downstream route while ballistic, then estimate *last connector* duration, project the current ballistic state for that predicted duration, recompute ONLY last connector, and adopt on reachability/geometry checks. Normal OnPrefix adoption is a valid efficient fast path (mathematically same line and live progress), but it should explicitly report whether projection/trim is equivalent and not confuse the user. Never force a costly rebind if the live and projected point both remain on the same valid protected tangent straight.

**Potential real source of long lead:** `OrdinaryRoutePolicy::requiredStartLeadMeters` computes design speed=min(v,0.8*vmax), design radius=max(vdesign²/lateral, vdesign/omega), lead=max(1000m, 2*(brakingDistance+designRadius)). New shared motor budget divides Assisted lateral/braking planning authority by 3, increasing geometric lead. For v=500m/s and ~400m/s design speed with lateral~73.55/3=24.52 m/s², design radius alone ~6.53 km; doubled design-radius contribution alone ~13.05 km. This explains why first turn may remain tens of km away despite 12s compilation. Verify whether current lead has double-counted braking and frame-comfort margins before reducing anything; calculate exact `[DockEntryPolicy]` requested lead and first curve progress in next test. This is about PROPORTIONAL physical maneuver radius, not a reason to weaken hull/KeepInside/visible-frame safety.

**Next targeted work pending user direction:** add explicit stage timing (expensive backbone vs final connector), planned straight lead, elapsed worker distance, first-turn distance, physical braking/curvature threshold, slack and entry bind disposition to `D:\\tmp` diagnostic log; instrument relative dock hold angular velocity/position drift separately. Add native 500m/s ballistic moving-worker test for OnPrefix fast-path and missed-prefix connector path, demanding finite retries and correct s-progress. If shrinking lead, alter ONLY over-conservative policy where tested, preserve visual min frames and real simultaneous motor budget. No production source modifications performed during this audit; only root PROJECT_STATE.md updated.

---

## 2026-10-19 — LIVE ASSISTED FLIGHT STABLE; OVERWIDE DOCK ROUTES AND FINAL APPROACH STALL (ANALYSIS ONLY)

**New user evidence:** capture `20261019-1448-18.2982908.mp4` (259.6 s), route overview image `image(20261019-145844).png`, startup/stationary full log `Вставленный текст(20261019-145335).txt` (906 lines; 254 `DockClientTrack` records), 500 m/s supplementary log `Вставленный текст (2)(4).txt` (114 lines; 58 `DockClientTrack`). User says flight itself is normal but planner makes very wide excursions.

**Proved good in new runtime:** Full client/server MinGW production build in user's log; `DockLateEntry disposition=2`, `DockClient phase=executing control_path=ShipControlState`. No `installed Assisted motor authority unavailable` failure. Former severe somersaults no longer reproduced in the complete flight: stationary-start run max `cross_track_m=8.4059`, max after the initial alignment `course_err_deg` remained low, max curved `required_curve_a_mps2=20.625` versus signed installed turn authority 73.5499 m/s², max ship speed 411.98 m/s. The 500m/s initial-speed probe stays within ~43.89 m cross-track at live mid-route adoption and reduces to ~7.38 m; course error max 1.89°. No route completion was evidenced in 500m/s probe: last progress still has 93,241.6 m remaining. Do NOT equate stable flight with completed dock.

**Geometry issue visible in trace:** Stationary-start full route 79,715.5 m: free transit 34,154 m, volume-entry capture 700 m, hard BLUE transit 1,200 m, exit 150 m, terminal approach **43,511.5 m**. High-speed-start route 122,126 m: free transit 76,564.5 m, 700 m capture, 1,200 m BLUE, 150 m exit, terminal **same 43,511.5 m**. The fixed enormous terminal component is distinct from varying startup lead and strongly indicates terminal geometry policy, not a control-law problem.

**Source-backed explanation/optimization targets, NO changes yet:**
1. `DockingAdvisoryPlanner.cpp` derives geometric preferred terminal radius using design turn speed `max(initialSpeed,0.8*maxSpeed)` even when actual speed profile subsequently slows substantially. Given 400 m/s and the new effective lateral budget 73.5499/3≈24.52 m/s², conservative geometric design radius is ~6.53 km, a very wide compulsory 90° arc. Compare actual chosen `terminalTurnRadiusMeters`, arc, final-axis length and path-length ratio before modifying safety. Radius must remain comfortable and physically feasible, but may be minimized by choosing a correspondingly lower feasible turn speed (with braking before the arc); don't demand 400m/s through every terminal turn.
2. Preferred terminal-arc search tries `axisOffsetFactors` in fixed order and exits outer loop at first successful axis via `if(selected.valid) break;`. It compares valid 36 rotations inside that first axis, but does not rank path lengths across **all** feasible axis offsets. `OrdinaryRoutePolicy::preferCandidate` already exists and prefers shorter safe routes; consider using it across bounded axis candidates, with explicit hard clearances and mandatory topology unchanged.
3. `SpaceState.cpp` explicitly requires BLUE `cylinder_b.blue_lane` KeepInside, and `SegmentedRouteComposer` preserves hard volume entry/exit tangent straights. Don't shorten routes by deleting BLUE or altering safety without a gameplay/traffic-contract decision. Optimize free and terminal stages within this topology.
4. Log each route's direct-distance lower bound, realized total length, per-stage arc/line length, preferred/actual turn radius, alternative legal candidate distance/time, blocker, rejected reasons. Create regression that compares an authored reference overlarge safe route against an available shorter safe route; check I/Y/X/350-500mps + obstacle margins, keep `ShipCore` and flight-law controller untouched.

**Separate unfinished docking acceptance:** Last ~dozens of records in the otherwise successful stationary-start run repeat `page=898`, `remaining_m≈3.54684`, `cross_track_m≈6.46088` vs `center_deadband_m=6.4`, `actual_speed_mps≈1.13e-5`, `target_speed_mps≈10.527`, `target_accel_mps2≈-45.0125`, and `terminal_hold=0`. Log ends with GLFW close; no `DockClient complete`/successful docking. A final-meter stall/hold gate MAY be present. Need reproduce at finish, inspect terminal reference/stop logic, no premature `PASS docking`. Do not artificially widen endpoint tolerance to hide missed alignment.

**Decision at this point:** Do NOT rewrite the whole autopilot or physical control; targeted planner geometry/selection optimization and distinct final-stage completion bugfix are appropriate only after testing and proving constraints. No source changed during this analysis, only this sole state file updated. Plan accepted as analysis, not an instruction to modify all geometry immediately. All actual runtime logs to `D:\\tmp`.

---

## 2026-10-19 — DECISION: PLAYER AUTOPILOT IS ONLY A VIRTUAL PILOT; SERVER PHYSICS IS SOURCE-AGNOSTIC

**User reiteration:** The ship simulation/server does NOT need to know whether the player has engaged autopilot. At most it needs the ordinary control commands the pilot would otherwise provide; the server should not receive a privileged autopilot/route execution mode or bypass physical actuators.

**Verified existing code:** `SpaceState.cpp::submitAutopilotControl` calls exactly `GameClient::submitInput(ShipControlState)` used by `m_inputMapper` for humans; `GameClient.cpp::submitInput` retains the normalized pilot controls and fixed-tick discrete inputs; `GameSimulation.cpp::applyControl(id, control)` simply calls `ship->setControlState(control)`. `ShipControlState.h` explicitly forbids production player autopilot from using legacy direct world-acceleration/navigation target fields, and `ShipControlExecutionGateway` normally allocates real motor force/torque for ordinary inputs. Therefore the correct player-control boundary ALREADY exists. Maintain it, test it, and avoid designing a server-visible autopilot flag or privileged thrust call.

**Clarification:** Replicating `ShipCore::controlAuthorityTelemetry()` from server to client is a READ-ONLY physics-state stream. It does NOT tell the server autopilot is active, nor authorize the client to apply forces. Server remains authoritative for damage, mass, motor availability, actuator allocation, physical movement and input validation. Client is responsible for planning player routes and choosing the same `ShipControlState` as a human would. Inputs are normalized commands/samples and one-shot events, not literal keyboard keycodes; keyboard mapping is local.

**NPC exception:** `GameSimulation.cpp` also hosts server-owned NPC autonomous pilot controllers, legitimately, because NPCs need authoritatively generated inputs. They still call `ship.setControlState(...)` and must not bypass motor physics. This NPC decision-making component does not mean the server must distinguish a HUMAN player from client player-autopilot to execute player inputs.

**Engineering consequence for current authority wire:** Maintain `server ShipCore physical telemetry -> client planning/virtual pilot -> ordinary ShipControlState -> server physical executor`. The new telemetry replication on schema v18 supports the client's route feasibility calculation; it must stay agnostic to who produces pilot input. Architecture/integration checks should cover equivalence of human and virtual-pilot input execution and prevent legacy direct-acceleration paths, WITHOUT an `autopilotEnabled` server field.

**Status:** Design/architecture reaffirmed based on verified source. No changes to control source code in this step. The new direct-server telemetry wire fixes are still awaiting native tests and real-game acceptance; do not claim PASS. Only root `PROJECT_STATE.md` updated.

---

## 2026-10-19 — LIVE DOCK REGRESSION: AUTHORITATIVE MOTOR TELEMETRY MISSING IN CLIENT; DIRECT WIRE FIX (NATIVE TESTS PENDING)

**Live evidence:** user screenshot `image(20261019-141500).png` and console `Вставленный текст(20261019-141540).txt` (2664 lines). Game starts, player initiates `DockRequest serial=1 mode=automatic`, but `DockClient phase=failed reason=installed Assisted motor authority unavailable` floods **2572** consecutive times. No navigation planning occurs. Previously the standalone navigation CTests `assisted_authority_contract` and `route_entry_binder` were 2/2 PASS, but they missed this server-to-client availability seam. This is a REAL production-game regression despite green unit tests.

**Fault:** new `ReplicatedAssistedControlAuthority.h` client adapter demanded per-installed-engine `ObjectModuleSnapshot` + `hasEngineThermalState` to reconstruct the authoritative six-axis motor basis, but the normal ship graph is intentionally optional/sparse and sometimes entirely suppressed. `GameSimulation.cpp` publishes heavy module rows only under detailed-graph policy; certain probes skip them. Missing an optional graph does NOT prove engine failure. Repeated `fail()` every frame further produced log flood. Do not 'fix' by assuming nominal descriptor throttle or synthesizing a healthy real ship!

**New proper authority wire:** `ShipSnapshot.h` and `ClientWorldState.h` carry `ShipControlAuthorityTelemetry` read directly from server `ShipCore::controlAuthorityTelemetry()`. `GameSimulation.cpp` publishes this compact snapshot independently of module graphs; `ClientWorldState.cpp` hydrates it on both new and existing ships; `WireDataSchema.h` serializes valid, model, actuator revision, signed six-axis +/- values in ShipSnapshot; `WireDataCodec.h` bumped `SimulationSnapshotWireSchemaVersion` 17 -> **18**. `SpaceState.cpp` consumes `player.controlAuthorityTelemetry`, NOT reconstructed client module tables. During initial no-telemetry state it waits/logs one `phase=awaiting-server-capability` until server delivers data (`phase=capability-ready`); loss during execution fails safe. Persistent identical fail reports are emitted once instead of thousands of times. `SpaceState.h` removes client-side motor-program cache; old standalone replicated helper remains only as a tested diagnostic adapter and is not on the live autopilot path.

**Server source-of-truth after same-tick damage/thermal:** Engine systems may advance **after** the motor control tick, invalidating a freshly prepared actuator revision before snapshot publication. `ShipCore::controlAuthorityTelemetry() const` now reconstructs `MountedEngine` from the **authoritative runtime engine damage/operating state**, matches/refreshes the **same** cached `AssistedManeuverPrograms` object used by physical execution (mutable cache, never commands motors), sets current actuator revision, and returns signed actual basis. Explicit synthetic I/Y/X diagnostic layouts reuse the same diagnostic engine passports as the runtime; real installed engines never get a synthetic substitute.

**New integration-oriented source tests:** `tests/architecture_contracts/WireDataPlaneContractTests.cpp` now exercises schema 18 round-trip of signed motor authority and its persistence when `ShipSnapshot.graph = {}` (NO module graph); target `wire_data_plane_contract_tests`, CTest `wire_data_plane_contracts`. `tests/navigation_runtime/AssistedAuthorityContractTests.cpp` additionally constructs actual 4x Cobretty CR-1 healthy Cobra installed motors and solves real signed basis (X/Y/Z/pitch/yaw) and verifies aft-motor loss reduces real forward authority. `tests/architecture_contracts/check_assisted_docking_entry_contract.py` now protects direct server-to-client authority wiring, schema bump, wait handling, absence of old live client reconstruct path, and existing docking contracts. Read-only GitHub source-level audit verified these 13 source files are internally consistent but **none of these new modifications has yet been native-built, CTest-run or live-tested**. Do not mark PASS.

**User next actions (MSYS2 MinGW64, active branch, protect dirty files):**

```bash
cd /d/__elite/work
git status -sb
git fetch origin
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime --target assisted_authority_contract_tests route_entry_binder_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^(assisted_authority_contract|route_entry_binder)$' --output-on-failure
cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja
cmake --build build/tests/architecture_contracts --target wire_data_plane_contract_tests -j 8
ctest --test-dir build/tests/architecture_contracts -R '^wire_data_plane_contracts$' --output-on-failure
python tests/architecture_contracts/check_assisted_docking_entry_contract.py
mkdir -p /d/tmp
set -o pipefail
bash build_mingw64.sh 2>&1 | tee /d/tmp/elite_build.log
./build/EliteGame.exe
```

**Expected live behavior:** server startup; client receives valid real per-engine six-axis signed authority; `DockClient phase=planning` and subsequently `phase=executing` with corridor visible. No `installed Assisted motor authority unavailable` flood. If `phase=awaiting-server-capability` never transitions, diagnose server capability invalid or network; if `installed Assisted motors cannot execute a 3D docking path`, capture actual per-axis server values and repair physics/planner rather than resurrect descriptor ideal authority. Once route executes, original 350–400 m/s turn/corridor instability still needs physical game acceptance. Keep all logs in D:\\tmp; single project state file ONLY this root PROJECT_STATE.md.

---

## 2026-10-19 — NATIVE ASSISTED AUTHORITY TESTS GREEN; ARCHITECTURE GATE STALE LITERAL FIXED

**User executed in MSYS2 MinGW64** the navigation runtime CMake configure/build and two CTests:
- `assisted_authority_contract`: PASS 0.05s.
- `route_entry_binder`: PASS 0.01s.
- Total 2/2, 0 failures. This CONFIRMS this part of the source builds/runs under user's MinGW64 toolchain, not the full EliteGame.

**Architecture gate failure, not runtime failure:** `python tests/architecture_contracts/check_assisted_docking_entry_contract.py` failed on an obsolete literal string `projected Assisted entry bind missed its one-shot handoff`. Production SpaceState now implements finite, bounded connector recovery with `MaxEntryConnectorMisses=4`, `phase=entry-rebind-retry`, `phase=entry-rebind-exhausted`, explicit `fail(...)` and `return`; the retired one-shot diagnostic text is no longer present.

**Fix published** in `tests/architecture_contracts/check_assisted_docking_entry_contract.py`: remove the obsolete literal assertion, demand `phase=entry-rebind-retry` plus verify ordering of increment -> bound check -> exhausted log -> fail and return -> retry log. Preserves protection against unlimited retries. Read-only cross-check against live GitHub sources confirms all test tokens and ordering now present; user's **actual Python rerun is pending**, so do NOT mark architecture gate PASS yet.

**User next commands** (on correct feature branch, after ensuring no uncommitted work): `git fetch origin && git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16`; then `python tests/architecture_contracts/check_assisted_docking_entry_contract.py`. On PASS continue `bash build_mingw64.sh` and `./build/EliteGame.exe`; capture runtime logs to `D:\\tmp`. Full production build and live flight acceptance still PENDING.

---

## 2026-10-19 — ASSISTED AUTOPILOT NOW READS REPLICATED INSTALLED-MOTOR MANEUVER BASIS (SOURCE IMPLEMENTED; TESTS NOT EXECUTED)

**Reason:** Live docking after the numeric route-entry fix flew but performed severe somersaults when turn curvature required a_perp = v^2*kappa above installed actuator authority. Old SpaceState used `buildShipCapability(descriptor, snapshotSlots)` + nominal `ShipParams` without reading actual six-axis `AssistedManeuverPrograms` motor allocations, so damage/shock/heat and simultaneous turn+brake+attitude force could be overpromised. User reiterated that ONLY real ship acceleration may be trusted by autopilot, especially under damage.

**New read-only client-to-motor capability seam:** `src/game/ship/telemetry/ReplicatedAssistedControlAuthority.h` reads replicated installed module damage, emitter and hot-engine/shock state from `ClientShipState.modules`, matches descriptor's ACTUAL installed `EngineUnitDescriptor`, reconstructs `MountedEngine` list and invokes the same `AssistedManeuverPrograms::matches/build` solver used by physical `ShipCore`. Cached per-ship on `SpaceState` to avoid optimizing on each tick. Ctrl+F9 synthetic I/Y/X layout uses the same explicitly synthetic healthy engine passport as ShipCore; REAL installed ships fail closed if any engine state absent. No new fake main/RCS actuator. This is a deterministic client projection from replicated state: it is NOT yet the server's native six-axis authority snapshot and can diverge if current server mass/inertia or replication freshness differs. **Open architectural improvement:** explicitly replicate read-only `ShipCore::controlAuthorityTelemetry()` and effective mass/inertia to client, with source revision and stale-snapshot guard. Until then do NOT claim fully authoritative round-trip wire integration.

**Existing adapter fixed:** `ShipControlAuthorityTelemetry.h::applyAuthorityToControlParams` now clamps directional limits to signed physical motor basis AND clears `forwardMainEngineAvailable` / `reverseMainEngineAvailable` when respective acceleration becomes zero. This is essential because ShipDynamics historically interprets numeric zero with an available flag as the OLD descriptor's unspecified rating and substitutes the g-load envelope. Real destroyed motor bank must never be magically resurrected. Signed-axis minima also constrain lateral and angular values. New `hasSignificantAssistedAuthorityLoss` compares all 12 directional maneuvers with 5% hysteresis.

**Navigation wiring:** `SpaceState.cpp` derives live Assisted basis before forming `makeNavigationVehicleProfile`, route `RoutePlanRequest`, asynchronous worker `compilePhysics`, or calling client virtual pilot. It refuses planning when installed forward/reverse/lateral/pitch/yaw basis is genuinely zero. Stores planned basis at worker launch, compares at adoption; stores accepted basis and checks it every execution update. Material loss invalidates unsafe old trajectory, logs `phase=accepted-trajectory-invalidated`, releases autopilot control safely for human takeover; a new request is required. **Automatic damage replan is NOT implemented**; fail-safe handback is the temporary deliberate behavior, not the final autopilot feature.

**Simultaneous force sharing:** `src/game/navigation/AssistedConcurrentManeuverBudget.h` defines `ConcurrentTurnAxisFraction=1/3`. Physical motor force/cone sets are convex under reduced combinations; reserving a third each for path-normal curvature, braking, hull attitude prevents treating three independent peak axis readings as simultaneously available. Assisted route planning restricts lateral+braking estimates accordingly; `DedicatedClientRouteAutopilot.h` caps pre-curve target speed additionally by `sqrt((installed_lateral/3)/abs(kappa))` and computes pre-turn slowdown from one-third effective braking. Newtonian unchanged. This is a conservative bounding approximation; an **exact simultaneous achievable motor-wrench query** from the motor basis (including sign, thermal irregularity and attitude coupling) remains required to replace the fixed 1/3 model.

**Important correction to previous somersault diagnosis:** In physical `ShipControlExecutionGateway.cpp`, installed-engine `actual.accelerationBodyMps2` overwrites `motion.mainEngineAccelerationMps2` and `motion.assistedStabilizationAccelerationMps2` is reset to zero after matrix delivery. Therefore `[DockClientTrack] main_a_mps2≈0.01557` on this path can indeed mean virtually zero **total delivered installed-engine acceleration**, NOT merely the longitudinal component. Check frame phase and `engineTelemetry` if attributing the cause. Added `required_curve_a_mps2`, `installed_turn_a_mps2`, `motor_angular_a_radps2`, and `assisted_aux_a_mps2` to live dock telemetry to resolve delivered accel/torque vs v²κ demand.

**Regression tests SOURCE-ADDED (not yet native-run):** `tests/navigation_runtime/AssistedAuthorityContractTests.cpp` (new CMake target `assisted_authority_contract_tests`, CTest name `assisted_authority_contract`), checks lost forward bank never reverts to ideal, signed asymmetry, 30% damaged angular basis rejection, 2% jitter survival, unavailable installed data fail-closed, and curvature budget. `tests/architecture_contracts/check_assisted_docking_entry_contract.py` now checks the source contract. Need MSYS2 MinGW native C++ compilation and runtime proof before PASS; no claim that any native test or live flight has passed.

**Commands on the real Windows repo worktree, preserving local edits:**

```bash
cd /d/__elite/work
git status -sb
git fetch origin
git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime --target assisted_authority_contract_tests route_entry_binder_tests route_volume_containment_tests docking_advisory_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^(assisted_authority_contract|route_entry_binder|route_volume_containment|docking_advisory)$' --output-on-failure
python tests/architecture_contracts/check_assisted_docking_entry_contract.py
bash build_mingw64.sh
./build/EliteGame.exe
```

**Live acceptance next:** stationary and 350–400 m/s Assisted-I dock starts into k≈0.0005–0.001 1/m, visible tunnel and tangent-aligned velocity, no 360° hull flips, inspect `required_curve_a_mps2` versus `installed_turn_a_mps2/3`, turn speed ceiling, early braking and actually delivered installed motor accel. Repeat Assisted-Y/X and damage/overheat/2s engine impact shock while route is pending and executing; require explicit invalidation/handback on loss. Include Newtonian isolation tests to ensure none changed. Keep all runtime logs in D:\tmp. Fix first compile/test issue based on exact native log, no speculative PASS.

---
## 2026-10-19 — LIVE ASSISTED DOCK AUTOPILOT SOMERSAULTS: TURN-AUTHORITY / THRUST-STARVATION DIAGNOSIS (NO CONTROLLER CODE CHANGED)

**User evidence:** recording `20261019-1221-12.4579931.mp4` (about 221.5 s) and console dump `Вставленный текст(20261019-122638).txt` (869 lines). Examined video frames and all 246 `[DockClientTrack]` records. Following numeric prefix fix, `DockLateEntry disposition=2` and `DockClient phase=executing`: route successfully adopted; thus the old BeforeStart failure is gone in this run. Five semantic route stages logged, initial planner worker 11238.6 ms. Do NOT claim dock completion: log ends with GLFW close; flight remains pending.

**Observed first upset** ~page 154–183 / log lines 173–200: at actual speed 352→324→319→299 m/s, live curvature rises 0.000418→0.000546→0.000746→0.000977 1/m; theoretical required lateral a=v²*k reaches 51.8→57.5→76.0→87.7 m/s². Prescribed turn target speed falls to 262.8→218.1→160.9→134.4 m/s while actual velocity remains too high. `speed_rate=-1` commands hard slowing but `main_a_mps2` (only the magnitude of `motion.mainEngineAccelerationMps2`, *NOT* total Assisted lateral stabilization) collapses to roughly 0.01557 m/s² for multiple track rows. Course error spikes to 71.3 degrees, hull pose error 127 degrees, cross-track 252 m; craft then recovers.

**Observed second upset** ~page 407–433 / log lines 387–417: actual speed 336→318 m/s entering increasingly tight k 0.000634→0.000888 1/m, requested geometric lateral demand 71.7→89.7 m/s² while current target speed 232.7→178.4 m/s. `speed_rate=-1`, main longitudinal vector magnitude ~0.01557 m/s² in repeated logs. Cross-track rises from ~13 m to 378.1 m; peak course error 77.24 degrees, hull error 126.12 degrees, body angular speed up to ~1.65 rad/s over the flight. Two somersaults reproduce the same physical signature.

**Source mechanism found (strong code-backed hypothesis, requires targeted physical instrumentation for full proof):**
- `src/game/navigation/DynamicMotionSystem.cpp::applyLocalFrameInput` computes automatic Assisted lateral stabilization from nose-relative velocity error; it admits this before requested main longitudinal acceleration via `clampSecondaryToTotalAccelerationEnvelope(motion.assistedStabilizationAccelerationMps2, requestedMainAcceleration, totalLinearEnvelope)`. Therefore when lateral demand saturates the total force envelope, commanded longitudinal braking can be starved even at `speed_rate=-1`. Its separate `assistedStabilizationAccelerationMps2` is NOT printed as `main_a_mps2`; instrument delivered vector/main+stabilization and per-motor wrench before claiming total thrust failure.
- `src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h` uses nominal `ShipParams` to derive braking/turn speed and `v*kappa` angular-rate feedforward. The current Assisted client-side planner does NOT consume the newly exposed six-axis delivered `ShipControlAuthorityTelemetry` directly, per earlier documented open task. It may demand 50–90 m/s² lateral force despite combined real current authority and competing angular/throttle control being lower.
- `src/game/navigation/autopilot/AssistedAutopilot.h` converts VELOCITY-vs-tangent course error into hull pitch/yaw actuator commands, while `HullAttitudeControl` adds angular-rate feedforward. Turning hull does not instantly rotate velocity at hundreds of m/s: course rate needs delivered normal acceleration (`a_lat=v²*kappa`) across real motors. Once underactuated, hull can spin much more than velocity course and overcorrect. `course_lead_m=0`, `phase_lead_deg=0` at all 246 records by current explicit design; forecast cross-track is diagnostic-only. Do not blindly steer ahead to a different path; instead plan available force/burn schedule and speed BEFORE the curve while keeping the velocity navigation target equal to route tangent.

**Diagnosis scope:** The log is sufficient to demonstrate speed/curvature infeasibility and lack of usable main command at critical turns, but NOT sufficient to identify which physical motor-basis saturates or prove entire total acceleration is zero. Do NOT change the frozen Assisted manual flight law or Newtonian path during diagnostic review. Do not fix by arbitrary increased gains or silently deleting the tangent contract.

**Next engineering step:** add read-only live per-frame telemetry around tight turns: `a_required=v²*kappa`, physical six-axis +/- authority and combined actuator allocation, world/body executed acceleration split (main, Assisted stabilization), demanded vs achieved force/torque, angular acceleration, braking reserve, planned v_safe and ability to slow before curve. Build native stress/flight tests for v=350–400 m/s into k≈0.0005–0.001 /m, no-hull-flip, stable path error, braking demand actually delivered, and no Assisted/Newtonian cross-contamination. Plan v_safe using actually attainable simultaneous lateral+braking+angular force, and preview braking distance/actuator lag. If route cannot be flown, fail/replan BEFORE curvature rather than execute an unachievable bend. Preserve nose-vs-velocity distinction and autopilot as pilot-level input only. Capture subsequent run before marking PASS.

**Status:** Analysis only; no new C++ changes or in-game acceptance performed for the somersaults. Single state document remains this root PROJECT_STATE.md. Runtime logs must go only to D:\tmp.

---
## 2026-10-19 — ROUTE BOUNDARY PRECISION AUDIT (COMMITTED; NATIVE/LIVE GATE PENDING)

User requested a review of ALL analogous geometry comparisons after live docking entered an endless BeforeStart late-entry-rebind loop at along_m=-0.000147 m while almost stationary. This may reflect actual sub-millimetre coast drift during the 5–10 s asynchronous worker, world/frame arithmetic, or both; numeric rounding ALONE has NOT been proven. It is wrong to demand exactly nonnegative along at an authored prefix endpoint.

**Reviewed runtime navigation surfaces:** SpaceState automatic docking orchestration; RouteEntryBinder; DockingAdvisoryPlanner; RoutePlanner; SegmentedRouteComposer; RouteStageCompiler; TrafficRoutePlanAdapter; RouteVolumeContainmentValidator; RouteFrameField; ClientRouteAutopilot and DedicatedClientRouteAutopilot; Assisted safe-course, route-speed, trajectory-safety and other nearby navigation modules. Differentiated strict computational degenerate-vector/curvature/speed checks from decisions about independent geometric representations of the same authored point.

**Policy file (ONE source):** src/game/navigation/NavigationGeometryTolerance.h. Never round underlying dvec3/route curves, collision geometry, or double physics state. ConstructedPointMeters = 0.001 m for joins, via-point duplicates, portal-to-curve lookup and geometric stage boundaries. EntryStartBackstepMeters = 0.01 m, EntryEndOvershootMeters = 0.01 m for tiny stationary/ballistic drift around initial route-line endpoints, with returned along clamped to [0, line length]. ComposedStageLengthMeters = 0.01 m for aggregate multi-leg progress accounting. Units and intended domains are explicit. These are NOT hull/corridor/obstacle-clearance allowances and must NEVER weaken hard collision/BLUE containment or physical dynamics. Keep small 1e-9/1e-12 thresholds for vector degeneracy, parametric interpolation, normalized curvature and algebra where a physical metre comparison is not intended; kNumericalInsetMeters stays an inward (conservative) collision margin.

**Production files modified:** RouteEntryBinder.cpp (start + end tolerance, clamp, anchor matches), DockingAdvisoryPlanner.cpp (authored waypoint/mandatory tangent interval matching), planner/RoutePlanner.cpp (duplicate presentation gates), traffic/SegmentedRouteComposer.cpp (duplicate gates and multi-leg stage accounting), traffic/RouteStageCompiler.cpp (duplicate points), traffic/TrafficRoutePlanAdapter.cpp (avoid invented microscopic semantic stages/gaps), traffic/RouteVolumeContainmentValidator.cpp (portal-to-canonical-curve lookup only, NOT collision clearance), SpaceState.cpp (match terminal dock via using same tolerance, fix previously malformed literal AssistedSafeCourse reason log, bound otherwise unlimited late-entry connector rebind misses to 4 and fail safely with explicit phase=entry-rebind-exhausted). Assisted/Newtonian control laws, thrust and actual collision/velocity constraints NOT modified.

**Native regressions added:** RouteEntryBinderTests covers -0.5mm and -5mm start adopted/clamped, -5cm and -25m rejected, 5mm endpoint overshoot adopted/clamped, 5cm and 100m past-end rejected, and off-axis rejection unaffected. RouteVolumeContainmentValidatorTests checks portal reconstruction offset 0.5mm accepted but 5cm mismatch rejected, retaining existing invalid Bezier and hull-clearance checks. Existing segmented route/traffic compiler/docking planner tests must run. tests/architecture_contracts/check_assisted_docking_entry_contract.py now guards common policy, both endpoint checks, and finite rebind budget. All tests are SOURCE-ADDED, NOT native-executed here.

**Important boundary:** on repeated non-OnPrefix rebind results at different real positions, four misses now produce an explicit DockClient failure and safe Human handback instead of an infinite CPU/replan loop. Do not claim the underlying route can always be recovered. Effective first-run worker ~9.74 s and 5–6.6 s rebound computations remain a separate performance issue.

**MSYS2 MinGW64 validation (must run on target machine):**

```bash
cd /d/__elite/work
git status -sb
git fetch origin
git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime --target route_entry_binder_tests route_stage_compiler_tests route_volume_containment_tests segmented_route_composer_tests docking_advisory_tests route_planner_api_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^(route_entry_binder|route_stage_compiler|route_volume_containment|segmented_route_composer|docking_advisory|route_planner_api)$' --output-on-failure
python tests/architecture_contracts/check_assisted_docking_entry_contract.py
bash build_mingw64.sh
./build/EliteGame.exe
```

**Live acceptance:** first START DOCKING produces OnPrefix disposition=2 then phase=route-ready, visible tunnel and physical ship traversal. Both Assisted and Newtonian route adoption, all I/Y/X layouts if available. Deliberately start at rest and at modest velocity, close to route origin, BLUE transit, and near an endpoint. Confirm real 5cm+ off-axis/back/past-prefix requires correction, no silent clipping, and no repeated planner-worker storm. Runtime logs only to D:\tmp. If command fails, share FIRST native compiler/CTest error before assuming source is accepted.

---
## 2026-10-19 — DOCK ROUTE READY BUT REJECTED AT SUB-MILLIMETRE START (SOURCE FIX; NATIVE/LIVE PENDING)

**Live evidence:** START DOCKING #1 generated route stages (free, capture, BLUE lane, release, terminal), but never adopted the result. DockLateEntry logged disposition=1, along_m from -0.000147 to approximately -0.0000007 m, first-line prefix_m=remaining_m=1000, cross_m ~0.000002 m and effectively zero actual speed. DockClient repeatedly entered entry-rebind-retry (at least seven attempts). Initial worker lasted 9.74 s; rebound workers 5–6.6 s. The route stages exist; the immediately demonstrated failure is the route handoff predicate.

**Code root cause:** src/game/navigation/planner/RouteEntryBinder.cpp::inspectBallisticPrefix rejected EVERY negative along position using along < 0.0. Disposition 1 means BeforeStart. SpaceState.cpp retried a fresh entry connector for any non-OnPrefix outcome, so floating-point start-position noise prevented route adoption even while effectively stationary.

**Fix committed on feature/propulsion-authority-matrix-2026-10-16:** RouteEntryBinder.cpp now tolerates at most 0.001 m of negative along-track start noise, clamps accepted along distance to zero, recalculates remaining distance accordingly; real earlier position beyond tolerance remains BeforeStart. Cross-track and past-prefix checks remain intact. tests/navigation_runtime/RouteEntryBinderTests.cpp now checks -0.0005 m accepted and clamped, and -0.005 m rejected; existing larger early, off-axis and beyond-end tests remain. Changes do NOT touch motor physics, manual control, Assisted/Newtonian route geometry or autopilot law. Code commits 653425e77ebb and 67288a9be41c.

**Validation:** GitHub code and tests committed; no native MinGW build, CTest, or game run was performed here. Do not report PASS before user Windows evidence. Run:

```bash
cd /d/__elite/work
git status --short
git fetch origin
git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime --target route_entry_binder_tests -j 8
ctest --test-dir build/tests/navigation_runtime -R '^route_entry_binder$' --output-on-failure
bash build_mingw64.sh
./build/EliteGame.exe
```

**Live success criterion:** DockLateEntry reports disposition=2 (OnPrefix) instead of disposition=1 and DockClient reaches phase=route-ready, renders the tunnel and begins actual flight. If not, inspect new DockPlanFailure, DockLateEntry and DockClient lines. Secondary issues NOT yet fixed: 9.74 s full planner and 5–6.6 s entry connector, plus malformed literal safe-course log reason= << safetyResult.message <<. All runtime logs under D:\tmp.

---
## 2026-10-19 — IMPLEMENTED COOLANT→ENGINE DAMAGE, CINEMATIC IMPACT SHOCK, CONTROL-AUTHORITY TELEMETRY (NATIVE TESTS PENDING)

**User approved and requested code:** couple engine heat to damaged ship cooling; impact-triggered 70%-style coughing power loss followed 1–3 s later by once-randomized self-recovery of two thirds of the hit or no recovery; add a read-only ship control-authority telemetry layer for the autopilot, then verify game manual/autopilot behaviour. Branch `feature/propulsion-authority-matrix-2026-10-16`. **Changes committed to branch; DO NOT claim native or gameplay PASS.** Previous simplified cached I/Y/X Assisted maneuver executor is retained; Newtonian continues separate RCS and main control.

### Implemented physical heat/cooling
- `src/game/equipment/engine/EngineUnit.h`: `EngineOperatingState.coolantTemperatureK` stores ACTUAL last common coolant boundary. `safeEmitterThrustFraction01` uses it (legacy 1050 K only when uninitialized). Existing direct annulus heat radiation is retained. `advanceEngineOperatingState` now respects finite engine thermal mass and separates actual conductive **coolant heat transfer** from internal energy storage: `P_coolant = P_waste - P_emitter - C*(T_next-T_start)/dt`, including negative transfer when coolant warms a colder engine. Trapezoidal thermal-damage calculation retains five damage causes; protective thermal governor automatically derates as coolant warms.
- `ObjectModuleRuntime::advanceEngineOperatingLife` returns actual conducted heat via optional output pointer, preserving all previous call sites.
- `ShipCore::updateSystems` now passes `m_thermal.getTemperature()` rather than engine descriptor's fixed nominal coolant K; adds signed transferred heat (MW*dt) once into the shared coolant; calls `m_cooling.update` AFTER engine heat has entered the reservoir. No double radiation of the nozzle's direct emitter heat.
- `CoolingSystem::update` now caps radiator heat removal by available pump circulation and `maxHeatTransfer`, in addition to emissive surface. Damaged panels/overheated or unavailable pumps can elevate shared coolant temperature, thereby heating every mounted engine, temporarily limiting output and eventually adding permanent `Thermal` accumulated damage. Pump/radiator tuning needs IN-GAME validation.

### Implemented cinematic engine-only impact rebound
- Five canonical stored damage percentages stay **Laser/Impact/Thermal/Radiation/Wear**, no sixth permanent cause. `EngineOperatingState` stores a distinct transient `impactShock01`, timer, unrecovered portion and deterministic event serial; it is functional field instability, NOT structural healing. On collision or explosion that actually damaged an ENGINE hit-volume, `ShipCore::applyDamage` calls `ObjectModuleRuntime::addEngineImpactShock`, with reproducible per-world-incident entropy; other modules and laser/radiation remain on existing damage routes. The original hit-volume structural health loss is NOT reversed.
- Each hit instantly applies ONE THIRD of its amount to accumulated Impact and TWO THIRDS as temporary motor-field shock. Outcome is decided ONCE at event time (50% seeded chance). After 1–3 seconds, temporary field loss disappears if recovery; otherwise remaining two thirds enters permanent Impact. Example 70% hit: temporary total effective damage 70%, later 23.333% or 70% permanent. A single event never re-rolls each frame. Multiple hits stack under caps and share the latest pending expiry. For catastrophic structural destruction, a torn/detached motor is not restored by transient recovery.
- `evaluateEngineUnit(passport, damage, operating)` makes real `Engine.h` thrust, electricity, waste heat, gimbal and intermittent thrust reflect temporary shock immediately; adds a shock-specific irregular-thrust term so 70%-exact hits audibly/physically 'cough' even though normal sustained instability begins above 70%. It is physically executed and read by existing true RED/YELLOW motor telemetry.
- `ShipCore::applyDamage` immediately refreshes module-derived actuator capability (incident-driven only), preventing old undamaged performance projections from persisting after a hit.
- `ObjectModuleSnapshot`, its builder, `WireDataSchema` and client `ShipCapabilityBuilder` now replicate hot coolant boundary and active shock; schema version **17** in `WireDataCodec.h`. BOTH client/server must be rebuilt. No private transient countdown progression is computed by client: authoritative replicated snapshot is source.

### Physical autopilot-control telemetry
- New read-only `src/game/ship/telemetry/ShipControlAuthorityTelemetry.h` publishes **sign-separated six-axis** available acceleration and torque authority from the SAME 12 prepared damage-aware Assisted motor programs. Distinct from BITE fault history and `EngineLiveTelemetry` output-force HUD.
- `ShipCore::controlAuthorityTelemetry()` publishes current per-vessel program capability, invalidated when the module projection revision changes. `applyAuthorityToControlParams` conservatively clamps existing `ShipParams` to actual program output, preserving public control API. Server `GameSimulation.cpp` NPC autopilot passes the telemetry-limited parameters directly into its `AutopilotLawController`; this helps stop over-promising physical acceleration as engines fail.
- **OPEN:** legacy client-side `ClientRouteAutopilot`/`DedicatedClientRouteAutopilot` still take `ShipParams`; their replicated client physics envelope has been corrected for coolant/shock but a DIRECT control-authority readout is not yet wired into both client-specific speed/curve planners. Do not claim complete closed-loop corridor resonance solved. Need unify client route planner execution authority with physical six-axis readout, and test actual speed/curvature limits; no changes to established manual steering law in this task.

### Tests and validation status
- `tests/architecture_contracts/EngineUnitDamageModelTests.cpp`: added high-coolant derate and exact heat budget/storage assertion; adjusted damaged-annulus coolant expectation for nonzero housing energy storage.
- `EquipmentDamageFailurePipelineTests.cpp`: 70% impact sets lasting 1/3 and field 2/3, irregular thrust, deterministic one-shot settling after <=3 s, permanent/no-rebound alternatives, no healing unrelated wear or laser.
- `WireDataPlaneContractTests.cpp`: schema 17 payload round-trip includes engine coolant K, impact field shock and remaining timer.
- `InstalledEngineActuatorTests.cpp`: actual six-axis authority telemetry tells autopilot reduced usable acceleration after removing one installed I/Y/X motor.
- `check_engine_six_axis_matrix.py` source checks extended to new heat/coolant, impact, telemetry and wire wiring; the legacy v16 assertion changed to v17.
- No local C++ project is available here and direct `github.com` from container is unresolvable; native CMake/CTest, actual run and screen inspection NOT done. Source wiring inspected only; **possible build/runtime regressions remain**. Keep ALL game logs under `D:\\tmp`. Review cooling pump power and thermal duration in gameplay; test Assisted-I/Y/X and Newtonian-I/Y/X, button release damping, manual Num8/Num2, highway/corridor autopilot overshoot, single-engine hit and 2-s recovery. Validate both client & server network v17.

### User MSYS2 MinGW64 verification
```bash
cd /d/__elite/work
git status --short
git fetch origin
git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja
cmake --build build/tests/architecture_contracts --target engine_unit_damage_model_tests equipment_damage_failure_pipeline_tests installed_engine_actuator_tests wire_data_plane_contract_tests -j 8
ctest --test-dir build/tests/architecture_contracts -R "engine_unit_damage_model|equipment_damage_failure_pipeline|installed_engine_actuator|wire_data_plane_contracts" --output-on-failure
python tests/architecture_contracts/check_engine_six_axis_matrix.py
bash build_mingw64.sh
./build/EliteGame.exe
```
Check exact CMake target names against `tests/architecture_contracts/CMakeLists.txt` if they differ. First compiler errors or failing tests are to be fixed before game acceptance.

**Single source of context remains THIS ROOT PROJECT_STATE.md.** No new status files.

---

## 2026-10-19 — ASSISTED I/Y/X PRECOMPUTED SIX-AXIS MANEUVERS WITH DAMAGE-AWARE REBUILD (UNTESTED NATIVE)

**User agreed** to replace the runtime general 96-pass optimizer with a simpler deterministic maneuver-program system; explicitly accepted that one lost engine does NOT always prevent stable forward motion when remaining engine nozzles can redirect by up to 75°. The exact surviving nozzle angle (e.g., 60°) is geometric/operating-state-dependent; 60° is NOT hard-coded. A single nozzle at 60° supplies 50% of its force axially and ~86.6% transversely; only the TOTAL `ΣF, Σ(r×F)` determines whether the hull travels straight without rotating.

### Committed design / files
- New `src/game/ship/plant/ShipPresetEngineManeuvers.h`: **12 signed physical basis maneuver programs**, +/− for body X,Y,Z and pitch,yaw,roll, for each Assisted-I/Y/X ship. Basis built from INSTALLED motor Newton rating, fore/aft placement, CoM, mass, inertia, damage/temperature-limited available thrust and TVC cone. Each basis is a vector of per-engine axial, transverse-X and transverse-Y force fractions (actual force not throttle guesses), derived using a 6x6 geometric force/torque pseudoinverse. Physical positive axial thrust cone, 75°/damage TVC and per-engine max thrust are enforced. Exact symmetric equal-Newton preset for ordinary pure forward/reverse; neither unwanted gimbal turn nor parasitic torque. For failed/uneven motors the one-time generator uses bounded affine projection onto the physical engine cones; when the nominal maneuver is impossible, it searches a **lower achievable acceleration** with no large lateral/torque residual. This yields a physically valid slower straight maneuver with surviving motors, if one exists.
- The **per-ship** `game::ship::plant::AssistedManeuverPrograms m_assistedManeuvers` cache now lives in `src/game/ship/core/ShipCore.h`, not in network DTOs. On body/motor identity/capability/geometry changes `matches` invalidates and rebuilds all 12 templates. Up to 144 cone-correction iterations plus bounded binary scale search are a *one-off when rebuilding*, **NOT during flight execution**. This trades rare damage-event CPU work for deterministic cheap steady-state flight. Stochastic irregular delivered thrust is still measured each tick and common scaling prevents per-motor exceeding instantaneous rating. Minor wear changes below 0.5% threshold do not trigger unnecessary recomputations.
- `ShipControlExecutionGateway.h/.cpp` accepts optional cache. For Assisted with attached motor views it calls `maneuverPrograms->execute` rather than `executeEngineControlMatrix`; it combines up to six basis programs in constant bounded time, globally scales mixed motor force vectors against instantaneous motor headroom (thus preserving mix force/torque balance), then runs every `Engine::execute`, obtains exact `ΣF` and `Σ(r×F)`, mechanical angular acceleration, per-motor RED, available YELLOW, actual per nozzle angle, MW and heat. No artificially applied body forces/attitude; existing controller and live telemetry remain.
- **Newtonian deliberately unchanged** this turn: its independent RCS handling already works; old `executeEngineControlMatrix` remains active for mounted Newtonian motors, and optional cache not used in that law. Do NOT claim the six Newtonian/Assisted configurations all switched to the new preset runtime executor; **Assisted I/Y/X are migrated now, Newtonian is pending separate isolated verification**. Old 96-pass code remains for Newtonian and non-cached tests for compatibility, but NOT on native Assisted main motor execution.
- `ShipCore.cpp` sends a pointer to its persistent physical-template cache for all native/virtual Assisted I/Y/X; templates regenerate on motor failure, repair, power/heat/CoM/load changes.
- User's earlier agreed controller rules remain: manual rotations and keypad translation self-stabilize; +/− axial propulsion is inertial; autopilot is same input interface as pilot. The earlier autopilot corridor-centering overshoot remains **open** pending explicit authoritative capability export from this new cache; must not imply new motor controller fixed navigation automatically.

### Source audit/tests added (NATIVE PENDING)
- `InstalledEngineActuatorTests.cpp` now tests production `AssistedManeuverPrograms` rather than obsolete 96-iteration matrix for previous six-axis and Num8/Num2 acceptance; test `preparedManeuversSurviveSingleMainEngineFailure` loops Assisted I/Y/X, proves healthy axial/lateral no parasitic torque, detaches an aft engine mid-run, expects reduced real forward authority with negligible lateral/angular leak, survivor nozzle vectoring, residual pitch damping, and repaired full performance using SAME cached ship.
- `tests/architecture_contracts/check_engine_six_axis_matrix.py` guards new cache, real forces, no per-frame optimization, checks tests and current motor gateway. Existing Newtonian tests remain.
- **No native build/CTest/game was run in this GitHub-only session.** The prepared maneuvers must be tested on the user's actual MSYS2 Windows build, especially when the engine array is damaged, thermal throttle decreases, or multiple control axes saturate. The numerical prototype in Python on approximate 260t Cobra-I geometry with aft-left missing yielded one example of straight but reduced forward acceleration around 28 m/s² after full 73.55 m/s² command, with skewed remaining-engine thrust. This is **NOT a verified in-game speed**; actual I/Y/X and effectivePhysics limits must be observed. A 60° nozzle turn isn't mandated; in that prototype individual angles differed.
- Recommended verification:
```bash
cd /d/__elite/work
git fetch origin && git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16
cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja
cmake --build build/tests/architecture_contracts --target installed_engine_actuator_tests -j 8
ctest --test-dir build/tests/architecture_contracts -R installed_engine_actuator --output-on-failure
python tests/architecture_contracts/check_engine_six_axis_matrix.py
bash build_mingw64.sh
./build/EliteGame.exe
```
Run Assisted I/Y/X normal and one-engine-out, pure forward/reverse, lateral, pitch/yaw/roll, release damping, HUD power bars. Save runtime logs only `D:\\tmp`. If failure test fails, do NOT pretend healthy/stable; fix exact output and rerun. Benchmark damage-triggered basis rebuild vs NPC count; don't add per tick heavy inversions.

---

## 2026-10-18 — ASSISTED Num8/Num2 POST-MANEUVER PITCH REGRESSION: PHYSICAL MAIN-BANK FIX (NATIVE PENDING)

**User symptom:** In Assisted, keypad 8/2 body-forward/back after key release can trigger strong pitch oscillation. Newtonian RCS does not show it. Do not change Newtonian manual input or physics.

**Source-level analysis:**
- `PlayerInputMapper.cpp` maps KP8/2 to `forwardInput`; Assisted `DynamicMotionSystem::applyLocalFrameInput` turns it into main-vectoring authority and velocity cancellation; `ShipControlExecutionGateway.cpp` routes this through `ShipEngineControlMatrix.h`. Newtonian uses independent RCS.
- Old `ShipEngineControlMatrix` passed even pure-body-axial force and 0 angular demand through a general six-axis FISTA optimizer with rotational weights 3 vs linear 1. Healthy symmetrically installed engine banks should emit **ZERO angular acceleration and neutral nozzle angles** on pure Num8/2; relying on optimizer approximation for this trivial balanced case admits unwanted cross-axis torque under numerical/frame noise or degraded installations.
- Separate local numerical reproduction of the algebra (NOT native C++) showed concurrent full body-axial demand (+/-73.55 m/s²) and physical pitch-damping demand (-1 rad/s²) returned only ~-0.176 rad/s² actual pitch acceleration at 3:1 angular weight. With 80:1 weight, returned ~-0.95 rad/s² in Assisted-I and ~-0.93…-0.99 for Y/X. The previous controller therefore had insufficient corrective angular authority while it was also braking translation. This can explain the observed delayed oscillation; exact root-cause confirmation still requires runtime log/game.
- **Fix:** `src/game/ship/plant/ShipEngineControlMatrix.h` now has an **axial-only deterministic bank allocator**, engaged only when lateral demands are numerical noise and angular demand is zero. It selects the correct physical aft/fore engine bank, takes installed engines' **actual instantaneous available Newtons**, balances equal physical force over surviving engines, executes real `Engine::execute` with neutral nozzle orientation, then computes the **actual** `ΣF` and `Σ(r×F)` per installed geometry. This guarantees no artificial pitch/yaw/roll for symmetric healthy I/Y/X axial maneuvers, keeps damaged-motor response physical, and honors the real HUD telemetry. Do NOT zero the ship pitch rate or add a synthetic anti-torque; stabilization remains engine-executed.
- **Fix:** Main matrix's six-axis objective now prioritizes requested angular braking over axial precision (angular weights 80 instead of 3); this is a law-neutral numerical error priority, **not a hardcoded ship capability**. Under competing forward/back thrust and damping, the allocator must execute the stabilizing torque instead of starving it. Newtonian still submits zero main angular demand and uses its isolated RCS for attitude.
- **Tests:** `tests/architecture_contracts/InstalledEngineActuatorTests.cpp::assistedKeypadAxialReleaseHasNoParasiticPitch` covers all Assisted I/Y/X, KP8/2 signs, zero induced angular torque including tiny off-axis float-frame noise, actual physical pitch damping on release and **combined full axial + pitch-damping** demand, and neutral no-burn state. `check_engine_six_axis_matrix.py` locks the split and angular priority. Existing explicit per-engine TVC, BITE and HUD remain.
- **Status:** GitHub branch modified, **no native CMake/CTest nor in-game test was run here**. Follow-up on Windows MSYS2: `git pull --ff-only origin feature/propulsion-authority-matrix-2026-10-16`; `cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja`; `cmake --build build/tests/architecture_contracts --target installed_engine_actuator_tests -j 8`; `ctest --test-dir build/tests/architecture_contracts -R installed_engine_actuator --output-on-failure`; then `bash build_mingw64.sh`, run `./build/EliteGame.exe`. Reproduce Assisted-I/Y/X KP8/2 hold→release, watch pitch angular-rate and engine red bars. Compare Newtonian separate RCS. Put logs only in `D:\\tmp`. If pitch persists, capture actual `pitchRate`, `requestedMainAngularAccelerationBodyRadPerSec2.x`, `mainPropulsionAngularAccelerationBodyRadPerSec2.x`, per-motor thrust and actual angular command on release to isolate feedback vs matrix sign/cadence.
- Further open: autopilot's effective authority/response binding remains an independent issue; do not tune autopilot to hide manual KP8/2 torque.

---

## 2026-10-18 — GAMEPLAY TVC DOCTRINE: PRESERVE ACCEPTED ASSISTED HANDLING BY RELAXING REALITY (SOURCE COMMITTED, NATIVE PENDING)

**User decision** after the six-axis regression: prioritize the old responsive flight law. Do NOT abandon engine physics or fake HUD force. Instead make fictional vectoring-engine NOZZLES turn effectively INSTANTLY and widen their travel as required to preserve original gameplay. No expensive dynamic/nozzle servo simulation. Keep the common Engine/preset and installed-motor matrix; evaluate per-engine `ΣF` and `Σ(r×F)`, but let the fictional actuator satisfy legacy manual/autopilot demand. Main mission: ENGINE PHYSICS MUST FIT ACCEPTED PILOT/AUTOPILOT BEHAVIOUR, not the opposite.

### Changes in this iteration
- `src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h`: nominal Assisted Cobretty CR-1 field-vector limit **75°** (`1.3089969389957472 rad`) instead of 7°; nominal rate 1000 rad/s as descriptive near-instant limit, NO stepwise gimbal physics simulated. Damage-reduced available gimbal remains from common engine equipment law. No change to nominal axial force (9,561,483.75 N/motor), efficiency, material, thermal, wear or BITE state.
- `ShipEngineControlMatrix.h`: remove previous nozzle angle arrays, `elapsedSeconds` demand field, rotation slew dynamics/angle interpolation and old post-optimization lag. Choose nozzle direction afresh every control command, apply only real motor availability and cone-angle bounds. Instant idle returns nozzle directly to zero with no spent force. Increase thrust-cone projection ceiling from 1.2 rad (~68.8°) to 1.45 rad (~83.1°) so the preset's 75° is genuinely reachable; don't accidentally have a second hidden cap. Retain 6-axis finite-force matrix, FISTA optimizer and consistent hull forces/moments/telemetry, now 96 rather than 240 deterministic passes to spare game/NPC CPU. `ShipControlExecutionGateway.cpp` no longer copies historical servo angles into the matrix. HUD still reads the physically returned motor force, not old demand heuristics.
- **Newtonian isolation:** when `familyOf(model) == Newtonian`, matrix bounds final independent main-nozzle angle by the existing `ShipParams.mainGimbalYawLimitRad/mainGimbalPitchLimitRad` (normally 7°) as well as damage-reduced engine max. Existing Newtonian `DynamicMotionSystem::updateNewtonianMainGimbal` continues to time-limit the INPUT request; we do NOT grant Newtonian the wide 75° Assisted field vectoring or replace independent RCS. A dedicated native test confirms the narrow limit. This prevents changing Newtonian manual-control doctrine as collateral damage from an Assisted gameplay tuning.
- **Engine-to-orientation timing:** `GameSimulation.cpp` used to call `ship.propagateMotionOrientation(fdt)` BEFORE `shipPtr->updateTranslationControl` and thus before the actual new matrix generated its torque. Tactical ships with valid travel frames now integrate orientation immediately AFTER the physical motor control in the second loop, at the SAME authoritative fixed step. Coarse-cadence ships still integrate their held angular rate every physics tick, non-tactical/frame-unbound ones use the original earlier stage. No new direct attitude impulse bypassing Engine.
- `InstalledEngineActuatorTests.cpp`: replace stale 8°/s slew assertions with physically bounded direct-angle checks; **demand original Cobra Assisted 73.549875 m/s² lateral**, require >=95% delivery in the one motor-execution step and no fake four-motor 100%-load on the maneuver; **demand 3.0 rad/s² pitch/yaw/roll**, require >=90% delivered torque-driven angular acceleration per axis. Extra Newtonian-I check ensures angle does not exceed original main-gimbal policy despite wide Cobretty passport. Existing damaged/failed motor, idle and HUD sample tests remain.
- `check_engine_six_axis_matrix.py` (already called by the standard native architecture test runner) now asserts 75° passport, no historical angle/slew, separate narrow Newtonian clamp, 96 optimizer rounds and torque-before-orientation step. Do not interpret static code checks as native test PASS.

### Numerical sanity check of the SAME allocation algebra, NOT compiled C++
A small offline Numpy reproduction of the C++ matrix (Cobra 260 t, four healthy 9.561 MN motors at ±13 m lateral, ±11.1 m longitudinal, nominal rotational inertias, 75° cone, weighted 6-axis projected solver) found 96 passes delivered **72.96 m/s²** requested vs legacy 73.549875 m/s² sideways, approximately **0.514 of rated thrust per motor**; pure forward 72.96/73.55 with only the aft pair lit; pure pitch ~2.93/3, yaw ~2.92/3, roll ~2.92/3. This is an approximate *math/sanity* demonstration, not production gameplay proof and doesn't include client prediction, chassis-specific CoM changes, or damaged motors. Importantly, this is capability-compatible without adding fictitious side acceleration; all force originates in installed motors.

### REQUIRED native validation before main
```bash
cd /d/__elite/work
git fetch origin
git switch feature/propulsion-authority-matrix-2026-10-16
git pull --ff-only
cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja
cmake --build build/tests/architecture_contracts --target installed_engine_actuator_tests equipment_damage_failure_pipeline_tests wire_data_plane_contract_tests -j 8
ctest --test-dir build/tests/architecture_contracts -R "installed_engine_actuator|equipment_damage_failure_pipeline|wire_data_plane_contracts" --output-on-failure
python tests/architecture_contracts/check_engine_six_axis_matrix.py
bash build_mingw64.sh
```
Then inspect Assisted-I/Y/X keypad side/up, forward/reverse, manual pitch/yaw/roll response and release braking, HOME/END and high-speed autopilot lead-turn/panic replans; separately Newtonian-I/Y/X gimbals/RCS. HUD red/available yellow + true angles are read-only and must agree with physically emitted force. Compare in-game behaviour with pre-matrix commit `f9d5a3b`; repair any new native compiler or closed-loop regression before calling it PASS. Network schema remains 16 in both processes; both need rebuild.

**Status:** Source work committed to feature branch; not merged to main; NO native compiler/CTest/game testing completed in this remote coding environment. The physics is an explicitly fictional simplification to recover prior gameplay handling. Keep this single `PROJECT_STATE.md` as the only state file.

---

## 2026-10-18 — CONTROL REGRESSION ROOT-CAUSE AUDIT (NO CONTROL CODE CHANGED)

**User report:** severe chaotic ship response after six-axis matrix feature. Compared the SAME branch **pre-matrix `f9d5a3b696e9f4f721c106d06858c7aa292af539`** (stable baseline reported by user before the matrix) to **matrix head `7aae7dfcd911f8da78cfa1e624f2edce372d0c5a`**. 32 new commits, core runtime changes in `ShipCore.cpp`, `ShipControlExecutionGateway.cpp` and `ShipEngineControlMatrix.h`. `ShipController.cpp` and `DynamicMotionSystem.cpp` were NOT modified between these refs, which confirms the old autopilot/manual law remained tuned to the OLD execution dynamics.

**Verified source-level root causes (no inference of successful game tests):**
1. **Large physically impossible Assisted lateral demand:** old `EliteCobraMk1.cpp` authored `strafeAccel=73.549875 m/s²`, `strafeDamping=6 /s`, `angularAccel=3 rad/s²`; manual and autopilot `DynamicMotionSystem::applyLocalFrameInput` directly executed an independent `assistedStabilizationAccelerationMps2` up to old envelope. The new `ShipControlExecutionGateway` sums that demand into the four nominal Cobretty motors constrained to **7° TVC cone** and **8°/s slew**. With four *healthy* 9,561,483.75 N motors and a 260,000 kg Cobra, even simultaneous all-motor max lateral `4*F*sin(7°)/m ≈ 17.93 m/s²` (<25% of demanded 73.55 m/s²). At the first 60Hz tick after zero TVC the slew limit yields <= approximately 0.34 m/s² (all four engines at full thrust; actual can be lower). The old feedback and autopilot still request 73.55, causing controller saturation, long sideways drift, chronic correction and opposing max-thrust burns. This is NOT a cosmetic HUD problem.
2. **Attitude actuator/controller mismatch:** formerly `ShipController::applyRequestedAngularAcceleration` directly updated pitch/yaw/roll rates with authored `angularAccel`/damping and rate limits. The matrix migration in `ShipCore::updateMotionControl` invokes that same controller only to compute `(rateAfter-rateBefore)/dt`, then explicitly REVERTS the rates and asks `ShipEngineControlMatrix` to execute a *different and constrained* angular response. No changes were made to the controller's gain, available actuator response, anti-windup or stopping-distance estimator; autopilot overshoot, weakened stopping, oscillation or spin can follow.
3. **Changed execution order:** `GameSimulation.cpp` calls `updateMotionControl`, then `propagateMotionOrientation(fdt)`, and ONLY afterward `updateTranslationControl`. Before migration the angular rates were already updated before orientation propagation; now the physical matrix torque is applied after orientation propagation (minimum one fixed-step response delay), worsening high-gain attitude and navigation tracking.
4. **Newtonian gimbal ownership conflict:** older `DynamicMotionSystem::updateNewtonianMainGimbal` computes real rate-limited main nozzle vector and uses it as `mainEngineAccelerationMps2`. The new Newtonian mounted matrix receives that result only as a net requested acceleration, reoptimizes individual TVC directions and applies a separate slew against `engineTelemetry` previous angles, ignoring `motion.mainGimbalYawRad/PitchRad` as the physical authoritative nozzle positions. This is potentially a second slew delay and may break explicit manual TVC and braking alignment. Newtonian angular RCS stays separate.
5. **Optimizer is open-loop with respect to preceding flight-law controls:** `ShipEngineControlMatrix.h` solves weighted six-axis requested force/torque using 240 FISTA passes, zero physical force for off-axis nonexistent authority, then rate-clamps nozzle geometry AFTER optimization; the demand feasibility, torque and thermal load returned to flight-law controller do not automatically retune its existing braking curves. Its static tests only check individual one-step signs / bounds, NOT closed-loop tracking, hold/release, high-speed alignment or full game simulation.

**Conclusion:** New physically grounded matrix cannot be considered a drop-in replacement for accepted Assisted and Newtonian dynamics. It is the immediate regression candidate, not the HUD telemetry alone. **No control code was changed in this audit**, and no new native CTest/game run has been executed here. Avoid presenting previous `14/14 SOURCE CHECK` as proof of gameplay readiness.

**Safe recovery / next task:** Preserve all engine type/passport/damage/HUD-telemetry code and **use the user-confirmed pre-matrix commit `f9d5a3b` as control-behavior reference**. First restore baseline flight response behind a reversible runtime selector (do not destructively reset entire feature branch, do not mix with Newtonian isolated code). Then correct the mismatch by deciding whether the fictional universe permits independent high-lateral authority from installed engines (larger effective vectoring than visible TVC) or the pilot/controller must be explicitly retuned for <=17.93 m/s² lateral and finite TVC slew. Preserve the user-preferred GAMEPLAY handling; ideally tune matrix capacity to satisfy existing flight-law request without hidden/free acceleration but test. Do closed-loop tests: Assisted-I forward/brake, keypad lateral/lift held/released, pitch/yaw/roll held/released and neutral stabilization, HOME/END, autopilot lead turns, Newtonian-I TVC/RCS, diagnostic I/Y/X, HUD matches actual motor outputs, hot/damaged engine behaviors. No main merge until tests.

---

## 2026-10-18 — I/Y/X SIX-AXIS INSTALLED MAIN-ENGINE MATRIX + INDEPENDENT LIVE TELEMETRY (SOURCE COMMITTED; NATIVE TESTS PENDING)

This checkpoint supersedes earlier statements that Assisted sideways and angular control can only be synthetic or HUD thrust must be estimated from net acceleration. The **only** persistent project-state file remains `PROJECT_STATE.md`. Feature branch: `feature/propulsion-authority-matrix-2026-10-16`; **not merged to main**.

### User goal and physics
- Keep gameplay physics SIMPLE: installed main engines have available thrust N from generic `Engine` + Cobretty CR-1 or other preset, signed fore/aft axis, relative I/Y/X engine placement, and independent TVC yaw/pitch bounded by each engine's nominal and damage-reduced gimbal travel/rate. The body gets **only** the sum of executed main force vectors `ΣF` and moments `Σ(r×F)`; multiply by current mass/inertias to obtain acceleration and angular acceleration.
- Added `src/game/ship/plant/ShipEngineControlMatrix.h`. `executeEngineControlMatrix(model, mounted, ShipParams, EngineMatrixDemand)` forms 6-axis matrix (body +X/right, +Y/up, -Z/forward; pitch/yaw/roll compatible with ShipTransform) for all native I/Y/X mounting topologies. It reads physical installed motor ratings, damage, heat/emitter restrictions through `Engine`, and per-motor nozzle cone/rate. Uses bounded deterministic 240-iteration accelerated projected least squares with exact positive thrust-cone projection; no need for full rigid-nozzle dynamics. ZERO demand fast-path returns no force and recenters TVC at actuator slew limit, sparing NPC CPU. Explicit axial-only guard never lights the opposite main bank for pure forward/braking; both banks may counterthrust for lateral/rotational maneuvers when needed. Vector direction is independently clamped to the previous yaw/pitch plus per-engine TVC slew and combined angle limit; final **actual** `Engine::execute` yields force, electrical load, waste heat, working thrust fraction. No slot-generated force in this mounted path.
- During development an unstable cyclic iterative solver was found (spurious symmetric full burns). Replaced with global-step projected accelerated solver. A **separate mathematical numerical probe** (not compiled game test) exercised neutral, forward, side/up, pitch, yaw and roll demands and led to this correction; physical TVC slew bounds initial lateral/angular response. Some demanded acceleration can be unattainable; only actual result is applied, never fabricated.
- `src/game/ship/core/ShipCore.cpp::updateMotionControl`: for **native Assisted** with real modules, convert existing accepted attitude controller's limited rate delta into angular-acceleration request, reset the synthetic direct delta; `ShipControlExecutionGateway.cpp` feeds that angular request, longitudinal acceleration and *keypad + automatic Assisted lateral demand* into the ONE physical engine matrix. Server applies only the returned actual translational acceleration + `r×F` angular acceleration; clears the old synthetic Assisted lateral term to prevent double-application. Preserves pilot/autopilot control laws and modes. Attached **Newtonian** main-engine translation also runs through this vector matrix but **Newtonian attitude/RCS remains isolated**: zero demanded main-engine angular acceleration; any net residual motor torque is physical, Newtonian angular control still belongs to RCS law pending a separate RCS actuator migration. Never claim a full independent RCS motor inventory is implemented.
- The old slot-only `ShipPhysicalActuatorPlant` remains as compatibility for diagnostic I/Y/X overrides lacking corresponding physical installed motor bindings. Do not interpret diagnostic synthetic-slot motion as proof of production motor vectoring.

### Ctrl+F9 diagnostic I/Y/X topology parity and source-audit follow-up
- Initially the new physical matrix was reached only by ships whose runtime engine-module bindings matched the native layout. That would have left **Ctrl+F9 Y/X diagnostic constructions** using the retired slot-only simulator. Fixed: `ShipCore::updateTranslationControl` now mounts temporary healthy **Cobretty CR-1 `Engine` views** for each virtual diagnostic main-engine placement (never modifying native engine health/heat/wear), and submits them to the exact same six-axis physics matrix. It does NOT claim those virtual engines are installed/repaired physical modules; they exist for manual I/Y/X layout diagnosis only. RCS clusters remain Newtonian RCS.
- Assisted diagnostic I/Y/X attitude now also routes controller angular demand into physical engine torque, while legacy unbound non-diagnostic fixtures retain their existing behavior. The BITE-HUD fault calculation no longer associates a virtual diagnostic engine with an unrelated native Cobra module ID.
- Neutral per-engine thrust direction now reads `EngineSlotDefinition::nominalForceDirectionBody` (currently ±Z for all I/Y/X main motors); TVC adds independent yaw/pitch subject to each engine's rated and damage-constrained angles/slew. The actual nozzle pose is stored in live telemetry for future cockpit/3D visualization.
- New `check_engine_six_axis_matrix.py` guards the Ctrl+F9 virtual-engine seam. Do not claim performance/behaviour green until compiling native tests and watching the live game, especially all Assisted I/Y/X and Newtonian family layouts.

### New live telemetry boundary
- **BITE remains strictly the equipment DAMAGE/FAULT authority.** New `src/game/ship/telemetry/EngineLiveTelemetry.h`: separate fixed-size (max 16) authoritative engine operation DTO with validity, tick, actual red thrust fraction, available yellow steady thrust fraction, independent nozzle yaw/pitch, force vector N, consumed electrical MW, generated heat MW. `DynamicMotionState::engineTelemetry` persists the latest sample and previous nozzle angles for rate limiting. Engine life/hot annulus already uses same `mainEngineDeliveredThrust01` for runtime wear/thermal cost.
- `src/game/presentation/EngineLiveTelemetryPresentation.h` is the read-only BITE-like LIVE provider for widgets; does not infer actuator firing from net ship acceleration. `EngineThrustHudPresentation.h` now derives **RED only from actual reported per-engine force fraction**, YELLOW from sustained maximum installed-engine capability. BITE supplies only fault warning; no `normalizedDemand`, no HUD recomposition from `engineAccelerationMps2`, no shared assumed 100% across all motors. For diagnostic layouts lacking detailed per-engine telemetry, HUD may use the existing allocator's own actual delivered-thrust vector; it still must not invent thrust from hull motion.
- `src/game/network/WireDataSchema.h` carries engine telemetry and actual array/count for legacy Newtonian/diagnostics, plus requested angular demand so restored control state is coherent; bump `SimulationSnapshotWireSchemaVersion` **15→16** in `WireDataCodec.h` and version checker. `WireBinaryCodec.h` now serializes/deserializes `std::array<T,N>` without any network-supplied length or heap allocation. Both server/client must build matching v16 schema. Emitter 5-cause protocol remains intact.

### Tests/source changes and required acceptance
- `tests/architecture_contracts/InstalledEngineActuatorTests.cpp` extends mounted motor tests with idle zero, pure fore/aft exclusivity, lateral response and real nozzle bend, nonzero pitch/yaw/roll reaction, TVC one-step max angular slew, no thrust from a detached motor, read-only HUD telemetry fraction and angle.
- `WireDataPlaneContractTests.cpp` adds complete actuator torque/force, nozzle orientation, MW and heat, delivered thrust and angular command roundtrip.
- New `tests/architecture_contracts/check_engine_six_axis_matrix.py`, registered in `tests/architecture_contracts/run_mingw64.sh`, statically guards engine matrices, the authority-boundary/telemetry wire, and forbids HUD net-acceleration guesses. `check_wire_data_schema.py` asserts schema version 16.
- **No native compilation/CTest/gameplay test in this GitHub-only environment yet.** Do not claim game ready or completed physics. User should fetch branch and run:
  `cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja`
  `cmake --build build/tests/architecture_contracts --target installed_engine_actuator_tests wire_data_plane_contract_tests equipment_damage_failure_pipeline_tests -j 8`
  `ctest --test-dir build/tests/architecture_contracts -R "installed_engine_actuator|wire_data_plane_contracts|equipment_damage_failure_pipeline" --output-on-failure`
  `python tests/architecture_contracts/check_engine_six_axis_matrix.py`
  `bash build_mingw64.sh`
  Then run manual Assisted-I/Y/X keypad 4/6/9/3 with motor HUD, roll/pitch/yaw manual and autopilot; test Newtonian main/RCS separately; verify no runaway, spinning, or fake force, and test network remote HUD. Save logs in `D:\\tmp`. Fix FIRST compiler/regression error before interpreting the in-game result.
- **Open:** matrix is physically simple and optimizer weights are provisional gameplay tuning; without native tests do not assert exact manual control response. Rate-limited nozzle may delay sideways acceleration. Remote/client prediction still uses a transitional local attitude model and needs explicit parity testing. Source-managed diagnostics for non-native I/Y/X are deliberately retained as separate legacy behavior. Network fixed-size 16-engine telemetry needs a bandwidth review when NPC counts scale. Further RCS per-thruster physical vector allocation and actual nozzle-mesh animation are separate tasks.

---

## 2026-10-18 — Cobretty CR-1 preset and mounted Engine execution (IMPLEMENTED SOURCE; NATIVE TESTS PENDING)

**Current branch:** `feature/propulsion-authority-matrix-2026-10-16`. Do not merge `main` before native acceptance; older audit immediately below (stating slots still own force) is HISTORICAL and is superseded by this implementation.

### Final architecture decision for this pass
- **Any number of engine models use the same generic class and physical laws.** Added `src/game/equipment/engine/Engine.h`: `game::equipment::engine::Engine` binds to `MountedEngine {EngineUnitDescriptor*, EquipmentDamageLayer*, EngineOperatingState*, moduleId, attached}` and implements `execute(EngineCommand)` -> `EngineOutput`. The output is REAL BODY FORCE (newtons), delivered healthy-rated thrust fraction, requested electrical MW, waste-heat MW, available gimbal range/rate. The common class uses `evaluateEngineUnit`, `safeEmitterThrustFraction01` and a stable per-mounted-engine controlTick irregularity function. No ship-specific constants in `Engine`; authority, component wear, temperature and emitter replacement continue to be owned by `ObjectModuleRuntime` and its independent per-mounted-engine state. This is a stateless-view engine object (references persistent module state), not a newly duplicated engine lifecycle store.
- **New Cobretty manufacturer preset:** `src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h`, factory `presets::CobrettyCR1VectorMainDrive()`. Stable `EngineUnitDescriptor` identity `elite.engine.cobretty.cr1-vector.v1`, display name **Cobretty CR-1 Vector Main Drive**, manufacturer **Cobretty Corporation**. Passport includes own 9,561,483.75 N thrust, 7° gimbal, 8°/s rate, 4850 MW rated input, eta 98.55%, three material refs (Ti-Al for BOTH housing/TVC, HfC/C for annulus; shared C/SiC radiators), 2500 K emitter geometry, ideal bulk coolant, 30k/50k engine service hours and independent 8k/12k replace-only emitter hours. `EliteCobraMk1.cpp` no longer embeds engine calculations/constants: it instantiates ONE reusable type passport and mounts equivalent instances in the four engine modules. Other ship descriptions may reuse the preset or add different presets; never copy the law.
- **Real physical force path in NORMAL mounted gameplay:** `ShipCore::updateTranslationControl` uses immutable module bindings (slot index = placement/bank ONLY) to resolve each real engine descriptor and live per-instance equipmentDamage/operating state, then passes `std::vector<MountedEngine>` to `ShipControlExecutionGateway::evaluateTranslationControl` and `ShipPhysicalActuatorPlant::evaluateMainPropulsionFeedbackTrim`. New mounted path in `evaluateTrimmedAllocation` uses `Engine::instantaneousAvailableThrustNewtons` for balancing rated forces, `Engine::execute` to sum real force vectors, torque `r×F` about ship CoM, and output per-engine delivered-thrust telemetry for actual heat/age update. No aggregate slot ratio generates force in this mounted path. A diagnostic/legacy path, where no installed modules are available, retains old slot-based allocator intentionally; diagnostic I/Y/X substitutions remain separate and cannot invent physical installed engine types.
- **Capability envelope:** `ShipCapabilityBuilder::applyMountedEngineAxialAuthority` computes per-bank available F/m from the TYPE-rated nominal force and runtime per-engine availability; `ShipCore::capabilities` applies this after CURRENT mass properties, while server/client descriptor-backed capability snapshots use the equivalent projected envelope. This removes ordinary ship's independently authored longitudinal `ShipParams` rating from being the motor-force authority. Existing `ShipParams` API remains as a compatibility projection for flight law/planner; `EngineSlotState` stays as topology/capability summary but must not drive installed motor force itself.
- **Kept stable:** No code edits to Assisted/Newtonian controllers/planners/pilot input mapping, Newtonian RCS, navigation execution, reactor, external cooling, generic hit-volume combat mapping or emitter 5-cause/replacement policy. The flight law asks for acceleration, engine objects execute the main force. Main longitudinal bank balancing with 2-iteration conservative TVC trim retained; now sums actual engine N, can handle unlike motor ratings. For old diagnostics without engine mounts use legacy physical results.

### Tests and status
- New `tests/architecture_contracts/InstalledEngineActuatorTests.cpp` + CMake executable `installed_engine_actuator_tests`, CTest `installed_engine_actuator`. Tests a genuine named Cobretty preset, shared physical actuator force/power/heat, damage/temp, slot-fraction independence, asymmetric damage compensation, differing rated-motor outputs without editing slot/ship physics.
- Existing server/client module serialization, five-cause emitter, material library and engine tests continue. Performed GitHub-source structural audit **10/10 SOURCE PASS** for the new common engine, Cobretty preset, mounting seam, allocator, gateway, pilot/backend capability and test registration. Not a compiler or game PASS.
- **NOT YET TESTED:** native g++/MSYS2 build, new CTest, full navigation/Assisted/Newtonian regression, in-game fault/bank/heat acceptance. Run on user's checkout after `git fetch` and switching to feature branch:
  `cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja`
  `cmake --build build/tests/architecture_contracts --target installed_engine_actuator_tests universe_material_engine_thermal_tests engine_unit_damage_model_tests equipment_damage_failure_pipeline_tests wire_data_plane_contract_tests`
  `ctest --test-dir build/tests/architecture_contracts -R "installed_engine_actuator|universe_material_engine_thermal|engine_unit_damage_model|equipment_damage_failure_pipeline|wire_data_plane_contracts" --output-on-failure`
  Then execute existing navigation isolation and physical-corridor matrices before any mainline merge.

### 2026-10-18 — post-integration force-envelope correction (same main-engine checkpoint)

- **Found and fixed:** when the anti-yaw physical allocator balances a bank of unequal or damaged mounted engines down to the weakest surviving engine in NEWTONS, the *physically executable* thrust limit is not the raw sum of their individual maxima. `ShipCapabilityBuilder::applyMountedEngineAxialAuthority` now advertises `survivorCount × minimumSurvivingEngineAvailableNewtons / currentMassKg` per active main bank (and preserves a surviving singleton's full force). This applies identically to server and network/client capability projections. Newtonian reverse RCS remains independent.
- `ShipPhysicalActuatorPlant::evaluateTrimmedAllocation` and per-engine thrust telemetry now normalize requested force to the **balanced** steady bank maximum after same throttleScale, rather than the optimistic raw total. Prevents **double application of derating** in damaged asymmetrical motors. Legacy diagnostic slot-only execution unchanged.
- Contract `testMountedBankCapabilityMatchesBalanceAndNetwork()` added to `EquipmentDamageFailurePipelineTests.cpp`: 50%-damaged + 100%-healthy identical engine pair must advertise **50%** of healthy bank acceleration, not 75%; opposite bank stays unchanged; network/client prediction matches server.
- `InstalledEngineActuatorTests.cpp` expanded: at balancing cap and at half that cap, force is delivered precisely (no second hidden derating). Mixed rated motors 1:1.5 still have independent actual newton ratings, anti-yaw balancing and deterministic reported working percentages.
- Subsequent **GitHub-source static cross-file audit: 12/12 CHECK OK**. No executable tests have been run in this environment; do not infer compile PASS. Run `installed_engine_actuator` + `equipment_damage_failure_pipeline` and Assisted/Newtonian in-game route/control regressions before merging `main`.

---

### Remaining architectural limitation / next work
- **Secondary acceleration** (`motion.manoeuvreAccelerationMps2`, `assistedStabilizationAccelerationMps2`) remains emitted by pre-existing manoeuvre/Assisted stabilization models; this pass migrates REAL installed **MAIN-ENGINE** force/torque only. To claim literally every ship-force source passes an engine, first design individual RCS/stabilization actuator hardware or constrain lateral Assisted forces to installed engine vectors; do not silently delete existing flight-control features or pretend those extra forces already come from main motors. Preserving healthy flight baseline is intentional until native tests.
- Diagnostic geometry is still `physicalControlSlots(I/Y/X)` (not arbitrary CAD engine placements), which is separate from the common motor TYPE. Manual/newtonian gimbal commands continue via old law; the new generic Engine output reports gimbal limits and the existing small automatic trim respects them; full TVC nozzle pose integration needs separate runtime tests. Typed engine resource/economy inventory and replace-only annulus UI remain deferred.
- Do not add parallel state/status files; update **ONLY `PROJECT_STATE.md`** after subsequent steps.

---

## 2026-10-18 — AUDIT ONLY: engine actuator vs slot architecture (NO CODE CHANGES)

User's required target: flight control must command INSTALLED ENGINE objects, not regard topology slots as engine/actuator. Engine must have one common class/functional implementation for all models; Cobra engine must be only a reusable parameter PRESET. Do not confuse a slot address/topology binding with a physically independent engine.

**Current verified source path:** `ShipCore::refreshRuntimeState()` projects `ObjectModuleRuntime` + `EngineUnitDescriptor` into `std::vector<plant::EngineSlotState>` through `ShipCapabilityBuilder::buildPhysicalControlSlotState`, and `ShipState::ShipActuatorState` retains `propulsionSlots`; `ShipCore::capabilities()` turns them into aggregate `ShipParams`. `ShipCore::updateTranslationControl()` passes `ShipParams` and `propulsionSlots` to `ShipControlExecutionGateway::evaluateTranslationControl()`, which calls `evaluateMainPropulsionFeedbackTrim(model, slotState, physics, ...)`. `ShipPhysicalActuatorPlant.cpp` computes thrust fractions/torque based on `EngineSlotState` plus fixed I/Y/X topology and generic physics, NOT by calling an autonomous per-engine actuator. `nominalThrustNewtons` exists in `EngineUnitDescriptor`, and `evaluateEngineUnit` computes `maximumThrustNewtons`, but the ordinary control-force allocator consumes aggregated `ShipParams` and slot thrust fractions rather than this per-engine newton output. The physical slot is therefore MORE than mere address: it is the current primary actuation abstraction. This **does not meet user's requested architecture**, even though current slots are derived from true installed-engine damage and allocator telemetry is fed back to `ObjectModuleRuntime::advanceEngineOperatingLife` for correct thermal/wear updates.

**Current model organization:** No independent, state-owning generic `Engine` class has been found in inspected control path. Reusable, header-only functional `EngineUnitDescriptor`, `EngineUnitEffectiveState`, `EngineOperatingState`, `EngineEmitterOperatingState` and evaluation functions live in `src/game/equipment/engine/EngineUnit.h`, with operating data embedded in `ObjectModuleState.engineOperating`. In `EliteCobraMk1.cpp`, the Cobra's engine numerical passport is constructed INLINE in the ship descriptor and copied to each engine module. It is a de facto preset but NOT a separately identified reusable engine-type preset. One common thermal/damage law already exists; avoid duplicating physics in each ship.

**Recommended migration:** extract standalone reusable `CobraEnginePreset` (descriptor data only); introduce common per-installed-engine functional/actuator API (evaluate force N, vectoring/TVC force direction, electrical MW, heat MW, damage, runtime state, replace-only emitter), bind slot to installed engine identity and geometrical attachment ONLY. Change physical allocator/gateway to issue pilot/autopilot commands through this API and sum physical force/torque. Keep Assisted and Newtonian flight-control laws unchanged; provide compatibility `ShipParams` projection and per-engine force telemetry during migration and test healthy exact equivalence, damaged asymmetric acceleration/braking/TVC, network server/client parity. NOT IMPLEMENTED. Do not claim code already uses engines directly.

**Audit status:** source inspection only, no build/game testing, no engine/control code changes in this step. Existing user-imposed `PROJECT_STATE.md` remains the sole project state document.

---

## 2026-10-18 — FINAL ENGINE-MATERIAL AND THERMAL-REPLICATION INTEGRATION CHECKPOINT (SOURCE COMPLETE, NATIVE TESTS PENDING)

**Branch:** `feature/propulsion-authority-matrix-2026-10-16`. Code changes are committed to THIS feature branch, NOT merged into `main`. This section is authoritative over older historical notes below.

### Agreed engine/material boundaries
- One and the same fictional **Talon Ti-Al HT titanium-aluminide-based heat-resistant engine alloy** for BOTH the thrust housing and the TVC/nozzle gimbal mechanism. Stable catalog key: `elite.material.engine.tial-ht.v1`. Universal `MaterialPassport` includes stable mesh/commodity IDs, density 4550 kg/m³, cp 720 J/kg/K, conductivity 17 W/m/K, emissivity 0.68, laser absorption 0.43; nominal 1200 K, onset of weakening 1450 K, destruction 1850 K, gameplay laser/kinetic surface failure fluence 9/9 MJ/m². **FICTIONAL prototype/game balancing, not real Ti alloy certification.**
- Catalog now has THREE material passports: C/SiC radiator, HfC/C replace-only emitter, shared Ti-Al engine housing/TVC. `EngineUnitDescriptor::structureMaterialId` is the one canonical material for both housing and TVC, authored once for each Cobra main engine via `EliteCobraMk1.cpp`. Existing mechanical damage-to-thrust/TVC functions are deliberately NOT rewritten; universal catalog is identity/physical-property authority for future hit conversion.
- Cobra nominal per-engine **consumed** electric input still **4850 MW** (active engine pair 9700 MW), nominal useful-field budget 4779.675 MW, generated waste heat 70.325 MW per active full-power engine at eta 98.55%. User previously supplied 4790 MW input in a separate table; do not silently replace passport rating or retune thermal budget until resolved.
- Per installed engine still has independent: thrust-function cumulative five causes, TVC-function degradation and replacement-only hot annulus `EngineEmitterOperatingState`, with its own five causes (Laser/Impact/Wear affecting surface heat rejection; Thermal/Radiation presently inert for emitter), surface integrity, and equivalent run-hours. Annulus normal replacement recommendation 8000 equivalent full-thrust hours, exhausted by 12000, no repair, only explicit replacement. If damaged, engine warms at original thrust then protective cap gradually activates at 1250–1300 K; over-temperature accumulates main-engine Thermal damage. Reactor/shared cooling intentionally ideal in this phase.

### Closed replication/control parity gap
- `src/game/simulation/ObjectModuleSnapshot.h`: now carries `hasEngineThermalState`, housing `engineTemperatureK`, emitter intact-area, microcracks, ALL FIVE emitter damage counters, emitter equivalent operating hours. Static material geometry/passports remain local to both endpoints; older/non-engine rows default safely to untouched thermal condition.
- New single canonical, testable inline snapshot builder `src/game/simulation/ObjectModuleSnapshotBuilder.h`. Both ship and static-object graph generation in `GameSimulation.cpp` call it; removed duplicated packet packing. Unit test uses this ACTUAL producer.
- `ShipCapabilityBuilder.h::slotFromSnapshot` now reconstructs `EngineOperatingState` and calls **the same** `safeEmitterThrustFraction01` as `slotFromModule`. Thus remote/server physically available thrust and acceleration should match for healthy, damaged, hot, and replaced emitters, without any change to Assisted or Newtonian control laws.
- `WireDataSchema.h` serializes the new thermal/annulus fields. `SimulationSnapshotWireSchemaVersion` bumped **14 -> 15** in `WireDataCodec.h`; static schema checker updated. This intentionally prevents old-version decoders from interpreting a new layout; full client and server must be built from matching revision.
- **Important publication fix**: prior ship graph publication only sent module rows on initial/structural graph changes, so thermal changes would have remained stale remotely. `GameSimulation.h/.cpp` now tracks `m_lastPublishedShipModuleRevision` for each detailed ship. When that revision changes, publishes **module rows only**, not heavy structural links, assembly modules, or debug hit geometry, and retains existing initial/structural redundancy. Resets revision cache when demoted. Uses const moduleRuntime inspections to avoid an unintentional `modules()` mutable-access revision increment. Compatible with runtime 1%-point temperature/thrust governor and wear revision batching.
- No direct changes to pilot/autopilot commands, vehicle stabilization, Assisted/Newtonian law stacks or allocator internals; only available physical engine capability can differ on damage/overheat. Healthy baseline should remain the same. **Runtime behavior not yet verified in game.**

### Tests/verification
- Extended `UniverseMaterialEngineThermalTests.cpp` to check the 3-material catalog and titanium passport limits, and retained emitter radiation/wear/thermal tests.
- Extended `EquipmentDamageFailurePipelineTests.cpp` with `testLiveAndReplicatedEngineCapabilityRemainIdentical`, exercising healthy, laser-hit but cool, damaged/hot, replacement, and passive radiation/thermal emitter counters using canonical snapshot producer and server/client control projection.
- Extended `WireDataPlaneContractTests.cpp` to encode/decode a full object-module emitter state with all five causes + thermal readings + lifetime.
- Extended `check_replication_snapshot_boundary.py` with guards for module revision-triggered sparse publication and the canonical emitter DTO. `check_wire_data_schema.py` now asserts simulation schema v15.
- Conducted **GitHub-source cross-file static sanity audit: 10/10 PASS** on 2026-10-18, validating stable IDs, material assignment, DTO producer, ordered schema, consumer, publication seam, schema version and test inclusion.
- **NO native MSYS2/Ninja compilation, CTest or in-game Assisted/Newtonian acceptance executed in this environment; do NOT claim test PASS or merge readiness.** Native suggested commands from repo root:
  `cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja`
  `cmake --build build/tests/architecture_contracts --target universe_material_engine_thermal_tests engine_unit_damage_model_tests equipment_damage_failure_pipeline_tests wire_data_plane_contract_tests`
  `ctest --test-dir build/tests/architecture_contracts -R "universe_material_engine_thermal|engine_unit_damage_model|equipment_damage_failure_pipeline|wire_data_plane_contracts" --output-on-failure`
  Run separate `tests/architecture_contracts/check_replication_snapshot_boundary.py`, `check_replication_static_definitions.py`, `check_wire_data_schema.py` or entire architecture regression, then real Assisted/Newtonian game control tests. User's Windows MSYS2 g++, CMake and Ninja setup is the expected runtime environment.

### Explicitly deferred—not silently “complete”
- Material properties are now in catalog, but live weapon damage `DamageEvent.energy` lacks converted MJ units/precise annulus hit-volume identity; do not connect live hits until the user's separate damage-mechanism phase. Live in-game annulus replacement UI/economy not implemented yet.
- Physics budget and service curves are prototype gameplay values pending balance acceptance; no claim of measured real Ti-Al material capability. Shared coolant/reactor are still ideal; the unrelated old cooling parameter mismatch remains deferred.
- Engine type/group tests, native runtime and network full roundtrip need proof; until then retain current feature branch, `main` unchanged.

---

## 2026-10-18 — hot-annulus replace-only lifetime and dramatic but cheap engine overheating

- **Scope:** engine internals only; reactor/external cooling stay ideal; no new weapon hit volume or combat routing. Standard five damage channels for all equipment: Laser, Impact, Thermal, Radiation, Wear. Hot emitter has its OWN five-channel history; only Laser/Impact (lost emitting area) and Wear (age-dependent surface-efficiency reduction) affect routine emitter performance; thermal/radiation channels are retained but have zero ordinary functional influence.
- **Third autonomous engine component:** `EngineEmitterOperatingState` nested under each installed `EngineOperatingState`, with `surface`, `damage`, and `equivalentOperatingHours`. `EngineThermalEmitterDescriptor::ServicePolicy::ReplaceOnly`. Main thrust/TVC damage and service history stay independent. Generic `restoreSubtree` / `restoreAncestors` deliberately do not repair the hot emitter. Only `ObjectModuleRuntime::replaceEngineEmitter` replaces its surface and clears its five causes/hours while preserving engine age/temperature/damage.
- **Provisional lifecycle:** recommend replacement at 8000 equivalent full-thrust hours (25% emissive wear); completely exhausted at 12000 hours (100%). Linear wear in 0..8k, steeper linear wear in 8k..12k. At idle no emitter life spent. Damage factors combine cheaply: `effectiveArea01 = intactImpactLaserArea01 * (1 - wear01)`; no double count of Laser/Impact cause accumulation.
- **Engine thermal coupling:** existing rated annulus radiation multiplied by that effective area; unrejected heat enters the existing engine bulk temperature step (ideal fixed 1050 K coolant). When a damaged engine is cool it can temporarily produce 100% main thrust. A simple 50 K protection band starts at 1250 K and reaches the steady safe thermal cap at 1300 K. Result: damaged emitter heats the engine before protective gradual derating. Sustained over-limit bulk heat still contributes to engine's own Thermal damage through existing law. No expensive spatial heat solver, no reactor work, no rewritten control law. Runtime invalidates available propulsion only on 1%-point thermal-cap/wear buckets or committed engine damage, avoiding each-tick snapshot churn.
- **Files changed:** `src/game/equipment/engine/EngineUnit.h`, `src/world/modules/ObjectModuleRuntime.h/.cpp`; expanded `UniverseMaterialEngineThermalTests.cpp` and `EquipmentDamageFailurePipelineTests.cpp`.
- **Authority/tests:** source changes pushed to `feature/propulsion-authority-matrix-2026-10-16`; no local game build/CTest run in this GitHub-only tool context, so NOT PASS. Keep `main` untouched until native tests and Assisted/Newtonian regressions pass. Cobra nominal input remains 4850 MW per engine, NOT unverified 4790 MW from alternative table. Previous 2026-10-18 entries are historical and predate this lifecycle.
- **Next:** run CMake/Ninja `universe_material_engine_thermal`, `engine_unit_damage_model`, `equipment_damage_failure_pipeline`; validate cooling/wear on live Cobra; test replacement persistence and snapshot parity as separate phase; do not start weapon-hit geometry without explicit scope change.

---

## 2026-10-18 — universal materials and independent engine-annulus damage: IMPLEMENTED SOURCE, NOT YET NATIVE-ACCEPTED

**Current branch:** `feature/propulsion-authority-matrix-2026-10-16`. This checkpoint supersedes the earlier 2026-10-18 audit's statement that materials and emitter damage state were absent. Last verified implementation changes through `17b1ee6d`; this document update will be one subsequent commit.

### Branch consolidation decision

- Compared `main` to this feature branch using GitHub Compare on 2026-10-18: **1096 ahead, 0 behind** at inspected head. `main` is ancestor; FF-only consolidation is possible without merge conflicts.
- **NOT MERGED YET.** The current feature includes substantial Assisted/Newtonian/nav/game changes in addition to today's engine code. First run native MSYS2/Ninja build, engine/material tests, equipment integration, Assisted isolation regression, Newtonian corridor/runtime acceptance. Do not claim FF merge is safe as a release without those results. After green gates, fast-forward `main` to accepted feature HEAD, verify, then retire obsolete work branches deliberately (never force-push).
- Continue working on current feature branch until that gate. The proposal to consolidate branches is agreed as a goal, NOT an already completed merge.

### Universal material library (source integrated)

- New `src/game/physics/UniverseMaterials.h` with canonical `UniverseMaterialCatalog`, two stable material IDs, human names, renderer `meshMaterialId`, future economy `tradeCommodityId`, bulk density/heat capacity/conductivity, emissivity/laser absorptivity, nominal service/weakening/structural destruction temperature, surface laser/kinetic failure fluences, thermal shock limit and area fragmentation factor. These are **fictional design prototypes rooted in carbide-composite behavior**, not measured specifications for manufactured real alloys.
- `elite.material.radiator.c-sic.v1` = **Aster C/SiC radiator laminate**. Nominal 1050 K, weakening 1300 K, structural failure 1900 K, epsilon 0.90, rho 2650 kg/m3, cp 1000 J/kg/K, conductivity 25 W/m/K, laser threshold 4 MJ/m2, impact 2.5 MJ/m2, shock 350 K/s. Mesh `mat_radiator_aster_c_sic`, commerce `commodity_c_sic_radiator_laminate`.
- `elite.material.engine.hfc-c.v1` = **Vanta HfC/C reinforced emitter ceramic**. Nominal 2500 K, weakening 2850 K, structural failure 3800 K, epsilon 0.90, rho 10500 kg/m3, cp 550 J/kg/K, conductivity 35 W/m/K, laser threshold 12 MJ/m2, impact 6 MJ/m2, shock 500 K/s. Mesh `mat_engine_vanta_hfc_c`, commerce `commodity_hfc_c_emitter_ceramic`. Structural failure 3800 K is BELOW the melting point of pure HfC: it is a composite failure threshold, not a melting-point claim.
- New `SurfaceExposure` explicitly requires **incident MJ** and illuminated/contact footprint **m2**; `exposeSurface` uses absorbed laser energy, kinetic energy, thermal softening, shock rate, localized area fragmentation, persistent `SurfaceIntegrity{intactArea01,microcrack01}`. Damage does not automatically spread across the entire radiator/annulus from one microscopic hit. Spatial heat spots and fracture propagation are future extensions. Raw `DamageEvent.energy` currently has **unspecified units** and MUST NOT be passed into this MJ interface without explicit conversion.

### Engine: THREE distinct components, all mounted-module-owned

1. Primary field-drive thrust: existing five-cause equipment damage, efficiency and lifetime remain independent.
2. Nozzle TVC: existing gimbal failure profile remains independent.
3. **Hot annulus**: new `EngineThermalEmitterDescriptor.materialId/meshPartId` and persistent `EngineOperatingState.emitterSurface` for each installed engine. `ObjectModuleRuntime::exposeEngineEmitter` updates just the affected module, invalidates physical capability on change and does NOT inject fictional direct thrust/TVC damage. `restoreSubtree` restores emitter area when the module is structurally repaired.
- `EngineUnit::ratedThermalEmitterPowerMW` now resolves effective emissivity from the material library when a material is selected. `advanceEngineOperatingState` uses the REAL intact annulus area for thermal radiation and engine-housing temperature/wear, without counting emitter radiation twice. Standalone legacy tests can still use the scalar emissivity when no material ID is authored.
- NEW `safeEmitterThrustFraction01`: quasi-steady safety cap using the existing fixed 1050 K **ideal coolant boundary**, engine's OWN finite bulk conductance, and 1300 K safe housing temperature. `ShipCapabilityBuilder::slotFromModule` projects the cap onto real installed actuator `thrust01`, while leaving TVC authority independent. The pilot/autopilot still actuates ordinary controls; no direct bypass of ship engines was added.
- Cobra presets: all four engines reference `elite.material.engine.hfc-c.v1` and reserved mesh-part name `engine_hot_emitter_annulus`; ship radiator `CoolingDescriptor.radiatorMaterialId` references C/SiC. `CoolingSystem` resolves the actual plate emissivity through the same catalog and attaches material ID to panel instances. Legacy `maxTransferPower = 180,0` comma typo fixed to `180.0`; reactor/output settings left untouched.

### Engine thermal performance *calculated* at healthy output, no reactor/cooling realism

- Each engine: 9.56148375 MN rated thrust, 4850 MW electric rating, 0.9855 nominal efficiency, 70.325 MW waste heat, ring 4.5m OD/2.5m aperture => 10.995574m2, surface at 2500 K => ~49.31905 MW radiation. Ideal coolant boundary 1050 K, healthy engine housing 1200 K, safe threshold 1300 K. Bulk conductance ~0.14003966 MW/K and thermal inertia 12 MJ/K => ~85.69 s thermal response time.
- Lost emitter area 0/25/50/75/100% => nominal-fraction allowable steady thrust approximately **1.000/1.000/0.8485/0.6732/0.4978**. Fully forced 100% thrust WITHOUT the protective cap would approach housing ~1200/1288/1376/1464/1552 K respectively; above 1300 K leads to existing quadratic engine thermal damage. Starting from 1200 K, the estimated 1300 K crossing time at sustained unsafe 100% thrust is ~72 s (50% surface lost), ~41 s (75%), ~29 s (100%). These are analytic estimates, NOT a simulated live burn.
- Ideal ship reactor and radiator transport remain intentionally **out of scope**. The engine's finite heat transport to the ideal coolant boundary is NOT out of scope; otherwise annulus destruction has no consequences. 1050 K is a fixed ENGINE coolant boundary in this provisional model, not evidence that the real current CoolingSystem can handle that temperature.

### Tests and evidence

- Added `tests/architecture_contracts/UniverseMaterialEngineThermalTests.cpp` and registered CTest `universe_material_engine_thermal`: material IDs/thresholds, laser/kinetic local fracture, complete emitter thermal failure, 25/50/100% damage-cap equations, unsafe vs protected heat evolution.
- Extended `EquipmentDamageFailurePipelineTests.cpp`: a material-backed laser MJ/area surface hit to a real installed aft engine must reduce that ONE engine's actuator thrust but not neighboring engines or its own TVC, and must not create bogus five-cause damage.
- **Tests have NOT BEEN EXECUTED** in this remote GitHub editing session; no local checkout/build/game run is available from this environment. Current GitHub commit status has no checks. Source commit is not equivalent to a green build. Required command on user Windows dev PC: `cmake -S tests/architecture_contracts -B build/tests/architecture_contracts -G Ninja` then `cmake --build build/tests/architecture_contracts --target universe_material_engine_thermal_tests equipment_damage_failure_pipeline_tests engine_unit_damage_model_tests universe_thermal_radiation_tests` then `ctest --test-dir build/tests/architecture_contracts --output-on-failure -R "(universe_material_engine_thermal|equipment_damage_failure_pipeline|engine_unit_damage_model|universe_thermal_radiation)"`. After these are green, perform full game/native and Assisted/Newtonian regressions.

### Gaps / next exact steps

1. **Live hit binding not completed:** `ShipDamageHandler / ShipCore::applyDamage` still resolves general engine HitVolume and uses raw legacy energy. There is not yet an actual separate hot annulus hit mesh/volume with stable per-engine ID, MJ energy/footprint conversion and surface-hit dispatch into `ObjectModuleRuntime::exposeEngineEmitter`. Never claim laser fire in the live game already chips the annulus. Define corresponding mesh/hit part with actual Cobra geometry and a typed attack-energy adapter before activating this path.
2. **Replication not completed:** `ObjectModuleSnapshot` currently serializes five damage causes but NOT emitter intact area/microcracks or effective thrust ceiling. Authoritative module-runtime slot respects damage; client snapshot/prediction may disagree until DTO, serialization and compatibility tests are updated. Do not alter Assisted/Newtonian control laws as a workaround.
3. **Plate damage not integrated:** `CoolingSystem` now reads material identity/emissivity, but plate hit events still use their legacy health/efficiency (not material fluence/shock). Shipwide radiator thermal budget and reactor intentionally remain deferred under current ideal assumption. An existing `ShipCore::init` unit mismatch sends `maxTransferPower` as `maxCoolantTemp`; not addressed here because shared cooling is currently out of scope. Fix before integrating nonideal coolant.
4. Native build/test and live acceptance are pending. Check historical test fixtures and all CMake targets for C++17 compilation. Do not merge to `main` before gates.
5. Subsequent internal engine work: true transient hot-annulus temperature and thermal-shock response, isolated subsystem diagnostics/HUD, independently locatable hit meshes, and replacement/repair semantics. Only then tune catastrophic effects/thresholds against representative laser and kinetic weapon energy in physical units; present values are provisional.

**Stopped at:** New material library + thermal emitter third component + protective thermal thrust derating are pushed to the feature branch and test sources authored. Native tests and actual in-game laser/kinetic hit routing remain unverified/incomplete. Keep `PROJECT_STATE.md` as the SINGLE canonical state document and update on every further meaningful step; no competing state file is created.

---

## 2026-10-18 — source-audited thermal material / Cobra engine checkpoint

**Audit scope and provenance.** Reviewed GitHub branch `feature/propulsion-authority-matrix-2026-10-16` at source HEAD `08a1ea9e6b8ccfb8bd445616226abd3a5753daad`; default `main` is older (2026-10-07), so never treat its source as current engine implementation. No code build, local game launch, or test execution was possible in this audit. This entry records verified source and outstanding requirements, not a PASS claim. Continue all implementation work on the agreed feature branch; do not edit `main`.

**Implemented in source (verified by reading current branch).**
- Reusable generic *descriptor/evaluator* `src/game/equipment/engine/EngineUnit.h`: `EngineUnitDescriptor`, `EngineThermalEmitterDescriptor`, `EngineOperatingState`, thrust/TVC authority, five-cause damage, field-drive electrical/heat passport and analytical hot-emitter/engine-housing heat step. This is a reusable type/prototype, not a C++ inherited `Engine` class.
- Cobra-specific instantiation in `src/game/ship/descriptors/EliteCobraMk1.cpp` (~lines 1030–1143): 4 distinct mounted engine modules (two aft/two fore) mapped to the same engine *type preset*, each with independent module identity and operating/damage state. One active pair generates 7.5 g for a 260 t hull. Per unit 9.56148375 MN, 4850 MW electrical rating, efficiency 0.9855, 70.325 MW waste heat; two active units 140.65 MW waste heat. Design annulus OD 4.5 m / aperture 2.5 m, emissivity 0.9, view 0.9 and 2500 K. Forward-versus-aft nozzle art geometry remains unverified.
- Universe-wide radiation maths in `src/game/physics/UniverseThermalRadiation.h` owns shared Stefan-Boltzmann law and fictional *universal* multiplier 2.5 (exactly once), deep space 2.725 K, aperture area and surface emissivity clamp 0..1. `CoolingSystem.cpp` uses this function for generic radiators; Cobra authors 380 m² / 40 panels, emissivity 0.9. At a **hypothetical surface temperature** 1050 K, this computes ~58.92981 MW for ship radiators. Each healthy annulus hypothetically rejects ~49.31905 MW; two annuli + panels ~157.568 MW, ~10.74% capacity margin against main engines alone. These are design-point equations, NOT a proved integrated ship heat balance.
- `ObjectModuleRuntime::advanceEngineOperatingLife` and `ShipCore::updateSystems` wire individual age/thermal wear to real modules and measured delivered actuator thrust. Normal engine thermal integration uses an **ideal fixed coolant boundary**, not real returned `CoolingSystem` coolant heat.
- `tests/architecture_contracts/UniverseThermalRadiationTests.cpp` and `EngineUnitDamageModelTests.cpp` contain radiation/annulus and explicit 0%-area hypothetical failure checks. Test existence is not execution evidence.

**Missing / incomplete compared with current user requirements.**
1. NO two authored universal materials (distinct shared-radiator plate and engine hot-annulus): no independent material `id`, display/physical name, mesh material or render asset ID, commercial/trade commodity ID, material emissivity reference, nominal rated service temperature, progressive weakening onset, irreversible destruction temperature, impact energy/stress/thermal-shock failure law. The scalar surface temperature currently belongs to the Cobra engine emitter, not a material passport; `safeOperatingTemperatureK=1300` is **engine housing thermal-wear onset**, NOT annulus destruction temperature.
2. `RadiatorPanel` in `CoolingSystem.h` contains only area, efficiency, health. `EngineThermalEmitterDescriptor` contains only geometry/temperature/emissivity/view factor. No separately damageable physical emitter surface, hit volume, structural material state or concrete mesh/trade identity. Engine mesh IDs presently identify main engine modules, not an individual hot emitter material.
3. `EngineUnit::advanceEngineOperatingState` accepts `emitterEffectiveArea01` and can model emitter removal in a unit calculation, BUT `ObjectModuleRuntime::advanceEngineOperatingLife` calls it without that argument, so runtime always defaults to **1.0**. A hit cannot yet reduce real engine emissive area. No laser energy/type -> emitter structural damage -> actual thermal derating -> coolant overload end-to-end pipeline.
4. Engine excess heat (`EngineLifeStep.coolantHeatMW`) is **not deposited into** the ship's `ThermalSystem` or budgeted by common `CoolingSystem`. The current calculated ~10.74% margin excludes all other ship heat, pump capacity/temperature effects, angular shadowing beyond the fixed view factor, and transient coolant response.
5. Discovered an existing cooling descriptor/interface mismatch: `ShipCore.cpp` calls `m_cooling.init(..., descriptor.cooling.maxTransferPower)` as argument 4, while `CoolingSystem::init` interprets argument 4 as `maxCoolantTemp` (K); Cobra sources `maxTransferPower = 180,0` (comma expression, not proper `180.0`). Audit/fix unit and field semantics before connecting integrated thermal physics; do not silently alter the deferred reactor design. `desc.thermal.initialTempK=1000` also has a stale 20°C comment; separate actual running coolant temperature from design 1050 K radiator point.
6. Repo still contains historical `CURRENT_STATE.md`, `CURRENT_TASK.md`, `CONTINUE_PROMPT.md` and older project-state archive. These are **not** to be updated as canonical cross-chat context. Keep only `PROJECT_STATE.md` maintained going forward; audit/archive stale competing entrypoints deliberately, without deleting historical files automatically.

**Architecture decisions / next concrete steps.**
1. Implement a *ship-independent* thermal-material catalog/schema outside the Stefan-Boltzmann law, with TWO independently named and stable-ID material records: ship general radiator **plate** and engine **hot annulus**. Required fields: `materialId` (canonical physical/catalog identity), `displayName`, `meshMaterialId` (presentation), `tradeCommodityId` (future economy), `emissivity01`, `nominalOperatingTemperatureK`, `damageOnsetTemperatureK`, `destructionTemperatureK`, heat capacity/thermal-shock behavior and impact failure specification with physical units. Assign actual values/fictional alloy names only after agreeing material properties; don't mislabel engine housing 1300 K as hot annulus destruction temperature. Shared physics law consumes material properties without hardcoding Cobra.
2. Refactor `EngineThermalEmitterDescriptor` and ship radiator panel presets to reference those material IDs while keeping geometry and installation temperature owned by device and ship. Keep distinct instance IDs for all four Cobra engines and radiator surfaces, and preserve association to art/mesh and economy IDs.
3. Add separate actual hit volumes and material/surface damage for panel vs annulus; map actual laser energy, impact geometry and material resistance to structural loss, effective area and shock failure through existing real EquipmentDamage pathway. No fake forced failure slots and no arbitrary accepted hit counts.
4. Wire area degradation into `ObjectModuleRuntime`, engine heat rejection and normal flight derating; transfer residual MW to the common heat network and validate conservation/cooling failures without silently treating reactor ratings as implemented.
5. Fix the discovered cooling configuration unit mismatch, compile engine/radiation/actuator tests on Windows MSYS2, run actual live Cobra damage scenarios, record dated PASS/FAIL evidence in THIS file after running them.

**Stopped at:** Source audit and updated state file only; the two material definitions and actual combat/heat-network integration are NOT implemented or approved numerically.

---

## 2026-10-18 — canonical multi-dialogue checkpoint and mandatory state-file workflow

### State-file rule (mandatory for all future Elite work)
- There is exactly ONE permanent cross-chat state document: repo-root `PROJECT_STATE.md`. NEVER create competing PROJECT_STATUS.md, STATE.md, notes/status/plan mirrors or second canonical context records.
- At the end of EVERY substantive Elite conversation or material milestone, refresh THIS file on the CURRENT feature branch through GitHub. This also applies when the discussion only makes an architectural decision and no source code changes.
- Capture current work location/branch, what is IMPLEMENTED (and verified or unverified), what is merely APPROVED or PROPOSED, numerical parameters with units and assumptions, superseded alternatives, known gaps, regressions, and next concrete steps. Avoid claiming tests passed without actual execution output.
- Newest dated checkpoint is authoritative; older dated entries are retained for history and may have stale/conflicting numbers. Before beginning new work read this section and the current newest dated checkpoints, then verify against HEAD/source. Do not copy obsolete numbers into code from older history.
- Do not change `main`; work only on the specifically agreed feature branch. For damage tests use the actual `EquipmentDamage -> EngineUnit -> real physics` path, not debug slot overrides. Unit actuator mathematics tests are distinct from end-to-end damage acceptance.
- User expressly requested this permanent workflow in chat on 2026-10-18.

### Consolidated latest decisions — explicit precedence over older history

**Scope/architecture**
1. Universal component/physics laws must be ship-independent. Cobra is only one ship preset. The frame may change with future ship construction; it retains/accelerates mounted engines as part of one rigid body until supports fail. A detached engine is its own physical object. Do not anchor engine physics to Cobra-specific frame assumptions.
2. Only engines are under active development. There is ONE future reactor serving ship systems; reactor fuel (including later jump demands) may be modeled later, but propulsion carries **no reaction mass**. Reactor and ship cooling are ideal/unlimited **for now**, and the reactor's actual power-rating redesign is explicitly deferred. Do not tune the legacy reactor while working on the engine.
3. Ordinary longitudinal Assisted-I thrust uses either aft or fore pair, never both simultaneously; each of Cobra's four engines has its own installed module ID, own lifetime, thermal state and source-damage history. The physical actuator allocator, not throttle-key input, supplies actual delivered force for the engine-specific aging calculation.
4. Propulsion is hypothetical `FieldCoupling` with no ejected reaction mass. Don't present real-world `F * 500 m/s` as a fixed energy demand of an accelerating thruster. The engine authors its own electrical rating; thrust is force, not constant-speed maintenance power. Reactor capacity should later be derived from complete ship demand rather than adding four local reactors.

**Authoritative current Cobra engine/cooling preset (as implemented at latest branch HEAD, pre-build validation)**
- Approximate hull size: width 30 m, length 25 m, height 6 m. Engine/nozzle size estimates are provisional and need CAD verification.
- Mass 260 t. Acceleration 7.5 g. Each healthy main engine 9.56148375 MN; active 2-engine bank 19.1229675 MN. All four motors cannot be counted as firing at full opposing longitudinal thrust during ordinary forward/reverse control.
- Each engine independent nominal electric rating **4850 MW**; one active pair **9700 MW**. Nominal efficiency **98.55%** (`0.9855`), thermal output **70.325 MW PER engine / 140.65 MW active pair**. No enforceable current `PowerBus` or reactor supply limit; legacy reactor passport 2500/3200 MW and actual `ReactorSystem` default 70 MW are not sufficient and remain untouched by agreement.
- Universe-wide thermal-radiation law in `src/game/physics/UniverseThermalRadiation.h`: `RadiationEffectiveness=2.5`, Stefan-Boltzmann constant, background 2.725 K, common `radiatedPowerMW`, annular `annulusAreaM2`. Multiplier 2.5 is a deliberately **fictional universal gameplay modifier** for every radiating surface, including ship radiators and engine rings; it MUST be applied exactly once in the shared physics law. Material emissivity is a separate physically bounded 0..1 parameter. `CoolingSystem` is now linked to this shared law, not a private duplicate `RADIATOR_EFFECTIVENESS`.
- Cobra main radiator plates: current modeled effective radiating area **380 m²**, material emissivity **0.9**, design surface temperature **1050 K**, capacity approx **58.92981 MW** with universal ×2.5. Whether they can structurally fit the ship and their actual operational temperatures are CAD/thermal integration concerns.
- Cobra ENGINE thermal emitter is a **hot nozzle annulus ("пятак")**, not a solid 3 m² disk. Provisional external diameter 4.5 m and exhaust aperture 2.5 m => one exposed annulus area **10.995574 m²**, T=**2500 K**, emissivity 0.9, open-sky view factor 0.9. Rated direct radiation approx **49.31905 MW per engine** with shared ×2.5. Geometry, material, physical model/mesh ID, and later trade catalog ID are **engine-type-specific**, not universe constants. Need actual geometry for fore engines when art/mesh authoring occurs.
- Heat balance at full output per healthy active pair: ~98.63810 MW radiated directly by the two hot annuli; ~42.01190 MW transferred to ship coolant; ~58.92981 MW ship panel radiative capacity. Total radiator+annulus capacity **157.56791 MW** vs produced **140.65 MW**; **10.74% theoretical reserve** relative to combined radiator capacity. Reserve presently counts main engine heat only: NO reactor, weapons, avionics, cargo or other systems, and no actual shared heat-balance integration yet. Occlusion assumed via the authored 0.9 ring view factor.
- Healthy engine housing ~**1200 K** at full thrust, normal/ideal coolant boundary ~**1050 K**, thermal-damage threshold **1300 K**, hot annulus ~**2500 K** (a distinct high-temperature component). Engine housing heat capacity **12 MJ/K**. Its thermal conductance is derived from the **remaining 21.00595 MW per engine** into coolant divided by 150 K. Never subtract direct emitter radiation twice or assume housing temperature equals hot annulus temperature.
- Independent lifetime: **30,000 equivalent full-thrust hours** until overhaul marker = 20% wear; **50,000 equivalent hours** unrepaired = 95% wear functional failure. Equivalent aging `dt/3600 * deliveredHealthyNominalThrustFraction^1.5`. Engine thermal damage: 5% per hour at 100 K above 1300 K, quadratic with excess temperature. Thermal and wear histories are stored in installed `ObjectModuleRuntime` per-engine state; accumulated percentages commit in 1e-5 quanta to avoid per-frame revision storms.
- Five cumulative functional-damage causes **Laser, Impact, Thermal, Radiation, Wear**, each in normalized damage percent at the point of entry; `D_total = clamp(L+I+T+R+W, 0,1)` with NO later per-cause weighting for engines. The *different physical damage inputs* determine how fast those channels grow. Keep histories separate for damage diagnostics and repair. Smooth lost thrust until ~70% where irregularity starts; nozzle/TVC fails at 90%; thrust function fails at 95%, without falsely detaching the physical module. Decline in maximum thrust does NOT imply identical loss of electric conversion efficiency: Cobra uses gentle separate `maxRelativeEfficiencyLossAtFullDamage01=0.002`.
- External background radiation and future reactor leakage need absorbed-dose -> radiation-damage conversion, even on inactive engines; radiation dose accumulation is not yet connected in game runtime.

**Combat / vulnerable heat emitter — not yet integrated**
- Key required mechanic: a hot radiating annulus is a *real damageable part* of an engine, with separate structural/material state, real laser hit volume and energy/armor/penetration interaction. Damage reduces available radiative emitting area or effective heat transfer; **that does not simply set `slot.operational=false` or directly assign 100% main-engine damage**. Heat otherwise rejected by the emitter goes into the engine housing and shared cooling circuit, eventually causing throttling and, if abused, irreversible `Thermal` accumulated damage.
- Desired gameplay: tactical laser fights should disable propulsion rapidly enough to matter. A destroyed engine hot-emitter may force early thermal derating of the damaged motor, thereby reducing balanced bank thrust and slowing the ship. Thermal safety/limiting must precede slow long-term overheat accumulation; overheat/temporary emergency power is bounded by actual thermal inertia and component/material destruction limits.
- Determine hitpoint/laser power budget **from actual laser type, per-shot energy, absorbed heat, armor, hit geometry and material** so different weapons require different realistic-but-playable numbers of hits to destroy hot emitters or general radiator panels. No numeric laser energy or hit counts have been accepted yet; do not fabricate acceptance values.
- Distinguish ship common cooling RADIATOR PANELS from per-engine hot emitters; keep their own material properties, working temperatures, damage temperatures, impact/shock failure behavior, identifiers for future mesh/commerce. Actual material choices/IDs are NOT finally authored; this is outstanding work.
- Derating and transfer of actual engine waste heat to the shared `CoolingSystem` / overload response are NOT integrated end-to-end yet. The last code pass added shared radiation math, provisional ring descriptor and thermal equation; real laser hit volumes, emitter surface damage storage/wiring, type-dependent weapon damage and normal-control thermal throttling remain future work.
- Last common physics refactor and tests are committed in current feature branch, but **no build or test execution has been confirmed** by the assistant in this working step. Run targeted architecture/engine/actuator tests and the game build on user's MSYS2 checkout before declaring green, then exercise real combat cause path in actual game rather than fictitious slot-off scenarios.

**Superseded proposals (do NOT resurrect)**
- 80% engine efficiency and 500 m/s F*v design-point power; 99.8% efficiency with ~19.16 MW per active pair; 99.0% efficiency with 3 m² emitter at 2500 K; `emissivity=1.2` and 78.57 MW shared radiator capacity. These are discussion/earlier implementation states, not the current preset.
- Old weighted damage coefficients for individual engine damage causes, fake `propulsion-matrix`/slot-failure tests, and fully independent per-engine reactors are rejected.
- A hard material `emissivity>1` is not permitted; gameplay multiplier `2.5` belongs exclusively to the universal physics law.

### Immediate continuation
1. Confirm current branch HEAD and compile the shared-law + engine-heat tests; resolve errors without relaxing their behavioral requirements. Keep `main` untouched.
2. Author universal material/thermal-surface schema (plate vs nozzle ring), surface identity/mesh/trade IDs, and actual laser-energy -> emitter structural damage routing through `EquipmentDamage`/real hit volumes.
3. Integrate physically bounded per-engine thermal derating and shared cooling load; verify in 4-engine Cobra that striking one ring causes prompt but not imaginary propulsion loss and thermally stresses the common cooling without inventing missing energy.
4. Schedule reactor rating/fuel and full ship heat ecosystem only AFTER engine mechanics are accepted.

---
## 2026-10-18 — universal universe thermal-radiation law and Cobra ring emitters

Current accepted scope: physics-law refactor + provisional Cobra emitter sizing/energy passport. Do not touch reactor design, structural hull or artificial damage tests. Reactor/cooling service remain ideal for engine power authorization; live reactor capacity remains legacy and is insufficient to power the engine bank until reactor is explicitly redesigned.

A new header-only universe law is at src/game/physics/UniverseThermalRadiation.h (game::physics::thermal). One and only one RadiationEffectiveness = 2.5 gameplay multiplier applies to ALL thermal emitting surfaces, regardless of whether ship cooling panels or engine-mounted rings. Stefan-Boltzmann constant 5.670374419e-8 W/m2/K4, deep-space background 2.725 K, universal radiatedPowerMW(area, T, emissivity, viewFactor, background), annulusAreaM2(outerDiameter, apertureDiameter). Material emissivity strictly 0..1; the x2.5 multiplier is a fictional universal game law, not physical emissivity. Radiator dimensions and temperatures are device/ship properties, not hardcoded global constants.

CoolingSystem.cpp now calls this shared law rather than keeping its own local STEFAN_BOLTZMANN, SPACE_TEMP, RADIATOR_EFFECTIVENESS. Cobra ship-panel emissivity corrected from nonphysical 1.2 to 0.9, still 380 m2 nominal effective radiating area and 1050 K DESIGN coolant boundary. At those inputs global panel capacity ~58.92981 MW (not earlier 78.57 MW which used emissivity 1.2).

EngineUnitDescriptor now contains EngineThermalEmitterDescriptor {outerDiameterM, apertureDiameterM, surfaceTemperatureK, emissivity01, viewFactor01}. Engine types, not universe, author size and material. Geometry treated as ONE radiating annular side rather than whole disk or two unverified exposed surfaces; open-view occlusion is explicit.
Approx Cobra body 30m wide x25m long x6m tall, suggested nozzle/ring external diameter 4.5m, inner aperture 2.5m, effective annular area 10.995574 m2, 2500 K, emissivity 0.9, view factor 0.9 -> emitted capacity ~49.31905 MW PER engine with universal gameplay multiplier. Provisional for all four Cobra main engines (fore nozzle geometry should be revised when actual art/CAD measured).

At 100% requested thrust only ONE longitudinal bank of 2 main engines fires:
- Cobra remains 260,000 kg and 7.5g nominal envelope; individual rated thrust 9.56148375 MN.
- Electric power per full-thrust engine is NOW a *separately authored* 4850 MW (not a derived mechanical F*v at a 500m/s reference speed, which the user rightly rejected for a thruster that accelerates).
- Engine nominal efficiency 0.9855 (98.55%); engine waste 70.325 MW, active pair 140.65 MW waste, 9700 MW electric.
- Healthy engine ring rejects ~49.31905 MW; ~21.00595 MW remaining load flows via engine housing into ideal common coolant.
- Full active pair + ship radiators can reject ~157.56791 MW; headroom (157.56791 - 140.65) / 157.56791 = ~10.74% for MAIN-ENGINE heat only. Does not include other equipment, reactor thermal output or directional shadowing beyond authored 0.9 view factor.
- Engine housing steady full-burn temp remains 1200K above ideal 1050K coolant. Thermal conductance set from remaining ~21.006 MW /150K rather than all engine heat. At emitterEffectiveArea01=0 and full burn, direct hot emission drops to zero, coolant demand and engine temperature rise through real EngineLifeStep thermal integration (engine damage then feeds accumulated Thermal cause). The common cooling system has not been wired to receive this engine heat yet; surface damage is not yet wired to actual laser hit volumes.
- No reaction mass, no reactor fuel consumption, no engine electric load on PowerBus yet. Independent rated power is an explicit field-drive gameplay specification, not a claim of physically verified reactionless propulsion.

New acceptance target tests/architecture_contracts/UniverseThermalRadiationTests.cpp (universe multiplier, physical emissivity clamp, ring aperture geometry, shared/pair cooling and 10%-class design reserve), added to architecture CMake; existing EngineUnitDamageModelTests extended to exercise actual engine ring cooling and emitter-loss overheat; EquipmentDamageFailurePipelineTests power fixture no longer depends on F*v.

NOTE: All writes are only in feature/propulsion-authority-matrix-2026-10-16. These commits have NOT been compiled or executed by assistant; user Windows build/ctest results required. Next stage, if user authorizes: actual hit volumes/material resistance for independent emitter surface, laser type/energy dependent damage, thermal feedback throttling and shared coolant network. No fake slot tests.

## 2026-10-18 — engine thermal/wear operating life and realistic Cobra radiator budget

Approved scope: universal engine mechanics only. Frames remain structural attachment / impulse paths. Reactor and cooling remain ideal, no functional failure, shortages or fuel flow; main drives are fictional field-coupling, without expelled reaction mass. Ordinary Assisted-I propulsion allocates exactly ONE bank of two engines per forward/reverse burn; front and rear banks are not burned simultaneously for ordinary thrust.

DESIGN-POINT numbers for Cobra at 260,000 kg, 73.549875 m/s²:
- 9,561,483.75 N of rated thrust for each of four engines.
- Only active 2-engine bank supplies 19,122,967.5 N at full longitudinal acceleration.
- 500 m/s design speed, η_nom = 0.998. Electric input = F * 500 / η at that reference point, not a claimed physical propellantless energy mechanism.
- Per engine electric 4,790.322520 MW, waste heat 9.580645 MW. Active pair input 9,580.645040 MW, waste heat 19.161290 MW.
- Physical heat rejection comparison: 380 m² radiators at emissivity 0.9 and 1050 K emit ~23.572 MW according to Stefan–Boltzmann, leaving ~4.41 MW radiator margin for the main active pair (not counting any future other ship heat loads). Existing CoolingSystem is NOT modified or claimed to actually achieve this yet; its legacy emissivity/temperature internals remain separate.
- This high-efficiency field drive is pure science-fiction. No known reactionless drive exists. Existing Cobra descriptor reactor 2500/3200 MW is insufficient and is left unmodified because reactor supply is currently ideal and main propulsion is not connected to PowerBus.

IMPORTANT ENERGY FIX: percent loss of THRUST AUTHORITY is NOT an equal percent loss of conversion efficiency. EngineUnit keeps F_max = F_nominal * (1 - totalDamage) until 95% failure, but eta_effective = eta_nominal * (1 - 0.002*totalDamage) for Cobra, i.e. only up to 0.2% relative efficiency loss even at complete damage. Electrical input and heat use the separately degraded eta; this prevents 20% mechanical wear from spuriously creating gigawatts of heat.

Universal EngineUnitDescriptor thermal/service parameters, configured for Cobra:
- coolant design boundary 1050 K (ideal); engine steady full-load temperature 1200 K; safe thermal damage limit 1300 K.
- engine lumped thermal capacity 12 MJ/K; conductance (9.580645 MW)/(150 K) ≈ 0.063871 MW/K; temperature follows analytic exponential thermal equilibration over systems cadence.
- overheat rate: 0.05 damage (5 percentage points) per hour at 100 K ABOVE safe temperature; quadratic excess dependence; damage accumulates over time, never heals by cooling.
- overhaul at 30,000 EQUIVALENT full-thrust hours with 20% wear damage; no-overhaul functional end of life at 50,000 equivalent hours with 95% wear (below 30k linear, beyond 30k quadratic acceleration).
- operating-hour conversion: dt/3600 * (delivered thrust / healthy nominal thrust)^1.5 per engine; no wear for idle engine or inactive opposite bank.
- Per-engine state stored on installed ObjectModuleState: actual temperature, equivalent hours, pending fractional thermal and wear. Only increments >=1e-5 damage (=0.001 percentage point) committed into real EquipmentDamageLayer and module revision. No per-tick BITE/snapshot invalidation for tiny age increments.
- Per-engine delivered fraction produced by EXACT ShipPhysicalActuatorPlant physical allocator after bank selection, damage, intermittent dips, throttle balance, and commanded load; transmitted via ShipControlExecutionGateway into the ship's DynamicMotionState. ShipCore.updateSystems runs engine life for each real bound module; time update uses accumulated systems-cadence dt. No fake slots, special test override, or pilot-throttle approximation as a substitute for physical actual output.
- Unpowered/docked engines still thermally relax to coolant. Wear damage affects ShipCapability/Autopilot/BITE through the ordinary module-revision projection.
- External radiation inputs, per-module reactor leak and fuel, actual ship heat network/cooling damage, client serialization of detailed service hours/temperature, and true game-runtime impact-damage acceptance remain outside this step.

Tests revised/extended: EngineUnitDamageModelTests, EquipmentDamageFailurePipelineTests (real ObjectModuleRuntime wear and coolant overheat), ShipPhysicalActuatorPlantTests (per-bank delivered-thrust telemetry). This change has NOT been compiled/tested in the user's Windows environment; require local build and ctest, report failures before claiming PASS.

## 2026-10-18 — universal engine damage percentages and Cobra field-drive energy passport

User-approved engine law: five per-module cumulative percentages laser/impact/thermal/radiation/wear are already converted into functional loss by their source mechanisms. Effective engine loss is clamp(laser + impact + thermal + radiation + wear, 0, 1), with NO extra per-cause weights. Source histories remain separate in ObjectModuleRuntime and replicated snapshots. Generic weightedFunctionalDamage01 remains ONLY for non-engine equipment compatibility. EngineUnit no longer uses EquipmentDamageSensitivity or separately weighted gimbalDamage.

The common total damage drives output curves (descriptor thresholds):
- Axial thrust authority: 1 - total damage until functional failure at 95%.
- Irregular thrust starts at 70%, grows to failure at 95%.
- TVC authority: 1 - total damage, with distinct function failure at 90%; surviving axial thrust is not deleted when nozzle fails.
- Structural attachment and frame health remain independent from functional failure.
- BITE reports power/heat increase for same achieved fraction of original nominal useful thrust.

Cobra physical engine passport in EliteCobraMk1.cpp:
- 260,000 kg; nominal forward/reverse acceleration 73.549875 m/s^2 (7.5 g).
- Each of the 2 engines in one active longitudinal bank rated 9,561,483.75 N; bank 19,122,967.5 N.
- PropulsionPrinciple::FieldCoupling: SCI-FI assumption (no expelled reaction mass; external momentum sink unimplemented/not known physics).
- Reference point 500 m/s, nominal energy conversion efficiency 0.80.
- Electrical design demand per full-thrust engine 5,975.92734375 MW; active bank 11,951.8546875 MW.
- At reference speed nominal field work per full-thrust engine 4,780.741875 MW; nominal waste heat 1,195.18546875 MW per engine.
- Power and waste-heat queries are clamped to current damage-limited deliverable force.
- PowerBus and CoolingSystem remain IDEAL / NOT coupled to engine demands. The legacy Cobra reactor descriptor ratings of 2,500/3,200 MW cannot power these engines; no automatic reactor-rating or cooling edit was made. Fuel/resource consumption is reactor/jump ownership for a later phase, not engine propellant.
- The power rating is a DESIGN-POINT fiction and not a physically viable reactionless thrust claim. Photon propulsion would require orders of magnitude more power.

Changed direct engine test contracts EngineUnitDamageModelTests.cpp and EquipmentDamageFailurePipelineTests.cpp to verify percentages, nozzle/thrust thresholds, physical module survival, capability projection, electric demand and heat. Old weighted tests are replaced, not weakened.

Explicit incomplete follow-up: runtime sources for thermal temperature history, radiation dose and actual operating-hours wear are NOT yet connected. Cobra wearPerOperatingHour remains zero rather than fabricating lifetime using a global dt; real per-engine burn telemetry is needed to accrue wear correctly. Keep reactor/cooling ideal until user changes scope. The real game damage acceptance must still use real EquipmentDamage and never fake slot shutdown.

Validation has NOT been executed by assistant; require actual user build/ctest results.

## 2026-10-18 — hub tactical prediction contract fixture corrected

User post-cleanup architecture run: 41/42 tests passed; the only failure was ArchitectureContractTests.cpp:415 ("hub tactical client prediction advances local motion"). The test expected ballistic p += v*dt but omitted localControlLaw, thereby using the current default Assisted flight law, which actively corrects velocity during neutral pilot input. This is a fixture mismatch, not grounds to alter production physics.

ArchitectureContractTests.cpp now explicitly sets LocalFlightControlLaw::Newtonian in that ballistic test. The spatial/frame assertions remain unchanged. Compilation/rerun of this correction is pending user validation; do not claim a green test yet.

## 2026-10-18 — fake slot-level damage harness removed

Direct user rule: a test that exists only for implementation convenience and bypasses the behavior it claims to validate is deleted rather than kept as an alternate acceptance path.

Removed completely:
- propulsion-matrix and ship-damage:<...> runtime scenario kinds;
- ShipRuntimeDamageOracle;
- diagnostic scenario diagnostics/scenarios/propulsion_damage_matrix.scenario;
- scripts/run-ship-damage-oracle.ps1;
- scripts/run-ship-physical-damage-matrix.sh;
- ShipPhysicalDamageMatrixTests.cpp and its CMake target;
- ClientShipCommand::DebugFailPhysicalPropulsionSlot / DebugRestorePhysicalPropulsionSlots;
- GameServer handlers and ShipCore debugFailPhysicalPropulsionSlot/debugRestorePhysicalPropulsionSlots;
- wire-protocol coverage for those deleted commands.

Canonical damage acceptance boundary:
ObjectModuleRuntime::addEquipmentDamage(...)
    -> EquipmentDamage
    -> EngineUnit
    -> buildPhysicalControlSlotState(...)
    -> damaged-main automation
    -> physical execution
    -> replicated presentation/BITE

No runtime damage acceptance may inject slot.operational=false, slot.thrust01=0, or other DiagnosticOverride actuator mutations as a substitute for engine/module damage.

Low-level actuator tests may still construct EngineSlotState directly when their declared subject is actuator math itself; they must not be presented or named as end-to-end damage validation.

## 2026-10-18 — direct weakest-engine main-bank balancing

The previous 10%-step greedy throttle trim was removed.

Canonical main-bank rule now:
- for the currently active main bank, read each surviving engine's instantaneous available thrust (already includes damage and >70% irregular-output dips);
- find the weakest surviving engine;
- in one O(N) pass, reduce every stronger surviving engine directly to that same instantaneous level;
- failed/zero-output engines are excluded from the minimum so one dead engine does not shut the entire bank down;
- damage asymmetry is never treated as steering authority, even if its accidental torque happens to help the requested turn;
- after direct throttle balance, a small bounded common-TVC trim may reduce residual torque caused by missing geometry;
- any remaining reaction stays physical and is left to the normal attitude/stabilization loop.

This rule is common to I/Y/X layouts and to Assisted/Newtonian main banks. It replaces the previous coordinate-descent/10%-step search and is cheaper and more deterministic.

Tests now explicitly cover:
- Assisted-I unequal pair direct balance;
- Newtonian-Y 80/100/100 -> balanced bank;
- Newtonian-X four different available thrusts -> direct weakest-engine balance;
- damage asymmetry is not reused as steering;
- a dead slot does not force all surviving engines to zero;
- TVC may reduce residual missing-slot torque without synthetic cancellation.

## 2026-10-18 — damaged-main feedback compile fix and validation rule

- Fixed ShipPhysicalActuatorPlant.cpp feedback candidate accounting: instantaneousSlotThrust01 now receives state[i] rather than the whole EngineSlotState vector.
- The failed navigation_runtime build meant the immediately following ctest invocation executed stale pre-existing test binaries; those PASS results are not accepted as validation of the new feedback layer.
- Validation commands for this work must chain build and ctest with && so tests never run after a failed rebuild.

## 2026-10-17 — lightweight damaged-main feedback automation

A new automatic damaged-main compensation layer now sits inside the existing physical execution path. It does not create a second pilot/autopilot path and does not use a general optimizer.

Doctrine:
- ordinary human/autopilot ShipControlState still defines the maneuver;
- current active main bank is used exactly as before; opposite bank is not recruited by this first layer;
- instantaneous per-engine authority includes five-channel damage plus >70% irregular-thrust dips;
- the feedback only reduces engines whose reduction improves parasitic angular error per unit useful main acceleration;
- uniformly reducing an already-unbalanced bank is explicitly not accepted as compensation;
- at most eight 10% throttle coordinate steps are considered, and healthy/symmetric banks exit before that loop;
- after throttle balancing, at most two small common-TVC correction steps are tried;
- automatic common TVC is capped at 2 degrees and disabled when an explicit pilot/autopilot main-gimbal command is active;
- common TVC is also disabled if any currently firing engine in the bank has no usable vectoring authority;
- feedback never increases an engine above its instantaneous damage-limited thrust;
- residual torque remains physical and reaches hull angular rates; there is no synthetic angular-acceleration subtraction.

Family behavior:
- Assisted and Newtonian use the same cheap main-bank feedback rule;
- their existing flight laws, hardware topology and remaining attitude/RCS behavior stay different;
- Newtonian-I with one main has no throttle redistribution path, so explicit TVC behavior remains physically visible;
- no opposite-bank compensation and no new RCS allocator are included in this first implementation.

Performance:
- hot-path temporary throttle state uses a fixed stack array (no per-tick heap allocation);
- healthy banks perform one allocation/reaction evaluation and exit;
- extra candidate evaluations occur only when the current bank has a real harmful torque imbalance.

Tests added/updated:
- Assisted-I unequal-pair throttle feedback;
- protection against hiding torque by uniformly killing thrust;
- common TVC residual correction without thrust suppression;
- Newtonian-Y damaged-main feedback using the same rule;
- helpful reaction during a commanded turn is left alone;
- gateway test now requires physical reduction of bias and explicitly rejects the old synthetic zero-residual behavior.

The previous coarse Assisted-only block that reserved up to 20% main force and directly subtracted angular acceleration has been removed from ShipControlExecutionGateway.

Next validation:
1) build/run ship_physical_actuator_plant and ship_control_execution_gateway;
2) if green, build EliteGame and run a native damaged-engine scenario through the real EquipmentDamage path;
3) visually verify that a damaged ship still flies imperfectly but controllably, including >70% intermittent thrust.

## 2026-10-17 — damage pipeline fixture and release-test assertion correction

Validation exposed two test-harness issues, not a production damage-matrix failure.

- EquipmentDamageFailurePipelineTests authored 100 m/s^2 forward/reverse main acceleration but left ShipParams at the default 5 g common load envelope. ShipDynamics correctly clamped the nominal 100 m/s^2 to 49.03325 m/s^2, so a 50% surviving aft bank produced 24.516625 m/s^2 rather than the test's expected 50. The fixture now sets maxGs/maxLinearGs=20 so the intended 100 -> 50 authority split is actually tested.
- ShipPhysicalActuatorPlantTests used C assert(). In the current release/NDEBUG build those assertions were compiled out; the unused-variable warnings proved the executable could report PASS without evaluating its predicates. All actuator-plant assertions are now explicit require()/exception checks and remain active in release builds.
- The previous ship_physical_actuator_plant PASS from the NDEBUG run is therefore not accepted as evidence. It must be rerun after this correction.
- ship_control_execution_gateway already used active require() checks; its PASS remains meaningful.

Next validation: rerun equipment_damage_failure_pipeline and ship_physical_actuator_plant, then continue only if both are genuinely green.

## 2026-10-17 — severe engine damage causes deterministic irregular thrust

Engine primary damage now has a pre-failure instability region:
- weighted primary damage <= 70%: thrust remains steady;
- damage > 70% and < 95%: thrust becomes increasingly intermittent;
- at the existing 95% primary failure threshold: thrust function fails completely.

Mechanics:
- EngineUnit derives thrustIrregularity01 from the same five accumulated damage causes; no second damage state exists.
- Irregularity ramps linearly from 0 at 70% damage toward 1 immediately before 95% failure.
- EngineSlotState carries thrustIrregularity01 alongside thrust01 and vectoring01.
- The static capability matrix continues to describe the maximum damage-limited thrust authority.
- ShipPhysicalActuatorPlant applies a deterministic per-slot pulse at the real force seam using controlTick.
- Pulses only remove instantaneous thrust; they never exceed the damage-limited maximum.
- The same pulse affects net linear acceleration and per-slot reaction torque, so an unstable engine can physically jerk the ship and create changing asymmetric moments.
- Pulse samples are deterministic and held for four fixed-control ticks, preserving server/client prediction/replay agreement while producing perceptible hiccups instead of frame-frequency noise.
- Client capability reconstruction derives the same instability from replicated five-channel module damage.

Diagnostics:
- BITE remains read-only and now carries outputInstability01 as a symptom of the already-derived engine state.
- Existing severity still comes from functional loss/failure; instability does not create a second damage/severity authority.
- Terminal lines append output instability when present.

Tests:
- engine_unit_damage_model locks steady output through 70%, progressive instability above 70%, and near-maximal intermittency just below 95%.
- ship_physical_actuator_plant verifies deterministic, bounded thrust pulses and exact healthy scale=1.
- equipment_damage_failure_pipeline verifies severe surviving damage reaches physical slot instability through the real module damage path.

Known presentation boundary:
- the normal engine HUD still reconstructs per-bar red thrust from aggregate executed acceleration. The physical force path is now per-slot intermittent, but truthful per-engine instantaneous red-bar telemetry remains a separate presentation/data-plane task; do not fake it from throttle.

## 2026-10-17 — independent thrust/TVC failure contract

- EquipmentDamage now owns a generic functional failure threshold.
- Cobra thrust failure threshold: 95% weighted primary damage.
- Cobra TVC failure threshold: 95% weighted gimbal damage.
- Functional failure does not delete/detach the engine module.
- Thrust failure => thrust=0, electrical demand=0, generated heat=0.
- TVC failure => vectoring authority/range/rate=0 while surviving axial thrust remains available.
- Physical slot state is derived from module damage; direct slot failure is not a valid EquipmentDamage/BITE acceptance path.
- BITE diagnoses thrust-function FAILED separately from TVC/nozzle FAILED; whole-equipment failure remains reserved for unavailable/destroyed equipment.
- New equipment_damage_failure_pipeline test verifies five-channel ObjectModuleRuntime damage -> EngineUnit -> slot -> capability.
- Slot-level diagnostic failure is not a valid EquipmentDamage/BITE acceptance path and no fake slot-damage runtime harness is retained.

Next: native real-game damage validation must enter through ObjectModuleRuntime::addEquipmentDamage(...) and then traverse EngineUnit -> slot projection -> automation -> physics -> BITE/HUD.

## 2026-10-17 — BITE end-to-end presentation connection

BITE is now connected end-to-end as a read-only diagnostic consumer:
- compact per-ship `bite::Summary`: equipment/degraded/failed/maintenance counts + highest severity;
- engine module snapshots are adapted outside the library into BITE DTOs;
- engine HUD CHECK/bar attention uses BITE severity policy; physical bar capacity still comes from propulsion capability;
- existing cockpit status terminal accepts generic diagnostic lines and now displays BITE engine faults even when docking is idle;
- when BITE is the reason the terminal is visible its title is SYSTEM; docking still owns DOCKING mode;
- terminal diagnostics are ready text/severity DTOs, so the terminal has no path back into equipment, damage, control or physics;
- focused `bite_api` test covers silence below 30%, CAUTION at 30%, WARNING at 40%, maintenance threshold, FAILED, HUD attention and terminal identity;
- engine widget geometry was also corrected: CHECK/model label centered, model label lowered toward bars, bars widened, physical I/Y/X groups uniformly distributed while Assisted fore/aft pairs stay grouped.

BITE remains optional for NPC behavior. Ship physics/control never consumes BITE output; NPCs may use authoritative capability/ship state directly.


## 2026-10-17 — isolated BITE library and HUD fault authority

BITE is now a separate static runtime library `EliteBite` under `src/game/bite`.

Hard boundary:
- BITE accepts only compact public DTO `EquipmentSample` + source revision;
- BITE returns only `bite::State` / `bite::Fault` and terminal text lines;
- BITE has no include/dependency on ShipCore, ObjectModuleRuntime, ClientWorldState, renderer/HUD, networking, damage handlers or concrete equipment implementation;
- rich ship/equipment data is adapted outside the library in `ShipBitePresentation.h`;
- BITE is presentation/diagnostics only and cannot affect physics/control.

Current engine policy:
- slow degradation below 30% functional loss is silent;
- 30% loss -> CAUTION and HUD attention;
- 40% loss -> WARNING;
- inoperative -> FAILED;
- wear crossing descriptor maintenance threshold creates MAINTENANCE REQUIRED;
- Cobra engine maintenance threshold is 20% wear;
- no fake service lifetime was invented: wearPerOperatingHour remains 0 until authored.

Engine HUD no longer decides its own damage threshold. Capacity remains physical propulsion state, while CHECK/bar fault attention comes from BITE.

BITE terminal formatter already produces unambiguous per-equipment lines including available output/nozzle authority and increased power/heat where applicable. Wiring those lines into the existing cockpit terminal surface is the remaining presentation connection; the BITE library itself must not include or call terminal/HUD code.


## 2026-10-17 — real common equipment damage layer replaces scalar-health bridge

The transitional "lost module health == impact damage" bridge has been removed from propulsion capability.

Every equipment module instance now owns one common EquipmentDamageLayer:
- accumulated runtime values: laser01, impact01, thermal01, radiation01, wear01;
- immutable equipment-type sensitivity weights: laser, impact, thermal, radiation, wear.

Damage values and weights are different data:
- accumulated values answer "how much of this cause has this particular unit received?";
- weights answer "how strongly does this equipment type react to that cause?";
- functional damage is derived as sum(accumulated cause * sensitivity), clamped to 0..1.

Runtime path is now real:
DamageEvent -> hit-volume health loss -> cause-specific equipment accumulator -> module revision -> engine effective state -> propulsion slot capability -> physical-control matrix -> control execution -> force/torque -> ship motion.

DamageEvent mapping:
- Laser -> laser;
- Explosion/Collision -> impact;
- Radiation -> radiation;
- EMP is deliberately not forced into the five physical channels; electrical effects remain separate;
- Thermal and Wear are supported runtime accumulators and are to be fed by thermal/service simulation rather than fabricated from weapon hits.

For engines:
- the common equipment sensitivity is the primary useful-work/efficiency sensitivity:
  laser .30, impact .45, thermal .15, radiation .01, wear .09;
- nozzle/gimbal mechanics remain an engine-specific second functional response to the SAME accumulated five causes:
  laser .15, impact .65, thermal .05, radiation .00, wear .15.
- there are not two sets of damage values. There is one accumulated damage state; multiple useful functions may respond to it differently.

Replication:
- ObjectModuleSnapshot now carries the five accumulated runtime damage values;
- weights remain static descriptor data and are not sent every snapshot;
- wire protocol bumped 15 -> 16 for the schema change;
- client capability/HUD projection can therefore consume the same real damage causes as server physics instead of reconstructing them from scalar health.

Recomputation remains revision-driven: changing a damage accumulator increments ObjectModuleRuntime revision; unchanged frames do not recalculate damage-derived engine capability.


### Engine damage recomputation/cache invariant

Damage-derived engine capability is event/revision driven, not recomputed every physics tick.

Existing runtime path already satisfies this:
- ObjectModuleRuntime owns a monotonic revision.
- ShipCore::refreshRuntimeState() returns immediately while that module revision is unchanged.
- Only a changed module/damage revision rebuilds per-slot EngineEffectiveState -> EngineSlotState projection.
- ShipActuatorState increments its own revision only when the projected slot state actually changes.
- ShipCore::capabilities() is separately cached by constructionRevision + actuatorRevision + massRevision.

Therefore:
- damage-derived maximum thrust / efficiency authority / gimbal authority are recomputed only after equipment damage state changes;
- ordinary per-tick propulsion evaluates only current commanded/generated thrust against the cached capability;
- when the five persistent damage channels are added to ObjectModuleRuntime, every mutation of laser/impact/thermal/radiation/wear damage must increment the same module damage revision;
- merely reading state or advancing an unchanged frame must not invalidate engine capability.

Do not introduce a second per-engine timer/dirty cache unless this revision contract is later proven insufficient.


## 2026-10-17 — engine damage model finalized for first live tests

The first production engine-unit damage model now uses five universal equipment damage channels:
- laser
- impact/explosion
- thermal/overheat
- radiation
- wear/time

The engine itself maps those causes into only two damaged functional outputs:
1) efficiency authority;
2) nozzle/gimbal mechanical authority.

Fixed Cobra engine weights for v1:

Efficiency damage:
- laser 0.30
- impact 0.45
- thermal 0.15
- radiation 0.01
- wear 0.09

Gimbal damage:
- laser 0.15
- impact 0.65
- thermal 0.05
- radiation 0.00
- wear 0.15

Semantics:
- impact/explosion is the dominant engine-damage mechanism, especially for nozzle/vectoring mechanics;
- laser is important for an exposed rear engine but less mechanically destructive than impact;
- overheat primarily reduces efficiency and only weakly affects gimbal mechanics;
- radiation is negligible for the engine unit;
- wear degrades slowly and is relevant to maintenance, with a stronger effect on gimbal mechanics than on efficiency.

Derived physics:
- available maximum thrust at fixed input power scales with efficiency authority;
- for equal useful thrust, lower efficiency raises required electrical input approximately as 1/efficiencyAuthority;
- waste heat is derived from input power minus useful power rather than being an independent arbitrary damage variable;
- gimbal range and gimbal rate scale with gimbal authority;
- sufficiently low efficiency makes the engine inoperative.

Current compatibility bridge:
- ObjectModuleRuntime still persists one legacy scalar health value rather than the five separate cause accumulators.
- Until common five-channel per-module damage persistence is added, ShipCapabilityBuilder maps legacy lost health into the impact channel.
- This is explicitly transitional and keeps existing damage paths functional without pretending generic health identifies the original cause.

Contract test:
- engine_unit_damage_model now locks the five channel weights and verifies:
  - healthy passport preservation;
  - impact is more severe than laser for efficiency and gimbal;
  - laser remains materially damaging;
  - thermal/wear primarily affect efficiency;
  - radiation is negligible;
  - efficiency couples thrust, electrical demand and waste heat;
  - combined damage is bounded/monotonic and total accumulated damage can disable the engine.

Next validation order:
1) compile and run engine_unit_damage_model;
2) run existing propulsion/capability damage tests to catch regressions;
3) only after those are green, run the real-game ship-damage oracle and inspect per-engine HUD/physics behavior.
4) Do not claim five-cause live damage provenance until ObjectModuleRuntime persists the cause channels instead of the legacy scalar bridge.


## 2026-10-17 — engine unit damage model becomes propulsion truth

A concrete per-engine functional damage model has been added. The goal is to make each physical equipment unit own the mapping from damage to degraded functionality, instead of letting HUD, capability matrices or controllers guess from generic health.

Canonical engine model:
- Runtime damage truth remains one scalar module health fraction health01 for now.
- Each damageable engine module may carry an immutable EngineUnitDescriptor passport.
- evaluateEngineUnit(descriptor, health01) produces the engine's effective functional state.
- Consumers use the effective state; they do not reinterpret raw health independently.

EngineUnitDescriptor currently contains:
- nominalThrustNewtons;
- nominalElectricalPowerMW;
- nominalWasteHeatMW;
- nominalGimbalRangeRad;
- nominalGimbalRateRadPerSec;
- thrustHealthExponent;
- vectoringHealthExponent;
- maxPowerCostPenalty;
- maxHeatPenalty.

EngineUnitEffectiveState contains:
- operational;
- health01;
- thrustAvailable01;
- maximumThrustNewtons;
- vectoringAvailable01;
- gimbalRangeRad;
- gimbalRateRadPerSec;
- powerCostMultiplier;
- heatMultiplier.

Damage mapping:
- health is clamped to [0,1];
- health=0 => engine offline, zero thrust and zero vectoring;
- thrust authority = health^thrustHealthExponent;
- vectoring authority = health^vectoringHealthExponent;
- gimbal range/rate scale with vectoring authority;
- per-generated-thrust electrical cost increases with damage according to maxPowerCostPenalty;
- per-generated-thrust waste heat increases with damage according to maxHeatPenalty;
- damage is monotonic and must never improve useful thrust/vectoring authority.

Power/heat semantics:
- requestedElectricalPowerMW() and generatedWasteHeatMW() are defined from ACTUAL generated-thrust fraction and the damaged engine state.
- They are intentionally not yet connected to PowerBus/ThermalSystem because the current propulsion stack still does not expose truthful per-engine throttle/allocation.
- Do not reconstruct engine power/heat from aggregate ship throttle. Wire these functions only after the per-engine force/torque allocator outputs actual engine loading.
- Cobra engine nominal electrical/thermal ratings currently remain zero rather than inventing MW values without an authored energy budget.

Integration:
- ModuleDescriptor now has optional EngineUnitDescriptor engine.
- ShipCapabilityBuilder checks the bound engine module's passport and uses evaluateEngineUnit() to derive EngineSlotState thrust01/vectoring01.
- Legacy engine modules without a passport retain the previous linear health->thrust/vectoring fallback during migration.
- Cobra's four damageable main-engine modules now receive the same first engine passport.
- Cobra per-engine nominal thrust is derived from existing ship physics:
      nominalThrust = ship mass * nominal bank acceleration / engines in bank
  using 260000 kg, 73.549875 m/s^2 and two engines per bank.
- Cobra v1 damage response:
      thrustHealthExponent = 1.0
      vectoringHealthExponent = 1.35
      maxPowerCostPenalty = 0.50
      maxHeatPenalty = 1.00
- These coefficients are gameplay/technology data and can later differ by engine type/race without changing damage code.

Architectural rule:
- equipment unit runtime health is the damage source;
- the equipment unit's own descriptor defines how health loss affects its functions;
- propulsion authority matrix only aggregates the already-derived engine capabilities;
- physical force/torque consumes those capabilities;
- HUD/BITE must only present replicated functional state and must never independently derive engine health from unrelated data.

BITE scope:
- BITE is not required as a physics owner.
- If retained, BITE should be presentation/diagnostic projection for HUD and maintenance only.
- It must not become a second source of equipment health.

Test:
- engine_unit_damage_model contract verifies healthy preservation, damage-degraded thrust/vectoring, increased per-thrust power/heat cost, dead-engine zero output, and monotonic damage behavior.


## 2026-10-17 — propulsion authority matrix: canonical model and current limits

This section is the canonical cross-chat description of the propulsion authority matrix and how it relates to runtime physics.

### Purpose

The matrix is a capability projection layer. It answers:

- which physical propulsion slots exist for the selected ship construction;
- which of those slots are operational and at what health/availability fraction;
- which translational/rotational control axes those slots can contribute to;
- how much normalized authority remains on each axis after damage.

The matrix is NOT the final per-nozzle thrust allocator and must not be treated as one.

Current layering:

engine slot topology + slot health
    -> per-slot authority contribution rows
    -> aggregate ShipPhysicalAuthorityMatrix result
    -> effective ShipParams / capability envelope
    -> ShipControlExecutionGateway
    -> actual main force / reaction torque physics

For damaged flight, the long-term target is:

requested force + requested torque
    -> bounded actuator allocator
    -> per-engine throttle/gimbal/cluster commands
    -> sum(F_i), sum(r_i x F_i)
    -> rigid-body motion

### Physical topologies

Supported ordinary-flight construction models are fixed:

- Assisted-I: 4 vectoring mains = 2 aft + 2 fore.
- Assisted-Y: 6 vectoring mains = 3 aft + 3 fore.
- Assisted-X: 8 vectoring mains = 4 aft + 4 fore.
- Newtonian-I: 1 aft vectoring main + 4 omnidirectional manoeuvre clusters.
- Newtonian-Y: 3 aft vectoring mains + 6 manoeuvre clusters.
- Newtonian-X: 4 aft vectoring mains + 8 manoeuvre clusters.

No private/custom engine count is authored by a ship descriptor. The descriptor may tune ratings and bind physical slots to damageable modules, but topology comes from PhysicalControlModelId.

### Matrix row meaning

Each physical slot contributes an AuthorityContributionRow with:

- thrust: normalized translation/rotation contribution;
- vectoring: normalized control authority available through nozzle vectoring;
- poweredMainVectoring: trajectory/attitude control available specifically while main thrust is active.

Axis authority uses positive/negative pairs where relevant:

- longitudinal: forward / reverse;
- strafe: right / left;
- lift: up / down;
- pitch;
- yaw;
- roll.

The aggregate matrix combines surviving slot contributions and produces:

- KinematicAuthorityEstimate authority;
- PoweredMainVectoringEstimate poweredMainVectoring;
- weakestAxisAuthority(...) where a scalar compatibility projection is still required.

Damage is monotonic: reducing/failing a slot must never increase any reported authority.

### Assisted matrix semantics

Every Assisted main is a vectoring main.

Aft mains contribute forward longitudinal thrust.
Fore mains contribute reverse longitudinal thrust.
All Assisted main slots also contribute vectoring authority to:

- strafe;
- lift;
- pitch;
- yaw;
- roll;
- powered trajectory strafe/lift;
- powered pitch/yaw/roll.

Assisted has no ordinary dedicated RCS slots in the new physical model.

The transitional backend still consumes scalar lateral/angular limits, so the current Assisted capability projection uses the weakest relevant matrix axis as the surviving vectoring fraction. This preserves the accepted healthy response while ensuring damage can only reduce the envelope.

Healthy I/Y/X intentionally preserve the accepted scalar flight envelope. Therefore healthy player-visible acceleration can be very similar across I/Y/X; their main gameplay difference is redundancy and degraded behavior after damage.

### Newtonian matrix semantics

Newtonian constructions have no fore main bank.

Main engines contribute:

- forward longitudinal main authority;
- powered-main vectoring authority while thrusting.

Manoeuvre clusters contribute independent translation/attitude authority.

Newtonian-I:
- one centered aft main;
- powered roll authority from the main is zero;
- four manoeuvre clusters provide fine translation/attitude correction.

Newtonian-Y/X:
- multiple aft mains;
- powered main vectoring can contribute pitch/yaw and layout-dependent roll;
- manoeuvre clusters remain the fine-control/self-stabilization system.

Newtonian reverse translation remains manoeuvre-cluster/RCS-like trim, not a fore-main bank.

### Newtonian main-engine lever arms

The physical plant uses actual application points and center of mass:

    torque_i = r_i x F_i

where r_i is the vector from center of mass to the slot application point.

Newtonian design intent is a tightly clustered main bank near the thrust axis.

Current simplified geometry:

- Newtonian-I main is radially centered.
- Newtonian-Y/X main radial x/y placement is scaled by NewtonMainRadialScale = 0.12.
- Their z position remains aft of the center of mass.
- Manoeuvre clusters remain at large/full lever arms.

Consequences:

- a straight burn from centered Newtonian-I produces approximately zero lever-arm torque;
- gimbaling that aft-centered main produces real pitch/yaw torque because the lateral force acts at a nonzero longitudinal z arm;
- losing one Newtonian-Y/X main produces a real but deliberately moderate asymmetric reaction torque;
- RCS/manoeuvre clusters retain strong attitude leverage.

### Physical actuator reaction

ShipPhysicalActuatorPlant::evaluateMainPropulsionReaction() is the current physical reaction calculation.

For the active longitudinal main bank it:

1. selects surviving slots of the active role;
2. distributes requested main acceleration according to surviving thrust availability;
3. converts slot normalized position to a physical application point using ship actuator half-extents;
4. subtracts centerOfMassLocalMeters to obtain r_i;
5. computes per-slot force;
6. accumulates torque = sum(r_i x F_i);
7. converts torque to pitch/yaw/roll angular acceleration through ship inertias.

This reaction is applied by ShipControlExecutionGateway to the real hull angular rates.

Therefore the matrix and r x F plant are separate concepts:

- matrix = remaining command authority/envelope;
- actuator plant = physical force/torque consequence of the actual surviving main bank.

### Current Assisted damage trim — transitional and known inadequate

Current ShipControlExecutionGateway contains a cinematic transitional Assisted trim:

- compute raw asymmetric main reaction;
- compare raw angular acceleration with surviving angularAccel trim authority;
- derive mainTrimLoad01;
- reserve up to 20% of requested main force at full trim;
- expose mainTrimUsableScale01;
- subtract available trim authority from residual angular acceleration;
- apply any remaining reaction to the hull.

Diagnostics:

- mainTrimLoad01: 0 healthy, rising with required trim;
- mainTrimUsableScale01: 1 healthy, below 1 when trim consumes propulsive margin;
- mainPropulsionTorqueBodyNm;
- mainPropulsionAngularAccelerationBodyRadPerSec2.

This is NOT a real actuator allocator. It does not solve per-engine throttle/gimbal commands and does not guarantee that the requested net force and near-zero requested torque can be achieved simultaneously.

The live in-game damage test exposed this limitation on Assisted-I: after losing one aft main, the ship can continue moving while rotating for too long instead of rapidly finding a stable compensated solution. The test correctly remains incomplete in that state.

The current 20% trim reservation must not be treated as final physics.

### Required next physics step

Replace the current scalar Assisted pseudo-trim with a small bounded force/torque allocator.

Desired constraint:

    F_actual approximately F_requested
    torque_actual approximately torque_requested

For ordinary straight Assisted acceleration:

    torque_requested = 0

Allocator priorities:

1. suppress dangerous parasitic torque;
2. preserve requested force direction;
3. preserve as much force magnitude as physically feasible;
4. minimize additional actuator/energy use;
5. obey per-engine thrust, health and gimbal limits;
6. if no exact balanced solution exists, reduce usable thrust until the best stable solution is found;
7. expose residual torque only when physical authority is genuinely insufficient.

The allocator should eventually output real per-engine usage so the HUD and resource model can consume actual survivor loading instead of reconstructing it from aggregate throttle.

### Newtonian degraded-flight intent

Newtonian should expose asymmetry more visibly than Assisted, but ordinary self-stabilizing manoeuvre controls may still compensate where physically available.

Preferred strategy order:

1. find a static body attitude / main-vector solution whose surviving resultant aligns with the desired trajectory and minimizes parasitic torque;
2. use main gimbal/vectoring trim;
3. use small paired/pulsed manoeuvre-cluster corrections if a static solution is impossible;
4. avoid secular unwanted linear acceleration by balancing correction impulse;
5. use continuous roll only when it is mathematically useful, not as a visual effect.

### Test/evidence hierarchy

Do not infer gameplay correctness from the matrix unit tests alone.

Evidence priority:

1. manually launched in-game EliteGame runtime behavior;
2. deterministic restore-vs-feature game A/B;
3. lower-level control -> gateway -> physical plant oracle;
4. structural/unit matrix contracts.

Current structural coverage includes:

- healthy matrix preserves accepted envelope;
- damage never creates authority;
- ShipPhysicalActuatorPlant verifies centered/offset r x F behavior;
- Newtonian-Y/X single-main loss creates bounded real reaction;
- gateway verifies Assisted trim diagnostics and Newtonian physical reaction;
- damage matrix exercises all six physical models across healthy/degraded cases.

The next acceptance task after HUD cleanup is to continue the explicit --diagnostic-scenario in-game damage run and then implement/falsify the real damaged Assisted allocator against that runtime oracle.


## 2026-10-17 — standalone adaptive engine-status HUD widget

The existing main-engine thrust indicator has been promoted into a standalone engine-status widget.

Architecture:
- Telemetry/presentation stays in EngineThrustHudPresentation.
- Rendering stays client-side in EngineThrustIndicatorRenderer.
- New EngineStatusWidgetStyle contract isolates visual skin from engine semantics.
- ShipHudProfile now carries engineStatusStyleId (default: human_amber).
- A future race/faction/ship descriptor may select another registered style without changing propulsion telemetry, physical layout, damage logic or warning semantics.

Adaptive layout:
- The widget sizes itself from actual content bounds: check-engine icon + model label + real I/Y/X engine bars + padding.
- No oversized decorative frame and no hardcoded Cobra-only width.
- Existing physical topology still determines bars:
  Assisted I/Y/X -> 2/3/4 paired groups, 4/6/8 main bars.
  Newtonian I/Y/X -> only aft main bars.
- Bars are slightly wider and spaced farther apart than the previous compact indicator.

Visual style (human_amber):
- deep black panel background;
- thin amber/yellow-orange border with subtle glow;
- small rounded/stepped corners;
- automotive-style check-engine glyph above model label;
- existing yellow capability + red actual-thrust semantics preserved.

Damage warning:
- EngineThrustBarPresentation now exposes fault.
- EngineThrustHudPresentation exposes warning if any physical main slot is degraded/unavailable.
- Fault is derived from per-slot physical availability in the presentation layer, not guessed by the renderer.
- Client renderer animates only the presentation state.
- Faulty engine bar fades smoothly in/out with a 1.6 s full cycle.
- Check-engine icon uses the same synchronized pulse when any engine in the widget is faulty.
- Healthy bars and icon remain steady.

Tests:
- EngineThrustHudPresentationTests keeps I/Y/X topology contracts.
- Tests default and descriptor-selected style id.
- Tests healthy widget does not raise warning.
- Tests an unavailable bound physical engine slot raises both per-bar fault and widget warning.


## 2026-10-16 — damage trim and Newtonian short-lever main geometry

Damage behavior refinement after the first six-model healthy in-game oracle:
- healthy I/Y/X remain behaviorally equivalent by contract;
- damage must create a real parasitic rigid-body reaction from per-slot force application around COM;
- Assisted automatically spends remaining vectoring/attitude authority to cancel avoidable parasitic torque;
- Assisted trim is not free: a cinematic main-force reserve is applied, so usable main acceleration decreases as trim load grows;
- if Assisted trim authority saturates, residual torque remains and is applied to the hull;
- runtime diagnostics now expose `mainTrimLoad01` and `mainTrimUsableScale01`;
- Newtonian does not get the same hidden full automatic trim; residual damaged-main reaction remains visible for RCS/orientation doctrine;
- Newtonian Y/X main clusters are modeled close to the thrust axis: radial placement is 12% of the hull half-extent radius, while longitudinal placement remains aft;
- Newtonian RCS clusters remain on the full lever arm because they are intended to generate corrective torque efficiently;
- the physical actuator plant already computes `r x F` using slot position relative to `centerOfMassLocalMeters`; this is now the authoritative parasitic-moment basis.

New/updated tests:
- ShipPhysicalActuatorPlantTests require damaged Newtonian Y/X main torque to remain non-zero but bounded with the short radial lever arm;
- ShipControlExecutionGatewayTests require a damaged Assisted-Y aft bank to have a real raw geometric pitch bias, then consume automatic trim authority, reserve some usable main thrust, and leave near-zero avoidable pitch reaction when trim capacity is sufficient.


## 2026-10-16 — propulsion authority matrix and in-game control oracle

Current restore point before the propulsion-authority refactor:
`restore/pre-propulsion-authority-matrix-2026-10-16`.

Working branch:
`feature/propulsion-authority-matrix-2026-10-16`.

Physical-control architecture:
- individual engine/module health is projected through a simplified propulsion-authority contribution matrix into the existing aggregate movement envelope;
- the matrix is a cinematic/causal bridge, not a detailed engine/gas-dynamics solver;
- healthy Assisted/Newtonian I/Y/X constructions must preserve the accepted pre-matrix movement behavior;
- damage may remove or asymmetrically reduce authority but must never create extra authority;
- Assisted I/Y/X use 4/6/8 vectoring main engines respectively; Newtonian uses its own main/RCS topology;
- autopilot remains a virtual pilot and uses ordinary ShipControlState only.

Testing policy changed after prior false confidence from green unit/corridor tests:
- structural/CTest coverage is necessary but not sufficient;
- primary behavioral acceptance is now an in-game `EliteGame.exe` control->motion oracle;
- the oracle is manually armed/launched, then cycles all six physical models inside one game process through the same `SelectPhysicalControlModel` path used by Ctrl+F9/F10;
- each command logs COMMAND / EXPECTED / ACTUAL / RESULT;
- command completion is condition-driven: a command remains active until its physical expectation becomes true;
- elapsed game time must not produce a physics FAIL; if an expectation never occurs, the probe remains on that command and logs WAIT/INCOMPLETE evidence instead of declaring a timer-based behavioral failure;
- between models the oracle applies BrakeToStop and waits for near-zero linear/angular motion before starting the next model.

Manual launch:
- arm with `scripts/run-ship-control-oracle.ps1` (it now only creates the marker; it does not start EliteGame.exe);
- alternatively create `D:/tmp/elite/ship_control_oracle.enable`;
- start EliteGame.exe manually as usual;
- combined report: `D:/tmp/elite/ship_control_oracle_matrix.log`;
- the marker is removed automatically after all six models complete.

Observed from first runtime logs before converting the probe to condition-driven execution:
- Assisted-X, Newtonian-I and Newtonian-Y completed all 14 old timed steps;
- Newtonian manual strafe/lift release returned the commanded manoeuvre delta-velocity close to zero, confirming the self-stabilizing manual RCS path in those runs;
- the old timed oracle is not authoritative because it sampled at fixed elapsed times rather than waiting for the expected state.


## 2026-10-06 — current Navigation V2 execution contract

The authoritative current execution description is
`src/game/navigation/NAVIGATION_V2_EXECUTION_ARCHITECTURE.md`.

Key project decision:
- Planner publishes exact parametric route geometry and physical speed constraints;
- ClientRouteAutopilot owns current player-route composition and monotonic spatial progress;
- CourseCaptureGuidance uses actual velocity direction and a temporary smooth capture curve only when materially off-center;
- PredictivePilot treats hull attitude as an actuator problem and outputs only ordinary ShipControlState;
- server remains blind to player navigation intent and owns only authoritative physics/world state.

Production acceptance and historical navigation labs are now explicitly separated.
`legacy_navigation_lab` is diagnostic history, not a release gate.

## 2026-10-03 — player navigation ownership boundary

Player navigation/autopilot is a client concern.

The authoritative server owns:
- world state;
- collisions/damage;
- ship physics;
- application of ordinary ShipControlState;
- authoritative replicated position/velocity/orientation/angular rates.

The client owns:
- route intent and UI;
- RoutePlanner;
- route validation/refinement;
- visible corridor/HUD;
- RouteFollower V2;
- PredictivePilot V2;
- Human vs ClientAutopilot selection.

The server must not need to know whether an ordinary control sample was chosen by a human or by the client virtual pilot. An optional diagnostic source flag may be added later, but it must not change physics or navigation ownership.

SHOW ROUTE is local navigation functionality. If stabilization is required before planning, the client virtual pilot performs it through the same ShipControlState surface as keyboard control and waits for authoritative snapshot feedback before calculating the route.

Automatic docking similarly sends no navigation task to the server; it continuously converts the client route into ordinary control samples.

## 2026-10-03 — future Emergency Damage-Minimization Planner

Add a separate emergency-navigation module after the normal route-refinement loop is mature.

Normal mode and emergency mode must interpret "physically impossible" differently:
- normal mode may stop/hold/return control when no collision-free route exists within ordinary vehicle limits;
- emergency mode must continue searching for the least-damaging survivable outcome instead of treating an unavoidable collision as a terminal planner failure.

Emergency module scope:
- compute whether collision is truly unavoidable under all available thrust/rotation options;
- choose evasive continuation rather than braking when that reduces impact severity;
- choose contact point / hull attitude that minimizes expected structural, crew and critical-system damage;
- allow extreme turn/burn profiles outside ordinary comfort/game-flight doctrine while remaining inside hard physical capability;
- orient expendable/armored structure toward impact where appropriate;
- trigger subsystem isolation / shutdown where available;
- as final escalation, support pilot escape-capsule / ejection decision when modeled by ship systems.

This module is explicitly separate from ordinary RoutePlanner/Autopilot V2. Ordinary validation feedback must first request route refinement. PhysicallyImpossible is a planner outcome only after meaningful route alternatives have been exhausted or the world/vehicle geometry proves impossibility.

Planner critical outcomes must remain structured and user-readable:
- InvalidWorldData;
- HullDoesNotFit;
- GoalGeometricallyIsolated;
- UnavoidableCollision;
- PropulsionInsufficient;
- DynamicWindowUnavailable / WaitForWindow.

## 2026-09-28 — build/test command ownership is now explicit

Root `BUILD_TEST_LAYOUT.md` is the authoritative developer command map.
It records canonical runtime paths, standalone test build trees, build-target vs CTest-name mapping, and the requirement to inspect current CMake ownership before giving commands.

Current Automatic docking candidate uses stop-and-settle planning, speed-aware terminal turn geometry, curvature-based speed profiling, and semantic target-dock clearance. Historical sections describing moving-origin coast planning are retained only as history and are not the current contract.

## 2026-09-28 — Automatic docking state semantics corrected

Automatic docking now distinguishes two preparation semantics:

`ApproachHold`
: retain measured linear VREL, command zero translational acceleration, damp
  angular rate, and plan from the future coast position.

`FinalIngress`
: brake to a stop, hold position, acquire the rotating terminal attitude, then
  replan/execute the short terminal ingress.

The HOLD handoff is now based on whether the craft can physically brake inside
the accepted spatial envelope. It is no longer gated by an arbitrary fraction
of tracking-speed tolerance.

SpatialCorridor course validity is also corrected at zero reference speed:
stationary checkpoints have attitude, but no translational course direction.
This prevents a 180-degree hull attitude from being mislabeled as corridor
loss while the ship is essentially on the HOLD point.

Planner execution headroom is now explicit: the route/trajectory compiler uses
90% of the reserve-reduced acceleration envelope; AcceptedProgram validation
keeps the remaining margin.

Status: candidate pending Windows native/build/live evidence.

## 2026-09-28 — module split implementation candidate

Navigation and docking infrastructure now have concrete source/build boundaries.

Production orchestration consumes:
`RoutePlannerApi -> AcceptedManeuverProgram -> RouteFollowerApi`.

The legacy algorithms that produced the successful current flight remain private
backend implementation during migration. This avoids rewriting route geometry
and control behavior at the same time as module ownership.

`GameServer` no longer reaches into Planner/Follower backend classes for
planning, alignment, page selection, sampling or tracking. `SpaceState`
manual guidance likewise consumes the public RoutePlanner facade.

Dock traffic/model/landing source is physically separate under
`src/game/docking/` and builds as `EliteDockingInfrastructure` without a
Navigation link. The first controller implementations exist primarily to make
the API executable/testable before live integration.

The architectural invariant is now mechanically gated: new public navigation
headers cannot import docking Traffic/Landing, and Traffic/Landing cannot import
navigation Planner/Autopilot.

Status: candidate only. Windows MinGW/native gate and EliteGame compile are
required before proceeding to authoritative dispatcher integration.

## 2026-09-28 — reusable navigation separated conceptually from docking infrastructure

Project architecture now treats route planning/execution and docking terminal
operations as different domains.

Reusable navigation domain:
`RoutePlanner -> accepted route/program -> RouteFollower/Autopilot -> vehicle control/physics`.

Docking infrastructure domain:
`DockTrafficController -> DockingClearance -> Planner/Autopilot -> LandingHandoff -> DockLandingController -> gear/contact/latch`.

The separation is intentional so Planner/Autopilot can serve tunnels, canyons,
formation joins, pursuit, repair and NPC travel without importing docking queue,
pad, landing-gear or latch semantics.

The target source layout physically separates planner, autopilot, traffic,
landing and dock-model code. Public DTO/API headers are the only permitted
cross-domain dependency. Architecture tests must enforce the boundary rather
than relying on convention.

Dock traffic is modeled as reservation of infrastructure resources rather than
steering:
- portal;
- near-entry hold point;
- conflict zones;
- internal lane;
- parking pad.

A far-away docking request is only an inquiry/provisional recommendation. It
does not reserve resources. Queue eligibility begins when the ship reaches a
controlled approach horizon based primarily on ETA. This prevents a ship that
requested from five minutes away from blocking traffic that is already near the
station.

The first holding implementation is intentionally minimal: one physical
EntryHoldPoint/final approach lane per portal and a virtual dispatcher queue.
Only one ship enters that lane; other queued ships remain outside it and can stop
safely on their existing planned approach until advanced. A dedicated holding
yard/ring remains an optional future infrastructure feature, not a prerequisite.

Physical aperture/pad fit is absolute. Size class is a separate allocation
policy. Emergency may assign a physically compatible larger-class dock but
never a smaller one.

Full contract:
`src/game/docking/DOCKING_INFRASTRUCTURE_ARCHITECTURE.md`.

## 2026-09-28 — Dock geometry now matches docking semantics

The long Automatic SpatialCorridor has positive live flight evidence: corridor
retention, smooth turns and dock-relative vertical/roll alignment are working.

Docking geometry had a separate source-of-truth defect. Guidance dock render
meshes are hollow, but monolithic collision fallback was a solid descriptor OBB.
That made actual internal ingress unprovable and explains why the runtime had
historically terminated at exterior pre-capture.

Monolithic descriptors can now author a decomposition of logical collision
boxes. Guidance test docks use four structural wall boxes around the aperture,
so the existing shared HitComponent remains the sole collision/navigation source
while preserving the hole.

Dock semantic anchors now include an explicit positive internal
`captureDepthMeters`. FinalIngress consumes it to author one exact collinear
sequence through pre-capture, entrance and internal capture. Dynamic trajectory
compilation remains subordinate to that geometry and exact obstacle clearance
must succeed.

Permanent authoritative latch/undock state is not part of this candidate; it
follows only after physical ingress to the internal capture point is verified
on Windows.

## 2026-09-28 — Navigation v2 Automatic route ownership simplified

Automatic docking now has one route ownership chain:

`server DockingAdvisoryPlanner -> authored geometry -> TrajectoryGenerator dynamic compilation -> AcceptedManeuverProgram -> TrajectoryFollower -> ShipControlState -> ship flight law/physics`.

The AcceptedManeuverProgram is also the sole Automatic presentation source:
its route is replicated to the client and adapted read-only to Hub-map and HUD
corridors. The client/manual DockingAdvisoryPlanner is no longer an Automatic
preflight dependency.

TrajectoryGenerator is not a second route planner in this contract. For
Automatic Stage 1 it receives Planner geometry with
`pathGeometryAlreadyAuthored=true` and may calculate only physically feasible
timing, speed, orientation, angular state and acceleration along that geometry.
If dynamics cannot satisfy the geometry, planning fails/replans; an execution
layer must not silently invent a replacement path.

Spatial corridor execution is course-correcting rather than speed-governing.
Cross-track displacement creates an inward look-ahead steering target while
the accepted local speed profile remains authoritative. The old
cross-track-speed governor and its policy field were removed.

Docking Automatic is a machine controller, not a simulated human pilot:
`PilotSkillExecutor` was removed from its execution seam. NPC skill modelling
remains separate.

Status: implementation candidate on `main`; Windows/native/live acceptance is
pending.

## 2026-09-27 — publication after repository integration

The vehicle-level autopilot interface, local route-speed scheduling, spatial
HOLD capture and updated active docking test gate have been published on
`forzub/Elite` main together with the pre-existing space-games research
commit. GitHub merge `c3b5dc2` matches the verified local file tree.
The Windows target gate is pending and must decide whether the late angular
error, approach stop and FinalIngress now behave correctly in flight.

## 2026-09-27 — test-contract audit

The active docking gate now checks the vehicle-motion interface rather than
the superseded nominal engine schedule, and includes sampling of the accepted
motion program. The overlay check now asserts retained route start and
Hub-local projection rather than an obsolete comment string. Thirty non-benchmark navigation/docking Python checks were
also sampled: twenty passed, ten reported unrelated earlier marker/fixture
mismatches outside the revised control boundary. No Windows target-machine
verification has occurred for the new commits. Publication is blocked by
the prior automatic review rejection of `git push origin main`; the remote
checkout still cannot fetch the candidate.

## 2026-09-27 — vehicle-level control candidate

Autopilot authority is expressed as a system-frame target velocity plus
pilot-executed linear and angular acceleration demand. The physical ship
control law observes actual motion and chooses propulsion; engine schedules
no longer cross the accepted-program, Follower, bridge or ShipControlState
boundary. Newtonian program acceptance proves body-axis acceleration against
installed capability without issuing throttle commands. Assisted still uses
its canonical speed/stabilizer model. Runtime acceptance remains pending on
the Windows target. Port latch and precise in-plane capture remain separate
unimplemented scope.

## 2026-09-26 — route heading, entry attitude and boresight ownership

Manual route geometry may carry an explicit launch-heading constraint. For
docking guidance the start position is not enough: the first visible corridor
segment is authored from the real hull-forward axis so a human returning to the
cockpit sees the tunnel through the windshield. Obstacle handling may shorten
that forward lead, never silently rotate it.

Automatic execution separates **planned entry attitude** from **current hull
attitude**. A translation program must not assume that tiny RCS jets can provide
route acceleration while the main-engine axis still points elsewhere. Planner
therefore authors the route-entry body attitude; Autopilot physically acquires
it in the bounded Aligning phase; then the consumed/stale program is discarded
and replanned from authoritative aligned state/time.

AcceptedManeuverProgramBuilder remains a hard physical gate. Its main/RCS split
uses installed propulsion authority. Assisted automatic velocity stabilization
is not an executable RCS substitute and must not be used to make an otherwise
impossible actuator schedule appear feasible.

The cockpit center boresight is presentation only: it is a fixed projection of
the hull optical/nose axis. It owns no navigation or control state and is
separate from the velocity/flight-path indicator.


# PROJECT STATE

## 2026-09-26 — heavy Automatic planning no longer belongs to fixed-step

The authoritative Automatic docking pipeline now has an explicit Planning phase.

Fixed-step responsibilities:
- hold the stabilized ship;
- launch/snapshot planner work;
- poll completion;
- validate/install the immutable AcceptedManeuverProgram;
- execute Follower/control/physics.

Heavy advisory/Ruckig/program construction executes outside fixed-step from
immutable copied inputs. No worker reads live GameSimulation state.

Angular planning is now a Planner responsibility rather than a builder inference:
TrajectoryGenerator can consume real initial hull attitude/angular velocity and
compile a bounded angular state sequence under the vehicle's angular capability.
The accepted-program layer preserves that authored omega and remains a separate
feasibility gate.

Presentation remains independent from authority:
- retained docking tunnel stays visible in Guidance and Automatic;
- active card action state is rendered bright green;
- cockpit text reports the actual manual/automatic docking mode from request
  state, with full current UI language localization.

The existing NavigationWorkScheduler remains the deterministic job-order/budget
abstraction; the present Automatic fix removes main-thread blocking for live
docking. A future shared planner worker-pool can consume scheduler dispatch
without changing the Planner/Follower contract.


## 2026-09-26 — route presentation is independent from control ownership

Docking route/tunnel presentation and ship-control authority are now explicitly
separate concerns.

Guidance and Automatic use the same visible advisory corridor. Switching to
server-owned Autopilot must not erase a route already calculated for the same
dock, and Human hand-back must not erase it merely because authority changed.

Rotating terminal attitude is also now a first-class trajectory boundary:
terminal pose plus terminal angular velocity are fed into trajectory generation.
The trajectory carries the rotating target motion before the final sample;
AcceptedManeuverProgramBuilder remains a physical feasibility gate rather than
being relaxed to accept an instantaneous terminal omega discontinuity.

Assisted manual handling is accepted and is not a current change target.


## 2026-09-26 — Assisted is explicitly nose/course-coupled; Automatic plan failure is fail-safe

Assisted control doctrine is now explicitly airplane-like in the sense
that hull direction and travel direction are tightly coupled. A sharp hull turn
must prioritize removal of old sideways relative velocity before spending the
whole load envelope rebuilding longitudinal speed.

Automatic docking planning is still synchronous today, so a failed heavy plan
must never be called repeatedly from fixed-step. Until planning is moved to a
budgeted/asynchronous job boundary, initial plan failure is one-shot and returns
Human authority with a diagnostic reason.



## 2026-09-26 — Automatic docking native acceptance complete; live acceptance pending

Planner/Follower/actuator ownership is now green under the focused native
suite and canonical build. The next acceptance boundary is live behavior only.
Current Automatic scope ends at a collision-free pre-capture pose outside the
solid station target and restores Human control.



## 2026-09-26 — shared linear-load envelope remains authoritative

The runtime-control failure did not justify changing production physics.
The physical rule remains:
main propulsion has priority, but `main + manoeuvre/RCS` must fit the shared
`maxLinearGs/maxGs` linear acceleration envelope. A test may not demand full
main-envelope acceleration plus additional lateral/RCS acceleration beyond that
same envelope.



## 2026-09-26 — Automatic docking is implemented but not yet accepted

Current distinction:
- implementation exists end-to-end and canonical build passes;
- Planner/docking geometry, accepted-program construction and maneuver tracking
  native gates pass;
- the runtime-control native contract still fails.

Thus `START DOCKING` is no longer a stub architecturally, but working autopilot
status is still pending execution-layer acceptance and live evidence.



## 2026-09-26 — accepted actuator allocation is part of the Planner/Follower contract

The authoritative execution chain is now:

```text
trajectory / physical proof
  -> AcceptedManeuverProgram
      - reference pose/velocity/omega
      - rear-main / fore-main / manoeuvre feed-forward
      - explicit feedback reserve
  -> TrajectoryFollower
      - samples the same nominal actuator program
      - computes bounded tracking feedback separately
  -> NavigationRuntimeControlBridge::stepProgram
  -> ShipControlState
  -> DynamicMotionSystem::applyNavigationActuatorProgram
  -> shared fixed-step physics
```

This closes the previous second-source-of-truth hole where physics received only
a net linear acceleration and chose the engine/RCS split again.

Automatic docking currently owns approach execution only. Its terminal point is
a collision-free pre-capture pose outside the still-solid station hit volume.
The next docking layer, after live acceptance, is physical capture/contact/latch
ownership. Navigation must not bypass collision or teleport to simulate latch.

Before accepted-program execution the server may enter a bounded physical hull
alignment phase. Alignment consumes real time/state, therefore its old program
is discarded and the route is replanned before execution.



## 2026-09-26 — Planner/Follower execution has one authoritative executable product

Navigation execution authority is now explicit:

```text
planner / trajectory proof
        -> AcceptedManeuverProgram
        -> TrajectoryFollower
        -> NavigationFrameBoundary
        -> NavigationRuntimeControlBridge
        -> ShipControlState
        -> shared physics
```

`AcceptedShortSegment` is no longer a Follower API. It remains only as a transitional Stage-12 runtime-lab planner product and must cross the explicit lab-only adapter before execution.

Automatic docking is a server-owned Autopilot lifetime. The client owns request/presentation state only. Manual docking guidance remains a separate lifecycle that intentionally hands control back after route publication.

Current Automatic implementation proves and executes an approach program and handles controlled stabilization/replan/human hand-back. Physical station latch/contact transfer remains a later docking/game-state layer; navigation must not fake that transition by teleporting or writing authoritative motion directly.


## 2026-09-26 — acceptance code must consume authoritative mode state, not resurrect shadow owners

The client-mode ownership rule now explicitly covers diagnostics and acceptance
harnesses as well as production code.

A harness may drive `ClientModeState` and verify projection into
`CoordinateDisplayService`, but it must not require a service-owned transition
API such as the retired `CoordinateDisplayService::cycle()`. Test code is not
an exception to the one-owner rule.

The canonical build failure caused by that stale API use has been corrected;
fresh target-machine build evidence is pending.


## 2026-09-26 — selectable-mode ownership rule implemented across active domains

Architecture rule: every persistent selectable mode has exactly one
authoritative state owner; renderers/controllers/services may project or execute
that state but do not own duplicate transition logic.

Current owners:
- local flight doctrine -> `LocalFlightControlStateMachine` over
  `DynamicMotionState`;
- global client presentation modes -> `ClientModeState`;
- navigation module enablement -> `NavigationModuleState`;
- game presentation target -> `GamePresentationCoordinator`;
- system-map Galaxy/System/Detail/Hub submode -> `MapModeState`.

Assisted automatic lateral stabilization is deliberately separate from manual
gas-limited RCS. It uses the ship-profile aggregate Assisted lateral capability
and remains bounded by the common linear-load envelope.

## 2026-09-26 — state ownership rule generalized across runtime modes

Architecture rule: every persistent selectable mode has one authoritative state
owner and explicit transition API. UI/input may request a transition; renderers,
formatters and physics systems consume/projection the resulting state and do not
silently choose their own mode.

Current owners:
- local flight law/alignment/Assisted persistent controls:
  `LocalFlightControlStateMachine` over `DynamicMotionState`;
- global human-facing client modes:
  `ui::platform::ClientModeState`;
- navigation module visibility/enabled state:
  `NavigationModuleState`;
- application presentation:
  `GamePresentationCoordinator`;
- system-map Galaxy/System/Detail/Hub submode:
  `MapModeState`.

Projection/storage objects may mirror a selected mode only where necessary, but
must not expose independent transition logic.

Assisted and Newtonian now also differ materially in physical doctrine:
Assisted owns automatic lateral stabilization and neutral angular damping;
Newtonian preserves translational/angular inertia absent explicit thrust or
alignment commands.

## 2026-09-26 — automatic docking architecture boundary

Automatic docking must not be implemented as visual gate chasing.

Production execution ownership must be:
`AcceptedManeuverProgram -> TrajectoryFollower ->
NavigationRuntimeControlBridge -> ShipControlState -> shared physics`,
under a server-owned Autopilot lifetime for the player ship.

The current BrakeToStop takeover proves control ownership can move to the
server, but it is only preparation. Full automatic docking begins when that
ownership persists while a proved maneuver program is advanced each fixed step.

## 2026-09-25 — shortened-axis join must have real route-search clearance

The final-axis soft-shortening contract now includes endpoint clearance. The
binary-search boundary itself is not an acceptable join because it can be only
epsilon-clear and unreachable by the visibility graph.

Current policy backs the join away by at least 50 m or four hull radii, then
retreats farther toward mandatory ingress if route search still fails. This
preserves the exact docking axis while preventing a false no-route result at an
obstacle contact boundary.

## 2026-09-25 — canonical docking verification entrypoint

Repository now owns a single root verification entrypoint:
`verify_docking.sh`.

This script encodes the build-layout contract so developers cannot accidentally
request navigation-runtime-only targets from the canonical game `build/` tree.
Its dedicated test tree is `build/tests/navigation_runtime`.

The manual-docking static checker is revision-stamped
`20260925-soft-axis-v2` so stale working copies are immediately visible in
verification output.

## 2026-09-25 — navigation-runtime tests are standalone, not targets of canonical game build

Build-layout contract: `docking_advisory_tests` and its CTest registration live
under the standalone CMake project `tests/navigation_runtime`. They are not
configured into the canonical graphical `build/` directory.

Use `build/tests/navigation_runtime` as the dedicated test build tree. A
Ninja `unknown target` from canonical `build/` is a command/layout error,
not a test failure.

Static commissioning checks must test semantics rather than ban valid probe
syntax. The long final-axis `clear(align,stop)` probe is legal when it leads to
soft-axis shortening; only immediate rejection of that preferred lead is
forbidden.

## 2026-09-25 — final docking axis has hard ingress plus soft preferred lead

Docking-axis semantics are now explicitly split.

Hard semantic geometry:
- stop point is outside the port by standoff;
- only the immediate ingress length `max(700 m, 3 * standoff)` must remain
  collision-free on the exact port axis.

Soft/manual-flyability geometry:
- Assisted requests up to 9000 m of straight final-axis lead;
- that long lead exists to support a broad 6000 m terminal turn;
- obstruction on the far part of this preferred lead does not mean the dock is
  unreachable;
- Planner finds the longest clear axis prefix, then continues topology search
  and radius fallback.

This prevents a planning preference from becoming an early global task
cancellation while preserving the real port-ingress safety condition.

## 2026-09-25 — implemented commissioning constants for broad manual Assisted docking

Current product defaults and manual-docking commissioning values are:
- local flight default: Assisted;
- manual Assisted final-axis lead: 9000 m;
- preferred terminal radius: 6000 m;
- terminal fillet allocation: 0.85 of adjacent segment room;
- nominal open-transit tunnel center tolerance: 60 m;
- open-transit release tolerance: 120 m;
- outside-release grace: 1.00 s.

The nominal tunnel remains the guidance target. The wider release band only
controls when manual guidance is discarded, so warnings still occur before
cancellation.

Assisted BrakeToStop semantics are not a slow target-speed ramp. The controller
sets target VREL to zero immediately and uses installed directional main
authority. Healthy Cobra reverse/fore main therefore brakes without the
Newtonian tail-to-velocity maneuver; Newtonian remains a selectable alternate
law.

These source changes are not yet target-accepted. Windows native/static tests,
canonical build and live map/stop inspection remain the evidence boundary.

## 2026-09-25 — player local-flight default is Assisted; manual guidance is forgiving, not disposable

Control-law policy: newly initialized ship motion should default to Assisted.
Newtonian remains a fully supported explicit mode, but must no longer be the
implicit fresh-session/player default.

Docking guidance policy: the manual tunnel is advisory presentation, not a
single-strike safety interlock. Crossing the nominal section still drives
warning/critical HUD state, but route lifetime uses a much wider release
envelope plus grace. A modest piloting excursion must not erase the navigation
task.

Manual Assisted route shape must be visibly broad at station scale. The
preferred curve is allowed to be long and expensive; Planner must first change
topology/ingress and only then tighten radius. The final semantic docking axis
remains hard.

DockPrep Assisted stopping semantics are physical but decisive: command target
VREL=0 immediately, use the installed fore/reverse main bank directly when
healthy, and reserve hull flip for Newtonian or Assisted fallback when reverse
main authority is unavailable.

## 2026-09-25 — terminal radius is a route preference; execution remains program-owned

Manual Assisted docking now distinguishes **preferred geometry** from
**route existence**.

The 1500 m terminal turn radius is a human-flyable preference. Planner must try
to preserve it by changing coarse geometry first. It first samples alternate
pre-alignment ingress directions around the fixed final docking axis, then may
use expanded-clearance/full-scene rerouting. Either path is allowed to become
substantially longer, including going around an entire station or structure. If no such route can preserve 1500 m,
Planner may tighten the terminal circular fillet as far as collision-free
geometry permits. A single failed arc candidate must never cancel navigation.

The exact final docking-axis ingress remains semantic hard geometry: if that
segment is blocked, the dock is genuinely unavailable under the current docking
contract.

The planner now publishes diagnostic route-choice facts
(`terminalDetourUsed`, `terminalTurnRadiusRelaxed`,
`terminalTurnRadiusMeters`) so live acceptance can distinguish rerouting from
radius concession instead of inferring behavior visually.

Automatic docking has a separate invariant: display guidance is not executable
authority. START DOCKING must eventually retain server Autopilot ownership and
consume the same physical program that was capability/collision proved:
AcceptedManeuverProgram -> TrajectoryFollower -> NavigationRuntimeControlBridge
-> ShipControlState -> shared physics. Current production player docking has
not yet completed that owner; until it does, the automatic UI remains disabled.

## 2026-09-25 — docking frame-density boundary is now non-crossable by sparse cadence

The 500 m open-transit cadence may not jump across the start of terminal
density. Transition is therefore handled as an explicit boundary event:
- compute remaining route distance at the current published frame;
- if a normal sparse step would cross the terminal activation distance, shorten
  that interval to the boundary;
- after that anchor, use the 250 m terminal cadence.

This makes the final-density guarantee independent of prior sparse-grid phase.

## 2026-09-25 — final docking frame density transition is anchored, not endpoint-triggered

The 500 m -> 250 m display transition must not be selected from the next frame's
endpoint, because that allows one long sparse chord to terminate inside the
final dense region. Terminal cadence therefore activates one terminal interval
before the nominal 2 km boundary. This makes the transition continuous and
ensures all intervals whose downstream region is inside the last 2 km are at
most 250 m.

## 2026-09-25 — docking guidance geometry is curvature-aware near the station

Manual docking guidance now distinguishes route truth from display density.
Planner corners are circular fillets derived from lateral acceleration
capability and segment room. Display frames use 500 m spacing in open transit
and 250 m within the last 2 km, measured by route progress rather than chord
distance. This prevents a curved approach from being represented by a few long
straight visual chords.

The temporary docking-preparation Autopilot is authoritative and physical: it
owns the ship, repeatedly commands BrakeToStop, and planning is gated on a
replicated near-zero Hub-relative speed plus near-zero angular rate. Numeric
begin/settled diagnostics are now part of the live acceptance evidence.

Automatic docking remains a separate execution milestone. The request enum
already contains Automatic, but the active SpaceState docking implementation
processes Guidance only. The correct next automatic path is accepted physical
maneuver program -> TrajectoryFollower -> NavigationRuntimeControlBridge ->
ShipControlState, not direct chasing of advisory display gates.

## 2026-09-25 — docking headers must remain safe under full Windows include stack

A header-only docking helper used the identifier `near`. Narrow native tests
did not expose a problem, but the full Windows game translation units include
Windows headers before/around this code and reject that identifier. The helper
is now named `nearBoundary`, and the static docking contract guards against
the exact unsafe declaration returning.

This is a portability/build-boundary correction only. It does not change
corridor geometry, warning thresholds, route tracking, propulsion, or planner
semantics.

## 2026-09-25 — directional main failure doctrine now reaches live Newtonian motion

The directional-main architecture is now consistent across descriptor/runtime
health, navigation capability, physical maneuver compilation, hull-attitude
selection, and low-level Newtonian thrust execution.

When the aft/rear bank is unavailable but the fore/nose bank survives:
- the physical compiler may select fore main as primary and reverse the working
  hull direction;
- ShipController selects the corresponding attitude;
- DynamicMotionSystem ordinary Newtonian '+' drives the fore bank along
  `-forward`;
- Newtonian BrakeToStop uses the fore bank after nose-with-velocity alignment.

Healthy aft-main behavior remains preferred and unchanged. Newtonian '-' does
not become hardware-independent reverse thrust. This preserves the rule that
control doctrine consumes installed hardware rather than inventing symmetric
propulsion.

## 2026-09-25 — failed build invalidates subsequent stale CTest result

The local-flight contract source now depends explicitly on the canonical
`ShipDynamics.h` accessor for the linear acceleration envelope. A missing test
include caused the target build to fail before linking. CTest then executed the
old binary still present in the build tree, so the repeated old `maxGs` failure
is not evidence against the current main-priority/RCS-trim implementation.

Architecture/static checks for local flight and manual docking both pass. The
next valid evidence must come from a freshly linked local-flight contract
binary.

## 2026-09-24 — main thrust has priority over RCS inside the linear envelope

Directional main-bank health remains binary and full-power at the descriptor
level. The combined controller now also enforces the ship linear acceleration
envelope without weakening that rule: selected main acceleration is preserved,
and only the secondary manoeuvre/RCS vector is scaled when their vector sum
would exceed the allowed total acceleration.

This is distinct from engine-health derating. A live main bank retains its full
available rating; a failed bank has zero authority. RCS is a subordinate vector
inside the remaining load budget.

The manual-docking static check was also corrected: `corners[1]` is legitimate
inside the semantic bottom-marker geometry. Speed-label anchoring is checked
only in its own placement block and must use the stable projected-frame
upper-left helper.

## 2026-09-24 — propulsion hardware is descriptor/runtime truth, not control-law fiction

Cobra is no longer an aft-main-only vehicle. Its descriptor now declares real
aft and fore longitudinal main-engine banks with explicit ratings and module
bindings. Control law chooses how to use installed hardware; it does not create
or remove that hardware.

Effective propulsion is derived per ship instance from authoritative module
state. Main-bank availability is binary: full rated thrust while operational,
zero when failed. RCS remains an independent bounded actuator. This effective
profile is consumed by authoritative motion, Navigation runtime capability and
manual docking planning.

The physical maneuver compiler now accepts Assisted as well as Newtonian and
separates total body-axis authority from explicit main-bank authority. Normal
doctrine prefers the aft bank for primary propulsion. A fore-bank failure
therefore makes Assisted strong braking use the Newtonian-style flip-and-burn.
An aft-bank failure instead permits the fore bank to become primary, which
reverses the hull's working travel direction and is represented by an actual
attitude + burn trajectory rather than by symmetric fictitious acceleration.

The latest live docking evidence still predates this propulsion slice. Windows
build/test/gameplay acceptance is the next boundary; no target result for these
engine-failure cases exists yet.

## 2026-09-24 — manual docking has physical Assisted stop and corridor hysteresis

The manual docking commissioning path no longer assumes that Assisted can stop
a high-VREL Cobra with precision RCS alone. Vehicle hardware remains authoritative. The newer dual-main descriptor
supersedes the aft-only Cobra assumption recorded by this docking slice:
healthy Assisted now has physical fore-main braking, while the flip-to-aft
behavior remains the required fallback when reverse main is unavailable.

Manual corridor truth remains the same nominal geometric target, but route
lifetime now has recovery semantics. Warning starts at 80% of a nominal axis;
outside nominal is critical but not immediately fatal. The release envelope is
nominal plus max(25%,10 m) on lateral/vertical axes, with 0.35 s continuous
outside-release grace before cancellation.

Displayed frame extent is defined as `ship extent + 2 * center tolerance`.
Consequently the terminal frame equals the usable dock opening after required
wall clearance, while transit frames intentionally include maneuvering room.
Cockpit speed labels are rendered only at <=500 m and docking frames carry a
semantic bottom marker. Live `request=8` confirms requests are not globally
one-shot; each button action already owns a fresh serial.

## 2026-09-24 — current docking blocker is target provenance, not planner construction

The last playable result (brief route followed by bare
`dock moved off approach axis`) came from the pre-fix Windows binary at
`84d59f4d`. Route visibility proves that takeover, physical settling,
authoritative snapshot capture, planning and publication had already occurred
in that old run; disappearance was caused by the post-publication consistency
guard.

The corrected published baseline `512b917c` removes the split world/epoch
prediction from that guard: terminal gate, dock attachment and semantic port
are evaluated in one Hub-local coordinate owner, while map/HUD conversion is
presentation-only. Local focused regression reproduces the old orbital drift
and passes the replacement path. Live Windows acceptance remains open until a
rebuilt executable is positively identified and exercised.

## 2026-09-24 — public main now contains corrected docking tree

Public `forzub/Elite` `main` at verified commit `512b917c` contains the
Hub-local docking geometry, one-tick corridor validation, shared Hub-map/HUD
gate source, numeric axis logs, and JSON decimal-dot startup fix. The published
tree SHA matches the locally tested tree. Old notes about blocked remote
publication below describe earlier attempts only. Target Windows build and
in-game route persistence are not yet accepted.


## 2026-09-24 — GitHub publication pending explicit public-main authorization

Local `main` contains the corrected docking route and JSON numeric locale.
The public `forzub/Elite` branch was confirmed at `84d59f4d`; automatic
approval review blocked the direct push even after the user's request to put
code on GitHub. Until express public-main publication approval is received,
target `git pull` alone remains an old-code installation path. Do not suggest
patches again as the user explicitly declined them.


## 2026-09-24 — old binary explains repeated bare axis failures

The target checkout and runtime binary were inspected: both are based on
remote `84d59f4d`; the executable lacks the corrected numeric-axis marker.
The two previous bare failure excerpts cannot be used to evaluate the newer
Hub-local decision contract. Distribution/installation is the current gate;
the user's untracked trace artifacts are unrelated and must be preserved.


## 2026-09-24 — deployment identity precedes a fresh dock-model diagnosis

The repeated brief route and bare axis-failure excerpt contains no numeric
measurement from the corrected local implementation. The canonical build
script uses the invoking checkout's `build/EliteGame.exe`; the target's
checkout and binary identity remain unknown. Treat this as an unresolved
deployment-versus-runtime distinction until the complete combined log and
binary marker are inspected.


## 2026-09-24 — JSON numeric locale invariant

Process text locale may follow the OS, but JSON numeric conversion must always
use decimal point `.`. `main.cpp` now calls `core::useJsonNumericLocale()`
immediately after applying the user's `LC_ALL` and before creating the app or
threads. A focused C++ regression verifies fractional/exponent JSON parsing
under this boundary. Target startup confirmation remains open; the docking
route failure cannot be evaluated until the game remains running.


## 2026-09-24 — corrected branch not yet published

The corrected Hub-local docking source and documentation exist as local
commits beyond remote `84d59f4`. Automatic approval review rejected direct
publication to public `origin/main` despite the request to apply the patch in
the repository. Distribute the exact local commit series as a patch for the
Windows acceptance run; do not represent `git pull` alone as installing it.


## 2026-09-24 — live evidence must identify deployed source

The legacy remote docking guard can emit `dock moved off approach axis` alone.
The corrected local guard always emits a numeric `[DockAdvisory] axis` record
to the same stream first. A repeated failure without that record is evidence
for an old build only when output is complete and unfiltered; the user excerpt
does not establish that condition. Require binary/source verification before
attributing a fresh failure to the Hub-local contract.


## 2026-09-24 — docking frame and epoch contract

| Operation | Coordinate owner | Time owner |
| --- | --- | --- |
| Capture and plan | ship, port and obstacles in one tactical Hub-local snapshot | authoritative source tick/universe epoch |
| Track and guard | sampled ship and attachment, fixed local gates | one authoritative server tick/universe epoch per validation |
| Cockpit display | local gates converted through the player's render Hub frame | player render-frame universe epoch |
| Hub-map display | identical local gate list projected directly by Hub camera | fixed local geometry; no orbit/time conversion |

The target's authored local offset has zero translational velocity; its axial
spin changes orientation around the dock entrance axis. All dock-axis and
corridor comparisons now stay in tactical Hub-local coordinates. World
`GuidanceFrame`s are a generic display adapter only. Planning and flight
validation reject timeline/Hub mismatches; missing render frames postpone
publication without invalidating local route truth. Local regression and
architecture checks pass; Windows build and flight acceptance remain open.
The focused docking check passes; the entire architecture suite does not:
its Python portion reports nine pre-existing unrelated check failures and its
Windows toolchain prerequisites are unavailable here.

## 2026-09-24 — docking uses Hub-local route truth across time domains

The advisory planner consumes only tactical Hub-local meters at one captured
planning epoch. Its gates remain fixed Hub-local state; map projection,
cockpit projection and corridor tracking must never substitute a world pose
from another epoch. Runtime corridor tracking uses the ship's Hub-local
position. Cockpit gates use the player's render reference frame and its time;
map gates use current guidance frames and are projected against the map's
co-moving frame at each frame time. A numerical fixture now bounds disagreement
between the canonical orbital predictor and map co-moving predictor over 120 s.
The generic `GuidanceFrame.centerMeters` interface remains system-local world
meters at presentation boundaries; it is not a planner coordinate.


## 2026-09-24 — one Hub frame owns fixed docking gate and moving port

The terminal advisory gate remains fixed in Hub-local coordinates. At each
validation epoch, both that gate and the target module/semantic port derive
from `NavigationWorldPredictor::predictHubFrameAt`. Module pose comes from its
stable `HubAttachmentSnapshot`, not linear extrapolation of a sampled world
anchor. This is necessary even for a dock with zero authored local translation
because the Hub follows a curved orbit. Diagnostic axial spin does not displace
the entrance center or approach axis; an off-axis spin still fails the 2 m
consistency guard. Local focused regression passes; target game acceptance and
server-confirmed Human hand-back remain open.


## 2026-09-24 — docking live gate isolates post-publish dock prediction mismatch

The playable docking slice now reaches route publication in the real game. The
remaining observed failure is not takeover, braking, angular settling or route
construction; it is the terminal dock-axis consistency guard evaluated after a
valid advisory is active.

The fixed advisory endpoint is propagated through the predicted Hub frame while
the semantic port is independently propagated from a resolved world anchor.
Those prediction paths must be made identical in ownership/epoch semantics
before the live route can persist.


## 2026-09-24 — test integrity guard made extension-safe

The test-source integrity guard now distinguishes complete C/C++ extensions and
only resolves repository-owned CMake source forms. This prevents the guard
itself from inventing `.c` files from `.cpp` registrations or treating
external include probes as local sources.


## 2026-09-24 — test ownership normalized to current navigation architecture

Navigation testing is no longer allowed to compile or assert the deleted
DockingPathPlanner/GuidanceTunnel stack. Current test ownership is split across:
- architecture contracts for boundaries/absence of retired code;
- navigation_runtime for current geometric, docking advisory, maneuver and
  planner/follower behavior;
- dedicated navigation_map/local/space/trajectory/ruckig suites for their
  respective kernels.

The former `tests/navigation_guidance` all-in-one suite is retired. Its useful
GeometricPathPlanner behavior coverage is preserved as a focused runtime test.
A repository-level test-source integrity check now guards CMake/runner paths.


## 2026-09-24 — local flight parameter ownership remains centralized

The target gate exposed a stale architecture assertion, not a physics defect.
Raw ship-profile interpretation for local speed and main-engine authority lives
in `ShipDynamics.h`; `DynamicMotionSystem` consumes typed accessors. The
architecture check now enforces this split rather than requiring duplicated raw
`ShipParams` reads in the motion kernel.


## 2026-09-24 — authority replication normalized through one server helper

Per-session Autopilot authority publication is now owned by one
`GameServer::controlledEntityAutopilotActiveForSession()` helper. Full,
hydrated and sparse session snapshots consume that same source, eliminating
copy-path drift and the duplicate-declaration patch error found during review.


## 2026-09-24 — per-session authority acknowledgement closes prediction race

Manual docking authority is now observable in replicated session state instead
of inferred from command timing. The session snapshot carries whether the
controlled entity is currently under Autopilot authority; all production
snapshot paths populate it and data-plane schema version 9 serializes it.

The client may optimistically suppress its own prediction at SHOW ROUTE, but it
will not plan until server takeover is observed and will not resume prediction
until server hand-back is observed. This preserves the physical stop/500 m guide
implementation while removing the speculative one-tick Human/Autopilot overlap.


## 2026-09-24 — temporary authority invariant receives native test

A dedicated ControlRegistry contract now verifies that docking preparation may
change only controller kind Human <-> Autopilot. PlayerId -> EntityId ownership
identity must remain stable, foreign players cannot steal/restore that authority,
and unrelated player bindings remain unchanged. Target execution is pending.


## 2026-09-24 — playable manual docking preparation wired

The manual docking commissioning seam is now code rather than only a contract:
Human -> temporary Autopilot -> physical Hub-relative stop/angular settle ->
authoritative snapshot -> advisory planning/publication -> Human. It reuses the
existing ship control/physics path and does not introduce a second motion model.

Presentation now uses 500 m nominal spatial gates, per-gate speed at the
projected upper-left, and a unified-localization blinking manual docking mode
status. Old rolling-tunnel architecture checks are retired/replaced.

Acceptance remains open until the Windows game demonstrates the complete
sequence and both reset conditions.


## 2026-09-24 — manual docking commissioning boundary fixed

The next playable docking milestone is manual advisory commissioning, not
automatic docking. SHOW ROUTE temporarily borrows authoritative ship control
only to establish a deterministic physical start state. Navigation then plans
from the newly captured state, publishes a fixed route/tunnel product, and
returns authority to the human pilot before manual tunnel flight begins.

The presentation contract is now explicit: Hub Map planned trajectory; cockpit
spatial gates at 500 m nominal spacing; recommended speed at every gate's
upper-left; localized blinking MANUAL DOCKING MODE status while the manual
advisory is active. Closing the dock card or leaving the entered tunnel cancels
the whole advisory product. The latter is a corridor-volume condition, not
merely distance from a polyline.

This requirement intentionally uses the existing unified localization service
and existing server-authoritative ship-control physics. It must not revive the
removed client rolling tunnel or create a second localization path.


## 2026-09-24 — docking preparation and corridor contract

The user requires both dock-card modes to stabilize the craft before fixing
the initial planning state. Manual advice chooses a stop relative to the Hub;
an authoritative bounded maneuver must capture pose, velocity and angular
rate before static route search. A corridor exit is meaningful only after
entry into a published, physically clear swept-hull center envelope. Existing
client-only SHOW ROUTE does neither; server Autopilot authority and program
ownership are not yet wired. The 60 m transit width remains provisional and
must not be presented as collision-proof. No target result is claimed for the
new contract.

## 2026-09-24 — first live Hub-map docking gate failed

Windows game logs show advisory requests 1–6 cancelling with `ship left
guidance corridor` before route display. The corridor check mistakenly used
the tight port-opening margin along the whole flight and enforced exit before
initial entry. The revised advisory has separate transit/terminal cross
sections and an entry-before-exit tracking state; the map prefers the selected
dock route over unrelated guidance. Local checks and target re-test are
distinct: visibility in game remains unaccepted until the next run.

## 2026-09-24 — isolated dock route native check

The new advisory route passed a local native test with station and two docks,
including a yawed far spinning dock; 45-second port position/orientation
prediction agrees with authored local-Z rotation. Simulation and prediction now
use the same yaw-aware angular velocity. The diagnostic flight scene excludes
the unrelated Stage-12 NPC; an NPC pilot-step compile blocker is corrected.
Target game build and live flight remain open, so neither advice in gameplay
nor automatic docking is accepted. Automatic docking still needs the separate
server-owned physical program and dispatcher/capture contracts.

## 2026-09-24 — legacy docking removed from game path

The diagnostic `HubDockingFlightTestScene` now leaves the station, Cobra and
both docks while removing the stress objects. The far dock is yawed 27 degrees
and spins at 2 degrees/s around its entrance axis. This is a fixture for a
future in-game acceptance test, not evidence of successful docking.

The dormant client-side docking route/tunnel pipeline and its planner and
builder were deleted. A fresh advisory route planner and two dock-card actions
are added; the automatic action fails closed while server docking authority and
physical program proof are missing. Native static geometry testing passes. The client build cannot be verified in
this environment because the `websocketpp` header is missing; live flight
acceptance is outstanding.


## 2026-09-24 — commissioned static navigation scope

A* is retained for a fixed A-to-B static scene of one craft or one group
envelope. Scheduled corridors/dispatch and online dynamic avoidance have
independent owners. The full accepted flight program is constructed and
proved separately; it is executed unchanged or cancelled. The A* polyline
alone is never a flyable instruction. Route provenance now includes vehicle
capability revision, and capped support-node search may no longer falsely
accept a route that intersects an omitted static obstacle. Worker scheduling,
physical compilation and full program acceptance remain open work.

## 2026-09-24 — throughput and traffic semantics separated

The original 500-request benchmarks are valid for independent route-compute
load but invalid for simultaneous traffic: starts/goals are distinct yet many
13 m-radius hull pairs overlap on their narrow shared lines. A new 500-lane
empty-space control spaces starts and finishes 30 m apart in 3D, with zero
overlap pairs. All 500 arrive in the final position region with correct body
orientation; all fail <=2 m/s terminal speed (median 19.214 m/s). This proves
the tested greedy B5 chain lacks terminal braking even in an adequately posed
separate-goal control. It does not condemn A* generally or establish any
alternative planner's runtime.

## 2026-09-24 — first full point-chain diagnostic rejects false B5 success

The 500-request fixture now chains exact B5 terminal samples into each next
waypoint query and checks sampled clearance over the full scene. Complete
terminal state (8 m position, <=2 m/s speed, +X hull within 10 degrees):
**0/500** in empty space, **0/500** in the forced dogleg and **0/500** with 100
barrels, despite 500/500 unproved first primitives in each case. Empty space
reaches final position 500/500 but all miss final speed, 448 miss attitude.
Dogleg/barrel position capture is 25/500 and 13/500; sampled contact rejects
178 and 406 respectively.

The diagnostic is bounded and greedy; its failures are not proofs of global
infeasibility. No physical-alternative planner or complete cost comparison
exists yet. The replacement must own corridor transitions, backward terminal
admissibility and exact actuator/hull proof before its runtime is compared.

## 2026-09-23 — first scaling measurement separates geometry from physics

Correction: the original 5 m radius/15.33 s result was a fixture error and is
superseded. The measured Cobra geometric radius is 13 m. Corrected 100-barrel
scene: 500 routes in 17.075 s; first B5 primitive accepted as **unproved** for
all 500. The added four-wall dogleg: 1,000 bends above 45 degrees, 153 above
90 degrees, 500/500 initial unproved candidates. The physical compiler sees
only the first primitive, so neither result establishes any complete feasible
flight or compares the proposed physical route search against geometry-first.

The production geometric A* baseline has been measured on a deterministic
100-obstacle / 500-ship local fixture: 15.33 s sequentially, 30.03 ms per
route p50 and 57.75 ms p95 with the default 32-obstacle consideration cap.
All 500 paths were clear against the full fixture; no physical maneuver was
planned or flown. One all-100-obstacle request took 419.4 ms. The user's
concern about expensive repeated geometric-first searches is therefore
measurable, but no alternative is declared faster until it solves this exact
workload with the same success/safety obligations.

## 2026-09-23 — M4 spatial eligibility introduced

B5 now treats target position as a real eligibility constraint: velocity-only
progress that misses the requested capture region returns a typed spatial
witness. The bounded coordinator can negotiate a new terminal point without
changing mission ownership. Two focused native tests passed; target MinGW and
visual integration evidence are still needed. This slice is deliberately
observer-only and does not establish corridor capture, chained state, B6
continuous proof or accepted-program publication.

## 2026-09-23 — visual evidence rejects initial-only physical success as route evidence

The target viewer run of remote commit `15f4c6c6f856cc9cf7974ef1527730315807c880`
shows both products at once: an honest unproved initial rotate/burn candidate
and a legacy route execution that fails at the first high-speed corner.

At 20.60 m/s the route widens geometrically, but geometry alone cannot provide
the lateral acceleration. The legacy program contains 50/801 infeasible
actuator intervals; tracking is invalidated at 5.33 s with no collision. This
confirms that speed-dependent clearance is not a substitute for kinodynamic
planning.

The observer is presently too local to validate a route. Its target position is
not part of the compiler's dynamic solve, and the single candidate does not
propagate into the next corridor transition. Project work therefore remains in
M4: construct a spatially bound receding-horizon physical chain before starting
B6 proof or any replacement execution.

## 2026-09-23 — observer-only physical search reaches trace and viewer

The first integration-scale inspection seam is implemented. Runtime composition
adapts scenario state and canonical ship capability into the already accepted
pure coordinator, then copies results into a presentation DTO. Ranked horizon
alternatives retain provenance and typed infeasibility; candidate samples retain
the mandatory continuous-proof flag.

The DTO is serialized in navigation trace JSON and rendered independently of
the legacy reference, accepted-program corridor and actual flight. Viewer colors
and its explicit `НЕ ПРИНЯТО` title prevent an unproved B5 candidate from being
presented as executable navigation. Observer code has no steering authority.

Target compilation and visual inspection remain pending. The local environment
has neither CMake nor GLM, so this entry claims implemented source/dataflow, not
a compiled or visually accepted result.

## 2026-09-23 — visual integration promoted ahead of deeper planner work

The corrected coordinator fixture passed on exact commit
`b506397ca30f223ee6cb29597c8673e0815626d1` (1/1, 0.04 s).

Project policy now treats isolated green tests as necessary but insufficient.
Before literal execution or accepted-program publication, the replacement path
must be visible in observer-only mode beside the legacy path. Candidate,
rejected, proved, accepted and actual motion receive distinct trace products and
rendering. This is intended to expose integration-scale nonsense early, where a
unit test cannot politely hide it.

## 2026-09-23 — coordinator failure is a test-physics error, not search failure

Commit `3492ca3ba314dcf250c5d3ebc03c6e8cc0c3dce6` compiled on
MinGW64. The coordinator test supplied 4.0 s as a feasible retry horizon, while
its 135-degree quintic rotation requires about 4.60 s before burn. Both options
were therefore correctly rejected. The fixture is corrected to 6.0 s and needs
retest; production physics is unchanged.

Visual inspection of the replacement path is not yet authoritative. The
existing viewer follows legacy execution. The planned observer seam will first
show search alternatives/witnesses, then rigid-body phases and literal thrust;
proved/accepted motion must use separate rendering from unproved candidates.

## 2026-09-23 — bounded physical search becomes an explicit product

The typed physical compiler contract is accepted on MinGW64 at exact commit
`9af337c2e23a32d5f11d34a3e048ecd98842674d` (2/2 focused tests pass).

The next replacement seam is implemented as a pure bounded coordinator. Route
and goal owners provide a ranked, revisioned frontier; the coordinator neither
invents doctrine nor edits capability. It advances through explicit terminal,
speed and horizon alternatives under a caller-owned attempt budget and returns
a persistent cursor. Exhausting one frontier keeps the objective live and asks
for a new frontier/safe fallback rather than disabling navigation. Objective
and frontier revisions are separate: rebuilding route alternatives does not
change the NPC's mission goal.

This candidate is not yet wired to the legacy runtime author and does not claim
collision proof or accepted-program publication. Its focused C++ test still
requires target execution.

## 2026-09-23 — legacy physical-authoring preservation is no longer required

The owner explicitly authorized replacement of legacy blocks instead of
behavior-preserving surgery. Active work therefore moves from the remaining M2
monolith cleanup to an M3/M4 vertical slice: truthful physical-solve output,
bounded alternative negotiation, exact proof, then acceptance.

The first API correction adds a typed physical infeasibility witness. It tells
the coordinator whether the request lacks translation authority, attitude
authority, program horizon, a supported control law or a valid input/body
frame. This is the minimum feedback required to vary route/speed/terminal/time
parameters without disabling navigation.

The candidate additionally closes two physical-compiler truth defects:
already-aligned burns can proceed without angular authority, while a non-zero
initial angular velocity is rejected with its own witness until a candidate
family models it instead of silently replacing it with a fixed attitude.

## 2026-09-23 — planning is bounded negotiation, not fail-and-disable

Mission/behavior supplies intent and acceptable terminal conditions; global
planning supplies alternative corridors; physical planning supplies a proved
maneuver or typed rejection witness. The coordinator continues search by
varying legal spatial and temporal parameters.

An exhausted solve keeps navigation active. Safe-progress/hold programs and
replanning remain available. When collision is unavoidable, an explicitly
contact-predicted, damage-minimizing emergency program replaces both silent
shutdown and knowingly infeasible nominal motion.

## 2026-09-23 — M2 target runtime evidence preserves the split and re-exposes M3/M4

The corrected M2 slice reaches the real runtime pipeline and preserves the M1
frame invariant. The only reported E2E stop is the known high-speed Newtonian
case: 34/753 actuator intervals are infeasible and Follower invalidates the
unreachable accepted reference after 0.51 s.

This evidence reinforces the target architectural invariant: translational
timing cannot precede body-attitude and actuator feasibility for a
main-engine-dominant Newtonian vehicle. Aft-only main propulsion means strong
braking may require a hull flip before the burn; RCS authority cannot be treated
as an omnidirectional substitute.

M3 owns truthful rejection of infeasible accepted programs. M4 owns physical
short-horizon maneuver compilation before final time parameterization. The M2
composition-root split must remain behavior-preserving and must not tune around
this failure.

The supplied target excerpt does not contain the tested checkout hash, therefore
no exact verified SHA is attached to this evidence entry.


## 2026-09-23 — M2 slice 1 first target compile failure

The first target-machine validation of the scenario-I/O split did not reach
runtime behavior. Architecture, nominal Stage-1 and follower component gates
passed; compilation then failed because `normalizedOr()` and
`basisFromForwardUp()` were accidentally moved into the private
`NavigationScenarioIo.cpp` implementation while
`NavigationScenarioRuntime.cpp` still used them.

The correction moves those pure shared helpers to
`NavigationScenarioMath.h` and includes that header from both owners. This
preserves the intended I/O boundary while restoring the runtime dependency
explicitly. M2 slice 1 remains pending target compile/link/E2E evidence.


## 2026-09-23 — migration gate M1 closed, M2 opened

Target evidence accepts the canonical NavLocal/System frame boundary and the
single maneuver-page timeline. Both coordinate fixtures completed all storage
pages and emitted the independent frame-equivalence PASS marker.

The remaining pipeline failure is deliberately not attributed to M1: the
high-speed Newtonian program contains 34 infeasible actuator intervals and is
invalidated after 0.51 s of tracking loss. Physical acceptance remains RED for
M3/M4.

Active work is M2: split scenario I/O, route composition, maneuver/proof
composition, execution harness and trace adapters out of
`NavigationScenarioRuntime.cpp` without changing observed behavior. The first
slice now has a code candidate: scenario JSON/file parsing is owned by
`NavigationScenarioIo`, exposed through a separate header and linked as
`EliteNavigationScenarioToolIo`. Static contracts pass; target compile/runtime
evidence is pending.

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Evidence boundary

Accepted baseline:
```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest target-tested checkout:
```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Current unverified code baseline before documentation sync:
```
abfd7a6a26168f177968f0dd4299f3c712105fc0
```

## B4 current architecture

Ordinary local avoidance is receding-horizon and command-continuous.

A local solve is responsible for the **next safe executable geometric segment**,
not for completing an artificial leave-route-and-return loop inside one horizon.

```text
nominal route
 -> bounded physical horizon
 -> projected/static conflict
 -> safe short offset segment if available
 -> execute
 -> refresh from actual state
 -> continue offset or begin route reacquisition
```

If no safe short offset exists, navigation remains active and returns braking
intent. It is not disabled and it does not surrender ownership.

The route is reacquired progressively. No fixed distance, including 30 m, is a
required merge point.

## Removed/superseded behavior

Still forbidden:
- angular deflection fan;
- azimuth branch search;
- persistent left/right branch continuity;
- branch-switch API;
- mandatory Brake-before-changing-side recovery.

Also superseded:
- mandatory same-horizon current->bypass->merge proof;
- treating the current bounded nominal endpoint as a compulsory return point.

## Current milestone

Target-machine validation of the corrected receding-horizon B4 behavior.

The final composite must distinguish:
- geometry can evade -> continue through proved local free space;
- geometry cannot evade -> braking command while navigation continues;
- physical compiler cannot execute geometric bypass -> higher maneuver
  ownership brakes/replans rather than accepting an impossible maneuver.

## Other block status

Accepted/strong:
- B0 snapshot ownership;
- B7 maneuver decision mechanics;
- B8 accepted program;
- B9 sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 propulsion/physics;
- B14 scheduler mechanics.

Still incomplete/transitional:
- B1 shared influence builder;
- B2 unified objective;
- B3 coarse vehicle-aware topology feasibility;
- B4 receding-horizon local bypass under target validation;
- B5 general production maneuver compiler;
- B6 generalized continuous proof;
- B11 explicit safety/reflex monitor;
- ordinary-live migration of accepted-program execution.

## State protocol

After every state/evidence change synchronize project state MDs, active Stage-12,
and recreate `CONTINUE_PROMPT.md` from scratch.

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
- `` FAIL:
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

## 2026-09-22 — Stage-12 execution recovery correction

The physical runtime exposed a failure in the first reference-clock-hold
implementation under Assisted / Expert / Standard at 10 -> 10 m/s.

Planner evidence remained healthy (Ruckig max about 11.71 m/s), while physical
execution escaped to 100.64 m/s and 2.4 km terminal error. The reference clock
was held for 47.70 s.

Architecture correction:
- a held moving reference sample is not allowed to keep moving-reference
  derivatives authoritative;
- outside the B10 execution envelope, recovery uses the fixed geometric
  reference with zero feed-forward and damps actual angular rate;
- bounded follower feedback remains the only recovery authority;
- accepted moving derivatives resume only after physical reacquisition.

This prevents a reference hold from becoming an indefinite accelerator/spin
command without mutating `AcceptedManeuverProgram`.

Diagnostic viewer correction:
camera fit is based on authored/accepted scene geometry rather than the complete
physical execution history, so a runaway trace cannot destroy scene scale.

Candidate baseline before docs: `59ff756996229bf15a122eb0fe43cf0d9a14b245`.
Target-machine acceptance is pending.

### 2026-09-22 — FreeTransit sparse derivative consistency

Stage-12 FreeTransit attitude authoring now treats the accepted sparse basis
sequence as the source of truth for angular motion. Dense-source instantaneous
angular velocity is no longer copied into a <=16-sample program because that
creates temporal aliasing when B9 interpolates the sparse product.

Interior sparse angular velocity is re-derived from neighboring accepted
basis/time samples, endpoints are zero-rate, and angular acceleration
feed-forward remains zero. This complements out-of-envelope derivative
neutralization in B10.

Exact Assisted 10 -> 10 physical-runaway E2E regression is now pinned.

Candidate baseline before docs: `13ef6bd731ef6d8e78c75c72bf7a59f524b30bcb`.
Acceptance remains target-machine pending.

## 2026-09-22 — propulsion truth unified across control laws

The Stage-12 physical runtime exposed an Assisted-only hidden propulsion model:
negative longitudinal navigation demand was implemented as symmetric fore/nose
main thrust. That allowed velocity to brake and reverse while the hull remained
pointed along the accepted route.

Architecture is now corrected for navigation execution:
- propulsion hardware is authoritative and independent of control law;
- at the 2026-09-22 baseline the Cobra model was aft-only in both laws
  (superseded by the 2026-09-24 dual-main descriptor);
- bounded RCS is the only translation authority available against hull forward
  before a physical hull rotation;
- Assisted reference-attitude authoring is propulsion-aware, so a substantial
  acceleration/braking vector can demand a real cant/flip before main burn.

This directly enforces the invariant:
`no hull rotation + no physical thrust => no arbitrary velocity-vector change`.

Pinned E2E: Assisted / Expert / Standard, 20.90 -> 20.00 m/s.

Candidate before docs: `b833eddb7bd04b5c025b2be0fd8334c33f8824e6`.
Target evidence pending.

## 2026-09-22 — Newtonian maneuver doctrine separated from Assisted RCS use

The higher-speed static viewer gate proved that Newtonian navigation was
physically using the Cobra's full 2.0 m/s^2 manoeuvre-thruster envelope as
ordinary sustained route propulsion. Since the Ruckig curve requested about
1.5 m/s^2, the attitude author kept the hull on the travel tangent and RCS did
the maneuver almost alone.

This was physically integrated but architecturally wrong for a
main-engine-dominant Cobra: Newtonian is supposed to preserve inertial motion,
lead-rotate, then use aft-main delta-v with RCS as trim.

Current architecture:
- physical RCS maximum remains hardware truth;
- maneuver doctrine separately controls how much RCS may justify *not* rotating
  the hull;
- current Cobra Newtonian attitude author uses the existing 0.35 m/s^2
  precision threshold for that decision;
- Assisted may still use the full real RCS authority for its own coupled-flight
  doctrine;
- at that baseline navigation main propulsion was aft-only; this is historical
  and superseded by the descriptor/runtime dual-main model.

Viewer now includes always-visible rear-main / fore-main / manoeuvre-thruster
lamps driven by physical acceleration channels.

Candidate before docs: `421ecb1b2721d54bc0c33737a520100a3c10fac9`.
Target acceptance pending.

## 2026-09-22 — trajectory architecture clarification

The navigation stack currently has a geometry/timing inversion for high-speed
Newtonian flight.

Multi-point routes are implemented as:
`coarse polyline -> Bezier execution guide -> scalar Ruckig s(t) -> attitude`.

This is useful for simple kinematic path following, but it does not make Ruckig
a full ship maneuver solver. Ruckig sees path progress and a symmetric scalar
acceleration budget; it cannot account for finite hull rotation or aft-main
pointing before braking.

The next architectural step is to move physical maneuver compilation ahead of
final timing:
- real Cobra profile;
- propulsion allocation;
- hull rotation reachability;
- braking/turn lead distance;
- physically sized arc/transition geometry;
- then Ruckig timing.

The current hard-coded `cobraParams()` duplicate is now explicitly considered
technical debt and should be removed.

No further follower tuning should be treated as a solution to this issue.

## 2026-09-22 — Ruckig demoted to inner solver; follower remains actuator layer

Navigation responsibility is now explicitly:

```text
route topology -> physical maneuver compiler -> Ruckig inner solve
              -> AcceptedManeuverProgram -> follower/autopilot -> physics
```

The physical maneuver compiler is authoritative for geometry, hull attitude and
engine allocation. Ruckig is not an autopilot and is not a global path planner.

The prior assumption in execution-guide sizing that normal acceleration is
limited to `manoeuvreThrusterAccel` is now considered architecturally wrong
for a main-engine-dominant Newtonian ship. Required turn acceleration may come
from combined aft-main + RCS after finite hull rotation.

Viewer diagnostics now distinguish instantaneous accepted reference (pink cross)
from active-phase endpoint (violet cross).

Current scenario terminal state is oriented and moving: finish forward/up are
required and non-zero finish speed defines terminal velocity direction.
Therefore a free-space high-speed turn may remain a constant-speed broad arc;
braking is conditional, not mandatory.

## 2026-09-22 — navigation ownership renamed and fixed

The navigation architecture now uses the following names literally:

**Planner** — decides the physical maneuver.

**ManeuverProgram** — time-ordered physical states plus propulsion/attitude
commands for each interval.

**Ruckig** — internal numerical trajectory/timing helper used by Planner.

**Autopilot/Follower** — executes the accepted ManeuverProgram and monitors
unexpected hazards.

This removes the previous ambiguous split where Planner emitted geometry,
Ruckig emitted kinematics, and Follower was expected to infer how a real ship
should realize them.

A physical maneuver is accepted only if integrating real hull attitude and
available propulsion can reach the next planned state.

## 2026-09-22 — ManeuverProgram now carries explicit propulsion intervals

Navigation now has a concrete representation of the agreed ownership model.

`AcceptedManeuverProgram` no longer consists only of reference states and
abstract acceleration feed-forward. It also carries fixed-capacity actuator
intervals between states.

Planner-side interval data includes rear-main throttle, explicit fore-main
channel, manoeuvre/RCS vector and a physical-feasibility witness.

At the 2026-09-22 baseline the Cobra compiler kept fore main disabled because
the descriptor did not install one. This historical safeguard is superseded by
the 2026-09-24 descriptor, which installs a real fore bank and requires B5 to
distinguish its explicit authority from RCS.

B9 samples the command interval; Follower exposes it; trace/viewer show planned
propulsion separately from actual propulsion.

Execution is intentionally not switched yet. This is an observation gate before
literal actuator execution, so bad Planner authoring can be diagnosed without
confusing it with an Autopilot bug.

Next accepted transition:
`sampled ActuatorSegment + bounded tracking correction -> real propulsion`.

## 2026-09-22 — sparse accepted-program alias identified behind RCS-only execution

The course-oscillation correction did not itself change propulsion allocation.
It exposed a deeper representation defect.

Long Ruckig trajectories were being compressed to <=16 accepted samples per
route leg. Linear state/feed-forward and attitude were therefore interpolated
across intervals much larger than the original physical trajectory sampling.

This allowed an accepted program to contain little/zero feed-forward
acceleration while its position/velocity reference still curved. Follower
generated lateral recovery acceleration, and the downstream net-vector allocator
correctly—but undesirably—assigned that recovery mostly to RCS.

The accepted-program representation now preserves consecutive dense source
states by splitting long legs into multiple <=16-sample chunks.

Invariant:
`sum(actuatorSegmentCount) == denseTrajectorySamples - 1`.

Planner actuator commands remain observe-only until this corrected source
program is target-validated.

## 2026-09-22 — architecture audit supersedes follower tuning

Dense trajectory coverage is now correct, but current execution still fails
because representation and timing semantics are wrong.

Accepted fixed-capacity chunks are storage pages of one continuous maneuver,
not independent maneuver phases. Current runtime incorrectly gates them as
phases and freezes their clock on tracking-envelope loss. This can deadlock on
Program 0 and turn recovery into return-to-stale-reference behavior.

A second hard defect is attitude reachability: reference orientation is limited
by angular speed but not by angular acceleration, so a zero-rate ship may be
asked to acquire near-max angular rate in one sample.

A vehicle-dynamics SSOT audit also failed. Authoritative descriptor,
`cobraParams()`, NavigationVehicleProfile, CapabilitySnapshot and manual
Assisted propulsion semantics are not one consistent truth.

Project repair order is now:
continuous time/page semantics -> invalidation/replan instead of indefinite
freeze -> angular reachability -> vehicle dynamics SSOT -> physical maneuver
compiler -> direct actuator execution.

Stage-1 build success is explicitly not Stage-2 E2E evidence.

## 2026-09-22 — navigation input ownership normalized

Navigation no longer owns a ship-specific parameter copy.

The application supplies a generic `VehicleDynamicsProfile`; all downstream
vehicle capability views are derived through common adapters.

A separate explicit `ScenarioNavigationPolicy` contains the diagnostic
stand's calculation policy. Vehicle facts and navigation policy are therefore
different API domains.

The continuous maneuver timebase is now independent from fixed-capacity storage
pages, and stale-reference freezing has been replaced with bounded invalidation.

Reference attitude authoring now respects angular acceleration in addition to
angular rate.

Remaining architectural blocker:
physical maneuver geometry/propulsion proof is still downstream of the current
geometric path + scalar Ruckig timing and must be moved ahead of final timing.

## 2026-09-22 target gate exposes physical timing mismatch

API/SSOT/clock cleanup compiles and the Stage-1/component gates pass.

The Stage-2 E2E now fails for two separate reasons:
1. stale regression code still expects removed REFERENCE CLOCK HOLD diagnostics;
2. the real accepted trajectory contains physically infeasible actuator intervals
   and becomes invalid after 0.51 s of tracking loss.

The second failure is useful: monotonic-clock + invalidation semantics are no
longer masking planner/authoring defects by homing to stale references.

Current blocker is now precisely the planned order mismatch:

```text
scalar trajectory timing first
 -> attitude / actuator fit afterwards
```

must become:

```text
physical geometry + attitude + actuator feasibility
 -> timing
```

## 2026-09-22 — strict API/purity contract strengthened

A second API audit found that "explicit top-level inputs" was not enough:
private helpers were still accepting oversized context objects or carrying
hidden behavioral thresholds.

The project now distinguishes:
- pure calculation kernels;
- deterministic stateful executors;
- orchestration/composition boundaries.

Calculation helpers must receive exactly the values they consume. They may not
reach into Scenario/Settings/ShipDescriptor/GameSimulation/global time/I/O for
missing inputs.

Behavioral thresholds that affect accepted motion belong to an explicit policy
object. Only numerical epsilons and mathematical identities may remain private
implementation constants.

A dedicated `NAVIGATION_API_CONTRACT.md` and expanded static purity checker now
pin this architecture.

Remaining major RED item is unchanged: the physical maneuver must be authored
and proved before final Ruckig timing. API cleanup is a prerequisite, not the
physics fix itself.

## 2026-09-22 — normative navigation-layer blueprint

`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md` is now the
primary target architecture for navigation work. It records the full function
and ownership audit of the active runtime, the scalable 3D navigation-world
model, hierarchical corridor search, physical local-maneuver generation,
continuous swept-hull proof, literal actuator execution, scheduler/LOD model,
public API shapes, revision semantics, tests and an M0-M13 migration.

Key project decision:

```text
static generated 3D topology + exact static BVH + dynamic spatial index
    -> async/shared global corridor
    -> receding-horizon physical maneuver candidates
    -> capability/resource/continuous collision proof
    -> immutable accepted state + actuator program
    -> literal execution with bounded reserved correction
```

The existing `NavigationSpace`, `NavigationMap`, scheduler, dynamics profiles
and accepted-program value objects are foundations to evolve, not proof that
this target already exists. `NavigationScenarioRuntime.cpp`, the visibility
graph and scalar Ruckig path remain transitional.

Migration is at M0 complete by audit/documentation and M1 pending. M1 repairs
the canonical frame/API boundary and semantic architecture tests. No later
stage may claim completion before its predecessor's evidence gate passes.

## 2026-09-23 — M1 implementation candidate

The first migration stage is implemented but not accepted.

The runtime now has one explicit coordinate seam:

```text
NavLocal planner/Follower state
    -> NavigationFrameBoundary
    -> System control/physics state
    -> NavigationFrameBoundary
    -> NavLocal observation/trace/terminal evaluation
```

Initial rigid-body state uses the same boundary. The execution composition
receives a value-owned `KinematicFrame` snapshot and epoch; the harness advances
the frame during fixed-step execution instead of freezing a moving frame at its
initial pose.

Execution deadline comparison now uses the same absolute universe-time domain
as `ExecutionVehicle::timeSeconds`; the prior code silently assumed epoch zero.

Four local architecture gates pass. C++ compilation and the new non-identity
product-chain E2E still require the Windows MSYS2/MinGW64 target. M2 is not
active until that evidence is recorded.

## 2026-09-23 — first M1 target gate classified and corrected

The first supplied MinGW64 attempt did not compile the M1 test executable. A
retained E2E fixture wrote the already-removed `ScenarioRunSettings::pilot`
field even though it also supplied the correct resolved
`pilotExecutionProfile`. The dead assignment is removed and the purity gate now
forbids that API regression.

The `ctest` output following the compiler failure came from a stale executable:
it contains assertion text absent from the current source and lacks the new
non-identity-frame marker. It is not valid M1 runtime evidence. Its physical
failure remains consistent with the known later-stage blocker—42 infeasible
actuator segments followed by tracking invalidation—but does not change the M1
frame-boundary decision.

Build and test must now be chained so a compile failure cannot fall through to
an old executable. M1 remains pending; M2 has not started.

## 2026-09-23 — second M1 target compile blocker removed

The next MinGW64 run passed all architecture checks, Stage-1 nominal routing and
the maneuver-tracking-controller test. It also compiled the corrected E2E
translation unit, confirming removal of the stale `ScenarioRunSettings::pilot`
use.

Compilation subsequently exposed incomplete earlier cleanup inside
`TrajectoryGenerator.cpp`: implementation code still referenced the removed
wall-clock timing diagnostic, and the removed perf logger had left behind an
unused blended-waypoint counter whose signature later changed. Those dead
references are now removed. An unused arc-table parameter was also removed from
`globalGuideSpeedLimit()` to preserve narrow calculation APIs.

This iteration changes no planning semantics. The pipeline has still not linked
or executed the non-identity product-chain fixture. M1 remains pending and M2
has not started.

## 2026-09-23 — third M1 target gate exposed duplicate page-time ownership

Target build and link now pass. The identity and non-identity runs agree across
all printed planning and NavLocal execution metrics, but runtime advanced to
storage page 1 a floating-point instant before that page's own canonical start.
Sampler failed closed with `PROGRAM_PAGE_BEFORE_START` at 50.30 s.

A new pure `ManeuverProgramTimeline` is the single owner of page windows,
page-local elapsed time and active-page selection. It is consumed by runtime,
Sampler, Follower and phase gate. The known Follower completion omission of
`sequenceStartOffsetSeconds` is corrected at the same boundary.

The M1 E2E now proves frame invariance by comparing the entire identity and
non-identity NavLocal trace plus terminal outcome. It does not require the known
78-infeasible-segment physical plan to complete; that would make M1 depend on
M3/M4. M1 remains pending until this candidate runs on target.

## 2026-09-23 — navigation map and execution-risk contract expanded

New product requirements are now normative in the blueprint and related API/
control-law contracts:

- octree/voxel free space and merged regions must be conservative certified
  subsets; a region AABB is broadphase only;
- clearance is a guaranteed lower bound, not an optimistic sample;
- sphere fit is a fast sufficient test, while orientation-aware configuration
  search preserves genuinely traversable slits and portals;
- portals carry aperture, normal, depth, orientation, velocity and
  capture/transit/release semantics;
- primary doctrine is `STANDARD`/`EXTREME`; Squeeze is an orthogonal constrained
  passage profile;
- nominal physical feasibility, robustness/risk and pilot execution error are
  separate products;
- poor pilot contact on a correct nominal route is execution-envelope departure,
  not automatically a map/planner defect.

No active M1 code changed in this requirements iteration. Its focused sampler
and pipeline target gate remains next.

## 2026-09-27 — flight law is becoming a ship-family capability

Design direction is now two materially different local-flight families rather
than treating Assisted/Newtonian as two cosmetic input mappings over one
navigation algorithm.

### Assisted family

Intended character:
- comparatively maneuverable;
- somewhat lower top speed;
- hull nose defines desired travel direction;
- finite hull angular rate/acceleration still applies;
- automatic game-flight stabilization bends velocity toward the nose;
- forward/reverse main propulsion controls longitudinal speed;
- physical manoeuvre/RCS is precision authority, not ordinary route curvature.

Canonical Automatic execution now reuses the same game-flight law as manual
Assisted. Planner capability projection uses the Assisted stabilizer budget
rather than `manoeuvreThrusterAccel` for course-change timing.

### Newtonian family

Intended character:
- faster/heavier/less maneuverable craft, including barge-like hulls;
- velocity and attitude are independent;
- ordinary navigation uses coast + hull rotation + primary main-engine burn;
- braking is normally rotate/flip then main burn;
- routes and parking should favor long, nearly straight legs rather than many
  aircraft-like arcs;
- final placement is slower and may use stronger class-specific manoeuvre
  thrusters or external tug logic.

Ordinary Newtonian route execution is now forbidden from consuming precision
RCS as synthetic lateral main thrust. The dedicated stop/rotate/burn compiler
remains to be completed.

The current runtime control-law switch is retained temporarily for development
and regression work. The architecture no longer depends on the switch being a
player preference; a later descriptor-level ship-family lock can select the
same two branches without redesigning Planner/Follower.

## 2026-09-27 — angular terminal state is a backwards-reachable boundary

Angular trajectory generation now treats requested terminal angular velocity as
a true boundary condition throughout the remaining maneuver time, not as a
value to be imposed on the last sample.

For maximum angular acceleration `alpha_max`, every authored state must remain
inside:
```text
|omega(t) - omega_terminal| <= alpha_max * (T - t)
```

This is independent of Assisted/Newtonian doctrine. Both ship families require
physically reachable body attitude and angular velocity programs.

## 2026-09-27 — Automatic docking owns its route-preflight UX

START DOCKING is now a complete user operation. A pre-existing manual guidance
route is an optimization/presentation reuse case, not a prerequisite.

When no matching visible corridor exists, the client orchestration uses the
same authoritative preparation protocol as CALCULATE TRAJECTORY:
stop -> settle -> authoritative Hub snapshot -> advisory route -> publish
corridor. It then returns temporary authority and starts server Automatic.

This does not move execution authority to the client. Server Automatic still
owns its own immutable planning inputs, collision proof, accepted program and
actuation. The preflight route is presentation/safe-start preparation.

Visible and executable Assisted docking now share these route assumptions:
- same actual hull-forward launch direction;
- same minimum/requested nose-first lead doctrine;
- same Assisted control-law capability interpretation rather than 2 m/s²
  precision RCS;
- denser 150 m advisory cadence around the route.

Longer-term, if server accepted geometry becomes materially different from the
preflight preview because of moving target prediction, the authoritative
accepted program/corridor should be replicated back for exact presentation.
That remains a presentation-consistency refinement, not permission for the
client preview to command physics.

## 2026-09-27 — diagnostic scoping correction

The self-contained Automatic route-preflight architecture is unchanged.
A compile-only regression from the diagnostic patch was fixed by scoping
DockPrep and DockAuto logging to their respective function APIs.

This correction does not alter control authority or flight behavior.

## 2026-09-27 — translation and attitude share one maneuver clock

A valid maneuver is no longer defined as "fastest translational trajectory,
then see whether attitude can keep up."

For executable multi-point navigation, linear motion and body attitude share one
physical time axis. If a collision-free geometric route can be flown
translationally faster than the bounded hull can satisfy an exact angular
boundary, Planner must first increase maneuver time by reducing translation
speed.

Only an angular-terminal infeasibility triggers this timing relaxation. Other
numerical, collision or path failures remain hard failures.

This preserves the architectural split:
- route geometry remains Planner-owned and collision-proved;
- ship angular limits remain descriptor/ShipDynamics-owned;
- the Accepted program remains the proof product;
- Follower does not repair an impossible Planner clock.

### Controller ownership

Player Automatic and NPC-style control should share the same controller concept:
the physical Ship does not change identity or become a different simulation
class. ControlRegistry selects the command source (Human, AI, Autopilot) while
ShipDynamics and fixed-step physics remain identical.

### Persistent docking UI contracts

Manual visible docking frames are locked at 500 m ordinary / 250 m terminal by
explicit user requirement.

Docking ports are semantic actionable subtargets and therefore have click
priority over their parent infrastructure, including at distant zoom.

## 2026-09-27 — docking phase ownership

Automatic docking is explicitly staged:

1. **ApproachHold** — navigation transit to a standoff hold point.
   Completion requires arrival/stop but not exact rotating-port angular state.
2. **FinalIngress** — a separate short precision maneuver created only after the
   real ship has stopped and a fresh port pose is available.

The final ingress owns full dock-basis alignment. Forward fixes nose direction;
Up fixes roll about that axis, so yaw/pitch/roll are all represented by the
same 3-D attitude target.

This separation prevents terminal capture constraints from poisoning ordinary
route feasibility and matches the intended game-play sequence:
arrive -> stop -> align -> enter.

## 2026-09-27 — request, presentation and server authority are distinct

`SystemMapRenderer` records a docking UI request in the shared navigation
workspace. `SpaceState` separately prepares/publishes a visible advisory route,
then sends `BeginAutomaticDocking`. `GameServer` separately accepts Autopilot
authority and executes the two-stage maneuver. The UI must not equate a pending
request or visible route with accepted Automatic authority. Transition logs now
share the request serial across these boundaries; target evidence is pending.

The subsequent audit found two missing links: active Automatic skipped client
route reprojection, and terminal server outcomes did not cross the network.
The candidate now maintains route presentation during Automatic execution and
replicates `{serial, succeeded, reason}` per controlled session. This is an
outcome channel, not an extra Planner or Follower command. Wire protocol 12
requires rebuilding both sides together.
The implementation tree was published to `main` at `4da95fa3` after explicit
authorization. Publication is distinct from native or live acceptance.

The first target Automatic execution revealed a bridge clock-domain bug:
absolute planned program time was used to reset PilotSkillExecutor, whereas
later steps supplied per-tick gameplay deltas. The corrected executor clock is
local to the installed program and advances only by actual control deltas;
the accepted maneuver schedule remains in authoritative universe time. This
does not relax actuator or tracking acceptance. Repeated bridge failure is a
terminal, serialled server result instead of an infinite replan loop.
This correction is delivered on canonical main for Windows verification;
successful static checks remain distinct from flight acceptance.
## 2026-09-27 — pilot clock precision contract

Pilot decision cadence and latency use the same elapsed interval as the filter
integration. Matching absolute time to `deltaSeconds` to nanoseconds is not a
gameplay requirement: accept a difference up to max(0.1 ms, 1% of the step).
Reject larger missing/duplicated tick gaps. The live server still supplies a
local pilot clock from gameplay delta; world time remains authoritative for
trajectory sampling. Windows runtime evidence for Automatic remains pending.

## 2026-09-27 — timeline storage-page adjacency uses local time

Windows observed the first real Automatic motion, then an InvalidInput loop
when selecting accepted program pages. Comparing separately reconstructed
absolute page ends/starts at the large universe epoch against a nanosecond
gap was invalid for contiguous local samples. Page adjacency now compares
sequence offsets and page-local duration, preserving strict continuity
without demanding impossible absolute-clock precision. BeforeStart holds
position; corrupt timelines fail once and report the reason to the client.
Live target validation is pending.

## 2026-09-27 — distinguish follower-envelope failure from velocity snap

A corrected accepted-program timeline now reaches Follower, but live Automatic
reports repeated tracking envelope excess with feasible propulsion. Server
previously hid the dimension/size of the tracking error and retried forever.
Follower now carries effective FreeTransit envelope errors alongside raw errors
for diagnosis. Runtime logs real thresholds and speeds, limits repeated replans
to three and reports a terminal reason. A velocity-continuity diagnostic
compares successive authoritative samples with acceleration authority plus
diagnostic slack. A visual speed snap is not yet proven to be a physics write.
Windows native/live evidence is pending.

## 2026-09-27 — Assisted planner-to-physics longitudinal response

The executed Assisted speed command previously carried only the planned
velocity. Actual controller acceleration is `gain * (target - actual)`
clipped by installed physical main authority. A feasible planned acceleration
ramp therefore creates persistent lag `a/gain` and triggers a velocity-only
tracking rejection. The bridge now also gives the canonical Assisted motion
function the already PilotSkill-executed linear acceleration demand; its
forward projection divided by the same response gain advances the speed
setpoint. Speed and physical acceleration remain clamped by the existing
control law. Automatic Human handback actively brakes and clears a retained
Assisted speed target through LocalFlightControlStateMachine. No trajectory
acceptance, geometry or terminal tolerances were widened. Windows live
verification remains pending.

## 2026-09-27 — late-turn Assisted feed-forward gap

Live Automatic reached page 390 at 117.06 s. Actual/planned scalar speeds
agreed near 98 m/s, but vector velocity error was 9.9 m/s > 8 m/s; position
and attitude remained within limits. The pilot-executed lateral trajectory
acceleration was discarded by Assisted actuation. Candidate feeds it into
the canonical stabilization actuator within physical caps. New diagnostics
differentiate vector motion from attitude. Scalar path progress uses the
whole-route minimum of local constraints, explaining a possible ~98 m/s
program despite a local 500 m/s corridor recommendation. Native/live proof
and route-local speed scheduling remain pending.

## 2026-09-27 — first Windows test of lateral turn candidate

Windows compile succeeded and three of four native docking tests passed.
The fourth was navigation_runtime_control. Inspection of the newly added
turn regression shows an impossible expectation: 4 m/s^2 demanded with a
fixture allowing only 2. Fixture now allows 8, tests an ordinary 4 m/s^2
turn and a saturated 100-to-8 m/s^2 command. No production limits were
relaxed. Native and live acceptance remain open.

## 2026-09-27 — collision-free HOLD capture after residual rotation

Windows live ApproachHold reached page 422 at 126.82 s: position 9.415 m
short, speed 1.026 m/s, angular-rate error 0.284 > 0.25 rad/s. The old
orchestrator rejected this at the final reference and retried the full
approach, which failed its Assisted reverse-flight proof; final ingress had
not started. Bounded standoff capture now enters physical stop/rotation
damping before fresh FinalIngress planning when distance <=12 m and speed
<=2 m/s at the last sample. Nearby recovery suppresses the ordinary 1000 m
forward launch. Detailed reverse rejection keeps the strict physical gate.
No final docking acceptance has yet been demonstrated.

## 2026-09-27 — correctable errors belong to the controller

The user identified a game-design mismatch: minor hold position/angular-rate
errors should induce physical steering, not abandon the task. The candidate
keeps Planner/Follower feed-forward when angular rate alone leaves its soft
tracking envelope, counter-steers through installed angular authority and
continues near the dock at low speed. Unsafe translation or excessive actual
angular speed still triggers recovery. The earlier bounded hold capture
enters fresh FinalIngress. Full docking, including physical latch, has not
been implemented or accepted.
## 2026-09-27 — revised docking navigation and control contract

User-reported request 2 invalidated the previous near-port acceptance claim:
HOLD was almost reached, but storage-page completion and angular tracking
still drove a full approach recovery and Assisted reverse-flight failure.

The target pipeline is: authoritative safe stabilized start; start/end safe
sections; collision-free 3D tangent geometry; route-local speed and full
orientation keyframes; vehicle control plus observed feedback; in-plane HOLD
stop/alignment; dock-relative short ingress; physical contact/latch. The
Planner may use observed performance limits to keep speed/braking feasible,
but it must not replace the ship's physics or treat exact program samples as
the only safe physical states. The visible top marker has a real attitude
meaning; roll is not implicit from yaw.

First candidate covers local speed timing, shared port-up reference and the
HOLD spatial handoff. Strict capability/proof remains in force. Full control
interface and contact/latch stages remain open. No live acceptance yet.
## 2026-09-27 — trajectory-following failure gate

Windows logs supersede the previous acceptance candidate: Automatic abandoned
the moving program after 0.255 versus 0.25 rad/s angular-rate error, then
after roughly 9–10 m/s of forward-speed lag. New telemetry records immutable
reference, PilotSkill output, physical engine acceleration and body rates on
the same tick. Assisted route geometry no longer silently falls below a
cruise-based minimum on ordinary arcs. Manual key torque ramps over 0.25 s;
navigation's independent angular-demand path retains full authority. This is
a diagnostic/geometry candidate, pending Windows build and flight evidence.


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

## 2026-09-28 — SpatialCorridor execution contract refined after first live gate

The first live test of progress-driven SpatialCorridor revealed a start singularity: at the exact stopped first point, projection alpha remained zero and therefore the sampled target velocity also remained zero forever. Execution now resolves this using spatial checkpoint semantics, not time. Geometry/progress comes from the real-position projection; when a zero-speed lower checkpoint has a moving next checkpoint, the segment tangent and next checkpoint speed provide the local launch target. This does not move the reference position ahead and does not consume nominal trajectory time.

The route-loss contract is also separated from rotational state tracking. SpatialCorridor hard validity is cross-track position/velocity plus forward-course angle. Angular-rate mismatch remains bounded feedback state, not a reason to discard a still-valid tunnel. TimeScheduled execution keeps the full dynamic envelope. Candidate is published on main in commits `2608765`, `5a2ca7d`, `3f199b7`; Windows target evidence remains pending.



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


## 2026-10-05 — corridor capture gets explicit angular-rate demand; braking and final attitude are separate phases

Latest live run improved curve tracking and exit braking, but exposed two remaining control defects.

1. After curve exit the craft could fly almost parallel to the final straight while remaining tens of metres off centerline. Live telemetry showed cross-track around 80–90 m with only ~1–2 m/s closing speed at ~289 m/s forward speed. The critically damped position law already produced a non-zero capture angle, but the angular reference on a straight still published desired omega=0. This created a contradictory command: hold a few degrees toward center while also asking angular rate to be zero, so the hull converged too slowly and effectively cruised beside the tunnel.

ClientRouteAutopilot now publishes an explicit capture angular-rate component:
    omega_capture ~= correction_angle / course_tau
clamped by physical max angular rate. This component is added to route curvature omega and roll omega. Position and lateral closing speed still determine the correction angle, so the controller can reverse the correction before center crossing instead of shuttling.

2. Dock-bottom alignment is now an explicit roll task throughout route execution. The follower projects actual hull up and the authoritative docking routeUpReference onto the plane normal to the desired nose, computes a signed roll error, derives a desired roll rate, and adds that rate around the desired nose axis. Therefore bottom of hull to bottom marker is no longer expected to emerge indirectly from the combined 3-D attitude error while pitch/yaw are busy tracking the route.

HUD corridor orientation and autopilot both use the same resolved docking-port up vector. If future live logs show roll_err_deg converging near zero while the rendered hull is still visually misaligned with the bottom marker, the remaining defect is likely a visual-model/body-axis basis mismatch rather than navigation control. New telemetry exposes roll_err_deg and roll_target_omega_radps to distinguish those cases.

3. Terminal braking and final attitude capture are now hard-separated:
- TRACK: follow route geometry and continuously align roll/up to docking-frame up.
- BRAKE: on the final route primitive, BrakeToStop owns translation; desired forward remains the final straight tangent, desired up remains docking-frame up, and final pose capture is disabled while speed > max(0.5 m/s, stop epsilon). If the craft crosses the nominal stop plane, it continues braking straight and does not pitch/yaw back toward the point.
- HOLD: only after near-zero translational speed does terminal attitude capture lock to final forward/up and finish orientation.

Brake-attitude lock is restricted to the final geometric primitive so early braking on a preceding curve cannot suppress required curvature tracking.

Published commits:
- 4c1a1d56 — actively capture corridor center and dock-bottom roll
- f64d2753 — separate straight braking from terminal attitude capture
- 4de34fef — restrict braking attitude lock to final route primitive
- 94ef227c — trace brake-attitude / terminal-capture phases
- de0b3b8c — regression: terminal braking stays straight until translational stop

Next gate: Windows MinGW client_route_autopilot test, then live run. Key telemetry:
- on a parallel-offset straight, capture_angle_deg and capture_omega_radps must remain materially non-zero until cross_track returns toward the center band;
- roll_err_deg must converge toward zero with a consistent roll_target_omega_radps;
- terminal_brake=1 with brake_attitude_lock=1 must occur while terminal_attitude_capture=0 and pitch/yaw remain small;
- terminal_attitude_capture becomes 1 only after speed is nearly zero.


## 2026-10-05 — Assisted turn slowdown now includes speed-setpoint slew and corridor-loss speed cap

The latest live log showed that the follower could compute a low dynamic turn ceiling yet continue at cruise until too late. Example pattern: turn_vmax around 169 m/s with ~607 m to the curve while target/actual speed stayed near 288 m/s; only ~266 m before the curve did target speed begin to drop, and the craft entered the curve still roughly 50 m/s above the dynamic ceiling.

Root cause: pre-turn braking included physical braking authority and a small feedback-response term, but it did not include the finite Assisted target-speed setpoint slew. targetSpeedRate changes targetForwardSpeedMps at a configured rate (for Cobra-class current parameters about 90 m/s^2), so moving the speed handle from 288 to 169 itself takes roughly 1.3 s and hundreds of metres at cruise speed.

The pre-turn slowdown horizon now includes:
- speed-handle slew time: deltaV / targetSetpointRate;
- conservative distance travelled during that slew at current speed;
- measured/nominal physical braking distance from current speed to turn ceiling;
- longitudinal feedback response distance.

Required turn slowdown distance is therefore:

    d_turn =
        v * t_slew
        + (v^2 - v_turn^2)/(2*a_effective)
        + v * t_feedback

When d_turn >= distanceToTurn, the follower commands the full curve speed ceiling immediately. The Assisted setpoint slew and physical propulsion still make the actual deceleration smooth; the important change is that the command is no longer delayed until the ideal braking-only boundary.

The follower also now reduces cruise speed when substantial cross-track error already consumes corridor authority. Cross-track occupancy between the central deadband and the route tolerance smoothly blends the current target speed toward the next known controllable turn-speed ceiling. A craft that is tens/hundreds of metres off-center therefore cannot continue at unrestricted straight-line cruise while attempting recovery.

New telemetry:
- turn_need_m
- turn_slew_s

Published commits:
- 6e8ecf82 — include speed-handle slew in turn braking horizon
- f1d079d8 — make actual speed available to pre-turn slew model
- 78a2b56b — log turn slowdown horizon and speed-handle slew
- 81bd52dd — regression requires Assisted speed-setpoint slew in turn slowdown

Next gate: build/test client_route_autopilot, then live first-turn validation. Expected behavior: target speed begins falling while turn_distance_m is still on the order of the full turn_need_m; actual speed should be near turn_vmax at curve entry rather than tens of m/s above it. If cross-track exceeds corridor tolerance, target speed should remain capped until centerline recovery is substantially complete.


## 2026-10-05 — Route execution contract audit: exact curves + shared Assisted response

The route/autopilot implementation was audited against the hard execution
contract in `src/game/navigation/ROUTE_NAVIGATION_CONTRACT.md`.

Confirmed active automatic-docking execution path:
`SpaceState::updateDockingAdvisory -> planner::RoutePlanner ->
ClientRouteAutopilot -> PredictivePilot -> ordinary ShipControlState`.
The older sampled `autopilot/RouteFollower.cpp` look-ahead implementation is
not the active docking control path and was deliberately not used to redefine
the current route geometry.

Fixes published:
- `0cd1c984` — declare the missing turn-slowdown diagnostics that broke the
  build; preview authored future curves beyond an immediately adjacent straight;
  inspect CubicBezier interior arc-length knots when endpoint curvature is zero.
- `382d7212` — regression coverage for a bend beyond an intermediate straight
  and for Bezier interior curvature.
- `e0d6d908` + `bfeafa83` — make PredictivePilot expose one canonical
  effective Assisted target-speed change rate and make turn preview use that
  same learned/measured rate instead of a separate configured-only estimate.
- `2865e476` — regression requires turn preview and PredictivePilot to agree
  on the learned Assisted speed-handle response.
- `8e1e40fb` — repository hard contract for route geometry, global speed
  envelope, Assisted delay, arcs/radii, Bezier/splines, look-ahead and
  anti-regression rules.

Important architecture preserved:
- `RouteCurveSegment` remains the authoritative Line/CircularArc/CubicBezier
  geometry; sampled gates are representation, not a replacement route.
- runtime speed profile retains its forward acceleration and backward braking
  feasibility passes;
- the new Assisted response model is composed with that profile instead of
  replacing it;
- actual scalar speed remains the speed feedback during a turn;
- course phase lead follows the authored curve rather than cutting a waypoint
  chord.

Remaining design item, not changed blindly in this audit:
terminal Assisted radius selection still begins from a design-speed policy
(roughly 0.8 of route max speed) and then derives lateral/angular radius. This
is physically bounded but can make a fast small ship request an excessively
large terminal arc. The next geometry change should solve the pair
`turn radius <-> turn speed` together from hull/corridor scale plus
lateral/angular authority, with its existing docking radius tests updated in
the same change. Do not replace it with another fixed radius.

Verification gate:
run the Windows MinGW `client_route_autopilot` test first, then
`docking_advisory`, then the live first-turn run. No CI runner is present in
the repository, so these commits have been source-audited and regression tests
added but have not been executed in this environment.
