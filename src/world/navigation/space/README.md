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
- future sparse-brick/BVH/navmesh acceleration structures.

The first CPU reference uses axis-aligned free-space regions plus explicit portals. That is a reference representation, not a permanent storage mandate. A later sparse-brick, convex-cell or hybrid implementation may replace internals without changing planner call sites.

## Free-space rather than giant dense voxels

The static world is represented as traversable regions connected by portals. Large open volumes may use coarse regions; detailed interiors may use smaller regions. A narrow docking corridor/breach is represented by a portal whose clearance limits which agent envelopes may pass.

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

## Private connectivity / dense graph index

The first scaling benchmark showed that scanning the complete portal map for every BFS region is pathological: the 10k reference examined about 286 million portal records and took roughly two seconds per corridor query.

Optimization 1 introduced per-region adjacency and reduced the target-machine 10k corridor to about 22 ms while portal examinations fell to about 57k. That measurement showed the remaining graph-search cost is no longer full portal scanning; it is primarily generic ordered-map bookkeeping during traversal.

The CPU reference therefore keeps public `RegionId` / `PortalId` identity but builds a private dense graph index:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS visited/previous/frontier state is vector-backed by `RegionSlot`, not `std::map<RegionId,...>`. Portal order remains deterministic because the graph is built from the ordered portal map. Public query results are converted back to stable RegionId/PortalId values.

The graph index is rebuilt transactionally together with full publication/local patching. It is internal acceleration only; the public API does not expose or depend on dense slots.

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

The first CPU reference still scans regions linearly for invalidation. This is behavior/reference code, not the final acceleration strategy. `NAV-V2-SPACE-1` benchmarking determines where sparse/hierarchical indexing is required next.

## Determinism

The CPU reference stores regions/portals in ordered maps and builds dense adjacency in stable PortalId order. For identical published input and query, the returned coarse corridor is deterministic.

## Relation to dynamic NavigationMap

```text
NavigationSpace (CPU)
    static free-space / clearance / regions / portals
             |
             +---- coarse corridor ----+
                                       |
NavigationMap (hybrid dynamic)         |
    P/V/A prediction / bins / conflicts
             |                         |
             +---- local candidates ---+
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
- point location and invalidation are still linear scans;
- corridor search is unweighted BFS rather than costed A*/Dijkstra;
- local patching still copies full ordered region/portal maps and rebuilds the private graph transactionally;
- no live `EliteGame` / `EliteServer` integration yet.

These are deliberate `NAV-V2-SPACE-1` reference limitations. The public API keeps storage/search replacement possible.
