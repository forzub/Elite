# NavigationSpace turn-aware benchmark

This isolated benchmark measures the extra cost of static turn-aware corridor search after `zigzag_vs_smooth` behavior is accepted.

It compares two policies on the **same published NavigationSpace snapshot**:

```text
zero_turn
    turnPenaltyMetersPerRadian = 0
    -> accepted v1 RegionSlot Dijkstra fast path

turn_aware
    turnPenaltyMetersPerRadian > 0
    -> expanded state (RegionSlot, incoming PortalId)
```

Pinned scales:

```text
open_1k / open_5k / open_10k
hub_1k  / hub_5k  / hub_10k
```

The topology is a deterministic 3D region lattice with bidirectional portals. Publication/BVH build is outside the timed section; this benchmark measures only `queryCostedCorridor()`.

Reported metrics:

```text
median / p95 ms
unique regions visited
portals examined
region-path length
coarse accumulated path turn radians
coarse meter-equivalent cost
```

`regionsVisited` in the public diagnostics remains a unique-region diagnostic. `portalsExamined` is the important expansion-work indicator for the turn-aware state space because a region may be expanded through multiple incoming portals.

Default run:

```bash
bash benchmarks/navigation_space_turn/run_mingw64.sh
```

Defaults:

```text
warmup=1
iterations=3
output=navigation_space_turn_benchmark.csv
```

Interpretation is evidence-driven. Turn-aware search is worker/reference work; it is not a frame-path requirement. If expanded-state p95 or portal examinations grow pathologically, optimize the private turn-aware state/queue representation before moving this mode toward frequent many-NPC replanning.
