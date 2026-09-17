# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / static turn-aware hot-path optimization  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-SPACE-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior domains publish only relevant subsets.

Legacy route-wide navigation remains migration code. `RuckigTrajectorySolver` is downstream local kinematics.

Accepted hybrid ownership:

```text
CPU: static free-space, portals, corridor search, precision local search
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, conflict reduction
```

Moving-goal pursuit is specified in `src/world/navigation/PURSUIT_HORIZON.md`; runtime pursuit remains later.

## `NAV-V2-MAP-2` — CLOSED

Accepted dynamic-map CPU/GPU evidence remains recorded in the navigation benchmark logs.

## `NAV-V2-SPACE-1` — accepted foundation

```text
baseline corridor              ≈1.95-1.99 s at 10k
adjacency                      ≈21.8-22.4 ms
dense RegionSlot BFS          ≈7.9-8.0 ms
BVH point p95                 <=0.0353 ms
BVH bounded invalidation p95  <=0.0115 ms
```

Full replace/local patch were roughly 44-49 ms median at 10k before the active turn-angle publication work; publication/update cost must be remeasured later because it is worker/update-side rather than frame-path work.

## Costed route v1 — ACCEPTED

Behavior accepts traversable apertures and policy-dependent canyon/overflight routing. Dedicated 10k p95 remains <=9.6103 ms, below the pinned 15 ms acceptance threshold.

## Static turn cost v2 — ACCEPTED semantics

Target-machine architecture + behavior tests pass. Positive turn penalty preserves arrival direction in semantic state `(RegionSlot, incoming PortalId)` and `zigzag_vs_smooth` selects the smoother branch when turn burden dominates.

## Turn-aware performance progression

### Tree-backed expanded state — REJECTED

```text
open_10k turn-aware p95 216.1602 ms
hub_10k  turn-aware p95 232.6520 ms
turn portals examined   329,660
```

### Dense arrival-state + binary heap — IMPROVED, STILL ABOVE GATE

```text
open_10k turn-aware p95  67.9647 ms
hub_10k  turn-aware p95  72.6054 ms
turn portals examined   329,660
```

Dense `TurnStateSlot` storage and vector-backed query state removed most ordered-state bookkeeping cost but left expanded work unchanged.

### Euclidean A* — REJECTED

Target-machine rerun on `5376e7179cfe791df843eb846c2d4c8af41432cd`:

```text
open_10k zero-turn p95   11.2399 ms
open_10k turn-aware p95  97.0537 ms
hub_10k  zero-turn p95    9.3477 ms
hub_10k  turn-aware p95  97.6909 ms
turn portals examined   323,888
```

Architecture, behavior and benchmark contracts passed. Route length, coarse turn total and route cost stayed correct. The heuristic reduced portal work only about 1.75% while increasing turn p95 about 42.8% open / 34.6% hub versus dense Dijkstra. Therefore the Euclidean A* candidate is rejected.

## Active optimization — published static hot geometry

Positive-turn search is restored to exact dense-state Dijkstra ordering by `g`.

Static values are now published with the graph:

```text
RegionSlot -> center / capacity / invalidation
PortalSlot -> center / invalidation
AdjacencyEdge -> geometricMeters / availableClearanceMeters
TurnStateSlot -> flattened precomputed outgoing turn angles
```

`invalidateBounds()` keeps dense invalidation flags synchronized with authoritative map states. The positive-turn expanded-state loop no longer performs ordered-map region/portal lookups and no longer recomputes center geometry, `sqrt`, or `acos` for each examined transition.

Public API, zero-turn v1 and accepted turn semantics are unchanged.

## Next order

1. rerun architecture/behavior with the published-hot-geometry candidate;
2. rerun the identical `benchmarks/navigation_space_turn/` harness;
3. if 10k turn-aware p95 <=40 ms, accept static turn-aware worker/reference search and stop adding persistent static policy terms;
4. if still 40-120 ms, move to exact search-work reduction such as bidirectional/hierarchical turn-state search rather than another weak Euclidean heuristic;
5. after static turn search closes, combine static corridor with dynamic conflict/local-horizon selection;
6. implement pursuit/receding-intercept consumer;
7. integrate NavigationWorld into live game/server, then retire legacy route-wide migration code only after v2 owns live navigation.

Do not add velocity, braking, dynamic traffic or pursuit prediction to persistent `NavigationSpace` static cost.
