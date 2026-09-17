# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** `NAV-V2-SPACE-1` — static route semantics accepted; published hot-geometry turn optimization active

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

Full replace/local patch remain worker/update-side at roughly 44-49 ms median at 10k before the current extra turn-angle publication work is remeasured.

## Costed static corridor v1 — ACCEPTED

Behavior accepts traversable apertures and policy-dependent canyon/overflight routing.

10k p95:

```text
open distance_only      8.1733 ms
open clearance_aware    8.5020 ms
hub  distance_only      7.7609 ms
hub  clearance_aware    9.6103 ms
```

The predeclared `<=15 ms` gate passed, so zero-turn RegionSlot Dijkstra remains accepted.

## Static turn cost v2 — ACCEPTED semantics

Target-machine architecture + behavior gates are green. Turn-aware routing preserves arrival direction and `zigzag_vs_smooth` passes.

```text
turnPenalty == 0
    -> accepted v1 RegionSlot Dijkstra

turnPenalty > 0
    -> semantic state = (RegionSlot, incoming PortalId)
```

## Turn-aware performance progression

Tree-backed expanded state was rejected:

```text
open_10k turn p95  216.1602 ms
hub_10k  turn p95  232.6520 ms
```

Dense `TurnStateSlot` + vector state + binary heap improved the same 10k search to:

```text
open_10k turn p95   67.9647 ms
hub_10k  turn p95   72.6054 ms
turn portals examined 329,660
```

### Euclidean A* — REJECTED

Target rerun on commit `5376e7179cfe791df843eb846c2d4c8af41432cd`:

```text
open_10k turn p95   97.0537 ms
hub_10k  turn p95   97.6909 ms
turn portals examined 323,888
```

Architecture, behavior and benchmark contracts passed and route costs remained correct. However the heuristic reduced portal work by only about 1.75% while increasing turn-aware p95 by about 42.8% open / 34.6% hub versus dense Dijkstra. The A* candidate is therefore rejected.

## Active optimization — published static hot geometry

Positive-turn search is restored to dense-state Dijkstra ordering. The graph publication step now precomputes static data needed by the expanded-state loop:

- dense region centers/capacities/invalidation flags;
- dense portal slots/centers/invalidation flags;
- per-directed-edge geometric distance and available clearance;
- flattened precomputed turn angles for each `(TurnStateSlot, outgoing edge)` transition.

`invalidateBounds()` synchronizes dense invalidation flags with authoritative region/portal state.

The positive-turn query therefore avoids ordered-map region/portal lookup and avoids repeated `sqrt`/`acos` geometry in the hot loop. Public API, turn semantics and zero-turn v1 remain unchanged.

Design note: `src/world/navigation/space/TURN_HOT_PATH_CANDIDATE.md`.

Status: **candidate on canonical `main`, pending architecture/behavior + identical target-machine turn benchmark rerun**.
