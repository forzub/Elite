# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — optimize turn-aware expanded-state search

## Accepted foundation

Static indexing, costed corridor v1, and static turn-cost behavior are accepted.

Accepted 10k reference evidence:

```text
coarse BFS corridor              ≈7.3-8.2 ms
BVH point p95                    <=0.0353 ms
BVH invalidation p95             <=0.0115 ms
costed v1 p95                    <=9.6103 ms
```

Turn-aware semantics are also accepted:

```text
turnPenalty == 0
    -> RegionSlot Dijkstra fast path

turnPenalty > 0
    -> semantic state = (RegionSlot, incoming PortalId)
```

`zigzag_vs_smooth`, aperture and canyon/overflight behavior all pass on target MinGW64.

## Rejected performance baseline

Dedicated turn benchmark:

```text
open_10k zero p95       9.6982 ms
open_10k turn p95     216.1602 ms
hub_10k  zero p95       9.8862 ms
hub_10k  turn p95     232.6520 ms

10k portals examined
zero-turn              57,197
turn-aware            329,660
```

The pinned decision rule was `>120 ms p95 -> optimize before next NavigationWorld layer`, therefore the original tree-backed expanded-state implementation is rejected for performance.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

## Active candidate — dense turn state + heap

The semantic state is unchanged, but private representation is now:

```text
published graph:
    directed portal arrival -> TurnStateSlot
    adjacency edge -> arrivalTurnStateSlot

turn-aware query:
    vector<double> bestCost
    vector<TurnStateSlot> previous
    vector<uint8_t> settled
    binary priority_queue frontier
```

The zero-turn v1 Dijkstra path is untouched.

This intentionally does **not** reduce `portalsExamined`; it removes ordered-tree bookkeeping so the rerun can separate container cost from the unavoidable ~330k expanded-state edge checks.

## RUN NOW

Because `NavigationSpace.cpp` and its architecture contract changed, run behavior/architecture first and then the same turn benchmark:

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

## Decision after rerun

At 10k turn-aware p95:

- `<=40 ms`: accept the dense expanded-state worker/reference solve and move to dynamic conflict/local-horizon composition;
- `40-120 ms`: container optimization helped, but remaining expansion is still expensive; add an admissible A* / stronger search reduction before moving on;
- `>120 ms`: treat the current expanded search as still pathological and optimize immediately.

Do not add more static policy terms now. Do not put velocity, braking, dynamic traffic or pursuit prediction into persistent `NavigationSpace` cost.
