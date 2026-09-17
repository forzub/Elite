# NavigationSpace turn-aware benchmark log

**Stage:** `NAV-V2-SPACE-1` — **CLOSED / ACCEPTED**  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine  
**Compiler:** GNU 15.2.0

## Optimization 3 — published static hot geometry — ACCEPTED

Target-machine run used canonical commit:

```text
1acaddc1771d3b1a9466dfec7b0974379d74fcd1
```

Gates:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
NAVIGATION SPACE TURN BENCHMARK CONTRACT: PASS
```

Measured output:

```text
scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_1k       0.5376        0.8065          5,297        28,520
open_5k       4.5447        5.5494         28,097       159,480
open_10k      8.4498       12.0072         57,197       329,660
hub_1k        0.5282        0.8358          5,397        29,580
hub_5k        3.8803        5.3406         28,097       159,480
hub_10k       8.4125       11.9065         57,197       329,660
```

All routes were found. The 10k accepted route products remained unchanged:

```text
open route regions = 63
hub  route regions = 63
turn path radians  = 3.1416
open turn cost     = 76284.9556
hub  turn cost     = 3178.5398
```

The pinned acceptance rule was:

```text
<=40 ms p95   accept worker/reference turn search
40-120 ms     continue optimization
>120 ms       reject/repair immediately
```

Both 10k cases pass with large margin.

Compared with dense `TurnStateSlot` Dijkstra before publishing hot geometry:

```text
open_10k  67.9647 -> 12.0072 ms   ~5.66x faster
hub_10k   72.6054 -> 11.9065 ms   ~6.10x faster
```

Compared with the original tree-backed implementation:

```text
open_10k 216.1602 -> 12.0072 ms   ~18.0x faster
hub_10k  232.6520 -> 11.9065 ms   ~19.5x faster
```

`turn_portals_examined` returned to the exact dense-Dijkstra value `329,660`. This is decisive evidence for this reference workload: the accepted expanded-state semantics were not the practical bottleneck after dense state conversion; repeated per-transition map lookup and geometry (`boundsCenter`, `sqrt`, `acos`) were.

Accepted private representation publishes dense region/portal state, static per-edge geometry/clearance and precomputed turn angles. The query keeps exact dense Dijkstra ordering and accepted semantic state `(RegionSlot, incoming PortalId)`.

`NAV-V2-SPACE-1` is closed. Do not extend static turn policy or optimize this search further without new runtime evidence.

---

## Optimization 2 — Euclidean A* — REJECTED

Target-machine run used canonical commit:

```text
5376e7179cfe791df843eb846c2d4c8af41432cd
```

Gates passed and route semantics remained correct, but measured 10k output was:

```text
scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_10k     11.2399       97.0537         57,197       323,888
hub_10k       9.3477       97.6909         57,197       323,888
```

Compared with dense Dijkstra:

```text
open_10k turn p95  67.9647 -> 97.0537 ms   (+42.8%)
hub_10k  turn p95  72.6054 -> 97.6909 ms   (+34.6%)
turn portals       329,660  -> 323,888      (-1.75%)
```

The heuristic reduced expanded portal work only marginally while adding enough `h(n)` geometry/queue overhead to create a clear timing regression. The candidate is rejected and must not be restored without a materially stronger measured lower bound.

---

## Optimization 1 — dense arrival states + binary heap — IMPROVED, ABOVE GATE

Measured target-machine results after replacing tree-backed turn state with dense `TurnStateSlot` vectors + binary heap:

```text
scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_1k       0.5800        5.0260          5,297        28,520
open_5k       4.0222       35.1876         28,097       159,480
open_10k      8.6427       67.9647         57,197       329,660
hub_1k        0.6405        5.0208          5,397        29,580
hub_5k        4.1600       31.2744         28,097       159,480
hub_10k       8.5862       72.6054         57,197       329,660
```

This removed a large amount of container overhead but left the search above the <=40 ms gate and left transition work unchanged.

---

## Baseline — tree-backed expanded state — REJECTED FOR PERFORMANCE

```text
scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_1k       0.5753       12.9429          5,297        28,520
open_5k       4.4013       89.3081         28,097       159,480
open_10k      9.6982      216.1602         57,197       329,660
hub_1k        0.6575       14.0834          5,397        29,580
hub_5k        3.8335       97.5381         28,097       159,480
hub_10k       9.8862      232.6520         57,197       329,660
```

The rejected implementation represented every expanded state `(RegionSlot, incoming PortalId)` through ordered maps and used a multimap frontier. Its semantics remain accepted; its representation is permanently rejected for this reference path.
