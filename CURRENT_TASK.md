# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — published hot-geometry turn optimization pending target-machine rerun

## Accepted foundation

Static indexing, costed corridor v1 and static turn-cost semantics are accepted.

```text
10k coarse BFS corridor          ≈7.3-8.2 ms
10k BVH point p95                <=0.0353 ms
10k local invalidation p95       <=0.0115 ms
costed v1 10k p95                <=9.6103 ms
```

Turn semantic state remains:

```text
turnPenalty == 0
    -> accepted RegionSlot Dijkstra fast path

turnPenalty > 0
    -> (RegionSlot, incoming PortalId)
```

`zigzag_vs_smooth`, aperture and canyon/overflight behavior have passed on target MinGW64.

## Performance history

### Tree-backed expanded state — REJECTED

```text
open_10k turn p95  216.1602 ms
hub_10k  turn p95  232.6520 ms
turn portals examined 329,660
```

### Dense TurnStateSlot + binary heap — IMPROVED, ABOVE GATE

```text
open_10k turn p95   67.9647 ms
hub_10k  turn p95   72.6054 ms
turn portals examined 329,660
```

Dense state removed most ordered-state bookkeeping cost but did not reduce expanded search work.

### Euclidean A* — REJECTED

Target-machine rerun on `5376e7179cfe791df843eb846c2d4c8af41432cd`:

```text
open_10k zero p95   11.2399 ms
open_10k turn p95   97.0537 ms
hub_10k  zero p95    9.3477 ms
hub_10k  turn p95   97.6909 ms

turn portals examined 323,888
```

Architecture, behavior and benchmark contracts all passed. Route lengths, turn totals and costs stayed correct.

The heuristic reduced 10k portal work by only about 1.75% (`329,660 -> 323,888`) while increasing turn-aware p95 by about 42.8% open and 34.6% hub versus the dense-Dijkstra candidate. The Euclidean heuristic therefore does not justify its query-time overhead for this topology and is rejected.

Do not restore this A* candidate unless a materially stronger lower bound is introduced and benchmarked.

## Active candidate — published static hot geometry

Positive-turn search is back to exact dense-state Dijkstra ordering by `g`.

The semantic state is unchanged, but static values are now published with the graph instead of recomputed in every expanded transition:

```text
RegionSlot
    -> center
    -> capacity
    -> dense invalidation flag

PortalSlot
    -> center
    -> dense invalidation flag

AdjacencyEdge
    -> portalSlot
    -> neighborSlot
    -> arrivalTurnStateSlot
    -> geometricMeters
    -> availableClearanceMeters

TurnStateSlot
    -> precomputed turn-angle slice
```

`invalidateBounds()` synchronizes the authoritative map state with the dense invalidation flags.

The positive-turn query hot loop no longer performs ordered-map portal/region lookups and no longer recomputes `boundsCenter`, geometric `sqrt`, or turn `acos`. Policy-dependent distance, clearance and turn multipliers remain query-time values.

This is a private representation optimization. Public API and the accepted zero-turn v1 path are unchanged.

Design note: `src/world/navigation/space/TURN_HOT_PATH_CANDIDATE.md`.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh

python tests/architecture_contracts/check_navigation_space_turn_benchmark.py
bash benchmarks/navigation_space_turn/run_mingw64.sh
```

Send the complete output.

## Decision after hot-path rerun

At 10k turn-aware p95:

- `<=40 ms`: accept static turn-aware worker/reference search and stop extending persistent static policy;
- `40-120 ms`: per-edge cost is still insufficient; move to exact search-work reduction such as bidirectional/hierarchical turn-state search;
- `>120 ms`: regression or implementation defect; repair before proceeding.

For this candidate `turn_portals_examined` may remain close to the dense-Dijkstra `329,660`; the intended gain is lower cost per examined transition. Route cost/behavior must remain identical.

After acceptance, next layer is combined static corridor + dynamic conflict/local horizon, followed by pursuit/receding-intercept consumption. Do not add ship velocity, braking or dynamic traffic to persistent `NavigationSpace` static cost.
