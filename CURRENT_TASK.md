# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — dense graph accepted; private spatial-index candidate pending target-machine rerun

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Accepted prior gates

`NAV-V2-MAP-2` is closed with measured hybrid ownership:

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

`NAV-V2-SPACE-1` boundary/reference behavior is accepted:

```text
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Public `NavigationSpace.h` remains backend-neutral/PImpl.

## Static-space scaling decisions already accepted

### Baseline

```text
10k corridor ~1.95-1.99 s
portal examinations=285,913,245
```

### Optimization 1 — per-region adjacency

```text
10k corridor ~21.8-22.4 ms
portal examinations=57,189
```

### Optimization 2 — dense RegionSlot graph

```text
open_10k corridor median/p95 = 7.9396 / 7.9735 ms
hub_10k  corridor median/p95 = 7.9981 / 8.4877 ms
```

The dense-slot implementation passed the C++ behavior test and benchmark. The architecture test's reported failure was a test-string defect (`std::vector<RegionSlot>` vs actual `std::vector<Impl::RegionSlot>`); that marker is fixed on `main`.

A roughly 8 ms 10k coarse unweighted global corridor is acceptable for the asynchronous worker/reference path. Stop micro-optimizing BFS for now.

## Accepted moving-goal design

The user's pursuit/fleeing case is documented in:

```text
src/world/navigation/PURSUIT_HORIZON.md
```

Pursuit uses a receding moving-goal/intercept horizon:

```text
target observed/shared P/V/A
        |
short bounded prediction
        |
static-space feasibility / dynamic conflicts
        |
predicted intercept region/state
        |
reuse corridor while branch remains valid
        |
local target -> Ruckig -> execute near segment -> repeat
```

Do not rebuild the complete global corridor every frame. Do not give hostile pursuers private target-route intent unless gameplay policy explicitly allows it.

Implementation of pursuit is later than the current static-space gate.

## Optimization 3 candidate — private spatial index

Current measured ordinary/static costs before this candidate:

```text
open_10k point p95=0.2814 ms, invalidate median/p95=3.6275/3.7643 ms
hub_10k  point p95=0.7043 ms, invalidate median/p95=3.2642/3.3515 ms
```

The private implementation now builds an AABB BVH over dense `RegionSlot` identity.

It is used by:

```text
queryPoint()
queryCorridor() start/end region localization
invalidateBounds()
```

`invalidateBounds()` also uses endpoint incidence:

```text
RegionSlot -> PortalId[] touching the region
```

so local invalidation does not scan the entire portal map.

Exact containment/intersection/clearance checks remain authoritative. Public API is unchanged.

Full replacement and local patch rebuild graph + BVH transactionally; those are update/worker-path operations and are allowed to cost more in exchange for cheaper ordinary queries.

## RUN NOW

Because `NavigationSpace.cpp` and the architecture contract changed, rerun the full static-space gate and benchmark:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Send the complete output.

## Decision after spatial-index measurement

Do not add another internal optimization before seeing the numbers.

Expected decision rule:

- point/invalidation collapse to sub-millisecond: accept spatial query side and move next to weighted/costed corridor semantics + aperture/canyon acceptance cases;
- patch/publication dominate but ordinary queries are cheap: leave them worker-side until bounded update ownership becomes necessary;
- spatial build/query unexpectedly regresses: fix/reject the BVH before proceeding;
- only add more hierarchy/bricks if measured evidence requires it.

Do not yet wire NavigationSpace into live runtime, delete migration navigation code, or implement pursuit runtime behavior.
