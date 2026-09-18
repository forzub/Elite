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
