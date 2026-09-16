# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-SPACE-1` — static-space indexing and costed route semantics accepted; costed-corridor scaling benchmark active

## Repository source of truth

`main` is the only canonical game-development branch. `REPOSITORY_SOURCE_OF_TRUTH.md` governs branch/recovery policy.

## Navigation v2 accepted architecture

Navigation v2 uses one shared ship-centered `NavigationWorld`. The legacy whole-route synchronous chain remains migration code. `RuckigTrajectorySolver` is downstream local kinematics after navigation selects a safe temporary target.

Accepted hybrid ownership:

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

Moving-target pursuit is documented in `src/world/navigation/PURSUIT_HORIZON.md`: receding predicted intercept state/region, bounded prediction, corridor reuse while the branch remains valid, no complete global replan every frame.

## `NAV-V2-MAP-2` — CLOSED

Accepted 100-iteration 10k evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

## `NAV-V2-SPACE-1` static boundary/indexing — ACCEPTED

Canonical block: `src/world/navigation/space/`.

Public `NavigationSpace` remains backend-neutral/PImpl. The deterministic CPU reference uses free-space AABB regions + explicit portals internally; storage/index representation remains replaceable.

Accepted graph/index progression:

```text
baseline full portal scan
    10k corridor ≈1.95-1.99 s
    portal examinations=285,913,245

Optimization 1: per-region adjacency
    10k corridor ≈21.8-22.4 ms
    portal examinations=57,189

Optimization 2: dense RegionSlot graph
    open_10k corridor median/p95=7.9396/7.9735 ms
    hub_10k  corridor median/p95=7.9981/8.4877 ms

Optimization 3: RegionSlot AABB BVH + incident portal index
    open_10k point median/p95=0.0065/0.0353 ms
    hub_10k  point median/p95=0.0064/0.0099 ms
    open_10k invalidate median/p95=0.0102/0.0112 ms
    hub_10k  invalidate median/p95=0.0109/0.0115 ms
```

The BVH reduced 10k point candidate examination to 5 regions. Static point lookup and bounded invalidation are comfortably sub-millisecond.

Full replace/local patch cost roughly 44-49 ms median at 10k because graph + BVH are rebuilt transactionally. Those paths remain worker/update operations; bounded/chunked update ownership is deferred until live mutation frequency requires it.

Raw scaling evidence: `benchmarks/navigation_space/RUN_LOG.md`.

## Costed corridor semantics — ACCEPTED

Fast `queryCorridor()` remains the deterministic topology/BFS oracle.

`queryCostedCorridor()` adds explicit static policy:

```text
CorridorCostPolicy
    distanceWeight
    preferredClearanceMultiple
    clearancePenaltyMeters
```

Static cost v1:

```text
edge cost = distanceWeight * coarse geometric distance
          + static clearance penalty
```

Target-machine acceptance on MinGW64:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Pinned semantic cases are therefore accepted:

```text
wall_with_aperture
    fitting agent -> through opening
    oversized agent -> rejected

canyon_vs_overflight
    distance-only policy -> short canyon
    clearance-aware policy -> longer open route
    oversized canyon agent -> open route
```

This explicitly permits NPC routes through holes, tunnels, station apertures and canyons when free-space topology and agent clearance admit them. The enclosing obstacle is not treated as one indivisible keep-out volume.

`totalCostMetersEquivalent` remains a coarse region/portal comparison metric, not an exact physical trajectory-length claim. Turn/curvature, dynamic traffic/risk and pursuit prediction are later layers.

## Active measurement — costed corridor scaling

A dedicated isolated harness now exists:

```text
benchmarks/navigation_space_costed/
```

It measures `queryCostedCorridor()` on open/hub 1k/5k/10k topologies under two policies on the same published snapshot:

```text
distance_only
clearance_aware
```

Metrics include median/p95, regions visited, portals examined, path length and reported coarse cost.

The result decides whether the current deterministic Dijkstra-style reference is retained as-is or whether priority-queue/A* optimization is required before adding turn/curvature cost.
