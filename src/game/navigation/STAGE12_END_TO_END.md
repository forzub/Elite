# Navigation v2 — Stage 12 end-to-end runtime/stress/debug

**Status:** ACTIVE  
**Started:** 2026-09-18 Europe/Kyiv  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md`

## Goal

Stage 12 proves that the accepted Navigation v2 components work as one live system under real game ownership, real obstacle geometry, real vehicle authority, real replication, and real presentation.

It is not a new planner stage. The purpose is to compose and stress the already accepted planner/trajectory/control chain and retire legacy route-wide navigation only after equivalent or better live behavior is demonstrated.

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
