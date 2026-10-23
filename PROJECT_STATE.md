# ELITE — LIVE PROJECT STATE
Updated: 2026-10-23, Europe/Kyiv. Canonical branch: `main`.

> This is the ONE authoritative, living continuation document for `forzub/Elite`.
> Update it after every material finding, code correction or Windows/gameplay gate.
> Keep this document **current and concise**; historic narratives are recoverable
> from Git history. `CURRENT_STATE.md` and `CURRENT_TASK.md` are legacy histories,
> not competing current state files. Work on `main` only (no side branches).
> Record real verification level: SOURCE, LOCAL TEST, WINDOWS TEST, GAMEPLAY.
> User diagnostic outputs go under `D:/tmp`. No tests merely to add tests.

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
  error before such a profile is activated.
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

**Verification:** All above are SOURCE / GitHub changes only. The agent cannot
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

1. `f0d2a192` — `ShipCapabilityBuilder.h`: an EMPTY *individual* module
   binding inside an otherwise bound installed motor bank now projects
   `{operational=false, thrust=0, vectoring=0}` in both ObjectModuleRuntime
   and replicated snapshot overloads, including explicit-model overloads.
   Only an ENTIRELY unbound explicit diagnostic topology may use synthetic
   healthy motors. Prevents a second, obscure healthy fallback.
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
