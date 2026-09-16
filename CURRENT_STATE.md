# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-SPACE-1` — static indexing + distance/clearance costed routing accepted; turn-aware static cost candidate pending target-machine gate

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

Accepted graph/index progression:

```text
baseline full portal scan
    10k corridor ≈1.95-1.99 s

per-region adjacency
    10k corridor ≈21.8-22.4 ms

private dense RegionSlot graph
    open_10k corridor median/p95=7.9396/7.9735 ms
    hub_10k  corridor median/p95=7.9981/8.4877 ms

private RegionSlot AABB BVH + incident portal index
    open_10k point p95=0.0353 ms
    hub_10k  point p95=0.0099 ms
    open_10k invalidation p95=0.0112 ms
    hub_10k  invalidation p95=0.0115 ms
```

Full replace/local patch are roughly 44-49 ms median at 10k because graph + BVH are rebuilt transactionally. Those remain worker/update paths, not frame-path operations.

## Costed static corridor v1 — ACCEPTED

Fast `queryCorridor()` remains the deterministic topology/BFS oracle.

`queryCostedCorridor()` v1 adds:

```text
distanceWeight
preferredClearanceMultiple
clearancePenaltyMeters
```

Behavior is target-machine accepted:

```text
wall_with_aperture
    fitting agent -> through opening
    oversized agent -> rejected

canyon_vs_overflight
    distance-only -> short canyon
    clearance-aware -> longer open route
    oversized canyon agent -> open route
```

Dedicated costed-scaling benchmark is also accepted. Target-machine 10k p95:

```text
open_10k distance_only     8.1733 ms
open_10k clearance_aware   8.5020 ms
hub_10k  distance_only     7.7609 ms
hub_10k  clearance_aware   9.6103 ms
```

The predeclared acceptance threshold was `<=15 ms p95`; therefore deterministic Dijkstra v1 is accepted as an asynchronous worker/reference solve without A*/heap optimization before the next semantic term.

Raw evidence:

```text
benchmarks/navigation_space/RUN_LOG.md
benchmarks/navigation_space_costed/RUN_LOG.md
```

`totalCostMetersEquivalent` remains a coarse branch-comparison metric, not exact physical trajectory length.

## Active candidate — static turn cost v2

Design contract:

```text
src/world/navigation/STATIC_TURN_COST.md
```

`CorridorCostPolicy` now includes:

```text
turnPenaltyMetersPerRadian
```

Critical invariant:

```text
turnPenalty == 0
    -> accepted v1 RegionSlot Dijkstra fast path

turnPenalty > 0
    -> expanded state (RegionSlot, incoming PortalId)
```

The expanded state is required because the future turn cost depends on arrival direction. Two arrivals at the same region through different portals cannot be collapsed into one state.

Static v2 turn term:

```text
turn cost = turnPenaltyMetersPerRadian * turn_angle_radians
```

Turn angle is measured at the current coarse region center between incoming-portal direction and outgoing-portal direction. The special start state has no static turn penalty because `CorridorQuery` does not carry initial heading/velocity.

Pinned new fixture:

```text
zigzag_vs_smooth
    turnPenalty=0 -> slightly shorter zig-zag
    turnPenalty>0 -> smoother branch when saved turn cost exceeds distance delta
```

Implementation + fixture + architecture markers are on `main`; target-machine MinGW64 behavior gate is pending.
