# NavigationSpace turn-aware benchmark log

**Stage:** `NAV-V2-SPACE-1`  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine  
**Compiler:** GNU 15.2.0

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

The benchmark executable still built and ran successfully. This was a stale exact-text assertion in `check_navigation_space_turn_benchmark.py`, not a runtime/search failure; the contract now checks the active benchmark path + turn-aware task semantics instead of one brittle sentence.

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

The expansion work did not change, as intended: `turn_portals_examined` remained `329,660` at 10k. This isolates the remaining cost as expanded-state search work rather than ordered-map bookkeeping.

The pinned rule for the rerun was:

```text
<= 40 ms p95   accept worker-side reference
40-120 ms      reduce search expansion before moving on
> 120 ms       pathological; optimize immediately
```

Both 10k runs are now in the middle band. Turn semantics stay accepted; dense state/heap stays accepted as a private representation improvement, but turn-aware global search is not yet performance-accepted.

### Optimization 2 candidate — admissible A*

The active candidate retains the same dense `(RegionSlot, incoming PortalId)` state and binary heap but orders the turn-aware frontier by:

```text
f(n) = g(n) + h(n)

h(n) = distanceWeight * euclidean_distance(
    current_region_center,
    end_region_center
)
```

This heuristic is admissible and consistent for the current static cost because every geometric edge path is at least the straight-line center distance and clearance/turn penalties are non-negative. `distanceWeight == 0` naturally reduces the heuristic to zero.

The accepted zero-turn v1 path remains unchanged.

Status: **A* candidate pending target-machine architecture/behavior + identical turn benchmark rerun**.

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

All routes were found. `regions_visited` remained about the topology region count because diagnostics count unique regions, while the turn-aware solver expanded multiple arrival states per region. The decisive diagnostic is portal work: about `329,660` examined directed transitions at 10k versus about `57,197` for zero-turn v1.

The predeclared decision rule was:

```text
<= 40 ms p95   accept worker-side reference
40-120 ms      keep semantics, optimize state/queue
> 120 ms       optimize turn-aware search before next NavigationWorld layer
```

Both 10k turn-aware runs exceeded 200 ms p95, therefore the tree-backed implementation is rejected for performance while its turn-cost semantics remain accepted.

### Root cause / first optimization

The rejected implementation represented every expanded state `(RegionSlot, incoming PortalId)` through ordered maps and used a multimap frontier:

```text
std::map<TurnState, double>
std::map<TurnState, TurnPrev>
std::map<TurnState, bool>
std::multimap<...>
```

This compounded the unavoidable expanded-state work with repeated tree allocation / `O(log N)` lookup.

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

The special start state remains synthetic/no-heading and seeds outgoing arrival states directly. Equal-priority queue ordering remains deterministic by route priority/cost and stable state identity.
