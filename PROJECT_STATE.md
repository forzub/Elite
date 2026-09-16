# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / static route quality  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-SPACE-1`

## Repository state

`main` is the only canonical game-development branch. Branch governance is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not resume feature work on historical parallel refs.

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior spaces remain local domains and publish only relevant subsets.

The old route-wide chain is migration code only. `RuckigTrajectorySolver` remains downstream local kinematics, not path-search authority.

Accepted hybrid ownership:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local planning

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

## Moving-goal / pursuit contract

`src/world/navigation/PURSUIT_HORIZON.md` defines receding moving-goal pursuit. A pursuer targets a bounded predicted intercept region/state using available target P/V/A and known feasibility, reusing a still-valid corridor branch instead of rebuilding a complete global route every frame.

Runtime pursuit implementation remains later than the current static-space gate.

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k long-run evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

## `NAV-V2-SPACE-1` static foundation — ACCEPTED

Canonical block: `src/world/navigation/space/`.

Accepted static scaling progression:

```text
baseline 10k corridor           ≈1.95-1.99 s
per-region adjacency            ≈21.8-22.4 ms
dense RegionSlot BFS            ≈7.9-8.0 ms
BVH point p95                   <=0.0353 ms at 10k
BVH local invalidation p95      <=0.0115 ms at 10k
```

Full replace/local patch are roughly 44-49 ms median at 10k because graph + BVH are rebuilt transactionally. They remain worker/update-path operations.

## Policy-aware static corridor v1 — ACCEPTED

Behavioral target-machine acceptance:

```text
wall_with_aperture
    fitting craft -> through opening
    oversized craft -> rejected

canyon_vs_overflight
    distance-only -> short canyon
    clearance-aware -> longer open branch
    oversized canyon craft -> open branch
```

Dedicated costed-scaling target-machine p95 at 10k:

```text
open distance_only      8.1733 ms
open clearance_aware    8.5020 ms
hub  distance_only      7.7609 ms
hub  clearance_aware    9.6103 ms
```

The pinned rule was `<=15 ms p95`, therefore deterministic Dijkstra v1 is accepted as an asynchronous worker/reference solve. No A*/heap redesign is required before the next static route-quality term.

Raw evidence:

```text
benchmarks/navigation_space/RUN_LOG.md
benchmarks/navigation_space_costed/RUN_LOG.md
```

## Active candidate — static turn cost v2

Design authority:

```text
src/world/navigation/STATIC_TURN_COST.md
```

`CorridorCostPolicy` gains:

```text
turnPenaltyMetersPerRadian
```

Search ownership:

```text
turnPenalty == 0
    accepted v1 RegionSlot Dijkstra fast path

turnPenalty > 0
    expanded state (RegionSlot, incoming PortalId)
```

The expanded state is required because future turn cost depends on arrival direction. Equal region with different incoming portals is not the same turn-aware search state.

Static turn cost is measured at the current region center from incoming portal direction to outgoing portal direction. No initial-heading penalty is invented because `CorridorQuery` does not own current ship velocity/attitude.

Pinned fixture:

```text
zigzag_vs_smooth
    zero turn penalty -> shorter zig-zag
    positive turn penalty -> smoother branch when saved turn burden exceeds distance delta
```

Implementation, behavior fixture and architecture gate are on `main`; MinGW64 target-machine validation is pending.

## Next order

1. pass static turn-cost architecture + behavior gate;
2. benchmark expanded turn-aware state separately;
3. stop extending persistent static cost if turn-aware scaling is acceptable;
4. combine static branch with dynamic conflicts and local horizon;
5. implement pursuit/receding-intercept consumer;
6. integrate NavigationWorld into live game/server;
7. retire legacy route-wide migration code only after v2 owns live navigation.

## Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize current state/task, project state, affected contracts and private project-context evidence before handoff.
