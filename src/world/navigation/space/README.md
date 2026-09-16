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

## Local invalidation

`invalidateBounds()` is fail-closed:

1. free-space regions intersecting the changed geometry bounds become invalid;
2. portals touching invalidated regions become invalid;
3. queries stop traversing them immediately;
4. `applyLocalPatch()` can transactionally replace/remove only the affected regions/portals and restore connectivity.

The first CPU reference scans regions linearly for invalidation. This is behavior/reference code, not the final acceleration strategy. `NAV-V2-SPACE-1` benchmarking will determine where sparse/hierarchical indexing is required.

## Determinism

The CPU reference stores regions/portals in ordered maps and traverses portals in stable ID order. For identical published input and query, the returned coarse corridor is deterministic.

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
- point location and invalidation are linear scans;
- corridor search is unweighted BFS rather than costed A*/Dijkstra;
- no static-space benchmark is accepted yet;
- no live `EliteGame` / `EliteServer` integration yet.

These are deliberate `NAV-V2-SPACE-1` reference limitations. The public API keeps storage/search replacement possible.
