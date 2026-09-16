# NavigationSpace — static NavigationWorld topology boundary

`NavigationSpace` is the **only production-facing API** for persistent static navigation topology in the active ship-centered NavigationWorld.

Current stage: `NAV-V2-SPACE-1`.

## Ownership

The block owns static free-space topology and its CPU reference implementation. Callers publish free-space regions and portals by value and receive compact point/corridor/invalidation products by value.

The public API deliberately does **not** expose:

- GLM;
- OpenGL / GLFW;
- renderer state;
- `SpaceState` / client state;
- internal maps/graphs/queues;
- private BVH / dense-slot acceleration structures;
- future sparse-brick/navmesh acceleration structures.

The first CPU reference uses axis-aligned free-space regions plus explicit portals. That is a reference representation, not a permanent storage mandate. A later sparse-brick, convex-cell or hybrid implementation may replace internals without changing planner call sites.

## Free-space rather than giant dense voxels

The static world is represented as traversable regions connected by portals. Large open volumes may use coarse regions; detailed interiors may use smaller regions. A narrow docking corridor, tunnel, breach, canyon entrance or wall opening is represented by explicit traversable free space and portals whose clearance limits which agent envelopes may pass.

Do not build one dense system-scale velocity/occupancy field. Dynamic P/V/A belongs to `NavigationMap`; this block owns static free-space topology only.

## Agent envelope / clearance

Every query supplies an `AgentEnvelope`:

```text
required clearance = radiusMeters + additionalClearanceMeters
```

A point is traversable only when it lies in a valid free-space region with enough distance to the region boundary and enough authored region clearance.

A portal is traversable only when its clearance admits the same envelope.

This gives the required behavior where the same geometry can admit a drone and reject a larger ship.

## Regions and portals

The CPU reference currently accepts:

```text
RegionInput
    id
    AABB in ship-centered map coordinates
    clearance radius cap
    flags
    geometry revision

PortalInput
    id
    region A / B
    center
    clearance radius
    bidirectional flag
    flags
    geometry revision
```

`queryCorridor()` returns only ordered region/portal IDs and diagnostics. It does not return internal graph nodes or materialize a dense trajectory.

## Fast topology corridor vs costed corridor

Two static-space corridor products intentionally coexist.

### `queryCorridor()`

This is the deterministic **topology/BFS oracle**. It answers whether a traversable region/portal chain exists with the requested agent envelope and returns a stable coarse path cheaply. It remains useful for broad validity, mass/background use and reference diagnostics.

### `queryCostedCorridor()`

This is the policy-aware static route selector.

Accepted v1 cost:

```text
edge cost = distanceWeight * coarse_geometric_distance
          + static_clearance_penalty
```

Accepted v1 policy fields:

```text
distanceWeight
preferredClearanceMultiple
clearancePenaltyMeters
```

Traversal still fails closed when the agent physically cannot fit. The penalty only chooses between routes that are already traversable.

The geometric term approximates travel through a coarse region graph using region centers and portal centers. `totalCostMetersEquivalent` is therefore a coarse branch-comparison metric, not an exact physical trajectory-length claim.

Accepted behavioral fixtures:

```text
wall_with_aperture
    small agent -> route through opening
    oversized agent -> opening rejected

canyon_vs_overflight
    distance-only policy -> shorter canyon
    clearance-aware policy -> longer open route
    oversized canyon agent -> open route regardless of preference
```

This is the contract required for tunnels, holes, station apertures, canyons and similar navigable geometry: an opening is not semantically merged into the surrounding obstacle when it has valid free-space topology and enough clearance.

### Accepted costed-scaling result

Dedicated target-machine benchmark at 10k regions / 28,600 portals:

```text
open distance_only p95      8.1733 ms
open clearance_aware p95    8.5020 ms
hub  distance_only p95      7.7609 ms
hub  clearance_aware p95    9.6103 ms
```

The acceptance threshold was `<=15 ms p95`, so deterministic Dijkstra v1 is retained as an asynchronous worker/reference solve. No A* or priority-queue redesign is required before the next static route term.

Raw evidence: `benchmarks/navigation_space_costed/RUN_LOG.md`.

## Static turn cost v2 candidate

Design authority:

```text
src/world/navigation/STATIC_TURN_COST.md
```

`CorridorCostPolicy` adds:

```text
turnPenaltyMetersPerRadian
```

When it is zero, `queryCostedCorridor()` keeps the accepted v1 region-state Dijkstra path. The expanded state-space must not tax distance/clearance-only background queries.

When turn penalty is positive, turn-aware search uses:

```text
state = (RegionSlot, incoming PortalId)
```

This is mandatory because the future cost of leaving a region depends on the direction from which the route entered it. Two arrivals at the same region through different portals may have different optimal futures and cannot be collapsed into one region-only state.

Static v2 edge cost is:

```text
edge cost = distanceWeight * coarse_geometric_distance
          + static_clearance_penalty
          + turnPenaltyMetersPerRadian * turn_angle_radians
```

Turn angle is measured at the current coarse region center between:

```text
incoming portal center -> current region center
current region center  -> outgoing portal center
```

The special start state has no turn penalty because `CorridorQuery` does not carry current ship heading/velocity. Final maneuver feasibility still belongs to the local/precision planner and Ruckig.

Pinned candidate fixture:

```text
zigzag_vs_smooth
    turnPenalty=0 -> slightly shorter zig-zag
    positive turnPenalty -> smoother branch once saved turn burden exceeds distance delta
```

Speed-dependent turn radius, angular acceleration, braking distance, traffic/risk and pursuit prediction are intentionally not persistent `NavigationSpace` static terms.

## Private connectivity / dense graph index

The first scaling benchmark showed that scanning the complete portal map for every BFS region is pathological: the 10k reference examined about 286 million portal records and took roughly two seconds per corridor query.

Optimization 1 introduced per-region adjacency and reduced the target-machine 10k corridor to about 22 ms while portal examinations fell to about 57k.

Optimization 2 retained public `RegionId` / `PortalId` identity but replaced ordered-map BFS bookkeeping with a private dense graph index:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS visited/previous/frontier state is vector-backed by `RegionSlot`. Public query results are converted back to stable RegionId/PortalId values.

The dense-slot target-machine rerun reduced the 10k corridor again from about 22 ms to about 8 ms. At that point further unweighted-BFS micro-optimization stopped being the active priority because full/global corridor search is worker-side by contract.

## Private spatial index

Optimization 3 adds a private deterministic AABB BVH over the same dense `RegionSlot` identity.

```text
RegionSlot[]
    |
private AABB BVH
    |
    +-> point candidate regions
    +-> invalidation-bounds candidate regions
```

The BVH is rebuilt together with full publication/local patching and remains private to `NavigationSpace::Impl`.

`queryPoint()` and corridor endpoint localization use BVH candidate reduction. `invalidateBounds()` queries the BVH and invalidates only portals incident to changed regions through a private endpoint-incidence list.

Accepted 10k target-machine result:

```text
open point p95      0.0353 ms
hub  point p95      0.0099 ms
open invalidate p95 0.0112 ms
hub  invalidate p95 0.0115 ms
```

Full replacement/local patch increased to roughly 44–49 ms median because graph + BVH are rebuilt transactionally; those operations remain worker/update-path work.

## Local invalidation

`invalidateBounds()` is fail-closed:

1. free-space regions intersecting changed geometry become invalid;
2. portals touching them become invalid;
3. queries stop traversing them immediately;
4. `applyLocalPatch()` can transactionally restore only the affected topology.

## Determinism

Authoritative regions/portals are stored in ordered maps; RegionSlots are assigned in stable RegionId order; adjacency is built in stable PortalId order; BVH query candidates are sorted before semantic evaluation.

Costed v1 uses deterministic queue ordering by `(cost, RegionSlot)`. Turn-aware v2 candidate orders expanded states by cost, RegionSlot and incoming PortalId, with outgoing adjacency already stable by PortalId.

## Relation to dynamic NavigationMap

```text
NavigationSpace (CPU)
    static free-space / clearance / regions / portals
             |
             +---- coarse/costed corridor ----+
                                              |
NavigationMap (hybrid dynamic)                |
    P/V/A prediction / bins / conflicts       |
             |                                |
             +---- local candidates ----------+
                                              v
                                     local/precision planner
                                              |
                                     temporary target state
                                              |
                                     RuckigTrajectorySolver
```

`NavigationSpace` and `NavigationMap` share the same conceptual ship-centered working domain but remain separate ownership blocks.

## Hub / local-domain rule

Hub, station, carrier and interior geometry remains owned by its local domain. The relevant static free-space subset is transformed/published into the active NavigationWorld boundary. This block must not force the whole station/interior to rebuild when the active ship moves.

## Current limitations of the CPU reference

- free-space regions are AABBs, not arbitrary convex cells;
- `totalCostMetersEquivalent` is a coarse region/portal metric, not an exact flight-path length;
- turn-aware v2 candidate is static/coarse only and does not model vehicle dynamics;
- local patching still copies full ordered region/portal maps and rebuilds graph + BVH transactionally;
- full publication/local patch are worker/update-path operations, not frame-path operations;
- no live `EliteGame` / `EliteServer` integration yet.

These are deliberate `NAV-V2-SPACE-1` reference limitations. The public API keeps storage/search replacement possible.
