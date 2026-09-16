# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — static-space CPU reference accepted; scaling benchmark is the active gate

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Previous dynamic-map stage — CLOSED

`NAV-V2-MAP-2` selected hybrid ownership from target-machine measurements:

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

At 10k actors the accepted long-run GPU p95 was `1.3226 ms` cruise and `1.6258 ms` Hub on Quadro RTX 3000; CPU compact dynamic queries remained sub-0.2-ms p95 while full CPU rebuild was the expensive path.

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Canonical block:

```text
src/world/navigation/space/
    NavigationSpace.h
    NavigationSpace.cpp
    CMakeLists.txt
    README.md
```

Public concepts:

```text
AgentEnvelope
RegionInput
PortalInput
StaticSpaceUpdate
LocalPatch
queryPoint
queryCorridor
invalidateBounds
stats
```

Reference representation is free-space AABB regions + explicit portals behind PImpl. Public API does not commit production storage to AABBs, `std::map`, BFS or any specific acceleration structure.

Fresh target-machine evidence on canonical `main`:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Accepted reference behavior:

- point admission is parameterized by agent radius + additional clearance;
- region and portal clearance reject oversized agents;
- deterministic region/portal corridor is returned for connected traversable topology;
- disconnected/invalidated topology fails closed;
- bounded invalidation affects intersecting regions and touching portals;
- local patches are transactional and rejected patches do not partially mutate state.

## Active gate — static-space scaling benchmark

New isolated benchmark:

```text
benchmarks/navigation_space/
    main.cpp
    CMakeLists.txt
    run_mingw64.sh
    README.md

tests/architecture_contracts/check_navigation_space_benchmark.py
```

The benchmark generates deterministic `open_*` and `hub_*` 3D region/portal lattices at approximately 1k / 5k / 10k regions and measures independently:

```text
replaceStaticWorld
queryPoint
queryCorridor
invalidateBounds
applyLocalPatch
```

It also records:

```text
point regions examined
corridor regions visited
corridor portals examined
invalidated region/portal counts
corridor success
```

This is intentionally a stress/diagnostic benchmark of the current reference. Known implementation costs being measured are:

```text
queryPoint       -> linear region scan
queryCorridor    -> BFS + full portal-map scan per visited region
invalidateBounds -> full region + portal scan
applyLocalPatch  -> transactional copy of region + portal maps
```

Do not optimize before seeing the target-machine numbers.

## RUN NOW

The boundary/behavior gate has already passed and does not need to be repeated for this benchmark-only change.

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Default benchmark is deliberately short (`warmup=1`, `iterations=3`) because the unindexed 10k corridor case may expose pathological scaling.

Send the complete output. CSV is written to:

```text
D:\__elite\work\navigation_space_cpu_benchmark.csv
```

## Decision after measurement

Choose the **first internal optimization** from evidence, while keeping `NavigationSpace.h` stable:

- point/invalidation expensive -> add spatial point-location/invalidation index;
- corridor dominated by portal scans -> build compact per-region adjacency first;
- local patch expensive -> replace whole-map copy with bounded/chunked copy-on-write or equivalent;
- only introduce hierarchical regions/bricks if measured scale actually requires them.

After the reference is indexed and remeasured, add costed corridor search (portal traversal cost / distance / clearance policy) and static-space benchmark acceptance thresholds. Live `EliteGame` / `EliteServer` integration remains later.

## Not yet

Do not yet wire NavigationSpace into live runtime, repair the old route-wide planner, implement final NPC steering/precision docking, implement Shift+F12 debug rendering, or delete migration navigation code.
