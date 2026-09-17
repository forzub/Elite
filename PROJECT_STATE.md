# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / local dynamic avoidance  
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

Static turn search is closed; do not spend more work there without new runtime evidence.

## `NAV-V2-LOCAL-1` — REFERENCE ACCEPTED

Target-machine behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39` accepted the backend-neutral `LocalHorizonPlanner` boundary.

The planner consumes an upstream nominal target plus compact `NavigationMap::QueryResult`, computes a latency/braking/turn/margin-bounded horizon, evaluates bounded dynamic conflicts, and returns `Clear`, `ConflictHold` or `StaleHold` with `PassThrough`, `Terminal` or `Hold` target semantics.

No second NavigationWorld, full actor-table scan, GLM/OpenGL/render/game dependency or unverified free-space assumption is allowed.

### Compact-candidate scaling — ACCEPTED

Target-machine benchmark on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
scenario         p95_us     p95_ns/candidate
clear_16          0.4814          30.0873
clear_64          1.8327          28.6362
clear_256         7.9820          31.1798
clear_1024       36.9641          36.0977

conflict_16       0.5073          31.7062
conflict_64       1.9333          30.2078
conflict_256      7.5441          29.4693
conflict_1024    31.8656          31.1188

stale_1024        0.0359          0 candidates examined
```

The one-pass dynamic reference is therefore not a CPU bottleneck. `1024` candidates is already a stress scale and still remains below `0.037 ms p95`.

## Active local slice — conservative adjusted target

`LocalAvoidancePlanner` is the first measured lateral-avoidance candidate. It composes the accepted local horizon reference with the accepted `NavigationSpace` public point-query boundary.

For nominal `ConflictHold`, it tries at most sixteen 3D target probes:

```text
15 degree deflection x 8 azimuths
30 degree deflection x 8 azimuths
```

Each candidate must pass both:

1. **static proof:** current agent point and candidate target are envelope-safe and resolve to the same `NavigationSpace` region;
2. **dynamic proof:** the candidate is rechecked through `LocalHorizonPlanner` against the same compact dynamic snapshot.

The same-region rule is intentionally conservative. A semantic free-space region is a convex AABB, so the complete straight segment between two envelope-safe points in that same region remains statically contained. Portal-crossing avoidance is not yet claimed.

The accepted closest-approach reference uses current ship P/V/A. Therefore the first lateral slice may clear a future swept-corridor blocker but must not claim that a target change alone erased an already predicted head-on/crossing collision. Those cases remain fail-closed `ConflictHold` until a trajectory-aware maneuver is separately demonstrated.

Current avoidance candidate status: **pending target-machine architecture/compile/behavior gate**. Avoidance probe performance is also pending and will be measured only after behavior passes.

## Next order

1. run the local avoidance architecture + behavior gate;
2. benchmark multiplied avoidance probe cost for nominal-clear, early-adjust, static-reject and dynamic-reject paths;
3. if accepted, design the trajectory-aware head-on/crossing maneuver layer;
4. add pursuit/receding-intercept as a consumer of the accepted local layer;
5. implement raw NavigationWorld debug visualization from the same completed snapshot;
6. integrate accepted NavigationWorld products into live game/server;
7. retire legacy route-wide navigation only after v2 owns the live path.

Do not add velocity, braking, dynamic traffic or pursuit prediction to persistent `NavigationSpace` static cost.
