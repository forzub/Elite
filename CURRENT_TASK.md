# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — turn-aware A* optimization pending target-machine rerun

## Accepted foundation

Static indexing, costed corridor v1 and static turn-cost semantics are accepted.

Accepted reference evidence:

```text
10k coarse BFS corridor          ≈7.3-8.2 ms
10k BVH point p95                <=0.0353 ms
10k local invalidation p95       <=0.0115 ms
costed v1 10k p95                <=9.6103 ms
```

Turn-aware semantic state remains:

```text
turnPenalty == 0
    -> accepted RegionSlot Dijkstra fast path

turnPenalty > 0
    -> (RegionSlot, incoming PortalId)
```

`zigzag_vs_smooth`, aperture and canyon/overflight behavior pass on target MinGW64.

## Performance history

### Tree-backed expanded state — REJECTED

```text
open_10k turn p95  216.1602 ms
hub_10k  turn p95  232.6520 ms
turn portals examined 329,660
```

### Dense TurnStateSlot + binary heap — IMPROVED, STILL ABOVE GATE

Fresh rerun:

```text
open_10k zero p95      8.6427 ms
open_10k turn p95     67.9647 ms
hub_10k  zero p95      8.5862 ms
hub_10k  turn p95     72.6054 ms

10k portals examined
zero-turn              57,197
turn-aware            329,660
```

Dense state/heap cut turn-aware p95 by roughly 3x while leaving expansion work unchanged. This proves ordered-map bookkeeping was expensive, but the remaining expanded search is still above the pinned `<=40 ms` acceptance target.

The one benchmark-contract failure in this run was documentation-only:

```text
[FAIL] CURRENT_TASK does not declare the turn-aware performance gate
```

`NavigationSpace` architecture/behavior passed and the benchmark executable built/ran. The brittle exact-text assertion has been replaced with a stable benchmark-path + turn-aware task check.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

## Active candidate — admissible turn-aware A*

Dense turn state and vector-backed storage remain unchanged. Only positive-turn frontier ordering changes:

```text
f(n) = g(n) + h(n)

h(n) = distanceWeight * EuclideanDistance(
    currentRegionCenter,
    endRegionCenter
)
```

Why this is safe:

- geometric edge cost is center -> portal -> neighbor center and is therefore >= straight-line center distance;
- clearance penalty is non-negative;
- turn penalty is non-negative;
- `distanceWeight == 0` gives `h == 0`, reducing to dense-state Dijkstra.

Therefore the heuristic is admissible/consistent and should reduce settled expanded states without changing the optimal static route or accepted semantics.

The zero-turn v1 path is untouched.

## RUN NOW

Run architecture/behavior first, then the identical turn benchmark:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh

python tests/architecture_contracts/check_navigation_space_turn_benchmark.py
bash benchmarks/navigation_space_turn/run_mingw64.sh
```

Send the complete output.

## Decision after A* rerun

At 10k turn-aware p95:

- `<=40 ms`: accept static turn-aware worker/reference search and stop extending persistent static cost;
- `40-120 ms`: A* helps but global expanded search is still expensive; inspect remaining per-edge map lookups and/or move to stronger/hierarchical corridor reduction before dynamic integration;
- `>120 ms`: treat the candidate as a regression and repair before proceeding.

Also inspect `turn_portals_examined`: unlike the dense-state-only optimization, A* is expected to reduce this count materially.

After acceptance, next layer is combined static corridor + dynamic conflict/local horizon, followed by pursuit/receding-intercept consumption. Do not add ship velocity, braking or dynamic traffic to persistent `NavigationSpace` static cost.
