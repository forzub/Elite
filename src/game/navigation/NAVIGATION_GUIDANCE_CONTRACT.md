# Navigation Guidance Layer Contract

Status: Wave 4 foundation, pre-route-solver/autopilot.

## 1. One stack, separate responsibilities

Navigation guidance is a composition of independent modules. No renderer,
planner, predictor, sensor or network endpoint becomes the owner of the whole
stack.

- `NavigationPlanningSnapshot` is the immutable-by-convention planning picture.
- `TrajectoryPredictor` answers: **what happens if this acceleration program is
  executed in this gravity environment?**
- `TrajectorySafetyEvaluator` answers: **does that predicted trajectory violate
  known time-dependent safety envelopes?**
- `LocalGuidancePlanner` produces a short-range candidate for approach/docking/
  transit, validates it with the shared predictor+safety evaluator and emits a
  universal `GuidanceCorridor`.
- future long-range `RouteSolver`, server/ATC and mission/fleet systems publish
  the same `GuidanceCorridor` product.
- HUD/map presentation consumes corridor data and never decides flight physics.
- future `TrajectoryFollower` executes an accepted trajectory closed-loop; it is
  not part of Wave 4.

`TrajectoryPredictor` performs no server/radar/ATC I/O and does not choose which
side of an obstacle to pass. Planners propose; predictor propagates; safety
checks; planners replan.

## 2. Module and HUD-layer switches

`NavigationModuleState` is the runtime equipment/control seam. Computational
modules and presentation layers are deliberately independent.

Computational switches include trajectory prediction, safety evaluation, route
planning, preferred official-lane planning, scheduled traffic, local guidance,
server guidance and sensor fusion.

HUD switches include tactical target markers, route markers, guidance corridor,
galactic compass and flight-vector instrument. The experimental corridor HUD
defaults off while its planning/safety computation remains enabled.

Hiding a HUD layer must not implicitly disable safety/navigation computation.
Likewise disabling a data source/planner must not require deleting its renderer.
`SpaceState::setNavigationModuleEnabled()` / `toggleNavigationModule()` are the
binding seam for future cockpit buttons, equipment states and debug controls;
Wave 4 does not consume arbitrary new keyboard shortcuts.

## 3. Planning snapshot: official knowledge plus local refinement

A planning caller obtains a `NavigationPlanningSnapshot` for the spatial region
and time interval that a candidate route will traverse. Expected producers are:

- authoritative celestial/gravity data and ephemerides;
- official navigation lanes and beacon service;
- restricted/closed volumes;
- published large-vessel traffic schedules/flight intents;
- transponders and navigation beacons;
- later local radar/sensor observations.

The predictor never fetches this data itself. This makes a calculation
reproducible, testable offline and reusable by client and headless server.

### 3.1 One planning epoch, never presentation state

Every concrete plan has one `NavigationPlanningEpoch`. Client planning also
keeps a separate authoritative `sourceEpoch`: the source tick/server/universe
time identifies the canonical replication seed, while the planning epoch is the
single current time to which that complete seed has been resolved. Both remain
on the same universe-timeline revision. `renderTransform`, `renderWorldPosition`
and other presentation-interpolated values are not legal planning inputs.

`NavigationWorldPredictor` owns prediction from the authoritative seed to the
planning epoch. Orbital Hub motion is advanced as a curved co-moving frame;
Hub-attached modules are reconstructed from that predicted frame, Hub-tactical
ships advance in Hub-local coordinates, and unattached current-snapshot objects
use a bounded/simple constant-velocity model until richer dynamics are wired in.
No planner or `SpaceState` caller may advance only one participant independently.

For Hub-local planning the replicated source Hub pose uses the shared
`makeReplicatedHubKinematicFrame()` axis contract before prediction. Hub Map
composition and route planning therefore retain the same prograde/radial/normal
permutation even though their presentation/planning epochs may differ.

Navigation prediction is deliberately separate from authoritative simulation
integration. `NavigationWorldPredictor` may evaluate the known Hub orbit at the
planning epoch, but it must not replace `GameSimulation`'s production update or
change the global `OrbitalMotion` velocity semantics merely to satisfy route
planning. The same navigation predictor is reusable by a client-local planner or
a server-shared planner; execution placement changes, not the production world
state contract.

### 3.2 Navigation foundation lock

The time/frame foundation is a frozen dependency boundary before route-planner
replacement. Regression tests must preserve all of these invariants:

- one canonical authoritative source epoch seeds a calculation; the complete
  problem is then resolved to one planning epoch and one planning frame;
- world/local position, velocity and acceleration transforms remain reversible
  at large system-local coordinates;
- route calculation is input-pure with respect to simulation, replication, map
  resources and cloud presentation; only navigation workspace/output may change;
- navigation prediction reads production world semantics but does not replace
  `GameSimulation` integration or global orbit semantics;
- Hub attachment phase is evaluated at the consumer epoch with periodic angles
  reduced before float conversion, so large universe times cannot produce
  freeze-then-snap rotation;
- the Hub Motion Lab keeps the box as a slow 2 deg/s rotation probe and the
  cylinder static;
- client-local versus server-shared execution placement may change who executes
  the deterministic planner, never the planning contract or world authority.

Stage 4 may replace strategic path-search internals, obstacle geometry and route
costs. It must not weaken these foundation invariants.

Official lanes are preferred planning infrastructure, not rails. An open,
beacon-served lane normally carries a planning-cost advantage because position
is more predictable and traffic/safety policy is managed. A solver may still
choose free space when it is legal and its total cost/risk is better. Restricted
or closed lanes/volumes are constraints.

`NavigationPlanningSnapshotBuilder` is the sensor-fusion seam. A better radar
observation may reduce positional/velocity uncertainty, but it may not shrink an
authoritative physical radius or required separation envelope.

## 4. Safety is four-dimensional

Collision/safety reasoning is `X,Y,Z,time`, not static geometry.

For each trajectory segment the evaluator compares the ship at passage time
with moving obstacles and scheduled traffic at the same time. Safety separation
contains at least:

`ship envelope + hazard physical envelope + required clearance + navigation uncertainty`.

Published traffic is valid only in its declared sample time window extended by
explicit timing uncertainty. A ship is never frozen forever at the last
schedule point. Between samples, closest approach is evaluated continuously
with a linear segment approximation.

Wave 4 uses conservative spherical hazard envelopes. Detailed convex/mesh
collision volumes can later refine the safety evaluator without changing who
owns world knowledge or route choice.

## 5. Universal GuidanceCorridor

`GuidanceCorridor` is the common product presented to the player and later to a
trajectory follower. Sources may include:

- long-range RouteSolver;
- LocalGuidancePlanner;
- docking computer;
- station traffic control / ATC from the server;
- mission/fleet guidance;
- emergency control.

Purposes include transit, approach, docking, landing, cargo approach, obstacle
bypass, attack run, formation join, departure and emergency escape.

A corridor is a time-ordered sequence of `GuidanceFrame` cross-sections. Each
frame provides system-local position, orientation, width/height, tolerances and
recommended speed/closure rate. Therefore the same HUD vocabulary can show a
docking tunnel, a safe path between rocks, an ATC arrival lane or the next
seconds of a long autopilot transit.

The cockpit displays only a sliding time window of a potentially long corridor.
This creates useful optical flow during otherwise visually slow long-distance
automatic flight without requiring the renderer to know how the path was
computed.

`NavigationGuidanceState` may hold several corridors. Priority and freshness
select the active one, while `NavigationModuleState` can suppress an entire
source class independently from the HUD layer switch.

## 6. Local guidance: docking 6-DOF + rolling emergency fallback

`LocalGuidancePlanner` is deliberately small-distance and rolling-replan ready.
For generic approach/transit it retains the predictor + 4D safety pipeline. For
`Docking` it now builds a two-stage terminal candidate against the predicted
moving/rotating semantic gate:

1. an approach state outside the entrance plane;
2. an ingress state inside the dock whose terminal velocity is along the inward
   entrance normal.

`VehicleGuidanceEnvelope` carries canonical ship length/width/height. The
current safety evaluator uses its circumscribed radius as a conservative
swept-volume bound; the type is intentionally separate so oriented-box/mesh
sweeps can replace that first conservative implementation without changing the
planner/HUD contract. Docking `GuidanceFrame.orientation` is an explicit desired
hull pose (`requiredVehiclePose=true`): ship nose follows the tunnel and the
terminal pose aligns nose with the inward dock normal and top/belly with the
dock up direction. This remains advisory presentation only; no follower or
autopilot control is executed.

The planner still tries lateral detours through the same predictor+safety
pipeline. If no docking candidate is safe it attempts a separately validated
`EmergencyEscape` corridor. That corridor sets `noSafePrimarySolution`; the HUD
shows a flashing warning and the primary typed docking request remains intact.
A later rolling replan therefore replaces the escape tunnel with the original
docking tunnel automatically as soon as a safe solution exists again.

## 6a. Shared geometric path planning

`CALCULATE ROUTE` now consumes one immutable current planning snapshot and
composes docking semantics around the shared `world::navigation::GeometricPathPlanner`.
The repair drone uses the same planner. There is no second visibility-graph
implementation for small craft and no client-only strategic obstacle type.

The canonical obstacle geometry is `world::navigation::NavigationObstacle`:

- `Sphere`;
- oriented `Box` / OBB;
- `Capsule`.

Geometry uses double-precision positions/bases and carries authored clearance.
Dynamic motion/observation data wraps this geometry in `NavigationObstacleState`;
it does not redefine physical size or shape. The client planning snapshot builds
Hub module geometry from the predicted object pose at the same planning epoch.
The future server-shared execution path must call the same geometry adapter and
planner; computation placement may change, the algorithm may not.

`GeometricPathPlanner` owns only spatial collision-free path search:

1. direct line-of-sight when clear;
2. deterministic support nodes for sphere/OBB/capsule geometry;
3. a 3D visibility graph searched with A*;
4. line-of-sight shortcutting of the resulting polyline.

It has no velocity, thrust, arrival-time or autopilot semantics. Current ship
velocity was deliberately removed from geometric planning; dynamic feasibility
belongs to the later `TrajectoryGenerator` stage.

`DockingPathPlanner` is a thin semantic composer. It asks the generic planner
for a path from the ship to the authored approach point, while treating the
target module as solid during transit. Only the final semantic docking ingress
may enter the target obstacle; every other obstacle remains active. The final
segment is exactly parallel to the inward dock normal.

Legacy `StrategicTrajectoryPlanner`, `ObstaclePathPlanner` and
`buildSimpleAvoidancePath` implementations are removed.

## 7. Hub construction semantics are not mesh geometry

Docking/landing/attack/service/navigation elements use `HubSemanticAnchor` data.
An anchor has a stable semantic ID, kind, module-local pose, dimensions,
clearance and optional entry-speed policy. Runtime resolution combines that
semantic pose with the authoritative module position, orientation, linear
velocity and angular velocity, including `omega x r` velocity for off-axis
features.

Test and production meshes must not encode these semantics in C++. Replacing an
OBJ must not require navigation-code changes.

Wave 4 diagnostic assets live under:

`assets/models/hub/guidance_test/`

and semantic anchors under:

`assets/data/navigation/hub_semantic_anchors.json`.

The Hub Motion Lab currently adds an external-mesh box/parallelepiped and
cylinder docking module several kilometres apart. Their authored corridor axis
is local Z, which the hub visual basis aligns with orbital prograde/retrograde.
The box is the single slow-rotation geometry probe and rotates at 2 deg/s around
that local-Z/docking axis. The cylinder is deliberately non-rotating. Both
meshes use narrow rectangular end slots leading into a through corridor.

## 8. Galactic compass

The cockpit compass uses standard galactic coordinates:

- galactic longitude `l`, with Galactic Center at `l=0 deg` and Galactic
  Anticenter at `l=180 deg`;
- galactic latitude `b`, with North/South Galactic Pole at `+90/-90 deg`.

`l=90` and `l=270` stay neutral `L90/L270` presentation labels; no invented
physical cardinal nomenclature is embedded in the coordinate contract.

The compass uses the same `galactic_center_dir` / `galactic_north_dir` from
`assets/data/galaxy/milky_way.json` as the Milky Way presentation. It displays
**ship-nose orientation**, while the existing flight-vector instrument continues
to display actual motion. Those may legitimately disagree in Newtonian flight.

## 8a. Spatial computation placement is not entity authority

Expensive deterministic spatial work is placed as close to the only consumer as
possible. A server-resolved **consistency domain** supplies the number of human
participants that must observe the same result:

- one human participant: run eligible route/prediction/autopilot calculation on
  that client;
- two or more human participants: run the calculation on the server and publish
  the shared result;
- explicit security/persistence/ATC/fleet policy or missing client capability may
  require server execution even for one participant.

This applies to any spatial domain in the universe, not only Hub maps. The same
server-neutral planner/predictor/evaluator implementation is used in either
placement; only the executor changes. The client must never infer "I am alone"
from render visibility or its replication-interest set. The participant count or
final placement decision is server-resolved.

`SpatialComputationPlacement` is deliberately separate from
`simulation::AuthorityPolicy`: physical entity state may remain server-authoritative
while a deterministic calculation is executed client-side.

## 9. Network/server boundary

For a shared consistency domain the authoritative server answers planning
queries with relevant official lanes, restrictions and scheduled traffic for the
candidate cubes/time window and may publish ATC `GuidanceCorridor` products
directly. For an eligible single-participant domain the same data contract can
be hydrated to the client and the common solver executes there instead. Local
sensors may then refine the client's planning snapshot.

None of that changes `TrajectoryPredictor`, solver or HUD rendering contracts;
network placement chooses an executor, not a different navigation algorithm.

## 10. Wave 4 live laboratory and next implementation layers

The live client producer is now request-driven rather than hard-wired to one
Hub Motion Lab gate. `CALCULATE ROUTE` stores a stable semantic docking request;
`SpaceState` resolves that module/anchor from ordinary replicated state, checks
current dock compatibility, and replans about every 0.2 s from the ship's actual
position/velocity/orientation/angular-rate state. The selected rotating anchor
keeps its module rotation center, so future gate positions follow circular
rotation instead of tangent-line extrapolation.

The request is scoped to the selected dock information card. Closing that card
**with its own `X`** means **cancel docking guidance**: the typed request is cleared, the active
corridor expires/gets erased on the next update, and tracking reconciliation
removes its cockpit marker. Repeated dock clicks, empty-map deselection and
selection of another object do not close the card and therefore do not cancel
the request. Hub docking ports are projected as direct semantic interaction
targets for every authored enabled port, with the exact projected
opening rectangle taking priority over parent-module CPU-mesh selection.

The active corridor remains a shared planning product, but the current live
validation slice is deliberately narrowed to **calculation + map trajectory**.
Cockpit GuidanceTunnel code is not modified by this slice.

Docking target motion is sampled in a short-horizon Hub co-moving frame. A Hub
attached to an orbiting parent therefore follows the curved Hub motion instead
of being extrapolated forever along its instantaneous world-space tangent. The
planner may integrate the ship in the inertial/system frame, but terminal
position/velocity are compared against the moving dock at the same future time.
A candidate is not `Ready` merely because it is collision-free: it must satisfy
terminal position and relative-velocity tolerances. The physical predictor end
is never snapped to the requested dock point for presentation.

During solver validation, System/Detail/Hub maps render the supplied **raw
planner samples as one solid non-blinking polyline**, without dashes, sample
dots, endpoint decoration or Bezier smoothing. On Hub Map each future world
sample is transformed through the Hub co-moving pose for that sample time, so
common orbital translation/rotation does not appear as a hundreds-of-kilometres
tangent laser. The first physical sample remains in the line so the path stays
attached to its rolling-plan start. Rotating attached modules advance from their
epoch-0 local rotation with the shared Hub universe time, so the visible dock
and semantic/planner dock use the same phase. Raw-end/required-terminal errors
remain available in `docking_guidance_trace.txt` instead of being decorated onto
the validation line.

A single `NoTerminalSolution` result between successful 0.2 s replans does not
immediately erase the last accepted map corridor. Presentation may retain that
accepted corridor for at most 0.65 s from its original generation time; repeated
failures cannot extend the grace. `Blocked`, invalid and prediction-failure
results still remove guidance immediately. This hysteresis changes only map
stability, never planner acceptance or collision safety.

The Galaxy map intentionally does not draw local docking guidance: at galactic
scale the metre/kilometre corridor has no useful screen extent.

Current order after this docking-guidance slice:

1. validate/tune terminal convergence and the raw Hub-local map trajectory in
   the live Hub Motion Lab;
2. after the map calculation is trusted, return to live GuidanceTunnel/HUD
   validation;
3. replace the conservative spherical vehicle sweep with oriented/swept-volume
   collision checking and richer alternate-candidate search;
4. connect real radar/transponder/beacon fusion into `NavigationPlanningSnapshot`;
5. build long-range `RouteSolver` and server planning-query/ATC corridor transport.

`TrajectoryFollower` / autopilot is explicitly **not** part of this stage.
