# NavigationSpace turn-aware benchmark log

**Stage:** `NAV-V2-SPACE-1`  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine  
**Compiler:** GNU 15.2.0

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

This compounds the unavoidable expanded-state work with repeated tree allocation / `O(log N)` lookup.

Optimization candidate now on `main` keeps identical turn semantics but changes private representation to:

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

The special start state remains synthetic/no-heading and seeds outgoing arrival states directly. Equal-cost queue ordering remains deterministic by `(cost, RegionSlot, incoming PortalId, TurnStateSlot)`.

Status: **candidate pending target-machine architecture/behavior + turn benchmark rerun**. No optimized timing claim is made yet.
