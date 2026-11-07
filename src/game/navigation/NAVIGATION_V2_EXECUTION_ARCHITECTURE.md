# Navigation V2 execution architecture

**Status:** authoritative current execution contract  
**Updated:** 2026-10-06  
**Scope:** client-owned player navigation / docking automatic execution; reusable route-following rules for tunnels, canyons and later NPC/autonomous actors.

This document overrides older execution descriptions when they conflict with it.
Older architecture documents remain useful as history, research and product intent,
but they are not allowed to reintroduce retired control seams.

## 1. Canonical ownership

**Author decision 2026-11-06 (normative):** the server simulates many players, so every planner and, where
possible, every autopilot runs on the CLIENT; the server receives ordinary control commands and stays the
plant + the authority:
- CLIENT: route planning (route window, J charge walk, refuel insertion, corridors, body avoidance), the
  docking planner and autopilot, the global J autopilot, the landing autopilot (site search, de-orbit,
  terminal pilot), route-leg execution, take-off, rendezvous, the route state (pause / resume).
- SERVER: fixed-step physics on the travel frame, the J field EXECUTING the client's command within the rules
  (max acceleration, ramp, keep-out, J floor, 0.9 c cap, the charge), attitude slewing within the hull's
  rates, ground contact / damage, collisions, navigation-zone fines, the replicated authoritative state.
- The command surface is `ShipControlState` (ordinary input every frame) extended with a global-flight part:
  a J WORLD VELOCITY TARGET with the client's acceleration limit, a main-engine world acceleration, an attitude
  target, a frame request (normal flight / ground stabilization / stabilization) and a free-rotation request.
  With client-server latency at up to 1e5 g the client commands TARGETS that the server tracks every tick
  (deadbeat within the rules) - the J braking profile (tail d / 6 s) stays stable under that latency.
- Shared rules on both sides: `navigation/rules/BodyFlightRules.h`, `navigation/rules/JDrive.h`.

Migration status (2026-11-06): DONE in code (G1..G4), live-verified on DEV runs, not yet author-accepted. The body
autopilot (`navigation/globalflight/BodyAutopilot`) and the route-leg pilot (`navigation/globalflight/RouteLegPilot`)
are neutral modules run by the client (`SpaceStateBodyAutopilot.cpp`, `SpaceStateRouteRun.cpp`); the server executes
the command (`GameSimulation::applyGlobalFlightCommand` / `applyGlobalFrameRequests`), judges zones, keeps the J
charge and runs a route stop's service stay at a hub (the charge fill is an authority action). The server-run
autopilot / route legs / take-off are deleted; the old `FreeFlightAutopilot` / route `RouteLeg` commands are ignored.
The hand-over rule: a global leg ends by leaving J into a stabilization frame (hub / own orbit / ground);
a pilot-initiated global action while a client tactical route runs MUST cancel it first (OPEN).

The rest of this section describes the local tactical chain.

The server owns authoritative world/ship state, collision/damage and fixed-step
physics. It receives the same ordinary `ShipControlState` surface whether the
input source is a human or the client virtual pilot.

Canonical player chain:

```text
SystemMap / local route intent
    -> SpaceState client navigation state machine
    -> authoritative replicated planning snapshot
    -> planner::RoutePlanner
    -> RoutePlan
         routeCurves = authoritative parametric geometry
         executionGates = storage/presentation/checkpoint support
    -> TrajectoryGenerator
    -> AcceptedManeuverProgram pages
         physical speed/acceleration/attitude reference
    -> ClientRouteAutopilot
         continuous spatial progress
         route-speed preview/envelope
         course capture
         roll/terminal composition
    -> AutopilotLawController
         -> AssistedAutopilot | NewtonianOptimalController
    -> ordinary ShipControlState
    -> GameClient::submitInput
    -> server fixed-step ship physics
    -> authoritative replicated P/V/q/omega feedback
```

The current production composition is inside `ClientRouteAutopilot`.
The older `RouteFollower` generation and its fixed-distance look-ahead
implementation have been deleted. There is one player follower/execution owner.

Removed/forbidden execution seams:
- `ShipControlAdapter` — deleted; NPC and player ordinary controls use the hard flight-law autopilot API;
- `CorridorCaptureGuidance` — deleted; recapture is `CourseCaptureGuidance`;
- old `RouteFollower` fixed-look-ahead execution path — deleted;
- old composite proving-ground execution path — deleted;
- direct `navigationAccelerationDemand*` / `navigationVelocityTarget*` actuator paths remain migration debt and may not be reintroduced into the player autopilot;
- fixed-distance steering look-ahead;
- a separately authored follower route.

## 2. Route geometry is authoritative

A route is parametric geometry, not a waypoint cloud.

Each `RouteCurveSegment` owns a stable progress interval and evaluable:
- position;
- tangent;
- curvature;
- radius where meaningful;
- primitive type (Line / CircularArc / CubicBezier; future splines follow the same contract);
- route-local speed ceiling;
- terminal/corridor semantics.

Samples and gates may support bounded storage, HUD drawing, checkpoints and
collision proof. They do not replace the curve and must not create a new route
inside the follower.

Centered execution uses the exact local tangent at current route progress.
Bezier/spline speed preview must inspect interior curvature; zero curvature at an
endpoint is not evidence that the primitive is straight.

## 3. Navigation direction is velocity-course

While translational speed is meaningful, the navigation direction is the
measured velocity direction.

```text
actualCourse = normalize(actualVelocity)
desiredCourse = route tangent or CourseCaptureGuidance result
courseError = angle(actualCourse, desiredCourse)
```

Hull forward is not route truth. It is an actuator state used to rotate the craft
so the real flight law can move the velocity vector toward `desiredCourse`.

At very low speed the velocity direction is undefined. A deterministic hull
fallback is allowed only for:
- launch from near-zero speed;
- precision STOP;
- explicit terminal attitude capture.

The fallback must never leak back into ordinary moving route-course measurement.

## 4. Course capture is a bounded temporary geometry

`CourseCaptureGuidance` owns route recapture geometry.

Centered inside the deadband:
- no capture curve is active;
- `desiredCourse` is the exact current Planner tangent.

Materially displaced:
- start point = actual craft position;
- start tangent = actual velocity direction;
- meeting point = a future station on the accepted Planner route;
- meeting tangent = exact route tangent there;
- temporary capture geometry = cubic Bezier/Hermite-equivalent;
- steering horizon = measured course-response distance, not a fixed number of metres.

The capture curve is a control/recovery construction only. It does not mutate
`RoutePlan.routeCurves`, does not become HUD route truth and does not authorize
a shortcut through the accepted corridor.

## 5. Speed ownership and physical envelope

Keep four speeds separate:

```text
routeSpeedLimit
requestedSpeed
assistedCommandedSpeed
actualSpeed
```

Rules:
- curvature/geometry establishes physical route speed ceilings;
- the full route speed envelope propagates future restrictions backward through
  braking reachability;
- checkpoint/re-anchor propagation uses measured state forward through
  acceleration/braking reachability;
- measured initial speed is immutable state, never a refinement knob;
- overspeed means brake toward the envelope, not reject merely because the
  measured state is above a future ceiling.

For Assisted longitudinal braking, reverse-main authority is the braking source.
Lateral stabilization authority is not additional reverse thrust.

`makeManeuverExecutionAuthority()` is the shared source for executable forward,
braking and lateral authority plus feedback reserve. Generator/runtime/validator
must not carry independent physical formulas for the same control law.

Turn preparation includes command response, braking and closed-loop margin.
A speed restriction that becomes active only after curve entry is too late.

Sampled numerical validation may use a small engineering tolerance, but tolerance
must not redefine the physical envelope or hide material infeasibility.

## 6. Hull attitude is an actuator problem

`AutopilotLawController` selects exactly one concrete flight-law pilot. `AssistedAutopilot` and `NewtonianOptimalController` convert the same law-neutral guidance request into ordinary hull/translation controls through the same API.

Pitch/yaw:
- target the desired velocity-course while moving;
- use real hull angular rates for damping/braking;
- do not feed a velocity-course error into an angular controller that assumes
  that error is a hull pose error.

Roll:
- is an independent hull-frame channel;
- aligns dock/tunnel up reference;
- must not reduce pitch/yaw authority;
- pitch/yaw must not create roll.

The pure separation remains:

```text
HullPoseGuidance
    -> pitch/yaw pose error + independent roll error
HullAttitudeControl
    -> bounded pitch/yaw input + independent roll input
```

Terminal attitude capture is explicit. Only terminal hold may intentionally make
a specified hull pose override the moving-course actuator target.

## 7. Assisted control law

Assisted semantics remain aircraft-like:

```text
where the nose points, velocity follows with finite lag
```

The lag is real and measured; it is not permission to navigate by nose.

Ordinary execution:
- pitch/yaw/roll rotate the hull;
- `targetSpeedRate` changes the Assisted longitudinal speed setpoint;
- actual scalar speed is `length(actualVelocity)`, never projection onto the
  newly rotated nose;
- fore/reverse main owns material longitudinal braking;
- manoeuvre/RCS translation is precision/stabilization authority, not normal
  route-turn propulsion.

STOP:
- material speed -> ordinary `BrakeToStop` / END path;
- residual below the precision threshold -> ordinary keypad RCS;
- direct navigation actuator demand remains forbidden.

## 8. Planner versus follower

Planner owns:
- world/topology choice;
- exact route geometry;
- corridor/clearance proof;
- physical speed envelope;
- refinement of speed/radius/geometry when validation asks for it;
- structured `NeedsRefinement / WaitForWindow / PhysicallyImpossible` outcomes.

Follower/autopilot owns:
- monotonic spatial progress on the accepted route;
- current route evaluation;
- bounded speed re-anchor from measured state;
- bounded course recapture without changing route truth;
- hull/roll execution;
- terminal braking/capture;
- invalidation when execution leaves the proved envelope.

The follower must not solve a second normal route. A material deviation that
cannot be recovered inside the accepted envelope wakes replanning.

## 9. Test architecture

The navigation-runtime suite contains two classes of tests.

### 9.1 Current production/V2 acceptance

The canonical runner is:

```bash
tests/navigation_runtime/run_mingw64.sh
```

It builds current targets and runs:

```bash
ctest --test-dir build/tests/navigation_runtime   -LE legacy_navigation_lab   --output-on-failure
```

Current acceptance coverage includes:
- `autopilot_law_api` / `navigation_runtime_control` — hard flight-law API, ordinary controls and STOP semantics;
- `hull_pose_guidance`;
- `hull_attitude_control`;
- `route_speed_guidance`;
- `course_capture_guidance`;
- `hull_roll_alignment_contract`;
- `client_route_autopilot` — production composition and curve/speed/capture regressions;
- `navigation_scenario_runner` — RoutePlanner -> ClientRouteAutopilot -> real physics;
- `navigation_v2_tunnel_proving_ground` — lower-level accepted-tunnel physical proof;
- planner/compiler/authority/infrastructure/API tests that do not own a retired
  execution path.

`NavigationExecutionReplanPolicyTests` is a scheduling/invalidation test only;
retired `TrajectoryFollower` execution assertions are not part of that production gate.

### 9.2 Remaining legacy labs

Only legacy tests whose underlying production components still exist may remain.
Deleted navigation generations are removed from CMake and the repository rather
than preserved as runnable comparison code. The remaining `legacy_navigation_lab`
label is therefore a temporary migration bucket, not an archive.

## 10. Architecture-contract locks

`tests/architecture_contracts/check_automatic_docking.py` must lock:
- client-owned Planner/autopilot;
- `CourseCaptureGuidance` in production composition;
- law-neutral route guidance at the hard autopilot API boundary;
- separate HullPose/HullAttitude layers;
- shared executable authority;
- no retired adapter/direct-demand/private follower route in `ClientRouteAutopilot`;
- production/legacy test-gate separation.

The current runtime architecture must not be inferred from historical
`CURRENT_*` paragraphs that predate this document.

## 11. Cleanup status, 2026-10-06

The repository is being collapsed to one execution generation.
Completed in this cleanup:
- `ShipControlAdapter` deleted and production NPC/player ordinary control migrated to `AutopilotLawController` with isolated Assisted/Newtonian pilots;
- obsolete `CorridorCaptureGuidance` and its test deleted;
- obsolete composite proving-ground test deleted;
- current architecture contract is `ClientRouteAutopilot -> AutopilotLawController -> {AssistedAutopilot | NewtonianOptimalController} -> ShipControlState -> ShipController/DynamicMotionSystem`.

Still to migrate before direct-demand removal:
- the old NavigationRuntimeControlBridge/runtime-lab path;
- any remaining `TrajectoryFollower`-based runtime-lab execution;
- direct navigation-demand fields in `ShipControlState` and the corresponding `ShipController` overload, once no caller remains.

Target-machine build/test evidence is required after each deletion slice.

## 11a. Orbital Transfer (hub -> hub around one body), 2026-11-03

**REMOVED 2026-11-07** (§1 migration, author order "open items closed in the same run"): the server-run
carrier mode, `GameSimulationOrbitalTransfer.cpp`, `orbital/OrbitalTransferPlanner` and its offline check are
deleted. "Transfer to the selected hub" is a one-stop CLIENT route Hub leg (`SpaceStateRouteRun.cpp`,
`globalflight/RouteLegPilot`): the J drive to the hub's keep-out edge with its velocity, then the hub capture
(`handOverToHub`). The text below is the historical design.

Hub Tactical and Orbital Transfer are different jobs and different modes.

- **Hub Tactical** (unchanged): local tens of km inside the moving hub frame,
  docking, queues, traffic separation, the 500 m/s controlled envelope. The
  ship-owned travel frame is matched to the hub frame.
- **Orbital Transfer** (server-authoritative navigation mode,
  `GameSimulationOrbitalTransfer.cpp`): the ship's travel frame is DETACHED
  from the source hub (`TravelFrameSystem::detach`), re-based at the ship and
  its origin driven by the carrier law of `src/game/navigation/orbital/`:
  plan feed-forward + real central-body gravity + PD, clamped to the ship's
  main engine. The ordinary tactical integrator keeps running inside that
  frame, so the local envelope and `maxCombatSpeed` are never raised; the
  transit speed envelope is an output of the plan (curvature, lateral and
  braking acceleration, target velocity), not a speed limit.
- The plan (`planOrbitalTransfer`) ends at the destination hub's approach
  point AT THE ARRIVAL EPOCH with the hub's velocity at that epoch (moving
  target, ephemeris from `OrbitalMotion`), departure / arrival ramps inside
  the traffic zones, a nearly Keplerian phasing chain over the body (minimum
  safe altitude checked), smallest arrival epoch within the thrust envelope
  and a delta-v budget.
- Hand-over: at the arrival epoch the ship is re-based into the destination
  hub frame with its actual relative position and velocity
  (`worldToLocalKinematics`, `matchToReference`) - no snap.
- A destination around another body (Earth -> Ceres) uses the SAME plan
  type, mode, executor and hand-over in compressed time
  (`planCompressedTransit`): the frame follows a bowed star-relative tunnel
  to the destination's approach point at the REAL arrival epoch (no snap);
  the physical brachistochrone it stands for is reported.
- While a transfer carries the frame, no pilot / autopilot translation is
  executed inside it (authoritative mode).
- Not a second autopilot of the tactical route chain: the carrier law moves
  the FRAME; route planning / docking inside a hub frame stays with the
  existing chain. Known debt: ship attitude is not slaved to the carrier
  thrust; free gravitational flight of ships outside a transfer is still not
  integrated (gravity remains metadata in Hub Tactical).
- Checks: `EliteOrbitalTransferCheck` (offline), live server log
  `[OrbitalTransfer]` (t, speed vs destination, altitude, thrust, tracking
  error).

## 11b. Free flight (Ctrl+J), 2026-11-03

**Current (2026-11-06)** - the modes as implemented (details in PROJECT_STATE slices cc-1 .. cc-10):
- NORMAL flight (`motion.freeFlight`, Ctrl+K leaves / enters STABILIZATION): main engines, no speed
  envelope; the hull may lead the velocity by 10 deg and the velocity turns toward it only as fast as
  the main engines can (a_main / v); Backspace = a free hull (no prograde hold, no bending).
- J engine (Ctrl+J, `motion.jEngine`): the throttle sets an ACCELERATION (HOME +max, END -max, INSERT 0,
  +/- exponential), the field follows exponentially; off = the field winds down to the main engines'
  level first; it spends the jump-drive charge (`JDrive.h`), the ship's own figures (`ShipParams`).
  Refused near objects (keep-out) and below the J floor.
- STABILIZATION: hub frame / own orbit / ground (500 m/s manoeuvre envelope); automatic on a closed
  safe orbit after a coast.
- Landing / take-off / route legs / rendezvous: server-owned (section 1).

The history below is the 2026-11-03 first version.

The pilot's own global flight, a third job next to Hub Tactical and Orbital
Transfer (`GameSimulationFreeFlight.cpp`, flag `DynamicMotionState::freeFlight`,
replicated; command `ClientShipCommand::ToggleStabilization`, Ctrl+K - since the
2026-11-05 flight architecture "free flight" is NORMAL flight and leaving it is
STABILIZATION; the J engine is a separate switch, `ToggleJEngine`, Ctrl+J).

- On: the travel frame is detached from the hub and re-based at the ship with
  its world kinematics (same seam as Orbital Transfer). The frame carries the
  ship's GLOBAL motion - position / velocity in the system's world frame (the
  galaxy reference) - integrated under the real gravity of all bodies
  (`GravityFieldSystem`, velocity Verlet on the gameplay tick).
- Every tick, before the ship's flight step, the previous tick's local motion
  is folded into the frame; the flight laws act on a local state starting at
  rest. Thrust therefore accumulates without a cap and no servo pulls the
  velocity back to the 500 m/s envelope; `maxCombatSpeed` is not raised.
- Controls (both laws, one path: the main command into `applyLocalFrameInput`
  through `SharedShipPhysics::freeFlightMainCommand`, then the installed
  plant): HOME latches full main thrust, END flips the hull against the
  velocity relative to the reference (ordinary alignment controller,
  BackwardToVelocity) and burns only once aligned, down to ~2 m/s, INSERT
  stops; '+'/'-' remain momentary thrust. Keypad translation only while the
  main engine is idle. Backspace free rotation burns nothing; leaving it aligns
  the nose with the velocity relative to the reference (the latched thrust
  resumes after the alignment).
- Velocity reference: the hub the ship left (`freeFlightReferenceId`, its
  current world velocity replicated as `freeFlightReferenceVelocityMps`). The
  HUD vector / speed, the Backspace exit alignment and the brake all use
  v_ship - v_reference, so the speed starts at ~0 and grows with thrust; the
  heliocentric speed is not a pilot instrument. Top banner: "J СВОБОДНЫЙ ПОЛЁТ
  - скорость не локальная", reference, thrust state, keys, capture refusal.
- Turn rate under thrust: omega_max = j_crew / |a_thrust| with j_crew = 3 m/s3
  (sustained lateral jerk of the swinging thrust vector, upper transport
  comfort band - PROPOSAL value), capped by the ordinary attitude limits. Cobra
  at 36.8 m/s2: 0.082 rad/s.
- Off = capture: only the nearest hub within the capture envelope (50 km,
  PROPOSAL until hubs carry a traffic-control radius) and only at a relative
  speed within the tactical envelope (500 m/s); otherwise the ship stays in
  free flight and the refusal is shown (no local frame / unsafe speed). A
  capture re-bases with the actual relative kinematics (no snap).
- Rendering: hub-attached objects and co-hub ships are placed in the player's
  frame only while that frame is MATCHED to the hub
  (`HubFramePresentation.h`); a detached frame keeps the hub id as metadata
  only (before: the station was drawn around the ship).
- Not yet: atmosphere / impact model (below the surface is only logged),
  collision with hub geometry at free-flight speeds (a HOME burn aimed at the
  station passes through it), client prediction of the re-based frame
  (server-authoritative; live jitter not measured), the global navigation
  layer below.

### Global navigation layer (PROPOSAL, not implemented)

Free flight makes the ship's state a heliocentric (system) state; destinations
are moving bodies and hubs. That needs a separate planner above the hub
tactical chain, not an extension of it:

- state model: patched conics (sphere of influence per body) for planning, the
  same n-body `GravityFieldSystem` for execution;
- solvers: Lambert (arrival epoch -> departure / arrival velocity) for
  coast-dominated legs, brachistochrone / constant-thrust legs for the engine
  envelope, a combined search over departure epoch and flight time;
- gravity assists: flyby of a body changes the heliocentric velocity by the
  turn angle at periapsis (bounded by the minimum safe altitude); the planner
  evaluates sequences of 0-2 flybys like Earth -> Venus -> Mercury;
- autopilot: a carrier-style executor (the Orbital Transfer carrier law, but
  with the ship's real attitude and engine instead of a frame carrier), with
  hand-over to Hub Tactical at the destination;
- the in-system Orbital Transfer planner becomes the special case "same
  central body".

## 12. Known implementation debt

These are not contradictions to the architecture, but they remain cleanup work:
- `ClientRouteAutopilot` is currently a large composition/orchestration header;
  pure responsibilities are separated, but composition may later be split into
  smaller files without changing behavior;
- the standalone navigation-runtime test build recompiles production source
  files into its own static test library; source identity is preserved, but a
  future build cleanup should reduce compile-definition/link drift risk;
- older navigation MD files contain historical stages. This document is the
  precedence source for current execution semantics.

## 13. Change rule

Use the sequence:

```text
isolate
-> pure function/layer
-> dedicated test
-> freeze
-> compose
-> integration test
-> live evidence
```

Do not fix one layer by violating another frozen contract. In particular:
- do not weaken route geometry to fix pilot control;
- do not widen corridors to hide tracking failure;
- do not turn numerical tolerance into physical authority;
- do not reintroduce fixed look-ahead;
- do not navigate by hull nose while moving;
- do not use lateral stabilization as longitudinal braking;
- do not make tests depend on magic actuator numbers when they can derive the
  requirement from route geometry and the production authority model.
