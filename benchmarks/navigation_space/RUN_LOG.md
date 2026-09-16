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

## Interpretation

The first optimization target is unambiguous: corridor BFS was scanning the complete portal map for every visited region. From 1k to 10k regions, corridor median increased by roughly 102-105x while portal examinations increased by roughly 106-108x.

Other costs are secondary at this stage:

- worst-case linear point lookup at 10k is ~0.28 ms median/p95 and remains inside the current ordinary main-thread budget;
- invalidation reaches ~2.6-2.8 ms median at 10k and should eventually receive a spatial index / worker-side treatment;
- one-region transactional patch reaches ~7.6-8.7 ms median because the reference copies the full ordered region/portal maps;
- full publication is ~6.7-7.2 ms median at 10k and is not intended as a per-frame operation.

## Optimization 1 — private per-region portal adjacency

`NavigationSpace.h` remains unchanged.

The private implementation now owns:

```text
regionId -> ordered portalId list
```

The adjacency index is rebuilt transactionally with full publication/local patch and preserves stable PortalId ordering. `queryCorridor()` now examines only portals adjacent to the current region instead of scanning the complete portal map at each BFS step.

Architecture contract now rejects a regression to full portal-map scanning inside corridor BFS.

Status: **pending target-machine behavioral + benchmark rerun**.
