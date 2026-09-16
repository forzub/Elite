# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / static turn-aware search optimization  
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

Full replace/local patch remain worker/update-path operations at roughly 44-49 ms median at 10k.

## Costed route v1 — ACCEPTED

Behavior accepts traversable apertures and policy-dependent canyon/overflight routing. Dedicated 10k p95 remains <=9.6103 ms, below the pinned 15 ms acceptance threshold.

## Static turn cost v2 — ACCEPTED semantics

Target-machine architecture + behavior tests pass. Positive turn penalty correctly preserves arrival direction in semantic state `(RegionSlot, incoming PortalId)` and `zigzag_vs_smooth` selects the smoother branch when turn burden dominates.

## Turn-aware performance baseline — REJECTED

Fresh dedicated benchmark:

```text
open_10k zero-turn p95    9.6982 ms
open_10k turn-aware p95 216.1602 ms
hub_10k  zero-turn p95    9.8862 ms
hub_10k  turn-aware p95 232.6520 ms

zero-turn portals examined  57,197
turn-aware portals examined 329,660
```

The predeclared rule was `>120 ms p95 -> optimize before the next layer`, so the original tree-backed expanded-state reference is rejected for performance. Its semantics remain accepted.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

## Active optimization

Turn-aware representation now uses stable dense `TurnStateSlot` values created at graph publication time. Each directed adjacency edge points directly to the arrival-state slot. Per-query best-cost, previous and settled state are vectors, and the frontier is a binary priority queue.

```text
directed arrival -> TurnStateSlot
AdjacencyEdge    -> arrivalTurnStateSlot

query state
    vector bestCost
    vector previous
    vector settled
    priority_queue frontier
```

The zero-turn RegionSlot Dijkstra path remains unchanged. The expanded topology itself is unchanged, so the next benchmark will isolate tree-container overhead from the remaining ~330k turn-state transition checks.

## Next order

1. rerun architecture/behavior after dense turn-state patch;
2. rerun `navigation_space_turn` benchmark;
3. if 10k turn-aware p95 <=40 ms, accept static turn search and move to dynamic conflict/local-horizon composition;
4. if still 40-120 ms, add admissible A* / search reduction;
5. then implement pursuit/receding-intercept consumer;
6. live game/server integration follows isolated NavigationWorld stabilization.

Do not add more persistent static cost terms before the turn-aware performance gate closes.
