# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Navigation:** `NAV-V2-SPACE-1` — static indexing + costed route quality accepted; turn-aware scaling active

## Accepted Navigation v2 ownership

Navigation v2 uses one shared ship-centered `NavigationWorld`.

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

`RuckigTrajectorySolver` remains downstream local kinematics, not path search. Moving-target pursuit remains documented in `src/world/navigation/PURSUIT_HORIZON.md` and is not yet the active runtime task.

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k evidence includes:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms
GPU cruise total p95=1.3226 ms
GPU hub    total p95=1.6258 ms
```

## `NAV-V2-SPACE-1` indexing — ACCEPTED

Accepted progression:

```text
baseline 10k full-portal corridor ≈1.95-1.99 s
per-region adjacency              ≈21.8-22.4 ms
dense RegionSlot BFS              ≈7.9-8.0 ms
BVH point p95                     <=0.0353 ms
BVH local invalidation p95        <=0.0115 ms
```

Full replace/local patch remain roughly 44-49 ms median at 10k and stay worker/update-side.

## Costed static corridor v1 — ACCEPTED

Behavior:

```text
wall aperture: fitting agent passes; oversized agent rejected
canyon vs overflight: distance/clearance policy chooses the expected branch
```

Accepted 10k p95:

```text
open distance_only      8.1733 ms
open clearance_aware    8.5020 ms
hub  distance_only      7.7609 ms
hub  clearance_aware    9.6103 ms
```

The predeclared `<=15 ms` gate passed, so RegionSlot Dijkstra v1 remains unchanged.

## Static turn cost v2 — ACCEPTED behavior

Fresh target-machine evidence:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Policy:

```text
turnPenaltyMetersPerRadian
```

Search invariant:

```text
turnPenalty == 0
    -> accepted v1 RegionSlot Dijkstra fast path

turnPenalty > 0
    -> expanded state (RegionSlot, incoming PortalId)
```

The expanded state is required because future turn cost depends on arrival direction. `zigzag_vs_smooth` is accepted: zero penalty chooses the shorter zig-zag; positive penalty chooses the smoother branch once saved turn burden exceeds the distance delta.

Static turn cost remains coarse route quality only. Initial heading/velocity, angular acceleration, speed-dependent turn radius, braking and dynamic traffic remain local/dynamic planner concerns.

## Active measurement — turn-aware scaling

New harness:

```text
benchmarks/navigation_space_turn/
```

It compares `turn=0` vs positive turn penalty on the same open/hub 1k/5k/10k topology and records med/p95 plus expansion diagnostics. The result decides whether the current expanded `std::map`/`std::multimap` reference is retained or privately optimized before dynamic/local-horizon integration.
