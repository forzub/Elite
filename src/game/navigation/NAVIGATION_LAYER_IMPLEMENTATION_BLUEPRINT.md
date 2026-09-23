# Elite Navigation Layer — normative architecture and migration blueprint

Status: **normative target architecture**

Date: 2026-09-22

Scope: static and dynamic 3D navigation for one to many thousands of ships,
drones, missiles and other autonomous actors.

This document is the primary implementation specification for the navigation
layer. It supersedes the accidental architecture embodied by the current
`tools/navigation_runtime` execution path whenever that path conflicts with the
ownership, dataflow or acceptance rules below.

The document does not declare the current implementation complete. It separates:

- what exists and may be preserved;
- what is transitional;
- what is architecturally wrong and must not be extended;
- the target semantic model;
- the algorithms and APIs required to reach that model;
- the migration order and evidence required at every gate.

---

## 1. Required result

Elite needs one navigation layer that can answer two different questions without
confusing them:

1. **Where can an actor travel through the world?**
2. **What physically executable maneuver should this particular actor perform
   now?**

The finished system must support:

- unconstrained 3D flight in sparse space;
- dense station, wreck, canyon and debris geometry;
- real openings, tunnels, breaches, docking portals and narrow slits;
- ships with different sizes, shapes, masses, engines and control laws;
- static and moving obstacles;
- stationary, moving and oriented terminal states;
- ordinary, extreme, squeeze, docking, formation, pursuit, repair and attack
  behavior;
- automatic flight and a visible manual-navigation corridor;
- hundreds or thousands of actors without all-pairs checks or full replanning
  every frame;
- deterministic server-authoritative execution and reproducible tests.

The central invariant is:

```text
The object that is accepted for execution is the exact physical maneuver that
was capability-checked and collision-proved.
```

No downstream block may reinterpret an accepted geometric curve into a different
physical maneuver.

---

## 2. Executive diagnosis of the current model

The project already contains useful foundations:

- `NavigationSpace` for persistent static topology;
- `NavigationMap` for dynamic broadphase candidates;
- `NavigationWorkScheduler` for bounded asynchronous work;
- `VehicleDynamicsProfile` and canonical `ShipDynamics` projections;
- `AcceptedManeuverProgram` as a bounded value product;
- a Planner/Follower ownership direction;
- local prediction, passage evaluation and exact obstacle queries;
- benchmark harnesses at 1k/5k/10k scale.

Those pieces should be preserved and completed.

The active `tools/navigation_runtime` path, however, is not the target
architecture. Its effective order is:

```text
scenario primitives
 -> visibility-graph polyline
 -> locally rounded Bezier guide
 -> scalar Ruckig progress
 -> attitude fitted after timing
 -> propulsion fitted after attitude
 -> AcceptedManeuverProgram pages
 -> acceleration-vector Follower
 -> propulsion allocated again by physics
```

This order is invalid for a main-engine-dominant rigid body. It decides where
and when the ship moves before proving that the hull can rotate and that the
installed actuators can create the required force at that time.

The correct order is:

```text
navigation world
 -> global route corridor
 -> local geometric maneuver candidates
 -> hull/actuator/resource feasibility
 -> continuous swept-hull proof
 -> time parameterization
 -> accepted state + actuator program
 -> literal execution plus bounded correction
```

Ruckig remains useful, but only as an inner timing solver after the physical
maneuver family and its constraints are known.

---

## 3. Terminology and semantic boundaries

The following names are normative.

### 3.1 World Truth

Authoritative gameplay state:

- static collision geometry;
- moving actor transforms and motion;
- damage and geometry revisions;
- vehicle instance capabilities and resources;
- objectives and terminal requirements;
- authoritative universe time and reference frames.

World Truth owns facts. Navigation consumes immutable snapshots or narrow
read-only query capabilities. Navigation must never reach back into
`GameSimulation` or a descriptor registry for missing facts.

### 3.2 Navigation World

A search-oriented representation derived from World Truth:

- persistent static free-space topology;
- exact static collision geometry;
- dynamic spatial index;
- clearance information;
- revisions and source identity.

It is not perception. NPCs do not need to cast rays to discover a world already
known to the simulation.

### 3.3 Route

A coarse, reusable global answer describing which free-space regions and portals
connect the actor's current region to the goal region.

A route is not a trajectory, not a list of throttle commands and not a promise
that a turn can be flown at the actor's current speed.

### 3.4 Route Corridor

The ordered free-space tube implied by a route:

- region sequence;
- portal sequence;
- portal geometry and traversal semantics;
- clearance bounds;
- permitted local free-space envelope.

Local maneuver generation may move anywhere inside the corridor. It does not
have to touch mathematical waypoint points.

### 3.5 Maneuver Candidate

A short, receding-horizon physical proposal containing:

- maneuver family;
- time-ordered rigid-body state;
- body attitude and angular state;
- actuator allocation;
- resource consumption;
- reserved correction authority;
- route/corridor provenance.

It is not executable until continuously proved.

### 3.6 Proven Maneuver

A Maneuver Candidate plus successful witnesses for:

- capability;
- continuous static clearance;
- continuous dynamic clearance for the accepted horizon;
- resource availability;
- reference-frame and revision validity.

### 3.7 Accepted Maneuver Program

The immutable program selected from proven candidates. It is the only normal
command authority consumed by Autopilot/Follower.

### 3.8 Autopilot/Follower

Executes the accepted program. It may add bounded feedback from authority
reserved by the Planner. It does not invent another route or another maneuver.

### 3.9 Safety Monitor

Checks whether the assumptions and proof of the accepted program remain valid.
It may trigger bounded reflex behavior and request replanning. It does not
silently mutate the accepted program.

---

## 4. Complete problem inventory

### 4.1 Architectural aggregation in `NavigationScenarioRuntime.cpp`

One translation unit currently owns:

- JSON parsing;
- scenario defaults;
- vehicle adaptation;
- route planning;
- path rounding;
- trajectory construction;
- attitude authoring;
- propulsion fitting;
- program paging;
- Follower execution;
- physics orchestration;
- collision diagnostics;
- trace and file output.

Explicit parameters alone do not make this clean architecture. The file is both
composition root and implementation of multiple domain layers. This makes it
easy for test-only behavior to become production semantics and prevents an
independent replacement of any layer.

Required correction: split orchestration from domain components. The runtime
tool must compose the same production APIs as the game; it must not own a second
navigation implementation.

### 4.2 Coordinate boundary violation

The runtime defines incompatible `NavigationLocalControlIntent` and
`NavigationSystemControlIntent` types, but its private `toSystemIntent()` copies
vectors without using `NavigationFrameBoundary`.

The same assumption appears when the execution vehicle writes NavLocal position
and basis directly into world/system transform fields.

Consequences:

- behavior is only correct for an identity frame;
- translated/rotated/moving/rotating frames are not actually supported;
- angular state is inconsistent with a rotating reference frame;
- tests can pass while production coordinates are wrong.

Required correction:

- all world/NavLocal conversion goes through `NavigationFrameBoundary`;
- delete the tool-local conversion function;
- construct the initial world transform from the frame boundary;
- add non-identity, moving and rotating-frame E2E fixtures.

### 4.3 Purity is tested lexically rather than semantically

Current Python architecture checks search for exact source strings. They can:

- fail after harmless formatting changes;
- mistake an explicitly populated DTO default for hidden world input;
- miss an actual semantic bypass implemented under another helper name.

Required correction:

- retain lightweight textual guards only for forbidden includes/symbols;
- add compile-time type incompatibility and dependency tests;
- add behavior tests using non-default frames and policies;
- where practical, build pure kernels in isolated targets that cannot link I/O,
  descriptor or simulation owners.

### 4.4 No generated static 3D navigation volume in the active stand

The stand passes obstacle primitives directly to a per-request geometric
planner. It does not exercise `NavigationSpace`, a navigation-volume builder,
tiles, hierarchical topology or reusable corridors.

Required correction: the stand scenario must publish geometry into the same
static navigation-world builder used by production, then request a corridor
through `NavigationSpace`.

### 4.5 `GeometricPathPlanner` does not scale as a global planner

The planner creates support points around every selected primitive and tests a
near-complete visibility graph. Its practical work grows roughly as:

```text
O(support_nodes^2 * obstacle_count)
```

It also has a `maxConsideredObstacles` policy that can discard obstacles based
on distance from the direct start-goal segment. That is not a valid global
completeness rule: a required detour may enter geometry initially considered
far from that segment.

Required correction:

- demote this planner to a bounded local fallback/debug tool;
- global planning must search persistent sparse free-space topology;
- obstacle filtering must come from a spatial index and corridor bounds, not an
  arbitrary count cutoff.

### 4.6 Primitive-only geometry cannot represent the intended world robustly

Box/sphere/capsule primitives are useful collision inputs, but a global route
planner built only from their support points cannot robustly encode:

- arbitrary station hull holes;
- compound apertures;
- concave interiors;
- wreckage tunnels;
- damaged breaches;
- large mesh-derived free-space regions.

Required correction: voxelize authoritative collision geometry into a tiled
sparse navigation volume while retaining exact primitives/meshes for the final
narrow proof.

### 4.7 A conservative sphere is used beyond its proper role

The largest body half-extent becomes one collision radius. This is acceptable
for coarse topology and broadphase but rejects valid slit passages and says
nothing about clearance during hull rotation.

Required correction:

- use sphere/capsule envelopes for coarse reachability and candidate reduction;
- use oriented hull proxies or convex decomposition for local continuous proof;
- squeeze mode must search orientation-dependent configuration states.

### 4.8 Stage-1 clearance is a heuristic, not physical reachability

The current additional clearance is roughly speed multiplied by a representative
turn time. It does not consider:

- actual corner angle;
- current angular velocity;
- direction of current velocity;
- braking versus acceleration authority;
- control-law family;
- simultaneous rotation and thrust;
- obstacle geometry after the next corner.

It may remain a conservative route-cost hint. It must not be described as proof
that a route can be flown.

### 4.9 Geometry/timing inversion

`TrajectoryGenerator` fixes a path and scalar timing before hull attitude and
force allocation are known. A Newtonian ship can therefore be told to brake in
a direction that its aft main engine cannot yet point toward.

Required correction: physical maneuver family, attitude schedule and actuator
topology precede final timing.

### 4.10 Globalization of local speed restrictions

For multi-point routes the current scalar solve calculates one global speed
ceiling from:

- the minimum speed-range limit anywhere on the path;
- non-terminal point constraints;
- the maximum sampled curvature anywhere on the guide.

This violates the stated doctrine that a local difficult corner must not limit
unrelated clear segments.

Required correction: represent speed constraints as functions or intervals over
route progress and solve forward/backward reachable speed envelopes, then time
parameterize the local physical phases.

### 4.11 Path capture is diagnosed but not planned

Initial cross-track velocity sets a diagnostic flag, but the scalar path solve
immediately assumes motion along the guide. The first stored sample is then
overwritten with the authored initial velocity, creating a discontinuity between
sample zero and later samples.

Required correction: compile an explicit capture/rejoin maneuver from the actual
six-degree-of-freedom state, or reject the corridor as unreachable from the
current state.

### 4.12 Terminal state is overwritten instead of solved

The final trajectory sample is assigned the requested terminal velocity and
orientation after the scalar solve. Assignment is not reachability.

Required correction: terminal position, velocity, acceleration, attitude,
angular velocity and tolerance are boundary conditions of the physical maneuver
solve. If they cannot be met, the Planner must return failure or a different
maneuver family.

### 4.13 Duplicate attitude authority

`TrajectoryGenerator` stores an orientation, then the runtime separately calls
`buildReferenceAttitudes()` and authors another attitude stream. Two blocks can
therefore disagree about the same reference state.

Required correction: one physical maneuver compiler owns `q/omega/alpha`.
Ruckig or another timing primitive may support that compiler but may not publish
a competing orientation.

### 4.14 Physical compiler exists but is absent from the active path

`OrdinaryPhysicalManeuverCompiler` is not linked into the runtime stand. The
documents name it as the target P6 block while the tested E2E bypasses it.

Required correction: no E2E may claim the target architecture until the active
route-to-execution chain calls the physical compiler and continuous prover.

### 4.15 Existing physical compiler is only a seed implementation

The current compiler:

- supports Newtonian only;
- does not use target position when shaping motion;
- does not preserve non-zero initial angular velocity in emitted samples;
- offers a simple rotate-then-burn sequence;
- does not emit literal actuator segments;
- does not prove geometry;
- cannot author flip-and-burn, curved main-engine turns, precision capture or
  squeeze transit.

It should be extended, not mistaken for a finished solution.

### 4.16 Physically infeasible programs are still accepted

`makeProgramPhase()` records `propulsionFeasible=false`, yet sets the program
valid and executes it. This makes feasibility a diagnostic instead of an
acceptance condition.

Required invariant:

```text
if any interval is physically infeasible, no AcceptedManeuverProgram exists.
```

### 4.17 Planned actuators are observe-only

The accepted program carries main/fore/RCS interval data, but Follower outputs
an abstract acceleration intent. `DynamicMotionSystem` then allocates engines
again from the abstract vector.

Required correction:

- Autopilot samples the literal planned actuator schedule;
- feedback is separately allocated from the reserved authority;
- the two are combined under hardware/resource limits;
- physics executes the combined physical channels without reinterpreting the
  maneuver family.

### 4.18 Program interpolation is not kinematically consistent

The sampler linearly interpolates position, velocity and acceleration
independently. It also interpolates body axes and angular derivatives
independently. Therefore:

- `dP/dt` need not equal stored velocity;
- `dV/dt` need not equal stored acceleration;
- orientation derivative need not equal stored angular velocity.

Required correction: use a representation with internally consistent segment
polynomials or sample the original time law directly. Attitude interpolation
must use quaternion interpolation consistent with angular state.

### 4.19 Storage-page time semantics remain fragile

Pages correctly share one accepted time, but Follower completion compares
elapsed maneuver time with a page-local end offset without adding
`sequenceStartOffsetSeconds`.

Required correction: introduce a `ManeuverProgramView`/page-indexing layer that
owns global maneuver time. Follower should not manually reconstruct page time.

### 4.20 Misnamed tracking quantities

Follower reports full position-reference error as cross-track error. This hides
the distinction between harmless along-track phase drift and corridor departure.

Required correction: report separately:

- along-track error;
- cross-track error;
- corridor clearance;
- velocity error decomposed along/across the local tangent;
- attitude and angular-state error.

### 4.21 Follower is still a second maneuver author

The controller converts reference errors into arbitrary acceleration vectors.
When the accepted feed-forward is impossible or absent, this feedback becomes
the actual maneuver and may be assigned mostly to RCS.

Required correction: bounded feedback is allowed only inside explicitly
reserved authority. Exceeding the reserve invalidates the accepted maneuver and
requests replanning.

### 4.22 Dynamic obstacles are not part of the runtime E2E

The sudden-obstacle and dynamic actor inputs are explicitly ignored. The current
stand therefore proves nothing about dynamic avoidance, pursuit or many-agent
interaction.

Required correction: add dynamic publication and local query only after the
static physical maneuver chain is truthful. Dynamic actors must not rebuild the
global static route every frame.

### 4.23 Continuous collision proof is missing from the accepted path

The execution path checks chords between dense samples against inflated
obstacles. It does not prove the swept oriented hull while rotating.

Required correction: use adaptive continuous intervals with conservative bounds
over translation and rotation. A sample/chord check may be an early rejection,
not the final witness.

### 4.24 `NavigationSpace` has no production geometry-to-topology builder

Its region/portal API is useful, but current benchmarks generate synthetic AABB
lattices. A real station/world builder is still required.

### 4.25 Static exact-obstacle queries scan all obstacles

`NavigationSpace::querySegment(... exactObstaclesOnly)` iterates the complete
static obstacle collection. For many agents and large worlds this becomes a
hidden `agents * obstacles` cost.

Required correction: exact obstacles need their own tile/BVH index, queried by
the swept candidate bounds.

### 4.26 Dynamic publication and query copying are prototype-grade

`NavigationMap` rebuilds actor/cell storage on whole-world replacement and
copies exact obstacle vectors into each query result. This is clean by-value API
semantics but expensive for high-frequency thousands-of-agent publication.

Required correction:

- immutable double-buffered snapshots;
- stable compact shape handles into snapshot-owned geometry;
- pooled query buffers or caller-provided bounded output;
- incremental cell update where measurement proves whole-snapshot rebuild too
  expensive;
- no borrowed mutable owner state across worker boundaries.

### 4.27 Dynamic broadphase uses one global maximum motion expansion

One unusually fast or large actor can enlarge every query AABB because the grid
range uses a maximum swept-radius upper bound.

Required correction: index motion AABBs into every overlapped cell, or maintain
speed/size classes so a global maximum does not poison all queries.

### 4.28 Static route search is worker-grade, not per-frame per-agent work

Measured 10k-region costed searches are several milliseconds and turn-aware
search can be much more expensive. That is acceptable asynchronously, not once
per actor per frame.

Required correction: schedule, cache and share route/corridor products. Most
ticks should execute an existing local program, not search global topology.

### 4.29 Behavior modes are mixed with geometry and hardware

`Standard/Extreme` currently changes a clearance factor. That is insufficient;
mode affects costs and allowed maneuver families, while hardware facts remain
unchanged.

Required correction: explicit doctrine profile described in Section 12.

### 4.30 Green component tests do not prove the real chain

Many tests either:

- generate the maneuver directly in the fixture;
- test Ruckig without Follower/physics;
- test Follower/physics without production route authoring;
- use identity frames;
- use spheres or coarse envelopes only;
- omit dynamic actors and resource exhaustion.

Required correction: retain unit tests but add product-chain tests that use the
same APIs and products as production without test-authored intermediate motion.

---

## 5. Target semantic structure

The navigation layer is divided into six vertical layers and four cross-cutting
services.

```text
L0  World truth adapters
L1  Navigation-world publication and indexes
L2  Global route/corridor planning
L3  Local physical maneuver generation and proof
L4  Maneuver decision and acceptance
L5  Autopilot execution and safety monitoring

Cross-cutting: scheduler, revisions/cache, replication, diagnostics
```

Each layer may depend only on the layer directly below it or on a declared
cross-cutting value API. Execution does not call back into route planning.

---

## 6. L0 — World truth adapters

### Purpose

Convert authoritative gameplay state into immutable navigation values.

### Inputs

- authoritative transforms and velocities;
- collision assets/hit volumes;
- ship capabilities and damage state;
- terminal objective;
- reference frame;
- world and capability revisions.

### Outputs

- `StaticNavigationSourceSnapshot`;
- `DynamicNavigationSourceSnapshot`;
- `NavigationAgentState`;
- `NavigationAgentCapability`;
- `NavigationObjective`;
- `NavigationDoctrine`.

### Rules

- concrete descriptor lookup ends here;
- all coordinate conversion uses `NavigationFrameBoundary`;
- no planner receives `GameSimulation&`, `ShipDescriptor&` or mutable entity
  state;
- damage/resource changes increment capability revision;
- shape identity and geometry revision are explicit.

---

## 7. L1 — Navigation-world publication and indexes

### 7.1 Static navigation volume

Recommended backend: tiled sparse voxel octree with an automatically derived
coarse region/portal graph.

The public `NavigationSpace` API can remain backend-neutral.

#### Build algorithm

1. Partition the active world into streamable navigation tiles.
2. Voxelize authoritative collision geometry per tile.
3. Mark solid/free cells conservatively.
4. Build an adaptive octree: merge uniform free or solid children.
5. Compute conservative clearance from each free cell to solid geometry.
6. Connect adjacent free leaves through portals.
7. Merge leaves into coarse regions where clearance/connectivity permit.
8. Construct a higher-level region graph across tiles.
9. Retain exact collision shape handles in a tile BVH for local proof.
10. Publish the new immutable snapshot with source and derived revisions.

#### Multiple vehicle sizes

Do not build one map per ship instance.

Store clearance on cells/portals. A route query supplies its required coarse
radius. A portal is traversable when:

```text
available_clearance >= agent_coarse_radius + doctrine_clearance
```

For a small number of common size bands, optional precomputed connectivity
islands may accelerate queries. Exact oriented fit remains a local proof.

#### Runtime geometry changes

- invalidate only intersecting tiles/regions;
- fail closed while a tile is rebuilding;
- rebuild asynchronously;
- publish transactionally;
- invalidate cached corridors by affected tile/region revision, not by one
  world-wide boolean whenever possible.

### 7.2 Exact static geometry index

Each static tile owns a BVH over exact navigation collision shapes. Query input
is a swept AABB or capsule/OBB interval bound. The result is a bounded set of
shape handles copied by value or retained through immutable snapshot ownership.

No continuous proof may iterate every obstacle in the world.

### 7.3 Dynamic spatial index

Recommended first production backend: double-buffered spatial hash or loose
octree over conservative motion AABBs.

For each dynamic actor publish:

- position, velocity, acceleration bound;
- angular velocity bound;
- broadphase radius;
- exact shape handle or bounded convex proxies;
- motion and capability revisions;
- predicted motion horizon validity.

Insert an actor into every cell intersected by its motion AABB over the indexed
horizon. This avoids expanding every query by the globally fastest actor.

### 7.4 NavigationWorldSnapshot

One immutable value identifies a coherent set of:

- static topology revision;
- exact static geometry revision;
- dynamic snapshot revision;
- frame identity/revision;
- publication time.

Planning jobs capture this identity. Results from stale snapshots may be useful
as hints but cannot be accepted without revalidation.

---

## 8. L2 — Global route and corridor planning

### Purpose

Find a reusable coarse route through persistent free space. This layer knows
nothing about throttle samples or Follower gains.

### Route request

```text
RouteRequest
  actor/agent class key
  start NavLocal position
  goal region/position
  required coarse radius
  doctrine cost profile
  required semantic checkpoints
  static topology revision
  maximum search cost/budget
```

### Hierarchical algorithm

1. Locate start and goal free-space leaves.
2. Search the coarse region/tile graph with A* or Dijkstra.
3. Restrict the detailed search to the selected coarse corridor.
4. Search octree leaves/portals within that corridor.
5. Apply a 3D funnel/string-pulling equivalent where valid.
6. Return ordered regions and portals plus clearance bounds.
7. Do not manufacture a physically timed trajectory.

### Route cost

Cost is doctrine-dependent and may include:

- distance;
- inverse clearance/risk;
- coarse turn cost;
- portal alignment difficulty;
- congestion reservation;
- known hazard exposure;
- mission-specific forbidden/preferred volumes.

Cost never changes installed vehicle capability.

### Reuse and caching

Cache by:

```text
(start_region, goal_region, size_band, doctrine_class,
 relevant_static_revisions)
```

Actors with the same destination may share:

- a coarse corridor;
- a reverse shortest-path tree;
- a flow field over a local hub/station topology.

The actor-specific physical maneuver is never shared blindly because current
state, orientation, damage and local traffic differ.

---

## 9. L3 — Local physical maneuver generation

### Purpose

Convert the next part of a route corridor and the actor's measured rigid-body
state into a small bounded set of executable maneuver candidates.

This is a receding-horizon planner. It should normally plan only far enough to
cover:

- stopping/avoidance horizon;
- the next meaningful portal or corner;
- a small time window, for example 2–6 seconds depending on speed and mode.

### Input

```text
ManeuverQuery
  measured NavigationAgentState
  NavigationAgentCapability + resources
  LocalFlightControlLaw
  RouteCorridorWindow
  local exact static candidates
  local dynamic candidates
  terminal requirement if inside horizon
  pilot/control latency reserve
  doctrine
  world/revision/time identity
```

### Candidate families

At minimum:

- `Coast`;
- `TrimRcs`;
- `MainBurn`;
- `LeadRotateMainBurn`;
- `CurvedMainBurn`;
- `Brake`;
- `FlipAndBurn`;
- `DriftTurn`;
- `StopTurnGo`;
- `PortalCapture`;
- `PrecisionTransit`;
- `FormationCapture`;
- `DockingCapture`;
- `EmergencyAvoidance`;
- `SqueezeAlignTransit`.

### Newtonian doctrine

For a rear-main-dominant vehicle:

- velocity and nose are independent states;
- main thrust is available only along the current/reachable thrust axis;
- braking may require lead rotation or flip-and-burn;
- RCS is trim/correction authority unless hardware states otherwise;
- a broad drifting arc may be preferable to braking;
- corridor sizing includes rotation and delayed thrust authority.

### Assisted doctrine

Assisted flight may stabilize velocity relative to the hull, but it may use only
installed hardware. The control law cannot synthesize a fore engine. If a ship
has rear main only, reverse longitudinal acceleration still requires rotation or
bounded RCS.

### Candidate generation method

Use a bounded motion-primitive lattice rather than unconstrained optimization as
the first production implementation:

1. Derive a small set of target states from the corridor window.
2. Generate applicable maneuver families from current state and control law.
3. Solve attitude reachability for each family.
4. Allocate installed actuators and reserve feedback authority.
5. Integrate the candidate rigid-body state forward.
6. Reject capability/resource violations.
7. Send surviving candidates to continuous proof.

This is deterministic, debuggable and naturally bounded. More advanced optimal
control/MPC may replace individual families later without changing the API.

### Force and actuator allocation

For every interval solve:

```text
required_world_force
 -> body coordinates at q(t)
 -> rear main / fore main / RCS / gimbal allocation
 -> saturation and resource check
```

The result stores literal physical channels, not just a net acceleration.

### Resource state

Capability includes time-varying constraints:

- RCS gas/pressure;
- fuel or energy budget where applicable;
- damaged/disabled thrusters;
- throttle slew;
- gimbal range/rate;
- thermal or duty-cycle limits if modeled.

The Planner must not publish a program whose assumed resources disappear before
the end of its horizon.

### Time parameterization

Only after the maneuver geometry, attitude phases and actuator family are known:

- use Ruckig for scalar progress or individual bounded phase transitions;
- use forward/backward speed-envelope propagation for spatially varying limits;
- preserve continuity of `P,V,A` and `q,omega,alpha`;
- include actuator slew and phase-boundary conditions.

Ruckig is an internal numerical service, not the owner of route or maneuver
semantics.

---

## 10. Continuous maneuver proof

### Static proof

For each candidate interval:

1. Build a conservative swept AABB from translation and rotation bounds.
2. Query the exact static tile BVHs.
3. Broad-reject shapes using swept sphere/capsule bounds.
4. Prove oriented hull clearance adaptively over the interval.
5. Subdivide when curvature, rotation or clearance uncertainty exceeds policy.
6. Record minimum clearance and blocking witness.

Possible first implementation techniques:

- conservative advancement for convex hull pairs;
- swept OBB/capsule tests for authored proxies;
- interval subdivision with a proven displacement/rotation error bound.

Dense point sampling without an interval bound is not proof.

### Dynamic proof

Dynamic proof uses the same candidate time history and predicted obstacle motion.
It must account for uncertainty growth. If prediction validity is shorter than
the maneuver, shorten the accepted horizon.

For many agents, RVO/velocity-obstacle reasoning may generate or rank local
candidates, but the chosen rigid-body maneuver still passes the same physical
and continuous proof.

### Proof witness

The accepted witness records:

- exact program revision/hash;
- topology and geometry revisions;
- dynamic snapshot revision and prediction horizon;
- capability revision;
- minimum static/dynamic clearance;
- minimum linear/angular feedback reserve;
- resource margin;
- proof time interval.

Changing the program invalidates the witness.

---

## 11. L4 — Decision and acceptance

### Candidate cost

Only proven candidates enter decision. Cost may include:

- progress toward corridor/goal;
- travel time;
- fuel/RCS use;
- minimum clearance;
- tracking difficulty;
- amount of hull rotation;
- terminal-state quality;
- doctrine-specific aggression or comfort;
- oscillation/hysteresis penalty for changing family.

### Acceptance rules

An `AcceptedManeuverProgram` exists only if:

- every state is finite and continuous;
- every actuator interval is feasible;
- all required resources are available;
- exact continuous proof succeeded;
- revisions match the captured inputs;
- correction reserve remains;
- its validity horizon is explicit.

Storage paging is representation only. It never creates another maneuver,
clock, acceptance event or phase-capture contract.

### Program representation

Prefer polynomial/phase segments over unrelated sampled fields:

```text
ManeuverProgram
  header/provenance
  physical phases[]
    global start/end time
    state law or consistent coefficients
    attitude law
    actuator schedule
    reserved feedback authority
  terminal contract
  proof witness
```

Fixed-capacity pages may store these phases, but one accessor must expose them as
one continuous global-time program.

---

## 12. Navigation doctrine and modes

Doctrine is policy. Capability is hardware. Keep them separate.

### Standard

- high clearance preference;
- comfortable angular/linear loads;
- earlier braking/rotation;
- stronger avoidance hysteresis;
- StopTurnGo allowed when efficient or safer;
- larger feedback reserve.

### Extreme

- smaller clearance reserve, never below hard hull safety;
- higher allowed load fraction;
- later braking;
- drift and aggressive main-burn families preferred;
- shorter replanning horizon and higher update priority.

### Squeeze

- available only for an explicitly recognized constrained passage;
- orientation-dependent configuration search;
- low speed and strong terminal/corridor capture requirements;
- exact hull proof mandatory;
- dynamic entrance blocking fails closed;
- no reduction of the hull to a fictitious smaller sphere.

### Docking/precision

- terminal pose, velocity and angular velocity are hard/typed boundary states;
- approach corridor and capture volume are semantic geometry;
- final phases may trade travel time for precision;
- docking ingress exceptions are explicit portal/target permissions, not ignored
  collision objects.

### Pursuit/formation

- target state is predicted at candidate arrival time;
- route goal revision and prediction revision are distinct;
- formation capture includes desired relative velocity and orientation;
- a moving terminal must not be encoded as a stop.

---

## 13. L5 — Autopilot/Follower

### Responsibilities

- sample the accepted global-time program;
- emit literal planned actuator channels;
- calculate bounded state feedback;
- allocate feedback only from reserved authority;
- report tracking and resource margins;
- never search global topology;
- never change maneuver family silently.

### Output

```text
AutopilotCommand
  planned actuator channels
  bounded correction channels
  combined saturated physical command
  program/revision identity
  tracking diagnostics
```

Physics consumes these channels directly.

### Tracking decomposition

Report and control separately:

- along-track phase error;
- cross-track position error;
- along/cross velocity error;
- attitude error;
- angular-velocity error;
- clearance margin;
- consumed correction reserve.

### Loss of validity

If the craft leaves the proved tracking envelope:

1. stop treating feed-forward as authoritative;
2. apply a bounded safe command/reflex;
3. invalidate the program after the explicit tolerance;
4. enqueue a local-horizon replan from measured state;
5. never freeze time indefinitely and home toward a stale reference.

---

## 14. Safety monitor and unexpected obstacles

The monitor runs cheaper and more frequently than the Planner.

It checks:

- static revision changes intersecting the proof corridor;
- dynamic actors entering the stopping/avoidance horizon;
- capability/resource changes;
- tracking-envelope departure;
- program expiration/completion;
- terminal/objective revision changes.

### Reflex scope

Allowed reflexes are deliberately bounded:

- cut thrust;
- maintain/rotate toward a safe attitude;
- emergency brake when proven safe;
- apply a short collision-separation impulse within known authority;
- hold the current safe corridor cell.

The reflex does not become an undocumented alternate planner. Material deviation
always produces a replan request.

---

## 15. Many-object scheduling and scalability

Thousands of actors do not receive a full route and trajectory solve every
frame.

### 15.1 Work classes

```text
Class A: per-physics-step execution
  program sampling, tracking, actuator command

Class B: frequent safety/local checks
  dynamic broadphase, short-horizon validity

Class C: local maneuver planning
  only on horizon depletion, hazard or material deviation

Class D: global route planning
  only on objective/topology/corridor invalidation

Class E: navigation-world rebuild
  asynchronous tile/snapshot publication
```

### 15.2 Scheduling

Extend `NavigationWorkScheduler` rather than creating per-actor threads.

Jobs carry:

- actor and objective revision;
- topology/dynamic/capability revision;
- scope and priority;
- deterministic cost estimate;
- deadline/horizon remaining;
- result commit token.

Dispatch uses fixed work/cost budgets. Completed stale jobs are discarded before
commit.

### 15.3 Update frequency/LOD

- near hazards/high speed: frequent monitor and local replans;
- stable open-space cruise: long maneuver horizon, low planning frequency;
- distant/background actors: coarser path and lower update cadence;
- offscreen does not mean non-authoritative, but it may use a longer integration
  quantum if gameplay rules permit;
- missiles and imminent collisions receive urgent priority.

### 15.4 Shared computation

- shared immutable static snapshot;
- shared corridor cache/goal trees;
- batched dynamic publication;
- bounded candidate arrays;
- worker-local scratch arenas;
- no per-tick copying of complete exact geometry;
- no all-pairs actor checks.

### 15.5 Performance targets

Targets must be measured on the user's target machine.

- ordinary per-agent Follower step: allocation-free and suitable for every
  physics tick;
- dynamic broadphase: proportional to local occupied cells/candidates;
- local candidate planning: bounded by fixed family/candidate counts;
- global route search: asynchronous and budgeted;
- main-thread navigation work: target below 0.5 ms typical, below 1.0 ms normal
  peak, excluding explicitly asynchronous worker work;
- 1k/5k/10k actor and topology scenarios remain mandatory.

---

## 16. Required public APIs

Names may evolve, but responsibilities and data direction are fixed.

### Publication

```cpp
StaticNavigationBuildResult buildStaticNavigationTiles(
    const StaticNavigationSourceSnapshot&,
    const StaticNavigationBuildPolicy&);

void NavigationWorld::publishStatic(StaticNavigationSnapshot);
void NavigationWorld::publishDynamic(DynamicNavigationSnapshot);
```

### Global route

```cpp
RouteResult GlobalRoutePlanner::plan(
    const RouteRequest&,
    const NavigationStaticQueryApi&,
    const GlobalRoutePolicy&);
```

### Local candidates

```cpp
ManeuverCandidateSet PhysicalManeuverPlanner::generate(
    const ManeuverQuery&,
    const LocalNavigationQueryApi&,
    const ManeuverGenerationPolicy&);
```

### Proof

```cpp
ProvenManeuverSet ContinuousManeuverProver::prove(
    const ManeuverCandidateSet&,
    const ExactStaticQueryApi&,
    const DynamicPredictionQueryApi&,
    const ManeuverProofPolicy&);
```

### Decision/acceptance

```cpp
ManeuverDecision ManeuverDecisionController::select(
    const ProvenManeuverSet&,
    const NavigationDoctrine&);

AcceptedManeuverProgram accept(
    const ManeuverDecision&,
    const AcceptanceContext&);
```

### Execution

```cpp
AutopilotResult ManeuverAutopilot::step(
    const AcceptedManeuverProgramView&,
    double universeTime,
    double dt,
    const MeasuredRigidBodyState&,
    const CurrentCapabilityState&,
    const AutopilotPolicy&);
```

Every calculation-affecting input crosses one of these APIs explicitly. I/O,
file parsing, descriptor selection and diagnostics remain outside them.

---

## 17. Revision and cache semantics

Keep distinct revisions for:

- objective intent;
- accepted maneuver;
- vehicle capability/resources;
- static topology;
- exact static geometry;
- dynamic snapshot;
- individual actor motion;
- reference frame;
- doctrine if changed at runtime.

Do not overload `goalRevision` or `staticWorldRevision` as a universal identity.

### Invalidation matrix

| Change | Global route | Local maneuver | Accepted program |
| --- | --- | --- | --- |
| Objective region changes | rebuild | invalidate | invalidate |
| Objective terminal pose changes in same corridor | maybe retain | rebuild | invalidate |
| Static topology intersects route | rebuild | invalidate | invalidate |
| Exact geometry changes only near local horizon | route may remain | rebuild/prove | invalidate if intersecting proof |
| Dynamic actor changes | retain | re-evaluate locally | invalidate only on hazard |
| Capability/damage changes | route often retain by size fit | rebuild | invalidate |
| Doctrine changes | recost/rebuild if material | rebuild | invalidate |
| Frame revision changes | transform/rebuild by contract | rebuild | invalidate |

---

## 18. Diagnostics and observability

Diagnostics are products, not hidden logging inside pure kernels.

For every actor expose:

- route/corridor revision and age;
- active maneuver family;
- accepted horizon remaining;
- planned versus actual actuator channels;
- correction reserve remaining;
- static and dynamic minimum clearance;
- along/cross tracking errors;
- capability/resource revision;
- last invalidation/replan reason;
- scheduler queue/in-flight age;
- stage timing and candidate rejection counts.

The viewer should render separately:

- coarse route corridor;
- local candidate maneuvers;
- selected/proven swept tunnel;
- accepted reference;
- actual trajectory;
- nearby dynamic candidates;
- planned and executed thrust vectors.

No white polyline should be visually presented as a physically proved tunnel.

---

## 19. Testing strategy

### 19.1 Pure unit tests

- coordinate transforms including rotating frames;
- topology and portal search;
- speed-envelope propagation;
- every maneuver family;
- actuator allocation and resource depletion;
- program sampling continuity;
- follower correction bounds;
- revision invalidation matrix.

### 19.2 Continuous proof tests

- translating and rotating OBB hulls;
- sphere/capsule/box and compound geometry;
- slit fit where sphere approximation rejects but oriented hull passes;
- near-miss intervals between samples;
- moving gaps and crossing actors;
- docking ingress exception limited to the authorized portal.

### 19.3 Product-chain E2E

No test-authored intermediate program.

```text
scene geometry
 -> navigation-world build
 -> route query
 -> physical candidates
 -> continuous proof
 -> decision/acceptance
 -> Autopilot
 -> authoritative physics
 -> terminal result
```

Required scenarios:

- straight line;
- 90-degree two-leg route;
- 3–4 segment 3D corridor;
- high-speed Newtonian turn;
- Assisted with and without reverse main hardware;
- stop, fly-through and moving terminal;
- flip-and-burn;
- narrow oriented slit;
- portal capture/transit;
- docking;
- pursuit/formation;
- sudden obstacle;
- moving gap;
- damaged thruster/capability revision;
- non-identity moving and rotating frame;
- Standard/Extreme/Squeeze comparisons.

### 19.4 Scale tests

- 1k/5k/10k static regions;
- 1k/5k/10k dynamic actors;
- hundreds/thousands of scheduled actors;
- shared destination cache hit/miss;
- topology tile rebuild under load;
- worst-case local density;
- stale worker-result rejection.

### 19.5 Test truth rules

- a component test proves only that component;
- architecture grep is not behavior evidence;
- build PASS is not flight PASS;
- collision-free samples are not continuous proof;
- final position alone is insufficient: velocity, attitude, angular state,
  clearance, resources and program provenance must also pass.

### 19.6 Industry basis and deliberate extension

The target structure follows established production separation rather than a
single fashionable algorithm:

- [Recast/Detour](https://github.com/recastnavigation/recastnavigation) uses a
  generated, tiled navigation representation and separates construction from
  path queries. Its tiles support streaming and bounded rebuilds.
- [Unreal Engine avoidance](https://dev.epicgames.com/documentation/unreal-engine/using-avoidance-with-the-navigation-system-in-unreal-engine)
  keeps global navigation/path corridors separate from local agent avoidance;
  RVO and Detour Crowd are local interaction mechanisms, not replacements for
  global topology.
- [O3DE/Kythera octree generation](https://www.docs.o3de.org/docs/user-guide/gems/reference/kythera-ai/octree-generation/)
  uses an octree as the 3D-flight equivalent of a ground navmesh.
- [Mercuna 3D navigation](https://mercuna.com/3d-navigation/features/) uses a
  voxelized sparse octree and A* for free-flight pathfinding.

These systems support the decision to use generated hierarchical free-space
topology, tiled publication, shared queries and a separate local layer. They do
not by themselves prove Elite's rigid-body maneuver. Elite deliberately extends
the conventional game-navigation stack with capability, actuator, resource and
continuous swept-hull proof because a main-engine-dominant spacecraft cannot
truthfully execute an arbitrary steering velocity chosen by ordinary crowd
avoidance.

---

## 20. Migration plan

The migration is incremental. Each stage must leave a runnable, diagnostically
honest system. Do not perform a single unreviewable rewrite.

### M0 — Freeze and characterize the current baseline

Actions:

- preserve failing E2E traces and telemetry;
- record current function/ownership inventory;
- record current performance baselines;
- mark the old Stage-2 chain `TRANSITIONAL` in diagnostics;
- prohibit gain/timeout tuning as a substitute for physical planning.

Exit gate:

- baseline artifacts reproducible;
- known failures classified as architecture versus implementation defects.

### M1 — Repair coordinate and API boundaries

Implementation status (2026-09-23): **CODE CANDIDATE COMPLETE / TARGET
MINGW64 VALIDATION PENDING**.

Implemented in the candidate:

- deleted the tool-local `toSystemIntent()` copy converter;
- routed Follower intent through
  `NavigationFrameBoundary::toSystemControlIntent()`;
- changed `ExecutionVehicleInit` to receive an explicit `KinematicFrame`
  snapshot and its epoch instead of reconstructing a frame internally;
- converted initial NavLocal position, velocity, basis and relative angular
  velocity through `NavigationFrameBoundary` before populating system-space
  physics state;
- added the missing rotating-frame inverse for angular-velocity state
  (`NavAngularVelocity -> SystemAngularVelocity`);
- converted system-space physical state and control telemetry back to NavLocal
  before it enters Follower, terminal checks, trace, viewer or local metrics;
- advanced the explicit translating/accelerating/rotating frame snapshot during
  execution and synchronized it into `DynamicMotionState::travelFrame`;
- corrected the execution deadline to include the explicit maneuver start
  universe time instead of assuming every frame epoch begins at zero;
- added a product-chain equivalence E2E comparing identity execution with a
  translated, axis-rotated, linearly moving/accelerating and rotating frame;
- replaced the known whitespace/exact-spelling purity assertions with
  formatting-independent semantic-operation checks;
- corrected two stale architecture assertions that still described Assisted
  as permission to invent fore-main hardware.

Local evidence:

- `check_navigation_api_purity.py` PASS;
- `check_navigation_stage1_nominal_route.py` PASS;
- `check_navigation_stage12_runtime_planner.py` PASS;
- `check_geometric_path_planner.py` PASS;
- Python checker byte-compilation PASS;
- `git diff --check` PASS.

Not yet claimed:

- C++ compile/link PASS;
- non-identity E2E runtime PASS;
- full `navigation_runtime_pipeline` PASS.

The local Linux environment has `g++` but no CMake or GLM development headers,
so it cannot compile the project. M1 remains open until the target Windows
MSYS2/MinGW64 gate validates the candidate. Do not begin M2 merely because the
lexical gates are green.

Actions:

- replace tool-local `toSystemIntent()` with `NavigationFrameBoundary`;
- initialize world transform through the same boundary;
- remove implicit identity-frame assumptions;
- add translated, rotated, moving and rotating-frame tests;
- replace brittle exact-string purity assertions where they provide false
  confidence.

Exit gate:

- local/system types cannot cross without the boundary;
- all frame E2E fixtures produce equivalent NavLocal behavior.

### M2 — Split the runtime composition root

Create separate modules for:

- scenario parsing/tool I/O;
- route request composition;
- maneuver planning/proof composition;
- execution harness;
- trace adapters.

The tool must link production libraries rather than compile domain logic from a
tool-owned monolith.

Exit gate:

- pure libraries build/test without filesystem, viewer or scenario JSON;
- tool orchestration contains no maneuver math.

### M3 — Make program acceptance truthful

Actions:

- reject any infeasible actuator interval;
- introduce one global-time program view over storage pages;
- fix completion time to include sequence/page offset;
- replace independently interpolated state fields with consistent segment laws;
- separate along-track and cross-track diagnostics;
- attach complete proof/capability provenance.

Exit gate:

- an accepted program cannot contain `propulsionFeasible=false`;
- sampling preserves state continuity and correct global time across pages.

### M4 — Activate a short-horizon physical maneuver compiler

Start with Newtonian families:

- Coast;
- TrimRcs;
- LeadRotateMainBurn;
- Brake/FlipAndBurn;
- StopTurnGo fallback.

Correct current compiler defects:

- preserve initial angular velocity;
- use target position/corridor bounds;
- emit literal actuator schedules;
- reserve feedback authority;
- integrate rigid-body state consistently.

Do not yet replace global topology.

Exit gate:

- straight, corner and high-speed Newtonian tests compile feasible candidates;
- impossible timing is rejected before acceptance.

### M5 — Add continuous swept-hull proof

Actions:

- static tile/BVH candidate query;
- oriented hull proxy;
- interval translation/rotation bounds;
- adaptive subdivision/conservative advancement;
- proof witness attached to exact candidate hash.

Exit gate:

- between-sample collisions are detected;
- oriented slit pass/fail matches exact hull geometry;
- accepted program and proved program are byte/semantic identical.

### M6 — Execute literal actuator programs

Actions:

- extend control/physics seam for explicit rear main, fore main, RCS and
  attitude channels;
- sample planned channels in Autopilot;
- allocate only bounded feedback reserve;
- remove observe-only actuator execution marker;
- prevent `DynamicMotionSystem` from re-authoring the planned maneuver.

Exit gate:

- planned versus executed channels match absent disturbance/pilot degradation;
- correction never exceeds reserved authority;
- resource consumption agrees with proof.

### M7 — Add Assisted maneuver families

Actions:

- distinguish doctrine from hardware;
- compile assisted stabilization through installed actuators;
- test rear-only and rear+fore hardware separately;
- add continuous curved/combined main+RCS candidates.

Exit gate:

- Assisted does not synthesize nonexistent reverse thrust;
- both laws reach the same terminal through law-appropriate programs.

### M8 — Replace tool global planning with `NavigationSpace`

Actions:

- publish stand geometry into `NavigationSpace`;
- route through regions/portals;
- return a typed corridor;
- feed a bounded corridor window to the physical planner;
- demote `GeometricPathPlanner` to bounded fallback/debug use.

Exit gate:

- the tool and production use the same route API;
- global route is reusable across local replans;
- dynamic changes do not rebuild static topology.

### M9 — Implement tiled sparse-octree construction

Actions:

- voxelize real collision assets;
- build adaptive free-space leaves and clearance;
- generate portals/coarse regions;
- add tile streaming and local rebuild;
- retain exact geometry BVHs;
- support multiple agent size bands.

Exit gate:

- station/wreck/canyon scenes build automatically;
- holes and tunnels are routes when the coarse envelope fits;
- 1k/5k/10k topology benchmarks meet worker budgets.

### M10 — Integrate dynamic navigation

Actions:

- publish double-buffered dynamic snapshots;
- index motion AABBs rather than current centers plus global expansion;
- query only the local physical horizon;
- generate/rank dynamic avoidance candidates;
- continuously prove the selected candidate;
- add bounded emergency reflex and replan.

Exit gate:

- sudden-obstacle, crossing, pursuit and moving-gap E2Es pass;
- no all-pairs checks;
- no dynamic-triggered global route rebuild unless topology truly changes.

### M11 — Scale scheduler and shared routes

Actions:

- connect route/local/rebuild jobs to `NavigationWorkScheduler`;
- add deadlines and horizon remaining;
- cache/share global corridors and goal trees;
- use worker-local scratch memory;
- batch publications and queries where measurement supports it;
- add actor LOD/update cadence.

Exit gate:

- deterministic bounded slices;
- stale results never commit;
- 1k/5k/10k actor workloads meet declared budgets;
- no starvation of urgent collision work.

### M12 — Complete portals, squeeze, docking and behavior modes

Actions:

- configuration-space orientation states for narrow passages;
- explicit portal capture/transit programs;
- exact docking terminal solve;
- Standard/Extreme/Squeeze doctrine profiles;
- formation/repair/attack scenario policies.

Exit gate:

- mode changes select different legal costs/families, never different physics;
- terminal and portal semantics are carried by typed products end to end.

### M13 — Production integration and removal of transitional paths

Actions:

- replace legacy `AcceptedShortSegment` execution;
- remove tool-owned route/attitude/propulsion authoring;
- remove acceleration-vector reallocation for accepted programs;
- retain explicit fallback only for controlled failure states;
- update replication and persistence contracts;
- lock product-chain E2E and performance gates.

Exit gate:

- one production path from world snapshot to physics;
- no duplicate planner/follower implementations;
- documentation and code ownership agree.

---

## 21. Recommended immediate implementation sequence

The next commits should be small and ordered:

1. Fix `NavigationFrameBoundary` usage and add rotated-frame E2E.
2. Fix current architecture checks so the baseline gate is meaningful.
3. Add hard rejection of infeasible actuator programs.
4. Add one global-time page/program accessor and fix completion semantics.
5. Extend `OrdinaryPhysicalManeuverCompiler` into an actual Newtonian
   short-horizon program author.
6. Link that compiler into `tools/navigation_runtime` before Ruckig timing.
7. Add continuous oriented-hull proof for those candidates.
8. Switch Autopilot to literal actuator execution.
9. Only then replace stand-level global planning with `NavigationSpace` and
   begin octree construction.

This ordering deliberately proves one truthful ship maneuver before multiplying
the same mistake across ten thousand actors.

---

## 22. Definition of done

The navigation layer is working only when all statements below are true:

- static geometry is represented by a reusable, generated 3D navigation volume;
- global routes are hierarchical, asynchronous and independent of per-frame
  dynamic avoidance;
- local plans use actual vehicle state, hull, actuators, resources and control
  law;
- a program cannot be accepted before continuous swept-hull proof;
- the accepted program contains the exact actuator schedule that physics
  executes;
- Follower adds only bounded reserved feedback;
- unexpected hazards invalidate/replan rather than mutate hidden semantics;
- all coordinate conversions cross the canonical frame boundary;
- Standard, Extreme and Squeeze are explicit doctrines, not magic constants;
- stationary, moving and oriented terminal states are solved as boundary
  conditions;
- many actors share world/topology work and receive budgeted asynchronous
  planning;
- 1k/5k/10k topology and actor benchmarks pass declared budgets;
- product-chain E2Es use no test-authored intermediate route, trajectory or
  maneuver program;
- diagnostics can explain why every candidate was selected, rejected,
  invalidated or replanned.

Until then, individual green tests are useful evidence, but they are not proof
of a completed navigation layer.
