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

This is the first policy-aware route selector. It evaluates alternate traversable branches using meter-equivalent cost:

```text
edge cost = distanceWeight * geometric_distance
          + clearance_penalty
```

The current `CorridorCostPolicy` exposes:

```text
distanceWeight
preferredClearanceMultiple
clearancePenaltyMeters
```

Traversal still fails closed when the agent physically cannot fit. The penalty only chooses between routes that are already traversable.

The geometric term approximates travel through a coarse region graph using region centers and portal centers. Clearance preference uses the narrowest available clearance across current region, portal and neighbor region. A policy may therefore choose a short narrow canyon for an aggressive/small craft and a longer open overflight for a cautious/larger craft.

The costed route is intentionally separate from the fast BFS oracle. Do not make every background NPC pay for policy-aware search when a cached/coarse branch is sufficient.

Current behavioral acceptance fixtures include:

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

Turn cost, traffic/risk cost and dynamic-conflict cost are intentionally not part of static cost v1. Dynamic risk belongs to the combined NavigationWorld/local planner rather than being baked into persistent static topology.

## Private connectivity / dense graph index

The first scaling benchmark showed that scanning the complete portal map for every BFS region is pathological: the 10k reference examined about 286 million portal records and took roughly two seconds per corridor query.

Optimization 1 introduced per-region adjacency and reduced the target-machine 10k corridor to about 22 ms while portal examinations fell to about 57k.

Optimization 2 retained public `RegionId` / `PortalId` identity but replaced ordered-map BFS bookkeeping with a private dense graph index:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS visited/previous/frontier state is vector-backed by `RegionSlot`, not `std::map<RegionId,...>`. Portal order remains deterministic because the graph is built from the ordered portal map. Public query results are converted back to stable RegionId/PortalId values.

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

The BVH is rebuilt together with full publication/local patching and remains completely private to `NavigationSpace::Impl`.

`queryPoint()` asks the BVH for candidate region slots and then applies the same exact region/clearance tests in stable RegionSlot/RegionId order. `findTraversableRegion()` used by corridor endpoint localization uses the same candidate reduction.

`invalidateBounds()`:

1. queries the BVH for intersecting candidate regions;
2. exact-tests only those region AABBs;
3. marks affected regions invalid;
4. invalidates only portals incident to those regions via private endpoint adjacency.

The graph therefore owns two related but distinct private views:

```text
traversal adjacency
    RegionSlot -> { PortalId, neighbor RegionSlot }

incident portals
    RegionSlot -> PortalId[] touching the region
```

The second list is required because even a one-way portal must fail closed when either endpoint region is invalidated.

### Accepted Optimization-3 target-machine result

At 10k regions / 28,600 portals:

```text
open_10k
    point median/p95      0.0065 / 0.0353 ms
    invalidate median/p95 0.0102 / 0.0112 ms
    point regions examined=5

hub_10k
    point median/p95      0.0064 / 0.0099 ms
    invalidate median/p95 0.0109 / 0.0115 ms
    point regions examined=5
```

Compared with the dense-slot pre-BVH run, ordinary point lookup and local invalidation are now comfortably sub-millisecond. Full replacement/local patch increased to roughly 44–49 ms median because graph + BVH are rebuilt transactionally; those operations remain worker/update-path work and are not accepted frame-path operations.

The public API does not expose the BVH, dense slots, or adjacency representation.

Raw benchmark history is recorded in:

```text
benchmarks/navigation_space/RUN_LOG.md
```

## Local invalidation

`invalidateBounds()` is fail-closed:

1. free-space regions intersecting the changed geometry bounds become invalid;
2. portals touching invalidated regions become invalid;
3. queries stop traversing them immediately;
4. `applyLocalPatch()` can transactionally replace/remove only the affected regions/portals and restore connectivity.

The spatial index accelerates candidate discovery; exact AABB intersection remains authoritative for the CPU reference.

## Determinism

The CPU reference stores authoritative regions/portals in ordered maps, assigns RegionSlots in stable RegionId order, builds adjacency in stable PortalId order, and sorts BVH query candidate slots before semantic evaluation.

For identical published input and query, coarse corridor and point-resolution semantics remain deterministic. Costed routing uses deterministic queue ordering by `(cost, RegionSlot)` and stable PortalId adjacency order; equal-cost branches therefore remain reproducible.

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

`NavigationSpace` and `NavigationMap` share the same conceptual ship-centered working domain but remain separate ownership blocks. Neither block reaches into the other's internals.

## Hub / local-domain rule

Hub, station, carrier and interior geometry remains owned by its local domain. The relevant static free-space subset is transformed/published into the active NavigationWorld boundary. This block must not force the whole station/interior to rebuild when the active ship moves.

## Current limitations of the CPU reference

- free-space regions are AABBs, not arbitrary convex cells;
- costed corridor v1 has geometric distance + static clearance preference only; turn/risk/traffic terms are later layers;
- local patching still copies full ordered region/portal maps and rebuilds graph + BVH transactionally;
- full publication/local patch are worker/update-path operations, not frame-path operations;
- no live `EliteGame` / `EliteServer` integration yet.

These are deliberate `NAV-V2-SPACE-1` reference limitations. The public API keeps storage/search replacement possible.
