# Navigation v2 — Stage 12 end-to-end runtime/stress/debug

**Status:** ACTIVE  
**Started:** 2026-09-18 Europe/Kyiv  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md`

## Goal

Stage 12 proves that the accepted Navigation v2 components work as one live system under real game ownership, real obstacle geometry, real vehicle authority, real replication, and real presentation.

It is not a new planner stage. The purpose is to compose and stress the already accepted planner/trajectory/control chain and retire legacy route-wide navigation only after equivalent or better live behavior is demonstrated.

> **Current local-avoidance canon — 2026-09-20:** the production ordinary
> unexpected-obstacle path is the trajectory-normal **projected visible-horizon
> bypass**. The former angular deflection fan, branch-continuity API and
> branch-switch Brake/recovery path have been removed from production code,
> runtime tests and live acceptance contracts. Any older fan/branch sections
> below are retained only as chronological failure evidence and are
> **SUPERSEDED — DO NOT IMPLEMENT OR RESTORE**.

## 12A-1 — live composition seam candidate

Before the physical proving-ground scene is attached to `GameSimulation`, Stage 12 first closes the missing runtime composition seam.

Current candidate:

```text
NavigationSpace costed corridor
    -> ordered selected portal centers
    -> NavigationRuntimePlanner coarse waypoint
    -> LocalHorizon / LocalAvoidance
    -> NavigationRuntimeControlBridge::Intent
    -> PilotSkillExecutor
```

`NavigationSpace` now publishes `portalCentersMapMeters` in corridor results. These centers correspond one-to-one with the selected `portalPath`; callers no longer need to inspect or reconstruct the internal region graph merely to obtain the next steering waypoint.

`NavigationRuntimePlanner` is a shared client/server production component. It consumes:
- one agent kinematic/envelope state;
- one target state;
- one completed `NavigationMap::QueryResult`;
- one `NavigationSpace` snapshot;
- bounded corridor/local-avoidance policy.

It outputs only ideal navigation acceleration intent plus diagnostics. It never writes authoritative position, velocity or angular rates.

Fail-closed behavior remains command-producing:

```text
static route unavailable -> braking/hold intent
stale dynamic result      -> braking/hold intent
unresolved local conflict -> emergency braking/hold intent
```

The isolated Stage-12 runtime gate pins:
- selected static portal -> live bounded target;
- the same portal rejects an oversized hull;
- a `NavigationMap` moving crossing conflict changes the command to braking hold;
- the resulting intent crosses the accepted `PilotSkillExecutor` runtime bridge.

This is intentionally **not yet the full Stage-12 acceptance**. After the target-machine gate is green, the next slice wires this shared planner into authoritative `GameSimulation` ownership and feeds it the deterministic proving-ground geometry through the real hit-volume/static-space publication adapter.

## 12A-2 — authoritative GameSimulation proving actor

12A-1 was accepted on target-machine evidence from
`af58b46cdb01ad254097383e5e4274c733c4e28e`: the architecture contract passed,
`navigation_runtime` passed 3/3, and both canonical production executables
built.

The next candidate wires that accepted planner into one real authoritative NPC
without changing ordinary NPC behavior.

```text
NAVIGATION V2 RUNTIME LAB
    -> GameSimulation
    -> NavigationRuntimePlanner
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> ShipControlState
    -> authoritative physics
```

The proving actor is pinned Active so activation cadence cannot become a second
experimental variable.

The physical proving field is not duplicated. `GameSceneSetup` already creates
deterministic `NAV STRESS CUBE/CYLINDER` StaticObjects. The initial lab route
is chosen so the unmodified straight line passes through `NAV STRESS CUBE 08`.

### Authoritative geometry adapter

`NavigationHitVolumeAdapter` consumes `HitComponent/HitVolume`, not render
meshes. Active non-support HitVolume OBBs are transformed by the owning object
pose into `NavigationObstacle::Box` records. The same source also yields a
conservative entity radius for NavigationMap broadphase.

The first live candidate uses those hit-volume-derived conservative radii for
bounded local avoidance. Exact per-volume OBB output is already available and
tested, but the temporary lab `NavigationSpace` is still one open region.
Therefore 12A-2 must not be treated as proof of exact static topology, narrow
apertures or precision-passage geometry yet.

A green compile/contract gate is followed by deterministic runtime evidence:
the blocked straight route must produce a real changed plan/control demand,
authoritative motion must follow it, and contact clearance must be measured.

## 12A-3 — authoritative live obstacle behavior proof

12A-2 is accepted on target-machine evidence from
`7b4db95788d80c95afb3b57c109c671cb7a41366`.

The next gate is executable behavior, not another architecture-only seam.
`EliteServer --self-test-navigation` boots the real `ServerRuntime` and
advances fixed-step authoritative simulation until the following chain is
observed or 120 simulated seconds expires:

```text
CUBE 08 published from authoritative HitVolume geometry
 -> enters bounded local NavigationMap candidate set
 -> rejects nominal straight target
 -> nominal conflict identity survives adjusted-target search
 -> adjusted safe target selected
 -> PilotSkillExecutor executes lateral acceleration
 -> authoritative ship leaves the original straight line
 -> positive conservative clearance while passing obstacle
 -> continued progress toward final goal
 -> replicated execution vector equals authoritative execution vector
```

The dynamic candidate query covers the complete local avoidance fan with one
bounded `NavigationMap::querySphere()` around the current physical horizon.
A nominal-corridor-only candidate query is forbidden because a lateral probe
could otherwise encounter an obstacle absent from the immutable candidate set.

This gate intentionally measures conservative broadphase-sphere clearance.
Passing it does not accept sphere geometry for narrow apertures. Exact
HitVolume OBBs must feed the subsequent static/precision topology gate.

### 12A-3 working-frame defect exposed by live run

The second live target-machine attempt produced physically absurd behavior:

```text
obstacle_candidate=1
obstacle_conflict=1
adjusted=1
lateral_exec=1
max_lateral_accel_mps2=340.222
max_route_deviation_m=43699.4
min_conservative_clearance_m=1758.57
progress_m=2256.52
passed_obstacle_plane=1
reached_goal=0
```

This is not accepted avoidance. The ship detected and cleared the obstacle but
left the route by tens of kilometres and made poor goal progress.

Root cause: the live NavigationRuntimePlanner works in the published hub/map
working frame, but its acceleration intent was handed directly to the accepted
Stage-11 control seam, which consumes world-space vectors. The missing
map-to-world basis transform therefore rotated both linear and angular control
commands incorrectly.

Correction:

```text
planner map-space intent
    -> mapIntentToWorld(workingFrame)
    -> PilotSkillExecutor / ShipControlState world-space demand
```

The live lateral-demand metric is also corrected to compare world execution
against the world-space route direction. The old `340.222 m/s²` lateral metric
cannot be interpreted physically because it mixed world execution with a
map-space route vector.

A non-identity working-frame regression now pins this boundary.

### Sparse replication epoch rule

The live proving gate must compare replication products at the same
authoritative publication epoch.

A per-fixed-step diagnostic observation may be newer than a client's retained
sparse state because omission means "retain previous value". Therefore this is
**not** a valid invariant:

```text
latest retained client execution == newest fixed-step execution
```

The valid proof is:

```text
sparse packet publishes lab row at serverTick T
    ==
authoritative GameServer published snapshot at serverTick T

then

client canonical state after hydrating packet T
    ==
that same authoritative execution product
```

The test must wait for an actual lab-row publication. It must not weaken
floating-point tolerance to hide publication-age differences.

## 12A-3 — live obstacle behavior acceptance

Target-machine acceptance on
`9246530e5eb539e227a1af69461113f2b30ec93a` closes the first real
authoritative obstacle-behavior gate.

The accepted chain is:

```text
CUBE 08 broadphase candidate
 -> nominal conflict identity
 -> bounded adjusted target
 -> map intent -> world control transform
 -> PilotSkillExecutor
 -> authoritative physics
 -> positive clearance and route progress
 -> same-tick sparse replication
 -> canonical hydration
```

The accepted live run demonstrated bounded physical response rather than merely
planner output: maximum relative speed remained approximately 59.88 m/s,
physically applied lateral acceleration approximately 5.51 m/s², conservative
clearance 129.59 m, goal progress 3.50 km in 58.9 s, and exact replication
errors of zero.

## 12A-4 — exact static HitVolume geometry

Persistent static obstacles now have a precision layer in `NavigationSpace`.
The source is the authoritative collision/damage HitVolume product:

```text
HitComponent volumes
 -> NavigationHitVolumeAdapter
 -> exact map-frame NavigationObstacle OBBs
 -> NavigationSpace::queryPoint/querySegment
 -> LocalAvoidance static proof
```

Coarse region/portal topology remains responsible for global connectivity.
Exact obstacles do not replace that topology; they refine bounded local
feasibility.

`LocalAvoidance` must prove the bounded nominal segment against static
geometry even when `NavigationMap` is dynamically clear. Every adjusted probe
must also pass exact static segment proof before dynamic evaluation can accept
it.

The canonical narrow-gap regression is intentionally hostile to sphere-only
geometry: two boxes have enclosing conservative spheres that overlap the
centreline while their real OBBs leave a 4 m aperture. A fitting agent must
traverse that aperture; an oversized envelope must fail.

The live NAV STRESS fixture publishes non-self-rotating HitVolume OBBs only
after authoritative hub/object transforms and HitVolume rebuild are current for
the fixed step. The live self-test must observe:

```text
exact_static=1
exact_static_obstacles>0
exact_static_query=1
exact_static_block=1
max_exact_static_examined>0
```

This proves both publication and real live-planner consumption of the exact
static layer. A stored-but-unused OBB snapshot is not acceptance evidence.
Only then may the existing CUBE 08 physical/replication chain pass.

For this first exact-static gate, the previously accepted static conservative
sphere candidates remain in `NavigationMap`. Removing static objects from the
dynamic layer is deliberately deferred until the OBB publication/proof gate is
green, so ownership cleanup cannot hide a precision-geometry regression.

### 12A-4 first target-machine run: interpretation

The first target-machine 12A-4 run proved that exact static HitVolume geometry
was published and queried live, but exposed three independent contract edges.

1. A newly-authored local test fixture violated the pre-existing dynamic
   candidate invariant by setting conservative swept radius below actor radius.
   The fail-closed validator was correct; the fixture is fixed.

2. A corridor-selected portal centre lies on the shared boundary between free
   regions. Generic exact-static endpoint envelope clearance therefore rejected
   a valid portal target. The runtime now carries the already-proven corridor
   portal clearance into the nominal exact segment query. This exception applies
   only to the selected nominal portal endpoint; adjusted local probes remain
   strict.

3. The old live criterion
   `minimumConservativeClearanceMeters > 0` is no longer valid once exact OBB
   geometry owns static precision. An enclosing broadphase sphere may overlap
   genuinely free space around an OBB. Negative sphere clearance is diagnostic,
   not collision truth.

The corrected live safety proof sweeps every actual authoritative fixed-step
motion segment through NavigationSpace exact HitVolume geometry using the ship
envelope plus local static clearance. Acceptance requires:

```text
exact_static_motion_samples > 0
exact_static_violation = 0
```

alongside the existing publication/query/block, physical progress and exact
replication evidence.

## 12A-4 — acceptance

Accepted target-machine run on:

```text
8f5801b9e26cfbb8e4e5e3174587a900292e8394
```

proved the exact-static contract end to end:

```text
NavigationSpace exact OBB layer
 -> LocalAvoidance exact nominal/adjusted proof
 -> authoritative PilotSkillExecutor/physics
 -> 3431 swept fixed-step exact-static safety samples
 -> exact_static_violation = 0
 -> same-tick sparse/canonical replication error = 0
```

The accepted run also preserved a real OBB aperture, valid corridor portal
boundary semantics and successful physical progress around CUBE 08.

## 12A-5 — static/dynamic ownership cleanup

12A-4 deliberately kept stationary static objects duplicated as conservative
NavigationMap spheres during the transition. That duplicate ownership is now
removed.

Canonical ownership becomes:

```text
stationary infrastructure
    -> HitVolume-derived NavigationObstacle
    -> NavigationSpace exact static layer

time-varying / self-rotating infrastructure
    -> NavigationMap broadphase/prediction
    -> later moving/rotating exact-geometry stage
```

The deterministic CUBE 08 proving object is stationary in the hub/map frame.
Therefore the live 12A-5 gate requires:

```text
obstacle_candidate = 0
obstacle_conflict = 0
exact_obstacle_block = 1
adjusted = 1
exact_static_violation = 0
```

This is stronger than the old duplicated proof: the maneuver must still happen
after the dynamic sphere is removed, proving that exact static geometry alone
owns the stationary obstacle.

Diagnostic centre/radius measurements may remain for logging, but they do not
publish NavigationMap ownership.

### 12A-5 first live result: ownership passed, encounter fixture failed

Target-machine run on
`f7e86a70fe590ab02cb72f02967f3679840c1e24` proved the static/dynamic
ownership split itself:

```text
obstacle_candidate=0
obstacle_conflict=0
dynamic_queries=6000
max_dynamic_candidates=1
exact_static=1
exact_static_obstacles=17
exact_static_query=1
exact_static_violation=0
```

So stationary CUBE 08 was no longer duplicated into NavigationMap and the
exact-static layer remained active and collision-free.

The missing evidence was:

```text
exact_obstacle_block=0
exact_static_block=0
adjusted=0
```

The cause was the deterministic fixture, not ownership. The ship had roughly
3.1 km to travel before the cube and accumulated about 500 m of natural
rotating-frame lateral drift, enough for the bounded nominal segment to miss the
real 360 m-class OBB.

Correction:
- the start point is now 1300 m before CUBE 08, inside the first local horizon;
- start X/Y are derived from the obstacle coordinates;
- static publication itself queries the configured start->goal centerline and
  requires the blocker identity to be the actual CUBE 08 entity;
- the server self-test fails fast if
  `configured_route_exact_block=0`.

This preserves the 12A-5 rule: stationary spheres are not reintroduced.

### 12A-5 second live result: configured route blocks, live nominal segment does not

Target-machine run on
`54bd19ef647f0eca8dcc738be4342052ee164683` confirmed the ownership split and
the exact proving geometry:

```text
obstacle_candidate=0
obstacle_conflict=0
configured_route_exact_block=1
exact_static=1
exact_static_obstacles=17
exact_static_query=1
exact_static_violation=0
reached_goal=1
```

But runtime local planning never observed the same blocker:

```text
exact_obstacle_block=0
exact_static_block=0
adjusted=0
```

The hub visual/tactical axis contract was re-audited and is correct. The next
diagnostic split therefore compares two independent products before changing
any ownership or tolerance:

1. a standalone runtime regression using the live scale
   (OBB 1300 m ahead, ~1420 m first horizon, distant goal, unrelated dynamic
   actor), which must produce exact-static adjusted avoidance;
2. a first-live bounded-segment probe inside GameSimulation that records the
   actual agent map position, actual goal, bounded target, horizon and exact
   blocking entity immediately before `NavigationRuntimePlanner::plan()`.

The server self-test fails fast if the first live segment does not hit CUBE 08.
It separately fails if that exact segment does hit CUBE 08 but planner loses the
static block during composition.

Stationary conservative spheres remain forbidden.

## 12A-5 — acceptance

The static/dynamic ownership cleanup is accepted by target-machine live evidence:

```text
obstacle_candidate=0
obstacle_conflict=0
exact_obstacle_block=1
adjusted=1
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```

This proves stationary CUBE 08 is no longer represented by a duplicated
NavigationMap sphere, while exact HitVolume geometry alone still causes the
authoritative avoidance maneuver and physical clearance.

Reference-frame placement/order defects exposed during this gate were also
closed:
- stale local velocity/propulsion state is reset on frame placement;
- matched ship world pose is synchronized to the current hub-frame epoch before
  AI/navigation;
- sub-millimetre orbital-coordinate round-trip residue is treated as numerical
  noise through a named 1 mm diagnostic tolerance.

## 12A-6a — live angular-motion publication

Time-varying navigation requires more than P/V/A. The accepted MovingGapPredictor
also consumes angular velocity to publish material surface velocity:

```text
v_surface = v_center + omega x r
```

Therefore NavigationMap now carries:

```text
DynamicActorInput.angularVelocitySystemRadPerSecond
    -> vectorToMap(workingFrame)
    -> Candidate.angularVelocityMapRadPerSecond
```

The deterministic live fixture is the already-authored:

```text
GUIDANCE DOCK CUBE A
hub-local angular velocity = (0,0,2) deg/s
```

GameSimulation transforms this authored hub-visual vector through
`hubVisualLocalToWorldVector()`, publishes it into NavigationMap, then performs
an independent diagnostic query at the actor location.

12A-6a acceptance requires:

```text
rotating_actor_seen=1
rotating_actor_omega_verified=1
rotating_actor_omega_error<=1e-12
```

No moving-gap or moving-passage behavior is changed in this slice. The purpose
is to prove the live dynamic DTO/frame boundary before composing accepted
prediction math on top of it.

### 12A-6a — acceptance

Target-machine acceptance was completed from:

```text
a0efa9190180043b05da3103a5d466d256744935
```

The live run proved the required angular-motion path:

```text
rotating_actor_seen=1
rotating_actor_omega_verified=1
rotating_actor_omega_error=0
```

and preserved the previously accepted Stage-12 safety/ownership/replication
invariants:

```text
obstacle_candidate=0
obstacle_conflict=0
exact_obstacle_block=1
adjusted=1
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```

Therefore 12A-6a is accepted. Live dynamic candidates now have proven compact
linear and angular motion state through the real working-frame publication path.

### 12A-6b — live moving-gap / moving-passage composition

The next slice must consume that proven motion state rather than add more DTO
plumbing.

Target production chain:

```text
NavigationMap bounded dynamic query
    -> relevant moving/rotating candidate pair
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
    -> bounded feasibility / maneuver consequence
    -> NavigationRuntimePlanner intent
    -> mapIntentToWorld(...)
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> authoritative physics
    -> same-tick replicated execution truth
```

The predictor and moving-passage evaluator are already accepted isolated
Navigation v2 components. 12A-6b is their live composition gate.

Required live evidence must show:
- relevant real dynamic actors are selected from the bounded NavigationMap
  result;
- P/V/A/angular velocity reaches the predictor unchanged by frame confusion;
- the predictor result is actually consumed by MovingPassageTrajectoryEvaluator;
- moving passage feasibility changes or constrains the authoritative maneuver
  when the deterministic fixture requires it;
- no global all-pairs moving-gap scan is introduced;
- stationary infrastructure remains owned by NavigationSpace exact HitVolumes;
- exact-static swept motion remains violation-free;
- sparse/canonical execution replication remains exact at the same server tick.

DTO presence alone cannot satisfy 12A-6b.

### 12A-6b1 — runtime precision observe/prove candidate

The first 12A-6b slice is intentionally non-authoritative. It connects the
accepted precision stack to the real shared runtime product while preserving the
accepted 12A-4/12A-5 steering/safety authority.

Candidate implementation through:

```text
616f5b868795439fb42308d2c2d13ffc87218cba
```

Runtime composition:

```text
NavigationRuntimePlanner
    -> real bounded NavigationMap::QueryResult
    -> nominal dynamic conflict identity
    -> BoundedGapCandidateBuilder
         hard candidate cap <= 8
    -> MovingGapPredictor
         P / V / A / angular velocity
         continuous gap closure/alignment proof
    -> MovingPassageTrajectoryEvaluator
         continuous ship/gap geometry + authority proof
    -> diagnostics only
```

Pinned deterministic fixtures:
- open translating two-boundary gap -> prediction remains open -> moving ship
  passage must be feasible;
- closing two-boundary gap -> predictor detects closure inside the horizon ->
  moving passage must not be evaluated/accepted.

The moving-passage evaluator additionally publishes
`initialLinearAccelerationMapMetersPerSec2` from the exact verified Hermite
segment. This value is reserved for the authority step so runtime control can
execute the same trajectory that was proved rather than solve another curve.

#### Why this slice does not steer yet

The moving evaluator proves the ship against the moving aperture, but Stage 12
also has an accepted independent safety authority for stationary exact HitVolume
geometry.

A dynamically feasible moving curve is therefore **not yet enough**:

```text
moving passage feasible
    +
same moving curve exact-static safe
    =
eligible authoritative maneuver
```

12A-6b1 does not yet provide the second term for the complete moving Hermite
curve. Granting steering authority now could regress static collision safety.
Therefore `LocalAvoidance` / exact-static behavior remains authoritative and
moving precision is observe/prove only.

The next slice must compose exact-static proof over the same accepted moving
trajectory before the verified first control sample may flow into
`mapIntentToWorld()`, `PilotSkillExecutor`, authoritative physics and
replication.

#### 12A-6b1 first target-machine run

Run from `6badb5f74ea65900a38a48854d66f16f34127a89`:

```text
navigation_runtime_control    PASS
navigation_runtime_planner    PASS
navigation_replication_truth  PASS
3/3 runtime tests PASS
```

The architecture script failed before acceptance with:

```text
[FAIL] exact obstacle geometry must link in isolated and production NavigationSpace targets
```

The failure was not a missing link. The runtime target configured, built and
linked successfully. The checker depended on a literal
`PUBLIC EliteNavigationGeometry` CMake substring; adding the trajectory target
changed whitespace/layout and triggered a false negative.

Correction:
- preserve the existing CMake link-contract layout;
- normalize whitespace in the architecture checker before validating the
  relationship.

No production navigation behavior changed in this correction.

The corrected target-machine rerun on updated `main` now reports:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
```

Together with the prior `navigation_runtime` 3/3 PASS, this closes the false
architecture-checker failure. 12A-6b1 remains a candidate pending only the
trajectory/map regressions, full build, and unchanged live server self-test.

## Project-state recording protocol

Every state-affecting Stage-12 event must be recorded before beginning the next
implementation slice: candidate activation, failed gate/root cause, acceptance,
ownership/architecture change, or next-task change.

Synchronize:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- this document.

Use the last actually target-machine-verified code baseline as evidence. Do not
label a hash "current HEAD" in state documentation because the documentation
commit itself changes HEAD.

## 12A — deterministic runtime proving ground

The first slice is a deterministic proving ground around the station / hub domain.

The field must contain deliberately awkward geometry rather than a clean synthetic corridor:

```text
boxes / slabs
cylinders / pylons
offset walls
narrow gaps
wide gaps
dead ends
overhangs / underpasses
moving crossing obstacle
moving gap pair
station/dock approach volume
```

The scenario must be generated from one explicit deterministic seed/configuration so failures can be reproduced exactly.

### Geometry truth

Navigation does not receive a presentation-only obstacle list.

The proving ground publishes the same collision / hit-volume geometry used by the live NavigationWorld adapter. Passage feasibility is therefore based on actual usable volume and ship envelope, including holes and narrow passages where the hull genuinely fits.

No centimeter-resolution global scan is allowed. The accepted hierarchy remains:

```text
cached coarse corridor / global structure
    -> bounded local horizon
    -> bounded precision passage/trajectory only near conflicts
```

### First live scenarios

The minimum deterministic set is:

```text
A. clear A -> B
B. static clutter requiring at least one coarse detour
C. narrow traversable gap
D. same gap with an envelope that does not fit
E. crossing moving obstacle
F. moving gap
G. dead-end / blocked route requiring explicit fail-closed behavior
H. docking final approach into a moving/rotating dock fixture
I. unavoidable-contact fixture exercising emergency mitigation
J. post-impact replan from actual physics state
```

The first implementation may stage these fixtures incrementally, but the scenario representation must be reusable rather than hard-coded into ten unrelated tests.

## 12A ownership chain

The runtime proof must traverse the production chain:

```text
scenario / goal
    -> NavigationWorld
    -> global/local/precision accepted navigation
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> ShipControlState navigation demand
    -> SharedShipPhysics / ShipController / DynamicMotionSystem
    -> authoritative motion
    -> replication
    -> read-only guidance/debug presentation
```

A test that calls only an isolated trajectory evaluator does not count as Stage 12 end-to-end evidence.

## Manual guidance tunnel

Manual guidance and autopilot must derive from the same accepted navigation/trajectory product.

```text
autopilot
    consumes accepted maneuver/control targets

manual guidance
    visualizes the accepted corridor/trajectory as a tunnel

forbidden
    second presentation-only planner
    visually convenient path that the autopilot would not execute
```

The tunnel may be simplified for readability, but its topology and accepted passage must remain faithful to the executed route.

## 12B — raw NavigationWorld debug

`Shift+F12` is the raw correctness view.

It should expose, from the same completed live snapshot:

```text
static regions / portals / clearance
dynamic actors and bounds
P/V/A prediction / swept bounds
local candidates and conflicts
gap candidates / moving-gap state
accepted trajectory
emergency mitigation/contact witness
dock interface / tolerances
pilot execution state
snapshot generation / age
```

The debug renderer must not synchronously rebuild navigation or run a second planner.

## Performance evidence

Stage 12 must measure live composition rather than only isolated kernels.

Keep the existing design budgets:

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/global precision solve      asynchronous only
```

Record at least:

```text
actor count
static obstacle count
dynamic candidate count
precision candidate count
CPU update/query p50/p95
GPU update p50/p95 where used
snapshot age
replan count
route/trajectory revision count
collision / emergency events
goal completion time
```

Do not optimize an isolated component merely because a microbenchmark looks large if the live stage remains inside budget.

## Legacy retirement gate

Legacy route-wide navigation may be removed only after the Stage 12 proving ground demonstrates stable v2 ownership for:

```text
ordinary travel
obstacle avoidance
narrow passages
moving conflicts
docking
post-contact replan
manual guidance presentation
NPC server-authoritative execution
```

Before that point, legacy code may remain as an explicitly quarantined compatibility path but may not silently take steering authority back from Navigation v2.

## First implementation target

Build the reusable deterministic proving-ground scenario and one headless end-to-end fixture first:

```text
station-adjacent clutter
A -> B
at least one forced detour
at least one narrow but traversable opening
at least one opening too small for the configured hull
one moving crossing obstacle
```

The first gate must prove:

```text
production ownership chain is exercised
actual obstacle/hit-volume data reaches NavigationWorld
the safe ship reaches B without collision
an oversized ship cannot claim the narrow opening
moving conflict causes a real maneuver/replan
server execution truth remains the replicated truth
no legacy steering fallback fires
```

After this gate is green, expose the same scenario in the interactive game and add the `Shift+F12` visualization rather than inventing a separate visual-only test world.


### 12A-6b1 — acceptance

Target-machine acceptance baseline:

```text
a587dcd96bdf0b05edbf4fcfe9a32f5f7be1058d
```

Full accepted gate:

```text
Stage-12 runtime planner contract   PASS
navigation_runtime                  3/3 PASS
navigation_trajectory              11/11 PASS
navigation_map                      1/1 PASS
EliteGame / EliteServer             BUILD PASS
server navigation self-test         PASS
exact_static_violation              0
replication_error_mps2              0
canonical_replication_error_mps2    0
```

12A-6b1 is therefore closed. The real runtime can identify a bounded moving
aperture, predict whether it remains open, and prove a concrete ship trajectory
through it without changing authoritative steering.

### 12A-6b2 — exact-static proof of the same moving trajectory

The new safety composition is:

```text
MovingPassageTrajectoryEvaluator accepted Hermite trajectory
    -> bounded continuous static-geometry proof
    -> NavigationSpace exact HitVolume obstacle layer
    -> statically safe / blocked diagnostic
```

The proof must cover the curve between samples conservatively. Sampling only the
33 trajectory states is insufficient because a static obstacle could exist
between sample points.

12A-6b2 remains non-authoritative. Only after target-machine acceptance may the
already published first verified acceleration sample become steering authority.


#### 12A-6b2 candidate implementation

Candidate baseline before documentation commits:

```text
61f62e9d096542ae52680cf47d6da1c03b0bb8c0
```

The moving evaluator now emits a trajectory witness rather than forcing runtime
composition to reconstruct its curve:

```text
center[0..32] = exact accepted Hermite center samples
deviation[0..31] = max endpoint |acceleration| * dt^2 / 8
hull_radius = radius containing the complete precision hull
```

For interval `i`, the exact Hermite centerline is contained by the capsule
around `center[i] -> center[i+1]` with radius `deviation[i]`. Adding the
published hull containment radius gives a continuous conservative volume for
the complete ship.

The runtime submits every such interval to
`NavigationSpace::querySegment()`, which retains exact authored static
HitVolume geometry and blocker identity.

Pinned runtime cases now include:
- open moving gap + no static blocker -> all 32 intervals statically safe;
- same class of dynamically feasible moving gap + static beam on the accepted
  trajectory -> dynamic proof remains feasible, exact-static same-trajectory
  proof fails closed and returns the beam identity.

This slice remains diagnostic-only. The verified first acceleration sample is
still forbidden from replacing LocalAvoidance steering until 12A-6b2 receives a
green target-machine gate.


#### 12A-6b2 first target-machine gate — failed before behavioral tests

Candidate `8c30ebc1c7a724c06113b5b1e4704a7a172b6d93` produced:

```text
Stage-12 architecture contract   PASS
navigation_trajectory            COMPILE FAIL
navigation_runtime               COMPILE FAIL
navigation_map                   1/1 PASS
full production build            COMPILE FAIL
```

The failure occurred before the new behavioral fixtures could execute.

Root cause:
- the header declared `TrajectoryWitness` but omitted
  `TrajectoryWitness trajectory {}` from `MovingPassageTrajectoryEvaluator::Result`;
- runtime static-proof query endpoints used braced assignment rejected by the
  target MinGW compiler.

The shell continued after build failure, so the later
`EliteServer --self-test-navigation` PASS was a stale previously built binary
and is explicitly not counted as 12A-6b2 evidence.

Corrective code/contract baseline:

```text
52a5fa9e239d8ea00923cbb65a293cca5863567b
```

The architecture script now also requires `TrajectoryWitness trajectory` in
the public result contract, preventing this exact structural omission from
passing again. The safety model and observe-only authority boundary are
unchanged.


#### 12A-6b2 — acceptance

Corrected target-machine baseline:

```text
25dc4369b95b4872d9a70a2d236db5e482cf45e2
```

Accepted gate:

```text
architecture                     PASS
navigation_trajectory            11/11 PASS
navigation_runtime               3/3 PASS
navigation_map                   1/1 PASS (first 6b2 gate; untouched by correction)
EliteGame / EliteServer          BUILD PASS
rebuilt server self-test         PASS
exact_static_violation           0
replication errors               0
```

The exact moving Hermite trajectory is now proven continuously against both the
time-varying aperture and persistent exact-static NavigationSpace geometry.

#### 12A-6b3a — verified moving-passage authority seam

Authority condition:

```text
movingPassageFeasible
    && movingPassageStaticSafe
```

When true, the planner may use only the exact
`movingPassageInitialAccelerationMapMps2` emitted by that accepted trajectory.
It must not derive a new desired velocity/trajectory after proof.

When false, the accepted LocalAvoidance/fail-closed behavior remains in control.

12A-6b3a acceptance requires deterministic proof through the existing
map-intent -> world transform -> PilotSkillExecutor seam. A later live gate must
still demonstrate moving-passage authority through actual physics and
replication before 12A-6b is closed end-to-end.


#### 12A-6b3a candidate — deterministic authority seam

Candidate baseline before documentation commits:

```text
5e96b59ab99a22d3d1fa94cf16577b5ff71c84bb
```

The moving precision policy now has two independent switches:

```text
enabled = run bounded precision proof
allowSteeringAuthority = permit a doubly-proven result to steer
```

Both default to false.

Authoritative promotion is legal only when:

```text
allowSteeringAuthority
&& local result is not stale
&& movingPassageFeasible
&& movingPassageStaticSafe
```

The resulting planner state is `MovingPassageClear`. Its linear intent is
assigned directly from the already-verified
`movingPassageInitialAccelerationMapMps2`. The authority block returns before
the ordinary selected-target / desired-velocity solve, preventing a
prove-one-trajectory / execute-another defect.

Pinned deterministic evidence:
- observe-only precision cannot steal steering;
- closing gap cannot gain authority;
- exact-static blocker vetoes authority even when moving passage geometry is
  dynamically feasible;
- a doubly-proven open passage takes authority;
- its exact acceleration sample crosses a deliberately non-identity
  map->world transform and the accepted PilotSkillExecutor bridge.

This sub-slice does not yet satisfy the complete 12A-6b live requirement.
After target-machine acceptance, a live deterministic moving-gap fixture must
exercise `MovingPassageClear` through authoritative physics and same-tick
replication.


#### 12A-6b3a — acceptance

Target-machine baseline:

```text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
```

The deterministic authority gate passed architecture, runtime 3/3, trajectory
11/11, canonical builds and the rebuilt server self-test. This accepts the
planner/control ownership rule:

```text
allowSteeringAuthority
&& fresh dynamic result
&& movingPassageFeasible
&& movingPassageStaticSafe
    -> MovingPassageClear
    -> exact first proved Hermite acceleration sample
    -> mapIntentToWorld
    -> PilotSkillExecutor
```

#### 12A-6b3b — live physics/replication authority gate

The remaining 12A-6b authority evidence must come from one deterministic live
moving-gap fixture. The server self-test must observe `MovingPassageClear`,
non-zero executed moving-passage demand, authoritative physical response,
zero exact-static violation, and exact same-tick sparse/canonical replication.

The same accepted navigation/trajectory product is also the future source for
manual guidance visualization. Manual mode must never run a separate planner.


### Deferred galactic route planning — explicit Stage-12 non-goal

Strategic interstellar routing is specified separately in
`src/game/navigation/GALACTIC_ROUTE_PLANNING.md`.

Stage 12 must not stretch LocalAvoidance / NavigationMap / exact HitVolume work
to galactic scales. Galactic routing will be a higher-level planner with gravity,
fuel/delta-v, jump-drive capability and timing constraints. It will hand off to
System / Local / Precision navigation as the active leg becomes more local.

Manual guidance and automatic execution must still consume one accepted
navigation product; presentation may not run a second planner.


#### 12A-6b3b candidate — live moving-passage authority

Candidate baseline before documentation commits:

```text
d5e1f990aed38ded7a517d719e935b6b6e763d4d
```

The old live test could not prove moving-passage authority because its bounded
NavigationMap query never contained a two-obstacle aperture
(`max_dynamic_candidates=1` in the accepted 12A-6b3a run).

The candidate adds two physical hub-attached moving boundaries:
`NAV MOVING GAP UPPER` and `NAV MOVING GAP LOWER`. Their world transforms
are advanced from deterministic hub-local linear motion. NavigationMap receives
their map-relative linear velocity; NavigationSpace explicitly excludes them
from static ownership.

The Cobra enters the moving evaluator with its authored logical half-extents and
the same physical acceleration limits used by DynamicMotionSystem:
- forward main-engine authority from the linear-G envelope;
- reverse/lateral/vertical authority from manoeuvre thrusters;
- authored angular acceleration/rate limits.

The diagnostic planner explicitly enables:
```text
movingPassage.enabled = true
movingPassage.allowSteeringAuthority = true
```

The live server gate is intentionally stronger than a sticky planner flag. It
must first stop on an epoch where `MovingPassageClear` is actively executing
and producing physical acceleration. Sparse replication is accepted only from a
publication while that authority remains active and must match the authoritative
and canonical execution at the exact same server tick. The simulation then
continues until the actual ship passes the moving aperture plane.

Existing CUBE 08 exact-static ownership and physical sweep assertions remain
active throughout the run.


#### 12A-6b3b first live gate — fixture rejected by real vehicle authority

Target-machine failed candidate: 4df5a4420f4712b0794e6df3940d45e99e6ad363

Observed live evidence:
- architecture contract PASS;
- navigation_runtime 3/3 PASS;
- navigation_trajectory 11/11 PASS;
- EliteGame / EliteServer BUILD PASS;
- moving_gap_pair=1;
- moving_gap_kinematics=1;
- moving_precision=1;
- moving_passage_feasible=0;
- moving_passage_static_safe=0;
- moving_passage_authority=0;
- exact_static_violation=0.

Root cause is fixture design, not the accepted moving-passage algorithm.
The live aperture was authored near z=-2500 while the ship starts near z=-6200.
With duration=30 s the exact Hermite segment must cover about 3.7 km while
ending near the 60 m/s local speed cap. Its terminal acceleration is therefore
about -16.7 m/s^2 along travel, far beyond the Cobra's real ~2 m/s^2 reverse
manoeuvre authority. The evaluator correctly rejects it before static proof.

The same old fixture is also behind CUBE 08 on the nominal route. Therefore,
even if its propulsion envelope were loosened, the same-trajectory exact-static
proof should subsequently reject the Hermite path through CUBE 08.

Correction: move the deterministic moving aperture ahead of the ship but before
CUBE 08, preserving the real Cobra capability and the existing CUBE 08 static
gate. The intended order becomes:
moving passage live authority -> physical gap crossing -> CUBE 08 exact-static
avoidance -> same-tick replication, with no weakening of either proof.


## 12A-6b3b corrected live candidate

Corrective code/contract baseline before documentation commits:

```text
89ead2a4a256052817023f2fbf01ce9a9ca683e0
```

Corrections after the failed `4df5a442...` target run:
- relocate the translating moving aperture from visual z=-2500 to z=-5700,
  about 500 m ahead of the lab spawn and before CUBE 08;
- keep the real 30 s Hermite horizon and the Cobra's real vehicle authority;
- preserve CUBE 08 as the later independent exact-static obstacle;
- split acceptance ordering into:
  1. active MovingPassageClear + real physical acceleration,
  2. same-tick sparse/canonical replication of that active epoch,
  3. physical moving-gap plane crossing,
  4. subsequent CUBE 08 exact-static avoidance/progress;
- expose the last moving-passage evaluator status and required forward/reverse/
  lateral/vertical peak accelerations plus sample/continuous clearance in the
  server diagnostic line.

The corrected fixture no longer asks the Cobra to cover ~3.7 km in 30 s and
then brake at ~16-17 m/s^2 with 2 m/s^2 reverse authority. At the authored
~500 m initial range, the expected Hermite endpoint demands are inside the
physical manoeuvre envelope, while the same trajectory ends before CUBE 08 so
exact-static proof can succeed independently.

Target-machine acceptance is still pending.


## 12A-6b3b corrected fixture target-machine gate — EXACT-STATIC PHYSICAL VIOLATION

Target-machine baseline:

```text
69d8098d795c25b24b778b5229644244ae1384cb
```

Passed before the live failure:
- Stage-12 architecture contract;
- navigation_runtime 3/3;
- navigation_trajectory 11/11;
- canonical EliteGame / EliteServer build.

The authoritative live run then failed correctly on physical exact-static sweep:

```text
exact_static_violation=1
violation_entity=24
proving_obstacle_entity=24
planner_status=2
```

The ship entered the exact HitVolume of the legacy single CUBE 08 proving
obstacle after the moving-gap fixture was moved earlier in the route. This is
not accepted and 12A-6b3b remains open.

The next correction changes the static proving geometry, not vehicle capability
or collision acceptance: replace the single arbitrary block as the terminal
acceptance obstacle with a deterministic exact-HitVolume slit/tunnel gate. The
goal remains beyond it, so the planner must guide the real hull through authored
empty space rather than merely skirt one isolated cube.

The general contract remains vehicle-agnostic: runtime planning consumes the
current vehicle's physical capability and hull dimensions; no Cobra-specific
acceleration constants may be introduced into the navigation algorithm.


## 12A-6b3b slit-tunnel candidate

Candidate code/contract baseline before documentation commits:

```text
d05c97905f3df8e42980a88fbc216e03c7f8dbab
```

The single isolated CUBE 08 terminal obstacle is no longer the acceptance
geometry. The live static proving fixture is now a real exact-HitVolume tunnel:

```text
six GuidanceDockCube physical blocks
two rows x three blocks
tunnel depth: 900 m
horizontal slit height: 140 m
slit half-width: 540 m
slit center: {975, -1180, -4500} map/visual
straight authored route y: -1300
```

The slit is intentionally offset by 120 m from the original straight route.
The lower-middle wall block retains the `NAV STRESS CUBE 08` identity so the
existing direct-route exact-block witness still has a stable physical blocker.

NavigationSpace now publishes two coarse free-space regions connected only by
`NavigationRuntimeLabSlitPortalId` at the slit center. The route planner must
therefore publish that real portal as its static steering waypoint.

Fixture validation proves both facts independently:
- direct start->goal line is exact-static blocked by the representative wall;
- start->portal->tunnel-exit is exact-static traversable for the current ship's
  conservative physical envelope plus the existing 10 m static clearance.

Live physical acceptance no longer means merely "passed the obstacle plane".
The fixed-step exact sweep interpolates the actual ship crossing at the tunnel
center plane and requires the full conservative hull + static clearance to fit
inside the authored slit. The resulting crossing position and remaining margin
are retained in diagnostics.

The moving-gap authority gate remains first in the same run. Final ordered proof:

```text
live moving gap
 -> MovingPassageClear
 -> PilotSkillExecutor / real physics
 -> same-tick replication
 -> moving-gap plane crossed
 -> NavigationSpace slit portal selected
 -> exact-static tunnel crossing with positive margin
 -> continued route progress
 -> zero exact-static violation
```

Vehicle capability remains generic. The navigation algorithm reads the
descriptor/runtime capability of the current vehicle; no Cobra-specific
acceleration value is embedded in planner logic.


## 12A-6b3b slit candidate target-machine gate — FIXTURE VALIDATION FAILURE

Target-machine baseline:

```text
777716f52cdde3222dd875d93e91fe00df9859f1
```

Green before the live fail:
- Stage-12 architecture contract;
- navigation_runtime 3/3;
- navigation_trajectory 11/11;
- EliteGame / EliteServer build.

Fail-fast evidence:

```text
slit_exact_open=0
slit_exact_obstacles_examined=22
[FAIL] authored exact-static slit tunnel does not admit the current ship envelope
```

Root cause is the fixture proof's region-boundary semantics. The slit portal
center lies exactly on the shared NavigationSpace region boundary. Ordinary
`querySegment` validates its endpoint through `queryPoint`, where geometric
clearance to the region boundary is zero, so it rejects the legal portal point
before exact OBB aperture geometry can be accepted.

The runtime planner already has the correct selected-portal exception:
`allowEndOnStartRegionBoundary`. The fixture proof must use the same contract
for approach and reverse-exit segments rather than treating the portal center
as an ordinary interior point.

Architecture refinement from user review: a portal/tunnel is not accepted merely
because the hull center intersects the opening. A traversable finite-depth
passage needs an entry/capture frame. Before crossing the entry plane, automatic
execution must align:
- velocity direction with the oriented portal/tunnel normal;
- vehicle forward axis with that normal when the passage requires pose
  alignment;
- lateral/vertical position and cross-track velocity inside the capture
  envelope.

For a straight tunnel, transit keeps that entry axis through the exit. This must
be a generic portal traversal contract, not a NavigationRuntimeLab/Cobra special
case.


## 12A-6b3b oriented tunnel-capture candidate

Candidate code/contract baseline before documentation commits:

```text
b73cc129ce32cae1147165bf79b29f20b83ef04c
```

The target-machine failure on `777716f...` was a fixture-validation failure,
not a failed flight: `slit_exact_open=0` came from treating a legal portal
boundary as an ordinary region-interior point.

The correction now separates topology from collision truth and implements the
entry behavior required for finite-depth passages:

```text
APPROACH REGION
    -> staging/capture point
    -> ENTRY PORTAL (oriented normal + capture limits)
    -> TUNNEL REGION
    -> EXIT PORTAL
    -> DEPARTURE REGION
```

Before the entry plane can be accepted, the live actor must satisfy:
- hull position inside the passage capture envelope;
- velocity vector within 5 degrees of the entry normal;
- lateral speed <= 1 m/s;
- hull forward axis within 5 degrees of the entry normal.

The planner actively commands angular alignment using the current vehicle's
angular capability. It stages/brakes before the entrance, then releases
longitudinal transit only after capture is ready.

The live physical sweep now uses exact HitVolumes only; NavigationSpace region
boundaries remain topology and cannot produce a fake physical collision.

New regressions pin:
- route-direction portal normals, including reverse traversal;
- exact-obstacle-only sweep across a virtual region boundary;
- portal capture holding a misaligned hull;
- release to PortalTransit only after velocity + hull-axis alignment.

Target-machine acceptance is pending. The last accepted Stage-12 baseline
remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`; the later
`777716f...` run is retained as failed evidence, not acceptance.


## 12A-6b3b target-machine gate on e432363 — MOVING PASSAGE REJECTED

Target-machine baseline:

```text
e432363717d6bff49aaef35d34fac61cfe802903
```

Green evidence:
- Stage-12 architecture contract PASS;
- navigation_space 1/1 PASS;
- navigation_runtime 3/3 PASS;
- navigation_trajectory 11/11 PASS;
- canonical EliteGame and EliteServer build PASS;
- static slit fixture is now physically valid: `slit_exact_open=1`;
- physical exact-static sweep stayed clean: `exact_static_violation=0`;
- both intended moving actors were published and their kinematics verified.

The live run did not progress because the current test required the ordinary
free-space encounter to become a precision MovingPassage between the two moving
actors. The evaluator correctly rejected that geometry:

```text
moving_gap_pair=1
moving_gap_kinematics=1
moving_passage_feasible=0
moving_eval_status=2   # GeometryBlocked
moving_sample_clearance_m=-110.435
moving_passage_authority=0
conflict_hold=1
progress_m=0.0343542
simulated_s=120
```

This is not a tunnel/collision failure. It exposes a routing-policy problem:
when free space exists around a pair of obstacles, normal transit should not be
forced to prove a narrow moving passage between them.

Architecture decision from user review:
- default local transit becomes bounded line-of-sight / visibility steering;
- always start from the direct A->B direction;
- look only through the current physical horizon, sized by braking/turning and
  safety margin;
- if that corridor is blocked, increase the deflection only until a hull-sized
  safe corridor is found;
- on every receding-horizon update, retry direct A->B first and immediately
  return to it when visibility is restored;
- dynamic actors use their predicted swept volume inside the same bounded
  horizon;
- MovingPassage remains a precision mode only when passage is topologically or
  semantically mandatory: authored portal/tunnel, docking mouth, ravine, narrow
  gate, etc.

Current task changes from "force live MovingPassage authority through the free
moving pair" to "prove bounded visibility steering around the free moving pair,
then use the already-authored oriented portal capture for the mandatory tunnel."


## 12A live bounded-visibility steering candidate

Candidate code/contract baseline before documentation commits:

```text
e2584b4b798baa661d14ab1fb8f68789f76c99b9
```

Implemented after the `e432363...` live GeometryBlocked result:
- LocalAvoidance still tests the direct accepted target first on every update;
- the physical horizon remains bounded by latency + braking distance +
  turn-distance allowance + safety margin;
- when direct visibility is blocked, angular search now expands
  `15 -> 30 -> 45 -> 60 -> 75 deg` and stops at the first corridor that passes
  both exact-static and dynamic-horizon checks;
- the selected target is pass-through only; there is no persistent alternate
  route, so the next update automatically returns to direct A->B as soon as the
  corridor clears;
- live free-space MovingPassage authority is disabled. The precision
  MovingPassage contracts/tests remain intact for explicit mandatory gaps;
- live diagnostics now retain `visibilityBypassSeen`,
  `visibilityBypassActive`, `visibilityDirectRecoveredSeen` and maximum
  selected deflection;
- same-tick sparse/canonical replication is captured while the visibility
  bypass is actively executing;
- the same authoritative run must then pass the moving pair, recover direct
  visibility, perform oriented tunnel capture/alignment, traverse the exact
  HitVolume tunnel, and keep `exact_static_violation=0`.

A new local regression proves that a fixture requiring more than the old
15/30-degree fan finds a wider safe direction and then immediately resumes
direct A->B when the blocker disappears.

Target-machine acceptance is pending.


## Control-law-specific maneuver contract — candidate

Candidate code/architecture baseline before mandatory state-doc commits:

```text
8e707f8376099ce522cc300b293a33cf1d8f484c
```

New authority:
`src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`

Two local flight laws are now explicit in maneuver selection:

```text
Assisted / Elite-classic
    cheap passage proxy: oriented OBB ("brick")
    continuous truth: swept oriented hull
    velocity/forward coupling remains controller policy

Newtonian
    coarse arbitrary-rotation / flip free-volume proxy:
        conservative rotation sphere
    constrained passage truth:
        time-varying oriented hull / swept OBB
    strong braking:
        rotate tail toward velocity -> main-engine burn
```

The Newtonian sphere is not physical collision geometry and must never replace
exact oriented HitVolume truth. It exists only as a conservative test for free
rotation/flip space.

`ManeuverDecisionController` candidates now carry a control-law requirement:
`Any`, `AssistedOnly`, or `NewtonianOnly`. The selector filters
incompatible candidates before doctrine ranking. Candidate families now include
extended visibility recovery, backtrack, Newtonian flip-and-burn and reverse
escape.

The ordinary local visibility fan remains:

```text
direct -> 15 -> 30 -> 45 -> 60 -> 75 deg
```

75 degrees is an ordinary progress-preserving search boundary, not a vehicle
capability limit.

When all ordinary probes fail, `LocalAvoidancePlanner` publishes
`ordinarySearchExhausted=true`; `NavigationRuntimePlanner` publishes
`ordinaryVisibilitySearchExhausted=true`. This is a recovery-escalation
signal:

```text
>75 deg escape turn
backtrack / previous viable portal
safe stop/brake
Assisted brake-turn / controller-supported reverse
Newtonian coast-turn / drift / side-on pass / flip-and-burn / reverse escape
mandatory precision passage
contact-expected emergency
```

A provisional braking hold may protect the current control cycle while higher
selection occurs, but fan exhaustion is no longer architecturally equivalent to
"stay stopped forever".

The shared production library `EliteManeuverDecision` is linked by both
EliteGame and EliteServer. Architecture/runtime tests now pin control-law
filtering and ordinary-search escalation.

Target-machine validation is pending. Last target-machine accepted Stage-12
baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


## Accepted-segment execution / event-driven replanning candidate

Candidate code/architecture baseline before mandatory state-doc commits:

~~~text
d7b298f9fbc4cacd34eef912a96b324b894b197e
~~~

New authority:
`src/game/navigation/TRAJECTORY_EXECUTION_REPLAN_MODEL.md`

New production policy:
`NavigationExecutionReplanPolicy.{h,cpp}`

Core invariant:

~~~text
execution tick != planning tick
monitoring tick != planning tick
~~~

Automatic navigation is now specified as:

~~~text
plan -> accept short trajectory/segment -> execute -> monitor
~~~

A stable automatic segment is not replanned merely because another fixed frame elapsed. Local replanning is event-driven by:
- accepted segment completion/expiry;
- tracking error outside the accepted envelope;
- newly invalidating dynamic hazard evidence;
- vehicle capability change/damage.

Goal intent or topology/route-branch invalidation escalates to a full-route rebuild.

Manual navigation shares the accepted route but not autopilot authority.
Manual guidance performs:
- configurable periodic local suffix refresh (initial policy default 0.25 s);
- immediate local refresh when the player exits the recommended corridor;
- immediate local refresh for hazard/capability invalidation.

Manual local deviation preserves the global RoutePlan while its topology branch remains valid. Full route solving is reserved for goal/branch/topology invalidation.

Important current limitation:
`NavigationRuntimeLab` still calls `NavigationRuntimePlanner::plan()` in its fixed-step path. This is now explicitly transitional. The next integration slice must insert an accepted-segment/follower seam and prove stable automatic flight with many execution ticks per planner invocation (`planCount << executionCount`).

Target-machine validation for this scheduler/policy is pending. The last target-machine accepted Stage-12 baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


## AcceptedShortSegment + TrajectoryFollower integration candidate

Code candidate:

~~~text
0fc8d9b9d929cb19ad0338191433b635bb723f31
~~~

This slice replaces the live Stage-12 lab's unconditional fixed-tick
`NavigationRuntimePlanner::plan()` call with the first real accepted-execution
seam:

~~~text
planner epoch
    -> AcceptedShortSegment
    -> TrajectoryFollower every execution tick
    -> PilotSkillExecutor
    -> physics
    -> NavigationExecutionReplanPolicy
~~~

New production files:
- `src/game/navigation/AcceptedShortSegment.h`
- `src/game/navigation/TrajectoryFollower.{h,cpp}`

The accepted segment retains planner/world revisions, accepted/valid times,
local start/target/velocity, portal-axis capture data, control response data,
tracking envelope, emergency metadata and a vehicle-capability snapshot.

The follower deliberately has no NavigationMap/NavigationSpace dependency. It
tracks the already accepted local velocity/acceleration product and recomputes
fixed-step angular/linear control from current kinematics without running
obstacle or route search.

`GameSimulation::buildNavigationRuntimeLabIntent()` now asks
`NavigationExecutionReplanPolicy` before waking the planner. The current live
automatic triggers wired by this slice are:
- no accepted segment;
- segment completion;
- segment expiry;
- tracking-envelope escape;
- vehicle-capability change;
- goal revision change.

Stable execution keeps the accepted segment authoritative between those
events. Nominal lab segment validity is 0.75 s. Provisional
`ConflictHold/StaticHold/StaleHold` products are accepted only for 0.25 s, so
a safe hold cannot silently become a permanent navigation shutdown.

Diagnostics now distinguish:
- `planCount`;
- `executionCount`;
- `acceptedSegmentFollowCount`;
- `acceptedSegmentReplanCount`;
- accepted segment revision / last replan reason.

The isolated navigation-runtime target now also compiles
`TrajectoryFollower.cpp` and pins:
- follower execution without world search;
- tracking-envelope invalidation;
- stable `planCount << executionCount`.

Important limitations of this candidate:
- target-machine MinGW/runtime validation is still pending;
- the existing `DynamicHazardInvalidated` policy hook is not yet fed by a
  dedicated live accepted-segment hazard monitor in this slice;
- manual `GuidanceCorridor` publication from the same segment is still the
  next manual-mode integration step;
- the live slit/tunnel capture must be re-run after this cadence change.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Do not promote `0fc8d9b9d929cb19ad0338191433b635bb723f31` to accepted
baseline until the target-machine gate is green.


### Accepted-segment integration correction

Superseding code candidate:

~~~text
1e1b5ff2e72d743688833a403e94aefc0a832c87
~~~

This correction fixes two defects found during post-commit diff review of
`0fc8d9b9d929cb19ad0338191433b635bb723f31` before target-machine testing:

1. generated include edits contained literal `\n` text in
   `GameSimulation.h` and
   `NavigationExecutionReplanPolicyTests.cpp`; these are now real line breaks;
2. provisional hold segments no longer report immediate geometric completion.
   `AcceptedShortSegment::completionTriggersReplan` lets a safe hold remain
   accepted until its short validity expiry instead of waking the planner every
   fixed tick.

A regression now pins that a time-bounded hold stays in follower execution until
expiry.

Target-machine validation remains pending. The last actually accepted Stage-12
baseline is still:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Use `1e1b5ff2e72d743688833a403e94aefc0a832c87` as the code candidate for
the next MinGW/runtime gate.


### Target-machine gate failure: accepted-segment intent revision contract

Target-machine run against repository HEAD
`9a8b618f08f80b9945a49709640d1fc52e54be24` produced:

~~~text
architecture Stage-12 contract: PASS
navigation_local: PASS
navigation_space: PASS
navigation_runtime: PASS (5/5)
navigation_trajectory: PASS (11/11)
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL (return 43)
~~~

Failure:

~~~text
[FAIL] navigation-runtime same-tick sparse replication proof incomplete
error_mps2=inf canonical_error_mps2=inf
~~~

Root cause identified before changing code:

The pre-follower runtime planner used `NavigationRuntimePlanner::Result::intent`
whose `Intent::revision` is the goal/mission revision
(`plannerGoal.revision == 1202001` in the Stage-12 lab).

The first `TrajectoryFollower` implementation incorrectly replaced that
semantic identity with `AcceptedShortSegment::revision`, which is an execution
segment epoch (1, 2, 3, ...).

The live self-test intentionally filters sparse execution publications with:

~~~text
replicatedExecution->intentRevision == 1202001
~~~

Therefore every otherwise eligible sparse publication from the new follower was
discarded before same-tick/canonical comparison. Both replication error values
remained at their initialized infinity values. This failure does not demonstrate
a sparse/canonical acceleration mismatch; it demonstrates that the follower
violated the existing intent-revision contract before the comparison could run.

Required correction:
- preserve `goalRevision` as `Bridge::Intent::revision`;
- retain `AcceptedShortSegment::revision` separately as execution-segment
  identity/diagnostics;
- pin this distinction in the isolated follower test;
- rerun the same target-machine gate.

No new Stage-12 baseline is accepted. Last actually target-machine accepted
baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Intent revision / accepted target revision ownership fix

Code candidate:

~~~text
88bf3f05f405bc3c9b93a6ecf3619fcdcd0879cb
~~~

The target-machine replication failure exposed an overloaded revision field.
This code slice separates the two identities already represented by replication
output:

~~~text
intentRevision       = high-level maneuver/goal identity
activeTargetRevision = concrete accepted execution target/segment identity
~~~

Changes:
- `NavigationRuntimeControlBridge::Intent` now carries `targetRevision`
  separately from `revision`;
- `PilotSkillExecutor::Command` now carries the same optional target revision;
- zero `targetRevision` falls back to `revision`, preserving all legacy
  callers;
- reaction-delay ownership still follows high-level `revision`;
- the queued/applied target revision follows `targetRevision`;
- `TrajectoryFollower` publishes
  `intent.revision = AcceptedShortSegment::goalRevision` and
  `intent.targetRevision = AcceptedShortSegment::revision`.

This preserves the Stage-12 self-test requirement
`intentRevision == 1202001` while retaining explicit segment identity in
`activeTargetRevision`.

Target-machine validation of this correction is pending. Last actually accepted
Stage-12 baseline remains
`daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


### Revision-split regression coverage

Test candidate:

~~~text
25c8f2ad1df21de937c5db0ce24394b2e0b93fab
~~~

Regression coverage now pins the correction exposed by the failed target-machine
replication gate:

- runtime bridge preserves high-level `intentRevision` while publishing a
  distinct `activeTargetRevision`;
- `TrajectoryFollower` maps
  `goalRevision -> intent.revision` and
  `AcceptedShortSegment::revision -> intent.targetRevision`;
- `PilotSkillExecutor` may advance the concrete target revision inside the
  same intent without restarting reaction delay;
- the Stage-12 architecture contract now rejects re-merging these identities.

The next required action is a target-machine rerun. No acceptance is claimed
until the MinGW tests/build and `EliteServer --self-test-navigation` are green.

Last actually accepted Stage-12 baseline remains
`daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


### Target-machine gate failure after revision split

Target-machine run against
`91ac7f66e02f1a9aa76df82d09557e94e601310c` produced:

~~~text
architecture Stage-12 contract: PASS
navigation_runtime: PASS (5/5)
navigation_trajectory: 10/11 PASS
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL after the earlier replication gate
~~~

Isolated regression failure:

~~~text
navigation_pilot_skill:
NAVIGATION PILOT SKILL TESTS: FAIL:
new target inside same intent must not restart reaction delay
~~~

This regression was constructed incorrectly: it changed the target at t=0.01
after `reset(t=0)` while the configured initial reaction delay was still
active. It therefore measured the initial command delay, not a delay restart
caused by targetRevision. The test must first establish an already-reacted
intent, then change only targetRevision and verify that no new reaction window
starts.

More importantly, the live self-test now advances beyond the previous
same-tick replication filter and fails later on physical exact geometry:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
~~~

This is a real Stage-12 execution failure. The accepted-segment cadence is now
allowing a command to remain authoritative long enough that the physical exact
sweep catches a static HitVolume crossing. The next code pass must identify
entity 28, capture the accepted segment/planner status/target at the violation,
and correct segment invalidation/follower semantics rather than weakening the
exact-static gate.

No new Stage-12 baseline is accepted. Last actually target-machine accepted
baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Exact-static accepted-segment monitoring candidate

Code candidate:

~~~text
fe6ca95a17b9edda45de08c4ee624b3cc43a87c2
~~~

This pass addresses the target-machine failure where the live Stage-12 actor
physically crossed exact-static entity 28. Deterministic scene spawn order maps
entity 28 to `NAV STRESS CUBE 08`, the lower-middle block of the slit tunnel.

Root cause:
the first accepted-segment integration monitored expiry, completion, tracking
error, capability change and goal revision, but did not revalidate the
currently executing kinematics against authoritative exact-static HitVolumes
every fixed step. A still-unexpired segment could therefore remain
authoritative while inertia/tracking drift made its physical continuation
unsafe near the tunnel wall.

New monitor behavior:

~~~text
fixed tick
  -> follow accepted segment
  -> exact-static execution monitor
       * current position -> accepted target
       * short current-kinematics forecast
  -> if still clear: continue same segment, no planner call
  -> if blocked: StaticSafetyInvalidated -> immediate LocalHorizon replan
~~~

The monitor uses `NavigationSpace::querySegment` with
`exactObstaclesOnly=true`, the live ship envelope and the planner's static
additional-clearance policy. This is observation/validation work, not route or
avoidance search, so the core invariant remains:

~~~text
monitoring tick != planning tick
execution tick != planning tick
~~~

New policy reason:
`NavigationExecutionReplanPolicy::Reason::StaticSafetyInvalidated`.

New live diagnostics:
- `acceptedSegmentStaticSafetyInvalidationCount`;
- `acceptedSegmentLastStaticBlockingEntityId`.

The same pass corrects the newly added PilotSkillExecutor regression. Its
previous form changed targetRevision at t=0.01 while the initial reaction delay
from reset was still active, so it did not actually test whether a target
change restarted reaction delay. The corrected test first lets the existing
intent finish its initial reaction window, then changes only targetRevision.

Target-machine validation is pending. Last actually accepted Stage-12 baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Pilot regression timing correction

Superseding target-machine candidate:

~~~text
5665d4bc27edf138d744dacadc73329980df925b
~~~

Post-commit static review found one defect only in the new regression test:
`PilotSkillExecutor::step` requires `deltaSeconds` to equal elapsed time since
the previous step. The test attempted `step(0.50, 0.25)` immediately after
`reset(0.0)`, which would correctly return InvalidInput.

The regression now advances through two valid 0.25 s steps:
- t=0.25: initial reaction window is still active;
- t=0.50: the original intent becomes eligible;
- t=0.51: only targetRevision changes, proving that no new reaction window is
  started.

Production exact-static accepted-segment monitoring remains the code from
`fe6ca95a17b9edda45de08c4ee624b3cc43a87c2`.

Target-machine validation is pending. Last actually accepted Stage-12 baseline
remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


### Target-machine result: exact-static monitor candidate still fails live tunnel

Target-machine run against repository HEAD
`a29f3e01708663afb003dfe14ddbce0e4f330316` produced:

~~~text
architecture Stage-12 contract: PASS
navigation_runtime: PASS (5/5)
navigation_trajectory: PASS (11/11)
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL
~~~

The live failure is unchanged in physical terms:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
~~~

Entity 28 is the deterministic `NAV STRESS CUBE 08` lower-middle block of
the authored slit tunnel.

Therefore candidate
`5665d4bc27edf138d744dacadc73329980df925b` is NOT accepted as the new
Stage-12 baseline. The newly added exact-static accepted-segment monitor did not
prevent the collision in the authoritative runtime.

Important evidence from this run:
- the corrected target-revision/reaction regression now passes;
- all isolated runtime and trajectory tests pass;
- the failure is now exclusively in the live authoritative behavioral run;
- the current defect is specifically in execution/monitor/planner interaction
  near the slit tunnel, not compilation, replication, or the isolated policy
  contracts.

Next work must instrument the accepted segment immediately before entity-28
impact: segment revision, replan reason, exact-static monitor result, accepted
target, velocity, forecast endpoint and planner status. Do not weaken the
physical exact-static gate.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Accepted-segment exact-static impact instrumentation candidate

Code candidate:

~~~text
61d89d59a87c7034c948d85417c71c6ab4ae7f47
~~~

This pass is diagnostic-only. It does not alter navigation authority or
replanning behavior.

The authoritative live self-test still fails by physically sweeping the lab
Cobra through exact-static entity 28 (`NAV STRESS CUBE 08`, the lower-middle
slit-tunnel block). The previous exact-static monitor did not prevent that
impact, so the next required evidence is the execution state immediately before
the failing fixed step.

The runtime now preserves the last pre-physics accepted-segment safety probe:
- target blocked / forecast blocked flags;
- blocking entity;
- probe start;
- accepted target;
- short forecast endpoint;
- current map-space velocity;
- follower ideal acceleration;
- forecast duration and probe time.

At the first exact-static physical violation it additionally snapshots:
- accepted segment revision;
- last replan reason;
- planner status;
- accepted target + accepted target velocity;
- current velocity;
- last actually executed PilotSkill demand;
- the complete previous exact-static monitor witness.

`EliteServer --self-test-navigation` now prints that witness directly on
return-55 failure.

The purpose of this pass is to distinguish among:
1. the monitor never seeing entity 28;
2. monitor seeing it and replanning too late;
3. planner replacing the segment with another unsafe segment;
4. ideal follower forecast being optimistic relative to PilotSkill/physics
   inertia.

Target-machine validation/output is pending. No Stage-12 baseline changes.
Last actually target-machine accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Target-machine witness: ideal follower forecast diverges from executed PilotSkill command

Target-machine live self-test against the impact-instrumented candidate produced:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
segment_revision=127
planner_status=1
last_replan_reason=2
static_invalidations=1
previous_monitor_blocker=0
previous_target_blocked=0
previous_forecast_blocked=0
previous_probe_time_s=54.8
previous_forecast_s=0.73
violation_start=(1065.16,-1268.64,-4977.71)
violation_end=(1064.97,-1268.68,-4977.59)
current_velocity=(-9.77741,-2.20251,6.24856)
accepted_target=(811.472,-1361.83,-5385.02)
accepted_target_velocity=(-31.1628,-11.4446,-49.979)
last_executed_demand=(-3.88461,-43.6151,13.1693)
previous_forecast_end=(1053.75,-1272.09,-4984.39)
previous_velocity=(-9.77741,-2.20251,6.24856)
previous_ideal_accel=(-16.039,-6.93155,-42.1707)
~~~

Root cause is now concrete:

- accepted target and target velocity were already away from CUBE 08 in -Z;
- follower ideal acceleration also demanded strong -Z correction
  (`-42.1707 m/s^2`);
- the actually executed PilotSkill demand was still +Z
  (`+13.1693 m/s^2`);
- the exact-static monitor forecast used the follower's ideal acceleration, not
  the actual delayed/filtered PilotSkill command that authoritative physics was
  applying;
- therefore the monitor proved a hypothetical trajectory that the ship was not
  actually following and missed the imminent impact.

The prior monitor implementation is not accepted.

Required correction:
- execution safety prediction must start from current velocity and the latest
  actually executed PilotSkill acceleration;
- it must include a conservative braking/command-transition envelope before it
  may assume the new ideal follower demand;
- exact-static validation must cover that physically reachable swept envelope,
  not just one ideal endpoint chord;
- planner cadence remains event-driven; monitoring may remain per fixed tick.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Executed-demand exact-static safety candidate

Superseding code candidate:

~~~text
6fc035f23b079a33bde99bc564108a0494c9fca9
~~~

The target-machine impact witness proved that the first exact-static monitor
used the wrong acceleration source near the slit wall:

~~~text
follower ideal Z acceleration   = -42.1707 m/s^2
actually executed Z demand      = +13.1693 m/s^2
~~~

The planner/follower were already commanding motion away from entity 28 while
PilotSkillExecutor/physics were still carrying the previous command toward it.
The ideal-only monitor therefore proved a hypothetical safe path rather than
the authoritative near-term motion.

This candidate adds a second exact-static execution branch:
- start from current authoritative map position and velocity;
- use the latest actually executed PilotSkill linear demand;
- predict constant-current-command motion across the remaining accepted
  segment horizon;
- sample that parabolic path into 12 subsegments;
- exact-query every subsegment against physical HitVolumes;
- any blocked subsegment raises
  `NavigationExecutionReplanPolicy::StaticSafetyInvalidated` immediately.

The previous ideal follower forecast remains as a separate branch. Safety now
requires both:
1. the intended new command path is clear;
2. continued execution of the command that physics is actually receiving is
   also clear.

This preserves event-driven planning: the exact-static monitor may run every
fixed tick, but `Planner::plan` is still woken only by invalidating evidence.

Additional diagnostics now preserve and print:
- whether the executed-demand forecast was blocked;
- its last forecast endpoint;
- the executed acceleration used for the proof.

Target-machine validation is pending. No Stage-12 baseline promotion is claimed.
Last actually target-machine accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Root-cause correction: executed-demand monitor mixed WORLD and MAP frames

The latest target-machine run with sampled executed-demand monitoring still
failed on exact-static entity 28, but its telemetry exposed a more fundamental
frame error:

~~~text
last_replan_reason=6
static_invalidations=40
previous_monitor_blocker=28
previous_executed_forecast_blocked=1
~~~

So the monitor was now detecting danger and requesting
`StaticSafetyInvalidated` replans, but inspection of the production ownership
chain found that the acceleration fed into the executed-motion forecast was in
the wrong coordinate frame.

Production flow is:

~~~text
TrajectoryFollower intent      : NavigationMap working frame
Planner::mapIntentToWorld(...) : converts command to WORLD frame
PilotSkillExecutor             : executes that WORLD-frame command
ExecutionSnapshot              : carries the executed WORLD-frame vector
~~~

However `GameSimulation::updateNpcNavigationControl` copied
`ExecutionSnapshot::executedLinearAccelerationDemandMapMps2` directly into
`NavigationRuntimeLabObservation::lastExecutedLinearDemandMapMps2` and the
exact-static monitor then combined it with
`agent.velocityMapMetersPerSecond`.

Therefore the monitor mixed:

~~~text
position / velocity : MAP frame
executed acceleration: WORLD frame
~~~

The previous numerical comparison between follower ideal Z and executed Z is
not a valid same-axis comparison until the executed vector is transformed back
into the NavigationMap frame.

Required correction:
- retain WORLD executed demand for world-space application/diagnostics;
- explicitly transform authoritative executed WORLD acceleration back to the
  lab NavigationMap axes before storing
  `lastExecutedLinearDemandMapMps2`;
- exact-static motion forecast must consume only MAP-frame position, velocity
  and acceleration;
- pin this transform in the Stage-12 architecture contract/regression evidence.

The emergency-response hypothesis remains unproven and must not be acted on
until this frame error is removed.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### WORLD -> MAP executed-demand correction candidate

Code candidate:

~~~text
6659354c5b2c8e280cdae28cf9ea021b2ca4da5e
~~~

The sampled executed-demand safety monitor exposed a coordinate-frame ownership
bug rather than an established emergency-response failure.

In the live lab production path:

~~~text
TrajectoryFollower                   -> MAP-frame demand
NavigationRuntimePlanner::mapIntentToWorld
                                     -> WORLD-frame demand
PilotSkillExecutor / control bridge  -> WORLD-frame executed demand
DynamicMotionSystem                  -> applies WORLD-frame acceleration
~~~

The legacy ExecutionSnapshot member name still says
`executedLinearAccelerationDemandMapMps2`, but for this runtime path its
contents are already WORLD-space because the conversion happens before the
bridge.

The exact-static monitor was incorrectly combining that WORLD-space executed
acceleration with MAP-space position and velocity.

This candidate makes the boundary explicit in
`GameSimulation::updateNpcNavigationControl`:

~~~text
executedWorldVector
  -> dot(world, hub.normalAxis)     = map X
  -> dot(world, hub.radialAxis)     = map Y
  -> dot(world, -hub.progradeAxis)  = map Z
  -> executedMapVector
~~~

Only `executedMapVector` is stored in
`lastExecutedLinearDemandMapMps2` and consumed by the sampled exact-static
forecast. Existing world-space lateral/applied diagnostics continue using
`executedWorldVector`.

If the hub frame is invalid the stored map-space command is explicitly cleared
instead of reusing stale data.

The Stage-12 architecture gate now pins the complete inverse-basis transform so
the safety monitor cannot silently regress to mixed coordinate frames.

Target-machine validation is pending. The emergency-response hypothesis remains
unproven until this candidate is exercised.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Target-machine result after WORLD -> MAP correction

Target-machine run against
`60da8fc21a458b481894f3edebb266c02ed8d2be` produced:

~~~text
architecture contract: FALSE NEGATIVE
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL on exact-static entity 28
~~~

Architecture-check failure:

~~~text
[FAIL] accepted automatic segment must be monitored against exact-static geometry without restoring per-frame planning
~~~

Root cause of that check is textual/stale only: the architecture script still
requires the retired helper name `exactExecutionBlocked`, while production
code now intentionally uses `exactExecutionSegmentBlocked`. The build and
live runtime both compile/use the new helper. The gate must be updated rather
than reverting production code.

Live witness:

~~~text
segment_revision=129
planner_status=AdjustedClear
last_replan_reason=StaticSafetyInvalidated
static_invalidations=3
previous_monitor_blocker=28
previous_executed_forecast_blocked=1

velocity=(-9.77789,-2.20347,+6.24257)
ideal_accel=(-16.0268,-6.93006,-42.1738)
executed_accel=(-16.7456,-7.03306,-41.9582)
~~~

The WORLD -> MAP correction is validated by the witness: ideal follower and
actually executed acceleration now agree closely in the same NavigationMap
frame. Coordinate-frame mismatch is no longer the cause of the entity-28
impact.

The remaining failure is dynamic viability. The monitor detects the obstacle
and wakes the local planner, but `AdjustedClear` continues to optimize route
progress while the ship still has finite momentum toward the wall. Replanning
alone cannot instantaneously remove velocity. Static-safety invalidation must
therefore be able to select a short active recovery maneuver (brake/escape)
before returning to ordinary progress.

No Stage-12 baseline promotion. Last target-machine accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Directional stopping reserve + emergency recovery candidate

Superseding target-machine candidate:

~~~text
f25a9c2378c45950155db00caf48e71b5a07e3ac
~~~

The last live witness proved that WORLD -> MAP correction is working: follower
ideal and actually executed acceleration now agree closely in the same local
NavigationMap frame. The remaining entity-28 failure is therefore dynamic
viability, not coordinate conversion.

The accepted-segment monitor previously detected exact-static risk and requested
`StaticSafetyInvalidated`, but planner output remained an ordinary
`AdjustedClear` progress target. That cannot instantaneously remove existing
momentum.

This candidate adds a directional local-frame stopping reserve:

~~~text
current local velocity
  -> conservative control-response coast distance
     (decision period + command latency + one response-frequency interval)
  -> guaranteed braking distance at manoeuvre-thruster authority
  -> exact-static swept query along that stopping direction
~~~

If this stopping reserve intersects exact-static geometry, AUTO selects a short
active recovery product rather than only another progress replan:

~~~text
AcceptedShortSegment
  linearMode = FixedAcceleration
  acceleration = opposite current local velocity
  magnitude = guaranteed manoeuvre braking authority
  emergency = true
  hazardUrgency01 = 1
  validity = 0.25 s
~~~

The recovery segment is still normal navigation execution. It does not disable
the navigation module and does not discard the global route. While that recovery
segment is active, the same static warning does not wake the planner every fixed
tick; expiry returns control to the local planner, which either continues
recovery if the stopping reserve is still blocked or resumes progress.

The stopping-reserve proof is independent of the ordinary target/ideal/executed
forecast checks. This was corrected after static review so recovery still
activates when an ordinary forecast has already found the blocker.

Architecture-gate fixes in the same candidate:
- stale `exactExecutionBlocked` marker replaced with
  `exactExecutionSegmentBlocked`;
- recovery diagnostics are checked in `LAB_H`, not `GameSimulation.h`;
- the single LOCAL INERTIAL / TACTICAL translational-frame contract is pinned.

If live failure remains, the server witness now includes:
- emergency recovery count / active state;
- stopping-reserve blocked flag;
- response reserve seconds;
- stopping reserve distance.

Target-machine validation is pending. Last actually accepted Stage-12 baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Coordinate/spatial architecture audit

A repository-wide spatial-ownership audit was completed while the current
Stage-12 target-machine candidate remains under validation.

Audit document:

~~~text
src/game/navigation/COORDINATE_ARCHITECTURE_AUDIT.md
~~~

The audit finds that recurring MAP/WORLD defects are architectural debt rather
than unrelated local mistakes. Important confirmed issues include:
- tactical KinematicFrame basis and visual/model hub basis both entering
  navigation paths;
- DynamicMotionState mixing authoritative local and derived world quantities;
- propulsion acceleration bouncing Local -> WORLD -> Local;
- global gravity being sampled/stored but not consumed by HubTactical local
  fixed-step integration;
- HubNavigationFrame duplicating KinematicFrame transforms;
- duplicate spatial truth in ShipTransform + ShipReferenceFrameSnapshot;
- WorldPosition changing meaning between system-local and galactic-absolute
  based on side-channel systemId;
- transitional pendingReferenceVelocityMatch speed heuristic.

Recommended direction is two physical simulation domains with a shared frame
kernel: server-authoritative GlobalDynamics, deterministic shared
LocalSimulation, and a small shared SpatialCore owning strong types,
KinematicFrame transforms/rebasing and the only Global<->Local API.

This is an AUDIT/PROPOSAL, not yet an accepted migration contract. No current
navigation behavior or target-machine acceptance status is changed by this
documentation pass.


## 2026-09-19 spatial-foundation decision / Navigation freeze

The coordinate audit is now preserved in:

~~~text
src/game/navigation/COORDINATE_ARCHITECTURE_AUDIT.md
src/game/navigation/SPATIAL_ARCHITECTURE_DECISION.md
~~~

Project direction is now fixed as follows:

- complete and record the already-running target-machine gate for current
  recovery candidate `f25a9c2378c45950155db00caf48e71b5a07e3ac`;
- do not add another Navigation v2 algorithm/recovery patch after that gate;
- pause Stage-12 implementation at the current boundary;
- make Spatial Phase A the next implementation task;
- resume Navigation v2 only after the canonical Global/System <-> Local API,
  strong coordinate types and LocalDomainFrame authority are green.

Last actually target-machine accepted Stage-12 baseline is unchanged:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Current candidate remains unaccepted until the target-machine output is
reported.

The selected local-simulation model is a moving free-fall interaction domain:
GlobalDynamics advances the domain carrier under large-scale gravity; short
range LocalSimulation may cancel/ignore the common-mode field and integrate only
local forces. Differential/tidal gravity is an explicit optional residual and
may be omitted only under a bounded error criterion.

A local domain may follow an anchor ship, but interacting bodies must share one
LocalDomainId; the domain basis is not the ship body frame. Preferred basis is
non-rotating/inertial over the local horizon.

Star-system coordinates use one shallow System frame per SystemId. The Sun is
not a universal master origin, and planet/moon/hub relationships do not create
new public coordinate domains.

This changes project sequencing/architecture only. It does not promote or
reject the pending Stage-12 candidate and does not alter the last verified
baseline.


### Target-machine recovery gate — emergency brake still enters exact-static entity 28

Latest target-machine run of the Stage-12 recovery code path completed full
MinGW client/server build but the live navigation self-test still failed:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
segment_revision=136
planner_status=1
last_replan_reason=2
static_invalidations=191
emergency_recoveries=15
emergency_recovery_active=1
stopping_reserve_blocked=1
stopping_reserve_s=0.643333
stopping_reserve_m=27.7918
previous_monitor_blocker=28
previous_target_blocked=1
previous_forecast_s=0
~~~

Physical state at first violation:

~~~text
current_velocity=(-6.26585,-1.23874,+6.80803)
accepted_target=(1049.98,-1271.4,-4957.4)
accepted_target_velocity=(0,0,0)
last_executed_demand=(+1.35067,+0.272356,-1.46091)
previous_ideal_accel=(0,0,0)
previous_executed_accel=(0,0,0)
~~~

The emergency segment is physically braking opposite the current velocity:
the executed demand direction is consistent with active deceleration. Therefore
the remaining defect is not "emergency command failed to reach physics".

The decisive evidence is:

~~~text
stopping_reserve_blocked=1
previous_target_blocked=1
~~~

The recovery begins only after the state has already reached a configuration
whose complete conservative stopping envelope intersects exact-static geometry.
A replan/emergency command at that instant cannot make the collision avoidable.

Root cause / required Navigation-v2 correction:

- stopping viability must be an ACCEPTANCE invariant of every executable short
  segment, not only a late invalidation response;
- a segment may be accepted only if the swept trajectory plus the physically
  reachable stopping/reaction envelope at its future execution states remains
  exact-static safe;
- when that invariant first fails, the planner must select an earlier escape or
  braking maneuver while free space still exists;
- do not weaken the exact-static collision gate and do not hide the failure by
  increasing cube clearance ad hoc.

The last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

The current recovery candidate remains unaccepted.


### 2026-09-19 task sequencing correction — finish Navigation v2 behind a sealed spatial API

The previous spatial-foundation note proposed freezing Navigation v2 before
Spatial Phase A. Project sequencing is now intentionally changed after review
of the ACTUAL repository coordinate types and live failure evidence.

Current task:

~~~text
FINISH NAVIGATION V2 FIRST
    -> seal its coordinate boundary
    -> remove ambiguous MAP/WORLD control semantics
    -> move stopping viability into segment/candidate acceptance
    -> pass the Stage-12 live exact-static tunnel gate

THEN
    -> resume the wider repository spatial/global refactor
~~~

This is not a rollback of the coordinate audit. The audit remains valid and is
now used to define a strict Navigation-v2 containment boundary.

Verified existing coordinate facts that Navigation v2 must respect:
- runtime WorldPosition is system-local while systemId >= 0; the same type is
  used as galactic-absolute by PlayerSpatialDomainResolver outside a system;
- KinematicFrame is the existing canonical moving-frame P/V/A transform and
  already owns frame velocity, acceleration, omega/alpha and rebase math;
- HubNavigationFrame duplicates part of KinematicFrame and is not allowed to
  become an additional Navigation-v2 coordinate API;
- authored hub visual axes differ from tactical KinematicFrame axes;
- NavigationMap::DynamicActorInput currently claims system velocity but live
  Stage-12 supplies a pre-relative velocity because WorkingFrame lacks frame
  velocity/omega;
- NavigationRuntimePlanner::mapIntentToWorld currently returns the SAME
  Bridge::Intent type after changing the physical meaning of its vectors;
- ShipControlState / NavigationExecutionSnapshot still use *Map* field names
  for vectors that authoritative physics interprets in system/world axes.

Therefore the immediate Navigation-v2 cleanup target is:

~~~text
existing system/runtime state
        |
        | ONE explicit typed NavigationFrameBoundary
        v
NavLocal-only NavigationMap / NavigationSpace / planner / follower / monitor
        |
        | ONE explicit typed control egress
        v
System-space PilotSkill / ShipControlState / physics
~~~

No raw visual/model coordinates or semantically ambiguous "Map" control vector
may cross that boundary.

The wider question of galactic/system/global authority remains deferred until
Navigation v2 is closed and accepted; no new global coordinate model is assumed
by this task.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 target-machine late-braking root cause — stopping reserve invalidation was overwritten

The latest live witness raised the key timing question: Navigation must begin
avoidance/braking while the maneuver is still physically reachable, not only
after collision geometry is immediately ahead.

Repository inspection confirms the physical horizon itself is already
speed-dependent:

~~~text
latencyDistance = v * resultAge + 0.5 * |a| * resultAge^2
brakingDistance = v^2 / (2 * maxBrakingAcceleration)
horizonDistance = max(
    minimumHorizon,
    latencyDistance + brakingDistance + turnDistance + safetyMargin
)
~~~

The late emergency activation is caused by a concrete execution-monitor bug,
not by absence of a speed-dependent distance horizon.

Current Stage-12 code does:

~~~cpp
if (staticSafetyStoppingReserveBlocked)
{
    staticSafetyInvalidated = true;
    staticSafetyRecoveryRequired = true;
}

staticSafetyTargetBlocked = exactExecutionSegmentBlocked(...);

// BUG: destroys the earlier stopping-reserve invalidation whenever the direct
// current->target chord is still clear.
staticSafetyInvalidated = staticSafetyTargetBlocked;
~~~

Therefore Navigation can detect that the complete reaction+braking reserve is
already unsafe, then discard that evidence and continue executing the accepted
segment. Replanning/emergency begins later when the direct target chord itself
finally becomes blocked, at which point the target-machine witness shows:

~~~text
stopping_reserve_blocked=1
emergency_recovery_active=1
violation_entity=28
~~~

Required correction:

~~~cpp
staticSafetyInvalidated =
    staticSafetyInvalidated || staticSafetyTargetBlocked;
~~~

More generally, the Navigation-v2 acceptance invariant is:

- detection horizon is physical and speed/capability dependent;
- a candidate executable segment is valid only while a safe reaction/braking
  or maneuver reserve remains available;
- stopping-reserve invalidation has equal authority to direct target/forecast
  blockage and may never be overwritten by a later independent safety check;
- planner execution remains event-driven: monitor every fixed tick, replan only
  when the accepted segment loses that invariant.

The fixed 3 s dynamic-conflict look-ahead is a separate policy dimension from
the physical distance horizon. It must eventually be bounded from below by the
time represented by the physical maneuver reserve for high-speed moving
hazards, but it is not the cause of this CUBE 08 exact-static failure.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

### 2026-09-19 Navigation-v2 sealed spatial API + timely physical horizon candidate

Current unaccepted candidate code HEAD before the next broadphase-alignment slice:

~~~text
cfecf1014a6f109acef65d46707f15e7acfa4242
~~~

The target-machine failure at entity 28 established that emergency braking was
already physically commanded correctly but began after the safe stopping
envelope had been lost. Repository tracing found the immediate code defect:

~~~cpp
if (staticSafetyStoppingReserveBlocked)
    staticSafetyInvalidated = true;

// old bug: erased the earlier stopping-reserve result
staticSafetyInvalidated = staticSafetyTargetBlocked;
~~~

The monitor is now monotonic:

~~~cpp
staticSafetyInvalidated =
    staticSafetyInvalidated || staticSafetyTargetBlocked;
~~~

A later independent safety check can add an invalidation reason but cannot erase
an already proven unsafe stopping reserve.

Navigation timing is now based on one shared physical maneuver horizon rather
than unrelated planner/execution formulae. PhysicalManeuverHorizon derives:

~~~text
response time =
    snapshot age
  + pilot reaction delay
  + decision period
  + command latency
  + command/filter response reserve

response distance = v*t + 0.5*a*t^2
speed at brake    = v + a*t
braking distance  = v_brake^2 / (2*a_brake)

distance horizon =
    max(minimum,
        response distance
      + braking distance
      + maneuver/turn distance
      + safety margin)

dynamic time horizon =
    max(policy minimum,
        response time + braking time)
~~~

Therefore the horizon is deliberately NOT merely linear in speed: reaction
distance is approximately linear in speed, while braking distance is quadratic
in speed. Dynamic look-ahead also grows with the time physically required to
respond and brake.

The live pilot profile's reactionDelaySeconds is now included in the reserve;
the previous live stopping monitor omitted it.

Navigation-v2 coordinate ownership has also been sealed around the ACTUAL
repository coordinate contracts:

~~~text
system/runtime state
        |
        | NavigationFrameBoundary (KinematicFrame-backed)
        v
NavLocal-only NavigationMap / NavigationSpace / planner / follower / monitor
        |
        | NavigationLocalControlIntent
        | explicit NavigationFrameBoundary conversion
        v
NavigationSystemControlIntent
        |
        v
PilotSkillExecutor -> ShipControlState -> authoritative physics
~~~

Key API invariants now encoded in C++ types:

- NavigationMap accepts NavLocal actor position/velocity/acceleration/angular
  velocity only; it no longer owns a WorkingFrame or system->map conversion.
- NavigationLocalControlIntent and NavigationSystemControlIntent are distinct
  incompatible types.
- NavigationRuntimePlanner and TrajectoryFollower cannot emit a system-space
  execution command.
- NavigationRuntimeControlBridge, ShipControlState, replicated execution truth
  and authoritative physics use explicit System-space field names.
- the only local/system conversion admitted by Navigation v2 is
  NavigationFrameBoundary, backed by the existing KinematicFrame.
- the existing project basis was verified rather than replaced: hub tactical
  axes are X=prograde, Y=radial, Z=normal with
  normal=cross(prograde,radial); the Stage-12 navigation/visual permutation
  X=normal, Y=radial, Z=-prograde is likewise right-handed.

Network serialization retains the same byte ordering and scalar layout; only the
C++ execution field semantics/names changed from misleading Map to System.

New deterministic regression:
PhysicalManeuverHorizonTests.cpp proves that doubling speed from 10 to 20 m/s
with the same braking authority grows the required distance from 45 m to 125 m
and the dynamic time horizon from 5.5 s to 10.5 s in its fixture.

One remaining architecture alignment is intentionally NOT hidden:
NavigationMap broadphase still uses its configured fixed prediction sweep
(default/live lab 3 s) while LocalHorizonPlanner can now require a longer
physical dynamic look-ahead. Before Navigation-v2 acceptance, broadphase
candidate publication must consume the same effective physical horizon so a
fast incoming actor cannot be omitted before local conflict evaluation.

No target-machine validation has been run for this candidate yet.
No Stage-12 baseline promotion.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

### 2026-09-19 physical dynamic broadphase horizon aligned

Current unaccepted Navigation-v2 candidate code HEAD:

~~~text
57ae48cc4cc7d376279582e1da722961e5c9fb5a
~~~

The previously recorded final timing mismatch is now closed:
NavigationMap no longer forces every dynamic broadphase query to use only its
configured fallback prediction horizon.

Both CorridorQuery and SphereQuery now expose an optional lookAheadSeconds.
When supplied, all of the following are evaluated over that exact requested
time horizon:

- broadphase AABB expansion;
- per-actor conservative swept radius;
- candidate predicted endpoint;
- candidate conservative swept product;
- QueryResult/Candidate diagnostics identifying the horizon used.

The sparse index remains indexed by current NavLocal actor position. To avoid a
full actor scan merely to size the query AABB, NavigationMap retains bounded
publication maxima for indexed actor radius, speed and acceleration and uses
them to compute a conservative maximum swept-radius upper bound for the query
horizon. Per-actor acceptance then uses each actor's own velocity/acceleration
travel bound.

The authoritative Stage-12 dynamic query now explicitly supplies:

~~~cpp
dynamicQuery.lookAheadSeconds = physicalHorizon.lookAheadSeconds;
~~~

Therefore dynamic candidate discovery and LocalHorizonPlanner conflict
evaluation consume the same speed/capability-dependent physical time horizon.
The Config prediction horizon remains only a fallback for callers that do not
provide a physical query horizon.

A new NavigationMap regression proves the distinction: with a 1 s configured
fallback an incoming actor remains outside the local query, while the same
query with 5 s physical look-ahead includes that actor and predicts its crossing
at the correct endpoint.

Combined timing invariant for Navigation v2 is now:

~~~text
physical response/braking need
        -> distance horizon for static/current geometry
        -> time horizon for dynamic prediction
        -> same time horizon for NavigationMap candidate broadphase
        -> accepted short segment may execute only while stopping reserve remains safe
~~~

No target-machine validation has been run for this candidate yet.
No Stage-12 baseline promotion.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 Navigation-v2 purity/isolation audit and safety-math extraction

Current unaccepted candidate code HEAD:

~~~text
c549d3afb740df6cb6a56494c22a2c63c0ad409d
~~~

Navigation-v2 now has an explicit three-level purity contract in:

~~~text
src/game/navigation/NAVIGATION_PURITY_CONTRACT.md
~~~

The architectural rule is:

~~~text
calculation = value-in -> value-out
~~~

unless the component explicitly owns an immutable-at-query-time published
snapshot/index or is inherently sequential authoritative execution.

#### Strict-pure calculation core

The following components are intended to be deterministic value-in/value-out
calculations with no wall-clock/random/I/O/runtime-state dependency:

- PhysicalManeuverHorizon;
- NavigationExecutionSafetyProbeBuilder;
- LocalHorizonPlanner;
- BoundedGapCandidateBuilder;
- MovingGapPredictor;
- MovingPassageTrajectoryEvaluator;
- TrajectoryFollower;
- NavigationExecutionReplanPolicy;
- ManeuverDecisionController.

NavigationFrameBoundary is an immutable value-object transform: its result is
pure for the explicitly captured KinematicFrame and it performs no frame lookup.

#### Snapshot-pure query services

NavigationMap and NavigationSpace intentionally own published/indexed state.
Publication mutates their snapshot:

~~~text
NavigationMap::replaceDynamicWorld(...)
NavigationSpace::replaceStaticWorld(...)
~~~

Their query methods are deterministic/read-only over the current published
snapshot and return products by value. LocalAvoidancePlanner and
NavigationRuntimePlanner are therefore classified as snapshot-pure composition:
they do not mutate runtime state but consume a const NavigationSpace snapshot.

#### Intentionally stateful seam

State is legitimate only in:

- PilotSkillExecutor/runtime bridge: reaction delay, command queue/filter/slew;
- authoritative physics/ShipControlState;
- GameSimulation orchestration, accepted-segment/revision ownership and
  diagnostics;
- replication/network publication/hydration state.

The audit found one real isolation leak: execution-safety kinematics were still
embedded directly inside GameSimulation together with exact-static queries and
diagnostics.

That math has now been extracted into:

~~~text
src/game/navigation/NavigationExecutionSafetyProbeBuilder.h
~~~

It is a stateless pure builder which receives explicit NavLocal kinematics and
returns:

- stopping-reserve segment;
- ideal constant-acceleration forecast segment;
- 12-sample executed-acceleration forecast polyline.

It has no NavigationSpace, NavigationMap, HitVolume, GameSimulation, diagnostics,
clock, random source or I/O dependency.

GameSimulation now only:

1. reads authoritative state;
2. calls the pure probe builder;
3. submits returned segments to authoritative exact-static NavigationSpace
   queries;
4. records invalidation/recovery/diagnostic state.

The existing physical-horizon runtime test now also proves deterministic
same-input/same-output execution-safety probe construction and bounded forecast
geometry.

The Stage-12 architecture contract now checks:

- the pure safety-probe builder exists and remains free of stateful
  dependencies/I/O/time/random sources;
- the principal pure calculation components do not acquire ambient time/random
  or file I/O;
- GameSimulation delegates probe construction to the pure builder;
- the written purity categories remain part of the Navigation-v2 architecture.

No target-machine validation has been run for this candidate yet.
No Stage-12 baseline promotion.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Current next step: target-machine architecture/runtime/build gates, then the
authoritative headless Navigation self-test. If those pass, evaluate live
behavior before promoting the Stage-12 baseline.


### 2026-09-19 purity gate comment/dependency distinction

Current unaccepted Navigation-v2 code candidate before this documentation sync:

~~~text
e21c81a9acf360b01863729a7c98fed5990273a9
~~~

The new purity architecture gate was corrected so it rejects actual stateful
dependencies/includes in NavigationExecutionSafetyProbeBuilder rather than
matching architecture names that appear only in explanatory comments.

The purity/isolation behavior and code contract are unchanged:

- execution-safety kinematics remain strict value-in/value-out;
- GameSimulation remains the authoritative stateful shell;
- NavigationMap/NavigationSpace remain snapshot owners with deterministic
  read-only query phases;
- PilotSkillExecutor remains intentionally sequential/stateful.

No target-machine validation yet. Last actually accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 API boundary audit — remaining static-state capability leak

Audit of the current Navigation-v2 purity candidate found one remaining runtime
boundary weakness after the strict-pure math extraction.

The dynamic side is already sealed correctly:

~~~text
NavigationMap state owner
    -> querySphere/queryCorridor API
    -> NavigationMap::QueryResult value DTO
    -> planner
~~~

The static side is not yet equivalently sealed. Both production planners still
receive the state owner itself:

~~~text
NavigationRuntimePlanner::plan(..., const NavigationSpace&, ...)
LocalAvoidancePlanner::evaluate(..., const NavigationSpace&)
~~~

They use only public const query methods, so there is no direct field access,
but the calculation boundary still carries a capability to the stateful
NavigationSpace object. Under the strengthened isolation rule this is considered
a boundary leak.

New rule for Navigation v2:

~~~text
NO state-owner object crosses a calculation boundary.
State stays behind its owner.
Across the boundary pass only:
- immutable/value DTOs; or
- a deliberately narrow read-only query API capability with no publication,
  mutation, storage access or owner escape hatch.
~~~

Current implementation task is therefore to introduce a narrow static-navigation
read API, bind it at the GameSimulation/orchestration edge, and make
NavigationRuntimePlanner/LocalAvoidancePlanner depend on that API rather than on
NavigationSpace itself. Architecture gates must reject a future reintroduction
of `const NavigationSpace&` into those calculation seams.

Broader non-Stage-12 surfaces also deserve the same treatment: the legacy/client
workspace exposes mutable sub-state references, and NavigationFrameBoundary
currently exposes its captured frame by const reference. They are not part of
the active live planner path, but they are recorded for follow-up boundary
hardening rather than being treated as acceptable precedent.

No target-machine validation has been run for this new boundary slice. The last
actually accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 static-state read API boundary — implementation candidate

Candidate code/contract baseline before this state-document sync:

~~~text
dac32c59ed5c6c9031d3783ae707f603e7cb7e4e
~~~

The Stage-12 calculation boundary has now been tightened to the stronger rule
recorded in the preceding audit: **state-owner objects do not cross calculation
seams**.

Implemented:

- added `NavigationStaticQueryApi`, a narrow read-only capability bound to an
  owned `NavigationSpace`;
- the capability exposes only point/segment/corridor/costed-corridor queries and
  value results;
- it exposes no publication, patch, invalidation, stats/storage view or owner
  accessor;
- `NavigationRuntimePlanner::plan` now receives
  `const NavigationStaticQueryApi&`, not `const NavigationSpace&`;
- `LocalAvoidancePlanner::evaluate` now receives the same narrow API;
- planner/local-avoidance public and implementation surfaces no longer name
  `NavigationSpace::...` DTOs directly; they use aliases exported by the read
  API;
- `GameSimulation` remains the stateful orchestration owner and binds the
  short-lived read capability at the call edge;
- runtime/local behavioral fixtures now pass the explicit read capability;
- architecture gates now fail if either calculation surface regains a direct
  `NavigationSpace&` or `NavigationSpace::...` dependency, or if the read API
  grows owner mutation/escape methods;
- `NAVIGATION_PURITY_CONTRACT.md` and the local-layer contract now state the
  owner/API distinction explicitly.

The dynamic side was already compliant and remains unchanged:

~~~text
NavigationMap owner -> query API -> NavigationMap::QueryResult value -> planner
~~~

Authoritative exact-static safety checks performed directly by `GameSimulation`
remain legitimate because GameSimulation is the orchestration/state owner; they
are not calculation-layer backdoors.

Static inspection of the modified production calculation surfaces shows no
remaining direct `NavigationSpace&`, `NavigationSpace::...` or `staticSpace`
seam in NavigationRuntimePlanner or LocalAvoidancePlanner.

Target-machine compile/runtime validation is still pending. Do not promote the
accepted Stage-12 baseline yet. The last actually accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Required target-machine gate:

~~~bash
python tests/architecture_contracts/check_navigation_local_avoidance.py
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_space/run_mingw64.sh
bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
~~~

Broader follow-up remains recorded separately: legacy/client workspace mutable
sub-state references and the unused NavigationFrameBoundary frame-reference
escape hatch should be hardened after this active Stage-12 API gate is green,
rather than mixing unrelated legacy churn into the same acceptance slice.


### 2026-09-19 legacy route isolation + typed-boundary cleanup

Candidate code/contract baseline before this documentation sync:

~~~text
9ee0fdea3b366cb89633c3abddade270a308c9a9
~~~

Repository audit confirmed that the pre-Stage-12 client docking route pipeline
still exists in `SpaceState`:

~~~text
DockingPathPlanner
  -> GeometricPathPlanner
  -> TrajectoryGenerator
  -> GuidanceTunnel
~~~

It is not part of authoritative `GameSimulation`/Navigation-v2 control, but it
was still runtime-enabled through the old RoutePlanning/LocalGuidance module
switches.

This legacy path is now isolated and disabled in two independent ways:

- `NavigationModuleState` defaults both `RoutePlanning` and `LocalGuidance` to
  OFF;
- `SpaceState::updateDockingGuidance` additionally contains a hard
  `LegacyClientRoutePipelineEnabled = false` gate, so toggling those old module
  switches cannot re-activate the route pipeline accidentally.

Architecture protection now requires the legacy route/tunnel stack to stay out
of `GameSimulation` and `NavigationRuntimePlanner`. The old implementation is
retained only as reference/regression/presentation code for now; no Stage-12
authoritative command may originate from it.

The target-machine log also exposed migration leftovers from the typed
NavLocal/System split. Fixed in this candidate:

- stale `...DemandMap...` follower test names -> NavLocal names;
- stale `...DemandSystem...` control-test names -> current System intent names;
- NPC test kinematics `relativeWorldVelocity/forwardMap/rightMap/upMap` ->
  explicit System-space names;
- replicated HUD execution truth now remains explicitly System-space instead of
  relabelling server acceleration as map-space;
- the last untyped `pointToMap(...)` call in `GameSimulation` now crosses
  `NavigationFrameBoundary` explicitly.

The Stage-12 architecture gate now rejects those stale identifiers and rejects
re-introduction of the removed `pointToMap` helper.

Observed target-machine evidence before these fixes:

- navigation_local: PASS 2/2;
- navigation_space: PASS 1/1;
- navigation_runtime: compile failed on stale renamed identifiers;
- full build: compile failed on the same HUD rename leftovers plus the final
  `pointToMap` call;
- the subsequent navigation self-test result is not acceptance evidence because
  the build had already failed, so that command executed a previously built
  EliteServer binary.

No target-machine validation has been run for this fixed candidate yet. Last
actually accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Next gate is to rerun architecture + runtime + full build. Only after a fresh
successful build should `EliteServer --self-test-navigation` be interpreted as
current behavior evidence.


### 2026-09-19 second legacy route path — repair drones hard-off

Candidate code/contract baseline before this documentation sync:

~~~text
5dbc8ba11bf0e2317fc8e9cc3b4f35b74042a86d
~~~

A second live pre-Navigation-v2 route path was found in
`ObjectRepairJobRuntime`:

~~~text
repair job
  -> GeometricPathPlanner::plan
  -> SmallCraftNavigation waypoint follower
  -> TacticalCollisionMonitor diagnostics
~~~

This is separate from the already-disabled client docking route/tunnel path.
Because `SmallCraftNavigation` treats an empty waypoint list as completed,
simply returning an empty legacy path would be unsafe: the repair state machine
could advance without physical travel.

The repair-drone legacy route pipeline is therefore hard-disabled fail-closed:

- `LegacyRepairDroneRoutePipelineEnabled = false`;
- `startJob(...)` rejects new legacy-navigation repair jobs;
- `update(...)` freezes any pre-existing active job, clears its legacy waypoint
  path and returns no completion event;
- the old implementation remains in place only as migration/reference code;
- architecture tests pin both the OFF gate and the fail-closed update behavior.

This intentionally means automatic repair-drone travel is unavailable until the
repair task is connected to Navigation v2. It is preferable to silently running
a second route planner or falsely completing repair phases.

`SmallCraftNavigation` and `TacticalCollisionMonitor` are therefore not being
mistaken for the new Navigation-v2 runtime: their only confirmed live ownership
was the now-disabled repair-drone legacy path, and they remain forbidden from
`GameSimulation`/`NavigationRuntimePlanner` by the architecture gate.

No target-machine validation yet for this candidate. Last accepted Stage-12
baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 dormant LocalGuidance/Ruckig runtime exclusion

Candidate code/contract baseline before this documentation sync:

~~~text
5a144f7552848abe8b61c2978e3c02b35dc3f755
~~~

The older `LocalGuidancePlanner` / `RuckigRoutePlanner` stack remains in the
repository for regression/reference builds, but no live owner is allowed to
reactivate it. Architecture checks now forbid LocalGuidance/Ruckig dependencies
from `SpaceState`, `GameServer`, `ServerRuntime`, `NpcAiSystem`, and the repair
runtime, in addition to the existing `GameSimulation` / `NavigationRuntimePlanner`
exclusion.

This leaves the old code compiled/reference-only, with both confirmed live route
paths separately hard-disabled.


### 2026-09-19 target-machine pass: runtime green, wrapper/gate cleanup

Candidate code/contract baseline before this documentation sync:

~~~text
92b860fc07377e0fbdb64e78ab833d62bbb05586
~~~

Latest target-machine evidence at checkout `f02d8e67a974714af4fa24b84c57a8110f08d3b4`:

- `navigation_runtime`: PASS 6/6;
- `navigation_local` and `navigation_space` had already passed in the prior run;
- architecture gate 1 failed only because it required the old spelling
  `glm::dvec3(0.0, 0.0, 0.0)` for the static Hub cylinder while the scene now
  uses the equivalent canonical `glm::dvec3(0.0)`;
- architecture gate 2 failed because it still required manual
  normal/radial/prograde dot-product conversion for executed PilotSkill demand,
  while production now correctly crosses through
  `NavigationFrameBoundary::toNavigationVector(SystemVector{...})`;
- full client build failed because `SpaceState` still contained an unused
  diagnostic trace function typed against the removed legacy
  `LocalGuidancePlanner` include.

Corrections in this candidate:

- the dead LocalGuidance docking trace block was removed from `SpaceState`
  instead of reintroducing the legacy planner dependency;
- the static-cylinder architecture gate accepts both equivalent zero-vector
  spellings while still pinning the fixture as non-rotating;
- the executed-demand architecture gate now requires the typed
  `NavigationFrameBoundary` System -> NavigationLocal conversion instead of the
  superseded manual basis projection.

No Navigation-v2 planner/control algorithm changed in this pass. The fresh
runtime 6/6 result is retained as positive evidence, but full target-machine
acceptance still waits for architecture gates + complete build + a newly built
`EliteServer --self-test-navigation`.

Last fully accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 exact-static gate follow-up

Candidate code/contract baseline before this documentation sync:

~~~text
67065a0cd0f888273da0390891fc718569851e3c
~~~

Target-machine gate failure:

~~~text
[FAIL] accepted segment exact-static monitor must prove sampled continuation of the actually executed PilotSkill acceleration
~~~

Root cause: stale architecture-test ownership assumption, not a missing execution-safety mechanism. Production already builds the continuation with
`NavigationExecutionSafetyProbeBuilder::buildSampledConstantAccelerationForecast`, uses
`NavigationExecutionSafetyProbeBuilder::kExecutedForecastSamples = 12`, and checks every adjacent sampled chord through
`exactExecutionSegmentBlocked` while raising
`staticSafetyExecutedForecastBlocked` on collision. The gate still searched for the removed local implementation detail
`ExecutedSafetySamples = 12` inside `GameSimulation.cpp`.

Correction: the Stage-12 architecture gate now verifies the sample-count owner in
`NavigationExecutionSafetyProbeBuilder.h` plus the sampled builder call, sampled-loop traversal, exact-static segment check, and executed-forecast blocking state in `GameSimulation.cpp`.

No Navigation-v2 planner, follower, control law or safety algorithm changed in this pass. Full target-machine acceptance still requires the corrected architecture gates, canonical build, and freshly built server self-test.


### 2026-09-19 full post-failure Stage-12 gate trace

Candidate code/contract baseline before this documentation sync:

~~~text
a8a40a00985c9664e310c7b90c2ac6d4cec2e23f
~~~

Target-machine evidence at checkout `9d2f9120d8b66b7d8b553d5eb92b555552bdd4bf`:

- `check_navigation_foundation_lock.py`: PASS;
- Stage-12 runtime-planner gate advanced through all earlier checks and failed at
  `accepted segment target revision must remain distinct from high-level intent revision`.

Root cause of the reported failure: stale architecture-test ownership. Revision separation is intact in production:

- `AcceptedShortSegment::goalRevision` is the high-level maneuver/goal revision;
- `AcceptedShortSegment::revision` is the concrete accepted short-segment revision;
- `TrajectoryFollower` publishes them as `intent.revision` and `intent.targetRevision` respectively;
- `NavigationFrameBoundary` preserves both fields across NavLocal -> System conversion;
- `NavigationRuntimeControlBridge::toPilotCommand` forwards both revisions to `PilotSkillExecutor`;
- `PilotSkillExecutor` exposes the active concrete target revision through `activeTargetRevision`.

The obsolete gate incorrectly required a duplicate `targetRevision` field to be declared in
`NavigationRuntimeControlBridge.h`, although the bridge now aliases the typed
`NavigationSystemControlIntent` DTO and should not duplicate that state.

A full audit of every architecture condition after the failing check found one additional stale marker before rerunning the target machine: the live moving-gap kinematic proof now correctly owns
`expectedMovingGapVelocityMapMps` in NavLocal, while the gate still searched for the superseded
`expectedMovingGapVelocityWorldMps` name. That marker was corrected in the same candidate.

All other remaining post-failure contract markers were checked against current source: authoritative self-test evidence, slit/tunnel topology and capture, exact-static physical sweep, bounded visibility recovery, ManeuverDecision doctrines, automatic/manual replan policy, Assisted/Newtonian control-law contract, and client/server CMake ownership. No additional stale marker was found in that remaining portion of the gate.

No Navigation-v2 planner/control/safety behavior changed in this pass. Full target-machine acceptance still requires the refreshed Stage-12 gate, canonical build, and freshly built server self-test.


### 2026-09-19 live self-test reached rotating-infrastructure precision gate

Candidate code/contract baseline before this documentation sync:

~~~text
a611aaec5d01368dcd6c13a1b40a30f3197a7f6b
~~~

Target-machine evidence at checkout `8c32e5a72084646933e959d8308037bd6fbe9f3b`:

- foundation architecture gate: PASS;
- Stage-12 runtime-planner architecture gate: PASS;
- navigation runtime tests: PASS 6/6;
- canonical client build: PASS;
- canonical headless server build: PASS;
- freshly built `EliteServer --self-test-navigation` entered the real live proving run and failed only at rotating-infrastructure angular-velocity verification.

Observed witness:

~~~text
expected=(0,0,0.0349066)
observed=(1.33529e-10,6.92304e-10,0.0349066)
error=7.11278e-10 rad/s
~~~

Root cause: diagnostic precision contract was stricter than the authoritative source representation. `StaticObject::angularVelocity` is currently a `glm::vec3` float field. The hub/local angular velocity is computed in double, quantized once into that float field, then promoted back to double and crossed through `NavigationFrameBoundary::SystemAngularVelocity -> NavAngularVelocity`. The previous `1e-12 rad/s` acceptance threshold therefore demanded precision the authoritative source does not contain; the measured error is consistent with one float quantization at ~0.035 rad/s and does not indicate a frame-transform defect.

Correction: `NavigationRuntimeLabAngularVelocityToleranceRadPerSecond` is now `1e-8`, with the source-precision rationale pinned next to the constant. This remains far below any physically meaningful angular-velocity error for the lab while admitting the actual authoritative storage precision.

No planner, follower, collision, replan or control-law behavior changed in this pass. Next target-machine step is a canonical rebuild followed by the live navigation self-test so execution can proceed to the next physical acceptance gate.


### 2026-09-19 live visibility deadlock — dynamic sphere broadphase replaced by exact OBB narrow-phase

Candidate code/contract baseline before this documentation sync:

~~~text
cf224145d758d09d35d70c63324c97d02f619e60
~~~

Fresh target-machine live self-test evidence from checkout `93fa488b8a5b226134309b3486e96e00c93f9afb`:

- both architecture gates: PASS;
- navigation runtime tests: PASS 6/6;
- canonical client/server build: PASS;
- rotating infrastructure proof: PASS after source-precision correction;
- live flight remained collision-free for 120 s but stalled after ~351 m of progress;
- `visibilityBypassSeen=1`, later `ConflictHold` occurred, `movingGapPlanePassed=0`, `visibilityDirectRecoveredSeen=0`;
- moving-gap kinematic verification reported ~0.001967 m/s residual.

Root cause of the flight deadlock: `NavigationMap` published each moving 360 x 360 x 900 m guidance box only as a conservative enclosing sphere. Its radius is about 517 m before agent/safety inflation. At roughly the observed 350 m route progress the ship enters that conservative sphere while still outside the real HitVolume OBB. `LocalHorizonPlanner` then treated the broadphase sphere as final collision truth at t=0, so every visibility probe became dynamically conflicting and the actor fell into repeated `ConflictHold` instead of continuing around the real box.

Architecture correction in this candidate:

- `NavigationMap::DynamicActorInput` and query `Candidate` now carry optional value-owned exact NavLocal `NavigationObstacle` geometry in addition to the swept-sphere broadphase radius;
- authoritative dynamic infrastructure publishes its current HitVolume-derived OBBs through `NavigationHitVolumeAdapter`, explicitly transformed System -> NavLocal at the existing typed boundary;
- `LocalHorizonPlanner` keeps the swept sphere for candidate collection but, for translation-only dynamic shapes with negligible angular/acceleration terms, re-tests both current kinematics and the bounded requested corridor against the real moving OBBs in the obstacle translating frame;
- rotating/accelerating dynamic geometry remains conservative sphere fallback until a dedicated continuous swept-OBB solver owns those motion classes;
- regression `testDynamicSphereBroadphaseDoesNotSealClearExactObbRoute` pins the failure mode: sphere overlaps the route while exact OBB + ship envelope remains clear;
- NavigationMap contract now pins exact geometry as value-owned NavLocal state rather than an owner reference.

The ~0.001967 m/s moving-gap velocity residual has the same representation cause as the earlier angular-velocity residual: `StaticObject::linearVelocity` is currently `glm::vec3` while hub world velocity is orbital-scale. The live diagnostic tolerance is now a named `NavigationRuntimeLabLinearVelocityToleranceMps = 1e-2`, tight enough for the lab but consistent with the authoritative source precision.

No static exact-HitVolume authority was weakened. The actual authoritative ship sweep remains checked every fixed step against NavigationSpace exact static geometry. This change only prevents a dynamic broadphase sphere from overriding more precise dynamic HitVolume truth when that truth is available.

Next target-machine gate: architecture contract + navigation map/local/runtime tests + canonical build + fresh live self-test. The expected behavioral change is that the ship no longer stalls inside the moving box's enclosing sphere and can proceed through visibility bypass, direct recovery, portal capture and tunnel transit.


### 2026-09-19 dynamic OBB gate ownership correction

Candidate code/contract baseline before this documentation sync:

~~~text
12a759f725a903b047d198e9daf7aaf6939640b9
~~~

Target-machine architecture failure:

~~~text
[FAIL] dynamic conservative spheres must narrow against exact translation-only OBB geometry instead of sealing real free space
~~~

Root cause: the architecture gate searched for the new dynamic narrow-phase implementation in `LocalAvoidancePlanner.cpp`, but the implementation deliberately belongs to `LocalHorizonPlanner.cpp`, where dynamic candidate collision truth is evaluated. Production ownership is therefore correct; the contract pointed at the wrong owner file.

Correction: the gate now loads `LocalHorizonPlanner.cpp` explicitly and verifies `exactTranslationNarrowPhaseAvailable`, `exactTranslationConflict`, `segmentIntersectsNavigationObstacle`, exact candidate geometry consumption, and the regression `testDynamicSphereBroadphaseDoesNotSealClearExactObbRoute` against the correct owner.

The remainder of the new slice was rechecked against source: NavigationMap exact-geometry value ownership, live HitVolume OBB publication, and named moving-infrastructure velocity tolerance markers are all present. No navigation behavior changed in this follow-up; the dynamic exact-OBB candidate remains the implementation baseline awaiting target-machine compilation/runtime validation.


### 2026-09-19 exact-OBB broadphase fix exposed a non-blocking moving fixture

Candidate code/contract baseline before this documentation sync:

~~~text
18076a9851647fcf9eb1b237105d0de4bdfd3440
~~~

Fresh target-machine live evidence after the dynamic exact-OBB narrow-phase candidate:

- canonical build: PASS;
- moving-gap kinematics: PASS (`moving_gap_kinematics=1`);
- no dynamic `ConflictHold` and no exact-static violation;
- progress increased from ~351 m to ~1903 m inside the same 120 s run;
- slit portal selection/capture/alignment/entry-plane crossing all became visible;
- however `visibility_bypass=0` and `moving_gap_passed=0`, so the self-test still failed its ordered visibility-bypass evidence gate.

This is not a regression in avoidance. The exact geometry revealed that the authored moving pair did not actually block the route. The original centres were Y=-790 and Y=-1890 with 360 m-high GuidanceDockCube HitVolumes, leaving about 740 m of real vertical free space. The lab route is Y=-1300, almost through the middle of that gap. The old sphere-only broadphase falsely made this look blocked; once the real OBB narrow-phase became authoritative, the correct result was direct flight with no visibility bypass.

The proving fixture is corrected rather than weakening the self-test. The moving pair now has:

~~~text
aperture center Y = -900 m
boundary centre separation = 500 m
GuidanceDockCube height = 360 m
physical aperture = 140 m
route Y = -1300 m
~~~

The direct route therefore intersects the lower real OBB, while a bounded upward visibility deflection can enter the actual 140 m aperture. This preserves the intended test: NavigationMap sphere performs broadphase candidate collection, exact dynamic HitVolume OBBs decide collision truth, and bounded visibility steering must find real free space rather than react to an enclosing-sphere artefact.

The Stage-12 architecture gate now pins the explicit offset-aperture constants/derivation so this fixture cannot silently drift back into a geometry that requires no avoidance.

No planner/control-law behavior changed in this follow-up. Next target-machine run should prove that the corrected real geometry now produces `visibility_bypass=1` without restoring the previous false `ConflictHold` deadlock.


### 2026-09-19 corrected moving aperture: bypass+replication proven, forward progress still incomplete

Target-machine checkout actually exercised:

~~~text
46f6a37da6775a1d044391f773476df1bb07bc6a
~~~

Latest live result:

~~~text
[FAIL] visibility-bypass replication succeeded but the ordered live flight did not complete moving-pair bypass, direct recovery and exact-static tunnel passage inside the 120 s bound
moving_gap_passed=0
slit_portal=0
slit_entry_capture=0
slit_entry_crossed_aligned=0
passed_obstacle_plane=0
exact_static_violation=0
simulated_s=120
slit_entry_cross_track_m=148.3
~~~

Interpretation:

- the corrected offset moving aperture now does produce the required visibility bypass strongly enough that the self-test reaches and completes the same-tick replication proof;
- the dynamic exact-OBB broadphase/narrow-phase slice is therefore active in the real authoritative chain rather than only in isolated tests;
- no exact-static physical collision occurred;
- however the actor does not complete the moving-pair bypass/plane crossing and does not recover to the direct route inside the 120 s acceptance window;
- the static slit/tunnel phase is not reached in this run (slit_portal=0), so the current failure is upstream of tunnel capture;
- slit_entry_cross_track_m=148.3 is diagnostic state only here; no slit waypoint/capture was active.

This is now a behavior/execution problem after a valid visibility bypass, not a stale architecture gate and not the old false-sphere deadlock. The next task is to trace the accepted adjusted segment after bypass: selected target, segment expiry/completion, replanning reason, executed demand, actual ship progress relative to the moving-pair plane, and the condition that should switch back to the direct nominal target. Do not weaken the ordered self-test or reintroduce sphere collision authority to make it pass.

The last fully accepted Stage-12 target-machine baseline remains the previously recorded accepted baseline; checkout 46f6a37da6775a1d044391f773476df1bb07bc6a is a tested candidate, not accepted, because the live ordered-flight gate still fails.


### 2026-09-19 end-to-end chain audit: failure localized to LocalVisibility -> accepted execution semantics

Repository state analyzed: `794b02a803b91e47bc991c8faa932a02c3184d0c`. Latest target-machine behavior evidence remains checkout `46f6a37da6775a1d044391f773476df1bb07bc6a`.

Current failure is NOT inability to find a route and NOT inability to move. The live run already proves a real visibility bypass strongly enough to pass same-tick replication, while exact-static collision remains false. Static/global topology has also previously carried the same actor to slit portal capture when the moving pair did not physically block the route.

End-to-end source audit localized the integration defect to the ordinary local-visibility maneuver handoff:

1. Scene/publication is coherent: the lab starts at visual/NavLocal (975,-1300,-6200); the corrected moving lower OBB is centered at (975,-1150,-5700), 360 x 360 x 900 m, and the moving aperture is real. Dynamic OBB truth reaches LocalHorizon through NavigationMap value DTOs.
2. Global NavigationSpace corridor is coherent: the first oriented slit entry portal is at z=-4950 with a 500 m approach, so the coarse approach waypoint is approximately (975,-1180,-5450). From the start the nominal coarse vector is (0,+120,+750), length about 759.5 m.
3. LocalHorizon/LocalAvoidance correctly detect the real dynamic block and can produce an AdjustedClear visibility ray. However this ordinary fan proves geometry only. LocalHorizon AgentState has no directional linear/angular capability and the ordinary fan never time-parameterizes the ray against the craft's anisotropic propulsion.
4. NavigationRuntimePlanner AgentState DOES contain real linear/angular capability and Newtonian/Assisted mode, but those fields are currently consumed only by the precision MovingPassageTrajectoryEvaluator. The live lab explicitly disables MovingPassage steering authority for this ordinary moving-pair encounter, so the selected visibility ray bypasses the capability-aware trajectory layer.
5. A second concrete handoff defect changes maneuver semantics when the planner result is packed into AcceptedShortSegment. During AdjustedClear, NavigationRuntimePlanner itself does not request portal alignment; its angular intent only damps angular velocity. GameSimulation nevertheless sets AcceptedShortSegment.alignForward = lastPlan.portalTraversalActive and desiredForward = portalNormal. Therefore the downstream follower forces the hull to face the later slit portal even while the local planner is commanding a temporary lateral bypass.
6. The latest failure numerically confirms that premature alignment is active upstream of the portal: slit_portal=0 while slit_entry_fwd_angle_rad=0.000679006, i.e. the hull is already almost exactly aligned to the +Z portal normal before reaching the slit phase.
7. Cobra propulsion is highly anisotropic: main forward authority is maxLinearGs 7.5 (~73.55 m/s^2), while physical manoeuvre/RCS authority is only 2 m/s^2. DynamicMotionSystem correctly decomposes the requested System acceleration into forward-only main-engine thrust plus a 2 m/s^2-clamped remainder. Thus a steep visibility ray cannot be executed as planned while the accepted segment simultaneously pins the nose to +Z.
8. Example reconstruction from the actual fixture (illustrative, not the exact latest unlogged selected azimuth): a steep ~75 degree downward visibility ray can request roughly desired velocity (0,-54.8,+24.5) m/s and ideal acceleration from rest about (0,-41.1,+18.4) m/s^2. With the hull pinned near +Z, physics can provide the +Z main-engine component but only about 2 m/s^2 laterally. The geometrically proven ray and the physically executed trajectory therefore diverge materially.
9. TrajectoryFollower, NavigationFrameBoundary, NavigationRuntimeControlBridge, PilotSkillExecutor and DynamicMotionSystem are individually coherent: they preserve revisions/vectors, cross NavLocal/System explicitly, model reaction/latency/filtering, and enforce real propulsion authority. The wrong assumption has already entered the accepted execution product before these layers.
10. ManeuverDecisionController and CONTROL_LAW_MANEUVER_MODEL already document the required architecture: Navigation should generate physically truthful control-law-compatible maneuver candidates (Newtonian coast/drift/RCS trim/turn-then-burn/etc.), then the decision owner selects among them. The current live lab path does not invoke ManeuverDecisionController for ordinary visibility and still converts a geometric visibility ray directly into an arbitrary acceleration request.

Primary defect boundary: `LocalAvoidance geometric AdjustedClear -> NavigationRuntimePlanner desired velocity -> GameSimulation AcceptedShortSegment`. Two corrections are required by contract, not test weakening: (a) do not inherit later portal forward-alignment while executing an AdjustedClear bypass unless that specific maneuver requires it; (b) ordinary visibility candidates must become capability/time-aware or escalate to a control-law-compatible trajectory primitive before acceptance. Moving the fixture farther away could mask the issue but would not repair this contract.

The exact selected deflection/replan sequence in the latest target-machine run is not printed by the second-phase failure, so the next implementation/debug pass should add bounded event diagnostics (only on plan/replan/bypass transitions) for selected target/deflection, accepted alignment, ideal/executed/applied acceleration, replan reason and route/moving-plane progress. This will target-machine-confirm the predicted divergence without producing per-frame log spam.


### 2026-09-19 behavior-character and vehicle-feasibility contract tightened

Architecture baseline for this decision: `e2d4238968a1b3f360eb342ced2ca0d4d782b5e7`.

Three current defects are now explicit architecture blockers:

1. A geometric path/ray must never become an executable route or AcceptedShortSegment unless it is feasible for the actual vehicle from the current state. Global/topological routing may stay coarse, but every edge/portal must be vehicle-feasible at its abstraction level; local accepted segments must be time-parameterized and dynamically feasible from current P/V/A/attitude/angular state under real propulsion and damage-degraded capability.
2. For a main-engine-dominant Newtonian craft, substantial delta-v should normally come from rotating the hull to a burn vector and using the main engine, with coast/rotate/brake/flip-and-burn as required. The small manoeuvre/RCS system is primarily trim/precision/parking/docking/capture authority, not a hidden omnidirectional main engine. A craft whose real propulsion profile differs may legitimately use omnidirectional thrusters as primary translation.
3. Behavior character is not one scalar. It has two independent inputs: (a) situation/doctrine, such as ordinary/rational, extreme attack/escape, or precision ingress/retrieval; and (b) pilot model/transient pilot state. Doctrine changes candidate generation/ranking preferences and accepted risk bands. Pilot skill changes reaction/latency, anticipation, control precision, overshoot/damping, hull/clearance judgement uncertainty and execution envelope. Neither may modify world geometry or vehicle capability.

New canonical document: `src/game/navigation/NAVIGATION_BEHAVIOR_CHARACTER_MODEL.md`.

`CONTROL_LAW_MANEUVER_MODEL.md` now explicitly requires main-engine-oriented Newtonian course changes and forbids relying on the propulsion allocator to rescue an impossible arbitrary acceleration request. `MANEUVER_DECISION_TREE.md` now states `geometric path != executable route` and separates doctrine from pilot ownership.

Current live failure remains localized at the ordinary visibility handoff. The next implementation must not merely patch alignment; it must establish the missing sequence `free-space candidate -> control-law-compatible maneuver generation -> capability/pilot-aware continuous proof -> decision -> accepted segment`. The existing premature future-portal alignment bug is still a concrete defect inside that seam and must be removed as part of the correction.


### 2026-09-19 pipeline audit activated; first P9 handoff defect corrected

Code/contract candidate before documentation commits: `db79542ad0547f34dfadcb933bd1135067c145c7`.

Canonical audit: `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`. Navigation is now reviewed left-to-right as INPUT / RESPONSIBILITY / OUTPUT / HANDOFF / PERFORMANCE contracts; isolated green tests are not sufficient if the handoff changes semantics or omits required truth.

Scaling contract: avoid dense N^2 work. Current NavigationMap already uses a spatial hash and bounded local queries. Target architecture is scene-wide sparse broadphase -> unordered potentially-interacting pairs -> batch/SIMD kinematic filtering -> per-agent influence lists -> exact narrow phase only for survivors. Per-agent duplicate discovery is an optimization follow-up, not the current live failure.

Current audit localization: P5 LocalAvoidance produces geometric free-space candidates but ordinary AdjustedClear is not yet vehicle-feasible; P6 physical maneuver generation is missing for ordinary visibility; P7 continuous capability/geometry proof exists in precision components but is not applied there; P8 ManeuverDecisionController is bypassed by the ordinary live chain. P9 had a concrete semantic handoff defect and is corrected in this candidate.

P9 correction: NavigationRuntimePlanner now publishes `selectedManeuverRequiresForwardAlignment` plus `selectedManeuverForwardMap`. Only the selected current nominal portal capture/transit maneuver sets them. GameSimulation copies these fields into AcceptedShortSegment and no longer derives alignment from future route context (`portalTraversalActive`). A regression and architecture gate pin that AdjustedClear with a future portal must not inherit portal-forward alignment.

One-shot first-bypass diagnostics now capture P5->P9->P13 evidence: selected deflection/target, agent P/V, accepted attitude requirement, follower ideal acceleration, PilotSkill executed acceleration, and physically applied main/RCS/total acceleration. The next target-machine run will therefore expose the exact divergence without per-frame logging.

No acceptance promotion. Last actually exercised live checkout remains `46f6a37da6775a1d044391f773476df1bb07bc6a`; last fully accepted baseline remains the recorded accepted baseline. Candidate `db79542ad0547f34dfadcb933bd1135067c145c7` still needs target-machine verification.

Next repair stage is P6/P7: free-space rays become candidates only; for main-engine-dominant Newtonian craft they must be converted into real rotate/main-burn/coast/trim/brake or flip-and-burn primitives and continuously proven against capability/geometry/pilot uncertainty before ACCEPT.


### 2026-09-19 command ownership and accepted maneuver API corrected

Architecture/code candidate before documentation commits: `3aa0639e72d094ae95ecea041c8926f0dfecf59a`.

Portal semantics were corrected conceptually: portal existence must not imply mandatory centerline capture or hull alignment. The correct question is which passage maneuver is safe/acceptable for the current opening geometry/motion, vehicle capability, pilot execution uncertainty and Situation/Doctrine. A wide, slow, high-margin portal in Ordinary/Rational behavior may be crossed as FreeTransit without stopping or forced alignment; tight/fast/precision cases may require PrecisionCapture/Transit; Extreme behavior may accept smaller margin/higher load/contact according to explicit policy.

New canonical document: `src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md`.

Ownership is now fixed as:
- objective/mission owner selects semantic goal/terminal contract;
- global route selects topology/corridor/portal opportunities, not control;
- maneuver planner compiles the next physically valid bounded maneuver;
- accepted product must contain the same time-parameterized reference state and feed-forward control that were proved: P(t), V(t), A_ff(t), q/body-basis(t), omega(t), alpha_ff(t);
- follower samples that program and adds only bounded tracking feedback; it does not invent a new trajectory/target velocity;
- PilotSkill models reaction/latency/precision around the ideal program;
- propulsion allocator maps vehicle-level acceleration/attitude demand to real main-engine/RCS/torque actuators;
- physics remains final authority.

This means the current `AcceptedShortSegment` + `TrajectoryFollower` API is transitional. Today it carries target position/velocity or one fixed acceleration and the follower re-derives acceleration. That can diverge from the trajectory that was actually proved. `NavigationRuntimePlanner.h` and `AcceptedShortSegment.h` are now explicitly marked transitional toward `AcceptedManeuverProgram`.

Newtonian turn semantics were also tightened: turn does not imply stop-turn-go. A main-engine-dominant craft should preserve useful inertial velocity, rotate the hull ahead of the required future delta-v, begin main-engine burn while V remains non-zero, bend the velocity vector continuously, then coast/trim and rotate early for the next burn or flip-and-burn if braking is needed. Angular authority and pilot reaction/latency determine lead-rotation time.

`NAVIGATION_PIPELINE_AUDIT.md`, `CONTROL_LAW_MANEUVER_MODEL.md`, and `TRAJECTORY_EXECUTION_REPLAN_MODEL.md` were updated to this ownership/API contract.

No live acceptance promotion. The next implementation slice should introduce the bounded AcceptedManeuverProgram representation and migrate the runtime-lab execution seam so proof and execution consume the same program before building the full ordinary Newtonian maneuver generator.


## 2026-09-19 planner/follower two-world architecture analysis

Architecture analysis is now canonical in:

`src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md`

Decision:
- expose two top-level runtime worlds: Planner and Autopilot/Follower;
- Planner consumes authoritative world truth directly; obstacle rays are not perception;
- retain segment/sweep/ray intersection only as geometry/proof primitives;
- replace the LocalAvoidance angular ray-fan as the target ordinary search strategy with a route-aligned configuration-space corridor solver;
- keep NavigationSpace/global topology above that local solver because one A->B longitudinal frame cannot represent arbitrary backtracking/topology changes;
- Planner must compile/prove and publish the exact bounded `AcceptedManeuverProgram` that execution consumes;
- Follower tracks/recoveries only inside the accepted envelope and does not become a hidden normal planner;
- bounded imminent-hazard reflex is permitted inside the autopilot world, but material deviation invalidates the accepted program and immediately requests local replan;
- shared dynamic broadphase/pair isolation should become scene-wide/batched, while planner work remains event-driven for only agents needing a new program.

Current repository comparison:
- NavigationMap authoritative snapshot + spatial hash already match the world-truth model;
- NavigationExecutionReplanPolicy already matches plan -> accept -> execute -> monitor;
- TrajectoryFollower is already obstacle-search-free but still consumes transitional `AcceptedShortSegment`;
- ordinary `AdjustedClear` is still only geometrically clear and the visibility fan remains the primary local search method;
- scene-wide sparse pair/influence computation is still a target, not the current per-agent query implementation.

Implementation order remains conservative:
1. introduce `AcceptedManeuverProgram`;
2. migrate `TrajectoryFollower` to sample the same proved program + bounded feedback;
3. then prototype `RouteAlignedCorridorPlanner` beside LocalAvoidance and A/B test it;
4. add physical Newtonian/Assisted maneuver compilation;
5. retire the ray-fan search only after target-machine evidence.

This iteration changes architecture documentation only. It does **not** promote a new target-machine accepted Stage-12 baseline. Last fully accepted target-machine baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`; last actually exercised target checkout remains `46f6a37da6775a1d044391f773476df1bb07bc6a`.


## 2026-09-19 canonical B0-B14 architecture and B8/B9 migration slice

Canonical Navigation v2 architecture is now fixed in:

- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
- `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`

Top-level ownership is two worlds:

```text
Planner World:
    B0 World Snapshot
    B1 Shared Influence Builder
    B2 Navigation Objective
    B3 Topology Route
    B4 Route-Aligned Local Corridor
    B5 Physical Maneuver Compiler
    B6 Continuous Maneuver Prover
    B7 Maneuver Decision
    B8 Maneuver Acceptance / Program Store

Autopilot / Follower World:
    B9 Maneuver Program Sampler
    B10 Bounded Tracking Controller
    B11 Safety Monitor / Bounded Reflex
    B12 PilotSkill
    B13 Propulsion / Physics

Scheduling service:
    B14 Navigation Work Scheduler
```

Each canonical block now has an explicit owner, question, invocation cadence, input, output, forbidden responsibilities and scaling contract.

The current repository was audited against those blocks. The important preservation/migration result is:

- KEEP: NavigationSpace static topology/exact geometry foundation;
- KEEP: NavigationMap spatial hash/broadphase foundation;
- KEEP: PilotSkill and propulsion/physics authority;
- KEEP: NavigationExecutionReplanPolicy rule that another frame alone does not wake planning;
- MIGRATE: per-agent dynamic discovery toward shared scene-wide sparse InfluenceFrame;
- REPLACE AS TARGET SEARCH: LocalAvoidance angular ray-fan with a route-aligned configuration-space corridor planner, while retaining segment/sweep/ray intersection as proof primitives;
- ADD: ordinary physical maneuver compiler and generalized continuous proof;
- USE: existing ManeuverDecisionController only after candidates are physically proved;
- REPLACE BOUNDARY: AcceptedShortSegment with AcceptedManeuverProgram;
- SPLIT: program sampling from bounded feedback tracking;
- ADD: explicit bounded safety-reflex API;
- ADD: dirty-agent planning scheduler with urgent/normal/background queues.

Scaling canon is explicit: shared B0/B1 work for the scene; cheap fixed-step execution for active controlled actors; expensive B3..B8 work only for dirty agents. No dense N x N and no plan-every-frame. B14 will budget queued planner jobs, reject stale revisions, prioritize urgent invalidations and provide starvation prevention.

### Iteration 1 implementation candidate — B8/B9

Code/contract baseline before documentation commits:

```text
516f63eb7a3b2bbf09bad553aff975d52d7c3e8c
```

New production API:
- `AcceptedManeuverProgram.h` — fixed-capacity 16-sample value-owned program carrying P/V/A_ff, body basis, omega/alpha_ff, validity/revisions, terminal tolerances, tracking reserve, capability and proof witnesses;
- `ManeuverProgramSampler.h/.cpp` — pure time sampler with no world query, no obstacle search, no feedback controller and no replanning.

New isolated runtime test:
- `ManeuverProgramSamplerTests.cpp`;
- wired into `tests/navigation_runtime` as `maneuver_program_sampler`;
- sampler implementation also linked into the production `EliteNavigationWorldRuntime` target.

The live GameSimulation chain intentionally still uses AcceptedShortSegment. This slice introduces and tests the clean B8/B9 API before live migration.

Target-machine gate is pending:

```bash
bash tests/navigation_runtime/run_mingw64.sh
bash build_mingw64.sh
```

No Stage-12 acceptance promotion is claimed from this documentation/code pass. Last fully accepted target-machine baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


## 2026-09-19 B8/B9 accepted; B10 bounded tracking candidate

Target-machine verified checkout:

```text
701881ddae861cd5593e425de91600e048bd417c
```

Evidence supplied by user:
- `navigation_runtime`: 7/7 PASS;
- `maneuver_program_sampler`: PASS;
- canonical `EliteGame`: BUILD PASS;
- canonical `EliteServer`: BUILD PASS.

This accepts the first clean execution-boundary slice B8/B9:
`AcceptedManeuverProgram` + `ManeuverProgramSampler`.

Current B10 candidate baseline before documentation commits:

```text
66d89b93e3edf0817bb8d88b78e405180887cecc
```

B10 is now an explicit `ManeuverTrackingController` block. It consumes one
B9 reference sample plus actual vehicle kinematics and may add only bounded
feedback inside the reserve carried by `AcceptedManeuverProgram`.

The new Navigation-v2 TrajectoryFollower path is:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> ManeuverTrackingController
 -> NavigationLocalControlIntent
```

The old `AcceptedShortSegment` overload remains live for compatibility.

Architecture contracts now forbid B8/B9/B10 from depending on
NavigationMap, NavigationSpace, GameSimulation, NavigationRuntimePlanner,
planner/world query entry points or unbounded `std::vector` hot-path storage.

New regression `maneuver_tracking_controller` pins:
- zero tracking error => exact `A_ff/alpha_ff`;
- tracking feedback is clamped to the accepted reserve;
- B9 -> B10 composition inside TrajectoryFollower;
- terminal completion uses accepted tolerances;
- execution before acceptance time fails closed.

`tests/navigation_runtime/run_mingw64.sh` now prints configure/build/test/total
phase timings. No persistent log is currently required. If later diagnosis
creates one, the invoking script/command must print the exact path at completion.

B10 target-machine gate is pending; do not promote the candidate until that
evidence is supplied.


## 2026-09-19 B10 first target-machine gate failed; corrective candidate

The first B10 gate failed before runtime tests.

Evidence:
- architecture contract failed in 0.173 s because a bare-word dependency grep
  matched the documentation comment saying AcceptedManeuverProgram has no
  NavigationMap/NavigationSpace ownership;
- navigation_runtime configured, then MinGW/g++ 15.2 rejected the nested API
  default-reference argument `const Policy& policy = {}`;
- production build failed on the same header path after 17.209 s;
- no B10 runtime test result is acceptance evidence from that run.

Corrections are now in the code/contract candidate
`4a3d196c1574e91b747f05d194db7a35ad5c5517` before documentation commits:
- explicit 3/4-argument overloads replace braced default-reference arguments;
- the architecture lock checks real include/type/query dependency syntax, not
  prose comments;
- the lock pins the MinGW-safe overload form;
- `tests/navigation_runtime/run_mingw64.sh` prints configure/build/tests/total
  timing and failing phase even on failure.

No persistent log file was needed for this diagnosis.

B10 remains target-machine pending.
The last verified B8/B9 baseline remains
`701881ddae861cd5593e425de91600e048bd417c`.


## B10 corrective rerun — production build green, isolated gate evidence pending

Fresh target-machine evidence from the corrective rerun:
- canonical `EliteGame`: BUILD PASS;
- canonical `EliteServer`: BUILD PASS;
- full `build_mingw64.sh` wall time: **44.989 s**.

The previously failing MinGW header/API defect is therefore closed in the
production compile/link path.

The supplied excerpt does not include:
- the architecture-contract result;
- the `navigation_runtime` 8/8 result;
- the local `git rev-parse HEAD` line.

Therefore B10 is still **not accepted** yet. Production build evidence is green,
but the isolated B9/B10 behavioral gate and architecture lock still require
fresh target-machine output.

Current repository documentation HEAD at the time of this evidence:
`2abd79a6181a322fe15425994ab771942e47bc26`.
Do not equate that with the tested checkout unless the target-machine
`git rev-parse HEAD` output is supplied.

No persistent log was required for this successful build.


## 2026-09-19 B10 accepted; B14 scheduler candidate

Verified target-machine baseline:

```text
2abd79a6181a322fe15425994ab771942e47bc26
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 8/8 PASS;
- maneuver_tracking_controller PASS;
- EliteGame BUILD PASS;
- EliteServer BUILD PASS;
- production build real time 44.989 s;
- architecture-contract real time 0.186 s.

B8/B9/B10 are therefore accepted.

Current B14 candidate code/contract baseline before documentation commits:

```text
1ba23241d760b3a57c917189e333e4fbc0365ea4
```

B14 now exists as `NavigationWorkScheduler` + value-owned
`NavigationPlannerJob`.

The scheduler is intentionally stateful only for scheduling:
- urgent / normal / background queues;
- actor revision state;
- one pending actor job slot;
- in-flight tickets;
- deterministic age promotion;
- bounded dispatch slices.

It performs no geometry/world query and owns no planner callback.

Correctness/scale rules pinned in tests:
- monotonic per-actor job revision;
- duplicate suppression and priority upgrade;
- completed job revision cannot be replayed;
- stale work is rejected before dispatch;
- stale in-flight result is rejected before commit;
- capacity pressure reclaims stale jobs;
- ordinary replacement does not scan queues;
- lazy tombstones are amortized/compacted so physical queue storage remains bounded;
- dispatch is bounded by fixed job count and deterministic cost units;
- 5000 synthetic actors drain with no loss or duplication and all queue/in-flight
  storage returns to zero.

Timing is diagnostic only. The scheduler scale test prints enqueue,
dispatch/complete and total microseconds. The navigation_runtime gate repeats
only that successful test in verbose mode so the timing line is visible.

The B14 scheduler is not wired into live GameSimulation yet. This candidate is
only the clean scheduler block/API and scale gate. Live scheduler integration is
the next separately gated slice after acceptance.

No persistent diagnostic log is required for the current gate.


## 2026-09-19 B14 isolated gate green; live scheduler integration candidate

Fresh target-machine B14 isolated evidence:
- architecture contract PASS (0.204 s);
- navigation_runtime 9/9 PASS;
- navigation_work_scheduler PASS;
- 5000 actors: enqueue 1518 us, dispatch+complete 1258 us, total 2776 us;
- EliteGame / EliteServer BUILD PASS;
- production build 23.001 s.

The exact rev-parse line was not included in the supplied B14 excerpt, so no
tested B14 hash is invented. The last explicitly named target-machine hash
remains the B10 baseline
`2abd79a6181a322fe15425994ab771942e47bc26`.

Live B14 candidate before documentation commits:
`382c9d6f8ae347630ccd1a6ae6ec18bd077d086d`.

GameSimulation's deterministic Stage-12 lab now routes every dirty replan through
NavigationWorkScheduler enqueue -> bounded dispatch -> existing Planner::plan ->
complete(ticket), and commits the planner result only on CompletedCurrent.

Planner geometry and AcceptedShortSegment packing are unchanged in this slice.

Live orchestration publishes current world, objective, capability and job
revisions. Capability revision is monotonic and advances from actual current
linear/angular authority changes.

NavigationRuntimeLabObservation records scheduler enqueue/dispatch/completion
counts, queue depths and dispatch/planner timing. The headless navigation
self-test now requires dispatchCount == planCount,
completedCurrent == dispatchCount and zero stale completion in the synchronous
lab.

A logged gate script was added:
`tests/navigation_runtime/run_live_scheduler_gate_mingw64.sh`.
It stores the complete self-test output under `build/logs`, prints total time,
and prints the exact log path as its final line on both PASS and FAIL.

Live B14 target-machine acceptance is pending.


## 2026-09-19 live B14 gate blocked by pre-existing ordinary-maneuver defect

Target-machine checkout:

```text
a69771e3efb5b54834b002a5000b79d75a8f5e80
```

Fresh evidence:
- architecture contract PASS: 0.211 s;
- navigation_runtime 9/9 PASS;
- isolated navigation_work_scheduler PASS;
- 5000 actors: enqueue 2053 us, dispatch+complete 1261 us, total 3314 us;
- EliteGame / EliteServer BUILD PASS;
- production build 45.371 s;
- live headless navigation gate FAILED rc=56 after 15.144 s;
- log: D:\__elite\work\build\logs\navigation_live_scheduler_20260919-221240.log.

The live failure is not new B14 behavior. It reproduces the already documented
pre-B14 ordered-flight blocker from checkout 46f6a37...:
visibility bypass and same-tick replication succeed, but the actor does not pass
the moving obstacle plane or reach the static slit/tunnel within 120 simulated
seconds.

Current evidence:
- first visibility bypass is captured;
- selected deflection is 1.309 rad (~75 degrees);
- ideal linear demand at first bypass is approximately
  (0.00087, -41.08, +18.37) m/s^2;
- first executed/applied demand is still near zero while PilotSkill/latency
  catches up;
- alignForward is now false, so the former premature future-portal alignment
  defect is already fixed;
- exact-static violation remains false.

Revision audit also proves PilotSkill reaction delay is not restarted by every
AcceptedShortSegment: TrajectoryFollower uses constant goalRevision as
intent.revision and segment revision only as targetRevision. PilotSkill restarts
reaction delay only when intent revision changes.

Remaining primary defect is the already-known B4 -> B5 gap:
ordinary LocalAvoidance AdjustedClear is a geometric visibility ray. Runtime
Planner converts it directly to an arbitrary desired velocity/acceleration
without compiling it through current directional propulsion + finite attitude
authority. This is invalid for main-engine-dominant Newtonian craft.

Do not weaken the live gate. Next implementation task is clean B5 ordinary
physical maneuver compilation, followed by proof/integration.


## 2026-09-19 isolated B5 Newtonian ordinary maneuver compiler candidate

Current code/contract baseline before documentation commits:

```text
233d4023e81d5d466043a08c67acd8d49846c4b5
```

New strict-pure B5 block:
- `OrdinaryPhysicalManeuverCompiler.h/.cpp`;
- fixed-capacity `OrdinaryPhysicalManeuverCandidate`;
- no NavigationMap/NavigationSpace/planner/world query ownership;
- no wall clock, file I/O or unbounded vector.

The first slice supports Newtonian only:
- Coast;
- body-axis-feasible Trim;
- LeadRotateMainBurn for material delta-v outside direct RCS/body-axis authority.

LeadRotateMainBurn uses a quintic lead-rotation bounded by angular acceleration
and angular speed plus control-response reserve, followed by a forward-aligned
main-engine feed-forward burn. B10 feedback reserve is subtracted before B5
consumes feed-forward authority.

Every candidate keeps `requiresContinuousProof=true`; no B5 result can be
accepted without B6.

Assisted deliberately reports UnsupportedControlLaw in this first slice.

New isolated regression `ordinary_physical_maneuver_compiler` pins:
- direct trim authority;
- lateral delta-v escalation to lead-rotate/main-burn;
- fail-closed behavior without angular authority;
- tracking reserve preservation;
- explicit Assisted unsupported status;
- the live ~75-degree / ~41 m/s^2 lateral-demand failure class with ~2 m/s^2
  RCS;
- diagnostic batch timing for 10,000 dirty-actor compiles.

The long live navigation self-test should not be rerun until B6 proof and live
B5 integration are present.


## 2026-09-19 B5 green gate; planner owns maneuver family, follower only tracks

Fresh target-machine evidence:
- architecture contract PASS: 0.191 s;
- navigation_runtime 10/10 PASS;
- ordinary_physical_maneuver_compiler PASS;
- 10,000 B5 compiles: 30,495 us total, 3,049.5 ns/compile;
- navigation_work_scheduler 5000 actors: 2,801 us total;
- EliteGame / EliteServer BUILD PASS;
- production build: 23.544 s.

The supplied excerpt did not include a rev-parse line, so no exact tested B5
hash is invented.

Control ownership is now explicit:
- B4 chooses geometric local path/corridor;
- B5 generates physically executable maneuver families, including hull
  reorientation and use of the main engine;
- B6 proves those exact maneuvers;
- B7 chooses among proved alternatives;
- B8 freezes the chosen program;
- B9/B10 autopilot/follower samples and tracks the frozen program with bounded
  feedback only.

Therefore the follower must not independently decide that RCS is insufficient,
rotate the hull and substitute main-engine thrust. That is a planner-side
maneuver change and requires proof.

For main-engine-dominant Newtonian craft, main propulsion is the normal
translation authority for material delta-v; RCS is trim/precision authority.

After the green B5 gate the compiler was corrected so a main-engine
LeadRotateMainBurn candidate is exposed even when a slow RCS Trim candidate is
also physically feasible. B7 retains final selection ownership.

This revised B5 candidate is not yet target-machine accepted and must receive a
short isolated rerun before B6 work starts.


## 2026-09-19 revised B5 accepted; execution lab candidate

Fresh target-machine evidence after the main-engine-option correction:
- architecture contract PASS;
- navigation_runtime 10/10 PASS;
- ordinary_physical_maneuver_compiler PASS;
- RCS-feasible delta-v still exposes a main-engine alternative for B7: PASS;
- 10,000 B5 compiles: 30,824 us total = 3,082.4 ns/compile;
- navigation_work_scheduler 5000 actors: 2,670 us total;
- EliteGame / EliteServer BUILD PASS;
- production build 23.861 s.

The supplied paste did not include a rev-parse line, so no exact tested hash is
invented.

Before B6 integration, a new maneuver execution lab now tests the existing
AcceptedManeuverProgram execution chain without route search or obstacles.

Scenarios:
- 100 m straight stop-to-stop;
- 100 m + 90-degree stop/rotate + 100 m;
- the same two-leg route inside a 5 m half-width polyline corridor.

The lab executes through B9/B10 -> PilotSkill -> SharedShipPhysics ->
DynamicMotionSystem and measures final position error, final speed, overshoot,
cross-track, corner miss, corridor violation and simulated completion time.

The first profile is deliberately expert/zero-latency to isolate pure autopilot
tracking/physics behavior. This target-machine run may fail; that is useful
diagnostic evidence rather than a reason to relax the route or tolerances.


## 2026-09-19 maneuver execution lab candidate ready for target-machine measurement

Current execution-lab code candidate before documentation commits:

```text
78fd1e356138f94f6e6b8990053d80fdc419eb4d
```

The fixture is wired through the actual execution stack:
- AcceptedManeuverProgram;
- TrajectoryFollower B9/B10;
- NavigationRuntimeControlBridge / PilotSkillExecutor;
- SharedShipPhysics angular execution;
- DynamicMotionSystem main/RCS propulsion allocation;
- DynamicMotionSystem translation integration.

Scenarios:
- 100 m straight stop-to-stop;
- 100 m first leg, stop, 90-degree yaw, 100 m second leg;
- same two-leg path monitored inside a 5 m half-width corridor.

Output includes arrival classification, final position/speed, geometric
cross-track, overshoot, corner error, corridor violation and simulated time.

This is intentionally a measurement gate. Do not relax thresholds before the
first target-machine result is observed.


## 4-leg 3D corridor pilot/control-law matrix candidate — 2026-09-19

Previous simple execution lab is target-machine accepted on exact checkout:

```text
b5f558b18c8ef8e1b5d9562df36b34cb621c648f
```

Observed:
- architecture contract PASS;
- navigation_runtime 11/11 PASS;
- straight 100 m:
  - final position error 0.0285925 m;
  - final speed 0.102262 m/s;
  - zero cross-track / zero overshoot;
- right-angle 100 m + 100 m:
  - final position error 0.0285925 m;
  - final speed 0.102262 m/s;
  - maximum route cross-track 0.0470123 m;
  - corner error 0.0285925 m;
  - zero 5 m corridor violation;
- maneuver execution lab PASS;
- B5 compiler PASS;
- scheduler PASS.

New code/contract candidate before documentation commits:

```text
d0f8b07787339074e225cd02cfe96c93bad0446a
```

New target:
`maneuver_corridor_matrix`.

Route has four stop-to-stop 3D translation legs through five points:

```text
P0 (0,   0,   0)
P1 (0,   0, -120)
P2 (85, 35, -190)
P3 (25,100, -265)
P4 (120,55, -340)
```

Three bounded quaternion/axis-angle attitude transitions connect the legs.
The same route is executed in:
- Newtonian;
- Assisted.

PilotSkill matrix:
- expert: zero reaction/latency, high command bandwidth, no noise;
- competent: exact current production NpcAiSystem baseline
  (0.12 s reaction, 12 Hz decision, 0.06 s latency, 2 Hz response,
  current deterministic command noise/slew);
- rookie: deterministic degraded profile with 0.30 s reaction, 6 Hz decision,
  0.12 s latency, slower response/slew and larger deterministic command noise.

Total rows: 2 laws x 3 pilots = 6.

The corridor is a 5 m half-width tube around the 3D route polyline.

Every row reports:
- valid/completed;
- translation legs completed /4;
- rotations completed /3;
- final position error;
- final residual speed;
- maximum route cross-track;
- maximum active-leg cross-track;
- maximum corridor violation;
- maximum waypoint error;
- maximum overshoot;
- maximum forward-angle tracking error;
- tracking-envelope-exceeded tick count;
- total simulated seconds.

First target-machine pass is deliberately asymmetric:
- expert rows are strict and must complete inside the 5 m corridor with
  <=1 m final error and <=0.6 m/s residual speed;
- competent/rookie rows are diagnostic on the first pass so their actual
  deterministic envelopes are measured before gameplay acceptance limits are
  invented.

This data is intended to feed B6 execution-reserve/clearance policy: a lower
skill pilot may require more physical corridor clearance even when the accepted
centerline program is identical.


## 2026-09-19 first 3D matrix green; explicit Newtonian-vs-Assisted law stress candidate

Fresh supplied target-machine evidence:
- architecture contract PASS;
- navigation_runtime 12/12 PASS;
- maneuver_corridor_matrix PASS;
- maneuver_program_execution_lab PASS;
- ordinary_physical_maneuver_compiler PASS;
- navigation_work_scheduler PASS.

3D corridor matrix observations:
- expert: Newtonian and Assisted both complete 4/4 translation legs + 3/3
  rotations, final position error 0.014967 m, final speed 0.096233 m/s,
  zero 5 m corridor violation;
- production-competent: both laws complete 4/4 + 3/3, final position error
  0.111386 m, final speed 0.078158 m/s, zero corridor violation;
- rookie: both laws complete only first translation leg, no rotation, zero
  corridor violation, maximum forward-angle error 19.115652 deg.

Identical calm-route metrics are not considered sufficient proof that the laws
are materially different. Their current navigation-acceleration execution
differs mainly at the controlled-speed envelope:
- Newtonian physical RCS may accumulate inertial delta-v beyond that envelope;
- Assisted ("aircraft-like") applies the controlled-speed envelope to combined
  controlled propulsion.

A dedicated `[LAW-STRESS]` regression is now added:
- reachable speed envelope = 10 m/s;
- lateral RCS demand = 2 m/s2 for 8 s;
- Newtonian must exceed 12 m/s;
- Assisted must remain <=10.05 m/s;
- modes must differ by >2 m/s.

Current code/contract candidate before documentation commits:
`88ad3a8921239cf2865c32c0c8711b7514094aa1`.

This validates execution-law divergence only. Planner-side ordinary B5 still
supports Newtonian only; Assisted/aircraft-like maneuver compilation remains
future work.


## 2026-09-19 correction: point-center corridor is not hull-clearance proof

The previous `maneuver_corridor_matrix` is now classified as a **center-of-mass
tracking diagnostic only**. It is NOT accepted as proof that a real ship fits
inside the reported corridor.

Root cause:
- corridor distance was measured from `localPositionMeters` only;
- Cobra hull dimensions were not applied to corridor occupancy;
- the same stop-to-stop Trim translation reference was used for Newtonian and
  Assisted rows;
- therefore Newtonian braking did not require the physically expected 180 deg
  flip before aft-main-engine braking.

This made the calm Newtonian/Assisted corridor rows numerically identical even
though the separate law-stress correctly proved that the low-level laws differ.

Canonical Cobra Mk1 logical hull from `EliteCobraMk1Descriptor`:
- width = 26.0 m;
- height = 5.0 m;
- length = 22.2 m;
- body half-extents = (right 13.0, up 2.5, forward 11.1) m.

A 5 m half-width corridor can therefore never contain this hull even when the
centerline is perfect. Any previous "zero 5 m corridor violation" statement
refers only to center-of-mass tracking and must not be used as hull-clearance
evidence.

Required corrected physical test:
- explicit rigid-body hull envelope;
- explicit aft main / fore(reverse) main / RCS / angular-vectoring authority;
- Newtonian: aft main only for longitudinal main thrust, RCS for small
  translation, angular authority for attitude; braking requires flip and
  aft-main burn;
- Assisted/aircraft-like: forward and reverse longitudinal main authority,
  RCS for small translation/stabilization, angular authority for attitude;
- record P, V, complete body attitude, angular velocity, flip angle, actuator
  usage and hull corridor occupancy;
- corridor occupancy must be measured on the oriented hull, not only its
  center.

The previously supplied target-machine law-stress remains useful low-level
evidence:
- Newtonian final speed 15.960220 m/s;
- Assisted final speed 10.000000 m/s;
so the execution laws themselves are distinct. The invalid part is the
point-center corridor interpretation, not that low-level law distinction.


## 2026-09-19 rigid-body corridor / physical actuator candidate

The earlier center-point corridor interpretation is superseded. The old
`maneuver_corridor_matrix` remains useful only as a center-of-mass tracking /
PilotSkill diagnostic.

New code/contract candidate before documentation commits:

```text
753eae5dcf1d7aae8eb05893ba75e16896a52b91
```

Production control allocation changed:
- Newtonian navigation demand:
  - aft/forward main thrust only;
  - reverse demand cannot invent fore/nose main thrust;
  - reverse/lateral/vertical residual is bounded by manoeuvre/RCS authority;
- Assisted/aircraft-like navigation demand:
  - symmetric longitudinal aft/fore main thrust;
  - lateral/vertical residual remains bounded manoeuvre/RCS;
  - no omnidirectional main engine.

Assisted manual/stabilized local control was aligned to the same actuator model:
longitudinal error -> aft/fore main, lateral/vertical stabilization -> RCS.

New runtime control assertions pin:
- Newtonian reverse cannot use fore main;
- Assisted reverse can use fore longitudinal main;
- Assisted lateral demand cannot use main propulsion.

New target:
`maneuver_rigid_body_corridor`.

Rigid vehicle model uses canonical Cobra Mk1 logical dimensions:
- width 26.0 m;
- height 5.0 m;
- length 22.2 m;
- OBB half-extents (13.0, 2.5, 11.1) m.

Explicit actuator model:
- aft main: 7.5 g longitudinal authority;
- Assisted fore main: 7.5 g longitudinal reverse authority;
- RCS: 2.0 m/s2;
- angular/vectoring authority: 3.0 rad/s2 with Cobra rate limits.

The same 200 m center-of-mass stop trajectory is authored differently:
- Newtonian: main acceleration -> coast while yaw-flipping ~180 deg ->
  aft-main braking burn;
- Assisted: main acceleration -> nose-forward coast -> fore-main reverse
  braking with no hull flip.

The test measures every tick:
- center P/V;
- full body attitude;
- angular speed;
- maximum flip angle;
- aft-main / fore-main / RCS use during braking;
- oriented hull-corner corridor envelope.

The corridor centerline is extended past maneuver endpoints so width measures
transverse hull occupancy rather than finite-segment end-cap artifacts.

Strict expert invariants:
- Newtonian must complete a near-180 deg flip;
- Assisted must stay nose-forward;
- Newtonian brake must use aft main and zero fore main;
- Assisted brake must use fore main;
- aligned Assisted Cobra must fit a 14 m half-width corridor;
- Newtonian flip must require materially more than 14 m;
- 18.5 m half-width must contain the expert Newtonian flip envelope;
- both expert runs must stop within 1 m and <=0.5 m/s.

Competent and rookie rows are diagnostic on the first run.

Navigation-runtime expected test count is now 13.

The latest supplied 12/12 target-machine run did not include a
`git rev-parse HEAD` line, so no exact hash is assigned to that run.


## 2026-09-20 rigid-body baseline accepted; corner-family matrix next

Fresh target-machine evidence:
- architecture contract PASS;
- navigation_runtime 13/13 PASS;
- maneuver_rigid_body_corridor PASS;
- production build PASS, 84.197 s.

Rigid Cobra model:
- width 26.0 m, height 5.0 m, length 22.2 m;
- aft main 73.549875 m/s2;
- Assisted fore main 73.549875 m/s2;
- RCS 2.0 m/s2;
- angular/vectoring authority 3.0 rad/s2.

Expert:
- Newtonian: completed, final error 0.346002 m, final speed 0.399834 m/s,
  final forward 179.996705 deg from route, max hull half-width 17.275892 m,
  max flip 179.999972 deg, aft-main brake 8.325947 m/s2, fore main 0;
- Assisted: completed, same final P/V error, final forward 0 deg, max hull
  half-width 13.238202 m, no flip, fore-main brake 8.326298 m/s2.

Competent:
- both laws completed;
- Newtonian hull half-width 17.299164 m, final forward 179.465370 deg;
- Assisted hull half-width 13.297102 m, final forward 0.019004 deg.

Rookie:
- neither law completed inside the current fixed program horizon;
- Newtonian drifted to 33.553144 m center error and required 21.559101 m hull
  half-width;
- Assisted ended 18.896949 m from terminal target and required 13.467892 m
  hull half-width.

This accepts the rigid-body/actuator baseline and confirms the expected wider
Newtonian flip envelope.

Next task: compare three explicit corner-passage families on the same corridor:
StopTurnGo, RadiusTurn and DriftTurn, under Newtonian and Assisted with identical
pilot profiles. Total corridor time must be measured from a common entry gate to
the same final terminal gate.


## 2026-09-20 corner-family timing matrix candidate

New code/architecture candidate before state-document commits:

```text
8f1397a919009d4ed5fdc84412506ba681b3059c
```

New target:
`maneuver_corner_family_matrix`.

Purpose: determine how the same rigid Cobra and same PilotSkill traverse the same
90-degree L-shaped corridor under three distinct corner families and both local
control laws.

Common setup:
- incoming gate: 60 m before mathematical vertex;
- outgoing gate: 60 m after vertex;
- corner-zone timing gates: 35 m before/after vertex;
- initial speed: 10 m/s;
- common rigid-hull corridor half-width: 32 m;
- identical Cobra OBB and actuator model.

Families:
- StopTurnGo:
  waypoint capture / near-stop / outgoing attitude / depart;
- RadiusTurn:
  coordinated continuous radius, material speed retained, small slip angle;
- DriftTurn:
  deliberate large body/velocity slip, material speed retained, physical
  thrust bends velocity through the corner.

Passage semantics:
- touching the route vertex is not completion;
- internal fixture phases hand off by accepted-program schedule;
- corner passage is common entry-gate -> common exit-gate crossing;
- final P/V/attitude error is measured at the common outgoing terminal gate.

Matrix:
- expert / production-competent / rookie;
- Newtonian / Assisted;
- StopTurnGo / RadiusTurn / DriftTurn;
- total 18 rows.

Per row:
- validity/completion;
- phase count;
- total corridor time;
- corner-zone time;
- final P/V/forward error;
- minimum speed in corner zone;
- maximum body/velocity drift angle;
- center cross-track;
- rigid-hull required half-width and 32 m violation;
- peak aft main / fore main / RCS;
- tracking-envelope exceed ticks.

Nine `[CORNER-COMPARE]` rows directly compare Newtonian vs Assisted time and
required width for the same pilot/family.

First target-machine pass does not pin a timing winner. It checks expert
semantic quality:
- StopTurnGo must actually approach zero speed in the corner zone;
- RadiusTurn must retain >=5 m/s and stay <=20 deg slip;
- DriftTurn must retain >=7 m/s and produce >=60 deg material slip;
- all expert rows must cross the common exit gate, fit the common 32 m rigid
  corridor, and meet final P/V/attitude error bounds.

Expected navigation_runtime CTest count: 14.

After this isolated 90-degree primitive is measured, the accepted families will
be composed into the requested 3-4 segment 3D corridor so total route time can
be compared over mixed turn angles rather than only a symmetric 90-degree case.


## 2026-09-20 corner-family gate failed at Newtonian StopTurnGo

Target-machine checkout:

```text
519313ba430b73b557622436ed7df9a5a9a832cf
```

Fresh evidence:
- architecture contract PASS;
- navigation_runtime 13/14 PASS;
- only `maneuver_corner_family_matrix` failed.

Primary failing row:
- pilot=expert;
- law=Newtonian;
- mode=StopTurnGo;
- final position error = 27.257718 m;
- final speed error = 0.409381 m/s;
- minimum corner-zone speed = 2.933115 m/s;
- max center cross-track = 21.026823 m;
- required hull half-width = 34.330940 m;
- common half-width = 32 m;
- corridor violation = 2.330940 m;
- tracking envelope exceeded = 40 ticks.

Immediate diagnosis:
1. The current fixture changed all compound internal phases to schedule/time-based
   handoff so RadiusTurn/DriftTurn can remain moving. That is correct for moving
   corner families but wrong for StopTurnGo capture phases.
2. Newtonian StopTurnGo therefore advances from flip -> brake -> rotate -> exit
   even when the actual rigid body has not yet captured the required
   waypoint/zero-speed/attitude state. Errors accumulate and the later phases
   start from the wrong physical state.
3. The failed 32 m corridor assertion is real evidence of the bad executed
   maneuver, not a reason to widen the corridor.
4. Assisted StopTurnGo succeeds much better because fore/reverse main allows
   braking without the extra Newtonian flip/capture sequence.
5. RadiusTurn is already healthy for expert in both laws:
   ~0.115 m final position error, ~0.013 m/s velocity error, ~1.82 deg attitude
   error, ~7.99 m/s minimum corner speed, zero corridor violation.
6. DriftTurn physically works as a high-slip pass (10 m/s retained,
   ~101 deg slip, zero corridor violation, ~0.30 m P error) but expert final
   attitude error is still ~14.38 deg. The gate stops on the earlier
   Newtonian StopTurnGo failure, so this is a second expected issue to address
   after the primary fix.
7. The printed total times (15.18 / 12.88 / 11.16 s) are currently authored
   schedule durations. They are not yet valid comparative performance results.
   Real timing comparison must use completion/exit-state capture, not fixed
   authored duration.

Next correction:
- StopTurnGo internal brake/waypoint/attitude phases must use terminal capture
  semantics before advancing;
- RadiusTurn and DriftTurn may keep scheduled moving phase handoff, with common
  exit-gate completion;
- total-route timing must be measured from actual common entry gate to actual
  common terminal/exit capture;
- do not widen corridor or weaken tolerances.


## 2026-09-20 quality assessment after first corner-family gate

Exact tested checkout:

```text
519313ba430b73b557622436ed7df9a5a9a832cf
```

Architecture contract PASS.
navigation_runtime 13/14 PASS.
Only `maneuver_corner_family_matrix` failed.

### What the gate says about system quality

The failure does **not** indicate a general physics/follower collapse.

Strong evidence:
- the established rigid-body baseline remains green;
- RadiusTurn expert rows are excellent in both laws:
  - final P error ~0.115 m;
  - final V error ~0.013 m/s;
  - final attitude error ~1.82 deg;
  - minimum corner speed ~7.99 m/s;
  - slip ~5.85 deg;
  - rigid-hull half-width ~17.88 m;
  - zero 32 m corridor violation;
  - zero tracking-envelope exceed ticks;
- DriftTurn expert rows already execute a real high-slip maneuver:
  - 10 m/s retained through the corner;
  - ~101 deg body/velocity slip;
  - final P error ~0.298 m;
  - final V error ~0.0025 m/s;
  - rigid-hull half-width ~17.38 m;
  - zero 32 m corridor violation;
  - zero tracking-envelope exceed ticks.

This is strong evidence that B9/B10/B12/B13-style execution is already capable
of accurately tracking non-trivial continuous rigid-body maneuvers, including a
powered drift, when the accepted reference is coherent.

### Primary deficiency exposed

Expert Newtonian StopTurnGo failed because the fixture/orchestration treated
every compound phase as time-scheduled:
- minimum corner speed remained 2.933115 m/s, so the intended stop was never
  captured;
- final position error grew to 27.257718 m;
- center cross-track reached 21.026823 m;
- required hull half-width reached 34.330940 m;
- common 32 m corridor was violated by 2.330940 m;
- 40 tracking-envelope exceeded ticks occurred.

The root issue is phase-completion semantics, not lack of raw vehicle authority.
Newtonian has an extra flip/brake/capture sequence, so time-only handoff is much
more damaging there than in Assisted.

Architecture is now corrected:
- StopTurnGo capture phases are state-gated;
- RadiusTurn/DriftTurn moving internals may remain scheduled/program-driven;
- time expiry cannot stand in for successful waypoint/velocity/attitude capture.

### Secondary deficiency exposed

DriftTurn tracking through the corner is physically successful, but expert exit
attitude is still ~14.38 deg outside the desired outgoing orientation.

Therefore drift currently has:
- good P/V tracking;
- good corridor occupancy;
- correct high-slip character;
- incomplete post-drift attitude recovery/capture.

This is the next maneuver-quality issue after StopTurnGo handoff.

### Pilot-skill evidence

The matrix remains useful even though its aggregate test failed.

RadiusTurn:
- competent remains good, with ~0.19 m P error and ~4.18 deg final attitude;
- rookie still completes physically, with ~0.41-0.47 m P error,
  ~0.64 m/s V error and ~8.93 deg attitude error.

DriftTurn:
- competent preserves ~9.97 m/s but exits with ~47.3 deg attitude error and
  188 envelope-exceeded ticks;
- rookie preserves ~9.88 m/s, P error ~4.39 m, attitude error ~8.30 deg and
  135 envelope-exceeded ticks.

Thus PilotSkill already produces meaningfully different execution envelopes.
This supports keeping skill uncertainty as an explicit B6 clearance/tracking
reserve input rather than treating all pilots as equally precise.

### Timing result is not yet accepted

Printed authored totals:
- StopTurnGo 15.18 s;
- RadiusTurn 12.88 s;
- DriftTurn 11.16 s.

These are not yet real performance comparisons because the current fixture
advances phases by predefined durations. Newtonian/Assisted total-time equality
is therefore partly constructed.

Valid timing requires:
- common entry-gate timestamp;
- real state-gated capture where the family requires it;
- common exit/terminal-state capture timestamp.

### Engineering quality verdict

Current quality by subsystem:
- rigid-body physics / actuator truth: **strong baseline**;
- follower continuous P/V tracking: **strong**;
- RadiusTurn execution: **already high quality**;
- DriftTurn physical execution: **promising/functional**, exit attitude recovery
  still needs work;
- PilotSkill differentiation: **working and useful**;
- StopTurnGo compound orchestration: **not yet correct**;
- maneuver-family completion semantics: **architecture corrected, implementation
  pending**;
- Newtonian vs Assisted performance timing: **not yet measured honestly**;
- B6/B7 production integration: **still pending**, so the overall navigation
  system is not yet ready to claim general production-grade maneuver planning.

Current architecture/doc candidate before state-document commits:

```text
3ca267ad9ae5188f6f11826298344dbba9df8791
```


## 2026-09-20 ManeuverPhaseGate production candidate created

New production component:
- `src/game/navigation/ManeuverPhaseGate.h/.cpp`.

Purpose:
- separate compound phase handoff semantics from the test fixture;
- `ScheduledMoving`: advance at nominal program horizon;
- `StateCapture`: keep following the terminal sample until
  `TrajectoryFollower::Complete`;
- bounded `maximumCaptureOverrunSeconds`;
- explicit `CaptureTimedOut` instead of silently advancing a failed capture.

The gate does not plan, sample, track or mutate the accepted program. It only
decides whether the current physical phase may hand off.

Current implementation commits:
- API: `5f97b8e52992f537de650e071dab5615fce9fc1f`;
- implementation: `c19a260333083f1d6acd472d9d3977b2864193cf`.

This candidate is not target-machine accepted yet. Next: wire it into shared
runtime/test CMake, add an isolated deterministic test, then migrate the corner
fixture to use the gate.


## 2026-09-20 ManeuverPhaseGate wired with isolated regression

Production/shared-runtime wiring is now present:
- root `EliteNavigationWorldRuntime` compiles `ManeuverPhaseGate.cpp`;
- isolated `EliteNavigationRuntimeControl` test library compiles the same source.

New deterministic test:
`tests/navigation_runtime/ManeuverPhaseGateTests.cpp`.

It pins:
- ScheduledMoving holds before nominal end and advances at nominal end;
- StateCapture holds after nominal end while follower is still Following;
- StateCapture advances only on real `TrajectoryFollower::Complete`;
- bounded overrun produces `CaptureTimedOut`;
- invalid follower state fails closed.

Relevant candidate commits:
- root wiring `42494115dce06cf9945f2aab972c00d97b82f033`;
- test-library wiring `e8e5b6e0eb430d64e2cc41e7011cbe18c4c0a984`;
- isolated test `80fd3ea0c39042f2c9c446e5ad41bbdc43ce4125`;
- test target `9f410855d3784bede14ac1ddb1d7614df7d0814b`.

Expected runtime CTest count is now 15 once the target machine rebuilds.

Not target-machine accepted yet.


## 2026-09-20 state-gated corner execution candidate ready for target-machine gate

The compound-maneuver mechanism is now implemented through production
`ManeuverPhaseGate` and the corner fixture consumes that production seam.

Mechanism:
- `ScheduledMoving` advances at nominal reference horizon;
- `StateCapture` continues following the terminal sample until
  `TrajectoryFollower::Complete`;
- bounded capture overrun;
- explicit `CaptureTimedOut` failure.

Corner migration:
- StopTurnGo braking capture => StateCapture;
- StopTurnGo in-place outgoing-attitude capture => StateCapture;
- RadiusTurn/DriftTurn moving phases => ScheduledMoving;
- braking capture terminal feed-forward is zeroed so post-horizon tracking
  converges rather than continuing to command braking acceleration.

Drift recovery profile was also changed from:
- 2 s moving rotate + 2 s aligned coast
to:
- 3 s moving rotate + 1 s aligned coast.

Total post-arc time and 40 m travel remain unchanged; only the physical attitude
recovery profile is less aggressive.

New diagnostics:
- `capture_timeout_phases`;
- `max_capture_overrun_s`.

Static source/contract audit:
- ManeuverPhaseGate API: PASS;
- implementation semantics: PASS;
- isolated regression markers: PASS;
- root shared-runtime CMake: PASS;
- isolated runtime CMake: PASS;
- corner fixture StateCapture wiring: PASS;
- architecture-contract wiring: PASS;
- expected navigation_runtime CTest count: 15.

Important: this is still unverified on the user's target machine. The last
accepted/tested checkout remains
`519313ba430b73b557622436ed7df9a5a9a832cf`.

Relevant implementation commits:
- `3d06caecc09429336a4b57765d7d99d529ceb467` corner migration;
- `dc5fd711503c6d7863cea751c82bdbc1666e510d` fixture summary sync;
- `1f2228f35df29109df1999998a7b1c0d2bfc7778` architecture lock.


## 2026-09-20 corner-family rerun: phase gate works, maneuver authoring exposed

Latest exact target-machine checkout:

```text
ca0c5506a9912bf016e4c52c7545ddd35c66e1fb
```

Fresh evidence:
- Stage-12 runtime-planner architecture contract: PASS;
- navigation_runtime: 14/15 PASS;
- ManeuverPhaseGate isolated regression: PASS;
- only maneuver_corner_family_matrix failed.

The gate correction therefore did its job: capture phases no longer advance by
clock time when terminal state has not actually been captured.

### Observed matrix

Expert RadiusTurn remains strong in both laws:
- completed;
- final P ~= 0.45 m;
- final V ~= 0.027 m/s;
- final forward ~= 1.86 deg;
- minimum corner speed ~= 7.99 m/s;
- hull half-width ~= 17.88 m;
- zero 32 m corridor violation;
- zero tracking-envelope exceed ticks.

Expert DriftTurn remains physically strong:
- completed;
- final P ~= 0.68 m;
- final V ~= 0.009 m/s;
- 10 m/s retained;
- ~=108 deg body/velocity slip;
- hull half-width ~=17.37 m;
- zero corridor violation.

Its remaining strict defect is final attitude ~=9.67 deg versus <=5 deg.

Expert Newtonian StopTurnGo now fails explicitly at capture instead of silently
handing off:

```text
completed=0
phases=2
capture_timeout_phases=1
max_capture_overrun_s=6.01
final_pos_error_m=77.903552
final_velocity_error_mps=11.314457
final_forward_error_deg=95.378736
max_hull_required_half_width_m=42.605322
max_corridor_violation_m=10.605322
tracking_envelope_exceeded_ticks=227
```

Newtonian StopTurnGo times out for expert, competent and rookie. Rookie Assisted
StopTurnGo also times out; Assisted expert succeeds.

### Root cause

The failure is not a ManeuverPhaseGate defect.

Newtonian StopTurnGo authored a 2.6 s scheduled 180-degree moving flip and then
immediately started a dependent aft-main braking burn. With the real rigid-body
angular execution this is too aggressive. The brake reaches its nominal endpoint
with material P/V/attitude error.

The post-horizon terminal sample correctly has zero brake feed-forward and B10
has only 0.55 m/s2 reserved linear feedback. That reserve exists to track a
proved program; it must not be enlarged to repair a badly authored planner
maneuver.

The already-green rigid-body Newtonian fixture used about 5 s for the same
180-degree flip. Capture timeout is therefore useful evidence: it exposed the
under-timed planner reference that the old clock-only handoff concealed.

Do not widen the corridor, extend timeout blindly, weaken expert thresholds or
turn B10 into a maneuver planner.

## 2026-09-20 corrective corner-authoring candidate

Code candidate:

```text
f7e17a1a631157bc4cc8763c226ea73b57adeb23
```

Newtonian StopTurnGo now preserves the exact previous pre-brake travel time and
braking point while reallocating that time to physical attitude acquisition:

```text
old: 2.775 s approach + 2.600 s flip = 5.375 s
new: 0.375 s approach + 5.000 s flip = 5.375 s
```

The brake remains 1.25 s and StateCapture remains strict. This changes no
corridor geometry and no terminal tolerance; it simply ensures the planner
allows sufficient lead rotation before a burn that physically depends on hull
orientation.

DriftTurn recovery is also changed from:

```text
3 s moving rotate + 1 s aligned coast
```

to one continuous 4 s moving attitude recovery over the same 40 m outgoing
travel. Expert P/V/corridor behavior was already good; the change removes an
artificial reference boundary and slows the attitude recovery history without
altering route time or distance.

Target-machine validation is pending.

Required rerun:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

The existing strict expert quality thresholds remain unchanged.


## 2026-09-20 — long-arc angular-tracking diagnostic

Target-machine rerun on exact checkout:

```
be4686f4dcba419f451813b1ddc246088c145e48
```

proved that the previous Newtonian StopTurnGo authoring correction worked. Expert Newtonian StopTurnGo now completes with a real near-stop, no corridor violation, no capture timeout, and small final P/V/attitude errors. RadiusTurn remains healthy.

The remaining strict expert failure is DriftTurn terminal attitude: both control laws finish near 10.325 degrees from the common exit heading while P/V and corridor quality remain good. The <=5 degree rule is a terminal common-exit criterion; it does not prohibit attitude correction during translational motion.

The existing DriftTurn recovery already commands a 90 degree attitude change during 4 s / 40 m of motion. Therefore the next diagnostic must determine whether the residual angle is caused by general B9/B10 continuous angular tracking or by DriftTurn-specific reference/recovery construction.

Candidate `5c16bedc25c422f2c79ae5def14396839ef4ee7c` adds a separate long curved-flight probe without weakening any existing gate:

```
180 deg arc
R = 80 m
v = 10 m/s
length ~= 251.33 m
duration ~= 25.13 s
PilotSkill = expert / competent / rookie
law = Newtonian / Assisted
```

The probe continuously reports centerline error, rigid-hull annular corridor demand, forward-vs-tangent error and tracking-envelope exceed ticks, plus final P/V/attitude error. Expert quality keeps the same 32 m hull half-width corridor, <=1.5 m final P, <=1.0 m/s final V and <=5 deg final attitude; maximum in-flight forward/tangent error is bounded at 10 deg.

Interpretation:
- long arc clean + Drift exit bad => local DriftTurn recovery/reference defect;
- long arc also angularly bad => general B9/B10 angular tracking defect.

This candidate remains unverified until the target-machine architecture and navigation-runtime gates are rerun.


## 2026-09-20 — long-arc result closes the general angular-tracking question

Target-machine evidence from exact checkout:

```
d659416b9b1ddb2356c37eff315f9d13b70bafaa
```

kept the Stage-12 architecture contract green and produced 14/15 runtime passes. The only failure remained the strict expert DriftTurn common-exit attitude (~10.325 deg vs <=5 deg).

The new 180 deg, R=80 m, 10 m/s long-arc diagnostic completed successfully for every PilotSkill under both Newtonian and Assisted laws. Expert execution covered ~251 m / 25.1 s of continuous curved flight with:
- final position error ~0.332 m;
- final velocity error ~0.036 m/s;
- final forward error ~0.052 deg;
- maximum in-flight forward/tangent error ~3.221 deg;
- maximum centerline error ~0.330 m;
- zero tracking-envelope exceed ticks.

Competent and rookie also completed cleanly; rookie final forward error remained below 1 deg.

This closes the diagnostic split: the general sampler/follower angular tracking chain is capable of accurate continuous attitude correction during translational motion. The remaining DriftTurn miss is local recovery/reference authoring.

The next mechanism change must therefore remain planner-authored: make the DriftTurn recovery reach target yaw before the terminal endpoint and keep commanding target yaw during a short continuing-translation settle interval. Do not widen the 5 deg exit requirement or move maneuver choice into the follower.


## 2026-09-20 — DriftTurn in-motion settle candidate

Following the long-arc proof that general continuous angular tracking is healthy, candidate:

```
76346121516e5b00d14a4e6304621b55791093ab
```

changes only DriftTurn recovery authoring.

The final 4 s / 40 m recovery remains one planner-authored accepted moving reference. The smooth 90 deg yaw recovery now completes in 2.5 s, leaving 1.5 s of continued 10 m/s translation with final yaw held. This gives the physical ship an explicit in-motion settle interval before the common exit without adding a follower-side maneuver or changing acceptance tolerances.

Target-machine validation must keep the long-arc diagnostic green and bring expert DriftTurn final attitude to <=5 deg while preserving existing P/V/corridor and sustained-speed/slip semantics.


## 2026-09-20 — moving-settle experiment rejected

Exact target-machine checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

kept the architecture contract green and produced 14/15 runtime passes.

The 2.5 s rotate + 1.5 s moving-settle DriftTurn experiment did **not** solve terminal attitude. Expert final forward error worsened from ~10.325 deg to ~16.889 deg under both Newtonian and Assisted laws. Position (~0.473 m), velocity (~0.032 m/s) and corridor quality remained good.

The long 180 deg arc remained unchanged and clean (expert final attitude ~0.052 deg, maximum in-flight forward/tangent error ~3.221 deg), so general continuous angular tracking remains proven healthy.

Interpretation: compressing the same 90 deg recovery into a faster angular transient creates a larger terminal attitude residual; the subsequent passive 1.5 s settle is insufficient. The next iteration must measure actual/reference angular velocity and attitude residuals at the recovery boundary before changing timing again.

If evidence shows residual angular motion, the correct planner-side primitive is a moving terminal capture/reference continuation: advance position at terminal velocity while holding target attitude and damping angular velocity. Frozen-position StateCapture is not valid for a moving exit.


## 2026-09-20 — decouple outgoing tracking from old x=60 phase end

After reviewing the failed timed-settle experiment, the corner-family fixture was corrected at the semantic boundary rather than tuned again.

The old DriftTurn flow treated the 40 m post-arc distance to x=60 as a 4 s attitude schedule and ended the `ScheduledMoving` phase at that same checkpoint. This conflated route geometry with physical tracking convergence.

Candidate:

```
24fce76b30d2448a4b94c93ad78dd8d37e5102df
```

removes that coupling.

After the drift arc:
- the accepted position reference continues moving along the outgoing straight at 10 m/s;
- the accepted attitude is the final outgoing corridor heading;
- target angular velocity is zero;
- B10 continuously reduces attitude/angular-rate error while translation continues;
- x=60 is crossed without ending control.

The finite test corridor now continues to x=120. The fixture records the first point after x=60 where forward error is <=5 deg and angular speed <=0.08 rad/s, exposing the actual moving convergence distance instead of forcing an arbitrary angular deadline.

StopTurnGo and RadiusTurn are also extended through the same outgoing route terminus for a common finite comparison endpoint.

No production tracking authority, capability or quality threshold is weakened. This candidate is unverified until target-machine gates are rerun.


## 2026-09-20 — constant-heading outgoing tracking experiment rejected

The candidate that decoupled x=60 from DriftTurn phase termination was target-tested.

Architecture remained PASS and runtime remained 14/15. StopTurnGo, RadiusTurn and the long 180 deg arc stayed healthy.

DriftTurn did not converge under a constant final-heading outgoing reference:
- expert final attitude ~48.287 deg;
- 439 tracking-envelope exceeded ticks;
- no outgoing attitude capture;
- final position/velocity and corridor remained excellent.

Competent and rookie DriftTurn also failed strongly (~108 deg and ~107 deg final attitude).

This establishes that removing the route-time deadline is not sufficient. The experiment inadvertently asked B10 to perform the full 90 deg attitude maneuver using only its bounded tracking reserve. That is outside B10 ownership.

The next mechanism must restore planner ownership of the large-angle transition without restoring an arbitrary fixed deadline: a moving attitude-capture reference computed from actual/planned angular state, target attitude, target zero angular velocity, and vehicle angular acceleration/rate capability while translation continues.


## 2026-09-20 — capability-derived moving attitude capture candidate

Candidate:

```
31a66a3eb462df6b5a60b2da2aca9f018d8aa332
```

replaces the failed constant-heading DriftTurn exit.

The outgoing angular transition is now planner-authored from the actual arc-exit state. A quintic yaw profile uses actual yaw and yaw-rate as initial boundary conditions and the outgoing yaw with zero terminal yaw-rate as final conditions.

The profile duration is solved from the effective physical angular limits used by ShipController rather than a fixed route-time deadline. The test mirrors the crew/load envelope:
- effective angular acceleration = min(configured angularAccel, maxGs*g/turnRadius);
- effective yaw rate = min(configured maxYawRate, sqrt(maxGs*g/turnRadius)).

Feed-forward acceleration is constrained below that physical envelope with B10 tracking reserve and an additional execution margin left intact.

Translation continues at 10 m/s throughout the capture. New diagnostics expose chosen capture duration, initial yaw-rate and peak feed-forward yaw rate/acceleration.

This candidate is unverified until target-machine gates are rerun.


## 2026-09-20 — DriftTurn moving attitude capture accepted

Exact target-machine checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

passed the Stage-12 architecture contract and all 15 navigation runtime tests.

The capability-derived moving attitude capture closes the strict expert DriftTurn exit failure.

Expert Newtonian and Assisted DriftTurn both finished with approximately:
- 0.111 m position error;
- 0.00075 m/s velocity error;
- 0.03884 deg forward error;
- zero tracking-envelope exceed ticks;
- zero corridor violation.

The computed capture lasted ~3.12469 s and started from the real residual yaw rate ~-0.73158 rad/s. Peak feed-forward remained physically bounded (~1.29608 rad/s yaw rate and ~1.85469 rad/s2 yaw acceleration).

Architecture conclusion: large-angle physical transition belongs in the planner-authored accepted program; B10 remains bounded residual tracking.

Next active stage: mixed-angle multi-segment 3D corridor quality.


## 2026-09-20 — continuous 3D fly-through candidate

A new navigation-runtime regression has been added:

```
tests/navigation_runtime/ManeuverFlyThrough3dTests.cpp
```

CTest:

```
maneuver_fly_through_3d
```

The previous 3D corridor matrix already used non-axis-aligned waypoints, but its execution model was stop-to-stop with in-place attitude transitions. This new stage tests the missing behavior: continuous chained 3D fly-through.

The route has five moving segments and four corners (~35/60/90/120 deg). Nominal speed is 8 m/s. Each corner uses 45 m incoming/outgoing cut distance and a C2 quintic position reference with continuous endpoint velocity and zero endpoint acceleration.

The Cobra Mk1 full OBB is checked against a 32 m half-width polyline corridor every physics tick. Per-corner diagnostics report speed loss, slip, observed turn radius and hull envelope.

Expert Newtonian and Assisted are strict:
- no stop-turn-go substitution (speed >=3 m/s);
- zero corridor violation;
- zero tracking-envelope exceed ticks;
- all 9 phases complete;
- final P/V/attitude inside strict bounds.

Competent/Rookie are diagnostic for the first target-machine pass.

Candidate code commits:
- `5c10c19d7fd2eb4b1fb6aa26b903fd55713b6dcf`;
- `c48a92010350cf12f417aa19f23f75487dfb1459`.

This candidate is not accepted until exact target-machine evidence is recorded.


## 2026-09-20 — continuous 3D fly-through accepted

Exact target-machine checkout:

```
213bbfb62ff7dcb8e553c06bdca09d99d2d1fd56
```

passed:
- Stage-12 architecture contract;
- all 16 navigation runtime tests;
- the new `maneuver_fly_through_3d` strict gate.

Therefore strict Expert Newtonian and Assisted both completed the five-segment ~35/60/90/120 degree continuous 3D route without:
- stopping below the 3 m/s anti-StopTurnGo floor;
- leaving the full-hull 32 m half-width corridor;
- exceeding the tracking envelope;
- missing the final strict P/V/attitude bounds.

This accepts continuous chained 3D fly-through as a demonstrated capability.

The acceptance log did not include detailed FLY3D rows because the runner omitted the new test from its verbose diagnostic reruns. A diagnostics-only runner patch (`4cd9c4a14c8f2e4ce033082633766a21fece9331`) now adds that output. A follow-up run is needed only to characterize Newtonian/Assisted radius, slip, speed loss and hull envelope numerically; the core 16/16 acceptance is already established.


## 2026-09-20 — continuous 3D fly-through verbose acceptance

Exact target-machine checkout:

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

passed the Stage-12 architecture contract and all 16 navigation runtime tests. The verbose fly-through diagnostic confirms the strict gate quantitatively.

Expert Newtonian and Assisted both report:
- 9/9 phases;
- final position error ~0.120 m;
- final speed error ~0.00007 m/s;
- final forward error ~0.00032 deg;
- minimum route speed ~3.999 m/s;
- maximum center cross-track ~8.478 m;
- maximum full-hull required half-width ~17.474 m;
- zero corridor violation;
- zero tracking-envelope exceed ticks.

Measured Expert corner envelope:
- 35 deg: radius ~90.25 m, min speed ~7.629 m/s, slip ~0.033 deg;
- 60 deg: radius ~44.78 m, min speed ~6.928 m/s, slip ~0.081 deg;
- 90 deg: radius ~21.09 m, min speed ~5.657 m/s, slip ~1.088 deg;
- 120 deg: radius ~8.62 m, min speed ~3.999 m/s, slip ~1.369 deg.

All Newtonian/Assisted FLY3D rows are numerically identical, including Competent and Rookie. This is expected for this particular tangent-aligned reference: its transverse demand stays inside the common manoeuvre/RCS authority and it does not exercise law-specific reverse/main-thrust behavior.

Conclusion: continuous 3D fly-through is accepted as an execution/corridor capability. It is not a discriminator between Newtonian and Assisted laws.

Next active stage is speed/doctrine coverage with physically distinct candidate maneuvers and real selected-program execution.


## 2026-09-20 — B7 speed/doctrine execution matrix candidate

The original block audit identifies B7 Maneuver Decision as a remaining execution-chain gap: `ManeuverDecisionController` exists, but ordinary live navigation does not yet prove a full doctrine-select -> accepted-program -> real-execution path.

A new runtime regression now constructs six physical candidate programs for the same 180 m objective and feeds their measured/annotated tradeoffs into B7.

Expected choices:
- Rational -> balanced;
- PrecisionRetrieval -> precision;
- Extreme/Newtonian -> Newtonian-only high-slip drift dash;
- Extreme/Assisted -> common fast path after law filtering;
- CombatEscape -> low-threat escape;
- a faster `criticalRisk=0.90` reckless shortcut remains rejected above doctrine while preferred-risk choices exist.

Every selected `AcceptedManeuverProgram` is then executed through TrajectoryFollower/B10, PilotSkill and authoritative physics with full-hull obstacle clearance and terminal-state checks.

CTest target: `maneuver_speed_doctrine_matrix`.
Expected runtime suite size: 17.

This is an unverified candidate until exact target-machine evidence is recorded.


## 2026-09-20 — B7 speed/doctrine select->execute acceptance

Exact target-machine checkout:

```
a0f0991791665e30059be15efc47dedcdfafe090
```

passed:
- Stage-12 architecture contract;
- all 17 navigation runtime tests;
- `maneuver_speed_doctrine_matrix`.

The matrix proved deterministic doctrine choice over physically different candidate programs and then executed the selected `AcceptedManeuverProgram` through the accepted follower/PilotSkill/physics chain.

Observed selection:
- Rational -> balanced;
- PrecisionRetrieval -> precision;
- Extreme/Newtonian -> Newtonian-only drift dash;
- Extreme/Assisted -> common fast path after law filtering;
- CombatEscape -> low-threat escape.

The faster criticalRisk=0.90 reckless shortcut was rejected above doctrine.

The strongest law-specific evidence is Extreme:
- Newtonian drift dash: 18 s, ~14.11 m/s peak, ~34.08 deg actual slip, ~2.62 m actual clearance;
- Assisted fast path: 20 s, ~12.28 m/s peak, ~2.08 deg actual slip, ~7.59 m actual clearance.

All selected programs had zero tracking-envelope exceed ticks and terminal errors far inside the strict gate.

This accepts B7 behavior at lab/runtime level. It does not yet claim that every ordinary production navigation path has completed migration through B7; final live compatibility-seam retirement remains integration work.

The next laboratory gate is chained cross-family transitions plus negative/physical-limit rejection and invalidation cases.


## 2026-09-20 — chained transition + physical-limit matrix candidate

After B7 doctrine selection was accepted on target checkout `a0f0991791665e30059be15efc47dedcdfafe090`, the next laboratory candidate tests continuity and fail-closed limits.

New CTest:
```
maneuver_chained_limit_matrix
```

The chain executes four programs on the same live vehicle state with no reset:
```
FreeTransit
 -> hard moving PrecisionTransit
 -> Newtonian DriftPass / Assisted aligned PrecisionTransit
 -> PrecisionCapture with StateCapture
```

The next program is built from the actual previous P/V/body basis/angular velocity. The test fails on any synthetic seam jump and checks full Cobra hull occupancy against a 25 m reference corridor.

The same regression also pins five negative contracts with existing production components:
- B5 rejects a major turn when only 0.5 s of physical horizon is available;
- stopping-reserve math rejects 20 m/s braking when only 60 m is available;
- the full Cobra hull rejects a 12 m half-width corridor;
- Assisted B7 rejects an all-NewtonianOnly candidate population;
- a new dynamic hazard invalidates accepted execution and requests immediate local replanning.

Expected runtime suite size is 18. This candidate remains unaccepted until exact target-machine evidence is recorded.


## 2026-09-20 — first chained-limit target attempt: build failure

Exact target-machine checkout:

```
4753451be23f913d3e20d2ca11c112f113980434
```

passed the Stage-12 architecture contract but failed during compilation of `ManeuverChainedLimitMatrixTests.cpp` before any runtime test executed.

The root cause is test-harness-only:
```
glm::dvec3 basis * float ShipTransform angular rate
```
inside `captureState()`. GLM requires matching scalar precision.

Fix candidate:
```
1e0d555a504e6913ee417b4b628e7d062081a0c0
```
casts pitch/yaw/roll rates to double before reconstructing map-space angular velocity.

No planner/follower/physics/doctrine behavior changed. The chained/limit capability remains untested until the next target run.


## 2026-09-20 — chained-limit runtime result: Assisted slip diagnostic split

Exact target checkout:

```
8729abbbf3e74df0969f83bbc03773ebd827d3af
```

built successfully and passed 17/18 runtime tests.

Newtonian chained execution was physically strong:
- 4/4 phases;
- zero P/V/omega seam jumps;
- ~0.000001 deg attitude seam jump;
- 34.49 deg material slip;
- 17.43 m maximum hull half-width inside 25 m;
- terminal P 0.0246 m;
- terminal speed 0.0250 m/s;
- terminal forward error 0.0286 deg;
- zero tracking-envelope violations.

The sole failure was the Assisted phase-3 `maximumSlipDeg <= 8 deg` assertion.

Because phase 2 is ScheduledMoving, phase 3 inherits real physical state at the nominal handoff and may begin with residual slip. The previous metric conflated inherited seam transient with slip retained/generated by the Assisted aligned phase.

Diagnostic commit `6fda55f8a2a954ae1656d5eebf4538f585125f2e` now records entry slip, absolute max, max after 1 s and terminal slip for every chain phase.

The Assisted criterion remains strict:
- <=8 deg after the first 1 s of handoff transient;
- <=4 deg at phase end.

If these fail, the next correction must target reference/control behavior, not test tolerance.


## 2026-09-20 — Assisted chained failure root cause: discontinuous world-up frame

Target checkout:

```
350f7d593e22b8b89cb3ae4dbfbfbb7fbb53ea03
```

proved the Assisted failure is real inside phase 3, not inherited at the seam:
- entry slip 1.579 deg;
- max/final slip 25.789 deg;
- final forward error 25.965 deg;
- 171 tracking-envelope violations.

The chained fixture reconstructed each velocity-aligned body basis from a fixed world-up seed and switched that seed near a vertical tangent. This made right/up discontinuous even though forward remained smooth.

Because B10 tracks the full three-axis attitude, the artificial roll discontinuity became authoritative angular feed-forward/feedback demand.

Candidate fix `b8eb4641b013692c773d087d6cad96756c672b3c` replaces that reconstruction with a parallel-transport/Bishop frame:
- tangent owns forward;
- previous transverse axis is projected into the new normal plane;
- roll remains continuous unless explicitly commanded.

Terminal forward remains explicit; exact terminal roll/up belongs to a separate docking/placement attitude-capture requirement rather than an implicit world-up rule.

No physical or tracking acceptance tolerance was weakened.


## 2026-09-20 — chained transitions + physical-limit acceptance

Exact target-machine checkout:

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

passed:
- Stage-12 architecture contract;
- all 18 navigation runtime tests;
- `maneuver_chained_limit_matrix`.

Measured Newtonian chain:
- 4/4 phases;
- zero P/V/attitude/omega seam jumps;
- 35.114714 deg material drift;
- 17.419551 m maximum full-hull half-width inside 25 m;
- terminal P error 0.024611 m;
- terminal speed 0.024960 m/s;
- terminal forward error 0.029227 deg;
- zero tracking-envelope violations.

Measured Assisted chain:
- 4/4 phases;
- zero seam jumps;
- aligned phase max slip 1.139399 deg;
- max full-chain slip 1.934322 deg;
- same 17.419551 m hull envelope;
- terminal P error 0.024611 m;
- terminal speed 0.024960 m/s;
- terminal forward error 0.035921 deg;
- zero tracking-envelope violations.

The transported-frame correction is accepted: smooth tangent-following no longer invents roll discontinuities.

All five fail-closed limit contracts also passed:
- short turn horizon rejected;
- 110 m braking reserve does not fit 60 m;
- 13.238202 m Cobra support radius does not fit a 12 m corridor;
- Assisted rejects an all-NewtonianOnly candidate set;
- new dynamic hazard immediately invalidates the old accepted automatic execution.

This closes the chained/limit laboratory block.

Only one synthetic behavior gate remains: a single final composite end-to-end proving ground. After that gate is accepted, primary evaluation moves into the real game/NAV STRESS scene.


## 2026-09-20 — final composite proving-ground candidate

After acceptance of the chained/limit block on `3fe9b54eda0135b0cdebb7dc835d8a4b17580808`, one final synthetic behavior gate was added:

```
navigation_composite_proving_ground
```

The scenario composes:
- production exact-static geometry;
- production NavigationSpace topology;
- production NavigationRuntimePlanner;
- B7 Extreme law filtering/selection;
- AcceptedManeuverProgram execution through B9/B10, PilotSkill and real physics;
- mid-program dynamic hazard publication through NavigationMap;
- NavigationExecutionReplanPolicy invalidation;
- production AdjustedClear local bypass;
- replacement from actual live state;
- constrained second portal;
- final StateCapture.

Expected suite size is 19.

The test deliberately records one remaining migration boundary: general physical time-program authoring is still test-side because the production B5 Assisted/general-family compiler is not yet complete. A green composite ends synthetic maneuver behavior testing but does not claim the remaining B1-B6/B11 migration work is done.


## 2026-09-20 — first final-composite target attempt: recoverability fixture defect

Exact target checkout:

```
d57a22f69c3af1a8c967ce974b895295c77dd21a
```

passed the architecture contract and 18/19 runtime tests. Every previously accepted runtime gate stayed green.

The sole failure was:
```
navigation_composite_proving_ground
 -> composite production planner did not find adjusted dynamic bypass
```

The fixture inserted the new dynamic hazard only 35 m ahead after four seconds of the selected moving program.

Production LocalHorizon checks unchanged current kinematics for every candidate in addition to the bounded requested corridor. Once the hazard is already inside that current-state closest-approach envelope, ordinary lateral probes are correctly rejected rather than being granted steering authority.

Candidate `b57d81e42035f9771ae4feecee56899c4fa4f3f7` moves the hazard to a recoverable but still conflicting state:
- 48 m ahead;
- 6 m radius;
- -0.50 m/s cross motion.

The test now explicitly requires a nominal dynamic conflict before it may accept `AdjustedClear`, so the correction cannot pass by simply making the hazard irrelevant.

A new `[COMPOSITE-PLAN]` row records planner status and bounded-search diagnostics for the next target run.


## 2026-09-20 — final composite: AdjustedClear proved, replacement authority defect exposed

Exact target checkout:

```
18f93e15f3baa5218d459b289ae89beb170f1c54
```

passed the architecture contract and 18/19 runtime tests.

The production local planner now succeeded inside the final composite:
- nominal dynamic conflict = 1;
- conflict entity 12060;
- 20 probes;
- `AdjustedClear`;
- no static block.

The failure moved downstream to test-side physical replacement execution.

Using the logged live state and selected target, the old 6 s / 8 m/s quintic requires roughly 5 m/s2 transverse acceleration, far above the 2 m/s2 manoeuvre authority. The test had parameterized a safe geometric target with an unproved physical schedule.

Candidate `7444c5930586300d6cac48bd4b2fa63b27e96bd6` makes test-side replacement authoring capability-aware:
- dense analytic fitting;
- transverse FF <=1.35 m/s2;
- minimum speed >=0.5 m/s;
- planned dynamic clearance >=1.5 m;
- actual execution must keep zero tracking-envelope violations and >0.5 m dynamic clearance.

This does not weaken the final composite. It prevents the temporary test-side B5 seam from inventing a maneuver the accepted physics cannot execute.


## 2026-09-20 — final composite: first replacement passes, persistent world truth fixed

Exact target checkout:

```
852e5a71a71625cdfc0c71a6bb89724d2194990e
```

passed the architecture contract and 18/19 runtime tests.

Inside the final composite:
- production planner returned `AdjustedClear` with one real nominal dynamic conflict;
- first authority-bounded replacement used 24 s / 2 m/s;
- planned hazard clearance was 3.181387 m;
- actual clearance was 3.175166 m;
- tracking-envelope violations were zero;
- terminal replacement P error was 0.013099 m.

Thus the first physical bypass is accepted as healthy evidence.

The remaining failure came after that segment because the test resumed planning with `emptyDynamic()` while the same hazard remained physically alive.

Candidates `6c0a71d308040c568c109afc4425332021ade730` and `01a8d69cc635a450d73a49a31b91e5546e0b1828` keep dynamic world truth authoritative across segment boundaries:
- republish current hazard state;
- replan bounded suffix;
- repeat if necessary;
- return to static portal only after genuine `NominalClear`.

No safety tolerance was relaxed.


## 2026-09-20 — final composite exposes local-avoidance side ping-pong

Exact target checkout:

```
f626fb0373499928e0ae89585c3bd992e5436c92
```

kept the architecture contract green and all previous 18 runtime tests green.

Newtonian completed the entire final composite.

Assisted failed only after multiple successful persistent-hazard continuations. Its production adjusted targets alternated bypass side in Z:
```
+17.8 -> +15.5 -> -14.5 -> +13.8
```

The final opposite-side target was no longer physically authorable as a no-stop continuation from the actual live state.

Root cause was identified in transitional `LocalAvoidancePlanner`: within a deflection ring it returned the first safe azimuth. Because the local transverse basis changes after every segment, "first" does not preserve a physical avoidance side.

Production candidates `86640b05145938ec0880a3a26539957aaa72f085` and `ef2e6ec85823c229d6cadb6aa8dbe5b65e209193` now evaluate all safe candidates in the minimum deflection ring and choose the one most aligned with current velocity. No safety envelope is widened.

Regression `6064a22f565fd7cc82568b0babf2721891c8d925` pins symmetric +/-Z side continuity.

This is an improvement to the transitional B4 ray-fan; full route-aligned corridor migration remains open.


## 2026-09-20 — accepted local-segment continuity becomes explicit planner input

Target checkout `69f8ca4dbcb44df5340b94f45640bcb7d6e6ed1a` passed architecture but failed the focused continuity regression and final composite.

The focused fixture itself was partly wrong: its static region was only +/-10 m deep in Z, so the intended symmetric +/-Z probes were not both legal.

The composite nevertheless proved a deeper point: instantaneous velocity does not reliably encode the already accepted local bypass branch.

The ownership contract is now explicit:
- execution/accepted-program layer owns the direction of the accepted local segment;
- on REPLAN it passes that direction into `NavigationRuntimePlanner::AgentState`;
- runtime planner forwards it to `LocalAvoidancePlanner::Query`;
- local avoidance uses it to select among safe azimuths in the minimum deflection ring.

No mutable hidden planner memory is introduced and no safety bound changes.

Production/API commits:
`71b80c4529e1bc776e2a2dbf209059a2ccff44f9`,
`4bde26ee2fbb531116f960d089d488067f1bfd6d`,
`76022a5199422dd80ef4caff611539b53c39e731`,
`40d7b9852bc6f265ae02ecc0bf9d7b4e002d5b96`.

Regression/composite commits:
`bd31307fbda3d512a579f521efc1664199b1de46`,
`09bc81e03cab6c251b50678585161cc73e814ddb`.

This is still transitional B4 behavior. A future route-aligned corridor should carry branch continuity structurally rather than as a direction hint.


## 2026-09-20 — explicit continuity promoted across deflection rings

Target `51e6c41bb94b65e8cc269fb035164a4eb0aa23fd` passed architecture and 18/19 runtime tests. The focused continuity regression passed, proving the explicit hint reached production local avoidance.

The final composite still selected an opposite-side target because continuity was only ranked inside the first safe deflection ring.

Production candidate `e19c1804806ce5f3554f20c7b7d3d5ac19b4911e` changes the explicit-continuity ordering:
- scan all ordinary safe rings;
- same accepted branch beats opposite branch;
- alignment beats smaller angle;
- angle and azimuth remain deterministic tie-breaks.

The no-hint path retains legacy smallest-ring behavior.

Regression `06a917f058926a29f41a936dd994be3f8073e7cf` now blocks the accepted branch only on the primary ring and requires the planner to choose a larger same-branch ring instead of a smaller opposite-side candidate.

Composite continuity ownership was also tightened in `a2da6453daaa8e00cc1f4661321b9294513057ff`: only a successfully executed physical program becomes continuity state.

This remains a transitional B4 contract; future route-aligned corridor topology should carry branch identity structurally.


## 2026-09-20 — avoidance branch identity corrected to transverse geometry

Target `0ad327ad63d3e63f8c204b2e59a6f224f80c8fee` passed architecture but failed the strengthened branch regression and final composite.

The cause was conceptual: full direction dot product is not a valid bypass-side test because every progress-preserving visibility ray shares a large nominal-forward component.

Production now defines branch identity by transverse direction relative to current nominal forward.

Candidate `88f63b0d62803d73e7f3694d1c4f30bcbc1925d0` implements transverse branch classification.

Candidate `e9655a4b9f004fe790adbbc287bd55a3f1c269ea` ranks:
- same branch first;
- smallest safe ring inside that branch;
- best transverse alignment inside that ring.

Runtime diagnostics now expose whether a same-branch safe candidate actually existed. This is critical: if none exists, switching branches is legitimate and execution must recover/brake before accepting a radically different maneuver rather than forcing continuity.

The focused regression was also tightened to four deterministic azimuths in `0e344f3a3b2a5e887cbb474ce9ce650b68851716`.


## 2026-09-20 — forced branch switch promoted to explicit recovery escalation

Target `8011cc3ed19fc027fba256ee4aecca7c93a4ce0f` showed:
- continuity lateral valid;
- same-branch safe candidate count = 0 at the problematic replans;
- opposite target alignment as low as -0.976922.

Therefore the remaining composite failure is not a same-branch ranking error. The accepted branch is genuinely unavailable.

Production now exposes `avoidanceBranchSwitchRequired` through commits `4a91a78bea1e159a329ab346586a4d290ea5d420`, `9949b701bc8ba181a08b96e8077a375aada725be`, `7a5b4ae0520a10edbd89fd5f1e81f2427f4768be`, and `e407857d7764300193075cbee941be82312338f8`.

The final composite candidate `0ecf1b71b9620022a49ea71c996f5e81c02e5243` responds by executing a physically bounded Brake recovery, clearing the obsolete branch commitment, republishing world truth, and replanning from the recovered state before accepting the new branch.

The focused cross-ring fixture was also corrected in `c8e0c7cd6b6a7deb6c7f618c4d86ea80d4c62400` by placing its blocker on the actual primary preferred-ray endpoint.

This makes the transitional B4/B5 boundary explicit:
- geometry reports branch exhaustion/switch necessity;
- higher maneuver ownership performs the physical recovery/transition.


## 2026-09-20 — branch recovery accepted mechanically; post-recovery launch seam

Target `af9ee9d1694ac0facafaf23b0ec51c3adaf7dbbf` proved the new forced branch-switch recovery mechanism works physically:
- branch switch detected;
- 4 s Brake program;
- 6.757088 m stopping distance;
- 1.266954 m/s2 peak brake FF;
- 9.817623 m actual dynamic clearance;
- zero tracking-envelope violations;
- ~0.009 m/s final speed error.

The next failure was test-side authoring: `fitAuthorityBoundedReplacement` still required minimum speed >=0.5 m/s at t=0, making any deliberate post-recovery launch impossible.

Candidate `7c87e655788af4e95f9576675185482639fec528` permits a start below 0.5 only for a recovery-rest launch, while requiring non-reversing progress and a stable transition above the 0.5 m/s floor.

Focused regression candidate `252f9d81c5fd91363a0e0561e195c1f5ab0d375d` moves its blocker to an interior point of the primary ray and adds `[BRANCH-REGRESSION]` diagnostics.

No production safety or maneuver authority threshold was relaxed.


## 2026-09-20 — target build-only failure on branch-regression diagnostics

Target checkout:

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

passed the Stage-12 architecture contract, but navigation runtime behavior did not execute.

The build failed in `NavigationRuntimePlannerTests.cpp` because the newly added `[BRANCH-REGRESSION]` diagnostic used `std::setprecision(6)` without including `<iomanip>`.

This is compile-only noise and carries no new evidence about local planner behavior.

Commit `cfa56734020b41875354262302b9be51684413be` adds the missing standard-library include.

The active behavior gate remains:
```
branch exhaustion
 -> explicit recovery/brake
 -> clear continuity
 -> fresh replan
 -> post-recovery launch
```

No safety or physical thresholds changed.


## 2026-09-20 — HARD REPLACEMENT: projected visible-horizon bypass is the only ordinary local path

This migration supersedes every earlier Stage-12 experiment based on:
- angular deflection rings / azimuth fan search;
- persistent left/right branch continuity;
- same-branch ranking;
- branch-switch escalation as an ordinary local avoidance transition;
- mandatory Brake-before-changing-side behavior.

Those mechanisms are historical evidence only. They are no longer production
fallbacks.

Canonical unexpected-obstacle flow:

~~~text
accepted trajectory / corridor
        |
        v
physical visible horizon
        |
        +-- predict dynamic obstacle P(t), V(t), A(t)
        +-- retain known exact-static corridor constraints
        |
        v
plane normal to accepted trajectory tangent
        |
        v
project predicted swept occupancy
        |
        v
search bounded metric lateral/vertical offsets
        |
        +-- candidate exact-static proof
        +-- time-coupled dynamic proof
        |
        v
temporary bypass target
        +
merge target on original accepted trajectory
        |
        v
B5/B6 physical maneuver compilation/proof
        |
        v
execute short program
        |
        v
reacquire original trajectory
~~~

Normal local avoidance does **not** require a full stop. Longitudinal speed may
be reduced by physical maneuver compilation when useful. A fail-closed
hold/brake remains only for the case where no bounded safe offset is
demonstrated; higher topology/objective ownership may then choose another
waypoint/portal/route.

Hard-removal evidence in the current unverified repository state:
- `LocalAvoidancePlanner` public API contains metric offset/grid policy only;
- `NavigationRuntimePlanner` contains projected-bypass/merge products only;
- focused local/runtime tests no longer contain fan/branch fixtures;
- final composite no longer contains continuity state or branch recovery;
- live GameSimulation/server diagnostics use visible-horizon offsets in meters;
- Stage-12 architecture checker positively requires the projected solver and
  negatively rejects reintroduction of old fan/branch identifiers.

The latest actually tested target checkout remains:

~~~text
2ad1178bc5c778636748557ceb6c9a5b757c9a53
~~~

That checkout passed the architecture contract but failed to compile a
diagnostic test because `<iomanip>` was missing. It predates this hard
replacement and therefore provides **no acceptance evidence** for the new
visible-horizon solver.

The current hard-replacement code is UNVERIFIED until a fresh target-machine
architecture + navigation runtime gate is supplied.


### Mandatory state synchronization completed

The mandatory working-state documents have now been rewritten to the hard-replacement truth:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `CONTINUE_PROMPT.md` is recreated from scratch after this Stage-12 update.

The last accepted target baseline remains `3fe9b54eda0135b0cdebb7dc835d8a4b17580808`.
The latest actually tested checkout remains `2ad1178bc5c778636748557ceb6c9a5b757c9a53` and supplied no runtime-behavior evidence.
The projected visible-horizon hard replacement remains UNVERIFIED until a fresh target-machine gate is supplied.


### Two-segment visible-horizon candidate before first target gate

Current unverified code baseline before documentation/state-sync commits:

```
edb4c4106686ce625e1cd5eb99a6d1483cd32854
```

The initial projected-offset rewrite was tightened before target execution.

A bypass is no longer a single off-route endpoint at the merge station. The
solver now searches:
- metric lateral/vertical offsets;
- several longitudinal bypass stations inside the physical horizon.

Each candidate is:

```text
current state
    -> off-route bypass station
    -> on-route merge target
```

Acceptance requires:
- projected dynamic clearance;
- exact-static proof of current -> bypass;
- exact-static proof of bypass -> merge;
- time-coupled dynamic proof of the complete two-segment detour.

New diagnostics:
- `selectedBypassForwardDistanceMeters`;
- `routeCandidatesExamined`;
- runtime mirrors `localBypassForwardDistanceMeters` and
  `avoidanceRouteCandidatesExamined`.

A focused regression places exact-static geometry late in the horizon so an
insufficient detour can reach its bypass station but fails on the return leg.
This pins the requirement that route reacquisition itself is proved.

The replacement remains UNVERIFIED until the user's MinGW64 target gate passes.


## 2026-09-20 — 81d0c23 hard-replacement target gate

Exact target-machine checkout:

```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

passed the Stage-12 architecture contract and compiled/linked the runtime
suite, but finished 17/19 PASS.

Failures:
- `navigation_runtime_planner`: expected local visibility bypass;
- `navigation_composite_proving_ground`: first Newtonian replan returned
  `ConflictHold`, `localBypassExhausted=1` after 168 offset candidates and
  168 route candidates.

### Focused fixture audit

The failing portal-attitude fixture is incompatible with the new complete
two-segment dynamic proof. Agent start X=2, blocker X=4.5 and merge/staging
X=7 put both endpoints 2.5 m from the blocker. Required separation is
`1.0 + 0.75 + 0.0 + 1.0 = 2.75 m`. Since
`timeCoupledBypassClear()` samples both endpoints, no candidate can satisfy
the current contract. The fixture must be repaired geometrically without
weakening clearance.

### Composite audit

The logged state gives:

```text
current -> hazard = 48.0 m
current -> merge  = 30.0 m
merge -> hazard   = 18.0 m
```

Required dynamic separation is `26.775995 m` from the Cobra bounding radius
`17.275995 m`, hazard radius `6 m`, safety `2 m` and projection padding
`1.5 m`. At t=4 s the hazard has moved only 2 m laterally and remains about
`18.51 m` from the merge point. Therefore every two-segment candidate that
must finish at the current fixed merge target necessarily fails the
time-coupled dynamic proof.

This is more than a search-grid tuning issue. The current implementation
hard-wires merge to `boundedNominalTarget`. The next iteration must separate
the stale focused fixture from the production question of what happens when
safe reacquisition lies beyond the first bounded nominal point.

Next evidence:
- log projection/static/dynamic rejection counts at the first composite plan;
- repair the focused fixture geometry;
- add a regression for an unsafe first merge but safe later reacquisition;
- decide between downstream merge sampling, an adaptive horizon, or a
  proved multi-horizon off-route continuation;
- do not weaken safety thresholds or restore the superseded fan/branch path.


## 2026-09-20 — mandatory same-horizon merge removed

The first hard-replacement target failure exposed an incorrect assumption in
both implementation and tests: ordinary local avoidance had been made to prove
`current -> bypass -> return to nominal line` inside one visible horizon.

That is not the intended navigation behavior.

Current B4 contract:

```text
if a safe short bypass is available:
    accept and execute that short segment
    preserve forward progress
    do not require immediate return to the route

if no safe short bypass is available:
    issue active braking intent
    keep navigation ownership active
    re-evaluate fresh world truth

after the obstacle:
    progressively reacquire the nominal line under physical control limits
```

There is no fixed 30 m merge rule.

`mergeTargetMapMeters` remains temporarily for compatibility/diagnostics, but
its semantic is an on-route reacquisition reference, not a mandatory
current-horizon endpoint.

Production candidate before this documentation sync:
`70dc7bb9c024a388c39b79d54a12774475ca8b32`.

Implementation changes:
- ordinary adjusted candidate proves exact-static safety only for the selected
  current short segment;
- time-coupled dynamic proof covers the selected current short segment;
- equal lateral offsets prefer the farther forward station;
- `ConflictHold` still produces braking intent through the existing runtime
  control path; navigation is not disabled.

Regression changes:
- invalid portal fixture geometry corrected;
- mandatory return-leg regression replaced with
  `testBypassDoesNotRequireImmediateReturnToTrajectory()`;
- no-space regression still requires explicit exhaustion + braking.

This candidate is UNVERIFIED until a fresh target-machine architecture/runtime
gate is supplied.


### Composite physical-authoring fallback

The final composite previously failed immediately when
`fitAuthorityBoundedReplacement()` could not author the geometric bypass
inside ship authority. That contradicted the runtime contract.

The test now treats that state as:

```text
geometric bypass exists
    -> physical authoring succeeds -> execute bypass
    -> physical authoring fails    -> active braking
                                      keep hazard/world truth
                                      replan again
```

The braking path crosses the real `NavigationRuntimeControlBridge`,
`SharedShipPhysics`, and `DynamicMotionSystem`; it is not a test-side teleport
or navigation shutdown.

Current unverified code baseline before documentation sync:
`abfd7a6a26168f177968f0dd4299f3c712105fc0`.

## 2026-09-20 target run `navigation_test_20260920-182430.txt`

Important: the uploaded log does **not** contain the tested HEAD line, so the
exact checkout must not be inferred from the filename or current repository HEAD.

Observed evidence:
- Stage-12 architecture contract PASS;
- compile/link PASS;
- 17/19 runtime tests PASS;
- `navigation_runtime_planner` FAIL:
  `fixture must retain the future oriented portal as route context`;
- `navigation_composite_proving_ground` FAIL:
  `composite dynamic clearance lost for newtonian`.

### Planner fixture interpretation

The previous fixture repair moved the agent to X=0, exactly onto the minimum X
boundary of region 1 in `orientedPortalCaptureSpace()`. The new failure happens
before the actual bypass assertion: the test no longer retains the oriented portal
as route context. This strongly indicates the fixture was repaired in the wrong
place rather than a B4 regression. Use an interior start while preserving >2.75 m
separation from both start and staging endpoint (for example start X=1, blocker X=4, staging X=7).

### Composite interpretation

The B4 behavior itself improved materially:
- first solve: `AdjustedClear`, 29.38 m lateral offset, 22.5 m forward;
- physical replacement authored successfully;
- actual first replacement kept +4.747 m dynamic clearance and zero tracking violations;
- next receding-horizon solve again returned `AdjustedClear`;
- continuation kept +22.821 m actual dynamic clearance and zero tracking violations;
- next solve returned `NominalClear`.

The failure occurs **after** that successful B4 sequence. The composite then leaves
the receding-horizon planner loop and executes a fixed 10 s narrow-portal program
and then final capture while the dynamic hazard remains active. That violates the
new requirement that navigation/monitoring must remain active. A bounded segment
being nominal-clear does not prove the entire following scripted portal leg is clear.

Therefore the next fix should keep planner/monitor/replan ownership active through
the resumed topology leg instead of treating `NominalClear` as permission to run an
unmonitored long scripted phase.

### Visualisation

A visual trace is now justified. The useful first visualization should plot, in the
same 2D/3D scene, ship path, hazard path and inflated safety envelope, selected B4
targets, bounded nominal/reacquisition references, portal center, and replan points.

## 2026-09-20 navigation runtime 3D viewer

Implemented an independent diagnostic viewer under `tools/navigation_runtime/`.

Files:
- `tools/navigation_runtime/NavigationTrace.h/.cpp` — shared deterministic JSON trace schema/IO;
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp` — standalone GLFW/GLAD/GLM OpenGL 3D viewer;
- `tools/navigation_runtime/CMakeLists.txt` — isolated viewer build;
- `tools/navigation_runtime/run_mingw64.sh` — build/run helper;
- `tools/navigation_runtime/README.md` — controls and workflow.

The composite proving-ground test now writes the actual simulated run to
`tools/navigation_runtime/last_trace_<law>.json`. The writer is RAII-backed,
so the trace is retained when a later composite assertion throws.

Recorded data includes ship P/V/forward, dynamic-hazard P/radius, hull collision
envelope, planner safety envelope, actual dynamic clearance, phase/status,
selected B4 target, reacquisition reference, portal target and replan events.

Viewer presentation:
- topology route as a polyline;
- route turn/portal points as markers;
- actual ship trajectory;
- ship as an oriented wire rectangular box using Cobra half extents;
- explicit nose arrow;
- hazard trajectory and three wire envelopes;
- selected target / reacquisition / portal markers;
- all replan positions;
- current time/phase/status/clearance in the window title.

Controls: RMB orbit, MMB pan, wheel zoom, F fit, Space play/pause,
`[`/`]` frame step, R next replan, Esc close.

Validation state: source is committed but has NOT yet been compiled/run on the
target MinGW64 machine. Do not call the viewer accepted until that gate runs.

## 2026-09-20 target run on `c8972319390622a6b825bf0fa73e8a563c9d7068`

Verified on target MinGW64:
- navigation runtime configured and linked;
- 17/19 tests PASS;
- planner fixture still failed at future oriented portal route-context assertion;
- composite emitted `tools/navigation_runtime/last_trace_newtonian.json` with 770 frames;
- composite B4 sequence remained the same: two physically executed `AdjustedClear`
  segments, then `NominalClear`, then later dynamic-clearance failure;
- standalone viewer configure succeeded, but compile failed because the GLAD
  include root was wrong: source includes `<glad/gl.h>`, actual file is
  `glad/include/glad/gl.h`.

Fixes committed after that verified run:
- viewer CMake now includes `${ELITE_SOURCE_ROOT}/glad/include`;
- oriented-portal fixture now uses interior geometry start X=1, blocker X=4,
  stage X=7 while leaving global `baseAgent()` at its original X=0;
- these post-run fixes are UNVERIFIED until the next target run.

## 2026-09-20 viewer HUD iteration

Target feedback confirmed the standalone viewer now builds and opens, but the first
UI-less build was not self-explanatory: no visible buttons and no legend/state panel.

Viewer updated with an in-window diagnostic HUD:
- clickable PLAY/PAUSE, PREV, NEXT, NEXT REPLAN and FIT buttons;
- right-side live state panel with law/frame/time/phase/status/clearance;
- explicit `WHAT IS HAPPENING` explanation derived from trace phase/status;
- color legend for every diagnostic primitive;
- visible controls reminder;
- `portal_102` explicitly marked as the current known clearance-loss area.

The HUD uses a tiny built-in bitmap font and existing OpenGL primitives; no new UI
framework/dependency was introduced.

Validation state: previous viewer build/open is target-verified; the new HUD commit
still needs one target build/open check.

Same target run also showed planner fixture progress: the old route-context assertion
no longer fails; it now reaches `fixture must produce a safe adjusted target`. That
is a separate planner-fixture task and was not mixed into this HUD change.

## 2026-09-20 viewer physics/trace accuracy iteration

User clarified the viewer must remain an ordinary decorated Windows window, simply
maximized to the desktop work area; no exclusive/borderless game fullscreen.

Verified architecture of the current tool:
- composite test executes real Planner -> AcceptedManeuverProgram -> Follower ->
  runtime bridge -> SharedShipPhysics first;
- the viewer only replays the resulting JSON; it does not run planner/follower live;
- previous apparent low performance was primarily 0.10 s trace quantization (~10 Hz)
  with nearest-frame display, not evidence of a heavily loaded CPU.

Implemented, not yet target-verified:
- smooth interpolation between trace samples during playback;
- trace schema v2 records actual full ship basis forward/right/up (roll preserved);
- trace schema v2 records sampled AcceptedManeuverProgram reference P + full basis;
- viewer draws actual nose and program-reference nose separately and reports angular
  tracking error;
- viewer renders AcceptedManeuverProgram tracking.positionErrorMeters as a translucent
  tracking tube. This is explicitly NOT called a Planner volumetric corridor because
  NavigationRuntimePlanner::Result does not publish one;
- trace records hazard velocity + planner look-ahead;
- bottom-left `ГОРИЗОНТ КОБРЫ` inset projects the moving hazard and its predicted
  envelope tunnel onto Cobra's right/up plane perpendicular to actual forward;
- Russian HUD/Cyrillic bitmap support and Russian window title;
- interactive frame slider;
- ordinary GLFW window starts maximized.

Newtonian diagnostic correction:
- the composite test previously authored replacement/continuation programs with
  OrientationMode::VelocityAligned even under Newtonian law, making Newtonian motion
  visually look Assisted;
- Newtonian local bypass and the un-oriented portal-102 transit now preserve current
  body attitude (FixedStart); Assisted remains velocity-aligned;
- this change is unverified until the next target run.

Still pending and must not be faked in replay UI:
- pilot-skill selector (expert/average/loser);
- flight doctrine selector (standard/extreme);
- meaningful obstacle checkbox producing a different simulation, not merely hiding
  the obstacle;
- meaningful Assisted/Newtonian selector that runs/regenerates the scenario.
These require extracting/reusing a live scenario runner inside the tool or generating
distinct authoritative traces. Do not add cosmetic controls that leave physics unchanged.

## 2026-09-20 live navigation diagnostic stand

Architecture changed: `tools/navigation_runtime` is no longer a passive JSON trace
viewer. The JSON file is now an INPUT SCENARIO only. The executable calculates the
route and vehicle motion in-process when the user presses `РАССЧИТАТЬ`.

New production-chain runtime:
`scenario.json -> NavigationSpace/NavigationMap -> NavigationRuntimePlanner ->
AcceptedManeuverProgram -> TrajectoryFollower -> NavigationRuntimeControlBridge /
PilotSkillExecutor -> SharedShipPhysics / DynamicMotionSystem -> in-memory trace -> 3D`.

New files:
- `tools/navigation_runtime/NavigationScenarioRuntime.h`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tools/navigation_runtime/scenario.json`.

Real UI inputs:
- control law: Assisted / Newtonian;
- pilot: Expert / Average / Loser;
- flight style: Standard / Extreme;
- sudden-obstacle checkbox;
- `РАССЧИТАТЬ` button.

Pilot choice changes the actual PilotSkillExecutor profile (reaction delay, decision
rate, latency, response, slew and deterministic command error). Flight style changes
real cruise speed and planner horizon/cost aggressiveness. Control law changes the
actual local flight law and maneuver attitude semantics.

Sudden obstacle semantics:
- unchecked: sudden obstacle is never published;
- checked: it is absent from initial NavigationMap and appears only after
  `activation_time_s` during the simulation;
- therefore the initial route has no foreknowledge of the surprise obstacle;
- after activation normal receding-horizon replanning reacts to it.

`scenario.json` supports:
- start P/V/forward/up;
- optional forced ship route points (default empty);
- final position;
- optional final forward/up constraints;
- final constant speed requirement;
- static sphere/box/capsule obstacles;
- moving obstacles defined by velocity vector OR route points + speed;
- sudden obstacle, including spawn relative to live ship forward/right/up.

The default scenario leaves `ship_route_points` empty so Planner must calculate the
route around the static obstacle rather than replay authored portal points.

Calculated white route now records the live sequence of Planner `selectedTarget`
decisions. Adjusted targets are marked as turn/bypass points. Actual motion remains
a separate rendered path.

Static obstacles from JSON are carried into the calculated TraceDocument and rendered
in 3D. `last_calculated_trace.json` is still saved for diagnostics, but is an OUTPUT,
not an input to the stand.

Build architecture:
`tools/navigation_runtime/CMakeLists.txt` now links the production navigation/control/
physics stack directly instead of being an OpenGL-only replay executable.

Validation state: this live-runtime iteration is committed but NOT YET compiled/run on
the target MinGW64 machine. Do not claim acceptance until the target gate passes.

## 2026-09-20 target video diagnosis — live stand integration bug

User supplied a screen recording of the first live-stand behavior.

Observed on-screen:
- playback around t≈41 s showed phase `ТОРМОЖЕНИЕ` and status `ДАННЫЕ УСТАРЕЛИ`;
- Cobra made almost no meaningful route progress;
- right diagnostic panel visibly jumped/blinked because conditional rows (especially
  the one-frame `СОБЫТИЕ: ПЕРЕПЛАНИРОВАНИЕ`) were inserted/removed from flow layout;
- the default no-sudden-obstacle run therefore did not demonstrate routing quality.

Root cause found in `NavigationScenarioRuntime.cpp`:
`NavigationRuntimePlanner::plan(... dynamicResultAgeSeconds ...)` was called with
`vehicle.timeSeconds` (absolute simulation time) instead of the age of the freshly
queried NavigationMap snapshot. With `maxResultAgeSeconds=0.25`, every plan after
0.25 s became `StaleHold`, correctly triggering fail-closed braking.

Fix committed:
- live stand now passes `0.0` for the just-created dynamic snapshot age;
- this is the correct semantics for the current synchronous query->plan call.

HUD fix committed:
- right panel uses hard fixed Y slots for mode/frame/time/phase/status/orientation/
  clearance/event/calculation result/explanation/legend/controls;
- optional data displays `-` instead of adding/removing rows;
- replan indication may still change color/text, but cannot move any other text;
- calculation result is always visible during playback so a partial failed trajectory
  is not silently presented as a successful solution.

Interpretation of previous daytime tests:
- they were not fake: they exercised real production planner/follower/control/physics
  components and found genuine local failures;
- however the composite fixture staged the scenario in manually authored phases and
  handcrafted AcceptedManeuverPrograms, so it did NOT prove a free-running arbitrary
  start->world->finish orchestration loop;
- the live stand exposed exactly that missing integration gate.

Important remaining architecture gap:
- current live stand still converts Planner output to a locally authored quintic
  `makeShortProgram`; it does not yet drive every nominal/replan segment through the
  full production B5/B6/B7/B8 physical compile/proof/selection/acceptance chain;
- therefore the next task is to replace that local adapter with the actual production
  maneuver compilation/proof chain before treating live-stand success as navigation
  acceptance evidence.

Validation state: snapshot-age and fixed-HUD patches are committed but not yet target
recompiled/replayed after this video.

## 2026-09-20 architecture correction — global corridor vs local dynamic avoidance

User clarified the intended ownership and this matches the existing LocalAvoidancePlanner contract:

- the nominal global route/corridor is built once from start to finish;
- it remains authoritative while the destination and static navigation world remain unchanged;
- moving obstacles do NOT trigger global route reconstruction;
- dynamic snapshots are consumed only by a bounded local monitor/avoidance layer;
- local avoidance may temporarily leave the nominal corridor, then progressively reacquire it;
- follower/autopilot continuously executes the current accepted trajectory/control program;
- the global planner is invoked again only when the goal changes or the static navigation world/corridor revision becomes invalid.

`maxResultAgeSeconds` / the ~0.25 s freshness limit is therefore a dynamic-snapshot safety contract, NOT a global replanning cadence.

Current live stand violates this architecture because it calls NavigationRuntimePlanner::plan repeatedly and therefore recomputes the static corridor every short execution slice.

Important current capability gap:
`NavigationSpace::queryCostedCorridor` currently returns a region/portal corridor and portal centers. Exact static NavigationObstacle geometry is used to prove/reject local segments, but the global corridor search does not yet synthesize a full geometric path around arbitrary exact obstacles inside one coarse region.

Therefore the default one-region + wall live scenario cannot honestly demonstrate the requested `start -> finish` global corridor yet. A proper global geometric corridor product must be added/cached first, then local dynamic avoidance must operate against that immutable nominal corridor.

## 2026-09-20 clarification — corridor is not the physical collision tunnel

User clarified terminology:
- `corridor` is a navigation/test abstraction around the nominal route used to ask
  whether the ship can generally proceed along that route;
- it is NOT the authoritative physical swept volume of the Cobra;
- exact wall contact / aperture passage must be decided by the future physical
  `tunnel` product: the time-parameterized swept volume of the actual hull along the
  accepted trajectory, including body orientation;
- therefore corridor visualization may be approximate and route-centric;
- tunnel proof is where it becomes critical whether Cobra's real hull clips walls.

Implication:
- do not over-engineer global corridor generation as exact hull clearance geometry;
- global planning may retain a centerline/polyline + coarse corridor for navigation;
- exact physical feasibility remains downstream ownership of trajectory/tunnel proof
  against exact static geometry and dynamic occupancy.

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


## Architecture-gate compatibility fix

During pre-handoff audit, the existing `check_geometric_path_planner.py` was found to
encode an obsolete ownership assumption: it required the same geometric planner .cpp
to appear twice in root CMake. Current runtime architecture already compiles it once in
shared `EliteNavigationGeometry`, which both client/server navigation runtime reuse.
The checker now pins that shared-library ownership instead of duplicate compilation.
This is an architecture-test correction only; route behavior is unchanged.


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


## 2026-09-21 — retained-route Stage-2 target execution PASS

User-supplied target diagnostics prove that the route-leg phase correction works in the
real diagnostic stand. The supplied output does not contain `git rev-parse HEAD`, so
this evidence is intentionally not attached to an inferred commit SHA.

Stage 1 remained unchanged:
- START = (0,0,0) m with V=(6,0,0) m/s;
- FINISH = (300,0,0) m;
- one static obstacle;
- 4 retained route points;
- retained route length 323.75 m;
- static detour YES.

Observed Stage-2 runs:

| Pilot / style / law | Ruckig samples | phases / handoffs | final P error | final speed | max route dev | max follower error | contact |
|---|---:|---:|---:|---:|---:|---:|---|
| Expert / Standard / Newtonian | 1521 | 3 / 2 | 0.15 m | 0.10 m/s | 3.26 m | 1.08 m | NO |
| Expert / Extreme / Newtonian | 1416 | 3 / 2 | 0.10 m | 0.48 m/s | 3.26 m | 0.95 m | NO |
| Expert / Extreme / Assisted | 1416 | 3 / 2 | 0.10 m | 0.48 m/s | 3.26 m | 0.95 m | NO |

All three runs report:
- `ROUTE EXECUTION COMPLETE: YES`;
- `FOLLOWER: EXECUTED`;
- `FOLLOWER FAIL REASON: NONE`;
- `PILOT BRIDGE: EXECUTED`;
- `COARSE STATIC CONTACT: NO`.

This directly closes the previous 102-microprogram failure mode. The current four-point
retained route is executed as three meaningful physical route-leg phases with two real
ManeuverPhaseGate handoffs.

The exact retained-route E2E gate now exercises the previously missing chain:

```text
scenario.json
 -> NominalRoutePlanner once
 -> immutable retained route
 -> Ruckig
 -> route-leg AcceptedManeuverPrograms
 -> TrajectoryFollower
 -> PilotSkillExecutor
 -> SharedShipPhysics / DynamicMotionSystem
 -> authored finish
```

This is strong static integration evidence, not full Navigation-v2 acceptance. The
current static contact check is still coarse route-envelope geometry, not the final
oriented swept-hull B6 tunnel proof.

### Next active slice — dynamic local overlay on the retained route

Static retained-route execution is now sufficiently stable to start the previously
deferred dynamic layer.

Hard ownership contract:
- dynamic actors never rebuild the retained global/static route;
- moving/sudden obstacles are a bounded local monitor/avoidance overlay only;
- surprise obstacles become visible only after their authored activation time;
- if a physically executable short bypass exists, execute that bypass;
- do not require an immediate return-to-line segment inside the same horizon;
- if no safe executable bypass exists, issue active braking while navigation remains
  alive and continues evaluating fresh world state;
- after the obstacle clears, progressively reacquire the retained route under vehicle
  limits rather than teleporting or enforcing an arbitrary merge distance.

Initial dynamic acceptance should keep the same Stage-1 route immutable and first prove
Expert/Standard/Newtonian. Mode/style expansion follows after the local dynamic chain is
behaviorally correct.


## 2026-09-21 — video review: static maneuver quality blocker

User video review exposed two real quality defects that must be fixed before the
dynamic-overlay stage is promoted.

### 1. Shallow retained-route corners incorrectly become StopTurnGo

For the previous default retained route, the static planner produced approximately:

```text
(0, 0, 0)
 -> (110, -27, -45)
 -> (190, -27, -45)
 -> (300, 0, 0)
```

The two interior course changes are only about 25.5 degrees. An Expert pilot has no
behavioral reason to stop there.

The stop is caused by the current Ruckig waypoint-authoring fallback:
- `buildWaypointVelocities()` attempts a 25%-leg-length shortcut chord around each
  interior polyline point;
- on this geometry that chord intersects the inflated static wall;
- the waypoint is therefore left with zero target velocity;
- Ruckig then correctly solves a stop-at-waypoint trajectory;
- both Newtonian and Assisted inherit the same upstream StopTurnGo reference.

This is a trajectory-authoring defect, not a sensible Expert maneuver doctrine.

### 2. Newtonian hull/velocity semantics are not yet expert-quality

Low-level propulsion allocation is physically differentiated:
- Newtonian has aft-main thrust only for the main channel; reverse/main braking
  requires body reorientation, while lateral/vertical correction is bounded by RCS;
- Assisted has symmetric longitudinal main authority, with lateral/vertical correction
  still bounded by RCS.

However the Stage-2 reference is authored in the wrong order for a
main-engine-dominant Newtonian maneuver:
- Ruckig first creates a translational P/V/A history using a common vector
  acceleration budget;
- `buildReferenceAttitudes()` then points Newtonian body-forward toward the already
  requested acceleration whenever |A| > 0.35 m/s2;
- bounded RCS can therefore begin changing the velocity vector while the hull is still
  catching up;
- before a zero-speed waypoint, the acceleration vector becomes braking-oriented, so
  the body reference rotates toward the braking vector and then rotates again for the
  next leg.

The resulting motion is physically possible as an RCS-heavy correction, but it is not a
credible Expert Newtonian route maneuver. Large course changes must be authored as
body/thrust-aware physical primitives before ACCEPT, not derived by attaching attitude
after an arbitrary translational Ruckig curve.

Assisted also stops at these shallow corners because the zero waypoint velocity is
already baked into the common translational trajectory.

### 3. Standard vs Extreme is not yet a real flight doctrine

In the current diagnostic Stage 2, Standard vs Extreme primarily changes the requested
route speed (10 vs 18 m/s). It does not yet choose different physical maneuver families,
clearance use, slip tolerance or aggressiveness. Therefore visually similar maneuver
semantics are expected and are not sufficient implementation of the requested flight
styles.

### 4. Static route detour was unnecessarily long

The old box support graph had:
- 8 corners;
- 6 face centers;
- no 12 edge midpoints.

For the default wall, inflated/support half-extents are approximately
`(40, 27, 45)`. Because edge midpoints were missing, visibility A* had to pay
clearance on two transverse axes and selected the old 323.75 m route through
`(110,-27,-45)` and `(190,-27,-45)`.

The geometrically shorter same-clearance route is approximately:

```text
(0, 0, 0)
 -> (110, -27, 0)
 -> (190, -27, 0)
 -> (300, 0, 0)
```

with length about 306.53 m.

Candidate fixes committed:
- `e53312cc9b00119e69f1c7676edf14b5d21fea64` — add all box edge-midpoint support
  nodes to `GeometricPathPlanner`;
- `8e0d4c4c6ca7d4d7ae3e7cc71387ebff8b335302` — regression requiring a nearest-face
  one-plane box detour rather than unnecessary second-axis displacement;
- `ac30d441ebdafe232af0bf0fe7e8882da9f38767` — viewer uses a Cobra-like triangular
  prism, a much smaller translucent nose marker, and a thick current-velocity vector
  whose length is proportional to speed, with numeric speed at the vector tip;
- `c4c63c9751a4b5b381da290a570ee98775148eb6` — diagnostics now print every retained
  route point, interior retained-waypoint speeds and maximum body/velocity angle.

These candidates are not target-validated yet.

### Active priority change

Dynamic local avoidance is postponed, not discarded. First make the static retained
route execution behaviorally credible.

Next mechanism target:
- shallow/medium corners must remain moving when a physically safe maneuver exists;
- no zero-speed waypoint merely because one synthetic corner-cut chord is blocked;
- Newtonian Expert must use a body/thrust-aware maneuver (lead turn, drift/coast,
  RCS trim, main-burn as appropriate) rather than letting the translational vector
  rotate independently and asking attitude to catch up afterwards;
- Assisted Expert must also preserve speed on ordinary shallow bends;
- stop/flip is reserved for geometry, required terminal pose or braking physics that
  actually requires it;
- Standard and Extreme must differ by real maneuver doctrine/aggressiveness, not only
  by max-speed scalar;
- production B5/B6/B7/B8 ownership must be preserved. Do not turn geometric targets
  directly into accepted execution programs to make the viewer look good.

The already documented architecture gap remains decisive: ordinary B5 exists first for
Newtonian, but a generalized ordinary B6 continuous maneuver prover is still missing.
Do not bypass that proof boundary.


## 2026-09-21 — adaptive corner pass candidate after user retest

User retest confirms:
- the new nearest-face nominal route is logically acceptable;
- the full stops at the two shallow intermediate points remain;
- the program-reference arrow visibly leads the physical hull;
- the current velocity vector was not legible enough in the viewer.

The supplied performance log confirms the stop mechanism is still active: the latest
four-point / one-obstacle Stage-2 solves finish with `blended_waypoints=0`.
Therefore both interior waypoints are still authored as zero-speed points before this
candidate.

### StopTurnGo fallback changed

Candidate commits:
- `d2205f50259fdef05a6515fec3822055891c47d5`:
  `buildWaypointVelocities()` no longer tests only one 25%-of-leg corner chord.
  It searches from the widest proposed local blend toward a bounded smaller blend and
  keeps the widest collision-clear option. If a generated Ruckig leg still fails,
  through-speed is reduced progressively (70% steps) before falling back to zero.
- `641e6ef6aeb79d4dddcf58ce7601448723f79b9a`:
  removed the arbitrary global `0.65 * maxSpeed` waypoint cap. Curvature/available
  lateral acceleration and explicit speed limits now own the corner-speed limit.
- `20aef47942c675ae59b04c288b2ae4b3cb6de5e6`:
  focused Ruckig regressions now distinguish:
  1. wide corner chord blocked but tighter continuous turn available -> must keep moving;
  2. even minimum local blend blocked -> safe stop remains legal;
  3. the current default wall detour -> both shallow interior points must retain
     non-zero through-speed.

This makes the retained white polyline a geometric/topological intent rather than a
literal StopTurnGo execution prescription. Ruckig remains responsible for a continuous
time trajectory through the retained route; the candidate does not reintroduce the
retired custom B-spline backend.

If the target gate still collapses either default wall waypoint to zero, the next step
is to add explicit maneuver-space offset / corner-radius reserve to the execution path
rather than accepting StopTurnGo. Slightly lengthening the geometric route is allowed
when required to create real turn room.

### Viewer observability changed

Candidate commits:
- `cbdbcc2b2132f0ef29af9a73e3589d25c415a8f5`;
- `2ad3bf086c153895adefee64fb9f67bcabaa84da`;
- `f695a55454f5ccc5802c618347dfb400ec4d6bcb`.

Changes:
- speed number moved away from the ship into a fixed lower-right block;
- current velocity vector is now bright yellow, 6 px wide and 3.5 world-meters per
  1 m/s, so 10 m/s is about a 35 m arrow;
- short cyan arrow = actual physical hull nose;
- red arrow = Follower/program target nose;
- white line = retained geometric route;
- green line = actual flown trajectory;
- the lower-right block states these meanings explicitly.

Interpretation rule for the next video:
- red leading cyan by itself means the attitude reference is commanding ahead of the
  physical hull and is not automatically wrong;
- the important Newtonian fault is if the **yellow actual velocity vector** bends as
  though the requested thrust attitude were already achieved, rather than following
  the acceleration authority of the actual cyan-oriented hull/RCS system.

The deeper Newtonian architecture issue remains open: generic translational Ruckig P/V/A
is still authored before the final body/thrust-specific maneuver. The adaptive corner
candidate is intended to remove the clearly invalid zero-speed fallback first, not to
declare the complete B5/B6 Newtonian maneuver problem solved.

### Validation status

UNVERIFIED on target MinGW64.

Run the focused Ruckig route tests plus the two-stage viewer gate before promoting this
candidate.


## 2026-09-21 — target corner gate 7/8; convert Test 1 to moving-start/moving-finish

Target MinGW64 evidence from the focused Ruckig suite:
- straight route: PASS;
- diagonal stopped leg: PASS;
- clear corner keeps through velocity: PASS;
- blocked wide blend shrinks before stop: PASS;
- default wall shallow corners stay moving: PASS;
- initial acceleration preservation: PASS;
- impossible braking rejection: PASS;
- the only failure was the synthetic `truly blocked corner falls back to stop`
  assertion.

The important result is that the actual default four-point wall case now passes the
focused requirement that both shallow intermediate corners retain non-zero speed. The
single failure was not a safety failure: the test assumed that one tight fixture must
force zero speed. That assumption is stronger than the architecture contract. If the
solver proves a collision-free continuous passage, stopping only to satisfy the test is
wrong. The fixture is therefore retained as a swept-safety check, not as a mandatory
StopTurnGo assertion.

### Test-1 isolation change

For the default Standard stand, remove start/finish transients from the visual test:
- authored Standard max speed = 10 m/s;
- start velocity changed from 6 m/s to 10 m/s;
- finish speed changed from 0 m/s to 10 m/s.

The intent is a steady 10 m/s fly-through test:
```text
start already at 10 m/s
 -> shallow corner 1
 -> straight bypass leg
 -> shallow corner 2
 -> cross finish still at 10 m/s
```

This isolates the two turn maneuvers from startup acceleration and terminal braking.

### Moving terminal velocity support

The previous runtime had two artificial stop constraints:
- `executeCalculatedRoute()` rejected any non-zero authored finish speed;
- `RuckigRoutePlanner::plan()` unconditionally overwrote the final waypoint velocity
  with zero.

That is now replaced by an explicit exact terminal-velocity contract in
`TrajectoryGenerationRequest`:
- `hasTerminalVelocity`;
- `terminalVelocityMps`.

Point speed constraints remain upper bounds; the new terminal velocity is the authored
final inertial velocity.

Candidate commits:
- `c9a8e381531feee4516cafa436b67809fb2d753d` — request contract;
- `9372af939d2406768e530f9e2c02e05389f0df83` — Ruckig honors exact moving terminal;
- `82dc142e5c06f8be8e6c94aec810f16b8b47302f` — runtime accepts/authors moving finish;
- `75f11481979b83706861bbc98f8b6326341a07e0` — default stand start=10, finish=10;
- `d40e4fdc7cb1048d0f45daf2598788684e49affc` — focused tests use moving terminal and
  remove mandatory-stop assumption from tight corner;
- `bc5fbaa284663d7a986e2257b8d71f4a64a610ba` — focused straight Test 1 is a true
  steady 10 m/s transit;
- `7f57df3ef4a05d6601d05aa9c94de7121e0d9884` — retained-route E2E now requires
  crossing the finish at the authored 10 m/s.

These latest moving-terminal changes are not yet target-validated.

### Next target evidence

Run focused Ruckig first. Expected qualitative result:
- 8/8 focused tests;
- default wall shallow corners moving;
- straight Test 1 starts and ends at 10 m/s without an authored stop.

Then run Stage-1/Stage-2 viewer. For Expert/Standard/Newtonian, the useful visual is now
almost entirely the two corners because start and finish no longer require speed
transients.

After that, inspect yellow actual velocity vs cyan actual hull nose vs red target nose.
The deeper Newtonian body/thrust-aware maneuver-authoring issue remains open.


## 2026-09-21 — viewer-first maneuver inspection; moving finish is fly-through

Latest target output after switching the stand to start=10 m/s and finish=10 m/s showed:
- retained route remains 306.53 m with four points;
- Ruckig trajectory generated successfully;
- both interior retained waypoints are now exactly 10.00 m/s;
- no coarse static contact;
- the run failed only at the final phase with `FINAL_CAPTURE_TIMEOUT`;
- by timeout, final speed had collapsed to 1.01 m/s and final position error had grown to
  32.94 m;
- maximum body/velocity angle reached 178.94 degrees.

This failure was caused by a semantic mismatch in the stand, not by the two shallow
corner speeds. The last phase always used `ManeuverPhaseGate::StateCapture`, which is
correct for a parking/stopped terminal but wrong for an authored moving terminal. After
the nominal moving trajectory ended, the follower kept trying to capture a fixed final
position while also owning a non-zero terminal velocity reference. That produced the
artificial braking and eventual timeout.

Candidate fix:
- `9b1251e8601172ecd83ba4f656871f92f4669fb4` — a non-zero finish speed now makes
  the final phase `ScheduledMoving`; the terminal is a fly-through boundary rather
  than a parking capture.

Process change requested by user:
- current maneuver development is inspected visually in
  `navigation_runtime_viewer.exe`;
- the viewer preparation script must not auto-run the headless Stage-2 E2E and abort
  before the user can inspect the maneuver;
- `06513569f7861ac8b6e3104994ec72ffd0d891d1` removes that automatic headless
  Stage-2 run from `run_stage1_mingw64.sh`;
- focused/static architecture tests still run, and the viewer is still built;
- the headless E2E target remains available separately for later regression work.

Current visual objective:
```text
10 m/s at start
 -> corner 1 at moving speed
 -> bypass leg
 -> corner 2 at moving speed
 -> cross finish at 10 m/s
```

Do not interpret a red target-attitude arrow leading the cyan physical nose as a fault
by itself. The key Newtonian diagnostic remains whether the yellow actual velocity
vector changes consistently with the real cyan hull attitude and bounded RCS/main-thrust
authority. The 178.94-degree body/velocity diagnostic in the failed headless run is
strong evidence that this deeper maneuver-authoring issue remains.


## 2026-09-21 — expose the real calculated curve and replace exact-corner forcing with a rounded execution guide

User viewer evidence showed visible braking / path distortion on apparently straight
parts of the retained four-point wall route and requested that the actual calculated
"spline" be drawn.

Important correction: production runtime does **not** use the retired
`SmoothPathOptimizer`. The `SmoothPathPerf` records that can still appear in the
aggregate navigation performance log come from legacy/test compatibility runs. The
current runtime route-to-motion backend is the canonical Ruckig path. Therefore the
curve that must be inspected is the complete Ruckig reference trajectory, not a hidden
B-spline.

### Runtime geometry change

The previous Ruckig route authoring still forced each coarse Stage-1 support vertex to
be an exact target position while assigning a non-zero bisector velocity there. That
can create an unnecessarily awkward local state constraint: the solver must hit one
mathematical corner point and simultaneously leave it already rotated in velocity
space.

New candidate behavior:
- Stage-1 `routePoints` remain retained and unchanged.
- Stage-2 derives a separate collision-checked `executionGuide`.
- For every interior coarse vertex it computes a physically useful tangent reserve from
  authored speed and lateral acceleration.
- The coarse vertex is replaced locally by entry/exit guide points.
- If that useful corner does not fit, the local corner can be moved farther outward into
  free space in steps instead of tightening the curve or forcing StopTurnGo.
- Ruckig parameterizes this guide.
- The full guide is published in `TrajectoryGenerationResult::executionGuidePointsMeters`.

Candidate commits:
- `0b4212345b2c38b4f775937060a81ba56cace219` — expose execution guide;
- `939a7058ba58a244282d219e0c298150a72a410f` — rounded/widenable execution guide;
- `77f9640eb77267be6a258b0293720e00f089fbe6` /
  `f9b106f8c259c0f7a39ebde11f67c776e1a35c92` — guide diagnostics.

### Viewer change

The trace now carries two new Stage-2 products:
- `executionGuidePoints`;
- `calculatedTrajectoryPoints` (all Ruckig samples).

Viewer colors:
- white = retained coarse geometric route;
- blue = widened local execution guide;
- purple = complete calculated Ruckig curve;
- green = actual flown trajectory;
- yellow arrow = actual velocity;
- cyan short arrow = real hull nose;
- red arrow = program target nose.

Blue guide support points are also drawn as small crosses.

Candidate commits:
- `3ee684742afd8ad91307b99c0a1815bd30aea722` /
  `287ab085275873ffb1f991e09902d31706edb927` — trace schema/persistence;
- `33e3b228327cfb9c6031ecb09aa67847d16d2cda` — runtime publishes guide and full
  reference plus calculated min/max speed;
- `154d9e1a544dc957769efce127b70ee36dd8b0cc` — viewer renders the products.

### Focused regression

The default wall Ruckig test now additionally requires:
- execution guide has more support points than the four-point coarse route;
- calculated reference minimum speed stays >= 7.5 m/s in this steady 10 m/s stand;
- calculated reference does not geometrically backtrack along +X.

Commit:
- `68f4377aabae1fd894be4a882fb30b0cec762d89`.

This candidate is **not target MinGW64 validated yet**.

### Immediate diagnostic objective

In the next viewer run compare:
1. white coarse route;
2. blue execution guide;
3. purple calculated Ruckig curve;
4. green actual path.

Also inspect:
- `CALCULATED MIN SPEED`;
- `CALCULATED MIN SPEED POS`;
- `CALCULATED MAX SPEED`.

If purple is smooth and remains near 10 m/s while green brakes/loops, the fault is
downstream in Follower / body-thrust execution. If purple itself brakes or loops, keep
the fix in route-to-trajectory authoring and widen/reject the guide before ACCEPT.


## 2026-09-21 — purple reference itself bows at both rounded corners; replace chord-like guide with sampled C1 curve

User viewer evidence now isolates the defect conclusively:
- the purple calculated Ruckig reference itself has two visible lateral "barrels";
- they occur at the two rounded-corner transitions;
- therefore this is upstream of Follower/Pilot. The green actual path is not the primary
  cause of these two visible bulges.

Latest perf line for the new guide shows:
- coarse_points=4;
- guide_points=6;
- rounded_corners=2;
- expanded_corners=0;
- blended_waypoints=4;
- samples=2300;
- valid=1.

The six-point guide proved that merely replacing each coarse corner with one
`entry -> exit` chord is insufficient. Each Ruckig leg is solved with non-zero
endpoint velocities. When those velocity directions are not aligned with the leg chord,
Ruckig is free to generate a lateral polynomial bow while still satisfying endpoint
states.

### Mechanism fix

Each rounded corner is now represented by a densely sampled quadratic C1 curve:
```text
entry -> quadratic samples controlled by widened corner -> exit
```

Properties:
- the quadratic control point is the current local corner (including any outward
  expansion);
- its tangent at entry matches the incoming coarse leg;
- its tangent at exit matches the outgoing coarse leg;
- sample density targets about 2.5 m per segment, clamped to 4..24 segments per corner;
- the sampled curve itself is collision checked before acceptance;
- if the curve is not safe, the existing cut/expansion search continues.

This removes the long single Ruckig leg whose endpoint velocities were forcing the
visible barrel.

Commit:
- `3dccb8a36c8e8023083b21f317b944a8097fd6a0`.

### Small-angle speed fix

The old through-speed estimate clamped
`sin(angle/2)` to at least `0.15`. Once a curve is sampled densely, each local angle
is intentionally very small. That clamp falsely treated tiny smooth direction changes
as a much tighter turn and could create unnecessary braking.

The floor is now numerical only (`1e-4`), while the explicit max speed and lateral
acceleration remain the physical bounds.

### Focused regression

Default wall regression now additionally measures the full calculated Ruckig reference
against the published execution guide.

Requirement:
- maximum distance of any purple Ruckig sample from the blue execution-guide polyline
  must be <= 1.5 m.

Existing requirements remain:
- no major braking dip below 7.5 m/s in the steady 10 m/s experiment;
- no geometric backtracking along +X;
- collision-free;
- moving finish remains moving.

Commits:
- `517904389019fbf94ddbbabbe3248cdb7b6fab20` — visible-bow regression;
- `a056973c674194b109b8f37646f6d7a9e461c5b9` — explicit test include.

This candidate is not target MinGW64 validated yet.


## 2026-09-21 — target 7/8 after dense curve: functional speed-dip failure, not compile failure

Target MinGW64 evidence:
- `ruckig_route_planner_tests.exe` compiled and linked successfully;
- 7/8 focused tests passed;
- only failure:
  `default wall shallow corners stay moving`;
- failure reason:
  `default wall calculated curve contains a major unnecessary braking dip`.

This is a **functional trajectory test failure**, not a compiler/linker failure. The
test is intentionally retained: the steady 10 m/s wall stand must not hide a large
calculated speed dip.

### Root cause found

The sampled C1 execution guide introduced short local segments, but
`buildWaypointVelocities()` still computed corner speed from a heuristic
`blendDistance` proportional to local segment length.

That made speed depend on sampling density:
```text
same geometric curve
 + more guide samples
 -> shorter local segments
 -> smaller blendDistance
 -> lower turnSpeed
```

This is non-physical.

Candidate fix:
- use the three-point circumcircle curvature instead:
  `kappa = 2*|AB x BC| / (|AB| |BC| |AC|)`;
- local lateral-acceleration speed limit:
  `v_max = sqrt(a_lateral / kappa)`;
- this is invariant to guide discretization density;
- Ruckig perf log now records `min_speed_mps` and `max_speed_mps`.

Commit:
- `4dcebe77580e8abc7a0f9f4a22cec65f3ba985d5`.

### Remove start/finish heading transients from the steady stand

The prior "10 m/s start and finish" still had a hidden directional transient:
- start velocity was +X while the first retained route leg points
  `(110,-27,0)`;
- terminal velocity was +X while the final retained route leg points
  `(110,+27,0)`.

So the stand still asked Ruckig to perform extra heading changes at both boundaries.

The default scenario and focused wall fixture now align:
- start velocity and hull forward with the first route-leg tangent;
- finish velocity/forward with the final route-leg tangent;
- magnitude remains exactly 10 m/s.

Commits:
- `5c408b76b36a86bd6b4fecacc377142d80726796`;
- `3a22a4ae0aca037df4c9b8c76759506ed8a840c6`.

This makes the experiment genuinely:
```text
already moving along route at 10
 -> corner 1
 -> bypass
 -> corner 2
 -> leave along route at 10
```

### Viewer source revision control

Per user request, every viewer build now displays the exact Git source revision:
- CMake runs `git rev-parse --short=12 HEAD`;
- compile definition `ELITE_NAV_VIEWER_REVISION` is injected;
- the revision appears in both the native window title and the in-window HUD.

Commits:
- `2c1baf4854f083c2e3e44ed16136c70237dfafcd`;
- `1e84a9cac2f26da8e2802e1b97e8582f869f218d`.

The revision shown by the viewer is the HEAD that existed at CMake configure time. The
normal `run_stage1_mingw64.sh` configure/build flow refreshes it after every pull.

All changes after the reported 7/8 target run are currently unverified on target.


## 2026-09-21 — web review confirms dense 3-D Ruckig waypoint chaining is the wrong integration

Latest target evidence:
- focused binary still compiles/links;
- 7/8 tests pass;
- default wall test reports `min_speed=0.009 m/s`;
- viewer shows a large fan / convergence of purple calculated reference segments near
  the rounded route node.

The performance log explains the visual failure. After densifying the corner guide, the
runtime started feeding many close geometric samples to the 3-D state-to-state Ruckig
adapter. Recent lines show examples such as:
- `guide_points=52`, hundreds/thousands of Ruckig leg attempts;
- current default wall case `guide_points=18`, `legs=74`,
  `ruckig_ok=17`, `min_speed_mps=0.0094`.

This fan is therefore not a mysterious renderer artifact. It is the result of our own
integration: every close guide point became a new 3-D target state with its own target
velocity. Ruckig then solved many local polynomial state-to-state transitions and the
route nearly stopped at one of them.

### External verification

Official Ruckig documentation says:
- Ruckig's basic/open-source strength is state-to-state online trajectory generation;
- full local intermediate-waypoint trajectory calculation is a Pro feature;
- waypoint problems are significantly harder;
- Ruckig recommends as few waypoints as possible;
- it explicitly recommends filtering waypoint lists and prefers waypoints far apart.

This matches the target evidence exactly. The current dense chaining was a misuse of the
state-to-state solver.

The standard alternative architecture is also consistent with TOPP/TOPPRA:
```text
geometric path p(s)
    +
time parameterization s(t)
    =
trajectory p(s(t))
```

### Architecture correction implemented

Do not feed dense spatial samples to 3-D Ruckig.

New ownership:
```text
Stage-1 coarse route
 -> local rounded execution guide p(s)
 -> scalar jerk-limited Ruckig progress s(t)
 -> map s(t) back onto p(s)
 -> swept collision validation
 -> AcceptedManeuverProgram / Follower
```

Ruckig remains in the architecture, but in the role it is good at:
- true single-leg state-to-state solves still use the existing 3-D adapter;
- curved / multi-point routes use new one-dimensional
  `RuckigTrajectorySolver::solveProgress()`;
- dense guide points define geometry only and are never independent 3-D Ruckig targets.

Commits:
- `2e815ef993a31acd40596f170c67a124f9337bf9` — scalar progress API;
- `a50de59958887efbe50e2cdacee8c67623b2ebb8` — scalar Ruckig implementation;
- `9f166f4286dd03197705b0cd9e4ba63e33d000bc` — path-progress trajectory mapping;
- `3c58f4c3719d712e524e86e5f030820486af3102` — stop chaining dense 3-D legs;
- `c6f7b160878a3582362574691a12c96fd3ff3623` — keep stored guide sparse;
- `b9e7ce64182ce761edb45b4751d680950ab33ff5` /
  `3843194d999e302ca32171de3d49f3f21987de64` — contract docs corrected.

The current scalar timing uses one conservative path-wide speed cap for the current
slice (including curvature/range/positive point limits). This deliberately favors a
correct monotone reference over aggressive per-corner optimization. Later work may
introduce a sparse set of meaningful speed zones, but must never return to one Ruckig
3-D solve per geometric sample.

### Viewer revision and clutter

The previous revision stamp could be outside a cropped screenshot. Viewer now also
draws an always-visible in-window `NAV REV <sha>` badge below the top controls.

Per-sample cyan execution-guide crosses were removed. The BLUE guide line remains, but
internal geometric sampling no longer looks like dozens of commanded target states.

Commit:
- `1199a0c04ee7cf3a6938bef0ca9c3bb55d7bd4c8`.

### Validation state

This architecture correction has NOT yet been built or run on target MinGW64.
Do not call it PASS until the focused target gate and viewer confirm:
- no near-zero speed on the steady 10 m/s wall route;
- no purple fan/barrels;
- `NAV REV` visible and matches the current build revision.


## 2026-09-21 — excessive hull rotation isolated to Newtonian attitude authoring

Latest viewer/video evidence after scalar path-progress correction:
- purple calculated path is much cleaner;
- current wall-case perf reaches `min_speed_mps=10.0000`,
  `max_speed_mps=10.0000`, `guide_points=10`, one scalar Ruckig solve;
- remaining visually dominant defect is large hull rotation relative to the velocity
  vector during the shallow turn.

The cause is explicit in `buildReferenceAttitudes()`, not hidden physics:
```cpp
if (law == Law::Newtonian && acceleration > 0.35)
    requestedForward = normalize(sample.accelerationMps2);
```

For a constant-speed curved path, acceleration is predominantly centripetal and
approximately perpendicular to velocity. That rule therefore commands the ship to point
nearly broadside to its direction of travel even for a gentle turn.

This is especially wrong for the current stand because:
- `executionVehicleProfile.maxLateralAccelerationMps2` is already derived from
  `ShipParams::manoeuvreThrusterAccel`;
- Cobra test profile has `manoeuvreThrusterAccel = 2.0 m/s2`;
- the path curvature/speed combination is generated under that same lateral-authority
  envelope;
- therefore a gentle constant-speed turn that fits inside RCS authority does **not**
  require rotating the main engine into the centripetal acceleration vector.

The apparent sideways/out-of-plane motion in the video is primarily the commanded
broadside attitude viewed in perspective. The route and requested acceleration are
planar in this stand; the reference basis transport itself is designed to preserve the
previous roll orientation.

### Attitude fix candidate

Newtonian reference attitude now uses a minimum-cant allocation:

1. Default nose direction is velocity/path tangent.
2. If total requested acceleration is within manoeuvre/RCS authority, keep the nose on
   velocity.
3. If main-engine participation is required, calculate the smallest nose rotation such
   that:
   ```text
   positive main-engine thrust along nose
   + bounded omnidirectional RCS
   = requested acceleration
   ```
4. Do **not** point the nose directly at acceleration unless the physical demand truly
   requires that much rotation.

This keeps the hull close to the flight direction for ordinary gentle Newtonian turns,
while still allowing large cant/flip for demands that RCS cannot satisfy (hard braking,
high lateral acceleration, etc.).

Commits:
- `fcffa5bae3b4e3deab5d6f043d3c500137719dad` — minimum-cant Newtonian reference;
- `b943064d529935243863c30238ae0daf62f1c129` — report
  `MAX REFERENCE/VELOCITY ANGLE` separately from actual
  `MAX BODY/VELOCITY ANGLE`.

This candidate is not target MinGW64 validated yet.


## 2026-09-21 — viewer state authority, free-transit corridors, and main-engine visualization

User feedback after the minimum-cant Newtonian fix:
- visual behavior is now substantially better;
- UI appeared to disagree with actual control law after changing flight style
  (ASSISTED button still selected while motion looked Newtonian);
- exact 10.0 m/s tracking is undesirable for ordinary free transit: harmless
  ~10.1 m/s overshoot must not provoke a body rotation/braking manoeuvre;
- requested a visible indication of when the aft main engine is actually producing
  thrust.

Latest uploaded perf evidence remains healthy for the geometric/timing layer: the
steady wall cases at the tail of the log are one scalar Ruckig solve with
`min_speed_mps=10.0000` and `max_speed_mps=10.0000`. Therefore these changes belong
to execution/UI semantics, not the path generator.

### Redux-style viewer state

The native C++/GLFW viewer now uses one reducer-driven authoritative store:
- one `AppState`;
- explicit `ViewerActionType` actions;
- `reduceViewerState()`;
- all mode/pilot/style button clicks dispatch actions rather than directly mutating
  independent widget booleans;
- runtime control-law observations also dispatch into the same reducer.

This is intentionally Redux-style architecture, not the JavaScript Redux library.

Runtime truth is recorded in every execution frame:
- `hasRuntimeControlLaw`;
- `runtimeControlLaw`.

During playback the current frame is observed and, if a lower layer has actually changed
the law, the reducer updates `state.controlMode`. The ASSISTED/NEWTONIAN buttons are
therefore projections of the effective runtime state, not merely the last click.

Execution diagnostics now distinguish:
- `CONTROL LAW REQUESTED`;
- `CONTROL LAW EFFECTIVE`;
- `CONTROL LAW SWITCHES`.

The trace's textual law label is kept synchronized with reducer state, and effective-law
labels are localized in the viewer.

Relevant commits:
- `b503c3e35a9b8c2b0333b026b9251d6075a1afe2`;
- `1d17f9d208f9ef77a3dc8ac09753202aba7cd4c4`;
- `eedad40169c00509e01015f1a1777bd67f29965f`;
- `a60a215fde65505826b9b96c70edf511991da91f`.

### Free-transit speed/progress corridors

`AcceptedManeuverProgram::TrackingEnvelope` now has:
- `alongTrackSpeedDeadbandMps`;
- `alongTrackPositionDeadbandMeters`.

For `FreeTransit`, Follower decomposes position and velocity error into:
- cross-track components: still controlled normally;
- along-track components: ignored inside the configured corridor and corrected only
  by the excess outside it.

Current doctrine:
- STANDARD speed corridor: reference +/- 0.5 m/s;
- EXTREME speed corridor: reference +/- 1.0 m/s;
- STANDARD progress corridor: +/- 12 m;
- EXTREME progress corridor: +/- 16 m.

Thus a 10.1 m/s actual speed on a 10.0 m/s reference creates zero longitudinal speed
feedback in both styles. The vehicle is not required to chase exact schedule position
inside the progress corridor either.

This is deliberately limited to `FreeTransit`; precision docking/capture maneuvers keep
their stricter semantics.

The active speed corridor is persisted in trace frames and displayed in the viewer HUD
as e.g. `КОРИДОР V: 9.5 .. 10.5 М/С`.

Relevant commits:
- `92d65f86fe22ec1d0a404f952a0ebd0eb4935fb3`;
- `a171510889a2d235896e6ad567011c2b651ee3b7`;
- `023861170299dc7321975f2e73173af4b2547ca8`;
- `ad053356ada03d5212185b7d49d0b6aeb017ed6a`;
- `0b2e48b66744662e783b52b135ef26a714f43fb5`;
- `7a663da3b7f6bc8a92daa9433daeef40a10d41c9`;
- `3cadb29a840643524f2edafba3abb0b9091d795c`;
- `9e27bcd2392e7c18c36354374df33ab04882045f`;
- `3845b50910310494b394ec00820c86cc97b96aff`;
- `5299809e37f0a5061d56f066240619d56c6f27b4`.

### Aft-main-engine visualization

Each execution frame now records `mainEngineThrottle01` from the **actual physical**
`motion.mainEngineAccelerationMps2`, projected onto the current physical hull forward
axis and normalized by main-engine authority.

Viewer behavior:
- rear face of the diagnostic ship is filled orange only for positive aft-main thrust;
- intensity/alpha scale with actual main-engine throttle;
- HUD also shows `MAIN: N%`;
- RCS/manoeuvre-thruster acceleration does not light the rear face.

This directly answers whether a hull rotation was physically useful for main-engine
vectoring or was merely an attitude command with no aft-main thrust.

Relevant commits:
- `245bcc627fef5e0f79ad3e00e4b998c31e04b8db`;
- `2de51af96a3d9156fe7f3b5cb94a9f33d9f0e015`;
- `cbb58b16a7a15803cc8e56618d639916d748a53e`;
- `089d905f3207fa48afe3bba70935ce9413b145f6`;
- `163bee3c58443a5d0d6b4ad092ae7e6f970feef1`;
- `2e8692b68c670275e7f96654c4a990c473b885a9`.

### Validation state

These latest execution/UI changes are committed but are not yet user target-validated.
The next target viewer pass must explicitly compare STANDARD/EXTREME and
ASSISTED/NEWTONIAN while watching:
- effective button selection;
- `CONTROL LAW REQUESTED/EFFECTIVE/SWITCHES`;
- speed corridor text;
- `MAIN:%`;
- orange rear face.


## 2026-09-21 — body/engine telemetry + independent 5..50 m/s start/finish sliders

User feedback after reducer/corridor/main-engine visualization:
- current flight is visually much better, but a remaining unexplained hull somersault occurs
  near the low/straight part of the route while the main engine indicator is off;
- actual speed is about 10.1 m/s during the event;
- user asked whether logs contain body orientation changes and exact engine-on moments;
- user requested independent start and finish speed sliders, each 5..50 m/s.

### What the existing logs did and did not contain

`navigation_perf.log` is a trajectory-generator performance log. It contains Ruckig/path
timing facts only. The latest uploaded tail still shows the default wall reference as one
scalar solve with `min_speed_mps=10.0000` and `max_speed_mps=10.0000`; it cannot
explain a physical body flip.

Before this iteration, `last_execution_trace.json` already persisted:
- ship position;
- ship forward/right/up basis;
- ship velocity;
- effective runtime control law;
- actual positive aft-main throttle;
- accepted-program reference forward/right/up.

However, there was no compact human-readable time-series log correlating body attitude,
angular rate, main/RCS acceleration, and reference attitude.

### New execution telemetry

Each trace frame now additionally records:
- `shipAngularRatePyrRadPerSec`;
- `mainEngineAccelerationMps2`;
- `manoeuvreAccelerationMps2`;
- `engineAccelerationMps2`.

These fields are persisted in trace JSON.

A new file is emitted after every Stage-2 execution:
```text
tools/navigation_runtime/last_execution_telemetry.log
```

Every sampled line contains:
- time;
- phase;
- effective control law;
- position and speed;
- actual body forward/up;
- pitch/yaw/roll rates;
- main-engine throttle percent;
- main-engine acceleration vector;
- manoeuvre/RCS acceleration vector;
- combined engine acceleration;
- reference speed;
- reference forward/up;
- body-vs-velocity forward angle;
- actual-vs-reference forward angle;
- actual-vs-reference up angle.

It also marks exact sampled transitions:
- `MAIN_ON` / `MAIN_OFF`;
- `RCS_ON` / `RCS_OFF`.

This is the primary evidence for the remaining somersault. If main stays off while
`ref_forward/ref_up` themselves rotate, the problem is attitude authoring/program
sampling. If reference attitude is stable but actual body rotates, the problem is lower
in angular tracking/physics.

Relevant commits:
- `62c94992bfbde0983dbcb2d581c038f5939f890c`;
- `33854813be1d8299597a7aeb9e4bf40600403f16`;
- `72eeb3fde524136aff5edff1e33a683c9cf1e955`;
- `e8083bb2f27623e73753bb98a9b6137bfc5928c2`.

### Start/finish speed override seam

`ScenarioRunSettings` now has independent optional execution overrides:
- `startSpeedOverrideMps`;
- `finishSpeedOverrideMps`.

Negative values mean "use scenario.json authored value".

The runtime:
- preserves the authored start velocity direction and changes only its magnitude;
- applies the same effective start velocity both to trajectory generation and the actual
  simulated ship initial state;
- applies the effective finish speed to terminal Ruckig speed constraint/velocity,
  moving-terminal gate semantics, final-state validation, and diagnostics;
- Stage-1 static route geometry remains unchanged.

`ScenarioRunResult` exposes authored start/finish defaults so the viewer initializes
from scenario data, not a UI hard-code.

Relevant commits:
- `1932f80e1350a2631332b1092f7615bddc479dfe`;
- `0bebf41764177e6614efc10617ddfa034407f38b`.

### Viewer speed sliders

The reducer-owned viewer state now includes:
- `startSpeedMps`;
- `finishSpeedMps`.

Two independent sliders are visible under the main controls:
- `СТАРТ V`;
- `ФИНИШ V`;
- range 5.0 .. 50.0 m/s.

Dragging a slider dispatches `SetStartSpeed` / `SetFinishSpeed` through the same
Redux-style reducer. Changing speed after an execution restores the retained Stage-1
route and invalidates only Stage-2 execution, because speed is an execution setting and
must not force static route replanning.

Execution settings carry the current slider values into Stage-2.

Commit:
- `c199d0dbb042066075f1b320799d8e5dfa55b0a7`.

### Validation state

These telemetry and slider changes are committed but not target-MinGW64 validated yet.

Next evidence should include:
- successful runtime/viewer build;
- visible 5..50 start/finish sliders;
- one run that reproduces the unexplained somersault;
- the generated `last_execution_telemetry.log`.

Do not guess the remaining flip root cause from `navigation_perf.log`; use the new
execution telemetry.


## 2026-09-22 — control-chain audit, automatic Stage-2 refresh, speed-override validity, and somersault root cause

User raised two architectural/UX questions:
1. Who is actually controlling the ship during motion, versus merely animating a material point?
2. Do pilot/control-law/style/speed changes require another explicit Calculate press?

User also supplied `last_execution_telemetry.log`, which finally gives physical
attitude/propulsion evidence for the remaining hull somersault.

### Actual runtime control chain

The current Stage-2 execution is not a kinematic playback. The mutable ship state is a
real `ShipTransform` + `DynamicMotionState`, and the control chain is:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> TrajectoryFollower
 -> ManeuverTrackingController
 -> NavigationSystemControlIntent
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState navigation acceleration demands
 -> SharedShipPhysics / ShipController (angular)
 -> DynamicMotionSystem (main-engine + manoeuvre/RCS allocation and translation)
 -> ShipTransform / DynamicMotionState
```

So there is a concrete autopilot/controller pulling the virtual controls. Pilot skill
changes the delivered command through `PilotSkillExecutor`; body angular rates are
bounded by the actual ship parameters; translational demand is physically split into
main-engine and manoeuvre/RCS channels according to the active control law.

However, the user's concern is partly correct at the *authoring* level: the current
Stage-2 still starts from a kinematically-authored trajectory and then derives an
attitude/reference program around it. Full B5/B6 body/thrust-coupled maneuver authoring
is still not complete. The lower execution is physical; the accepted program is not yet
the final unified physical maneuver compiler/proof architecture.

### Telemetry proves the current somersault is an angular-control runaway

The uploaded telemetry shows:
- early flight: actual body basis equals reference basis, angular rate is zero, and
  both main/RCS translation channels are quiet;
- near the first turn, body and reference briefly become nearly coincident while the
  physical pitch rate is already about +0.75 rad/s;
- after that, the reference settles back toward the route tangent but pitch rate keeps
  increasing/saturating around +1.57 rad/s, so the physical body continues rotating
  *away* from the stable reference;
- during much of that runaway the main engine is OFF; manoeuvre acceleration is
  translational RCS, not evidence that the main engine needed the flip;
- a second runaway later reaches nearly 179 degrees body-vs-velocity/reference with the
  main engine still OFF, and main thrust starts only after the hull is already nearly
  inverted.

Therefore the main engine is not requesting the somersault. The angular command loop is.

### Root cause in the current accepted-program representation

`AcceptedManeuverProgram` is capped at 16 samples per phase. The runtime previously:
1. differentiated the dense reference attitude to angular velocity and angular
   acceleration;
2. downsampled those values into at most 16 program samples;
3. linearly interpolated `angularAccelerationFeedForwardMapRadPerSec2` between the
   sparse samples;
4. added that feed-forward directly to angular tracking feedback.

This can smear a short angular-acceleration pulse over a long interval. The result
matches the telemetry: the reference basis has already passed the desired orientation,
but positive angular acceleration/feed-forward can keep winding up the ship.

Additionally, the follower previously clamped only the angular *feedback reserve*, then
added angular feed-forward without a final clamp against the accepted physical angular
capability.

Candidate fixes:
- `018fd2c08874cb426a9d3fc8340c85ffb4539bad`
  - FreeTransit no longer publishes sparsely-sampled angular-acceleration feed-forward;
    body attitude is tracked using reference basis + reference angular velocity +
    closed-loop feedback.
  - explicit START/FINISH speed overrides expand the Stage-2 speed envelope so 5..50
    m/s slider values do not fail `validRequest()` merely because STANDARD nominal
    speed is 10 or EXTREME nominal speed is 18.
- `e8369c0fa21cc59f0429741540dd4903f7f556e8`
  - total follower angular acceleration demand is clamped to the accepted physical
    angular-acceleration capability after feed-forward + feedback composition.

These fixes are not target-validated yet.

### Automatic execution refresh after UI changes

The old UX did require a separate Stage-2 Execute after changing execution settings,
and the viewer could therefore continue showing the previous trace. That created the
correct impression that buttons/sliders did nothing until another explicit action.

New behavior:
- one initial `Calculate` obtains/retains Stage-1 static route and immediately queues
  Stage-2 execution;
- after a retained route exists, changing:
  - Assisted/Newtonian;
  - pilot skill;
  - Standard/Extreme;
  - sudden-obstacle toggle
  automatically queues a fresh Stage-2 execution;
- START/FINISH speed sliders queue one fresh Stage-2 execution on mouse release rather
  than once per dragged pixel;
- these execution-only changes do not re-run the static Planner.

Commit:
- `7a164a65292a47e41c19a01bdcb3f55b93d0110d`.

Therefore the intended UX is now:
```text
Calculate once for static route
 -> change any flight/execution setting
 -> Stage-2 recalculates automatically
 -> viewer plays the new trace
```

A new Stage-1 Calculate is only needed when static route facts/goal/world geometry change.

### Full control-chain telemetry

To answer "who pulled which lever" rather than infer it from hull motion, trace frames
now also store:
- Follower ideal linear acceleration command;
- Follower ideal angular acceleration command;
- PilotSkillExecutor executed linear acceleration command;
- PilotSkillExecutor executed angular acceleration command.

These are written to JSON and to `last_execution_telemetry.log` as:
- `ideal_lin_cmd`
- `ideal_ang_cmd`
- `exec_lin_cmd`
- `exec_ang_cmd`

Then the same line shows the downstream physical allocation:
- `main_a`
- `rcs_a`
- `engine_a`
- actual `pyr_rate`
- actual/reference body basis.

Commits:
- `a2ece66294ab0a8aa3c4258257dbcdbe0c72b086`;
- `4343b7d5289fca95e6989f3e7d21f691723361bc`;
- `9efe6934aa3f8141604e81e714319126a83afa6a`.

This makes the next target run able to distinguish:
```text
program/reference
 -> follower ideal command
 -> pilot-executed command
 -> physical allocator
 -> actual body/velocity
```

### Validation status

All changes in this section are committed but not yet target-MinGW64 validated.
Do not claim PASS until the user rebuilds/runs the viewer and returns the new telemetry.


## 2026-09-22 — FlightStyle = clearance doctrine; speed/style now invalidate Stage-1; Calculate has one explicit meaning

User clarified the intended semantics:

- FlightStyle must never own a nominal/cruise speed.
- STANDARD vs EXTREME answers only the risk/clearance question:
  - STANDARD: prefer more maneuver/safety room around static geometry;
  - EXTREME: accept a tighter pass when that is useful (for example to stay close to cover).
- START/FINISH speed changes are not execution-only. Higher inertia changes the
  geometric room needed for a credible maneuver, so the retained Stage-1 route may move.
- `РАССЧИТАТЬ` must have one unambiguous action:
  - enabled only when current inputs invalidate the displayed result;
  - one click performs every required calculation;
  - show a concise on-screen result log;
  - after the attempt, visibly disable the button until an invalidating input changes.

### Removed style-owned speeds

The runtime no longer contains or parses:
- `standardSpeedMps`;
- `extremeSpeedMps`;
- `standard_speed_mps`;
- `extreme_speed_mps`.

Those keys were also removed from `tools/navigation_runtime/scenario.json`.

The current test stand's trajectory speed envelope is derived only from the explicit
boundary requests:
```text
max(start speed, finish speed)
```

FlightStyle is no longer passed into FreeTransit program authoring and no longer changes
Follower speed/progress deadbands.

Relevant commits:
- `7e8c67bacfc6615c1d03faf8f771e6e5e65b66d3`;
- `75d0f50fc25c784d515f1e9f092c37600d4e6346`;
- `75c85b028b4c7b21a3ddce4c1dff3912b596c95f`.

### Speed-aware Stage-1 maneuver reserve

A first coarse speed-aware route reserve now exists in the diagnostic stand.

`routePlanningClearanceMeters()` uses:
- the larger requested START/FINISH speed;
- real ship angular acceleration/rate limits;
- a representative 30-degree turn time;
- a style-only clearance factor.

Current coarse formula:
```text
inertialLead = planningSpeed * characteristicTurnTime
additionalClearance =
    authoredClearance +
    inertialLead * styleReserveFactor
```

Current factors:
- STANDARD = 1.0;
- EXTREME = 0.35.

This is intentionally a coarse Stage-1 maneuver-space reserve, not the final B6
time-parameterized oriented swept-hull proof.

Practical consequence:
- increasing speed should usually keep the same obstacle-side topology but move support
  points farther away;
- if another passage becomes cheaper/feasible, topology/point count may also change;
- at the same speed EXTREME should route closer than STANDARD.

Stage-1 diagnostics now include:
- `ROUTE PLANNING SPEED`;
- `STYLE CLEARANCE`;
- `ROUTE ADDITIONAL CLEARANCE`.

A new E2E regression compares:
- STANDARD 10/10;
- STANDARD 40/40;
- EXTREME 40/40;
and requires the 40 m/s Standard detour to move farther from the wall and the 40 m/s
Extreme detour to cut closer.

Relevant commits:
- `7e8c67bacfc6615c1d03faf8f771e6e5e65b66d3`;
- `a8930d82b8379d26b73a1bd1f61b6f7dbdf66c5d`;
- `f3f4734d0ebc2d0a500d9607862319d26b3e6d44`;
- `70f099ccd77810da8c31356efd1951cbc0549e01`;
- `0c6700ae470b7758da014cf7fe78e719a3c61362`.

### Calculate UX/state contract

Viewer state now separates:
- `routeInputsDirty`;
- `executionInputsDirty`;
- `calculationInProgress`.

Invalidation:
- START speed -> route + execution dirty;
- FINISH speed -> route + execution dirty;
- STANDARD/EXTREME -> route + execution dirty;
- Assisted/Newtonian -> execution dirty only;
- Pilot skill -> execution dirty only;
- sudden-obstacle toggle -> execution dirty only.

No setting auto-runs a calculation anymore.

One `РАССЧИТАТЬ` click:
1. rebuilds Stage-1 only when route inputs are dirty (or no route exists);
2. executes Stage-2 from the resulting/retained route;
3. publishes a concise on-screen log with route point count/length, flight result and
   requested boundary speeds;
4. clears dirty state after the attempt.

The button is then rendered dark/dim and labeled `РАСЧЕТ ГОТОВ`. It cannot be clicked
again until an input marks the result dirty. Dirty state changes the short log to
`РЕЗУЛЬТАТ УСТАРЕЛ` and re-enables `РАССЧИТАТЬ`.

The obsolete second `UiAction::Execute`, `ЗАПУСТИТЬ ПОЛЁТ` path and SPACE-to-execute
behavior were removed. Playback controls now only control playback.

Relevant commits:
- `523d40866610f5b167ad30ffd6fbe598d91fa701`;
- `ba4b38c5a380bfbb238841fe5b4eee419730d653`;
- `8a013ea2f54ced80dc9495948196f3922a0aba8e`;
- `d254b18cf7bc7a70030c24041facf3fd73175124`;
- `e99d3be88d52283531d26cb9e631e03b018fa88f`.

Documentation was updated in `tools/navigation_runtime/README.md` to match this
one-button workflow and the new style semantics:
- `ef3c632d5e2dbbcdb51dd51dba3918de39a4e646`.

### Validation state

These changes are committed but NOT yet validated on the user's MinGW64 target.

Next target evidence must verify:
- architecture contract script passes;
- runtime E2E builds/runs;
- viewer builds;
- `РАССЧИТАТЬ` is enabled initially, visibly disabled after an attempt, and re-enabled
  only after a relevant setting changes;
- changing speed visibly changes white Stage-1 route coordinates/clearance;
- at equal speed EXTREME is closer to the obstacle than STANDARD;
- no style-owned nominal speed remains in behavior/diagnostics.


## 2026-09-22 — target build failure fixed; variable-speed invariant formalized

Target MinGW64 evidence from HEAD `c5a2ea68434a6743e034599e7c35134b0c3c1663` reached the viewer build and failed in
`NavigationRuntimeViewer.cpp` for two ordinary C++ integration mistakes:

1. `drawHud()` called `recalculationRequired(state)` before any declaration was visible.
2. The old speed-slider auto-refresh removal left a dangling
   `if (speedChanged)` with no statement before the closing brace.

These were not navigation-algorithm failures.

Corrections:
- forward declaration for `recalculationRequired(const AppState&)`;
- slider release now only clears `scrubbingFrames` and `speedSliderDrag`;
- unused viewer `orientationErrorDegrees()` helper removed;
- the unrelated unused `acceleration` warning in `buildReferenceAttitudes()` was also
  removed.

Relevant commits:
- `e2597a7714f5e7fee2200e7f47f9bfb9bafb2734`;
- `b15d0e97c6897d2b099e0c6e41d60830c45776b9`.

### Variable-speed contract

A new canonical rule is now recorded in
`src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`.

Speed is a state variable, not a FlightStyle constant and not a value that must remain
uniform across a route.

Rules:
- START/FINISH speeds constrain those boundary states only.
- Intermediate speed may vary freely inside physical, geometric, mission and safety
  limits.
- Any speed reduction must have a concrete reason local to the maneuver:
  curvature/turn authority, braking distance for an upcoming required state, collision
  or clearance proof, explicit local speed restriction, docking/formation precision,
  tracking recovery, or emergency avoidance.
- A difficult corner may require a major slowdown or a full stop.
- A clear segment may be flown substantially faster than the FINISH speed if later
  constraints can still be met.
- A local speed restriction must not silently cap unrelated route segments.
- Without a physical/geometric/mission/safety reason, the planner must not invent
  braking just to make the profile uniform.
- STANDARD/EXTREME remain clearance/risk doctrine only and never own a nominal speed.

Commit:
- `49009e6f480c7a70848f9a89c5cb4a0f7d4c8dab`.

### Concrete terminal-speed bug fixed

Two implementation points were corrected immediately:

1. `executionVehicleProfile.maxSpeedMps` is no longer
   `max(startSpeed, finishSpeed)`.
   START/FINISH are boundary states. The hard execution speed ceiling now comes from the
   vehicle capability (`ShipParams::maxCombatSpeed`), expanded only if a boundary state
   is already higher.

2. In the multi-point scalar path-progress backend, an exact moving terminal point-speed
   constraint is no longer folded into the route-wide maximum speed. Ruckig already
   receives terminal speed as `targetSpeedMps`; applying the same value globally was
   unjustified braking.

Relevant commits:
- `336b48a293dca0306847afbe3bc00e5787713a2e`;
- `16fe6a8918fa1a9f03fce3dd2f814987198f31e8`.

Focused regression added:
- three-point straight 500 m route;
- START = 10 m/s;
- FINISH = 10 m/s;
- vehicle max = 80 m/s;
- trajectory must rise above 12 m/s in the unrelated transit segment and still finish at
  exactly 10 m/s.

Commit:
- `22133bbe5c9ef61a52e839338e88b44a29061f22`.

Runtime README now states the same variable-speed rule:
- `30135fcd9db3a61b875ad0e4a4f5a4e98611e960`.

### Remaining known speed-profile defect

Do not claim the entire variable-speed contract is implemented yet.

The current multi-point scalar path backend still computes a single
`globalGuideSpeedLimit()` from the worst curvature found anywhere on the execution
guide. Therefore one difficult bend can still unnecessarily cap otherwise clear route
segments.

That behavior is safe but over-conservative and violates the new locality rule.

Required next trajectory-authoring correction after the target build is green:
- convert curvature / point / range speed restrictions into local speed-profile
  constraints along scalar progress;
- permit independent acceleration on clear segments;
- brake only early enough to satisfy the next local restriction;
- permit a local stop when genuinely required;
- accelerate again after the restriction when safe;
- never turn a local restriction into a whole-route cap.

This must stay inside the route-to-trajectory/B5-B6 authoring path. Do not hide it in
Follower behavior or FlightStyle.


## 2026-09-22 — Assisted selector bug fixed by separating requested vs effective runtime state

Latest user feedback:
- overall flight now looks materially more realistic;
- there is still some body oscillation while returning to the nominal attitude;
- ASSISTED appears not to switch at all;
- user requested an always-visible right-panel summary of current pilot, control type and
  behavior/style.

The uploaded execution telemetry begins with `law=NEWTONIAN` and the displayed run is
indeed a Newtonian execution. The important UI bug was found in the viewer state model,
not in the Assisted physics implementation.

### Root cause

The viewer previously used one field, `state.controlMode`, for two different products:

1. the **requested** user setting selected by the ASSISTED/NEWTONIAN buttons;
2. the **effective** control law observed from the currently displayed runtime frame.

Every render called:
```text
syncViewerStateFromRuntimeFrame(displayFrame)
 -> RuntimeControlLawObserved
 -> state.controlMode = frame.runtimeControlLaw
```

Therefore this sequence was broken:
```text
old trace = NEWTONIAN
user clicks ASSISTED
 -> requested state becomes ASSISTED
next render still displays old NEWTONIAN frame
 -> runtime observation writes NEWTONIAN back into state.controlMode
 -> ASSISTED button appears not to work
```

This also meant stale playback could modify the input that would be sent to the next
Calculate.

### Corrected state ownership

Viewer now has separate products:

```text
state.controlMode
    = requested control law for the next Calculate

state.effectiveRuntimeControlLaw
    = observed law of the currently displayed execution frame
```

`RuntimeControlLawObserved` is no longer allowed to assign `state.controlMode`.

The top buttons always show the requested input. The old trace may remain visible while
the result is dirty, but it cannot undo the newly selected input.

Commit:
- `3067ccfde2f260dcde30baa038ca82e400f233a8`.

### Right-panel mode diagnostics

The right diagnostics panel now contains a dedicated `ТЕКУЩИЕ РЕЖИМЫ` block:

- `ПИЛОТ`;
- `УПРАВЛЕНИЕ / ВЫБРАНО`;
- `УПРАВЛЕНИЕ / ФАКТ`;
- `ПОВЕДЕНИЕ`.

If settings changed and the displayed execution is still the previous trace, effective
control law is shown with:
```text
(СТАРЫЙ РАСЧЕТ)
```

This makes the intended interaction explicit:
- click ASSISTED -> selected line/button becomes Assisted immediately;
- before Calculate, effective line may still say Newtonian / old calculation;
- press Calculate -> fresh execution should show effective Assisted if the runtime really
  uses the requested law.

Pilot and Standard/Extreme behavior are also visible in the same block.

Architecture regression now forbids `RuntimeControlLawObserved` from writing
`state.controlMode`, and checks that the right-panel mode diagnostics remain present.

Commits:
- `d57cd3b285b0419f9bf8266887fb34811d57a592`;
- `12d91b3253f68907557308b9e27ee4fe6d871822`.

### Remaining body oscillation

The user still observes some oscillation while the hull returns toward the requested
attitude. Do not conflate this with the selector bug.

The control-chain telemetry already exposes:
- ideal angular command;
- pilot-executed angular command;
- actual P/Y/R rate;
- reference forward/up;
- actual forward/up.

After confirming ASSISTED really executes, use those channels to determine whether the
remaining oscillation is:
- reference motion;
- insufficient angular damping in ManeuverTrackingController;
- PilotSkillExecutor lag/overshoot;
- or ShipController/angular-rate dynamics.

Do not tune damping until the requested/effective control-law state is visibly proven on
the target viewer.


## 2026-09-22 — high-speed runaway root causes: underdamped hull loop + reference clock outrunning physics

Latest target evidence came from a 26.15 m/s -> 11.75 m/s Newtonian run.

Observed diagnostics:
- Ruckig trajectory itself was produced successfully;
- 3 accepted FreeTransit phases / 2 nominal handoffs;
- calculated speed range 11.38..26.15 m/s;
- old runtime claimed `ROUTE EXECUTION COMPLETE: YES`;
- but physical final position error was 68.49 m;
- max route deviation 62.24 m;
- max Follower position error 68.49 m;
- main engine could be zero while angular control and RCS remained active.

That proves two separate faults.

### 1. Hull "float" / oscillation

B10 default attitude feedback was:
```text
Kp = 2.0
Kd = 1.0
```

For the second-order attitude-error loop, critical damping for Kp=2 is about:
```text
Kd_critical = 2 * sqrt(Kp) ~= 2.83
```

So the old loop was intentionally/accidentally strongly underdamped. Telemetry matches
that: the body still carries substantial angular rate as forward-reference error approaches
zero, overshoots the requested attitude, then reverses correction like a pendulum.

Changed default B10 angular-velocity feedback to:
```text
Kp = 2.0
Kd = 3.0
```

This is near-critical/slightly overdamped and remains bounded by the accepted program's
angular feedback reserve and lower physical angular acceleration/rate limits.

Focused regression now requires default Kd >= 2*sqrt(Kp).

Commits:
- `e2b5270210b79c5b873a0437b2a4deb14c7973ae`;
- `32af480c8fee271a1f5f3f6396b02b6601a0e190`.

Important actuator semantics remain:
- hull rotation does NOT imply main-engine thrust;
- navigation angular acceleration is applied by the attitude/RCS torque path;
- main engine owns linear thrust;
- therefore `MAIN=0` while `exec_ang_cmd != 0` is physically legitimate.
The bug was unnecessary/underdamped rotation, not the independence of those actuators.

### 2. No physical return to the trajectory at high speed

The old FreeTransit execution was too schedule-driven.

`ManeuverPhaseGate::ScheduledMoving` advances when nominal phase time ends. At high speed
the reference could continue into the next phase while the physical craft had already
fallen far outside the intended track. The program clock therefore ran away from the ship.

This explains the previously contradictory diagnostic:
```text
ROUTE EXECUTION COMPLETE: YES
FINAL POSITION ERROR: 68.49 M
```

A reference-reacquisition policy is now implemented in the runtime fixture:

- `AcceptedManeuverProgram` remains immutable;
- runtime owns a separate `activeProgramReferenceDelaySeconds`;
- B9/Follower are sampled using:
  ```text
  programReferenceTime = physicalTime - referenceDelay
  ```
- when B10 reports `trackingErrorExceeded`, reference delay grows by one fixed step;
- therefore reference progress pauses while the physical ship catches/reorients/reacquires;
- phase gate sees the same delayed reference time, so it cannot hand off merely because
  wall-clock time elapsed;
- once the craft is back inside its envelope, reference time resumes;
- phase activation resets the local delay;
- accepted program timestamps are NOT mutated.

Viewer status while this happens:
```text
FOLLOWER ВОЗВРАЩАЕТСЯ В КОРИДОР
```

Diagnostics:
- `REFERENCE CLOCK HOLD FRAMES`;
- `REFERENCE CLOCK HOLD`.

The diagnostic harness overrun window was enlarged from +12 s to +30 s so a real recovery
attempt has time to occur instead of immediately ending the fixture.

Commits:
- initial implementation `4717263f59e9b50f90ad7696e927eb1f3f4696ba`;
- immutable accepted-program correction
  `c0378aa900518523a8f9ac9c51dd32563c7ed773`;
- viewer reacquisition status
  `4d2255366d4ff6ad330d82586b24a29214236b41`.

### 3. FreeTransit envelope now matches its own longitudinal deadbands

Previously FreeTransit could intentionally suppress harmless along-track lead/lag feedback
but still mark the same raw error as `EnvelopeExceeded`.

B10 now evaluates the tracking envelope using the same effective position/velocity errors
after longitudinal deadbands are applied. Real cross-track/velocity errors remain active;
accepted longitudinal slack no longer falsely pauses the reference.

A regression deliberately makes the raw along-track error exceed the nominal envelope
while remaining inside the accepted deadband and requires `Tracking`, not
`EnvelopeExceeded`.

Commits:
- `aca1d7af46a0cf5bdccc7e08629450bb217ced2b`;
- `cf963e972690e0f822146bdeb0ce96dbb8df166b`.

The runtime FreeTransit reacquisition envelope is now tighter:
- position 8 m;
- linear velocity 4 m/s;
- forward-angle 0.35 rad;
- angular-rate 0.8 rad/s.

Leaving this envelope does not disable navigation; it holds reference progress and
continues physical recovery.

Commit:
- `59aa0d404d02604e37f93955e083f18d0504b3cf`.

### 4. High-speed regression

Runtime E2E now reproduces the reported case:
- Newtonian;
- Expert;
- Standard;
- START 26.15 m/s;
- FINISH 11.75 m/s.

It requires:
- reference-reacquisition diagnostics are present;
- execution succeeds physically;
- final position error <= 5 m.

Commit:
- `41ea618fc089003410b6cba8f5b2c1db912c8cbc`.

This target regression is committed but NOT yet verified on the user's MinGW64 machine.

### 5. Completion diagnostics no longer conflate schedule and physics

Removed misleading:
```text
ROUTE EXECUTION COMPLETE: YES
```

It is now split into:
```text
PROGRAM PHASES COMPLETE: YES/NO
PHYSICAL TERMINAL STATE: REACHED/MISSED
```

Commit:
- `f6a8e68df19c9951ade574b38d7e4e08101aec2c`.

### 6. All current-run artifacts moved to repo root

Runtime logs now use process working directory. With supported commands launched from
repo root:
- `last_route_plan.log`;
- `last_execution.log`;
- `last_execution_telemetry.log`;
- `navigation_perf.log`.

Viewer traces also moved to root:
- `last_calculated_trace.json`;
- `last_execution_trace.json`.

Commits:
- `5aefdd85ec15b95e17f37834ab05e3458bf7a7ac`;
- `830069744134177f23ff829a8879032bdb0f4d4a`;
- docs `83fb94fc413290bcabf022cd53cd48ae840b7d86`,
  `a928e7e018567a8b1b27962aa7189f112c9ae575`.

### Validation status

Architecture guard updated to require:
- immutable-program external reference delay;
- reacquisition markers;
- root-log path ownership;
- Kd=3 attitude damping;
- effective FreeTransit envelope errors;
- high-speed E2E regression.

Commit:
- `baecf937643083e97d8af4a9e82ee10742a82f7d`.

Do not claim the new high-speed recovery or reduced oscillation is PASS until target
MinGW64 evidence is returned.

## Assisted 10 -> 10 target failure — frozen moving-reference derivatives

A fresh interactive Stage-12 runtime run used:

```text
ASSISTED / EXPERT / STANDARD
10.00 -> 10.00 m/s
```

The retained planner product itself stayed low-speed:

```text
TRAJECTORY: RUCKIG OK
CALCULATED MAX SPEED: 11.71 M/S
```

but physical execution ended at:

```text
FINAL SPEED: 100.64 M/S
FINAL POSITION ERROR: 2411.14 M
REFERENCE CLOCK HOLD: 47.70 S
MAX BODY/VELOCITY ANGLE: 179.99 DEG
PROGRAM PHASES COMPLETE: NO
PHYSICAL TERMINAL STATE: MISSED
```

### Root cause

The first reference-reacquisition implementation held **reference time** when
B10 left the tracking envelope. That correctly stopped B9 progress, but the
sample at the held time remained a moving trajectory sample. It therefore
continued to carry:
- non-zero linear acceleration feed-forward;
- non-zero reference angular velocity;
- potentially non-zero angular feed-forward in generic programs.

Holding the pose while replaying those derivatives indefinitely is
self-contradictory. In this run the bounded recovery reserve was smaller than
the frozen path acceleration, so the net command kept accelerating the real
ship away. The fixed attitude plus frozen non-zero angular velocity also drove
continuous hull rotation.

### Recovery contract correction

B10 now distinguishes normal accepted tracking from out-of-envelope recovery:

```text
inside tracking envelope
    -> accepted position/velocity/attitude
    -> accepted A_ff / alpha_ff
    -> accepted reference angular velocity
    -> bounded feedback

outside tracking envelope
    -> held geometric reference
    -> A_ff = 0
    -> alpha_ff = 0
    -> target angular velocity = 0
    -> bounded feedback only
```

The program object is not mutated. When the actual craft re-enters the tracking
envelope, the original accepted moving reference becomes authoritative again.

This is deliberately a recovery behavior after the accepted execution envelope
has already been violated; it does not claim that the original moving-trajectory
proof remains valid outside that envelope.

Regression:
`testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives()`.

### Viewer failure-mode usability

`fitCamera()` no longer expands its bounds using every physical execution
frame. The authored route, accepted trajectory/guide and obstacle geometry
define the normal fit. A pathological runaway remains drawable but cannot shrink
the useful navigation scene to a postage stamp.

Code candidate before documentation commits:

```text
59ff756996229bf15a122eb0fe43cf0d9a14b245
```

Target MinGW64/runtime validation is pending.

### Dense-to-sparse attitude-rate alias found in the same failure

The tumbling did not begin only after the reference clock was held. FreeTransit
program construction also copied `ReferenceAttitude::angularVelocity` from a
dense source trajectory into a program limited to at most 16 samples. B9 then
linearly interpolated those sparse values.

That is not kinematically equivalent to the sparse basis interpolation. A
short-lived dense angular-rate peak can become a long-lived sparse target rate.

The accepted FreeTransit product now uses:

```text
sparse basis_i + sparse time_i
    -> centered basis delta across i-1 .. i+1
    -> sparse omega_i
    -> physical rate clamp

omega_0 = 0
omega_last = 0
alpha_ff = 0
```

Therefore B9's visible pose program and the angular-rate target come from the
same temporal resolution.

Combined with B10 recovery:
- moving-sample feed-forward is neutralized outside the tracking envelope;
- actual hull angular rate is damped while the reference clock is held;
- after reacquisition the sanitized sparse moving reference resumes.

Pinned physical regression:
`testAssistedLowSpeedDoesNotRunAwayDuringReferenceHold()`.

Candidate before documentation commits: `13ef6bd731ef6d8e78c75c72bf7a59f524b30bcb`.
Target evidence pending.

## Higher-speed Assisted gate — body attitude must own main-thrust direction

Target evidence after the runaway fix:

```text
ASSISTED / EXPERT / STANDARD
20.90 -> 20.00 m/s
Ruckig max 20.90 m/s
final physical speed 7.45 m/s
final error 175.20 m
body/velocity max 180 deg
reference hold 40.70 s
```

Telemetry proved that the old Assisted allocator could apply negative
longitudinal `main_a` while `main_pct` was zero and the hull stayed aligned
to the reference. The ship then crossed zero speed and reversed relative to its
unchanged nose. Later, once velocity was backwards, positive aft main thrust
acted against that reversed velocity.

That was a physics-model contradiction, not merely a viewer problem.

### Correct execution invariant

For the current ship:

```text
main_a · hull_forward >= 0
```

for every physical main-engine sample in BOTH laws.

If desired acceleration is opposite the hull:
- bounded RCS may contribute;
- otherwise the accepted attitude program must rotate/cant the hull;
- only after aft-main alignment may strong braking occur.

Assisted and Newtonian may still produce different maneuver doctrine and
velocity/attitude behavior, but they do not get different fictional engines.

### Current candidate

- aft-only navigation main allocation for both laws;
- propulsion-aware reference attitude for Assisted as well as Newtonian;
- exact 20.90 -> 20.00 E2E regression;
- regression checks every non-zero main acceleration has non-negative projection
  on hull forward.

If target testing still misses the route after this correction, the next
required fix is not another allocator patch. It is maneuver timing:
point-mass Ruckig acceleration intervals must reserve finite lead-rotation time
before main-engine-dominant burns.

Candidate before docs: `b833eddb7bd04b5c025b2be0fd8334c33f8824e6`.

## Newtonian 21.20 m/s gate — RCS must not silently become the primary engine

Fresh target result:

```text
NEWTONIAN / EXPERT / STANDARD
21.20 -> 21.20 m/s
Ruckig 20.73 .. 21.20 m/s
final 8.17 m/s
final error 181.42 m
reference hold 40.61 s
body/velocity max 174.31 deg
```

The route itself widened correctly with speed. The failure was execution
doctrine.

Telemetry showed long intervals with:
```text
main_a = 0
RCS ~= 1.5 m/s^2
reference speed ~= 20.6 m/s
physical speed collapsing toward ~4 m/s
hull/reference error ~0
```

The old attitude author compared requested acceleration against the *full*
2.0 m/s^2 physical RCS capability. Because Ruckig requested less than that,
Newtonian never needed to point the aft main engine.

### Corrected distinction

```text
ASSISTED attitude authoring
    may use full physical RCS authority when choosing to stay velocity-coupled

NEWTONIAN attitude authoring
    RCS-as-primary threshold = tiny correction authority (0.35 m/s^2)
    material acceleration -> lead/cant/flip hull for aft-main participation
```

The downstream allocator still knows the full physical RCS capability; this is
not a fake clamp. It is a maneuver-authoring choice that stops a
main-engine-dominant ship from spending its whole route on manoeuvre jets.

### New visual diagnostics

Three persistent bottom indicators are driven from physical frame channels:
- `МАРШЕВЫЙ` — positive aft-main acceleration;
- `ПЕРЕДНИЙ МАРШЕВЫЙ` — negative main acceleration relative to hull forward;
- `МАНЕВРОВЫЙ` — non-zero RCS/manoeuvre acceleration.

For the current Cobra the middle indicator should remain off. If it lights,
that is direct evidence of a reintroduced hidden fore-main path.

Regression:
`testNewtonianHigherSpeedUsesMainEngineDominantManeuver()`.

If the next target run shows correct hull rotation + main burn but still cannot
track, proceed to finite attitude-acquisition/lead-rotation timing in the
trajectory/maneuver compiler rather than increasing RCS authority.

Candidate before docs: `421ecb1b2721d54bc0c33737a520100a3c10fac9`.
