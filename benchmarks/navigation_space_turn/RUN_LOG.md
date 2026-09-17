# NavigationSpace turn-aware benchmark log

**Stage:** `NAV-V2-SPACE-1`  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine  
**Compiler:** GNU 15.2.0

## Optimization 3 candidate — published static hot geometry — PENDING RERUN

The Euclidean A* candidate below did not reduce enough search work to justify its overhead. Positive-turn ordering is therefore restored to dense-state Dijkstra (`g` only), while immutable static work is moved out of the query hot loop and into graph publication.

Published dense/private data now includes:

```text
RegionSlot
    center
    capacity
    invalidation flag

PortalSlot
    center
    invalidation flag

AdjacencyEdge
    portalSlot
    neighborSlot
    arrivalTurnStateSlot
    geometricMeters
    availableClearanceMeters

TurnStateSlot
    flattened precomputed turn angles to outgoing adjacency edges
```

`invalidateBounds()` synchronizes dense invalidation flags with authoritative map state.

The positive-turn expanded-state loop should now avoid ordered-map region/portal lookups and avoid repeated `boundsCenter`, geometric `sqrt`, and turn `acos`. Policy multipliers remain query-time.

This candidate intentionally may keep `turn_portals_examined` near the dense-Dijkstra `329,660`; the measurement is cost per examined transition. Gate remains `<=40 ms p95` at 10k.

---

## Optimization 2 — Euclidean A* — REJECTED

Target-machine run used canonical commit:

```text
5376e7179cfe791df843eb846c2d4c8af41432cd
```

Architecture/behavior:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Benchmark contract:

```text
NAVIGATION SPACE TURN BENCHMARK CONTRACT: PASS
```

Measured output:

```text
scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_1k       1.0576        5.3155          5,297        25,560
open_5k       4.1389       43.9962         28,097       154,626
open_10k     11.2399       97.0537         57,197       323,888
hub_1k        0.6235        7.2722          5,397        26,904
hub_5k        4.1236       41.8217         28,097       154,626
hub_10k       9.3477       97.6909         57,197       323,888
```

All routes were found. At 10k:

```text
open route regions = 63
hub  route regions = 63
turn path radians  = 3.1416
open turn cost     = 76284.9556
hub  turn cost     = 3178.5398
```

So route semantics remained correct.

Compared with Optimization 1 dense Dijkstra:

```text
open_10k turn p95  67.9647 -> 97.0537 ms   (+42.8%)
hub_10k  turn p95  72.6054 -> 97.6909 ms   (+34.6%)
turn portals       329,660  -> 323,888      (-1.75%)
```

The heuristic therefore reduced expanded portal work only marginally while adding enough `h(n)` geometry/queue work to create a clear timing regression. The candidate is rejected.

Important topology observation: the 10k benchmark route itself contains only 63 regions, yet `turn_regions_visited` remains 10,006 diagnostics entries and portal work remains above 323k. Straight-line Euclidean distance is too weak a discriminator for this regular 3D grid.

Do not restore the same Euclidean A* candidate without a materially stronger lower bound.

---

## Optimization 1 — dense arrival states + binary heap — IMPROVED, STILL ABOVE GATE

Fresh target-machine architecture/behavior gate:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

The dedicated benchmark-contract itself reported a false documentation failure:

```text
[FAIL] CURRENT_TASK does not declare the turn-aware performance gate
```

The benchmark executable still built and ran successfully. This was a stale exact-text assertion in `check_navigation_space_turn_benchmark.py`, not a runtime/search failure; the contract was subsequently changed to check the active benchmark path + turn-aware task semantics instead of one brittle sentence.

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

Dense state/heap therefore reduced 10k turn-aware p95 by roughly 3x:

```text
open_10k  216.1602 -> 67.9647 ms
hub_10k   232.6520 -> 72.6054 ms
```

The expansion work did not change, as intended: `turn_portals_examined` remained `329,660` at 10k. This isolated the remaining cost as expanded-state search work plus per-expanded-edge runtime cost rather than ordered turn-state container bookkeeping.

The pinned rule for the rerun was:

```text
<= 40 ms p95   accept worker-side reference
40-120 ms      optimize before moving on
> 120 ms       pathological; optimize immediately
```

Both 10k runs were in the middle band. Turn semantics stayed accepted; dense state/heap remained accepted as a private representation improvement.

---

## Baseline — tree-backed expanded state — REJECTED FOR PERFORMANCE

Architecture benchmark contract:

```text
NAVIGATION SPACE TURN BENCHMARK CONTRACT: PASS
```

Run configuration:

```text
warmup=1
iterations=3
```

Measured target-machine results:

```text
scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_1k       0.5753       12.9429          5,297        28,520
open_5k       4.4013       89.3081         28,097       159,480
open_10k      9.6982      216.1602         57,197       329,660
hub_1k        0.6575       14.0834          5,397        29,580
hub_5k        3.8335       97.5381         28,097       159,480
hub_10k       9.8862      232.6520         57,197       329,660
```

All routes were found. `regions_visited` remained about the topology region count because diagnostics count unique regions, while the turn-aware solver expanded multiple arrival states per region. The decisive diagnostic was portal work: about `329,660` examined directed transitions at 10k versus about `57,197` for zero-turn v1.

The predeclared decision rule was:

```text
<= 40 ms p95   accept worker-side reference
40-120 ms      keep semantics, optimize state/queue
> 120 ms       optimize turn-aware search before next NavigationWorld layer
```

Both 10k turn-aware runs exceeded 200 ms p95, therefore the tree-backed implementation was rejected for performance while its turn-cost semantics remained accepted.

### Root cause / first optimization

The rejected implementation represented every expanded state `(RegionSlot, incoming PortalId)` through ordered maps and used a multimap frontier:

```text
std::map<TurnState, double>
std::map<TurnState, TurnPrev>
std::map<TurnState, bool>
std::multimap<...>
```

Optimization 1 kept identical turn semantics but changed private representation to:

```text
published graph build:
    each directed portal arrival -> stable TurnStateSlot
    adjacency edge -> arrivalTurnStateSlot

per query:
    vector<double> bestCost
    vector<TurnStateSlot> previous
    vector<uint8_t> settled
    binary priority_queue frontier
```

The special start state remains synthetic/no-heading and seeds outgoing arrival states directly. Equal-priority queue ordering remains deterministic by route cost and stable state identity.
