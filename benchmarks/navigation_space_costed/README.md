# NavigationSpace costed-corridor benchmark

Stage: `NAV-V2-SPACE-1`.

This benchmark measures only the policy-aware static route selector exposed by:

```text
NavigationSpace::queryCostedCorridor()
```

It does not remeasure full static publication, BVH point lookup, invalidation, or the fast BFS topology oracle. Those paths already have accepted target-machine evidence in `benchmarks/navigation_space/RUN_LOG.md`.

## Workload

The benchmark uses the same topology scales as the accepted static-space scaling harness:

```text
open_1k
open_5k
open_10k
hub_1k
hub_5k
hub_10k
```

Each topology is a deterministic 3D region grid with explicit portals. Roughly one portal in five has reduced but still physically traversable clearance. This creates genuine alternate-route pressure for clearance-aware search without changing graph size between policies.

Two policies are measured on the same published `NavigationSpace` snapshot:

```text
distance_only
    distanceWeight=1
    preferredClearanceMultiple=1
    clearancePenaltyMeters=0

clearance_aware
    distanceWeight=1
    preferredClearanceMultiple>1
    clearancePenaltyMeters=2*cellSize
```

The exact preferred multiple is pinned by scenario in `main.cpp` so open-space and Hub scales exercise the same semantic choice with physically meaningful clearances.

## Metrics

For both policies the harness records:

```text
median ms
p95 ms
regions visited
portals examined
region-path length
reported coarse meter-equivalent cost
```

The reported cost is the current coarse region/portal metric. It is useful for deterministic branch comparison but is not yet a claim of exact physical trajectory length.

## Run

From MSYS2 MinGW64:

```bash
bash benchmarks/navigation_space_costed/run_mingw64.sh
```

Default measurement:

```text
warmup=1
iterations=5
```

Optional longer confirmation:

```bash
bash benchmarks/navigation_space_costed/run_mingw64.sh \
  --warmup 10 --iterations 100
```

CSV output defaults to:

```text
navigation_space_costed_benchmark.csv
```

## Decision use

This benchmark answers one question before turn/curvature cost is added:

> Is the current deterministic Dijkstra-style costed corridor implementation cheap enough for asynchronous coarse NPC/precision-planner route selection at 1k/5k/10k static topology scale?

Do not compare its time directly to the BFS oracle as though both perform identical work. Costed search intentionally evaluates route alternatives and policy penalties; the BFS oracle remains the cheaper topology-validity product.
