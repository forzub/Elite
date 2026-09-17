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

## `NAV-V2-LOCAL-1` — BEHAVIOR ACCEPTED / PERFORMANCE PENDING

Fresh target-machine behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: 1/1 PASS
100% tests passed, 0 failed
Total Test time = 0.05 sec
```

The backend-neutral `LocalHorizonPlanner` ownership/safety reference is therefore accepted. It consumes an upstream nominal target plus compact `NavigationMap::QueryResult`, computes a latency/braking/turn/margin-bounded horizon, evaluates bounded dynamic conflicts, and returns `Clear`, `ConflictHold` or `StaleHold` with `PassThrough`, `Terminal` or `Hold` target semantics.

No second NavigationWorld, full actor-table scan, GLM/OpenGL/render/game dependency or unverified lateral bypass is allowed inside the block.

A dedicated candidate-count benchmark is now prepared at:

```text
benchmarks/navigation_local/
```

Pinned scales:

```text
clear:     0 / 16 / 64 / 256 / 1024 compact candidates
conflict:     16 / 64 / 256 / 1024 compact candidates
stale:                           1024 compact candidates
```

This benchmark measures only downstream `LocalHorizonPlanner::evaluate()` cost. It does not repeat the already accepted full-world NavigationMap broadphase benchmark. `1024` is a stress scale, not an expected normal candidate count.

Current status: **target-machine candidate scaling pending**. No avoidance algorithm beyond fail-closed hold is accepted yet.

## Next order

1. run/record `benchmarks/navigation_local/` candidate-count scaling;
2. choose and pin the first adjusted-target/lateral-avoidance algorithm from measured cost and game constraints;
3. add pursuit/receding-intercept as a consumer of the accepted local layer;
4. implement raw NavigationWorld debug visualization from the same completed snapshot;
5. integrate accepted NavigationWorld products into live game/server;
6. retire legacy route-wide navigation only after v2 owns the live path.

Do not add velocity, braking, dynamic traffic or pursuit prediction to persistent `NavigationSpace` static cost.
