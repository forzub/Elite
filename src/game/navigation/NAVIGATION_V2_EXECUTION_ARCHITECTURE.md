# Navigation V2 execution architecture

**Status:** authoritative current execution contract  
**Updated:** 2026-10-06  
**Scope:** client-owned player navigation / docking automatic execution; reusable route-following rules for tunnels, canyons and later NPC/autonomous actors.

This document overrides older execution descriptions when they conflict with it.
Older architecture documents remain useful as history, research and product intent,
but they are not allowed to reintroduce retired control seams.

## 1. Canonical ownership

Player navigation intent and automatic route execution are client-owned.

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
    -> PredictivePilot
    -> ordinary ShipControlState
    -> GameClient::submitInput
    -> server fixed-step ship physics
    -> authoritative replicated P/V/q/omega feedback
```

The current production composition is inside `ClientRouteAutopilot`.
`RouteFollowerApi` remains a useful public/lower-level compatibility seam and
is exercised independently, but `ClientRouteAutopilot` does not delegate its
current route-course execution to `RouteFollower::follow`.

Removed/forbidden execution seams:
- `ShipControlAdapter` — deleted; NPC and player ordinary controls use `PredictivePilot`;
- `CorridorCaptureGuidance` — deleted; recapture is `CourseCaptureGuidance`;
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

`PredictivePilot` converts the desired translational course into ordinary hull
controls.

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
- `navigation_runtime_control` — PredictivePilot ordinary controls and STOP semantics;
- `hull_pose_guidance`;
- `hull_attitude_control`;
- `route_speed_guidance`;
- `velocity_course_guidance` — pure navigation-course invariant;
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
- velocity-owned course in `PredictivePilot`;
- separate HullPose/HullAttitude layers;
- shared executable authority;
- no retired adapter/direct-demand/private follower route in `ClientRouteAutopilot`;
- production/legacy test-gate separation.

The current runtime architecture must not be inferred from historical
`CURRENT_*` paragraphs that predate this document.

## 11. Cleanup status, 2026-10-06

The repository is being collapsed to one execution generation.
Completed in this cleanup:
- `ShipControlAdapter` deleted and production NPC ordinary control migrated to `PredictivePilot`;
- obsolete `CorridorCaptureGuidance` and its test deleted;
- obsolete composite proving-ground test deleted;
- current architecture contract remains `ClientRouteAutopilot -> PredictivePilot -> ShipControlState -> ShipController/DynamicMotionSystem`.

Still to migrate before direct-demand removal:
- the old NavigationRuntimeControlBridge/runtime-lab path;
- any remaining `TrajectoryFollower`-based runtime-lab execution;
- direct navigation-demand fields in `ShipControlState` and the corresponding `ShipController` overload, once no caller remains.

Target-machine build/test evidence is required after each deletion slice.

## 12. Known implementation debt

These are not contradictions to the architecture, but they remain cleanup work:
- `ClientRouteAutopilot` is currently a large composition/orchestration header;
  pure responsibilities are separated, but composition may later be split into
  smaller files without changing behavior;
- `RouteFollowerApi` remains a transitional/public lower-level seam while the
  current player path owns continuous route progress inside `ClientRouteAutopilot`;
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
