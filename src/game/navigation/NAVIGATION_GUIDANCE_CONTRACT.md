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

## 6. Local guidance Wave 4 behavior

`LocalGuidancePlanner` is deliberately small-distance and rolling-replan ready.
V1 creates one deterministic direct candidate to a moving semantic target,
propagates it through `TrajectoryPredictor`, validates it through
`TrajectorySafetyEvaluator`, and emits corridor frames that progressively align
to the predicted moving/rotating target.

If that direct candidate conflicts with a known conservative hazard envelope,
V1 tries two simple lateral detour candidates around the first conflict. Each
detour is itself built from predicted legs and is accepted only after the same
4D safety evaluator reports it clear. If neither side is safe the result is
`Blocked`. This is intentionally a small local fallback; later graph/continuous
optimization extends the planner rather than moving avoidance into predictor.

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

Wave 4 already includes a live Hub Motion Lab producer. It repeatedly resolves
the rotating cube docking gate, builds a short-lived local planning snapshot,
runs `LocalGuidancePlanner -> TrajectoryPredictor -> TrajectorySafetyEvaluator`,
and publishes the accepted `GuidanceCorridor` to the cockpit HUD. The local
planner also contains the first deliberately simple left/right lateral detour
strategy for a known blocking hazard.

After the Wave 4 ready harness is green, the next layers are:

1. long-range `RouteSolver` using preferred official lanes, schedules,
   restrictions and terminal-state matching;
2. richer local alternate-candidate search / continuous corridor replanning;
3. `TrajectoryFollower` closed-loop execution/autopilot;
4. real radar/transponder/beacon fusion into `NavigationPlanningSnapshot`;
5. server planning-query and ATC `GuidanceCorridor` transport/protocol.
