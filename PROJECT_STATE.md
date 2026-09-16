# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / static turn-aware search optimization  
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

Accepted dynamic-map CPU/GPU evidence remains recorded in the navigation benchmark logs.

## `NAV-V2-SPACE-1` — accepted foundation

```text
baseline corridor              ≈1.95-1.99 s at 10k
adjacency                      ≈21.8-22.4 ms
dense RegionSlot BFS          ≈7.9-8.0 ms
BVH point p95                 <=0.0353 ms
BVH bounded invalidation p95  <=0.0115 ms
```

Full replace/local patch remain worker/update-path operations at roughly 44-49 ms median at 10k.

## Costed route v1 — ACCEPTED

Behavior accepts traversable apertures and policy-dependent canyon/overflight routing. Dedicated 10k p95 remains <=9.6103 ms, below the pinned 15 ms acceptance threshold.

## Static turn cost v2 — ACCEPTED semantics

Target-machine architecture + behavior tests pass. Positive turn penalty correctly preserves arrival direction in semantic state `(RegionSlot, incoming PortalId)` and `zigzag_vs_smooth` selects the smoother branch when turn burden dominates.

## Turn-aware performance progression

### Tree-backed expanded state — REJECTED

```text
open_10k turn-aware p95 216.1602 ms
hub_10k  turn-aware p95 232.6520 ms
turn portals examined   329,660
```

### Dense arrival-state + binary heap — IMPROVED, STILL ABOVE GATE

```text
open_10k zero-turn p95    8.6427 ms
open_10k turn-aware p95  67.9647 ms
hub_10k  zero-turn p95    8.5862 ms
hub_10k  turn-aware p95  72.6054 ms

zero-turn portals examined  57,197
turn-aware portals examined 329,660
```

Dense `TurnStateSlot` storage and vector-backed query state removed most ordered-tree overhead and cut turn-aware 10k p95 by roughly 3x. Expansion work remained unchanged, placing the candidate in the pinned `40-120 ms` band where search reduction is required before the next NavigationWorld layer.

The benchmark-contract failure in that run was a stale exact-text `CURRENT_TASK` assertion; it did not indicate a runtime/search failure and has been repaired.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

## Active optimization — admissible A*

Positive-turn search now keeps the accepted dense semantic state but orders the frontier by:

```text
f(n) = g(n) + h(n)
h(n) = distanceWeight * EuclideanDistance(currentRegionCenter, endRegionCenter)
```

This heuristic is admissible and consistent for the current static cost because:

- geometric edge cost is center -> portal -> neighbor center and cannot be shorter than direct center distance;
- clearance penalty is non-negative;
- turn penalty is non-negative;
- `distanceWeight == 0` yields `h == 0` and therefore dense-state Dijkstra ordering.

The accepted zero-turn RegionSlot Dijkstra path is untouched.

## Next order

1. rerun architecture/behavior with A* candidate;
2. rerun the identical `benchmarks/navigation_space_turn/` harness;
3. if 10k turn-aware p95 <=40 ms, accept static turn-aware worker/reference search and stop adding persistent static policy terms;
4. if still 40-120 ms, inspect remaining per-edge ordered-map lookups and/or adopt stronger/hierarchical global corridor reduction;
5. after static turn search closes, combine static corridor with dynamic conflict/local-horizon selection;
6. implement pursuit/receding-intercept consumer;
7. integrate NavigationWorld into live game/server, then retire legacy route-wide migration code only after v2 owns live navigation.

Do not add velocity, braking, dynamic traffic or pursuit prediction to persistent `NavigationSpace` static cost.
