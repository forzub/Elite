# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-SPACE-1` — static-space boundary accepted; adjacency optimization measured; dense RegionSlot graph bookkeeping is the active candidate

## Repository source of truth

`main` is the only canonical game-development branch. `REPOSITORY_SOURCE_OF_TRUTH.md` governs branch/recovery policy.

## Navigation v2 accepted architecture

The legacy route-wide synchronous chain remains migration code. `RuckigTrajectorySolver` is retained only as downstream local kinematics after navigation selects a safe temporary target.

Accepted hybrid split from `NAV-V2-MAP-2`:

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

GPU work remains asynchronous/double- or triple-buffered; no frame-thread `dispatch -> wait -> bulk readback` path is allowed.

## `NAV-V2-MAP-2` — CLOSED

Accepted 100-iteration 10k evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Canonical block:

```text
src/world/navigation/space/
```

Fresh target-machine behavior:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

The first deterministic CPU reference uses free-space AABB regions + explicit portals behind a backend-neutral PImpl API. Public representation remains replaceable.

## Static-space baseline — accepted evidence

Original 10k corridor behavior:

```text
open corridor median/p95 = 1991.0357 / 2008.2074 ms
hub  corridor median/p95 = 1951.6453 / 1956.0686 ms
portal examinations      = 285,913,245
```

Root cause: complete portal-map scan for every BFS region.

## Optimization 1 — per-region adjacency — ACCEPTED

Private connectivity:

```text
regionId -> ordered portalId list
```

Fresh target-machine rerun after adjacency:

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
```

Adjacency reduced 10k corridor median by roughly `87–91x` and portal examinations by roughly `5000x`. Publication/patch costs increased because connectivity is now built transactionally; those operations are not frame-path work.

`CorridorDiagnostics::regionsVisited` includes start/end point-location scans plus BFS visits; the ~20k diagnostic on a 10k topology does not mean 20k unique graph regions were traversed.

Raw evidence: `benchmarks/navigation_space/RUN_LOG.md`.

## Optimization 2 candidate — dense RegionSlot graph bookkeeping

Implemented on `main` without changing `NavigationSpace.h`:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS frontier, visited and predecessor state are vector-backed by RegionSlot rather than ordered maps keyed by RegionId. Stable RegionId/PortalId query output and deterministic PortalId traversal order are preserved.

Architecture contract rejects:

```text
full portal-map scan inside BFS
std::map<RegionId,...> visited/previous BFS bookkeeping
```

Point lookup, invalidation, transactional map copy and graph rebuild remain intentionally unchanged in this slice.

## Active gate

Rerun behavior + architecture + benchmark on the target machine:

```bash
python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Do not add another optimization before this measurement.

## Later order

After dense-slot measurement, choose the next internal improvement from evidence: remaining graph-state lookup, spatial point/invalidation index, or bounded topology patching. Weighted/costed corridor search and live runtime integration remain later. The raw `Shift+F12` NavigationWorld debug-view contract remains accepted but not yet implemented.
