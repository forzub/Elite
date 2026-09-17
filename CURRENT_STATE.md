# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** `NAV-V2-SPACE-1` — **CLOSED / ACCEPTED**  
**Active stage:** `NAV-V2-LOCAL-1` — behavior accepted, candidate-count performance measurement pending

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

`RuckigTrajectorySolver` remains downstream local kinematics, not path search. Moving-target pursuit remains specified in `src/world/navigation/PURSUIT_HORIZON.md` and is a later consumer of the local-horizon layer.

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k evidence includes CPU candidate queries below 0.2 ms p95 and GPU dynamic total below 1.7 ms p95 on the target machine.

## `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Static-space indexing and query foundation:

```text
baseline 10k full-portal corridor ≈1.95-1.99 s
per-region adjacency              ≈21.8-22.4 ms
dense RegionSlot BFS              ≈7.9-8.0 ms
BVH point p95                     <=0.0353 ms
BVH local invalidation p95        <=0.0115 ms
```

Costed static corridor v1 is accepted at <=9.6103 ms p95 against the pinned <=15 ms gate. Apertures, tunnels/canyons, agent-envelope admission and fail-closed local invalidation are pinned by tests.

Static turn semantics are also accepted:

```text
turnPenalty == 0
    -> RegionSlot Dijkstra fast path

turnPenalty > 0
    -> state = (RegionSlot, incoming PortalId)
```

### Turn-aware performance history

```text
tree-backed expanded state
    open_10k 216.1602 ms
    hub_10k  232.6520 ms

Dense TurnStateSlot + vector state + binary heap
    open_10k  67.9647 ms
    hub_10k   72.6054 ms

Euclidean A* — REJECTED
    open_10k  97.0537 ms
    hub_10k   97.6909 ms
```

Euclidean A* reduced transition work only ~1.75% and increased p95 materially, so it remains rejected.

### Accepted hot-path publication result

Target-machine rerun on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
NAVIGATION SPACE TURN BENCHMARK CONTRACT: PASS

scenario   zero p95 ms   turn p95 ms   zero portals   turn portals
open_10k       8.4498       12.0072         57,197       329,660
hub_10k        8.4125       11.9065         57,197       329,660
```

All routes remained found with identical accepted path length, turn burden and cost. The pinned turn-aware acceptance gate was `<=40 ms p95`; both 10k cases pass with large margin.

The decisive optimization was publication of immutable static hot data: dense region/portal state, per-edge geometry/clearance and precomputed turn angles. Expansion work stayed at `329,660`, proving that the practical bottleneck was cost per examined transition rather than the accepted expanded-state semantics.

Do not continue optimizing persistent static turn search without new runtime evidence. Do not add ship velocity, braking, traffic or pursuit prediction to `NavigationSpace` static cost.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

## `NAV-V2-LOCAL-1` — BEHAVIOR ACCEPTED / PERFORMANCE PENDING

Fresh target-machine gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: 1/1 PASS
100% tests passed, 0 failed
Total Test time = 0.05 sec
```

Accepted local ownership/safety behavior:

```text
accepted static corridor / nominal local target
        +
NavigationMap compact dynamic candidates
        +
agent P/V/A + result age / safety budget
        |
        v
bounded local conflict assessment
        |
        v
receding-horizon temporary target state
        |
        +-> Clear / PassThrough
        +-> Clear / Terminal
        +-> ConflictHold
        +-> StaleHold
```

The local layer consumes the accepted `NavigationSpace` and `NavigationMap` boundaries rather than creating another spatial world or full-scene planner. It remains independent of GLM/OpenGL/render/game state. The old `TacticalCollisionMonitor`, `SmallCraftNavigation`, route-wide `GeometricPathPlanner` and dense trajectory/guidance chain remain migration/reference code, not v2 authority.

No lateral bypass is accepted yet. `ConflictHold` and `StaleHold` fail closed until an adjusted-target algorithm is separately measured and pinned.

### Active measurement candidate

`benchmarks/navigation_local/` now measures only `LocalHorizonPlanner::evaluate()` over already reduced compact candidates:

```text
clear:     0 / 16 / 64 / 256 / 1024
conflict:     16 / 64 / 256 / 1024
stale:                           1024
```

The benchmark records median/p95 microseconds, p95 ns/candidate, examined candidates, conflicts and final status. Scenario construction is outside the timed region. `stale_1024` must examine zero candidates.

Status: **pending target-machine candidate-count scaling run**. No local performance acceptance or avoidance-algorithm choice is claimed yet.
