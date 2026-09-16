# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / static route quality  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-SPACE-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior domains publish only relevant subsets.

Legacy route-wide navigation remains migration code. `RuckigTrajectorySolver` is downstream local kinematics.

Accepted hybrid ownership:

```text
CPU: static free-space, portals, corridor search, precision local search
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, conflict reduction
```

Moving-goal pursuit is specified in `src/world/navigation/PURSUIT_HORIZON.md`; runtime pursuit remains later.

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k results remain recorded in the main navigation benchmark logs.

## `NAV-V2-SPACE-1` — accepted foundation

Static indexing progression:

```text
baseline corridor              ≈1.95-1.99 s at 10k
adjacency                      ≈21.8-22.4 ms
dense RegionSlot BFS          ≈7.9-8.0 ms
BVH point p95                 <=0.0353 ms
BVH bounded invalidation p95  <=0.0115 ms
```

Full replace/local patch remain worker/update-path operations at roughly 44-49 ms median at 10k.

## Costed route v1 — ACCEPTED

Target-machine behavior accepts apertures and policy-dependent canyon/overflight routes.

10k costed p95:

```text
open distance_only      8.1733 ms
open clearance_aware    8.5020 ms
hub  distance_only      7.7609 ms
hub  clearance_aware    9.6103 ms
```

The predeclared `<=15 ms` threshold passed, so the RegionSlot Dijkstra fast path is retained.

## Static turn cost v2 — ACCEPTED behavior

Fresh target-machine gate:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Policy and state model:

```text
turnPenaltyMetersPerRadian == 0
    -> accepted v1 RegionSlot Dijkstra

turnPenaltyMetersPerRadian > 0
    -> expanded (RegionSlot, incoming PortalId) search
```

`zigzag_vs_smooth` is accepted. Static turn cost remains a coarse branch-selection term; ship velocity, dynamic traffic, braking and speed-dependent maneuver feasibility remain outside persistent NavigationSpace cost.

## Active gate — turn-aware performance

Harness:

```text
benchmarks/navigation_space_turn/
```

It compares zero-turn and positive-turn policies on identical open/hub 1k/5k/10k snapshots. Timings and `portalsExamined` determine whether the current expanded `std::map`/`std::multimap` reference remains adequate or needs a private state/queue optimization.

After this measurement, if scaling is acceptable, stop adding static policy terms and move to dynamic conflict/local-horizon composition, then pursuit/receding-intercept consumption, then live game/server integration.
