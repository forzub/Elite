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

The connectivity index moves work into topology publication/patching: 10k full replacement is now ~13-14 ms median and one-region transactional patch ~16 ms median. These are not accepted frame-path operations and remain later optimization targets.

## Optimization 2 — dense RegionSlot graph bookkeeping — ACCEPTED

`NavigationSpace.h` remained unchanged.

The private graph added:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS `visited`, `previous`, and frontier storage became vector-backed by dense slots rather than `std::map<RegionId,...>`. Stable public RegionId/PortalId results and deterministic PortalId traversal order were preserved.

The first architecture-contract rerun reported:

```text
[FAIL] NavigationSpace CPU reference marker missing: std::vector<RegionSlot> frontier
```

This was a **contract bug**, not a code failure: the implementation correctly used `std::vector<Impl::RegionSlot> frontier`. The C++ behavior test passed and the benchmark executable built and ran. The architecture marker was subsequently corrected on `main`.

### Target-machine rerun after Optimization 2

```text
navigation_space: 1/1 PASS
100% tests passed, 0 failed
NAVIGATION SPACE BENCHMARK CONTRACT: PASS
```

Default benchmark:

```text
warmup=1
iterations=3
```

Measured results:

```text
scenario  regions portals  replace med/p95   point med/p95   corridor med/p95  invalidate med/p95  patch med/p95
open_1k     1000    2650    1.3405/1.3662     0.0174/0.0267   0.4599/0.4927      0.2420/0.2611        1.2210/1.3863
open_5k     5000   14050    7.0448/7.0584     0.1446/0.1618   3.2067/3.2989      1.5250/1.7706        7.3859/8.1861
open_10k   10000   28600   14.8300/15.1018    0.2783/0.2814   7.9396/7.9735      3.6275/3.7643       16.0010/16.0111
hub_1k      1000    2700    1.2987/1.3223     0.0151/0.0307   0.4365/0.4877      0.2408/0.2415        1.2337/1.4807
hub_5k      5000   14050    6.7531/7.5369     0.1450/0.1806   3.5757/3.7458      1.4276/1.4323        7.8196/7.8935
hub_10k    10000   28600   14.1335/14.9556    0.4301/0.7043   7.9981/8.4877      3.2642/3.3515       16.1113/16.1715
```

Diagnostics remained topology-equivalent:

```text
10k corridor portals examined=57,189
corridor_found=1
invalidated_regions=1
invalidated_portals=6
```

### Optimization-2 interpretation

Dense slots removed another large portion of generic graph bookkeeping:

```text
open_10k corridor median: 21.7848 -> 7.9396 ms  (~2.74x faster)
hub_10k  corridor median: 22.3689 -> 7.9981 ms  (~2.80x faster)
```

Combined with Optimization 1, the 10k coarse-corridor reference moved from roughly two seconds to roughly eight milliseconds while preserving deterministic output.

At this point unweighted coarse global corridor search is acceptable as worker-side reference work. It is not the next frame-path optimization target.

Remaining measured costs after Optimization 2:

- point lookup still scans region storage linearly and reached `0.7043 ms p95` in `hub_10k`;
- invalidation remains a full region+portal scan at roughly `3.3-3.6 ms` median;
- one-region transactional patch remains ~`16 ms` median and is update/worker-path work;
- full publication remains ~`14-15 ms` median and is update/worker-path work.

## Optimization 3 candidate — private RegionSlot spatial BVH

`NavigationSpace.h` remains unchanged.

The private implementation now adds an AABB BVH over dense `RegionSlot` identity:

```text
RegionSlot[] -> private AABB BVH
```

The BVH is used for:

```text
queryPoint candidate reduction
corridor start/end region localization
invalidateBounds candidate reduction
```

Portal invalidation no longer scans the complete portal map. A private endpoint incidence list:

```text
RegionSlot -> PortalId[] touching the region
```

invalidates only portals touching newly invalidated regions, including incoming one-way portals.

Exact region containment/intersection and clearance tests remain authoritative after candidate reduction. Candidate slots are sorted before semantic evaluation so stable RegionId behavior is preserved.

Full publication/local patch rebuild graph + BVH transactionally. Their cost may increase; they remain worker/update-path operations.

Status: **pending target-machine architecture + behavior + benchmark rerun**.
