# Route, trajectory and autopilot product contract

> **Current execution precedence (2026-10-06):** for the live Navigation V2
> course/capture/speed/PredictivePilot execution contract and test-gate
> classification, use `NAVIGATION_V2_EXECUTION_ARCHITECTURE.md`. Historical
> sections below remain product/design history and must not override that file.


This document fixes the intended end-state of player navigation before the
trajectory solver is implemented. The product rule is deliberately asymmetric:
**the internal mathematics may be difficult; the player-facing interaction must
remain simple, visual and almost cartoon-like.** Normal navigation must never
look like an orbital-mechanics workstation.

Status labels:

- **PRODUCT CONTRACT** — intended end-state; architecture should grow toward it.
- **IMPLEMENTED BASELINE** — present in the current route UI/model.
- **NEXT** — next layer to implement without changing the product contract.

## PRODUCT CONTRACT: three separate layers

Navigation is split into three boundaries:

1. **Route Plan** — player intent: ordered waypoints plus exactly one optional
   Finish and an arrival profile.
2. **Trajectory Solution** — the physically feasible answer: curves, predicted
   target state, manoeuvre points, time, velocity, acceleration, safety and
   propulsion constraints.
3. **Autopilot** — execution of an accepted trajectory solution.

The Route Plan must not contain hidden physics. The trajectory solver may insert
calculated manoeuvre/correction samples, but those samples do not silently
become player-authored waypoints.

## PRODUCT CONTRACT: one route container on all four maps

Galaxy, System, Detail and Hub share one player-private route container.
It is one UI/model object, not four synchronized copies.

The route container:

- may be collapsed and keeps that state while switching maps;
- contains player waypoint cards in route order;
- keeps Finish visually and logically last;
- supports drag-reordering of intermediate waypoints;
- has one master `SHOW ON HUD` toggle plus a per-node HUD toggle;
- requires confirmation before deleting one route node;
- requires a separate confirmation before deleting the whole route;
- deleting the container means deleting the Route Plan, not merely hiding UI;
- route nodes survive closing their source information cards.

Double-clicking a route card recalls the authored map/context in which that
node was created. Cross-system nodes first load the authored system/sector, then
open the authored System/Detail/Hub layer and reveal the route object/card.
Therefore each route node stores authored context in addition to its spatial
fallback.

## PRODUCT CONTRACT: adding route intent stays on ordinary info cards

The ordinary object/cube information card is the only entry point needed for
basic route authoring. Eligible cards expose simple contextual actions:

- `WAYPOINT` — add/remove a spatial intermediate route intent;
- `ROUTE RENDEZVOUS` on a ship — intercept that moving ship, match its motion,
  then continue to the next route node;
- `FINISH` — make/remove the terminal route target.

Ships and Hubs are semantic targets. A route must store their stable identity,
not only the coordinates at the moment the button was pressed. Empty cubes are
spatial targets and therefore retain their precise world address. A ship used
as an intermediate node is explicitly a rendezvous event, never a frozen point
at the ship's old coordinates.

Exactly one Finish may be active. While it exists, other cards do not advertise
a competing `FINISH` action. The active Finish card does not offer `WAYPOINT`;
the player cancels Finish first if that target should become intermediate.

## PRODUCT CONTRACT: Finish is a terminal-state request

Finish is not merely a point in space. It carries an **Arrival Profile** that
specifies how the vehicle should end automatic flight relative to the target.
The normal UI exposes four square pictograms, not engineering forms:

1. **SAFE** — arrive co-moving in a safe envelope and stop automatic approach;
   the remaining close approach is manual.
2. **FOLLOW** — match motion and remain offset slightly beside/behind the
   leader.
3. **FORMATION** — occupy an available slot in an existing formation.
4. **PARADE** — join a formation whose geometry is preserved while the group
   manoeuvres visually as one object; every craft still uses its own physics and
   actuators.

The solver/autopilot may later expose advanced parameters on demand (safe
radius, preferred offset, slot, orientation), but they are not required in the
normal route-authoring flow.

All dynamic-target arrival modes are terminal-state constraints. At minimum the
solver must reason about target position and target velocity at arrival time;
formation modes also add relative orientation/slot constraints.

## PRODUCT CONTRACT: moving targets are predicted, never frozen

For a dynamic Finish the solver targets the object's predicted future state,
not its current map position. Travel time and target prediction are coupled and
must be solved iteratively or by an equivalent convergent method.

Conceptually:

- estimate arrival time `T`;
- predict target position/velocity at `T`;
- solve a feasible player trajectory to that terminal state;
- obtain a new travel time;
- repeat until the solution is sufficiently consistent.

If target motion invalidates the accepted solution, navigation marks the
solution stale and recalculates rather than pretending the old endpoint is
still valid.

## PRODUCT CONTRACT: trajectory prediction is a shared service

Trajectory prediction must not live only inside autopilot. A common predictor
is used by:

- route/intercept solving;
- map trajectory display for any selected object;
- moving-target prediction;
- later NPC navigation and formation behaviour;
- later tactical systems that need predicted motion.

Known/deterministic trajectories may be drawn solid. Predicted future motion is
visually distinct (normally dashed); uncertainty may later widen/fade with time.

## PRODUCT CONTRACT: trajectory editing remains visual

Player-authored waypoints may later be dragged between neighbouring cubes of the
same navigation level. The trajectory curve itself may also be grabbed and
"bent" in 3D, but the UI must interpret that gesture as a new constraint/control
point and ask the solver for a new physically valid solution. The player never
literally edits a Bézier curve and thereby overrides physics.

When a drag changes the solution, any cost feedback should stay compact and
optional (for example ETA / fuel / risk), not become the default map language.

## PRODUCT CONTRACT: safety routing

The trajectory solver is responsible for avoiding or respecting:

- celestial bodies and exclusion radii;
- known anomalies/hazards;
- unsafe approach geometry;
- propulsion/acceleration/velocity constraints;
- requested terminal velocity/orientation/formation state;
- later policy such as preferred safe corridors.

Additional solver-generated manoeuvre/correction points may appear beneath the
player route as derived trajectory information, but they remain distinguishable
from player-authored route nodes.

## IMPLEMENTED BASELINE (2026-08-21)

The first Route Plan layer now provides:

- persistent waypoint/Finish intent independent of source-card lifetime;
- one Finish plus ordered intermediate waypoints;
- Finish-last route ordering;
- live drag reorder for intermediate waypoints: the row follows the pointer and
  route order updates while the button is held;
- master and per-node HUD visibility;
- explicit footer confirmation for deletion (`DELETE ROUTE/WAYPOINT?` + localized
  `YES/NO`) rather than a hidden second-click gesture;
- a shared collapsible route container rendered on Galaxy/System/Detail/Hub;
- contextual `WAYPOINT` / ship `ROUTE RENDEZVOUS` / single `FINISH` actions on
  eligible physical-object/body cards and existing empty-space cube cards;
- semantic metadata for dynamic Ship/Hub/Infrastructure targets plus a refreshed
  world-position fallback;
- four localized arrival-profile pictograms/data modes: SAFE, FOLLOW,
  FORMATION, PARADE;
- green square-with-center-dot route glyphs and explicit route order on map/HUD;
- single-click/drag selection is highlighted both in the Route Container and on
  the visible map object; double-click recalls authored context across
  Galaxy/System/Detail/Hub;
- native route/map labels are resolved by the global `LocalizationService` from
  `assets/localization`; renderers contain no per-language text branches.

### IMPLEMENTED NAVIGATION OWNERSHIP BOUNDARY

The current baseline now uses the intended ownership direction:

- `SpaceState` owns one renderer-independent `ClientNavigationWorkspace`;
- transient open-card target tracking lives in `TargetTrackingState`;
- persistent route intent lives in a separate `RoutePlan`;
- maps and HUD are consumers/editors, not owners of navigation state;
- `RouteTargetRef` stores semantic identity: `ShipInstanceId` for ships, stable
  Hub/body IDs for authored objects, and canonical `WorldPosition` for free
  space;
- `EntityId` is never durable route identity and may change after
  dematerialization/rematerialization without changing the route node;
- route-container editing uses stable route-node IDs; presentation object IDs
  are only mutable bindings used to highlight/reveal the current map object.

Target/track numbering is also presentation-scoped: only ships receive target
numbers. Hubs and celestial bodies are unnumbered; route-order numbers remain a
separate concept and continue to label route points/Finish.

## NEXT

The next implementation layers are intentionally separate:

1. projected/dashed trajectory rendering fed by the shared predictor;
2. route/intercept solver with obstacle/safety constraints and arrival-state
   matching;
3. direct map dragging between equal-level neighbouring cubes and later 3D
   trajectory bending through solver constraints;
4. autopilot execution;
5. formation controller using leader-relative slots rather than copied thruster
   commands.

## Execution asset / START contract (Wave 2)

`START` is not inferred from the cockpit ship. `RoutePlan` stores an explicit
`NavigationRouteStart` whose `NavigationAssetRef` identifies the ship or future
durable drone that will execute the route. START is logically first, fixed, not
deletable/reorderable as a waypoint, and survives route clearing so the player
may author several routes for the same executor.

The server is authoritative for route-capable ownership. Each session receives
`ClientSessionSnapshot::ownedNavigationAssets`; the client may select a START
executor only from assets marked `commandable`. Current direct player-owned
ships come from `ShipOwnershipRegistry`. Current repair/visual drones are not
durable owned assets and therefore are deliberately not advertised as route
executors; a future persistent drone ownership registry will project through the
same `NavigationAssetRef::drone(DroneInstanceId)` seam.

The cockpit suppresses START only when START equals the locally controlled
asset. A remotely dispatched owned ship/drone remains a visible START marker.


## Shared trajectory predictor baseline (Wave 3)

`TrajectoryPredictor` is a renderer/server-neutral translational propagation
service. It consumes only data: system-local `WorldKinematicState`, universe
time, static gravity sources, an optional piecewise-linear proper-acceleration
program, sampling/integration cadence and a caller-selected motion envelope.

The reusable result keeps more than a drawable polyline: each sample contains
position, velocity, total acceleration, gravity acceleration, non-gravitational
proper acceleration, translational proper load in G and cumulative proper
`delta-v`. `TrajectoryPredictionDiagnostics` also reports peak speed/load/jerk,
travel distance and whether acceleration or jerk had to be constrained.

Crew load is deliberately based on **proper acceleration only**. Gravity still
changes the predicted world trajectory but free fall does not masquerade as a
seat load. The predictor also does not choose a product G limit. Manual flight,
automatic route execution and uncrewed craft may pass different envelopes; the
future solver owns that policy and must still respect structural/equipment
limits.

`TrajectoryMapAdapter` is a one-way presentation adapter from the rich shared
result into the existing `MapObjectTrajectory` position/time seam. The predictor
itself has no dependency on maps, renderers, `RoutePlan`, `SpaceState`, server
sessions or autopilot execution.

This baseline intentionally uses static gravity-body centers. Continuous moving
celestial ephemerides remain a later source/provider extension; they must extend
the acceleration/environment input without changing the prediction result
contract.


## Deferred galactic route planning

Strategic Galaxy-scale routing is intentionally separated from the local
trajectory/autopilot implementation and is specified in:

`src/game/navigation/GALACTIC_ROUTE_PLANNING.md`

That future layer owns jump-node search, jump-drive range/capability, fuel and
reserve cost, gravity-aware long-leg validation, ephemeris/timing effects and
inter-system replanning. It preserves this document's three-layer product rule:
RoutePlan -> accepted Navigation/Trajectory Solution -> ManualGuidance or
Autopilot execution.


## Execution geometry, speed-envelope and Assisted-response contract

This section is a hard implementation contract for Planner/Follower/Assisted
changes. New navigation work must compose with these rules rather than replacing
one already-working layer with another.

### Authoritative route geometry

A route is not a waypoint cloud. Every execution segment carries its actual
geometry and stable path-progress interval. Supported primitives include Line,
CircularArc and CubicBezier; future spline primitives follow the same contract.

For each primitive the execution layer must be able to obtain, directly or by
evaluation, position, tangent/body-forward reference, local curvature, radius
where meaningful, start/end progress, corridor ownership and route-local speed
ceiling. Circular arcs preserve their authored center, normal, radius and sweep.
Cubic Bezier preserves P0..P3 and an arc-length mapping so spatial progress can
be converted to curve parameter without redefining the geometry.

Samples/gates may be used for HUD rendering, collision proof, bounded storage
and compatibility, but they never replace the parameterized route. Follower
must not cut an arc/Bezier by steering directly toward an unrelated sampled
point or rebuild a private route between samples.

### Speed ownership and global envelope

Keep these concepts separate:

- route speed limit: geometric / authored ceiling;
- requested speed: target chosen by the route execution layer;
- Assisted commanded/setpoint speed: what the flight law can currently ask for;
- actual speed: measured craft state.

The route speed profile is a full-route envelope. Future restrictions propagate
backward through physical braking reachability; measured checkpoint state
propagates forward through physical acceleration/braking reachability. A local
Follower may lower a target for recovery but must never raise it above the
already-authoritative route envelope.

A lower speed required by an upcoming bend is not useful if it is requested
only after curve entry. The physical craft must reach the permitted entry speed
by the authored start of that curvature.

### Assisted response is part of feasibility

Assisted speed commands do not move instantaneously. Turn preparation therefore
accounts for both command/setpoint slew and real longitudinal response:

    preparation distance =
        speed-command response distance
        + physical braking distance
        + closed-loop response margin

The same measured/effective Assisted speed-handle response used by
PredictivePilot must be used by turn preview. Do not maintain two independent
estimates.

Course response is also finite. Assisted remains the aircraft-like law
"velocity follows the nose", but a short measured velocity-to-nose lag is
expected. Route tracking predicts that lag and leads hull attitude around the
same authored curve; it does not move the route.

Manoeuvre/RCS authority is reserved for bounded stabilization/fine correction.
It must not be treated as a substitute for rotating the hull and using the main
longitudinal propulsion model to negotiate an ordinary route turn.

### Arc/radius rules

Turn radius is a vehicle-and-route result, not a universal constant. It is
derived from the applicable combination of hull size/clearance, requested
speed, lateral authority, angular-rate authority, angular acceleration/ramp
distance, longitudinal acceleration/braking and available corridor geometry.

A curve publishes where the bend begins and ends and what curvature/radius and
speed ceiling apply there. Follower tracks that curve. Large ships naturally
receive larger required geometry because the inputs scale with hull and
dynamics; no Cobra-sized radius is silently reused for a 100-150 m craft.

### Bezier/spline rules

Bezier/splines are first-class route geometry. Follower/preview must be able to
evaluate position, tangent and curvature as functions of spatial progress.
Speed restrictions are derived from local curvature along the curve, not only
from its endpoints. A zero-curvature Bezier endpoint does not mean the interior
is straight; preview must inspect the interior arc-length domain.

### Look-ahead and corridor tracking

Look-ahead is measured along the accepted route. Its effective horizon follows
speed, braking/response distance and upcoming curvature rather than a fixed
number of metres or a fixed number of sampled points. HUD frames may retain
their presentation cadence, but frame spacing is not the control law.

Normal tracking aims to keep the craft near the corridor center with a bounded
deadband. Corrections use measured position and velocity error and may be
updated every control tick. They may change control commands, not the accepted
route geometry. A route replan is reserved for a real planning/recovery event,
not ordinary tracking error.

### Anti-regression rule

A new useful control feature is added to the existing useful layers:

Route Geometry
-> curvature/radius/tangent/corridor
-> physical route speed limits
-> full-route speed envelope
-> Assisted response (speed slew + longitudinal response + course lag)
-> preview/braking envelope
-> exact-geometry Follower
-> LocalFlightControlLaw

Fixing turn geometry must not delete early braking or Assisted response.
Fixing speed control must not degrade exact arc/Bezier following. Fixing
tracking must not recreate the route from waypoints. Each repaired boundary
requires a regression/contract test before later work is considered complete.


## Strict execution-layer responsibility split

Automatic route execution is divided into independently testable layers. A
change in one layer must not redefine the contract of another layer.

### HullPoseGuidance

Pure geometry only.

Inputs:
- current hull forward/right/up basis;
- requested target forward;
- requested tunnel/dock up reference.

Outputs:
- pitch/yaw forward-alignment error;
- roll alignment error.

Hard rule: pitch/yaw and roll are mathematically separate channels. Forward
alignment must never create roll demand. Tunnel up/down alignment must never
create pitch/yaw demand.

### HullAttitudeControl

Pure angular actuator control only.

Inputs:
- pitch/yaw error and rates;
- roll error and rate;
- angular acceleration and rate limits;
- fixed-step dt.

Outputs:
- pitch/yaw control input;
- roll control input.

Hard rule: roll authority is independent from pitch/yaw authority. Full roll
must not reduce available pitch/yaw command, and full pitch/yaw must not
reduce available roll command.

### CorridorCaptureGuidance

Pure positional capture geometry only.

Inputs:
- current position;
- current route point/tangent;
- authored look-ahead point;
- centering deadband.

Output:
- desired forward direction for return to corridor center.

It owns no roll, speed, throttle, engine or angular-actuator policy.

### RouteSpeedGuidance

Pure scalar longitudinal envelope only.

Inputs:
- actual scalar speed;
- future turn speed ceiling;
- distance to restriction;
- Assisted speed-handle slew rate;
- physical braking response;
- longitudinal feedback delay.

Outputs:
- preparation distance;
- whether slowdown must start now.

It must never modify forward/up orientation or corridor capture.

### Composition

The execution pipeline is:

```text
Planner authored route geometry
        |
        +--> CorridorCaptureGuidance --> desired forward (pitch/yaw task)
        |
        +--> route up/down -----------> roll task
        |
        +--> RouteSpeedGuidance ------> scalar speed task
        |
        v
HullPoseGuidance
        |
        v
HullAttitudeControl
        |
        v
PredictivePilot / ShipControlState
        |
        v
ShipController
```

ShipController must preserve the same separation: pitch/yaw share the
nose-steering plane authority; roll is a separate longitudinal-axis authority.
A 3-D normalization of (pitch,yaw,roll) that lets roll consume nose-steering
authority is forbidden.

### Isolated regression gates

These tests are first-class gates and should be run before composite/live
flight:

```bash
ctest --test-dir build/tests/navigation_runtime -R "^hull_pose_guidance$" --output-on-failure
ctest --test-dir build/tests/navigation_runtime -R "^hull_attitude_control$" --output-on-failure
ctest --test-dir build/tests/navigation_runtime -R "^corridor_capture_guidance$" --output-on-failure
ctest --test-dir build/tests/navigation_runtime -R "^route_speed_guidance$" --output-on-failure
```

Once a layer is green, later work on another layer must not weaken or replace
its contract. Composite autopilot tests come after all isolated layer gates.
