# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Navigation:** `NAV-V2-SPACE-1` — static route semantics accepted; turn-aware A* optimization active

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

Target-machine architecture + behavior gate is green. Turn-aware routing correctly preserves arrival direction and `zigzag_vs_smooth` passes.

```text
turnPenalty == 0
    -> accepted v1 RegionSlot Dijkstra

turnPenalty > 0
    -> semantic state = (RegionSlot, incoming PortalId)
```

## Turn-aware performance history

Tree-backed expanded state was rejected:

```text
open_10k turn p95  216.1602 ms
hub_10k  turn p95  232.6520 ms
```

Dense `TurnStateSlot` + vector state + binary heap improved the same 10k search to:

```text
open_10k zero p95      8.6427 ms
open_10k turn p95     67.9647 ms
hub_10k  zero p95      8.5862 ms
hub_10k  turn p95     72.6054 ms
```

The improvement is about 3x, but turn-aware work still examines `329,660` transitions at 10k versus `57,197` for zero-turn. This puts the candidate in the pinned `40-120 ms` band: semantics stay accepted, search reduction is still required.

The benchmark-contract failure reported during that run was a stale exact-text documentation assertion; architecture/behavior and the benchmark executable itself passed. The contract now checks the benchmark path + turn-aware task state instead.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

## Active optimization candidate — admissible A*

Dense turn-state storage remains. Positive-turn search now orders the binary heap by:

```text
f = g + h
h = distanceWeight * EuclideanDistance(currentRegionCenter, endRegionCenter)
```

The heuristic is admissible/consistent because geometric edge cost is at least straight-line center distance and clearance/turn penalties are non-negative. `distanceWeight == 0` reduces `h` to zero.

The zero-turn v1 path remains unchanged. The next target-machine rerun must preserve behavior while materially reducing turn-aware `portalsExamined` and timing.

Status: **A* candidate on canonical `main`, pending architecture/behavior + identical turn benchmark rerun**.
