# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / dynamic conflict + local receding horizon  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-LOCAL-1`

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

Dynamic-map CPU/GPU boundary and hybrid ownership are accepted. `NavigationMap` returns compact dynamic candidates by value and hides backend/cell/GPU state.

## `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Accepted static foundation:

```text
baseline corridor              ≈1.95-1.99 s at 10k
adjacency                      ≈21.8-22.4 ms
dense RegionSlot BFS          ≈7.9-8.0 ms
BVH point p95                 <=0.0353 ms
BVH bounded invalidation p95  <=0.0115 ms
costed corridor v1 p95        <=9.6103 ms
```

Turn-aware semantics preserve `(RegionSlot, incoming PortalId)` for positive static turn penalty. Tree-backed state and weak Euclidean A* were rejected from target-machine evidence.

Final accepted published-hot-data implementation, target commit `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero-turn p95    8.4498 ms
open_10k turn-aware p95  12.0072 ms
hub_10k  zero-turn p95    8.4125 ms
hub_10k  turn-aware p95  11.9065 ms

zero portals examined     57,197
turn portals examined    329,660
```

Architecture/behavior/benchmark contracts all passed, all routes were found and accepted route costs were unchanged. The predeclared turn gate was `<=40 ms p95`; both 10k scenarios pass with large margin.

The transition count stayed unchanged while p95 fell from `67.9647/72.6054 ms` to `12.0072/11.9065 ms`. Therefore immutable per-transition work, not the semantic expanded-state count, was the practical bottleneck for this reference topology. Static turn search is closed; do not spend more work there without new runtime evidence.

## `NAV-V2-LOCAL-1` — ACTIVE

The next layer composes existing accepted boundaries:

```text
static corridor / nominal local target
        +
NavigationMap dynamic candidates
        +
agent state + bounded result age
        |
        v
local conflict assessment
        |
        v
receding-horizon temporary safe target
        |
        v
local kinematics / flight control
```

The new local boundary must remain backend-neutral and compact. It does not own a second NavigationWorld and does not scan full actor storage. It consumes `NavigationMap::Candidate` products and accepted static intent.

Existing `TacticalCollisionMonitor` and `SmallCraftNavigation` are pre-v2 GLM/old-state implementations. Their closest-approach/steering ideas may be used as reference, but they are not architectural dependencies of the new block.

## Next order

1. implement/test the `NAV-V2-LOCAL-1` backend-neutral local-horizon reference boundary;
2. measure candidate-count scaling on the target machine;
3. add pursuit/receding-intercept as a consumer of the accepted local layer;
4. implement raw NavigationWorld debug visualization from the same completed snapshot;
5. integrate accepted NavigationWorld products into live game/server;
6. retire legacy route-wide navigation only after v2 owns the live path.

Do not add velocity, braking, dynamic traffic or pursuit prediction to persistent `NavigationSpace` static cost.
