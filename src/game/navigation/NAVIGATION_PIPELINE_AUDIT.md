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

## 2026-09-20 full-chain evidence audit — why green slices did not mean green navigation

Re-audit performed after the live 3D stand exposed bad end-to-end behavior.

### Core conclusion

The previous tests were not fictitious: many of them exercised real production
Follower, PilotSkillExecutor, propulsion/physics, exact HitVolume geometry,
NavigationMap, NavigationSpace and parts of NavigationRuntimePlanner.

However, the evidence boundary was repeatedly over-interpreted. Most green tests
proved a component or a pre-authored slice, not the autonomous full chain:

`world/objective -> one retained global route -> local geometric response -> physical
maneuver generation -> continuous tunnel/proof -> decision -> ACCEPT -> follower ->
pilot -> physics -> event-driven monitor/replan -> finish`.

### What the main green test families actually proved

`ManeuverProgramExecutionLabTests`, `ManeuverCorridorMatrixTests`,
`ManeuverFlyThrough3dTests`, `ManeuverRigidBodyCorridorTests` and much of
`ManeuverChainedLimitMatrixTests` construct AcceptedManeuverProgram objects in the
test itself, then execute them through the real Follower/Bridge/Pilot/physics stack.
They prove execution/tracking of an already-good authored program. They do not prove
that production planning can generate that program from arbitrary world truth.

`NavigationRuntimePlannerTests` use deliberately authored region/portal fixtures. For
example the forced detour is already encoded by three regions and two known portals.
This validly proves topology/portal selection and local planner semantics, but not
automatic route synthesis around arbitrary raw obstacle geometry.

`OrdinaryPhysicalManeuverCompilerTests` genuinely test production B5 Newtonian
maneuver generation. They also explicitly pin that Assisted is unsupported in the
current first B5 slice, and candidates still require downstream continuous proof.
Therefore successful Assisted execution matrices were execution proofs of authored
programs, not proof of a production Assisted B5/B6/B7/B8 chain.

`NavigationCompositeProvingGroundTests` uses real planner/follower/physics components
but glues them together with test-owned helpers. Its topology is pre-authored through
portals; `makeProgram()` authors quintic programs; `buildDoctrineChoice()` authors
candidate programs/decision annotations; `fitAuthorityBoundedReplacement()` is a
test-side physical fitter. The file itself documents that production general B5
authoring remained a migration gap. The composite therefore was a proving ground,
not a production orchestrator.

The authoritative GameSimulation NavigationRuntimeLab was a real live test and remains
strong evidence for its narrow scope: real world objects/HitVolumes, real map/space,
real planner/control/physics and replication. But its slit/tunnel topology, approach,
entry/exit portals, start and goal are deterministic authored fixture data. It proves
that the live runtime can execute that authored topology/local situation; it does not
prove arbitrary `start + raw obstacles + finish -> full route/program` synthesis.

### Existing repository documentation already warned about this

`NAVIGATION_PIPELINE_AUDIT.md` explicitly says a unit test passing inside one stage is
not enough and records P6 ordinary maneuver generation / P7 proof integration / P8
ordinary decision integration / P9-P10 handoff as incomplete or transitional.

`NAVIGATION_V2_BLOCK_ARCHITECTURE.md` states B3 is event-driven, B5 current production
compiler is Newtonian-only, B6 proof is mandatory before ACCEPT, and the follower may
only execute the already accepted maneuver.

The project mistake was therefore not lack of warnings in code/docs; it was promoting
slice-level green evidence to a broader 'system works' interpretation.

### Why the new live stand looked dramatically worse

The new stand attempted, for the first time in this workflow, to synthesize much more
of the chain from a raw JSON scenario inside one executable. That immediately exposed
missing orchestration and also introduced new stand-side integration defects:
- absolute simulation time was mistakenly supplied as dynamic snapshot age, causing
  StaleHold/fail-closed braking;
- the stand periodically called the combined NavigationRuntimePlanner, incorrectly
  recomputing global/static planning instead of retaining one global route;
- the stand still uses its own `makeShortProgram()` quintic adapter, bypassing the
  complete production B5/B6/B7/B8 compile/proof/selection/acceptance chain;
- its default one-region + wall scenario asks current topology machinery to do a more
  general raw-obstacle route-synthesis job than the earlier authored-portal fixtures.

Thus the bad video is not evidence that Follower/PilotSkill/physics were fake. It is
evidence that the missing orchestration/handoff layers were never fully proved, and
that the newly written stand-side glue was itself incorrect.

### Evidence policy from now on

No collection of component/slice tests may be described as full navigation acceptance.
A true end-to-end acceptance must start from world/objective input and use the same
production products/handoffs as the game without test-authored maneuver programs or
test-only route/physics adapters.

Every test/result should be classified as one of:
- component/unit proof;
- execution of authored program;
- planner/topology slice;
- authoritative authored-world integration;
- true autonomous scenario end-to-end.

Only the last category may support a statement that the complete navigation chain
works for the scenario being tested.


## 2026-09-20 — Stage split implemented: Stage 1 static nominal route

The navigation diagnostic workflow is now explicitly split into two stages.

### Stage 1 — current implementation

One retained nominal route is built once from start to finish through static obstacle
geometry.

New production-facing component:
- `src/game/navigation/NominalRoutePlanner.h/.cpp`.

It wraps the existing `GeometricPathPlanner` backend behind an explicit v2 ownership
contract and returns a sparse polyline only. It accepts optional required authored
checkpoints and a coarse navigation envelope/clearance.

Nominal-route validity is revision driven:
- goal revision changed -> rebuild;
- static-world revision changed -> rebuild;
- dynamic-world revision changed -> **do not rebuild**.

The dynamic revision is deliberately present in `ValidityQuery` and deliberately
ignored by nominal-route invalidation. A regression test pins this behavior.

The live diagnostic stand now runs only Stage 1 on `РАССЧИТАТЬ`:

```text
JSON start/checkpoints/finish
 + static obstacles
 -> NominalRoutePlanner
 -> retained static route polyline
 -> viewer
```

It no longer runs the old half-second Planner/Follower/physics loop during Stage 1.
Therefore the two previously identified stand defects are removed from the canonical
Stage-1 path:
- no periodic global route replanning;
- no stand-local `makeShortProgram()` presented as production maneuver execution.

The ship remains at the start pose in Stage 1. The viewer shows the calculated route
and static obstacles only.

### Corridor/tunnel boundary

`route_envelope_radius_m` and `route_clearance_m` are coarse route/corridor inputs.
They are not exact physical collision proof. Exact Cobra wall/aperture contact belongs
to the later time-parameterized swept-hull tunnel proof.

### Dynamic-ready boundary

The scenario schema still accepts moving obstacles and a sudden obstacle. Stage 1 does
not use them for route reconstruction. Stage 2 will consume them as a local dynamic
overlay against the retained nominal route.

### Validation state

Code is committed but the new Stage-1 target has not yet been compiled/run on the
user's MinGW64 machine. Do not claim target acceptance until that run is supplied.


## 2026-09-20 Stage-1 cleanup lock

The Stage-1 diagnostic boundary was tightened after the initial implementation:
- old stand-local `makeShortProgram`, periodic planner loop, Follower/PilotSkill/
  SharedShipPhysics execution and dynamic publication code were removed from
  `NavigationScenarioRuntime.cpp` rather than merely left dormant;
- `tools/navigation_runtime/CMakeLists.txt` now links only the nominal-route/static
  geometry core for Stage 1;
- moving/sudden obstacle records remain parseable in the scenario schema as reserved
  Stage-2 inputs, but cannot influence the Stage-1 route;
- a new architecture checker pins that Stage 1 cannot regress into execution/replan
  ownership;
- target validation is still pending on the user's MinGW64 machine.


## 2026-09-21 target Stage-1 validation result

User target machine checkout: `5da0be0d05ef91958a0e7dc9adda3b4eb8fdee29`.

Observed:
- new `nominal_route_planner` test PASS;
- architecture suite stopped on an obsolete pre-System-frame marker in
  `check_navigation_live_runtime_control.py`;
- old Stage-2 `navigation_runtime_planner` still fails its adjusted-target fixture;
- old Stage-2 composite still fails at the narrow-passage full-hull check after a
  Newtonian dynamic bypass;
- all other 18/20 runtime tests passed.

Interpretation:
- Stage 1 static nominal route code itself passed its new behavioral test;
- the two red runtime tests are Stage-2 execution/local-dynamic regressions and must
  remain visible, but they do not invalidate Stage-1 static route acceptance;
- the architecture failure was not a behavior regression: the checker still expected
  `...Map...` control-field names while production had already migrated to explicit
  `...System...` names.

Fixes committed after this run:
- updated `check_navigation_live_runtime_control.py` to the current System-frame field
  names and `applySystemAccelerationDemand` API;
- labeled `nominal_route_planner` with CTest label `navigation_stage1`;
- added `tests/navigation_runtime/run_stage1_mingw64.sh` as the focused Stage-1 gate.

The focused gate runs only:
1. shared geometric-path architecture contract;
2. Stage-1 nominal-route architecture contract;
3. `nominal_route_planner` behavioral test;
4. Stage-1 viewer build.

This is not hiding the old red tests. It prevents Stage-2 failures from being
misreported as Stage-1 route-construction failures.


## 2026-09-21 — Stage-1 viewer observability fix after user screenshot

User screenshot showed a blank 3D area before pressing `РАССЧИТАТЬ`. After calculation the scene/route appeared, but there was no movement, making it impossible from the UI to tell whether Planner or Follower had failed.

Clarification: Stage 1 is route-only by design; Follower is not linked or executed. The UI was misleading because legacy playback controls remained visible as ordinary controls and the pre-calculation scene had no sufficiently explicit authored-scene rendering contract.

Fixes now committed:
- `TraceDocument` carries explicit authored `sceneStartMapMeters` / `sceneFinishMapMeters` independent of route existence;
- `loadScenarioPreview()` and calculated results populate those endpoints;
- viewer renders a reference grid, a large green START marker, a large yellow FINISH marker/ring, static obstacle geometry and Cobra-at-start before calculation;
- `fitCamera()` explicitly includes authored scene endpoints, so preview framing no longer depends on a calculated route;
- fixed diagnostics panel is placed below the top controls and shows the chain state without vertical reflow;
- Stage-1 playback controls are visually labeled `ПОЛЁТ: ЭТАП 2` / `FOLLOWER: OFF` and cannot start playback when only the one route frame exists;
- `ScenarioRunResult` now exposes diagnostics;
- calculation writes console diagnostics plus `tools/navigation_runtime/last_route_plan.log`;
- diagnostic chain explicitly distinguishes `SCENE`, `PLANNER`, and `FOLLOWER: NOT RUN (STAGE 1)`;
- Stage-1 architecture checker now pins pre-calculation scene endpoints, preview loading and diagnostic ownership.

This does not start Stage 2. No Follower/physics execution was reintroduced.

Target validation pending for these viewer/diagnostic changes.


## 2026-09-21 — Follower restored as Stage 2 of the same diagnostic stand

User correctly rejected the previous interpretation where splitting work into two stages
removed Follower/physics from the viewer entirely. The intended split is by ownership,
not by executable:

```text
Stage 1: Calculate
  static scene -> NominalRoutePlanner -> retained route polyline

Stage 2: Execute
  retained route polyline -> time trajectory -> Follower -> PilotSkill -> physics
```

The same viewer now supports both stages.

### Stage 1 remains unchanged

`РАССЧИТАТЬ` calls only `NominalRoutePlanner`. It builds the static route once. The
user's target evidence already showed a valid four-point 323.75 m detour around the
wall. Control law, pilot skill and flight style do not alter this nominal route.

### Stage 2 restored correctly

After a successful Stage 1 the playback button becomes `ЗАПУСТИТЬ ПОЛЁТ`.

Stage 2 consumes `calculatedRoute.routePoints` directly and MUST NOT invoke
`NominalRoutePlanner::plan`, `NavigationRuntimePlanner::plan`, or any other global
route search.

Current static Stage-2 chain:

```text
retained Stage-1 polyline
 -> TrajectoryGenerator / RuckigRoutePlanner
 -> time-parameterized trajectory
 -> bounded AcceptedManeuverProgram chunks
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> SharedShipPhysics + DynamicMotionSystem
 -> playback trace
```

The former stand-local `makeShortProgram()` quintic shortcut is NOT restored.

### Execution modes

The existing viewer selectors now become real Stage-2 inputs:
- Newtonian / Assisted -> physical local flight law + reference-attitude policy;
- Expert / Average / Loser -> real PilotSkillExecutor profiles;
- Standard / Extreme -> Ruckig route speed request.

Pilot execution starts from neutral revision zero so the first real route intent exercises
the selected pilot's reaction-delay model.

### Static-only dynamic boundary

Dynamic/sudden obstacles remain reserved for the next pass. Stage 2 currently reports
`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`; it does not silently use the sudden
obstacle checkbox and does not rebuild the route.

### Diagnostics

Stage 2 writes:
- stdout lines prefixed `[NAV-STAGE2]`;
- `tools/navigation_runtime/last_execution.log`;
- `tools/navigation_runtime/last_execution_trace.json`.

Diagnostics separate Ruckig trajectory generation, Follower, Pilot bridge, final errors,
route deviation and coarse static contact.

### Physical-proof limitation

The current static execution path uses the coarse route envelope for static collision
checks and is useful for observing Follower/physics behavior. It is NOT yet final B6
oriented swept-hull/tunnel proof. Do not promote a visually successful run to full
navigation acceptance.

### Validation state

This restored Stage-2 execution path is committed but has NOT yet been compiled/run on
the user's MinGW64 target. The last target-verified fact remains: Stage-1 nominal route
planner passed and produced the four-point 323.75 m static detour.


## 2026-09-21 — immutable retained route for Stage-2 comparisons

Viewer state now keeps the successful Stage-1 `TraceDocument` as a separate immutable
retained-route snapshot. Stage-2 execution traces no longer replace the only copy of the
Planner result.

Changing control law, pilot skill, flight style or the reserved sudden-obstacle toggle
after an execution run restores the retained Stage-1 route and marks only Stage 2 stale.
The next execution run reuses the exact same route without calling Planner again.

This is required for clean comparisons such as Expert/Newtonian vs Average/Assisted:
route geometry is held constant while only the execution layer changes.


## 2026-09-21 — target gate false negative in Stage-2 architecture checker

Target run stopped before compilation with:

`[FAIL] static-route/two-stage navigation: Stage-2 execution path missing TrajectoryGenerator::generate`

This was a checker defect, not a runtime defect. The checker isolated only the body of
`executeCalculatedRoute()`, while the actual Ruckig call is correctly delegated to
`buildExecutionTrajectory()`:

```text
executeCalculatedRoute()
 -> buildExecutionTrajectory()
 -> TrajectoryGenerator::generate()
```

Fix committed in `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`:
- execution function must call `buildExecutionTrajectory()`;
- trajectory-builder slice must contain `calculatedRoute.routePoints` and
  `TrajectoryGenerator::generate`;
- global-planner prohibition is checked across the owned Stage-2 trajectory-builder +
  execution text.

No production navigation/runtime behavior changed in this fix.

Validation state: target compilation of the restored Stage-2 viewer still has NOT been
reached. Rerun the focused gate from latest main.


## 2026-09-21 — second Stage-2 architecture checker false negative

Target gate stopped with:
`Stage-2 execution path missing DynamicMotionSystem::applySystemAccelerationDemand`.

Runtime call is present. The checker failed because production formatting splits the C++
scope operator/call across lines:

```cpp
game::navigation::DynamicMotionSystem::
    applySystemAccelerationDemand(...)
```

The architecture checker previously searched an exact single-line string.

Fix:
- added whitespace-insensitive C++ token normalization via `compact_cpp()`;
- Stage-2 execution and trajectory-builder call checks now compare normalized token
  streams rather than raw formatting;
- runtime behavior is unchanged.

Validation state: target compilation of the restored two-stage viewer still has not yet
been reached; rerun the focused gate from latest main.


## 2026-09-21 — Follower first-chunk handoff bug + full initial kinematics

Target Stage-2 execution reached real runtime and produced:
- cached Planner route OK;
- Ruckig trajectory OK, 1521 samples;
- 102 AcceptedManeuverProgram chunks;
- Follower FAIL after only 24 viewer frames;
- max Follower position error only 0.02 m;
- final position still ~295 m from goal.

The failure signature localizes the problem to the first AcceptedManeuverProgram chunk
boundary, not route tracking quality. At Standard 10 m/s the Ruckig sampling interval is
0.05 s for this scenario. A 16-sample program therefore spans about 0.75 s, matching the
~24 trace frames at 30 Hz before failure.

Root cause found in Stage-2 program selection:

```cpp
vehicle.timeSeconds + 1.0e-9 >= next.acceptedAtUniverseTimeSeconds
```

This can activate the next program fractionally before its acceptance time. The canonical
ManeuverProgramSampler treats any negative elapsed time as `BeforeStart`, and
TrajectoryFollower maps `BeforeStart` to `InvalidInput`.

Fix committed:
- remove the positive epsilon from future-program activation;
- keep the current chunk until simulation time is actually >= next acceptedAt;
- pre-sample the active program before Follower and record exact failure reason/index/
  execution time/program acceptedAt in Stage-2 diagnostics;
- architecture gate now forbids the early-switch expression and pins the new diagnostics.

The user's video is consistent with this: Cobra moves only a few meters, then execution
stops at the first chunk handoff.

### Initial ship state correction

The scenario previously supplied only position, velocity and body orientation. That was
incomplete for a physically continuous Newtonian execution handoff.

Stage-2 initial kinematic state is now explicitly:
- position P;
- linear velocity V;
- linear acceleration A;
- body orientation (forward/up basis);
- pitch/yaw/roll angular rates.

New scenario fields under `start`:
- `acceleration`;
- `pitch_rate_rad_s`;
- `yaw_rate_rad_s`;
- `roll_rate_rad_s`.

Default current scenario remains:
- P = (0,0,0) m;
- V = (6,0,0) m/s;
- A = (0,0,0) m/s2;
- forward = +X;
- up = +Y;
- angular rates = 0.

`TrajectoryGenerationRequest` now owns `initialAccelerationMps2`, validates it, and
passes it into the initial Ruckig state instead of hardcoding zero. A focused Ruckig route
test now requires initial acceleration to be preserved at the first trajectory sample.

The execution vehicle initializes pitch/yaw/roll rate from the scenario. No global route
recalculation semantics changed.

Validation state: these fixes are committed but not yet target-rebuilt/re-run.

## 2026-09-22 — runtime purity / source-of-truth audit after dense-program regression

Fresh target evidence after preserving every dense trajectory interval:

```text
10 -> 10 Newtonian:
    trajectory samples              1333
    program phases                    90
    actuator segments               1332
    source coverage             1332/1332 COMPLETE
    phase handoffs                     0
    final speed                     0.00 m/s
    final error                   270.27 m
    reference hold                56.62 s

27.8 -> 26 Newtonian:
    trajectory samples               613
    program phases                    42
    actuator segments                612
    source coverage               612/612 COMPLETE
    phase handoffs                     0
    final speed                    14.21 m/s
    final error                   148.72 m
    reference hold                42.22 s
```

The dense-source preservation itself is correct as data preservation, but it
exposed two architecture defects.

### Immediate stop-and-return root cause

At the beginning of the 27.8 m/s run the accepted reference changes forward
direction by about 1.4323 degrees in 0.00833 s. That is approximately
3.0 rad/s immediately from a zero-angular-rate start.

`buildReferenceAttitudes()` constrains the per-sample orientation delta by
`maxAngularRate * dt`, but does not integrate the vehicle's angular
acceleration limit. Therefore the authored reference may jump from omega=0 to
near max omega in one sample even though the Cobra has finite
`angularAccel = 3 rad/s^2`.

The tracking envelope permits only 0.8 rad/s angular-rate error, so the follower
leaves the envelope almost immediately.

Runtime then performs:

```text
if trackingErrorExceeded:
    activeProgramReferenceDelaySeconds += dt

programTime = wallTime - activeProgramReferenceDelaySeconds
```

This freezes the moving reference clock. The phase gate receives the same
frozen time, therefore it cannot reach the nominal phase end. The observed
`PHASE HANDOFFS: 0` is a direct consequence.

Outside the envelope B10 correctly disables moving-reference feed-forward and
uses bounded feedback toward the frozen reference. But when the reference is
held indefinitely, that defensive recovery mode becomes a stale-point homing
controller: the craft brakes to zero and then returns toward the old reference
point. That exactly explains the current visible "flies, stops, turns back"
behavior.

### Storage page != physical maneuver phase

The <=16-sample fixed-capacity object is a storage/execution page. After dense
preservation, one continuous trajectory became 42 or 90 Program objects.

The runtime currently treats every page as a semantic maneuver phase:
- new accepted time;
- phase gate;
- possible capture/reacquisition;
- independent reference delay.

That is wrong. Paging a continuous program must not alter maneuver semantics or
time.

Target rule:

```text
ONE continuous ManeuverProgram timebase
    -> many fixed-capacity storage pages if needed

page transition:
    storage/index operation only

maneuver phase transition:
    semantic event only when Planner authored a real phase boundary
```

### Current function / ownership audit

| Area | Verdict | Reason |
| --- | --- | --- |
| authoritative world/static geometry | GREEN | factual transforms/HitVolumes and collision queries are coherent for the fixture |
| Stage-1 NominalRoutePlanner | GREEN for topology only | returns a collision-free coarse detour; its speed clearance is a heuristic reserve, not a dynamics proof |
| SmoothPath geometric sampling/proof | GREEN for geometry only | checks safe geometry/curvature candidates; does not prove Cobra propulsion |
| Ruckig scalar solver | GREEN as numeric primitive | solves the 1-D request it is given; it does not know hull/thruster topology |
| DynamicMotionSystem Newtonian nav allocator | GREEN/LOCAL | aft main is forward-only, residual is bounded RCS, physical velocity is integrated |
| ManeuverProgramSampler | GREEN as sampler | deterministic interpolation; must not be made responsible for semantic paging |
| PilotSkill bridge | YELLOW | clean execution-quality layer, but its latency/slew is not yet included in Planner feasibility |
| AcceptedManeuverProgram actuator fields | YELLOW | correct direction, but currently observe-only and authored after trajectory geometry |
| ManeuverPhaseGate | YELLOW locally / RED integration | gate logic is coherent; feeding it a frozen page clock makes it deadlock |
| buildReferenceAttitudes | RED | rate-limited but not angular-acceleration reachable |
| reference-clock hold/reacquire policy | RED | can freeze forever and command return to stale reference |
| dense Program chunks as phases | RED | storage representation changes execution semantics |
| executionVehicleProfile | RED | converts maxLinearGs into symmetric forward/braking scalar authority independent of hull attitude |
| buildExecutionGuide/globalGuideSpeedLimit | RED for physical planning | cornering is primarily sized from RCS lateral authority and clamped local Bezier cuts, not combined main+RCS+hull reachability |
| hard-coded runtime cobraParams() | RED | duplicate of authoritative ship descriptor |
| CapabilitySnapshot reverse authority | RED | publishes symmetric reverse acceleration although current Cobra has no fore main |
| manual Assisted longitudinal model | RED SSOT | still implements a symmetric aft/fore main-thrust pair, contradicting current aft-only propulsion truth |
| throttle slew in navigation demand path | RED/incomplete | descriptor has throttleAccel, but nav main acceleration is applied immediately |
| turnRadius=20 m | RED/dead truth in this path | descriptor field is not authoritative for Stage-2 maneuver geometry |

### Source-of-truth verdict

There is currently **no single vehicle-dynamics source of truth**.

At least these competing representations exist:
1. `EliteCobraMk1Descriptor()`;
2. duplicated `cobraParams()` in the runtime stand;
3. simplified `NavigationVehicleProfile`;
4. `AcceptedManeuverProgram::CapabilitySnapshot`;
5. separate manual Assisted propulsion semantics.

Target replacement is one immutable
`VehicleDynamicsProfile / PropulsionCapability` derived from the authoritative
descriptor and passed unchanged through Planner, proof, AcceptedManeuverProgram,
Autopilot and physics where appropriate.

It must explicitly carry:
- installed main-thruster directions (aft main, optional real fore main);
- main acceleration and throttle slew;
- RCS vector authority / consumable model;
- angular rate and angular acceleration limits;
- controlled speed/load limits;
- hull/collision envelope;
- mass/inertia where required by proof.

Derived scalar constraints are views of this profile, never competing truth.

### Test-gate audit

`tests/navigation_runtime/run_stage1_mingw64.sh` does NOT execute the Stage-2
E2E pipeline. It builds the viewer and intentionally stops after Stage-1 +
component tests.

The true viewer pipeline test is wired in
`tools/navigation_runtime/CMakeLists.txt` as:

```text
navigation_runtime_pipeline
```

Therefore a viewer build PASS is not end-to-end acceptance.

Required target gate after the build:

```bash
ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

The current code is expected to FAIL this gate until the timebase/reacquisition
and physical-authoring defects are corrected.

### Repair order

1. Separate storage pages from semantic maneuver phases and restore one
   continuous program timebase.
2. Remove indefinite FreeTransit reference-clock freezing. A materially
   unreachable accepted program is invalidated/replanned from current physical
   state; the Autopilot does not home forever to an obsolete point.
3. Make attitude authoring enforce angular acceleration as well as angular rate.
4. Replace all duplicated/symmetric capability descriptions with one
   authoritative VehicleDynamicsProfile.
5. Move physical geometry + attitude + main/RCS/throttle allocation/proof ahead
   of final Ruckig timing.
6. Only after Planner emits a proved physical ManeuverProgram, switch the
   explicit actuator segments from observe-only to Autopilot authority.

## 2026-09-22 — audit update after API/SSOT cleanup

The following previous RED findings are now structurally corrected:

| Finding | New status |
| --- | --- |
| hard-coded runtime cobraParams() | GREEN: removed; vehicle supplied explicitly |
| duplicated hull envelope in scenario | GREEN: removed; vehicle geometry input only |
| symmetric fake reverse-main capability | GREEN for common projections: derived from installed topology |
| vehicle capability revision mixed with map revision | GREEN: separated |
| file-scope execution dt / reference timeout | GREEN: explicit ScenarioNavigationPolicy |
| storage pages treated as maneuver phases | GREEN structurally: one monotonic clock |
| indefinite stale-reference clock hold | GREEN structurally: bounded invalidation |
| angular reference ignores angular acceleration | GREEN structurally: acceleration-reachable integration |
| manual Assisted virtual fore-main | GREEN structurally: uses installed topology |

Still RED / not yet physically accepted:
- execution-guide geometry is not yet produced from combined hull + main + RCS
  reachability;
- scalar trajectory timing does not include finite lead-rotation / throttle slew
  proof;
- explicit ActuatorSegments are still observe-only in the Stage-12 stand;
- physical maneuver proof is still performed too late;
- target MinGW64 compile/E2E has not validated this cleanup.

New architecture checker pins the corrected ownership so these old defects cannot
silently return.
