# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` — dynamic conflict + local receding horizon

## Closed gate — `NAV-V2-SPACE-1`

Static indexing, costed corridor v1 and static turn-cost semantics/performance are accepted.

Final target-machine turn rerun on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
NAVIGATION SPACE TURN BENCHMARK CONTRACT: PASS

open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k  zero p95    8.4125 ms
hub_10k  turn p95   11.9065 ms
turn portals examined 329,660
```

Pinned acceptance gate was `<=40 ms p95`; static turn-aware search therefore passes and `NAV-V2-SPACE-1` is closed.

Historical rejected paths remain recorded in `benchmarks/navigation_space_turn/RUN_LOG.md`. In particular, do not restore the weak Euclidean A* candidate without new evidence.

## Active problem

Compose already accepted shared-world products instead of producing a route-wide dense trajectory:

```text
accepted static corridor / nominal local target
        +
NavigationMap compact candidates
        +
agent state + bounded latency/safety horizon
        |
        v
local conflict assessment
        |
        v
temporary safe target state
        |
        v
RuckigTrajectorySolver / flight control
```

The existing `NavigationMap::Candidate` already carries compact map-space dynamic state:

```text
entityId
positionMapMeters
velocityMapMetersPerSecond
accelerationMapMetersPerSecond2
predictedEndPositionMapMeters
conservativeSweptCenterMapMeters
actorRadiusMeters
conservativeSweptRadiusMeters
flags / motionRevision
```

Do not add a redundant planner API to `NavigationMap` and do not expose its internal cells/GPU state.

## Legacy inventory

Existing pre-v2 components include `TacticalCollisionMonitor`, `SmallCraftNavigation`, `GeometricPathPlanner`, `TrajectoryGenerator` and `GuidanceTunnel`.

`TacticalCollisionMonitor` contains useful closest-approach reference math, but its public surface depends on GLM and old `NavigationContact/NavigationScene` state. It is migration/reference code, not the new boundary.

`SmallCraftNavigation` is likewise local GLM steering for old waypoint state. Do not wire Navigation v2 through it.

## First `NAV-V2-LOCAL-1` slice

Build one backend-neutral local-horizon consumer under `src/world/navigation/` (separate from `map/` and `space/`) with these constraints:

- consumes compact dynamic candidates, never full actor/cell storage;
- works in ship-centered map coordinates and does not include GLM/OpenGL/game/render headers;
- deterministic CPU reference first;
- bounded horizon only; no route-wide dense sampling;
- relative-motion closest approach uses current P/V and conservative swept bounds;
- output is a compact temporary target/safety product, not a trajectory;
- fail closed when no safe local target is demonstrated;
- no synchronous GPU readback or second NavigationWorld snapshot;
- preserve revision/generation information needed to reason about stale results;
- pursuit remains a later consumer and is not baked into generic conflict logic.

Use the accepted static corridor/nominal target as intent. Dynamic traffic must not become a persistent `NavigationSpace` cost term.

## Acceptance work for this slice

1. define the backend-neutral local-horizon API and ownership contract;
2. add deterministic fixtures for clear corridor, crossing actor, head-on actor and stale/bounded horizon behavior;
3. add architecture boundary test forbidding GLM/OpenGL/game/render dependencies and full-scene ownership;
4. build/run isolated MinGW64 behavioral tests;
5. only after behavior is pinned, add a small benchmark for candidate-count scaling and target-machine timing;
6. then decide the next precision/steering algorithm from measurements rather than extending legacy route-wide planning.

Do not begin live `EliteGame` / `EliteServer` integration until this local composition boundary is accepted.
