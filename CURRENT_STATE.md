# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-SPACE-1` — static-space reference accepted; first measured internal optimization is adjacency indexing

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

GPU 1k correctness matched the independent CPU reference. All measured GPU scenarios had `overflow=0`, `out_of_bounds=0`, `valid=1`, fixed 32-byte readback.

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Canonical block:

```text
src/world/navigation/space/
    NavigationSpace.h
    NavigationSpace.cpp
    CMakeLists.txt
    README.md
```

Public concepts:

```text
AgentEnvelope
RegionInput
PortalInput
StaticSpaceUpdate
LocalPatch
queryPoint
queryCorridor
invalidateBounds
stats
```

Fresh target-machine evidence:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

The first deterministic CPU reference uses free-space AABB regions + explicit portals internally. Public storage/search representation remains replaceable behind PImpl.

## Static-space baseline benchmark — ACCEPTED EVIDENCE

Benchmark contract passed. Default target-machine run:

```text
warmup=1
iterations=3
```

10k results:

```text
open_10k
    regions=10000 portals=28600
    replace med/p95    6.6942 / 7.2406 ms
    point med/p95      0.2787 / 0.2812 ms
    corridor med/p95   1991.0357 / 2008.2074 ms
    invalidate med/p95 2.5731 / 2.9869 ms
    patch med/p95      7.6135 / 7.8543 ms
    portals examined   285,913,245

hub_10k
    regions=10000 portals=28600
    replace med/p95    7.2397 / 7.5269 ms
    point med/p95      0.2784 / 0.2845 ms
    corridor med/p95   1951.6453 / 1956.0686 ms
    invalidate med/p95 2.8412 / 3.2230 ms
    patch med/p95      8.6678 / 8.7083 ms
    portals examined   285,913,245
```

From 1k to 10k, corridor median grew roughly `102-105x`; portal examinations grew roughly `106-108x`. The original BFS scanned the complete portal map for every visited region, making corridor traversal effectively pathological at scale.

Raw evidence: `benchmarks/navigation_space/RUN_LOG.md`.

## Optimization 1 — private adjacency index

The first measured optimization is implemented on `main` without changing `NavigationSpace.h`:

```text
regionId -> ordered portalId list
```

`replaceStaticWorld()` and `applyLocalPatch()` rebuild adjacency transactionally. Stable PortalId ordering preserves deterministic BFS tie-breaking. Invalidation only changes validity flags and does not rebuild topology.

`queryCorridor()` now examines only portals adjacent to the current region. The architecture contract explicitly rejects regression to a full portal-map scan inside BFS.

This optimization intentionally leaves point lookup, invalidation and whole-map transactional patch copying unchanged so the next bottleneck remains visible.

## Active gate

Target-machine rerun is pending after the adjacency change:

```bash
python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Do not add another optimization before this rerun.

## Likely next decisions after measurement

- corridor cheap, invalidation dominant -> add shared spatial index for point lookup + invalidation;
- patch dominant -> replace full transactional map copy/rebuild with bounded/chunked topology updates;
- corridor still material -> compact graph bookkeeping / costed graph representation;
- add hierarchy/bricks only if measured scale requires them.

Live `EliteGame` / `EliteServer` integration remains later. The raw `Shift+F12` NavigationWorld debug-view contract remains accepted but not yet implemented.
