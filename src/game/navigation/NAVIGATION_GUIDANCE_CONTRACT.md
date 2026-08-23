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

The Hub Motion Lab currently adds external-mesh rotating cube/cylinder docking
modules several kilometres apart. Their authored corridor axis is local Z,
which the hub visual basis aligns with orbital prograde/retrograde. Both meshes
use narrow rectangular end slots leading into a through corridor and rotate
slowly around that same local-Z/docking axis.

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

## 9. Network/server boundary

The authoritative server will eventually answer a planning query with relevant
official lanes, restrictions and scheduled traffic for the candidate cubes and
time window. It may also publish ATC `GuidanceCorridor` products directly.
Local sensors later refine the client's planning snapshot.

None of that changes `TrajectoryPredictor` or HUD rendering contracts.

## 10. Wave 4 live laboratory and next implementation layers

The live client producer is now request-driven rather than hard-wired to one
Hub Motion Lab gate. `CALCULATE ROUTE` stores a stable semantic docking request;
`SpaceState` resolves that module/anchor from ordinary replicated state, checks
current dock compatibility, and replans about every 0.2 s from the ship's actual
position/velocity/orientation/angular-rate state. The selected rotating anchor
keeps its module rotation center, so future gate positions follow circular
rotation instead of tangent-line extrapolation.

The request is scoped to the selected dock information card. Closing that card
means **cancel docking guidance**: the typed request is cleared, the active
corridor expires/gets erased on the next update, and tracking reconciliation
removes its cockpit marker. Hub docking ports are projected as direct semantic
interaction targets for every authored enabled port, with the exact projected
opening rectangle taking priority over parent-module CPU-mesh selection.

The same active corridor has two presentation consumers and neither owns
planning physics:

- System/Detail/Hub maps project the supplied corridor centers and draw a dashed
  cubic-Bezier-smoothed planned path through those samples;
- the Flight cockpit HUD consumes the full `GuidanceFrame` pose sequence and
  renders the 6-DOF GuidanceTunnel.

The Galaxy map intentionally does not draw local docking guidance: at galactic
scale the metre/kilometre corridor has no useful screen extent.

Current order after this docking-guidance slice:

1. validate/tune the 6-DOF GuidanceTunnel and emergency recovery in the live Hub
   Motion Lab;
2. replace the conservative spherical vehicle sweep with oriented/swept-volume
   collision checking and richer alternate-candidate search;
3. connect real radar/transponder/beacon fusion into `NavigationPlanningSnapshot`;
4. build long-range `RouteSolver` and server planning-query/ATC corridor transport.

`TrajectoryFollower` / autopilot is explicitly **not** part of this stage. It
comes only after the displayed docking trajectory and GuidanceTunnel are stable.
