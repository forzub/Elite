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

`src/world/navigation/PURSUIT_HORIZON.md` defines receding moving-goal pursuit. A pursuer targets a bounded predicted intercept region/state using available target P/V/A and known feasibility, reusing the current corridor while its branch remains valid instead of rebuilding a complete global route every frame.

Runtime pursuit implementation is later than the current static-space gate.

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k long-run evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

## `NAV-V2-SPACE-1` static-space foundation — ACCEPTED

Canonical block: `src/world/navigation/space/`.

The public API is backend-neutral/PImpl. Internal reference representation is free-space AABB regions + explicit portals with private dense graph and BVH acceleration.

Accepted static scaling progression:

```text
baseline 10k corridor           ≈1.95-1.99 s
per-region adjacency            ≈21.8-22.4 ms
dense RegionSlot BFS            ≈7.9-8.0 ms
BVH point p95                   <=0.0353 ms at 10k
BVH local invalidation p95      <=0.0115 ms at 10k
```

Optimization 3 target-machine result at 10k:

```text
open_10k
    point med/p95      0.0065 / 0.0353 ms
    corridor med/p95   7.3479 / 7.7085 ms
    invalidate med/p95 0.0102 / 0.0112 ms
    replace med        44.2686 ms
    patch med          46.6777 ms

hub_10k
    point med/p95      0.0064 / 0.0099 ms
    corridor med/p95   8.0958 / 8.2105 ms
    invalidate med/p95 0.0109 / 0.0115 ms
    replace med        45.2447 ms
    patch med          48.7458 ms
```

Point candidate examination fell from 10,000 to 5 in the 10k benchmark. Ordinary static queries/invalidation are accepted. Full replace/local patch rebuild graph + BVH and remain worker/update-path operations; they are not suitable for per-frame mutation.

Raw history: `benchmarks/navigation_space/RUN_LOG.md`.

## Active candidate — policy-aware static corridor

The fast `queryCorridor()` remains the deterministic topology/BFS oracle.

A separate `queryCostedCorridor()` candidate adds explicit static route policy:

```text
CorridorCostPolicy
    distanceWeight
    preferredClearanceMultiple
    clearancePenaltyMeters
```

Cost v1 is geometric distance plus a meter-equivalent penalty when traversable clearance is below the requested preferred multiple. Physical fit remains a hard constraint.

Pinned acceptance fixtures:

```text
wall_with_aperture
    fitting craft -> through opening
    oversized craft -> opening rejected

canyon_vs_overflight
    distance-only -> short canyon
    clearance-aware -> longer open branch
    oversized canyon craft -> open branch
```

This candidate explicitly supports navigation through holes, tunnels, apertures and canyons instead of treating the surrounding geometry as a single coarse keep-out obstacle.

Status: implementation + tests are on `main`; target-machine architecture/behavior gate is pending.

## Next order

1. pass the costed-corridor MinGW64 architecture/behavior gate;
2. benchmark costed search separately before broad 10k use;
3. add turn/curvature policy only after static distance/clearance semantics are accepted;
4. combine static route with dynamic conflicts/local horizon;
5. implement pursuit consumer on top of that accepted route/local-target machinery;
6. integrate NavigationWorld into live game/server;
7. retire legacy route-wide migration code only after v2 owns live navigation.

## Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize current state/task, project state, affected contracts and private project-context evidence before handoff.
