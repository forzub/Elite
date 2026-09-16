# Static route turn-cost contract

**Stage:** `NAV-V2-SPACE-1`  
**Status:** design contract for costed corridor v2

## Purpose

`queryCostedCorridor()` v1 already chooses among traversable static branches using coarse geometric distance and clearance preference. The next static route-quality term is turn-angle cost.

This term exists to distinguish routes that have similar distance/clearance but very different maneuver burden, for example:

```text
short zig-zag canyon        slightly longer smooth corridor
/\/\/\/\/\                 __________________
```

It is a **coarse static route term**, not a replacement for flight dynamics. Ruckig/local guidance still owns feasible velocity/acceleration/jerk execution.

## Critical search-state invariant

Turn cost is path-history dependent.

The future cost of leaving a region depends on the direction from which the route entered that region. Therefore a turn-aware Dijkstra/A* search MUST NOT collapse all arrivals into one `RegionSlot` state.

Incorrect state:

```text
state = RegionSlot
```

Required minimum state:

```text
state = (RegionSlot, incoming PortalId)
```

with a special start state that has no incoming portal.

Two arrivals at the same region through different portals may have different optimal future costs and must coexist until settled by the search.

## Policy v2

`CorridorCostPolicy` gains:

```text
turnPenaltyMetersPerRadian
```

Default is `0.0`, preserving v1 behavior and performance.

Static v2 edge cost:

```text
edge cost = distanceWeight * coarse_geometric_distance
          + static_clearance_penalty
          + turnPenaltyMetersPerRadian * turn_angle_radians
```

The turn angle is measured at the current coarse region center between:

```text
incoming portal center -> current region center
current region center  -> outgoing portal center
```

The special start state has no turn penalty because `CorridorQuery` does not yet carry an initial heading. Final local alignment is also left to the local/precision planner.

## Fast path

When:

```text
turnPenaltyMetersPerRadian == 0
```

`queryCostedCorridor()` keeps the accepted v1 region-state Dijkstra path. Do not pay expanded-state allocation/search cost for background or distance/clearance-only queries.

When turn penalty is positive, use the expanded `(RegionSlot, incoming PortalId)` state-space.

## Determinism

For identical topology, query and policy, equal-cost choices remain reproducible.

Required deterministic ordering:

1. total cost;
2. `RegionSlot`;
3. incoming `PortalId`;
4. existing stable outgoing `PortalId` adjacency order.

## Acceptance case

Add a deterministic `zigzag_vs_smooth` fixture:

```text
A -> short zig-zag branch -> B
A -> slightly longer smooth branch -> B
```

Required behavior:

```text
turnPenaltyMetersPerRadian = 0
    -> shorter zig-zag wins

turnPenaltyMetersPerRadian > 0
    -> smoother branch wins once saved turn cost exceeds distance delta
```

Distance/clearance-only aperture and canyon fixtures remain unchanged.

## Scope boundary

This static term deliberately does NOT model:

- current ship velocity vector;
- maximum angular acceleration;
- speed-dependent turn radius;
- braking distance;
- dynamic actors/traffic;
- pursuit intercept prediction.

Those belong to local/precision planning and the combined dynamic NavigationWorld horizon. Static turn cost only makes the coarse branch choice less geometrically naive.
