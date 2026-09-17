# NavigationSpace — static NavigationWorld topology boundary

`NavigationSpace` is the **only production-facing API** for persistent static navigation topology in the active ship-centered NavigationWorld.

Stage `NAV-V2-SPACE-1`: **CLOSED / ACCEPTED**.

## Ownership

The block owns static free-space topology and its deterministic CPU reference. Callers publish free-space regions and portals by value and receive compact point/corridor/invalidation products by value.

The public API deliberately does **not** expose GLM, OpenGL/GLFW, renderer/game state, internal maps/graphs/queues, BVH state or dense-slot acceleration structures.

The first CPU reference uses axis-aligned free-space regions plus explicit portals. That is a reference representation, not a permanent storage mandate; later sparse-brick, convex-cell or hybrid internals may replace it without changing planner call sites.

## Free-space and clearance

The static world is represented as traversable regions connected by portals. Large open volumes may use coarse regions; interiors may use smaller regions. Tunnels, breaches, canyon entrances, hangars and wall apertures are explicit free-space topology rather than one giant obstacle box.

Every query supplies an `AgentEnvelope`:

```text
required clearance = radiusMeters + additionalClearanceMeters
```

The same geometry may therefore admit a drone and reject a larger ship.

Accepted behavioral fixtures include:

```text
wall_with_aperture
    small agent -> route through opening
    oversized agent -> opening rejected

canyon_vs_overflight
    distance-only -> shorter canyon
    clearance-aware -> longer open branch
    oversized canyon agent -> canyon excluded

zigzag_vs_smooth
    zero turn penalty -> slightly shorter zig-zag
    positive turn penalty -> smoother branch when turn burden dominates
```

## Fast topology corridor vs costed corridor

`queryCorridor()` is the deterministic topology/BFS oracle. `queryCostedCorridor()` is the policy-aware static route selector.

Accepted v1 cost:

```text
edge cost = distanceWeight * coarse_geometric_distance
          + static_clearance_penalty
```

Accepted 10k target-machine p95:

```text
open distance_only      8.1733 ms
open clearance_aware    8.5020 ms
hub  distance_only      7.7609 ms
hub  clearance_aware    9.6103 ms
```

The pinned v1 threshold was `<=15 ms p95`, so zero-turn RegionSlot Dijkstra is accepted.

## Static turn cost v2 — ACCEPTED

When `turnPenaltyMetersPerRadian == 0`, the accepted v1 region-state path is used.

When turn penalty is positive, search state is:

```text
(RegionSlot, incoming PortalId)
```

Arrival direction is part of state because future turn cost depends on how the route entered the region. The special start state has no turn penalty because `CorridorQuery` does not carry current ship heading/velocity; that belongs to local/precision planning.

Static v2 cost is:

```text
edge cost = distanceWeight * coarse_geometric_distance
          + static_clearance_penalty
          + turnPenaltyMetersPerRadian * turn_angle_radians
```

Speed-dependent turn radius, angular acceleration, braking, traffic/risk and pursuit prediction are intentionally **not** persistent `NavigationSpace` terms.

### Performance progression

```text
tree-backed expanded state — rejected
    open_10k 216.1602 ms
    hub_10k  232.6520 ms

Dense TurnStateSlot + binary heap
    open_10k  67.9647 ms
    hub_10k   72.6054 ms

Euclidean A* — rejected
    open_10k  97.0537 ms
    hub_10k   97.6909 ms
```

The accepted implementation restores exact dense Dijkstra ordering and publishes immutable hot data with the graph:

```text
RegionSlot -> center / capacity / invalidation
PortalSlot -> center / invalidation
AdjacencyEdge -> geometricMeters / availableClearanceMeters
TurnStateSlot -> precomputed outgoing turn-angle slice
```

`invalidateBounds()` synchronizes dense invalidation mirrors with authoritative region/portal state. The positive-turn expanded loop therefore avoids ordered-map region/portal lookup and repeated center geometry, `sqrt` and `acos`.

Final target-machine acceptance on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_10k       8.4498       12.0072         57,197       329,660
hub_10k        8.4125       11.9065         57,197       329,660
```

The pinned turn-aware gate was `<=40 ms p95`; both scenarios pass with large margin. Routes, accumulated coarse turn and costs remained correct. The unchanged transition count shows that per-transition work, not accepted expanded-state semantics, was the practical bottleneck in this reference workload.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md` and `TURN_HOT_PATH_CANDIDATE.md`.

## Private connectivity and spatial indices

Historical scaling:

```text
10k full portal scan      ≈1.95-1.99 s / ~286M portal records
per-region adjacency      ≈21.8-22.4 ms / ~57k portal records
dense RegionSlot BFS      ≈7.9-8.0 ms
```

A private deterministic AABB BVH handles point lookup and bounded invalidation. Accepted 10k target-machine p95:

```text
open point      0.0353 ms
hub  point      0.0099 ms
open invalidate 0.0112 ms
hub  invalidate 0.0115 ms
```

Full publication/local patch is worker/update-path work and may rebuild graph/BVH/precomputed turn data transactionally; it is not a frame-path operation.

## Local invalidation

`invalidateBounds()` is fail-closed:

1. intersecting free-space regions become invalid;
2. incident portals become invalid;
3. queries stop traversing them immediately;
4. `applyLocalPatch()` can transactionally restore affected topology.

Dense invalidation flags are private acceleration mirrors only.

## Relation to dynamic NavigationMap

```text
NavigationSpace
    static free-space / corridor intent
              |
              +--------------------+
                                   |
NavigationMap                      |
    P/V/A prediction / bins        |
    compact dynamic candidates     |
              |                    |
              +---------+----------+
                        v
               local horizon consumer
                        |
               temporary target state
                        |
               Ruckig / flight control
```

`NavigationSpace` and `NavigationMap` share the conceptual ship-centered working domain but remain separate ownership blocks.

## Current deliberate limitations

- regions are AABBs, not arbitrary convex cells;
- `totalCostMetersEquivalent` is a coarse region/portal metric, not exact trajectory length;
- local patching still rebuilds private graph/BVH/precomputed data transactionally;
- no live `EliteGame` / `EliteServer` integration yet.

No further static-search optimization is active. The current project stage is `NAV-V2-LOCAL-1`: dynamic conflict + bounded receding-horizon target selection.
