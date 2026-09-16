# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — adjacency accepted; dense RegionSlot corridor bookkeeping pending target-machine rerun

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Closed prior dynamic gate

`NAV-V2-MAP-2` selected hybrid ownership:

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

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Fresh target-machine behavior remains accepted:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Public `NavigationSpace.h` remains backend-neutral. Free-space AABB regions + portals are still only the deterministic CPU reference representation behind PImpl.

## Baseline scaling — accepted evidence

The original corridor BFS scanned the full portal map for every visited region.

10k baseline:

```text
open: corridor median/p95 = 1991.0357 / 2008.2074 ms
hub:  corridor median/p95 = 1951.6453 / 1956.0686 ms
portal examinations      = 285,913,245
```

This selected per-region adjacency as Optimization 1.

## Optimization 1 — per-region adjacency — ACCEPTED

Private connectivity became:

```text
regionId -> ordered portalId list
```

Target-machine rerun after adjacency:

```text
open_10k
    replace med/p95      13.3648 / 14.0799 ms
    point med/p95         0.3806 / 0.4356 ms
    corridor med/p95     21.7848 / 22.1695 ms
    invalidate med/p95    3.2622 / 3.4739 ms
    patch med/p95        16.1963 / 17.4121 ms

hub_10k
    replace med/p95      13.7190 / 15.3277 ms
    point med/p95         0.3836 / 0.4536 ms
    corridor med/p95     22.3689 / 22.7493 ms
    invalidate med/p95    3.3208 / 3.4248 ms
    patch med/p95        15.8516 / 16.7010 ms

portal examinations     57,189
corridor_found           1
```

Adjacency reduced 10k corridor median by about `87–91x` and portal examinations by about `5000x`.

The higher replace/patch cost is expected: topology publication now constructs connectivity. Full publication/local patch are not accepted frame-path operations.

## Remaining measured costs after adjacency

- point lookup is still a linear region scan but remains `<0.5 ms p95` at 10k;
- invalidation is a full region+portal scan at about `3.3 ms` median at 10k;
- transactional one-region patch copies full maps and rebuilds connectivity at about `16 ms` median;
- corridor remains about `22 ms` median because generic ordered-map bookkeeping still surrounds an otherwise compact adjacency traversal.

`CorridorDiagnostics::regionsVisited` currently includes the two start/end point-location scans plus BFS visits; do not misread ~20k diagnostics on a 10k topology as 20k unique graph regions.

Raw history: `benchmarks/navigation_space/RUN_LOG.md`.

## Optimization 2 candidate — dense RegionSlot graph bookkeeping

Implemented on `main` without changing `NavigationSpace.h`.

Private graph:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

Corridor BFS now uses:

```text
vector<uint8_t> visited
vector<Prev> previous
vector<RegionSlot> frontier
```

instead of ordered maps keyed by RegionId.

Determinism is preserved because RegionSlot assignment follows ordered RegionId iteration and adjacency edges follow ordered PortalId iteration. Query products remain stable RegionId/PortalId values.

The architecture contract now rejects both:

```text
full portal-map scan inside BFS
std::map<RegionId,...> visited/previous BFS bookkeeping
```

Point lookup, invalidation, transactional map copying and graph rebuild remain intentionally unchanged so the next measurement isolates the BFS bookkeeping improvement.

## RUN NOW

Because `NavigationSpace.cpp` changed, rerun behavior and both architecture gates before benchmarking:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Send the complete output of all four commands.

## Decision after dense-slot measurement

Do not add a third optimization before seeing the numbers.

Likely next choices:

- if corridor becomes a few milliseconds or less: stop optimizing unweighted BFS and move to measured point/invalidation/patch costs before weighted A*;
- if corridor remains materially expensive: remove remaining ordered-map state lookups from edge traversal / build a more compact immutable graph snapshot;
- if invalidation becomes the dominant interactive cost: add a spatial region index shared by point lookup + invalidation;
- if patch remains the dominant update cost: replace full transactional map copy/rebuild with bounded/chunked update ownership.

Live `EliteGame` / `EliteServer` integration remains later. Do not repair the old route-wide planner or delete migration code yet.
