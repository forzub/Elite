# Navigation v2 pipeline audit

**Status:** active architecture/code audit
**Updated:** 2026-09-19 Europe/Kyiv
**Purpose:** audit Navigation v2 left-to-right as a chain of explicit contracts. Every stage is judged by one question: **does this code contribute necessary and sufficient information to the overall navigation task without stealing another stage's responsibility?**

Related:
- `NAVIGATION_BEHAVIOR_CHARACTER_MODEL.md`
- `CONTROL_LAW_MANEUVER_MODEL.md`
- `TRAJECTORY_EXECUTION_REPLAN_MODEL.md`
- `../MANEUVER_DECISION_TREE.md`
- `STAGE12_END_TO_END.md`

## Audit method

Every stage is reviewed with the same template:

~~~text
INPUT
    what facts enter?
    are they authoritative?
    are coordinate/frame semantics explicit?
    is anything essential missing?

RESPONSIBILITY
    what single question should this stage answer?
    is it doing work owned by another stage?
    is it inventing physics/geometry/policy?

OUTPUT
    what exact product leaves?
    is it sufficient for the next stage?
    does it preserve the meaning of the selected maneuver?
    is unnecessary owner state leaked?

HANDOFF
    does the next stage consume the output as-is?
    does it silently re-derive or overwrite semantics?
    can information be lost or contradicted?

PERFORMANCE
    required cadence?
    bounded work?
    can accepted results be reused?
    is work shared scene-wide where appropriate?

VERDICT
    OK / INCOMPLETE / DEFECT
~~~

A unit test passing inside one stage is not enough. A stage is accepted only when its handoff contract is also correct.

## Non-negotiable global invariants

~~~text
GEOMETRIC FREE SPACE != EXECUTABLE ROUTE

vehicle feasibility is a hard constraint, not a scoring weight

global route:
    coarse is allowed
    impossible vehicle/portal edges are not

local accepted segment:
    must be dynamically/time feasible from current P/V/A/q/omega

planner must select a physically realizable propulsion/control-law maneuver

future route semantics must not overwrite the current selected maneuver

doctrine changes preference/risk, never physics

pilot changes execution quality/uncertainty, never physics or geometry

PLAN -> PROVE -> ACCEPT -> EXECUTE + MONITOR
replan only on completion/invalidation/expiry/goal/topology/capability change
~~~

## Pipeline

### P0 — World / scene truth publication

**Owner:** GameSimulation / world object owners.

**Input**
- authoritative ship/static-object transforms;
- HitVolumes / hull geometry;
- linear/angular motion;
- damage state;
- hub/system frame truth.

**Responsibility**
- publish factual world state only.

**Must not**
- choose routes;
- invent navigation clearances;
- fabricate collision geometry from render mesh when HitVolumes exist.

**Output**
- immutable/queryable geometry and kinematic facts.

**Current verdict:** **OK for current Stage-12 fixture.**

Evidence:
- real HitVolume OBBs reach NavigationSpace/NavigationMap;
- dynamic enclosing sphere is broadphase only where exact OBB is available;
- frame conversion is explicit.

---

### P1 — Dynamic broadphase / influence isolation

**Owner:** NavigationMap / future scene-wide broadphase service.

**Input**
- dynamic actor P/V/A/omega;
- conservative radius / exact obstacle witness;
- prediction horizon.

**Responsibility**
- answer only: *which actors can physically become relevant inside the requested maneuver horizon?*

**Current implementation**
- NavigationMap already uses a spatial hash grid and bounded corridor/sphere queries;
- it does **not** perform a mandatory all-pairs N² scan;
- actors are indexed by current cell and query bounds are inflated conservatively by swept-motion upper bounds.

**Current limitation**
- each agent currently issues its own query; a pair A/B may therefore be rediscovered independently by A and B.
- for hundreds/thousands of active actors, a later optimization may publish one scene-wide unordered relevant-pair set and per-agent influence lists.

**Required scalable form**

~~~text
WORLD DYNAMIC SNAPSHOT
    -> one spatial broadphase
    -> unordered potentially interacting pairs
    -> batch/SIMD kinematic filter
    -> per-agent influence lists
    -> exact narrow phase only for survivors
~~~

Do **not** build a dense N x N matrix. Matrix/SIMD arithmetic is useful **after sparse pair isolation**.

Desired complexity is approximately:

~~~text
index build: O(N)
pair work:   O(N * k)
~~~

where `k` is the local number of physically relevant neighbors, not total scene population.

**Current verdict:** **OK prototype / OPTIMIZATION FOLLOW-UP.**

No current Stage-12 failure is caused by N² work.

---

### P2 — Static topology / global route feasibility

**Owner:** NavigationSpace corridor/topology planner.

**Input**
- start/goal;
- static regions/portals;
- hull/envelope;
- vehicle feasibility metadata at the abstraction required by an edge/portal.

**Responsibility**
- choose a coarse reachable region/portal sequence;
- expose portal/aperture geometry and traversal constraints without deciding that a portal automatically implies centerline capture or hull-normal alignment.

**Required rule**
A global edge/portal is valid only if this vehicle class/current capability can plausibly traverse it. Global planning need not integrate each burn, but it may not choose a portal whose fit/orientation/braking/control-law requirements are impossible.

A portal is an opportunity/constraint surface. The later maneuver layer decides whether the current behavior model permits:
- FreeTransit through a large safe opening;
- adjusted transit;
- PrecisionCapture/Transit;
- Extreme/contact-expected transit.

The correct question is not "does a portal exist?" but "which passage maneuver is safe/acceptable for this geometry, vehicle, pilot envelope and doctrine?"

**Current implementation**
- exact envelope/hull clearance participates;
- ordered portal sequence is produced;
- current Stage-12 topology successfully leads to slit entry/tunnel.

**Missing**
- general vehicle/control-law feasibility metadata is not yet a first-class graph-edge filter beyond geometric envelope/traversal constraints.

**Current verdict:** **INCOMPLETE but not the immediate live failure.**

---

### P3 — Situation / doctrine profile

**Owner:** mission/tactical behavior layer.

**Input**
- mission state;
- threat state;
- objective urgency;
- allowed damage/contact/load policy.

**Responsibility**
- describe what behavior is desirable, not what is physically possible.

Initial profiles:
- Ordinary / Rational;
- Extreme / Attack / Escape;
- Precision ingress / retrieval / docking / squeeze.

**Output**
Explicit coefficients/bands for:
- time/progress;
- route deviation;
- clearance;
- stopping willingness;
- load/overload;
- threat exposure;
- projected silhouette;
- cover/masking;
- contact severity;
- component-loss tolerance;
- escape reserve.

**Current verdict:** **ARCHITECTURE DEFINED / LIVE INTEGRATION MISSING.**

---

### P4 — Pilot model and transient state

**Owner:** PilotSkill / AI pilot profile.

**Input**
- persistent skill;
- current stress/impact/injury/fatigue/sensor modifiers.

**Responsibility**
- model how accurately and how late a valid maneuver can be executed.

**Output**
- reaction/latency;
- decision cadence;
- anticipation;
- damping/overshoot;
- command slew;
- deterministic precision error;
- hull/clearance judgement uncertainty;
- execution/tracking uncertainty envelope.

**Must not**
- change hull geometry;
- change engine authority;
- make an impossible maneuver feasible.

**Current implementation**
- reaction, decision rate, latency, response frequency, damping, slew and deterministic noise exist in `PilotSkillExecutor`.

**Missing**
- anticipation and hull/clearance judgement are not yet composed into route/maneuver feasibility;
- transient pilot degradation is not yet wired.

**Current verdict:** **PARTIAL.**

---

### P5 — Local free-space / visibility candidate generation

**Owner:** LocalHorizonPlanner + LocalAvoidancePlanner.

**Input**
- current local P/V/A;
- hull radius/envelope;
- local dynamic candidates;
- exact static queries;
- coarse route target.

**Responsibility**
- find geometrically available near-field directions/targets.

**Correct output semantics**
- **candidate free-space targets**, not automatically executable maneuvers.

**Current implementation**
- direct target and bounded 15/30/45/60/75-degree visibility fan;
- exact static + dynamic conflict proof;
- progress-preserving target selection.

**Defect**
- ordinary visibility has no directional propulsion/angular/control-law feasibility.
- therefore `AdjustedClear` currently means geometrically clear, not physically executable.

**Current verdict:** **DEFECT at output semantics.**

---

### P6 — Maneuver generation / physical feasibility

**Owner:** missing/partial control-law-compatible maneuver layer.

**Input**
- free-space candidates from P5;
- current P/V/A/q/omega;
- hull;
- real propulsion directions/authority;
- angular authority;
- control law;
- damage-degraded capability;
- doctrine;
- pilot execution uncertainty.

**Responsibility**
Generate bounded, physically meaningful maneuver primitives.

For main-engine-dominant Newtonian craft, ordinary substantial course changes should consider:
- Coast;
- CoastAndRotate;
- RotateThenBurn / TurnThenBurn;
- BurnThenTurn;
- DriftPass;
- RcsTrim for small correction;
- FlipAndBurn for strong braking;
- precision 6DoF passage only when needed.

RCS is not a hidden omnidirectional main engine.

**Output**
- time-parameterized maneuver candidate(s) with reference state **and** feed-forward control semantics:

~~~text
P(t), V(t), A_ff(t)
q/body-basis(t), omega(t), alpha_ff(t)
~~~

The candidate may be represented analytically or by a small bounded set of knots/control keys. A point/velocity target alone is not sufficient.

**Current implementation**
- precision moving passage has capability-aware continuous evaluation;
- ordinary visibility bypass does not pass through an equivalent physical maneuver generator.

**Current verdict:** **MISSING for ordinary visibility; PRIMARY ARCHITECTURE GAP.**

---

### P7 — Continuous maneuver proof

**Owner:** trajectory evaluators + exact geometry query APIs.

**Input**
- one physical maneuver candidate;
- world/obstacle prediction;
- exact static geometry;
- dynamic geometry;
- pilot/tracking uncertainty envelope.

**Responsibility**
Prove continuously:
- required linear/angular authority;
- swept hull fit;
- dynamic clearance;
- exact static clearance;
- terminal state/escape reserve as required.

**Current implementation**
- MovingPassageTrajectoryEvaluator already proves capability + continuous moving passage;
- same Hermite witness can be proved against exact static geometry.

**Current verdict:** **GOOD COMPONENTS EXIST; not applied to ordinary visibility.**

---

### P8 — Maneuver decision

**Owner:** ManeuverDecisionController.

**Input**
Only physically valid candidates plus annotations:
- progress/time;
- clearance;
- load;
- contact severity;
- threat exposure;
- silhouette;
- damage/component risk;
- escape reserve.

**Responsibility**
- choose among feasible candidates according to doctrine.

**Must not**
- invent thrust;
- invent geometry;
- turn an invalid candidate valid by a high score.

**Current verdict:** **COMPONENT EXISTS / ordinary live chain bypasses it.**

---

### P9 — Accepted execution product

**Owner:** planner/orchestration handoff into AcceptedShortSegment.

**Input**
- exactly one selected, already-proved maneuver.

**Responsibility**
- preserve the selected maneuver's semantics without re-deriving them.

**Required output**
- trajectory/segment revision;
- validity horizon;
- the **same time-parameterized maneuver program that was proved**;
- reference state P/V/q/omega;
- feed-forward A/alpha;
- terminal state/tolerances;
- capability snapshot;
- tracking envelope;
- proof/source revisions.

Target API authority is defined in `NAVIGATION_COMMAND_OWNERSHIP.md`.

**Concrete current defect**
GameSimulation currently derives:

~~~text
alignForward = lastPlan.portalTraversalActive
~~~

which changes maneuver meaning. A future portal existing on the route is not equivalent to the current selected maneuver requiring portal-forward alignment.

**Required correction**
Planner must explicitly publish whether **the selected maneuver** requires forward alignment and what that forward vector is. AcceptedShortSegment copies that fact; it does not infer it from route context.

**Current verdict:** **DEFECT — first code correction in this audit.**

---

### P10 — TrajectoryFollower

**Input**
- accepted maneuver program;
- current agent state.

**Responsibility**
- sample the already-proved reference state/control program;
- apply bounded closed-loop correction around that program;
- report completion/tracking error;
- no obstacle search, route choice or trajectory re-solve.

Target semantics:

~~~text
sample:
    P_ref, V_ref, A_ff
    q_ref, omega_ref, alpha_ff

feedback:
    A_cmd = A_ff + bounded A_feedback
    alpha_cmd = alpha_ff + bounded alpha_feedback
~~~

Large tracking divergence invalidates the program and requests replan.

**Current behavior**
- current `AcceptedShortSegment` is transitional;
- follower still derives acceleration from target velocity or consumes one fixed acceleration sample.

**Current verdict:** **TRANSITIONAL / API MIGRATION REQUIRED.**
The follower code is coherent for the old segment type, but the old segment type is insufficient for the target physical-maneuver architecture.

---

### P11 — NavLocal/System boundary

**Owner:** NavigationFrameBoundary.

**Responsibility**
- explicit typed coordinate conversion only.

**Current verdict:** **OK.**

---

### P12 — PilotSkill execution

**Input**
- selected ideal control intent;
- pilot profile.

**Responsibility**
- reaction, decision sampling, latency, command filtering/noise.

**Must not**
- alter route topology/geometry/capability.

**Current verdict:** **OK core / pilot-planning uncertainty integration still partial at P4/P6.**

---

### P13 — Propulsion allocation / authoritative physics

**Input**
- physically meaningful control demand.

**Responsibility**
- enforce real main-engine directionality, RCS authority, angular limits, control law and state propagation.

**Current behavior**
- forward main engine;
- manoeuvre/RCS remainder clamped to actual authority;
- Newtonian and Assisted behavior separated.

**Current verdict:** **OK.**
Physics correctly exposes impossible upstream arbitrary acceleration requests rather than hiding them.

---

### P14 — Monitor / replan scheduler

**Input**
- accepted segment proof/validity;
- actual P/V/q/omega;
- new hazards;
- capability/topology/goal changes.

**Responsibility**
- continue accepted execution while valid;
- wake the correct planning scope only on completion/invalidation/expiry/change.

**Current verdict:** **architecturally correct; dependent on P6/P9 producing a truthful accepted maneuver.**

## Current repair order

1. **P9:** stop route-context portal alignment from overwriting current maneuver semantics.
2. Add bounded transition diagnostics to verify P5->P9->P13 with actual numbers.
3. **P6/P7:** insert capability/control-law-aware maneuver generation/proof between free-space candidate and ACCEPT.
4. **P3/P4/P8:** wire doctrine + pilot uncertainty into candidate generation/filtering/ranking.
5. **P2:** extend coarse global edge/portal feasibility beyond geometry-only envelope where required.
6. Re-audit P0..P14 end-to-end and remove any redundant/re-derived fields.
7. Performance pass: promote NavigationMap prototype broadphase toward shared sparse pair/influence-list publication only if profiling shows per-agent query duplication is material.

## Acceptance question for every future patch

Before accepting any Navigation patch, answer:

~~~text
Which pipeline stage owns this code?
What authoritative input does it consume?
What exact question does it answer?
What minimal/sufficient output does the next stage need?
Does it preserve semantics without re-deriving them?
Is it bounded at the cadence where it runs?
Would deleting this code make the architecture clearer without losing required truth?
~~~

If the last answer is yes, the code is probably doing unnecessary work.


## Planner/follower two-world refinement — 2026-09-19

Canonical detailed analysis: `PLANNER_FOLLOWER_ARCHITECTURE.md`.

The P0..P14 audit remains useful internally, but the external runtime architecture is now grouped into two worlds:

```text
Planner World:
    P0/P1 world query inputs
    + P2 topology
    + P3/P4 policy envelopes
    + P5 free-space candidates
    + P6 physical maneuver generation
    + P7 proof
    + P8 decision
    + P9 AcceptedManeuverProgram publication

Autopilot/Follower World:
    P10 program tracking
    + execution safety monitoring / bounded reflex
    + P11 frame boundary
    + P12 PilotSkill
    + P13 propulsion/physics
    + P14 completion/invalidation scheduling
```

P5 target correction:
- the current angular visibility fan remains a temporary bounded implementation;
- target ordinary local planning is a route-aligned configuration-space corridor built from already-known world geometry;
- ray/segment/sweep operations remain legal as proof/intersection primitives, but not as pseudo-perception;
- NavigationSpace topology remains above the local corridor solver.

P10/P14 refinement:
- nominal follower tracking must stay within the proved tracking envelope;
- small disturbances may be recovered while preserving route progress and reducing cross-track/state error;
- an imminent-hazard bounded reflex may temporarily override nominal tracking;
- a material reflex/deviation invalidates the accepted program and wakes local replanning rather than turning the follower into a second route planner.

Performance refinement:
- dynamic broadphase/pair isolation should be shared scene-wide and may be SIMD/batch processed;
- only agents requiring new programs enter planner batches;
- follower/program sampling remains fixed-step and may be batched for all active controlled actors.
