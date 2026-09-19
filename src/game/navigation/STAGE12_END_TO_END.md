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
