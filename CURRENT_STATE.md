# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Navigation:** `NAV-V2-SPACE-1` — static route semantics accepted; turn-aware search optimization active

## Accepted Navigation v2 ownership

Navigation v2 uses one shared ship-centered `NavigationWorld`.

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local search

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

`RuckigTrajectorySolver` remains downstream local kinematics, not path search. Moving-target pursuit remains specified in `src/world/navigation/PURSUIT_HORIZON.md` and is not yet the active runtime task.

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k evidence includes CPU candidate queries below 0.2 ms p95 and GPU dynamic total below 1.7 ms p95 on the target machine.

## `NAV-V2-SPACE-1` indexing — ACCEPTED

```text
baseline 10k full-portal corridor ≈1.95-1.99 s
per-region adjacency              ≈21.8-22.4 ms
dense RegionSlot BFS              ≈7.9-8.0 ms
BVH point p95                     <=0.0353 ms
BVH local invalidation p95        <=0.0115 ms
```

Full replace/local patch remain worker/update-side at roughly 44-49 ms median at 10k.

## Costed static corridor v1 — ACCEPTED

Behavior accepts traversable apertures and policy-dependent canyon/overflight routing.

10k p95:

```text
open distance_only      8.1733 ms
open clearance_aware    8.5020 ms
hub  distance_only      7.7609 ms
hub  clearance_aware    9.6103 ms
```

The predeclared `<=15 ms` gate passed, so zero-turn RegionSlot Dijkstra remains unchanged.

## Static turn cost v2 — ACCEPTED semantics

Target-machine architecture + behavior gate is green. Turn-aware routing correctly uses arrival direction and `zigzag_vs_smooth` passes.

```text
turnPenalty == 0
    -> accepted v1 RegionSlot Dijkstra

turnPenalty > 0
    -> semantic state = (RegionSlot, incoming PortalId)
```

## Turn-aware performance baseline — REJECTED

Fresh target-machine benchmark:

```text
open_10k zero p95       9.6982 ms
open_10k turn p95     216.1602 ms
hub_10k  zero p95       9.8862 ms
hub_10k  turn p95     232.6520 ms

10k portal examinations
zero-turn              57,197
turn-aware            329,660
```

The pinned rule was `>120 ms p95 -> optimize before the next layer`, so the tree-backed expanded-state implementation is rejected for performance. Semantics remain accepted.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

## Active optimization candidate

The same semantic `(RegionSlot, incoming PortalId)` state is now represented privately by stable dense `TurnStateSlot` values built with the graph:

```text
directed portal arrival -> TurnStateSlot
AdjacencyEdge            -> arrivalTurnStateSlot
```

Per-query turn-aware storage is now vector-backed:

```text
vector<double> bestCost
vector<TurnStateSlot> previous
vector<uint8_t> settled
priority_queue frontier
```

The accepted zero-turn v1 path is untouched. Turn-aware graph expansion is intentionally unchanged so the next benchmark isolates container/queue overhead from unavoidable expanded-state work.

Status: **candidate on `main`, pending target-machine architecture/behavior + turn benchmark rerun**. A* is not added unless the rerun shows that the remaining expansion itself is still too expensive.
