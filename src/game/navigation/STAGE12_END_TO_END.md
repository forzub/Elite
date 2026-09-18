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
