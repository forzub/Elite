# NavigationSpace CPU reference benchmark

Stage: `NAV-V2-SPACE-1`.

This benchmark measures the first isolated CPU reference implementation of persistent static navigation space before any spatial acceleration structure is selected.

The reference representation is free-space AABB regions plus explicit portals behind the backend-neutral `NavigationSpace` API. The benchmark is intentionally diagnostic: it exposes the scaling cost of the current linear/map-based implementation so the next internal representation/index can be chosen from evidence.

## Scenarios

Two topology classes are generated at approximately 1k / 5k / 10k regions:

- `open_*`: large coarse free-space cells representing sparse open flight space;
- `hub_*`: small dense cells representing station/interior-like topology.

Each topology is a deterministic 3D lattice with explicit bidirectional portals to +X/+Y/+Z neighbours.

The benchmark uses a small agent envelope that is admitted by all generated regions/portals. Point and corridor queries target the highest-id/farthest region so the current linear reference is not accidentally benchmarked only on easy early-exit cases.

## Measurements

For every scenario the benchmark records median and p95 for:

- full `replaceStaticWorld()` publication;
- `queryPoint()`;
- `queryCorridor()`;
- bounded `invalidateBounds()` affecting one interior region;
- one-region transactional `applyLocalPatch()`.

Diagnostics include:

- total region/portal counts;
- point-query regions examined;
- corridor regions visited / portals examined;
- invalidated region/portal counts;
- corridor success.

The current reference is expected to expose expensive scaling because:

- point location scans regions linearly;
- BFS scans the complete portal map for each visited region;
- invalidation scans all regions and portals;
- local patching copies the current region/portal maps transactionally.

Those costs are measurement targets, not accepted production behavior.

## Run

```bash
bash benchmarks/navigation_space/run_mingw64.sh
```

Default run is intentionally short (`warmup=1`, `iterations=3`) because the unindexed 10k corridor case may be expensive. A longer pass can be requested explicitly after the default run:

```bash
bash benchmarks/navigation_space/run_mingw64.sh --warmup 3 --iterations 20
```

CSV output defaults to:

```text
navigation_space_cpu_benchmark.csv
```

## Decision rule

Do not change the public `NavigationSpace` API from benchmark results alone. Use the measured diagnostics to choose internal acceleration first:

- spatial point-location index if point lookup/invalidation dominates;
- adjacency lists / compact graph if portal scanning dominates corridor search;
- bounded copy-on-write / chunked topology if local patch copying dominates;
- hierarchical regions/bricks only if the measured scenario requires them.

The static-space backend remains CPU-owned under the accepted hybrid NavigationWorld architecture.
