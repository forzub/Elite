# NavigationSpace target-machine benchmark log

**Stage:** `NAV-V2-SPACE-1`  
**Date:** 2026-09-16  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine

All runs below used the same deterministic `open_*` and `hub_*` topology generator unless noted otherwise.

## Baseline — unindexed corridor traversal

Architecture gate:

```text
NAVIGATION SPACE BENCHMARK CONTRACT: PASS
```

Default benchmark: `warmup=1`, `iterations=3`.

```text
scenario  regions portals  replace med/p95  point med/p95   corridor med/p95      invalidate med/p95  patch med/p95
open_1k     1000    2650    0.6289/0.8802    0.0184/0.0322    18.9418/20.9415       0.2384/0.2802        0.5203/0.5427
open_5k     5000   14050    3.3126/3.3299    0.1418/0.2394   474.7646/478.0360      1.2677/1.5127        2.9986/3.0979
open_10k   10000   28600    6.6942/7.2406    0.2787/0.2812  1991.0357/2008.2074     2.5731/2.9869        7.6135/7.8543
hub_1k      1000    2700    0.6206/0.7220    0.0180/0.0291    19.0919/19.1640       0.2440/0.2490        0.5087/0.5761
hub_5k      5000   14050    3.2571/3.4590    0.1360/0.2489   482.6135/482.7897      1.2645/1.2761        3.0438/3.4638
hub_10k    10000   28600    7.2397/7.5269    0.2784/0.2845  1951.6453/1956.0686     2.8412/3.2230        8.6678/8.7083
```

Diagnostics:

```text
1k  corridor portal examinations ≈2.64-2.69 million
5k  corridor portal examinations =70,206,895
10k corridor portal examinations =285,913,245
```

Interpretation: corridor BFS scanned the complete portal map for every visited region. From 1k to 10k, corridor time and portal examinations were effectively quadratic/pathological.

## Optimization 1 — per-region portal adjacency — ACCEPTED

Private connectivity:

```text
RegionId -> ordered PortalId[]
```

Target-machine rerun:

```text
scenario  regions portals  replace med/p95   point med/p95   corridor med/p95   invalidate med/p95  patch med/p95
open_1k     1000    2650    1.2020/1.2097     0.0246/0.0287    1.3816/1.3825      0.2474/0.2895        1.1639/1.1795
open_5k     5000   14050    6.5815/6.6636     0.1446/0.1460    9.4788/9.5562      1.2609/1.5969        7.2783/7.9176
open_10k   10000   28600   13.3648/14.0799    0.3806/0.4356   21.7848/22.1695     3.2622/3.4739       16.1963/17.4121
hub_1k      1000    2700    1.0866/1.0872     0.0113/0.0264    1.3178/1.9948      0.2314/0.2369        1.1989/1.8382
hub_5k      5000   14050    6.4667/6.7791     0.1418/0.2358    9.5743/10.1848     1.5549/1.9001        7.1233/7.3557
hub_10k    10000   28600   13.7190/15.3277    0.3836/0.4536   22.3689/22.7493     3.3208/3.4248       15.8516/16.7010
```

10k portal examinations fell to `57,189`.

```text
open_10k corridor median 1991.0357 -> 21.7848 ms  (~91.4x faster)
hub_10k  corridor median 1951.6453 -> 22.3689 ms  (~87.2x faster)
portal examinations      285,913,245 -> 57,189    (~5000x fewer)
```

Publication/patch cost increased because connectivity is constructed transactionally. Those operations are not frame-path work.

## Optimization 2 — dense RegionSlot graph bookkeeping — ACCEPTED

Private graph:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS `visited`, `previous` and frontier became vector-backed.

The first architecture rerun reported:

```text
[FAIL] NavigationSpace CPU reference marker missing: std::vector<RegionSlot> frontier
```

This was an architecture-test string defect. The implementation correctly used `std::vector<Impl::RegionSlot>`. The C++ behavior test passed and the benchmark built/ran. The marker was fixed on `main`.

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

```text
open_10k corridor median 21.7848 -> 7.9396 ms  (~2.74x faster)
hub_10k  corridor median 22.3689 -> 7.9981 ms  (~2.80x faster)
```

At this point coarse unweighted global corridor search is accepted as asynchronous worker/reference work; further BFS micro-optimization is deferred.

## Optimization 3 — private RegionSlot AABB BVH — ACCEPTED

Private static index:

```text
RegionSlot[] -> AABB BVH
```

Used by:

```text
queryPoint()
queryCorridor() endpoint localization
invalidateBounds()
```

Endpoint portal invalidation uses:

```text
RegionSlot -> incident PortalId[]
```

so damage/local invalidation no longer scans every portal.

Fresh target-machine gate:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
NAVIGATION SPACE BENCHMARK CONTRACT: PASS
```

Default benchmark remained `warmup=1`, `iterations=3`.

Measured results:

```text
scenario  regions portals  replace med/p95   point med/p95   corridor med/p95  invalidate med/p95  patch med/p95
open_1k     1000    2650    2.7210/2.8374     0.0046/0.0050   0.4817/0.4906      0.0039/0.0053        2.7643/3.0607
open_5k     5000   14050   19.9254/20.0685    0.0061/0.0062   3.2524/3.2940      0.0080/0.0091       20.5599/20.9438
open_10k   10000   28600   44.2686/45.9464    0.0065/0.0353   7.3479/7.7085      0.0102/0.0112       46.6777/51.2959
hub_1k      1000    2700    2.7646/2.8178     0.0043/0.0044   0.4898/0.4934      0.0033/0.0051        2.8668/3.0214
hub_5k      5000   14050   19.5868/19.7882    0.0058/0.0064   3.1884/3.2706      0.0118/0.0159       19.8892/20.4986
hub_10k    10000   28600   45.2447/48.0735    0.0064/0.0099   8.0958/8.2105      0.0109/0.0115       48.7458/48.8502
```

10k diagnostics:

```text
point regions examined=5
corridor regions visited≈10,003
corridor portals examined=57,189
invalidated regions=1
invalidated portals=6
corridor_found=1
```

Interpretation:

- `open_10k` point p95: `0.2814 -> 0.0353 ms`;
- `hub_10k` point p95: `0.7043 -> 0.0099 ms`;
- 10k point candidate examination: `10,000 -> 5`;
- `open_10k` invalidation median: `3.6275 -> 0.0102 ms`;
- `hub_10k` invalidation median: `3.2642 -> 0.0109 ms`.

The static ordinary-query side is therefore accepted: point lookup and bounded local invalidation are comfortably sub-millisecond.

The tradeoff is publication/update cost. Full replace and one-region transactional patch now rebuild graph + BVH and cost roughly `44–49 ms` median at 10k. This is acceptable only because those paths remain worker/update operations. Before frequent live topology mutation, bounded/chunked update ownership will need its own measured stage.

## Costed corridor semantics — ACCEPTED

After Optimization 3, route quality became the next problem.

`queryCorridor()` remains the fast deterministic topology/BFS oracle. A separate `queryCostedCorridor()` adds policy-aware route choice:

```text
edge cost = distanceWeight * coarse geometric distance
          + clearance penalty
```

Policy fields:

```text
distanceWeight
preferredClearanceMultiple
clearancePenaltyMeters
```

Target-machine behavior gate:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
Total Test time = 0.05 sec
```

Pinned behavior is therefore accepted:

```text
wall_with_aperture
    fitting agent -> route through opening
    oversized agent -> opening rejected

canyon_vs_overflight
    distance-only policy -> short canyon
    clearance-aware policy -> longer open route
    oversized canyon agent -> open route
```

This is the explicit static-space contract that openings, tunnels, breaches and canyons can be valid routes when the requesting agent fits; the enclosing obstacle is not one indivisible keep-out volume.

`totalCostMetersEquivalent` remains a coarse region/portal branch-comparison metric, not exact physical trajectory length. Turn/curvature, dynamic traffic/risk and moving-goal prediction remain later layers.

## Active next gate — costed corridor scaling

Dedicated harness:

```text
benchmarks/navigation_space_costed/
```

It measures `distance_only` and `clearance_aware` policies on open/hub 1k/5k/10k topology scales. Reduced-clearance portals are deterministic and remain physically traversable, so the clearance-aware profile performs real alternate-route evaluation.

Metrics:

```text
median/p95 ms
regions visited
portals examined
region-path length
coarse reported cost
```

Status: **benchmark implementation on `main`, pending target-machine contract + measurement**.
