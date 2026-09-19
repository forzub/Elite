# Navigation v2 — canonical block architecture

**Status:** CURRENT CANON / implementation target
**Updated:** 2026-09-19 Europe/Kyiv
**Scope:** System / Local / Precision navigation and autopilot execution
**Supersedes as top-level structure:** ad-hoc `NavigationRuntimePlanner -> AcceptedShortSegment -> TrajectoryFollower` composition
**Does not delete:** accepted NavigationMap, NavigationSpace, HitVolume, PilotSkill, propulsion or physics truth

## 0. Core model

Navigation v2 is split into two top-level runtime worlds.

```text
PLANNER WORLD
    authoritative world truth
    + NavigationObjective
    + vehicle capability / hull
    + pilot planning envelope
    + Situation / Doctrine
        |
        v
    route / corridor
        |
        v
    local geometric candidate
        |
        v
    physical maneuver candidate(s)
        |
        v
    continuous capability + geometry proof
        |
        v
    decision / ACCEPT
        |
        v
    AcceptedManeuverProgram

AUTOPILOT / FOLLOWER WORLD
    AcceptedManeuverProgram
    + actual current vehicle state
        |
        v
    time sampler
        |
        v
    bounded tracking controller
        |
        +----> safety monitor / bounded reflex
        |
        v
    PilotSkill
        |
        v
    propulsion allocator
        |
        v
    authoritative physics
        |
        v
    completion / invalidation / new objective
        |
        v
    planner scheduling
```

The planner does not discover the world through sensors. It consumes authoritative navigation world data. Segment/ray/sweep queries are geometric proof operations over already-known truth.

The follower does not normally plan. It executes one already accepted program and may only make bounded tracking corrections. A material emergency deviation invalidates that program and wakes the planner.

---

# Planner World

## B0 — Navigation World Snapshot

**Owner:** world/simulation publication layer.

**Question answered:** what geometry and motion are true now?

**Called:** when authoritative static/dynamic world state changes; dynamic publication may run every simulation tick or at a bounded navigation publication cadence.

**Uses:**
- authoritative entity transforms;
- linear velocity / acceleration;
- angular velocity;
- authored HitVolumes;
- static region/portal topology;
- world/frame revisions.

**Returns:**
- immutable/revisioned static navigation world;
- immutable/revisioned dynamic navigation snapshot;
- no route or control decisions.

**API target:**
```text
StaticNavigationSnapshot
DynamicNavigationSnapshot
WorldRevision
```

**Scaling:**
- one scene-wide publication, never one copy per vehicle;
- static data reused until revision changes;
- SoA-friendly dynamic records.

**Current repository:** NavigationSpace + NavigationMap publication are largely correct.

---

## B1 — Shared Dynamic Broadphase / Influence Builder

**Owner:** navigation world service.

**Question answered:** which dynamic actors can become relevant to which agents inside their physical planning horizon?

**Called:** after a new dynamic snapshot; may run every navigation world update.

**Uses:**
- dynamic snapshot;
- agent working positions/horizons/envelopes;
- conservative motion bounds.

**Returns:**
```text
InfluenceFrame
    worldRevision
    sparse unordered relevant pairs
    per-agent bounded influence lists
```

**Must not:**
- choose paths;
- use dense N x N;
- perform exact expensive trajectory proof for every pair.

**Scaling target:**
```text
spatial broadphase
 -> sparse pairs
 -> batch/SIMD kinematic rejection
 -> per-agent influence lists
```
Expected work approximately O(N*k), where k is local relevant-neighbor count.

**Current repository:** NavigationMap spatial hash is good, but current discovery is still primarily per-agent query and may rediscover A/B twice.

---

## B2 — Navigation Objective

**Owner:** mission / AI / gameplay / player-guidance request layer.

**Question answered:** what terminal result is requested?

**Called:** on new task, target change, mission-state change, formation/docking/intercept update or manual route request.

**Uses:**
- semantic task state.

**Returns a pure contract:**
```text
NavigationObjective
    revision
    target frame
    position/region target
    optional target/relative velocity
    optional attitude/roll
    terminal tolerances
    deadline/urgency
    Situation/Doctrine
```

**Must not:**
- select local obstacle bypasses;
- command engines.

**Scaling:** cheap immutable per-agent state; no world scan.

**Current repository:** fragmented between NavigationRuntimePlanner::Goal, NpcNavigationGoal and task-specific planners. Needs unification later.

---

## B3 — Global / Topology Route Planner

**Owner:** NavigationSpace topology layer.

**Question answered:** through which regions/portals/corridor branch can this vehicle reach the objective?

**Called:**
- no route yet;
- objective changes;
- topology branch invalidated;
- vehicle/hull/capability change makes current branch impossible.

**Uses:**
- static snapshot;
- objective;
- hull/envelope;
- coarse vehicle feasibility metadata.

**Returns:**
```text
RouteCorridor
    routeRevision
    region sequence
    portal/aperture sequence
    local corridor legs
    traversal constraints
```

**Must not:**
- command acceleration;
- force portal alignment merely because a portal exists;
- numerically integrate every engine pulse.

**Scaling:**
- event-driven, not per physics tick;
- cached by static/topology revision where useful;
- planner jobs may be queued.

**Current repository:** NavigationSpace is the correct base; vehicle/control-law edge feasibility is incomplete.

---

## B4 — Local Route-Aligned Corridor Planner

**Owner:** local geometric planner.

**Question answered:** inside the selected route/corridor leg, what local geometric path avoids known obstacles while preserving useful progress?

**Called:**
- new local suffix required;
- accepted program completed/expired;
- local dynamic/static invalidation;
- tracking/reflex deviation exceeded envelope.

**Uses:**
- current P/V only as geometric seed;
- current RouteCorridor leg;
- exact static geometry;
- B1 influence list / predicted occupancy;
- conservative hull envelope;
- clearance + pilot/tracking uncertainty.

**Primary representation:**
```text
route-aligned frame:
    s = forward progress along corridor leg
    u,v = cross-section

adaptive longitudinal slabs at:
    obstacle enter/exit
    portal/topology boundaries
    relevant dynamic events

inflate obstacle occupancy by ship envelope + margin
connect free-space components across adjacent slabs
select smooth progress-preserving chain
```

**Returns:**
```text
LocalGeometricPath
    revision
    bounded path/corridor primitives
    free-space witness
    terminal geometric handoff
```

**Important limitation:** this solver is local and normally monotonic in corridor progress. U-traps, required backtracking and branch changes escalate to B3.

**Must not:** call the result executable merely because it is geometrically clear.

**Scaling:**
- bounded local working set from B1/B3;
- no whole-world scan;
- adaptive slabs, not centimeter grids;
- fixed/bounded candidate counts where possible.

**Current repository:** LocalHorizon + LocalAvoidance perform this role imperfectly; the 15/30/45/60/75 degree ray fan is transitional and will be replaced/A-B tested.

---

## B5 — Physical Maneuver Compiler

**Owner:** control-law-aware planner.

**Question answered:** how can this actual vehicle physically follow the selected geometric path from its current state?

**Called:** after B4 supplies a geometric candidate, or directly for special precision/recovery candidates.

**Uses:**
- LocalGeometricPath;
- current P/V/A/q/omega;
- real hull;
- propulsion directionality and authority;
- angular authority;
- control law (Newtonian / Assisted / other);
- damage-degraded capability;
- pilot planning envelope;
- doctrine.

**Returns one or more bounded candidates:**
```text
ManeuverCandidate
    maneuver family
    bounded time domain
    P(t), V(t), A_ff(t)
    q/body-basis(t), omega(t), alpha_ff(t)
    terminal state
    required authority
```

Examples:
- coast;
- lead-rotate + main burn;
- drift pass;
- trim;
- brake;
- flip-and-burn;
- precision capture/transit.

**Must not:** assume propulsion allocator will rescue an impossible vector.

**Scaling:** bounded primitive library; generate a small candidate set, not an unbounded optimizer for every actor every tick.

**Current repository:** missing for ordinary visibility; MovingPassageTrajectoryEvaluator is an accepted special-case precursor.

---

## B6 — Continuous Maneuver Prover

**Owner:** trajectory/collision/capability proof layer.

**Question answered:** is this exact maneuver candidate continuously safe and executable?

**Called:** for each B5 candidate before ACCEPT.

**Uses:**
- exact candidate program;
- vehicle hull / orientation;
- static exact HitVolumes;
- dynamic predicted geometry from influence list;
- capability revision;
- pilot/tracking uncertainty.

**Proves:**
- linear/angular authority;
- continuous swept-hull geometry;
- dynamic clearance;
- exact-static clearance;
- terminal constraints;
- tracking/control reserve.

**Returns:**
```text
ProvedManeuver
    same maneuver program
    proof witness
    min clearance / reserve
    world/capability revisions
    pass/fail + reason
```

**Rule:** proof may annotate but may not silently create a different trajectory.

**Scaling:** exact narrow phase only for B1 survivors and B5 bounded candidates.

**Current repository:** strong components exist for moving passage + exact static proof, but are mixed inside NavigationRuntimePlanner and not generalized.

---

## B7 — Maneuver Decision

**Owner:** maneuver decision / doctrine layer.

**Question answered:** among physically valid candidates, which one best fits current doctrine?

**Called:** after B6 produces zero or more proved candidates.

**Uses only proved candidates plus annotations:**
- time/progress;
- clearance/reserve;
- load;
- fuel/delta-v;
- threat exposure;
- silhouette/cover;
- allowed contact/damage risk.

**Returns:** exactly one selected ProvedManeuver or no-solution/recovery request.

**Must not:** turn an invalid candidate valid by score.

**Scaling:** small bounded candidate array; cheap.

**Current repository:** ManeuverDecisionController exists but ordinary live chain bypasses it.

---

## B8 — Maneuver Acceptance / Program Store

**Owner:** Planner→Follower boundary.

**Question answered:** what exact immutable program now owns automatic execution?

**Called:** only after B7 selection and B6 proof.

**Uses:** selected ProvedManeuver.

**Returns/stores:**
```text
AcceptedManeuverProgram
    identity/revisions
    validity
    maneuver family
    fixed-capacity time samples/analytic segments

    reference:
        P(t), V(t)
        q/body basis(t), omega(t)

    feed-forward:
        A_ff(t), alpha_ff(t)

    terminal tolerances
    tracking envelope / feedback reserve
    proof/world/capability witnesses
```

**Rule:** the follower must execute this same program. No target-velocity reconstruction.

**Scaling:**
- fixed-capacity POD-like program;
- one active program per controlled actor;
- no unbounded per-agent vectors in the hot execution path.

**Current repository:** AcceptedShortSegment is transitional. This is the first code block to replace.

---

# Autopilot / Follower World

## B9 — Maneuver Program Sampler

**Owner:** execution world.

**Question answered:** what reference state/feed-forward command does the accepted program specify at time t?

**Called:** every fixed control/physics tick for each automatically controlled active actor.

**Uses:**
- AcceptedManeuverProgram;
- current universe/simulation time.

**Returns:**
```text
ManeuverReferenceSample
    P_ref, V_ref, A_ff
    body basis/q_ref, omega_ref, alpha_ff
    terminal/progress state
```

**Must not:** inspect world obstacles or choose a route.

**Scaling:** O(1) over a small fixed-capacity program; batch/SoA-friendly.

**Current repository:** absent as a clean block; TrajectoryFollower currently mixes program interpretation with feedback.

---

## B10 — Bounded Tracking Controller

**Owner:** follower.

**Question answered:** what bounded correction keeps the actual vehicle near the accepted reference?

**Called:** every fixed control tick after B9.

**Uses:**
- sampled reference;
- actual P/V/q/omega;
- tracking envelope / reserved authority.

**Returns:**
```text
IdealVehicleCommand
    A_cmd = A_ff + bounded A_feedback
    alpha_cmd = alpha_ff + bounded alpha_feedback
    tracking error diagnostics
```

**Recovery rule:** after a small disturbance, preserve program/route progress while cross-track, velocity and attitude errors decay. Do not fly shortest-path backwards to an old line if that sacrifices useful progress.

**Invalidation rule:** if tracking error exceeds the proved envelope, request replan instead of redesigning trajectory.

**Scaling:** fixed arithmetic per active controlled actor; batchable.

**Current repository:** TrajectoryFollower is close structurally, but it still re-derives acceleration from a target velocity and uses AcceptedShortSegment.

---

## B11 — Execution Safety Monitor / Bounded Reflex

**Owner:** autopilot safety supervisor.

**Question answered:** has new evidence made the next short execution horizon unsafe before a replacement plan can arrive?

**Called:** every control tick or at a high bounded safety cadence.

**Uses:**
- current actual state;
- current accepted program/ref sample;
- latest static/dynamic revision/influence data;
- short physical reaction horizon.

**Returns:**
```text
SafetyStatus:
    Safe
    InvalidateAndReplan
    ReflexActive

optional bounded emergency command
```

**Reflex rule:** an imminent collision may justify a short emergency override. If it materially changes the trajectory, invalidate the accepted program immediately and enqueue urgent local replanning.

**Must not:** become a second ordinary path planner.

**Scaling:** only short-horizon influence list; bounded exact checks; urgent events are rare.

**Current repository:** monitoring/replan logic exists partly in GameSimulation + NavigationExecutionReplanPolicy; explicit reflex API is missing.

---

## B12 — Pilot Skill Executor

**Owner:** PilotSkill.

**Question answered:** how does this pilot/controller imperfectly execute the ideal vehicle-level command?

**Called:** every control tick.

**Uses:**
- ideal linear/angular command;
- pilot profile/transient state;
- deterministic timing/noise state.

**Returns:** delayed/filtered/imperfect executed vehicle-level demand.

**Must not:** change route, geometry or physical capability.

**Scaling:** fixed per-agent arithmetic; batchable.

**Current repository:** accepted component.

---

## B13 — Propulsion Allocator + Physics

**Owner:** vehicle flight control / physics.

**Question answered:** which real actuators can produce the requested demand, and what motion actually results?

**Called:** every physics tick.

**Uses:**
- executed vehicle-level demand;
- body attitude;
- engine/RCS/torque capability;
- damage;
- physics/contact state.

**Returns:**
- actual forces/torques;
- authoritative next state.

**Must not:** fabricate capability to satisfy an upstream impossible command.

**Scaling:** existing physics responsibility.

**Current repository:** DynamicMotionSystem / SharedShipPhysics are the accepted downstream authority.

---

# Scheduling / thousands of units

## B14 — Navigation Work Scheduler

This is not a third navigation brain. It is an execution service deciding **when** planner blocks run.

**Question answered:** which actors need planner CPU now, and in what priority order?

**Called:** every navigation scheduling tick.

**Inputs:**
- new objectives;
- missing/expired/completed programs;
- static/dynamic invalidations;
- safety-reflex invalidations;
- vehicle capability changes;
- topology changes;
- manual guidance refresh requests.

**Queues:**
```text
URGENT
    imminent-hazard/reflex aftermath
    invalid program near hazard
    capability loss during active maneuver

NORMAL
    segment completion/expiry
    local dynamic/static invalidation
    new ordinary objective

BACKGROUND / ADVISORY
    manual guidance refresh
    far/non-active NPC route preparation
    cache/prewarm work
```

**Output:** bounded set of PlannerJobs for this scheduler slice.

**Required policy:**
- deterministic priority key for replay/tests;
- starvation prevention/age boost;
- per-tick job and/or time budget;
- urgent queue may pre-empt ordinary planning;
- no planner call merely because another physics frame elapsed;
- one actor has at most one active planning job per objective/revision;
- stale queued jobs are discarded by revision before expensive work.

**Thousands-unit rule:**
```text
all active controlled actors:
    cheap B9/B10/B12/B13 every needed control tick

all navigation actors:
    shared B0/B1 world work

only dirty actors:
    B3..B8 planner work
```

The architecture must therefore remain viable with hundreds or thousands of registered units even when only a fraction are simultaneously near complex obstacles.

A future worker-pool implementation may process PlannerJobs in parallel because the inputs are immutable snapshots and outputs are value-owned programs. Deterministic commit order must be by job identity/revision rather than thread completion order.

### Implemented B14 API slice

The first concrete scheduler seam is `NavigationWorkScheduler` plus the
value-owned `NavigationPlannerJob`.

One job carries:
- actor id;
- monotonically increasing per-actor job revision;
- objective revision;
- world revision;
- capability revision;
- optional route revision;
- local/full-route scope;
- urgent/normal/background priority;
- trigger;
- deterministic estimated cost units.

The scheduler owns only scheduling state:
- three priority queues;
- one actor slot for the current pending job;
- revision stamps;
- in-flight tickets;
- deterministic age-promotion state.

It does **not** own NavigationMap, NavigationSpace, NavigationRuntimePlanner or
any planner callback.

Dispatch is bounded by:
```text
maxJobs <= 128
maxCostUnits
```

Wall-clock time is intentionally external to this deterministic core. Runtime
orchestration may stop requesting/processing further slices when its measured
time budget is exhausted; the scheduler itself never reads a clock.

Replacement is O(1)-like at the actor slot. Old queue records become tombstones.
Physical queue storage is kept bounded by amortized tombstone compaction, so
repeated superseding jobs for one actor cannot grow memory without limit.

A dispatched job becomes **in-flight**. `complete(ticket)` rechecks world,
objective, capability, route and latest job revision and reports either:
```text
CompletedCurrent
CompletedStale
```
A stale worker result therefore cannot become authoritative merely because its
planner calculation finished.

The isolated scheduler API has now passed its target-machine gate.

### Live B14 integration candidate

The first live integration is intentionally limited to the deterministic
Stage-12 lab actor.

Ownership:

```text
ReplanPolicy
  -> NavigationPlannerJob
  -> B14 enqueue
  -> bounded dispatch
  -> existing NavigationRuntimePlanner
  -> B14 complete(ticket)
  -> commit planner result only if CompletedCurrent
  -> existing AcceptedShortSegment compatibility seam
```

This slice does not alter B3/B4/B5/B6 planner geometry or the live execution
product.

The live orchestrator publishes:
- current dynamic-world revision;
- objective revision;
- monotonic capability revision from actual vehicle authority;
- monotonic per-actor planner job revision.

GameSimulation may measure dispatch/planner wall time for diagnostics. B14 itself
remains clock-free.

The live headless acceptance gate must prove:
- every planner call was scheduler-dispatched;
- every authoritative planner commit followed `CompletedCurrent`;
- no stale completion committed;
- the prior Stage-12 physical flight/replication behavior remains green.

---

# Canonical APIs between blocks

```text
B0 -> B1:
    DynamicNavigationSnapshot

B0 -> B3/B4/B6:
    StaticNavigationSnapshot / query APIs

B1 -> B4/B6/B11:
    InfluenceList(agentId, worldRevision)

B2 -> B3/B14:
    NavigationObjective

B3 -> B4:
    RouteCorridor

B4 -> B5:
    LocalGeometricPath

B5 -> B6:
    bounded ManeuverCandidate[]

B6 -> B7:
    bounded ProvedManeuver[]

B7 -> B8:
    selected ProvedManeuver

B8 -> B9:
    AcceptedManeuverProgram

B9 -> B10:
    ManeuverReferenceSample

B10/B11 -> B12:
    IdealVehicleCommand

B12 -> B13:
    ExecutedVehicleDemand

B11/B13/program lifecycle -> B14:
    NavigationInvalidation / completion event
```

No block may reach around these boundaries to reconstruct another block's product from unrelated state.

---

# Implementation order

1. B8 `AcceptedManeuverProgram` data/API.
2. B9 `ManeuverProgramSampler`.
3. B10 migrate `TrajectoryFollower` to sampled program + bounded feedback.
4. B14 explicit planner work scheduler API and dirty/priority queue.
5. B4 `RouteAlignedCorridorPlanner` beside existing LocalAvoidance.
6. B5 ordinary Newtonian/Assisted maneuver compiler.
7. B6 general proof composition.
8. B7 route ordinary candidates through ManeuverDecisionController.
9. B11 explicit safety-monitor/reflex API.
10. B1 migrate per-agent dynamic discovery toward one shared InfluenceFrame.

At every iteration:
- one block/API change;
- deterministic isolated test;
- architecture contract update if ownership changes;
- target-machine gate before acceptance;
- then update state/task/continuation MDs.

