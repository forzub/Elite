# NavigationSpace CPU reference benchmark

Stage: `NAV-V2-SPACE-1`.

This benchmark measures the isolated CPU reference implementation of persistent static navigation space while internal acceleration structures are selected from evidence.

The public representation remains free-space regions plus explicit portals behind the backend-neutral `NavigationSpace` API. Benchmark results may change internal indexes; they do not change the public boundary by themselves.

## Scenarios

Two topology classes are generated at approximately 1k / 5k / 10k regions:

- `open_*`: large coarse free-space cells representing sparse open flight space;
- `hub_*`: small dense cells representing station/interior-like topology.

Each topology is a deterministic 3D lattice with explicit bidirectional portals to +X/+Y/+Z neighbours.

The benchmark uses a small agent envelope admitted by all generated regions/portals. Point and corridor queries target the highest-id/farthest region so easy early-exit cases do not hide scaling costs.

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

## Baseline result and first optimization

The initial target-machine baseline used linear point/invalidation scans, ordered maps, and a corridor BFS that scanned the complete portal map for each visited region.

At 10k regions it examined about `285.9 million` portal records and corridor median was about `1.95-1.99 seconds`. This dominated every other static-space cost by orders of magnitude.

The first internal optimization is therefore a private deterministic per-region adjacency index:

```text
regionId -> ordered portalId list
```

The current rerun measures the same exact benchmark workload after that change. `NavigationSpace.h` is unchanged.

Point lookup, invalidation and local patch remain deliberately unoptimized so the next bottleneck is visible after corridor traversal is repaired.

Raw before/after history is recorded in `RUN_LOG.md`.

## Run

```bash
bash benchmarks/navigation_space/run_mingw64.sh
```

Default run remains short:

```text
warmup=1
iterations=3
```

A longer pass can be requested after the optimized default run succeeds:

```bash
bash benchmarks/navigation_space/run_mingw64.sh --warmup 3 --iterations 20
```

CSV output defaults to:

```text
navigation_space_cpu_benchmark.csv
```

## Decision rule

After the adjacency rerun, choose the next internal optimization from measured cost while keeping `NavigationSpace.h` stable:

- point/invalidation expensive -> add spatial point-location/invalidation index;
- local patch expensive -> replace whole-map transactional copying with bounded/chunked copy-on-write or equivalent;
- corridor still too expensive -> move from ordered-map BFS bookkeeping to a compact graph / costed search representation;
- introduce hierarchical regions/bricks only if measured scale requires them.

The static-space backend remains CPU-owned under the accepted hybrid NavigationWorld architecture.
