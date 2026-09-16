# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — static turn-cost behavior accepted; turn-aware performance benchmark active

## Accepted foundation

Static indexing, costed corridor v1, and static turn-cost v2 behavior are accepted on the target MinGW64 machine.

Key accepted evidence:

```text
10k coarse BFS corridor          ≈7.3-8.2 ms
10k BVH point p95                <=0.0353 ms
10k local invalidation p95       <=0.0115 ms

costed v1 10k p95
    open distance_only           8.1733 ms
    open clearance_aware         8.5020 ms
    hub  distance_only           7.7609 ms
    hub  clearance_aware         9.6103 ms
```

Costed v1 therefore remains the accepted RegionSlot Dijkstra fast path.

Fresh static turn-cost target-machine gate:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Accepted turn semantics:

```text
turnPenaltyMetersPerRadian == 0
    -> accepted v1 RegionSlot Dijkstra

turnPenaltyMetersPerRadian > 0
    -> state = (RegionSlot, incoming PortalId)

zigzag_vs_smooth
    zero penalty     -> shorter zig-zag
    positive penalty -> smoother branch when turn saving beats distance delta
```

Aperture and canyon/overflight fixtures remain green.

## Active gate — turn-aware performance

New isolated harness:

```text
benchmarks/navigation_space_turn/
```

It measures the same published topology twice:

```text
zero_turn
    turnPenaltyMetersPerRadian = 0
    accepted v1 fast path

turn_aware
    turnPenaltyMetersPerRadian > 0
    expanded (RegionSlot, incoming PortalId) search
```

Pinned scales:

```text
open_1k / open_5k / open_10k
hub_1k  / hub_5k  / hub_10k
```

Metrics:

```text
median/p95 ms
unique regions visited
portals examined
region-path length
coarse accumulated path turn radians
coarse meter-equivalent cost
```

`portalsExamined` is the primary expansion-work diagnostic because turn-aware search may settle multiple incoming-portal states for the same region.

## RUN NOW

Only the new benchmark contract + benchmark are required:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_turn_benchmark.py
bash benchmarks/navigation_space_turn/run_mingw64.sh
```

Default run is intentionally bounded:

```text
warmup=1
iterations=3
```

Send the complete output.

## Decision after measurement

At 10k scale:

- `turn-aware p95 <= 40 ms`: accept the expanded-state reference as worker-side route-quality search and stop adding static policy terms;
- `40-120 ms`: semantics remain accepted, but optimize private turn-state/queue representation before frequent many-NPC replans;
- `>120 ms` or clearly pathological expansion growth: optimize turn-aware search immediately before moving to dynamic/local-horizon integration.

Do not require turn-aware global search every frame. Dynamic actors, speed-dependent turn radius, braking, pursuit prediction and collision horizon remain outside persistent `NavigationSpace` static cost.
