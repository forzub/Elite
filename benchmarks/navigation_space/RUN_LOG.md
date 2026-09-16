# NavigationSpace target-machine benchmark log

**Stage:** `NAV-V2-SPACE-1`  
**Date:** 2026-09-16  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine

## Baseline — unindexed corridor traversal

Architecture gate:

```text
NAVIGATION SPACE BENCHMARK CONTRACT: PASS
```

Default benchmark:

```text
warmup=1
iterations=3
```

Measured target-machine results:

```text
scenario  regions portals  replace med/p95  point med/p95   corridor med/p95      invalidate med/p95  patch med/p95
open_1k     1000    2650    0.6289/0.8802    0.0184/0.0322    18.9418/20.9415       0.2384/0.2802        0.5203/0.5427
open_5k     5000   14050    3.3126/3.3299    0.1418/0.2394   474.7646/478.0360      1.2677/1.5127        2.9986/3.0979
open_10k   10000   28600    6.6942/7.2406    0.2787/0.2812  1991.0357/2008.2074     2.5731/2.9869        7.6135/7.8543
hub_1k      1000    2700    0.6206/0.7220    0.0180/0.0291    19.0919/19.1640       0.2440/0.2490        0.5087/0.5761
hub_5k      5000   14050    3.2571/3.4590    0.1360/0.2489   482.6135/482.7897      1.2645/1.2761        3.0438/3.4638
hub_10k    10000   28600    7.2397/7.5269    0.2784/0.2845  1951.6453/1956.0686     2.8412/3.2230        8.6678/8.7083
```

Diagnostic counts:

```text
1k:  point regions examined=1000
     corridor portals examined≈2.64-2.69 million

5k:  point regions examined=5000
     corridor portals examined=70,206,895

10k: point regions examined=10000
     corridor portals examined=285,913,245
```

Each invalidation affected one region and six touching portals; every corridor query succeeded.

### Baseline interpretation

The first optimization target was unambiguous: corridor BFS scanned the complete portal map for every visited region. From 1k to 10k regions, corridor median increased by roughly 102-105x while portal examinations increased by roughly 106-108x.

Other baseline costs:

- worst-case linear point lookup at 10k was ~0.28 ms;
- invalidation reached ~2.6-2.8 ms median at 10k;
- one-region transactional patch reached ~7.6-8.7 ms median;
- full publication was ~6.7-7.2 ms median at 10k.

## Optimization 1 — private per-region portal adjacency

`NavigationSpace.h` remained unchanged.

Private connectivity became:

```text
regionId -> ordered portalId list
```

The adjacency index is rebuilt transactionally with full publication/local patch and preserves stable PortalId ordering. `queryCorridor()` examines only portals adjacent to the current region.

Architecture contract rejects regression to full portal-map scanning inside corridor BFS.

### Target-machine rerun after Optimization 1

Behavior / architecture:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
NAVIGATION SPACE BENCHMARK CONTRACT: PASS
```

Default benchmark remained:

```text
warmup=1
iterations=3
```

Measured results:

```text
scenario  regions portals  replace med/p95   point med/p95   corridor med/p95   invalidate med/p95  patch med/p95
open_1k     1000    2650    1.2020/1.2097     0.0246/0.0287    1.3816/1.3825      0.2474/0.2895        1.1639/1.1795
open_5k     5000   14050    6.5815/6.6636     0.1446/0.1460    9.4788/9.5562      1.2609/1.5969        7.2783/7.9176
open_10k   10000   28600   13.3648/14.0799    0.3806/0.4356   21.7848/22.1695     3.2622/3.4739       16.1963/17.4121
hub_1k      1000    2700    1.0866/1.0872     0.0113/0.0264    1.3178/1.9948      0.2314/0.2369        1.1989/1.8382
hub_5k      5000   14050    6.4667/6.7791     0.1418/0.2358    9.5743/10.1848     1.5549/1.9001        7.1233/7.3557
hub_10k    10000   28600   13.7190/15.3277    0.3836/0.4536   22.3689/22.7493     3.3208/3.4248       15.8516/16.7010
```

10k diagnostics:

```text
point regions examined=10,000
corridor portals examined=57,189
corridor found=1
invalidated regions=1
invalidated portals=6
```

### Optimization-1 interpretation

The adjacency index solved the pathological full-portal scan:

```text
open_10k corridor median: 1991.0357 -> 21.7848 ms  (~91.4x faster)
hub_10k  corridor median: 1951.6453 -> 22.3689 ms  (~87.2x faster)
portal examinations:      285,913,245 -> 57,189    (~5000x fewer)
```

This isolates the remaining corridor cost. It is no longer caused by checking irrelevant portals. The current BFS still uses ordered `std::map<RegionId,...>` containers for visited/predecessor bookkeeping and ordered-map lookups for graph state.

The connectivity index moves work into topology publication/patching: 10k full replacement is now ~13-14 ms median and one-region transactional patch ~16 ms median. These are not accepted frame-path operations and remain later optimization targets.

Point lookup remains sub-0.5-ms p95 at 10k, while invalidation is ~3.3 ms median and is also a later spatial-index candidate.

## Optimization 2 candidate — dense RegionSlot graph bookkeeping

`NavigationSpace.h` remains unchanged.

The private graph now adds:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS `visited`, `previous`, and frontier storage are vector-backed by dense slots rather than `std::map<RegionId,...>`. Stable public RegionId/PortalId results and deterministic PortalId traversal order are preserved.

Point lookup, invalidation, transactional map copying and graph rebuild costs are intentionally unchanged in this slice.

Status: **pending target-machine behavior + benchmark rerun**.
