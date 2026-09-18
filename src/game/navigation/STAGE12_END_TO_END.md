# Navigation v2 — Stage 12 end-to-end runtime/stress/debug

**Status:** ACTIVE  
**Started:** 2026-09-18 Europe/Kyiv  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md`

## Goal

Stage 12 proves that the accepted Navigation v2 components work as one live system under real game ownership, real obstacle geometry, real vehicle authority, real replication, and real presentation.

It is not a new planner stage. The purpose is to compose and stress the already accepted planner/trajectory/control chain and retire legacy route-wide navigation only after equivalent or better live behavior is demonstrated.

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
